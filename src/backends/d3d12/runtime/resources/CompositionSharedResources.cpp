#include "./CompositionSharedResources.h"

#include <algorithm>
#include <limits>
#include <set>
#include <utility>

namespace sge4::d3d12::runtime_detail
{
namespace
{
SharedResourceError Error(std::string stage, std::string message)
{
    return {std::move(stage), std::move(message)};
}
SharedResourceError Error(const ::sge4::runtime::RuntimeError& value)
{
    return {value.stage, value.message};
}
template<class T>
base::Expected<T, SharedResourceError> Failure(std::string stage, std::string message)
{
    return base::Failure<T, SharedResourceError>(Error(std::move(stage), std::move(message)));
}

package::d3d12_v13::ResourceState InitialStateForSameFrame(
    const artifact::VerifiedFrozenComposition& artifact,
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
        if (binding.resource != resource || binding.endpoint.value >= contract.endpoints.size() ||
            binding.leaf.value >= ordinal.size())
            continue;
        if (ordinal[binding.leaf.value] < best)
        {
            best = ordinal[binding.leaf.value];
            result = contract.endpoints[binding.endpoint.value].requiredIncomingState;
            found = true;
        }
    }
    return found ? result : package::d3d12_v13::ResourceState{};
}

base::Expected<std::pair<package::d3d12_v13::ResourceState,
                         package::d3d12_v13::ResourceState>, SharedResourceError>
TemporalInitialStates(
    const artifact::VerifiedFrozenComposition& artifact,
    ResourceFlowId resource)
{
    const auto& contract = artifact.ValidatedContract().Contract();
    const auto& plan = artifact.VerifiedPlan().Plan();
    const auto found = std::find_if(plan.temporalBuffers.begin(), plan.temporalBuffers.end(),
        [&](const auto& value) { return value.resource == resource; });
    if (found == plan.temporalBuffers.end() ||
        found->currentProducer.value >= contract.endpoints.size() ||
        found->previousConsumers.empty() ||
        found->previousConsumers.front().value >= contract.endpoints.size())
        return Failure<std::pair<package::d3d12_v13::ResourceState,
                                 package::d3d12_v13::ResourceState>>(
            "resource/temporal-plan", "Temporal Bufferが検証または実行の契約に違反しています。");
    const auto previous =
        contract.endpoints[found->previousConsumers.front().value].requiredIncomingState;
    const auto current =
        contract.endpoints[found->currentProducer.value].requiredIncomingState;
    return base::Success<std::pair<package::d3d12_v13::ResourceState,
                                   package::d3d12_v13::ResourceState>, SharedResourceError>(
        {previous, current});
}

std::uint32_t NativeTemporalIdentity(ResourceFlowId resource, std::uint32_t instance) noexcept
{
    return 0x8000'0000u | ((resource.value & 0x3fff'ffffu) << 1u) | (instance & 1u);
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
ResourceFlowId SharedResourceTable::ResourceForEndpoint(CompositionEndpointId endpoint) const noexcept
{
    return endpoint.value < endpointResources_.size() ? endpointResources_[endpoint.value] : ResourceFlowId{};
}

SharedResourceInstance* SharedResourceTable::InstanceForEndpoint(CompositionEndpointId endpoint) noexcept
{
    if (!domain_ || endpoint.value >= endpointResources_.size() ||
        endpoint.value >= domain_->Artifact().ValidatedContract().Contract().endpoints.size())
        return nullptr;
    auto* record = Record(endpointResources_[endpoint.value]);
    if (!record || record->instances.empty()) return nullptr;
    if (record->lifetime == ResourceFlowLifetime::SameFrame)
        return &record->instances[0];
    const auto access =
        domain_->Artifact().ValidatedContract().Contract().endpoints[endpoint.value].access;
    const auto index = access == EndpointAccess::WriteOnly
        ? record->currentInstance : record->previousInstance;
    return index < record->instances.size() ? &record->instances[index] : nullptr;
}
const SharedResourceInstance* SharedResourceTable::InstanceForEndpoint(
    CompositionEndpointId endpoint) const noexcept
{
    return const_cast<SharedResourceTable*>(this)->InstanceForEndpoint(endpoint);
}
SharedResourceInstance* SharedResourceTable::ObservableInstance(ResourceFlowId resource) noexcept
{
    auto* record = Record(resource);
    if (!record || record->instances.empty()) return nullptr;
    const auto index = record->lifetime == ResourceFlowLifetime::TemporalHistory
        ? record->previousInstance : 0u;
    return index < record->instances.size() ? &record->instances[index] : nullptr;
}
const SharedResourceInstance* SharedResourceTable::ObservableInstance(ResourceFlowId resource) const noexcept
{
    return const_cast<SharedResourceTable*>(this)->ObservableInstance(resource);
}

