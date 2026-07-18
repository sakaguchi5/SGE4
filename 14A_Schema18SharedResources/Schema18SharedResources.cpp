#include "Schema18SharedResources.h"

#include <algorithm>
#include <limits>
#include <set>
#include <utility>

namespace sge4::composition::schema18::runtime_v1
{
namespace
{
SharedResourceError Error(std::string stage, std::string message)
{
    return {std::move(stage), std::move(message)};
}

SharedResourceError Error(const runtime::RuntimeError& value)
{
    return {value.stage, value.message};
}

template<class T>
base::Result<T, SharedResourceError> Failure(std::string stage, std::string message)
{
    return base::Result<T, SharedResourceError>::Failure(
        Error(std::move(stage), std::move(message)));
}

package::d3d12_v18::ResourceState InitialStateFor(
    const linking::FrozenCompositionArtifact& artifact,
    ResourceFlowId resource)
{
    std::vector<std::uint32_t> ordinal(artifact.contract.leaves.size(), package::InvalidIndex);
    for (const auto& entry : artifact.plan.schedule)
        if (entry.leaf.value < ordinal.size()) ordinal[entry.leaf.value] = entry.ordinal;

    std::uint32_t bestOrdinal = std::numeric_limits<std::uint32_t>::max();
    package::d3d12_v18::ResourceState result{};
    bool found = false;
    for (const auto& binding : artifact.plan.bindings)
    {
        if (binding.resource != resource || binding.endpoint.value >= artifact.contract.endpoints.size() ||
            binding.leaf.value >= ordinal.size())
            continue;
        if (ordinal[binding.leaf.value] < bestOrdinal)
        {
            bestOrdinal = ordinal[binding.leaf.value];
            result = artifact.contract.endpoints[binding.endpoint.value].requiredIncomingState;
            found = true;
        }
    }
    return found ? result : package::d3d12_v18::ResourceState{};
}
}

const SharedResourceRecord* SharedResourceTable::Record(ResourceFlowId resource) const noexcept
{
    return resource.value < records_.size() ? &records_[resource.value] : nullptr;
}

SharedResourceRecord* SharedResourceTable::Record(ResourceFlowId resource) noexcept
{
    return resource.value < records_.size() ? &records_[resource.value] : nullptr;
}

ResourceFlowId SharedResourceTable::ResourceForEndpoint(
    CompositionEndpointId endpoint) const noexcept
{
    return endpoint.value < endpointResources_.size()
        ? endpointResources_[endpoint.value] : ResourceFlowId{};
}

std::shared_ptr<runtime::IExternalResource>
SharedResourceTable::ResourceForEndpointHandle(CompositionEndpointId endpoint) const
{
    const auto resource = ResourceForEndpoint(endpoint);
    const auto* record = Record(resource);
    return record ? record->resource : nullptr;
}

std::shared_ptr<runtime::ICompletionToken>
SharedResourceTable::AvailableAfter(ResourceFlowId resource) const
{
    const auto* record = Record(resource);
    return record ? record->availableAfter : nullptr;
}

package::d3d12_v18::ResourceState
SharedResourceTable::CurrentState(ResourceFlowId resource) const noexcept
{
    const auto* record = Record(resource);
    return record ? record->currentState : package::d3d12_v18::ResourceState{};
}

base::Result<void, SharedResourceError> SharedResourceTable::PrepareForEndpoint(
    CompositionEndpointId endpoint)
{
    if (!domain_ || endpoint.value >= endpointResources_.size() ||
        endpoint.value >= domain_->Artifact().contract.endpoints.size())
        return base::Result<void, SharedResourceError>::Failure(
            Error("resource/transition", "endpoint identity is invalid"));
    const auto resource = endpointResources_[endpoint.value];
    auto* record = Record(resource);
    const auto& endpointContract = domain_->Artifact().contract.endpoints[endpoint.value];
    if (!record || !record->resource || !record->availableAfter)
        return base::Result<void, SharedResourceError>::Failure(
            Error("resource/transition", "Resource Flow is not materialized"));
    if (record->currentState == endpointContract.requiredIncomingState)
        return base::Result<void, SharedResourceError>::Success();
    auto transitioned = domain_->Backend().TransitionSharedBuffer(
        domain_->NativeDomain(), record->resource, record->availableAfter,
        record->currentState, endpointContract.requiredIncomingState);
    if (!transitioned)
        return base::Result<void, SharedResourceError>::Failure(
            Error(transitioned.Error()));
    if (!transitioned.Value() ||
        transitioned.Value()->DeviceEpoch() != domain_->DeviceEpoch())
        return base::Result<void, SharedResourceError>::Failure(
            Error("resource/transition", "state transition returned an invalid epoch token"));
    record->availableAfter = std::move(transitioned).Value();
    record->currentState = endpointContract.requiredIncomingState;
    return base::Result<void, SharedResourceError>::Success();
}

base::Result<void, SharedResourceError> SharedResourceTable::UpdateAfterRelease(
    CompositionEndpointId endpoint,
    std::shared_ptr<runtime::ICompletionToken> token)
{
    if (!domain_ || endpoint.value >= endpointResources_.size() || !token)
        return base::Result<void, SharedResourceError>::Failure(
            Error("resource/release", "endpoint or completion token is invalid"));
    const auto resource = endpointResources_[endpoint.value];
    auto* record = Record(resource);
    const auto& contract = domain_->Artifact().contract;
    if (!record || endpoint.value >= contract.endpoints.size() ||
        token->DeviceEpoch() != domain_->DeviceEpoch())
        return base::Result<void, SharedResourceError>::Failure(
            Error("resource/release", "release token owner epoch or endpoint mapping is invalid"));
    record->availableAfter = std::move(token);
    record->currentState = contract.endpoints[endpoint.value].guaranteedOutgoingState;
    return base::Result<void, SharedResourceError>::Success();
}

base::Result<void, SharedResourceError> SharedResourceTable::UpdateAfterObservation(
    ResourceFlowId resource,
    std::shared_ptr<runtime::ICompletionToken> token)
{
    auto* record = Record(resource);
    if (!domain_ || !record || !token || token->DeviceEpoch() != domain_->DeviceEpoch())
        return base::Result<void, SharedResourceError>::Failure(
            Error("resource/observation", "observation token or resource identity is invalid"));
    record->availableAfter = std::move(token);
    return base::Result<void, SharedResourceError>::Success();
}

base::Result<SharedResourceTable, SharedResourceError> MaterializeSharedResources(
    SharedDeviceDomain& domain,
    std::span<const SharedResourceInitialData> initialData)
{
    if (domain.State() != runtime::DeviceRuntimeState::Active || domain.DeviceEpoch() == 0)
        return Failure<SharedResourceTable>(
            "resource/domain", "shared DeviceDomain is not Active");
    const auto& artifact = domain.Artifact();
    if (artifact.plan.allocations.size() != artifact.contract.resources.size())
        return Failure<SharedResourceTable>(
            "resource/plan", "allocation and Resource Flow counts disagree");

    std::vector<std::vector<std::byte>> bytes(artifact.contract.resources.size());
    std::set<std::uint32_t> initialized;
    for (const auto& value : initialData)
    {
        if (value.resource.value >= bytes.size() ||
            !initialized.insert(value.resource.value).second ||
            value.bytes.size() > artifact.contract.resources[value.resource.value].sizeBytes)
            return Failure<SharedResourceTable>(
                "resource/initial-data", "initial data is invalid, duplicated, or oversized");
        bytes[value.resource.value] = value.bytes;
    }

    SharedResourceTable table(domain);
    table.records_.resize(artifact.contract.resources.size());
    table.endpointResources_.assign(
        artifact.contract.endpoints.size(), ResourceFlowId{});
    std::vector<bool> seenResources(table.records_.size(), false);
    std::vector<bool> seenEndpoints(table.endpointResources_.size(), false);

    for (const auto& binding : artifact.plan.bindings)
    {
        if (binding.endpoint.value >= table.endpointResources_.size() ||
            binding.resource.value >= table.records_.size() ||
            seenEndpoints[binding.endpoint.value])
            return Failure<SharedResourceTable>(
                "resource/binding", "verified endpoint binding is not a dense one-to-one mapping");
        seenEndpoints[binding.endpoint.value] = true;
        table.endpointResources_[binding.endpoint.value] = binding.resource;
    }
    if (std::ranges::any_of(seenEndpoints, [](bool value) { return !value; }))
        return Failure<SharedResourceTable>(
            "resource/binding", "one or more required endpoints have no Resource Flow binding");

    for (const auto& allocation : artifact.plan.allocations)
    {
        if (allocation.resource.value >= table.records_.size() ||
            seenResources[allocation.resource.value] || allocation.sizeBytes == 0)
            return Failure<SharedResourceTable>(
                "resource/allocation", "allocation identity, size, or uniqueness is invalid");
        seenResources[allocation.resource.value] = true;
        const auto state = InitialStateFor(artifact, allocation.resource);
        auto created = domain.Backend().CreateSharedBuffer(
            domain.NativeDomain(), allocation.resource.value, allocation.sizeBytes,
            state, bytes[allocation.resource.value]);
        if (!created)
            return base::Result<SharedResourceTable, SharedResourceError>::Failure(
                Error(created.Error()));
        if (!created.Value().resource || !created.Value().availableAfter ||
            created.Value().resource->DeviceEpoch() != domain.DeviceEpoch() ||
            created.Value().availableAfter->DeviceEpoch() != domain.DeviceEpoch())
            return Failure<SharedResourceTable>(
                "resource/epoch", "new shared Buffer or token escaped the DeviceDomain epoch");
        table.records_[allocation.resource.value] = {
            allocation.resource, allocation.ownership, allocation.sizeBytes, state,
            std::move(created.Value().resource),
            std::move(created.Value().availableAfter)};
    }
    if (std::ranges::any_of(seenResources, [](bool value) { return !value; }))
        return Failure<SharedResourceTable>(
            "resource/allocation", "not every verified Resource Flow was materialized");
    return base::Result<SharedResourceTable, SharedResourceError>::Success(
        std::move(table));
}

base::Result<d3d12::ExternalBufferReadback, SharedResourceError>
ReadSharedResource(SharedResourceTable& table, ResourceFlowId resource)
{
    auto* record = table.Record(resource);
    if (!table.domain_ || !record || !record->resource || !record->availableAfter)
        return Failure<d3d12::ExternalBufferReadback>(
            "resource/readback", "shared Resource Flow is not materialized");
    auto read = table.domain_->Backend().ReadSharedBuffer(
        table.domain_->NativeDomain(), record->resource, record->availableAfter,
        record->currentState);
    if (!read)
        return base::Result<d3d12::ExternalBufferReadback, SharedResourceError>::Failure(
            Error(read.Error()));
    auto update = table.UpdateAfterObservation(resource, read.Value().availableAfter);
    if (!update)
        return base::Result<d3d12::ExternalBufferReadback, SharedResourceError>::Failure(
            update.Error());
    return base::Result<d3d12::ExternalBufferReadback, SharedResourceError>::Success(
        std::move(read).Value());
}
}
