#include "./CompositionRuntime.h"

#include <algorithm>
#include <map>
#include <set>
#include <utility>

namespace sge4::d3d12::runtime_detail
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

StaticRuntimeError Error(const ::sge4::runtime::RuntimeError& value)
{
    return {value.stage, value.message};
}

template<class T>
base::Expected<T, StaticRuntimeError> Failure(std::string stage, std::string message)
{
    return base::Failure<T, StaticRuntimeError>(
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

base::Expected<void, StaticRuntimeError> ValidateRuntimePlan(
    const artifact::VerifiedFrozenComposition& artifact)
{
    const auto& contract = artifact.ValidatedContract().Contract();
    const auto& plan = artifact.VerifiedPlan().Plan();
    if (plan.schedule.size() != contract.leaves.size() ||
        plan.bindings.size() != contract.endpoints.size() ||
        plan.allocations.size() != contract.resources.size())
        return base::Failure<void, StaticRuntimeError>(
            Error("static-runtime/plan", "検証または実行の契約に違反しています。"));

    std::vector<std::uint32_t> signalEndpoint(plan.signals.size(), package::InvalidIndex);
    for (const auto& signal : plan.signals)
    {
        if (signal.id >= signalEndpoint.size() || signal.producer.value >= contract.endpoints.size())
            return base::Failure<void, StaticRuntimeError>(
                Error("static-runtime/signal", "Contractが検証または実行の契約に違反しています。"));
        signalEndpoint[signal.id] = signal.producer.value;
    }
    for (const auto& wait : plan.waits)
    {
        if (wait.signal >= signalEndpoint.size() ||
            signalEndpoint[wait.signal] == package::InvalidIndex ||
            wait.consumer.value >= contract.endpoints.size())
            return base::Failure<void, StaticRuntimeError>(
                Error("static-runtime/wait", "Planが検証または実行の契約に違反しています。"));
    }
    return base::Success<void, StaticRuntimeError>();
}
}

std::shared_ptr<::sge4::runtime::IExternalResource>
LoadedStaticComposition::SharedResource(ResourceFlowId resource) const
{
    const auto* record = resources_ ? resources_->Record(resource) : nullptr;
    return record ? record->resource : nullptr;
}

std::shared_ptr<::sge4::runtime::ICompletionToken>
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

base::Expected<LoadedStaticComposition, StaticRuntimeError> LoadStaticComposition(
    std::span<const std::byte> frozenCompositionBytes,
    d3d12::Executor& backend,
    StaticCompositionLoadInput input)
{
    auto artifact = artifact::ReadVerifiedFrozenComposition(frozenCompositionBytes);
    if (!artifact)
        return Failure<LoadedStaticComposition>(
            "static-runtime/frozen-read",
            artifact.error().stage + "：" + artifact.error().message);
    auto planValid = ValidateRuntimePlan(artifact.value());
    if (!planValid)
        return base::Failure<LoadedStaticComposition, StaticRuntimeError>(
            planValid.error());

    auto domain = MaterializeSharedDeviceDomain(
        std::move(artifact).value(), backend, input.surface);
    if (!domain)
        return base::Failure<LoadedStaticComposition, StaticRuntimeError>(
            Error(domain.error()));
    auto domainOwner = std::make_unique<SharedDeviceDomain>(
        std::move(domain).value());
    const auto recoveryInitialResources = input.initialResources;
    auto resources = MaterializeSharedResources(
        *domainOwner, input.initialResources);
    if (!resources)
        return base::Failure<LoadedStaticComposition, StaticRuntimeError>(
            Error(resources.error()));

    LoadedStaticComposition loaded;
    loaded.endpointTokens_.assign(
        domainOwner->Artifact().ValidatedContract().Contract().endpoints.size(), nullptr);
    loaded.resources_ = std::make_unique<SharedResourceTable>(
        std::move(resources).value());
    loaded.recoveryInitialResources_ = recoveryInitialResources;
    loaded.domain_ = std::move(domainOwner);
    return base::Success<LoadedStaticComposition, StaticRuntimeError>(
        std::move(loaded));
}

base::Expected<StaticCompositionSubmission, StaticRuntimeError>
SubmitStaticComposition(
    LoadedStaticComposition& loaded,
    const StaticCompositionFrameInvocation& invocation)
{
    if (!loaded.domain_ || !loaded.resources_ ||
        loaded.domain_->State() != ::sge4::runtime::DeviceRuntimeState::Active)
        return Failure<StaticCompositionSubmission>(
            "static-runtime/device-state", "Compositionが検証または実行の契約に違反しています。");

    const auto& artifact = loaded.domain_->Artifact();
    const auto& contract = artifact.ValidatedContract().Contract();
    const auto& plan = artifact.VerifiedPlan().Plan();

    if (invocation.enabledLeaves.size() > contract.leaves.size())
        return Failure<StaticCompositionSubmission>(
            "static-runtime/conditional-region", "enabled Leaf集合が範囲外です。");
    std::vector<bool> enabled(contract.leaves.size(), false);
    std::uint32_t previousEnabled = package::InvalidIndex;
    bool hasPreviousEnabled = false;
    for (const auto leaf : invocation.enabledLeaves)
    {
        if (!leaf.IsValid() || leaf.value >= contract.leaves.size() ||
            (hasPreviousEnabled && leaf.value <= previousEnabled))
            return Failure<StaticCompositionSubmission>(
                "static-runtime/conditional-region",
                "enabled Leaf集合がCanonicalな順序または識別子規則に違反しています。");
        enabled[leaf.value] = true;
        previousEnabled = leaf.value;
        hasPreviousEnabled = true;
    }

    std::vector<std::vector<::sge4::runtime::DynamicDataBinding>> dynamics(contract.leaves.size());
    std::set<std::pair<std::uint32_t, std::uint32_t>> dynamicKeys;
    for (const auto& value : invocation.dynamicData)
    {
        if (value.leaf.value >= contract.leaves.size() ||
            !enabled[value.leaf.value] ||
            value.slot == package::InvalidIndex ||
            !dynamicKeys.emplace(value.leaf.value, value.slot).second)
            return Failure<StaticCompositionSubmission>(
                "static-runtime/dynamic-data", "Bindingが検証または実行の契約に違反しています。");
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
    result.executionOrder.reserve(invocation.enabledLeaves.size());
    result.leaves.reserve(invocation.enabledLeaves.size());

    for (std::size_t scheduleIndex = 0; scheduleIndex < plan.schedule.size(); ++scheduleIndex)
    {
        const auto& entry = plan.schedule[scheduleIndex];
        if (entry.leaf.value >= contract.leaves.size() ||
            entry.ordinal != scheduleIndex)
            return Failure<StaticCompositionSubmission>(
                "static-runtime/schedule", "ScheduleがCanonicalな順序または識別子規則に違反しています。");
        if (!enabled[entry.leaf.value]) continue;
        auto* instance = loaded.domain_->Instance(entry.leaf);
        if (!instance)
            return Failure<StaticCompositionSubmission>(
                "static-runtime/leaf", "検証または実行の契約に違反しています。");

        const auto& leaf = contract.leaves[entry.leaf.value];
        std::vector<::sge4::runtime::ExternalResourceBinding> external(leaf.endpointCount);
        std::vector<bool> present(leaf.endpointCount, false);
        const auto endpointEnd = leaf.endpointBegin + leaf.endpointCount;
        if (endpointEnd > contract.endpoints.size())
            return Failure<StaticCompositionSubmission>(
                "static-runtime/endpoints", "Endpointが検証または実行の契約に違反しています。");

        for (std::uint32_t index = leaf.endpointBegin; index < endpointEnd; ++index)
        {
            const auto& endpoint = contract.endpoints[index];
            if (endpoint.localExternalSlot >= external.size())
                return Failure<StaticCompositionSubmission>(
                    "static-runtime/binding", "LeafがCanonicalな順序または識別子規則に違反しています。");
            auto preparedState = loaded.resources_->PrepareForEndpoint(endpoint.id);
            if (!preparedState)
                return base::Failure<StaticCompositionSubmission, StaticRuntimeError>(
                    Error(preparedState.error()));
            const auto resource = loaded.resources_->ResourceForEndpoint(endpoint.id);
            auto* record = loaded.resources_->Record(resource);
            if (!record || !record->resource || !record->availableAfter)
                return Failure<StaticCompositionSubmission>(
                    "static-runtime/binding", "検証または実行の契約に違反しています。");

            // v1 deliberately retires fan-out consumers in canonical schedule order.
            // The current Resource Flow token therefore transitively dominates the
            // producer and all earlier readers, while still satisfying every F5 wait.
            if (endpoint.access == EndpointAccess::ReadOnly &&
                contract.resources[resource.value].boundary == ResourceBoundary::Internal &&
                !waitedConsumers.contains(endpoint.id.value))
                return Failure<StaticCompositionSubmission>(
                    "static-runtime/wait", "検証または実行の契約に違反しています。");

            external[endpoint.localExternalSlot] = {
                endpoint.localExternalSlot, record->resource, record->availableAfter};
            present[endpoint.localExternalSlot] = true;
        }
        if (std::ranges::any_of(present, [](bool value) { return !value; }))
            return Failure<StaticCompositionSubmission>(
                "static-runtime/binding", "Endpointが検証または実行の契約に違反しています。");

        ::sge4::runtime::FrameInvocation leafInvocation{
            invocation.frameNumber, dynamics[entry.leaf.value], external};
        auto submitted = loaded.domain_->Backend().Submit(*instance, leafInvocation);
        if (!submitted)
            return base::Failure<StaticCompositionSubmission, StaticRuntimeError>(
                Error(submitted.error()));
        if (submitted.value().deviceEpoch != result.deviceEpoch)
            return Failure<StaticCompositionSubmission>(
                "static-runtime/epoch", "検証または実行の契約に違反しています。");

        std::vector<bool> released(leaf.endpointCount, false);
        for (const auto& release : submitted.value().releasedExternalResources)
        {
            const auto* endpoint = FindLeafEndpoint(contract, entry.leaf, release.slot);
            if (!endpoint || !release.safeAfter ||
                endpoint->localExternalSlot >= released.size() ||
                released[endpoint->localExternalSlot])
                return Failure<StaticCompositionSubmission>(
                    "static-runtime/release", "Endpointが検証または実行の契約に違反しています。");
            released[endpoint->localExternalSlot] = true;
            loaded.endpointTokens_[endpoint->id.value] = release.safeAfter;
            auto updated = loaded.resources_->UpdateAfterRelease(
                endpoint->id, release.safeAfter);
            if (!updated)
                return base::Failure<StaticCompositionSubmission, StaticRuntimeError>(
                    Error(updated.error()));
        }
        if (std::ranges::any_of(released, [](bool value) { return !value; }))
            return Failure<StaticCompositionSubmission>(
                "static-runtime/release", "Endpointが検証または実行の契約に違反しています。");

        result.executionOrder.push_back(entry.leaf);
        result.leaves.push_back(std::move(submitted).value());
    }
    return base::Success<StaticCompositionSubmission, StaticRuntimeError>(
        std::move(result));
}

base::Expected<d3d12::ExternalBufferReadback, StaticRuntimeError>
ReadStaticCompositionBuffer(
    LoadedStaticComposition& loaded,
    ResourceFlowId resource)
{
    if (!loaded.domain_ || !loaded.resources_ ||
        loaded.domain_->State() != ::sge4::runtime::DeviceRuntimeState::Active)
        return Failure<d3d12::ExternalBufferReadback>(
            "static-runtime/device-state", "Compositionが検証または実行の契約に違反しています。");
    auto read = ReadSharedResource(*loaded.resources_, resource);
    if (!read)
        return base::Failure<d3d12::ExternalBufferReadback, StaticRuntimeError>(
            Error(read.error()));
    return base::Success<d3d12::ExternalBufferReadback, StaticRuntimeError>(
        std::move(read).value());
}
base::Expected<void, StaticRuntimeError> ValidateStaticCompositionHandleEpoch(
    const LoadedStaticComposition& loaded,
    const std::shared_ptr<::sge4::runtime::IExternalResource>& resource,
    const std::shared_ptr<::sge4::runtime::ICompletionToken>& token)
{
    if (loaded.State() != ::sge4::runtime::DeviceRuntimeState::Active)
        return base::Failure<void, StaticRuntimeError>(
            Error("static-runtime/device-state", "Compositionが検証または実行の契約に違反しています。"));
    if (!resource || !token || resource->DeviceEpoch() != loaded.DeviceEpoch() ||
        token->DeviceEpoch() != loaded.DeviceEpoch())
        return base::Failure<void, StaticRuntimeError>(
            Error("static-runtime/stale-epoch", "Resourceが検証または実行の契約に違反しています。"));
    return base::Success<void, StaticRuntimeError>();
}

}