std::shared_ptr<::sge4::runtime::IExternalResource>
SharedResourceTable::ResourceForEndpointHandle(CompositionEndpointId endpoint) const
{
    const auto* instance = InstanceForEndpoint(endpoint);
    return instance ? instance->resource : nullptr;
}
std::shared_ptr<::sge4::runtime::ICompletionToken>
SharedResourceTable::AvailableAfterForEndpoint(CompositionEndpointId endpoint) const
{
    const auto* instance = InstanceForEndpoint(endpoint);
    return instance ? instance->availableAfter : nullptr;
}
std::shared_ptr<::sge4::runtime::IExternalResource>
SharedResourceTable::ObservableResource(ResourceFlowId resource) const
{
    const auto* instance = ObservableInstance(resource);
    return instance ? instance->resource : nullptr;
}
std::shared_ptr<::sge4::runtime::ICompletionToken>
SharedResourceTable::AvailableAfter(ResourceFlowId resource) const
{
    const auto* instance = ObservableInstance(resource);
    return instance ? instance->availableAfter : nullptr;
}
package::d3d12_v13::ResourceState
SharedResourceTable::CurrentState(ResourceFlowId resource) const noexcept
{
    const auto* instance = ObservableInstance(resource);
    return instance ? instance->currentState : package::d3d12_v13::ResourceState{};
}

base::Expected<void, SharedResourceError>
SharedResourceTable::PrepareForEndpoint(CompositionEndpointId endpoint)
{
    if (!domain_ || endpoint.value >= endpointResources_.size() ||
        endpoint.value >= domain_->Artifact().ValidatedContract().Contract().endpoints.size())
        return base::Failure<void, SharedResourceError>(
            Error("resource/transition", "Endpointが検証または実行の契約に違反しています。"));
    auto* instance = InstanceForEndpoint(endpoint);
    const auto& endpointContract =
        domain_->Artifact().ValidatedContract().Contract().endpoints[endpoint.value];
    if (!instance || !instance->resource || !instance->availableAfter)
        return base::Failure<void, SharedResourceError>(
            Error("resource/transition", "検証または実行の契約に違反しています。"));
    if (instance->currentState == endpointContract.requiredIncomingState)
        return base::Success<void, SharedResourceError>();
    auto transitioned = domain_->Backend().TransitionSharedResource(
        domain_->NativeDomain(), instance->resource, instance->availableAfter,
        instance->currentState, endpointContract.requiredIncomingState);
    if (!transitioned)
        return base::Failure<void, SharedResourceError>(Error(transitioned.error()));
    if (!transitioned.value() || transitioned.value()->DeviceEpoch() != domain_->DeviceEpoch())
        return base::Failure<void, SharedResourceError>(
            Error("resource/transition", "検証または実行の契約に違反しています。"));
    instance->availableAfter = std::move(transitioned).value();
    instance->currentState = endpointContract.requiredIncomingState;
    return base::Success<void, SharedResourceError>();
}

