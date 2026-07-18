#include "CompositionSharedResources.h"

#include <algorithm>
#include <limits>
#include <set>
#include <utility>

namespace sge4::composition::canonical::runtime_v1
{
namespace
{
SharedResourceError Error(std::string stage, std::string message) { return {std::move(stage), std::move(message)}; }
SharedResourceError Error(const ::sge4::runtime::RuntimeError& value) { return {value.stage, value.message}; }
template<class T> base::Result<T, SharedResourceError> Failure(std::string stage, std::string message)
{
    return base::Result<T, SharedResourceError>::Failure(Error(std::move(stage), std::move(message)));
}

package::d3d12_v13::ResourceState InitialStateFor(
    const verification::VerifiedFrozenComposition& artifact,
    ResourceFlowId resource)
{
    const auto& contract = artifact.ValidatedContract().Contract();
    const auto& plan = artifact.VerifiedPlan().Plan();
    std::vector<std::uint32_t> ordinal(contract.leaves.size(), package::InvalidIndex);
    for (const auto& entry : plan.schedule)
        if (entry.leaf.value < ordinal.size()) ordinal[entry.leaf.value] = entry.ordinal;
    std::uint32_t best = std::numeric_limits<std::uint32_t>::max();
    package::d3d12_v13::ResourceState result{};
    bool found = false;
    for (const auto& binding : plan.bindings)
    {
        if (binding.resource != resource || binding.endpoint.value >= contract.endpoints.size() || binding.leaf.value >= ordinal.size()) continue;
        if (ordinal[binding.leaf.value] < best)
        {
            best = ordinal[binding.leaf.value];
            result = contract.endpoints[binding.endpoint.value].requiredIncomingState;
            found = true;
        }
    }
    return found ? result : package::d3d12_v13::ResourceState{};
}
}

const SharedResourceRecord* SharedResourceTable::Record(ResourceFlowId resource) const noexcept
{ return resource.value < records_.size() ? &records_[resource.value] : nullptr; }
SharedResourceRecord* SharedResourceTable::Record(ResourceFlowId resource) noexcept
{ return resource.value < records_.size() ? &records_[resource.value] : nullptr; }
ResourceFlowId SharedResourceTable::ResourceForEndpoint(CompositionEndpointId endpoint) const noexcept
{ return endpoint.value < endpointResources_.size() ? endpointResources_[endpoint.value] : ResourceFlowId{}; }
std::shared_ptr<::sge4::runtime::IExternalResource> SharedResourceTable::ResourceForEndpointHandle(CompositionEndpointId endpoint) const
{ const auto* r = Record(ResourceForEndpoint(endpoint)); return r ? r->resource : nullptr; }
std::shared_ptr<::sge4::runtime::ICompletionToken> SharedResourceTable::AvailableAfter(ResourceFlowId resource) const
{ const auto* r = Record(resource); return r ? r->availableAfter : nullptr; }
package::d3d12_v13::ResourceState SharedResourceTable::CurrentState(ResourceFlowId resource) const noexcept
{ const auto* r = Record(resource); return r ? r->currentState : package::d3d12_v13::ResourceState{}; }

base::Result<void, SharedResourceError> SharedResourceTable::PrepareForEndpoint(CompositionEndpointId endpoint)
{
    if (!domain_ || endpoint.value >= endpointResources_.size() || endpoint.value >= domain_->Artifact().ValidatedContract().Contract().endpoints.size())
        return base::Result<void, SharedResourceError>::Failure(Error("resource/transition", "endpoint identity is invalid"));
    const auto resourceId = endpointResources_[endpoint.value];
    auto* record = Record(resourceId);
    const auto& endpointContract = domain_->Artifact().ValidatedContract().Contract().endpoints[endpoint.value];
    if (!record || !record->resource || !record->availableAfter)
        return base::Result<void, SharedResourceError>::Failure(Error("resource/transition", "Resource Flow is not materialized"));
    if (record->currentState == endpointContract.requiredIncomingState)
        return base::Result<void, SharedResourceError>::Success();
    auto transitioned = domain_->Backend().TransitionSharedBuffer(
        domain_->NativeDomain(), record->resource, record->availableAfter,
        record->currentState, endpointContract.requiredIncomingState);
    if (!transitioned) return base::Result<void, SharedResourceError>::Failure(Error(transitioned.Error()));
    if (!transitioned.Value() || transitioned.Value()->DeviceEpoch() != domain_->DeviceEpoch())
        return base::Result<void, SharedResourceError>::Failure(Error("resource/transition", "transition token escaped DeviceDomain epoch"));
    record->availableAfter = std::move(transitioned).Value();
    record->currentState = endpointContract.requiredIncomingState;
    return base::Result<void, SharedResourceError>::Success();
}

base::Result<void, SharedResourceError> SharedResourceTable::UpdateAfterRelease(
    CompositionEndpointId endpoint,
    std::shared_ptr<::sge4::runtime::ICompletionToken> token)
{
    if (!domain_ || endpoint.value >= endpointResources_.size() || !token)
        return base::Result<void, SharedResourceError>::Failure(Error("resource/release", "release endpoint or token is invalid"));
    auto* record = Record(endpointResources_[endpoint.value]);
    const auto& contract = domain_->Artifact().ValidatedContract().Contract();
    if (!record || endpoint.value >= contract.endpoints.size() || token->DeviceEpoch() != domain_->DeviceEpoch())
        return base::Result<void, SharedResourceError>::Failure(Error("resource/release", "release token owner epoch is invalid"));
    record->availableAfter = std::move(token);
    record->currentState = contract.endpoints[endpoint.value].guaranteedOutgoingState;
    return base::Result<void, SharedResourceError>::Success();
}

base::Result<void, SharedResourceError> SharedResourceTable::UpdateAfterObservation(
    ResourceFlowId resource,
    std::shared_ptr<::sge4::runtime::ICompletionToken> token)
{
    auto* record = Record(resource);
    if (!domain_ || !record || !token || token->DeviceEpoch() != domain_->DeviceEpoch())
        return base::Result<void, SharedResourceError>::Failure(Error("resource/observation", "observation token or resource is invalid"));
    record->availableAfter = std::move(token);
    return base::Result<void, SharedResourceError>::Success();
}

base::Result<SharedResourceTable, SharedResourceError> MaterializeSharedResources(
    SharedDeviceDomain& domain,
    std::span<const SharedResourceInitialData> initialData)
{
    if (domain.State() != ::sge4::runtime::DeviceRuntimeState::Active || domain.DeviceEpoch() == 0)
        return Failure<SharedResourceTable>("resource/domain", "DeviceDomain is not Active");
    const auto& artifact = domain.Artifact();
    const auto& contract = artifact.ValidatedContract().Contract();
    const auto& plan = artifact.VerifiedPlan().Plan();
    if (plan.allocations.size() != contract.resources.size())
        return Failure<SharedResourceTable>("resource/plan", "allocation and Resource Flow counts disagree");

    std::vector<std::vector<std::byte>> bytes(contract.resources.size());
    std::set<std::uint32_t> initialized;
    for (const auto& value : initialData)
    {
        if (value.resource.value >= bytes.size() || !initialized.insert(value.resource.value).second || value.bytes.size() > contract.resources[value.resource.value].sizeBytes)
            return Failure<SharedResourceTable>("resource/initial-data", "initial data is invalid, duplicated, or oversized");
        bytes[value.resource.value] = value.bytes;
    }

    SharedResourceTable table(domain);
    table.records_.resize(contract.resources.size());
    table.endpointResources_.assign(contract.endpoints.size(), ResourceFlowId{});
    std::vector<bool> seenResources(table.records_.size(), false);
    std::vector<bool> seenEndpoints(table.endpointResources_.size(), false);
    for (const auto& binding : plan.bindings)
    {
        if (binding.endpoint.value >= table.endpointResources_.size() || binding.resource.value >= table.records_.size() || seenEndpoints[binding.endpoint.value])
            return Failure<SharedResourceTable>("resource/binding", "endpoint binding is invalid or duplicated");
        seenEndpoints[binding.endpoint.value] = true;
        table.endpointResources_[binding.endpoint.value] = binding.resource;
    }
    if (std::ranges::any_of(seenEndpoints, [](bool v){ return !v; }))
        return Failure<SharedResourceTable>("resource/binding", "required endpoint has no Resource Flow binding");

    for (const auto& allocation : plan.allocations)
    {
        if (allocation.resource.value >= table.records_.size() || seenResources[allocation.resource.value] || allocation.sizeBytes == 0)
            return Failure<SharedResourceTable>("resource/allocation", "allocation identity, size, or uniqueness is invalid");
        seenResources[allocation.resource.value] = true;
        const auto state = InitialStateFor(artifact, allocation.resource);
        auto created = domain.Backend().CreateSharedBuffer(
            domain.NativeDomain(), allocation.resource.value, allocation.sizeBytes,
            state, bytes[allocation.resource.value]);
        if (!created) return base::Result<SharedResourceTable, SharedResourceError>::Failure(Error(created.Error()));
        if (!created.Value().resource || !created.Value().availableAfter ||
            created.Value().resource->DeviceEpoch() != domain.DeviceEpoch() ||
            created.Value().availableAfter->DeviceEpoch() != domain.DeviceEpoch())
            return Failure<SharedResourceTable>("resource/epoch", "new Buffer or token escaped DeviceDomain epoch");
        table.records_[allocation.resource.value] = {
            allocation.resource, allocation.ownership, allocation.sizeBytes, state,
            std::move(created.Value().resource), std::move(created.Value().availableAfter)};
    }
    if (std::ranges::any_of(seenResources, [](bool v){ return !v; }))
        return Failure<SharedResourceTable>("resource/allocation", "not every Resource Flow was materialized");
    return base::Result<SharedResourceTable, SharedResourceError>::Success(std::move(table));
}

base::Result<d3d12::ExternalBufferReadback, SharedResourceError> ReadSharedResource(
    SharedResourceTable& table,
    ResourceFlowId resource)
{
    auto* record = table.Record(resource);
    if (!table.domain_ || !record || !record->resource || !record->availableAfter)
        return Failure<d3d12::ExternalBufferReadback>("resource/readback", "Resource Flow is not materialized");
    auto read = table.domain_->Backend().ReadSharedBuffer(
        table.domain_->NativeDomain(), record->resource, record->availableAfter, record->currentState);
    if (!read) return base::Result<d3d12::ExternalBufferReadback, SharedResourceError>::Failure(Error(read.Error()));
    auto update = table.UpdateAfterObservation(resource, read.Value().availableAfter);
    if (!update) return base::Result<d3d12::ExternalBufferReadback, SharedResourceError>::Failure(update.Error());
    return base::Result<d3d12::ExternalBufferReadback, SharedResourceError>::Success(std::move(read).Value());
}
}
