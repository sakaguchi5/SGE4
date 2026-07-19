#include "CompositionRuntime.h"

#include <algorithm>
#include <map>
#include <set>
#include <utility>

namespace sge4_5::composition::canonical::runtime_v1
{
namespace
{
StaticRuntimeError Error(std::string stage, std::string message)
{
    return {std::move(stage), std::move(message)};
}

StaticRuntimeError Error(const DomainError& value)
{
    return {value.stage, value.message};
}

StaticRuntimeError Error(const SharedResourceError& value)
{
    return {value.stage, value.message};
}

StaticRuntimeError Error(const ::sge4_5::runtime::RuntimeError& value)
{
    return {value.stage, value.message};
}

template<class T>
base::Result<T, StaticRuntimeError> Failure(std::string stage, std::string message)
{
    return base::Result<T, StaticRuntimeError>::Failure(
        Error(std::move(stage), std::move(message)));
}

const CompositionEndpointContract* FindLeafEndpoint(
    const PackageCompositionContract& contract,
    LeafPackageId leaf,
    std::uint32_t externalSlot)
{
    if (leaf.value >= contract.leaves.size()) return nullptr;
    const auto& owner = contract.leaves[leaf.value];
    const auto end = owner.endpointBegin + owner.endpointCount;
    if (end > contract.endpoints.size()) return nullptr;
    const auto found = std::find_if(
        contract.endpoints.begin() + owner.endpointBegin,
        contract.endpoints.begin() + end,
        [&](const auto& endpoint) {
            return endpoint.leaf == leaf && endpoint.localExternalSlot == externalSlot;
        });
    return found == contract.endpoints.begin() + end ? nullptr : &*found;
}

base::Result<void, StaticRuntimeError> ValidateRuntimePlan(
    const verification::VerifiedFrozenComposition& artifact)
{
    const auto& contract = artifact.ValidatedContract().Contract();
    const auto& plan = artifact.VerifiedPlan().Plan();
    if (plan.schedule.size() != contract.leaves.size() ||
        plan.bindings.size() != contract.endpoints.size() ||
        plan.allocations.size() != contract.resources.size())
        return base::Result<void, StaticRuntimeError>::Failure(
            Error("static-runtime/plan", "Frozen Plan cardinalities disagree with the Contract"));

    std::vector<std::uint32_t> signalEndpoint(plan.signals.size(), package::InvalidIndex);
    for (const auto& signal : plan.signals)
    {
        if (signal.id >= signalEndpoint.size() || signal.producer.value >= contract.endpoints.size())
            return base::Result<void, StaticRuntimeError>::Failure(
                Error("static-runtime/signal", "signal identity is outside the Frozen Contract"));
        signalEndpoint[signal.id] = signal.producer.value;
    }
    for (const auto& wait : plan.waits)
    {
        if (wait.signal >= signalEndpoint.size() ||
            signalEndpoint[wait.signal] == package::InvalidIndex ||
            wait.consumer.value >= contract.endpoints.size())
            return base::Result<void, StaticRuntimeError>::Failure(
                Error("static-runtime/wait", "wait identity is outside the Frozen Plan"));
    }
    return base::Result<void, StaticRuntimeError>::Success();
}
}

std::shared_ptr<::sge4_5::runtime::IExternalResource>
LoadedStaticComposition::SharedResource(ResourceFlowId resource) const
{
    const auto* record = resources_ ? resources_->Record(resource) : nullptr;
    return record ? record->resource : nullptr;
}

std::shared_ptr<::sge4_5::runtime::ICompletionToken>
LoadedStaticComposition::AvailableAfter(ResourceFlowId resource) const
{
    return resources_ ? resources_->AvailableAfter(resource) : nullptr;
}

ResourceFlowId LoadedStaticComposition::FindResourceFlow(const StableKey& stableKey) const noexcept
{
    if (!domain_) return {};
    const auto& resources = domain_->Artifact().ValidatedContract().Contract().resources;
    const auto found = std::find_if(resources.begin(), resources.end(),
        [&](const auto& resource) { return resource.stableKey == stableKey; });
    return found == resources.end() ? ResourceFlowId{} : found->id;
}

base::Result<LoadedStaticComposition, StaticRuntimeError> LoadStaticComposition(
    std::span<const std::byte> frozenCompositionBytes,
    d3d12::D3D12Backend& backend,
    StaticCompositionLoadInput input)
{
    auto artifact = verification::ReadVerifiedFrozenComposition(frozenCompositionBytes);
    if (!artifact)
        return Failure<LoadedStaticComposition>(
            "static-runtime/frozen-read",
            artifact.Error().stage + ": " + artifact.Error().message);
    auto planValid = ValidateRuntimePlan(artifact.Value());
    if (!planValid)
        return base::Result<LoadedStaticComposition, StaticRuntimeError>::Failure(
            planValid.Error());

    auto domain = MaterializeSharedDeviceDomain(
        std::move(artifact).Value(), backend, input.surface);
    if (!domain)
        return base::Result<LoadedStaticComposition, StaticRuntimeError>::Failure(
            Error(domain.Error()));
    auto domainOwner = std::make_unique<SharedDeviceDomain>(
        std::move(domain).Value());
    const auto recoveryInitialResources = input.initialResources;
    auto resources = MaterializeSharedResources(
        *domainOwner, input.initialResources);
    if (!resources)
        return base::Result<LoadedStaticComposition, StaticRuntimeError>::Failure(
            Error(resources.Error()));

    LoadedStaticComposition loaded;
    loaded.endpointTokens_.assign(
        domainOwner->Artifact().ValidatedContract().Contract().endpoints.size(), nullptr);
    loaded.resources_ = std::make_unique<SharedResourceTable>(
        std::move(resources).Value());
    loaded.recoveryInitialResources_ = recoveryInitialResources;
    loaded.domain_ = std::move(domainOwner);
    return base::Result<LoadedStaticComposition, StaticRuntimeError>::Success(
        std::move(loaded));
}

base::Result<StaticCompositionSubmission, StaticRuntimeError>
SubmitStaticComposition(
    LoadedStaticComposition& loaded,
    const StaticCompositionFrameInvocation& invocation)
{
    if (!loaded.domain_ || !loaded.resources_ ||
        loaded.domain_->State() != ::sge4_5::runtime::DeviceRuntimeState::Active)
        return Failure<StaticCompositionSubmission>(
            "static-runtime/device-state", "Composition DeviceDomain is not Active");

    const auto& artifact = loaded.domain_->Artifact();
    const auto& contract = artifact.ValidatedContract().Contract();
    const auto& plan = artifact.VerifiedPlan().Plan();

    std::vector<std::vector<::sge4_5::runtime::DynamicDataBinding>> dynamics(contract.leaves.size());
    std::set<std::pair<std::uint32_t, std::uint32_t>> dynamicKeys;
    for (const auto& value : invocation.dynamicData)
    {
        if (value.leaf.value >= contract.leaves.size() ||
            value.slot == package::InvalidIndex ||
            !dynamicKeys.emplace(value.leaf.value, value.slot).second)
            return Failure<StaticCompositionSubmission>(
                "static-runtime/dynamic-data", "dynamic binding is invalid or duplicated");
        dynamics[value.leaf.value].push_back({value.slot, value.bytes});
    }
    for (auto& bindings : dynamics)
        std::sort(bindings.begin(), bindings.end(), [](const auto& left, const auto& right) {
            return left.slot < right.slot;
        });

    std::set<std::uint32_t> waitedConsumers;
    for (const auto& wait : plan.waits) waitedConsumers.insert(wait.consumer.value);

    StaticCompositionSubmission result;
    result.deviceEpoch = loaded.domain_->DeviceEpoch();
    result.executionOrder.reserve(plan.schedule.size());
    result.leaves.reserve(plan.schedule.size());

    for (const auto& entry : plan.schedule)
    {
        if (entry.leaf.value >= contract.leaves.size() ||
            entry.ordinal != result.executionOrder.size())
            return Failure<StaticCompositionSubmission>(
                "static-runtime/schedule", "Leaf schedule is not canonical and dense");
        auto* instance = loaded.domain_->Instance(entry.leaf);
        if (!instance)
            return Failure<StaticCompositionSubmission>(
                "static-runtime/leaf", "scheduled Leaf is not materialized");

        const auto& leaf = contract.leaves[entry.leaf.value];
        std::vector<::sge4_5::runtime::ExternalResourceBinding> external(leaf.endpointCount);
        std::vector<bool> present(leaf.endpointCount, false);
        const auto endpointEnd = leaf.endpointBegin + leaf.endpointCount;
        if (endpointEnd > contract.endpoints.size())
            return Failure<StaticCompositionSubmission>(
                "static-runtime/endpoints", "Leaf endpoint range exceeds the Contract");

        for (std::uint32_t index = leaf.endpointBegin; index < endpointEnd; ++index)
        {
            const auto& endpoint = contract.endpoints[index];
            if (endpoint.localExternalSlot >= external.size())
                return Failure<StaticCompositionSubmission>(
                    "static-runtime/binding", "Leaf external slots are not canonical and dense");
            auto preparedState = loaded.resources_->PrepareForEndpoint(endpoint.id);
            if (!preparedState)
                return base::Result<StaticCompositionSubmission, StaticRuntimeError>::Failure(
                    Error(preparedState.Error()));
            const auto resource = loaded.resources_->ResourceForEndpoint(endpoint.id);
            auto* record = loaded.resources_->Record(resource);
            if (!record || !record->resource || !record->availableAfter)
                return Failure<StaticCompositionSubmission>(
                    "static-runtime/binding", "verified Resource Flow is not materialized");

            // v1 deliberately retires fan-out consumers in canonical schedule order.
            // The current Resource Flow token therefore transitively dominates the
            // producer and all earlier readers, while still satisfying every F5 wait.
            if (endpoint.access == EndpointAccess::ReadOnly &&
                contract.resources[resource.value].boundary == ResourceBoundary::Internal &&
                !waitedConsumers.contains(endpoint.id.value))
                return Failure<StaticCompositionSubmission>(
                    "static-runtime/wait", "internal consumer has no authoritative wait point");

            external[endpoint.localExternalSlot] = {
                endpoint.localExternalSlot, record->resource, record->availableAfter};
            present[endpoint.localExternalSlot] = true;
        }
        if (std::ranges::any_of(present, [](bool value) { return !value; }))
            return Failure<StaticCompositionSubmission>(
                "static-runtime/binding", "a required Leaf endpoint was not bound");

        ::sge4_5::runtime::FrameInvocation leafInvocation{
            invocation.frameNumber, dynamics[entry.leaf.value], external};
        auto submitted = loaded.domain_->Backend().Submit(*instance, leafInvocation);
        if (!submitted)
            return base::Result<StaticCompositionSubmission, StaticRuntimeError>::Failure(
                Error(submitted.Error()));
        if (submitted.Value().deviceEpoch != result.deviceEpoch)
            return Failure<StaticCompositionSubmission>(
                "static-runtime/epoch", "Leaf submission escaped the shared DeviceDomain epoch");

        std::vector<bool> released(leaf.endpointCount, false);
        for (const auto& release : submitted.Value().releasedExternalResources)
        {
            const auto* endpoint = FindLeafEndpoint(contract, entry.leaf, release.slot);
            if (!endpoint || !release.safeAfter ||
                endpoint->localExternalSlot >= released.size() ||
                released[endpoint->localExternalSlot])
                return Failure<StaticCompositionSubmission>(
                    "static-runtime/release", "Leaf released an unknown or duplicate endpoint");
            released[endpoint->localExternalSlot] = true;
            loaded.endpointTokens_[endpoint->id.value] = release.safeAfter;
            auto updated = loaded.resources_->UpdateAfterRelease(
                endpoint->id, release.safeAfter);
            if (!updated)
                return base::Result<StaticCompositionSubmission, StaticRuntimeError>::Failure(
                    Error(updated.Error()));
        }
        if (std::ranges::any_of(released, [](bool value) { return !value; }))
            return Failure<StaticCompositionSubmission>(
                "static-runtime/release", "Leaf did not release every required endpoint");

        result.executionOrder.push_back(entry.leaf);
        result.leaves.push_back(std::move(submitted).Value());
    }
    return base::Result<StaticCompositionSubmission, StaticRuntimeError>::Success(
        std::move(result));
}

base::Result<d3d12::ExternalBufferReadback, StaticRuntimeError>
ReadStaticCompositionBuffer(
    LoadedStaticComposition& loaded,
    ResourceFlowId resource)
{
    if (!loaded.domain_ || !loaded.resources_ ||
        loaded.domain_->State() != ::sge4_5::runtime::DeviceRuntimeState::Active)
        return Failure<d3d12::ExternalBufferReadback>(
            "static-runtime/device-state", "Composition DeviceDomain is not Active");
    auto read = ReadSharedResource(*loaded.resources_, resource);
    if (!read)
        return base::Result<d3d12::ExternalBufferReadback, StaticRuntimeError>::Failure(
            Error(read.Error()));
    return base::Result<d3d12::ExternalBufferReadback, StaticRuntimeError>::Success(
        std::move(read).Value());
}
base::Result<void, StaticRuntimeError> ValidateStaticCompositionHandleEpoch(
    const LoadedStaticComposition& loaded,
    const std::shared_ptr<::sge4_5::runtime::IExternalResource>& resource,
    const std::shared_ptr<::sge4_5::runtime::ICompletionToken>& token)
{
    if (loaded.State() != ::sge4_5::runtime::DeviceRuntimeState::Active)
        return base::Result<void, StaticRuntimeError>::Failure(
            Error("static-runtime/device-state", "Composition DeviceDomain is not Active"));
    if (!resource || !token || resource->DeviceEpoch() != loaded.DeviceEpoch() ||
        token->DeviceEpoch() != loaded.DeviceEpoch())
        return base::Result<void, StaticRuntimeError>::Failure(
            Error("static-runtime/stale-epoch", "resource or token belongs to a stale DeviceDomain epoch"));
    return base::Result<void, StaticRuntimeError>::Success();
}

}