base::Expected<void, SharedResourceError> SharedResourceTable::UpdateAfterRelease(
    CompositionEndpointId endpoint,
    std::shared_ptr<::sge4::runtime::ICompletionToken> token)
{
    if (!domain_ || endpoint.value >= endpointResources_.size() || !token)
        return base::Failure<void, SharedResourceError>(
            Error("resource/release", "Endpointが検証または実行の契約に違反しています。"));
    auto* instance = InstanceForEndpoint(endpoint);
    const auto& contract = domain_->Artifact().ValidatedContract().Contract();
    if (!instance || endpoint.value >= contract.endpoints.size() ||
        token->DeviceEpoch() != domain_->DeviceEpoch())
        return base::Failure<void, SharedResourceError>(
            Error("resource/release", "Epochの状態または世代が実行契約と一致しません。"));
    instance->availableAfter = std::move(token);
    instance->currentState = contract.endpoints[endpoint.value].guaranteedOutgoingState;
    return base::Success<void, SharedResourceError>();
}

base::Expected<void, SharedResourceError> SharedResourceTable::UpdateAfterObservation(
    ResourceFlowId resource,
    std::shared_ptr<::sge4::runtime::ICompletionToken> token)
{
    auto* instance = ObservableInstance(resource);
    if (!domain_ || !instance || !token || token->DeviceEpoch() != domain_->DeviceEpoch())
        return base::Failure<void, SharedResourceError>(
            Error("resource/observation", "Resourceが検証または実行の契約に違反しています。"));
    instance->availableAfter = std::move(token);
    return base::Success<void, SharedResourceError>();
}

base::Expected<void, SharedResourceError> SharedResourceTable::CommitTemporalFrame(
    std::span<const LeafPackageId> executionOrder)
{
    if (!domain_)
        return base::Failure<void, SharedResourceError>(
            Error("resource/temporal-commit", "Deviceが検証または実行の契約に違反しています。"));
    const auto& plan = domain_->Artifact().VerifiedPlan().Plan();
    for (const auto& temporal : plan.temporalBuffers)
    {
        if (std::ranges::find(executionOrder, temporal.currentProducerLeaf) == executionOrder.end())
            return base::Failure<void, SharedResourceError>(
                Error("resource/temporal-commit", "Temporal BufferのCurrent writerが実行されませんでした。"));
        auto* record = Record(temporal.resource);
        if (!record || record->lifetime != ResourceFlowLifetime::TemporalHistory ||
            record->instances.size() != 2 || record->previousInstance == record->currentInstance)
            return base::Failure<void, SharedResourceError>(
                Error("resource/temporal-commit", "Temporal Bufferの物理世代が無効です。"));
    }
    for (const auto& temporal : plan.temporalBuffers)
    {
        auto* record = Record(temporal.resource);
        std::swap(record->previousInstance, record->currentInstance);
    }
    return base::Success<void, SharedResourceError>();
}

base::Expected<SharedResourceTable, SharedResourceError> MaterializeSharedResources(
    SharedDeviceDomain& domain,
    std::span<const SharedResourceInitialData> initialData)
{
    if (domain.State() != ::sge4::runtime::DeviceRuntimeState::Active || domain.DeviceEpoch() == 0)
        return Failure<SharedResourceTable>(
            "resource/domain", "Deviceが検証または実行の契約に違反しています。");
    const auto& artifact = domain.Artifact();
    const auto& contract = artifact.ValidatedContract().Contract();
    const auto& plan = artifact.VerifiedPlan().Plan();
    if (plan.allocations.size() != contract.resources.size())
        return Failure<SharedResourceTable>(
            "resource/plan", "検証または実行の契約に違反しています。");

    std::vector<std::vector<std::byte>> bytes(contract.resources.size());
    std::set<std::uint32_t> initialized;
    for (const auto& value : initialData)
    {
        if (value.resource.value >= bytes.size() ||
            !initialized.insert(value.resource.value).second)
            return Failure<SharedResourceTable>(
                "resource/initial-data", "入力または内部状態が検証または実行の契約に違反しています。");
        const auto& resource = contract.resources[value.resource.value];
        const bool temporal = resource.lifetime == ResourceFlowLifetime::TemporalHistory;
        const bool validBuffer = resource.kind == package::d3d12_v13::ResourceKind::Buffer &&
            (temporal ? value.bytes.size() == resource.sizeBytes
                      : value.bytes.size() <= resource.sizeBytes);
        const bool validTexture = resource.kind == package::d3d12_v13::ResourceKind::Texture2D &&
            !temporal && (value.bytes.empty() || value.bytes.size() ==
                static_cast<std::uint64_t>(resource.texture2D.rowBytes) * resource.texture2D.height);
        if (!validBuffer && !validTexture)
            return Failure<SharedResourceTable>(
                "resource/initial-data", "入力または内部状態が検証または実行の契約に違反しています。");
        bytes[value.resource.value] = value.bytes;
    }
    for (const auto& resource : contract.resources)
        if (resource.lifetime == ResourceFlowLifetime::TemporalHistory &&
            !initialized.contains(resource.id.value))
            return Failure<SharedResourceTable>(
                "resource/temporal-seed", "Temporal Bufferには固定sizeの明示的Previous seedが必要です。");

    SharedResourceTable table(domain);
    table.records_.resize(contract.resources.size());
    table.endpointResources_.assign(contract.endpoints.size(), ResourceFlowId{});
    std::vector<bool> seenResources(table.records_.size(), false);
    std::vector<bool> seenEndpoints(table.endpointResources_.size(), false);
    for (const auto& binding : plan.bindings)
    {
        if (binding.endpoint.value >= table.endpointResources_.size() ||
            binding.resource.value >= table.records_.size() ||
            seenEndpoints[binding.endpoint.value])
            return Failure<SharedResourceTable>(
                "resource/binding", "Endpointが検証または実行の契約に違反しています。");
        seenEndpoints[binding.endpoint.value] = true;
        table.endpointResources_[binding.endpoint.value] = binding.resource;
    }
    if (std::ranges::any_of(seenEndpoints, [](bool value) { return !value; }))
        return Failure<SharedResourceTable>(
            "resource/binding", "Resource Flowが検証または実行の契約に違反しています。");

    for (const auto& allocation : plan.allocations)
    {
        if (allocation.resource.value >= table.records_.size() ||
            seenResources[allocation.resource.value] || allocation.sizeBytes == 0)
            return Failure<SharedResourceTable>(
                "resource/allocation", "Allocationが検証または実行の契約に違反しています。");
        seenResources[allocation.resource.value] = true;

        SharedResourceRecord record;
        record.id = allocation.resource;
        record.ownership = allocation.ownership;
        record.kind = allocation.kind;
        record.format = allocation.format;
        record.sizeBytes = allocation.sizeBytes;
        record.texture2D = allocation.texture2D;
        record.lifetime = allocation.lifetime;
        record.historyDepth = allocation.historyDepth;
        record.previousInstance = 0;
        record.currentInstance = allocation.lifetime == ResourceFlowLifetime::TemporalHistory ? 1u : 0u;
        record.instances.reserve(allocation.physicalInstanceCount);

        std::vector<package::d3d12_v13::ResourceState> initialStates;
        if (allocation.lifetime == ResourceFlowLifetime::TemporalHistory)
        {
            auto states = TemporalInitialStates(artifact, allocation.resource);
            if (!states)
                return base::Failure<SharedResourceTable, SharedResourceError>(states.error());
            initialStates = {states.value().first, states.value().second};
        }
        else
        {
            initialStates = {InitialStateForSameFrame(artifact, allocation.resource)};
        }

        if (initialStates.size() != allocation.physicalInstanceCount)
            return Failure<SharedResourceTable>(
                "resource/physical-count", "Temporal Bufferの物理世代数がPlanと一致しません。");
        for (std::uint32_t instanceIndex = 0;
             instanceIndex < allocation.physicalInstanceCount; ++instanceIndex)
        {
            const auto identity = allocation.lifetime == ResourceFlowLifetime::TemporalHistory
                ? NativeTemporalIdentity(allocation.resource, instanceIndex)
                : allocation.resource.value;
            base::Expected<d3d12::ExternalBufferBinding, ::sge4::runtime::RuntimeError> created =
                allocation.kind == package::d3d12_v13::ResourceKind::Texture2D
                    ? domain.Backend().CreateSharedTexture2D(
                        domain.NativeDomain(), identity,
                        allocation.texture2D.width, allocation.texture2D.height,
                        allocation.texture2D.rowBytes, allocation.format,
                        initialStates[instanceIndex], bytes[allocation.resource.value])
                    : domain.Backend().CreateSharedBuffer(
                        domain.NativeDomain(), identity, allocation.sizeBytes,
                        initialStates[instanceIndex], bytes[allocation.resource.value]);
            if (!created)
                return base::Failure<SharedResourceTable, SharedResourceError>(
                    Error(created.error()));
            if (!created.value().resource || !created.value().availableAfter ||
                created.value().resource->DeviceEpoch() != domain.DeviceEpoch() ||
                created.value().availableAfter->DeviceEpoch() != domain.DeviceEpoch())
                return Failure<SharedResourceTable>(
                    "resource/epoch", "検証または実行の契約に違反しています。");
            record.instances.push_back({initialStates[instanceIndex],
                std::move(created.value().resource),
                std::move(created.value().availableAfter)});
        }
        table.records_[allocation.resource.value] = std::move(record);
    }
    if (std::ranges::any_of(seenResources, [](bool value) { return !value; }))
        return Failure<SharedResourceTable>(
            "resource/allocation", "検証または実行の契約に違反しています。");
    return base::Success<SharedResourceTable, SharedResourceError>(std::move(table));
}

base::Expected<d3d12::ExternalBufferReadback, SharedResourceError> ReadSharedResource(
    SharedResourceTable& table,
    ResourceFlowId resource)
{
    auto* record = table.Record(resource);
    auto* instance = table.ObservableInstance(resource);
    if (!table.domain_ || !record || !instance || !instance->resource ||
        !instance->availableAfter ||
        record->kind != package::d3d12_v13::ResourceKind::Buffer)
        return Failure<d3d12::ExternalBufferReadback>(
            "resource/readback", "検証または実行の契約に違反しています。");
    auto read = table.domain_->Backend().ReadSharedBuffer(
        table.domain_->NativeDomain(), instance->resource,
        instance->availableAfter, instance->currentState);
    if (!read)
        return base::Failure<d3d12::ExternalBufferReadback, SharedResourceError>(
            Error(read.error()));
    auto update = table.UpdateAfterObservation(resource, read.value().availableAfter);
    if (!update)
        return base::Failure<d3d12::ExternalBufferReadback, SharedResourceError>(
            update.error());
    return base::Success<d3d12::ExternalBufferReadback, SharedResourceError>(
        std::move(read).value());
}

base::Expected<d3d12::ExternalTexture2DReadback, SharedResourceError>
ReadSharedTexture2DResource(
    SharedResourceTable& table,
    ResourceFlowId resource)
{
    auto* record = table.Record(resource);
    auto* instance = table.ObservableInstance(resource);
    if (!table.domain_ || !record || !instance || !instance->resource ||
        !instance->availableAfter ||
        record->kind != package::d3d12_v13::ResourceKind::Texture2D ||
        record->lifetime != ResourceFlowLifetime::SameFrame)
        return Failure<d3d12::ExternalTexture2DReadback>(
            "resource/readback-texture", "検証または実行の契約に違反しています。");
    auto read = table.domain_->Backend().ReadSharedTexture2D(
        table.domain_->NativeDomain(), instance->resource,
        instance->availableAfter, instance->currentState);
    if (!read)
        return base::Failure<d3d12::ExternalTexture2DReadback, SharedResourceError>(
            Error(read.error()));
    auto update = table.UpdateAfterObservation(resource, read.value().availableAfter);
    if (!update)
        return base::Failure<d3d12::ExternalTexture2DReadback, SharedResourceError>(
            update.error());
    return base::Success<d3d12::ExternalTexture2DReadback, SharedResourceError>(
        std::move(read).value());
}
}
