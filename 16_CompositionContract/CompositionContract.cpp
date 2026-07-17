#include "CompositionContract.h"

#include "../00_Foundation/BinaryIO.h"
#include "../09_FrozenPackageCore/PackageDigest.h"
#include "../09_FrozenPackageCore/PackageReader.h"
#include "../10_D3D12PackageSchema/D3D12Encoding.h"

#include <algorithm>
#include <map>
#include <set>
#include <tuple>

namespace sge4::composition
{
namespace
{
ContractError Error(std::string stage, std::string message) { return {std::move(stage), std::move(message)}; }

void WriteDigest(base::BinaryWriter& writer, const base::Digest256& value) { writer.WriteBytes(value); }
void WriteState(base::BinaryWriter& writer, const package::d3d12_v13::ResourceState& value)
{
    writer.WriteU16(static_cast<std::uint16_t>(value.stateClass));
    writer.WriteU16(value.reserved);
    writer.WriteU32(value.explicitBits);
}

template<class T>
bool UniqueAuthorKeys(const std::vector<T>& values)
{
    std::set<std::uint64_t> keys;
    for (const auto& value : values) if (value.authorKey == 0 || !keys.insert(value.authorKey).second) return false;
    return true;
}
}

base::Digest256 ComputeContractIdentity(const PackageCompositionContract& contract)
{
    base::BinaryWriter writer;
    writer.WriteU32(1);
    writer.WriteU32(static_cast<std::uint32_t>(contract.leaves.size()));
    writer.WriteU32(static_cast<std::uint32_t>(contract.resources.size()));
    writer.WriteU32(static_cast<std::uint32_t>(contract.endpoints.size()));
    writer.WriteU32(static_cast<std::uint32_t>(contract.bindings.size()));
    writer.WriteU32(static_cast<std::uint32_t>(contract.links.size()));
    for (const auto& leaf : contract.leaves)
    {
        writer.WriteU32(leaf.id.value); writer.WriteU64(leaf.authorKey);
        writer.WriteU32(leaf.targetKind); writer.WriteU32(leaf.schemaVersion); writer.WriteU32(leaf.minimumRuntimeVersion);
        WriteDigest(writer, leaf.executionDigest); WriteDigest(writer, leaf.fileDigest); WriteDigest(writer, leaf.targetProfileDigest);
        writer.WriteU32(leaf.surfaceSlotCount); writer.WriteU32(static_cast<std::uint32_t>(leaf.externalSlots.size()));
        for (const auto& slot : leaf.externalSlots)
        {
            writer.WriteU32(slot.slot); writer.WriteU16(static_cast<std::uint16_t>(slot.kind)); writer.WriteU16(0);
            writer.WriteU32(static_cast<std::uint32_t>(slot.format)); writer.WriteU64(slot.minimumBytes);
            WriteState(writer, slot.incomingState); WriteState(writer, slot.outgoingState);
            writer.WriteU32(static_cast<std::uint32_t>(slot.synchronization));
        }
    }
    for (const auto& resource : contract.resources)
    {
        writer.WriteU32(resource.id.value); writer.WriteU64(resource.authorKey);
        writer.WriteU16(static_cast<std::uint16_t>(resource.kind)); writer.WriteU8(static_cast<std::uint8_t>(resource.boundary));
        writer.WriteU8(static_cast<std::uint8_t>(resource.ownership)); writer.WriteU32(static_cast<std::uint32_t>(resource.format));
        writer.WriteU64(resource.sizeBytes); WriteState(writer, resource.initialState);
    }
    for (const auto& endpoint : contract.endpoints)
    {
        writer.WriteU32(endpoint.id.value); writer.WriteU32(endpoint.leaf.value); writer.WriteU32(endpoint.externalSlot);
        writer.WriteU8(static_cast<std::uint8_t>(endpoint.direction)); writer.WriteZeroes(3);
    }
    for (const auto& binding : contract.bindings) { writer.WriteU32(binding.resource.value); writer.WriteU32(binding.endpoint.value); }
    for (const auto& link : contract.links)
    {
        writer.WriteU32(link.id.value); writer.WriteU32(link.resource.value); writer.WriteU32(link.producer.value); writer.WriteU32(link.consumer.value);
        WriteState(writer, link.handoffState);
    }
    return base::Sha256(writer.Bytes());
}

base::Result<PackageCompositionContract, ContractError> BuildCompositionContract(ContractBuildInput input)
{
    if (input.leaves.size() < 2) return base::Result<PackageCompositionContract, ContractError>::Failure(Error("contract/leaves", "Level 4 v1 requires at least two Leaf Packages"));
    if (!UniqueAuthorKeys(input.leaves) || !UniqueAuthorKeys(input.resources))
        return base::Result<PackageCompositionContract, ContractError>::Failure(Error("contract/identity", "Leaf and shared-resource author keys must be non-zero and unique"));

    PackageCompositionContract result;
    for (auto& declaration : input.leaves)
    {
        auto package = package::PackageReader::Read(declaration.packageBytes);
        if (!package) return base::Result<PackageCompositionContract, ContractError>::Failure(Error("contract/leaf-read", package.Error().message));
        auto view = package::d3d12_v13::D3D12PackageView::Decode(package.Value());
        if (!view) return base::Result<PackageCompositionContract, ContractError>::Failure(Error("contract/leaf-decode", view.Error().message));
        LeafPackageContract leaf;
        leaf.authorKey = declaration.authorKey;
        leaf.targetKind = package.Value().Target();
        leaf.schemaVersion = package.Value().Header().targetSchemaVersion;
        leaf.minimumRuntimeVersion = package.Value().Header().minimumRuntimeVersion;
        leaf.executionDigest = package.Value().ExecutionDigest();
        leaf.fileDigest = package::ComputeFileDigest(package.Value().FileBytes());
        leaf.targetProfileDigest = package.Value().Header().targetProfileDigest;
        leaf.surfaceSlotCount = static_cast<std::uint32_t>(view.Value().SurfaceSlots().size());
        leaf.packageBytes = std::move(declaration.packageBytes);
        for (const auto& source : view.Value().ExternalSlots())
            leaf.externalSlots.push_back({source.id.value, source.requiredKind, source.requiredFormat, source.minimumBytes,
                source.requiredIncomingState, source.guaranteedOutgoingState, source.synchronizationContract});
        result.leaves.push_back(std::move(leaf));
    }
    std::sort(result.leaves.begin(), result.leaves.end(), [](const auto& a, const auto& b) {
        return std::tie(a.executionDigest, a.fileDigest, a.authorKey) < std::tie(b.executionDigest, b.fileDigest, b.authorKey);
    });
    std::map<std::uint64_t, LeafPackageId> leafIds;
    for (std::uint32_t index = 0; index < result.leaves.size(); ++index)
    {
        result.leaves[index].id = {index};
        if (!leafIds.emplace(result.leaves[index].authorKey, LeafPackageId{index}).second)
            return base::Result<PackageCompositionContract, ContractError>::Failure(Error("contract/leaf", "duplicate Leaf identity"));
    }

    std::sort(input.resources.begin(), input.resources.end(), [](const auto& a, const auto& b) { return a.authorKey < b.authorKey; });
    std::map<std::uint64_t, SharedResourceId> resourceIds;
    for (std::uint32_t index = 0; index < input.resources.size(); ++index)
    {
        const auto& source = input.resources[index];
        if (source.sizeBytes == 0 || source.initialState.reserved != 0)
            return base::Result<PackageCompositionContract, ContractError>::Failure(Error("contract/resource", "shared resource size and state must be valid"));
        result.resources.push_back({{index}, source.authorKey, package::d3d12_v13::ResourceKind::Buffer, source.format,
            source.sizeBytes, source.initialState, source.boundary, SharedResourceOwnership::DeviceDomain});
        resourceIds.emplace(source.authorKey, SharedResourceId{index});
    }

    std::sort(input.endpoints.begin(), input.endpoints.end(), [](const auto& a, const auto& b) {
        return std::tie(a.leafAuthorKey, a.externalSlot) < std::tie(b.leafAuthorKey, b.externalSlot);
    });
    std::map<std::pair<std::uint64_t, std::uint32_t>, CompositionEndpointId> endpointIds;
    for (const auto& source : input.endpoints)
    {
        const auto leaf = leafIds.find(source.leafAuthorKey);
        if (leaf == leafIds.end() || source.externalSlot >= result.leaves[leaf->second.value].externalSlots.size())
            return base::Result<PackageCompositionContract, ContractError>::Failure(Error("contract/endpoint", "endpoint references an unknown Leaf external slot"));
        if (source.direction != EndpointDirection::Import && source.direction != EndpointDirection::Export && source.direction != EndpointDirection::ImportExport)
            return base::Result<PackageCompositionContract, ContractError>::Failure(Error("contract/endpoint", "endpoint direction is invalid"));
        const CompositionEndpointId id{static_cast<std::uint32_t>(result.endpoints.size())};
        if (!endpointIds.emplace(std::pair{source.leafAuthorKey, source.externalSlot}, id).second)
            return base::Result<PackageCompositionContract, ContractError>::Failure(Error("contract/endpoint", "external slot is declared more than once"));
        result.endpoints.push_back({id, leaf->second, source.externalSlot, source.direction,
            result.leaves[leaf->second.value].externalSlots[source.externalSlot]});
    }

    for (const auto& leaf : result.leaves)
        for (const auto& slot : leaf.externalSlots)
            if (!endpointIds.contains({leaf.authorKey, slot.slot}))
                return base::Result<PackageCompositionContract, ContractError>::Failure(Error("contract/endpoint", "every required Leaf external slot must be explicitly declared"));

    std::set<std::uint32_t> boundEndpoints;
    for (const auto& source : input.bindings)
    {
        const auto resource = resourceIds.find(source.resourceAuthorKey);
        const auto endpoint = endpointIds.find({source.leafAuthorKey, source.externalSlot});
        if (resource == resourceIds.end() || endpoint == endpointIds.end() || !boundEndpoints.insert(endpoint->second.value).second)
            return base::Result<PackageCompositionContract, ContractError>::Failure(Error("contract/binding", "binding is unknown or duplicates an endpoint"));
        result.bindings.push_back({resource->second, endpoint->second});
    }
    if (boundEndpoints.size() != result.endpoints.size())
        return base::Result<PackageCompositionContract, ContractError>::Failure(Error("contract/binding", "every required endpoint must be bound exactly once"));
    std::sort(result.bindings.begin(), result.bindings.end(), [](const auto& a, const auto& b) { return a.endpoint.value < b.endpoint.value; });

    for (const auto& source : input.links)
    {
        const auto resource = resourceIds.find(source.resourceAuthorKey);
        const auto producer = endpointIds.find({source.producerLeafAuthorKey, source.producerSlot});
        const auto consumer = endpointIds.find({source.consumerLeafAuthorKey, source.consumerSlot});
        if (resource == resourceIds.end() || producer == endpointIds.end() || consumer == endpointIds.end())
            return base::Result<PackageCompositionContract, ContractError>::Failure(Error("contract/link", "link references an unknown resource or endpoint"));
        const auto state = result.endpoints[producer->second.value].contract.outgoingState;
        result.links.push_back({{}, resource->second, producer->second, consumer->second, state});
    }
    std::sort(result.links.begin(), result.links.end(), [](const auto& a, const auto& b) {
        return std::tie(a.resource.value, a.producer.value, a.consumer.value) < std::tie(b.resource.value, b.producer.value, b.consumer.value);
    });
    for (std::uint32_t index = 0; index < result.links.size(); ++index) result.links[index].id = {index};
    result.identity = ComputeContractIdentity(result);
    return base::Result<PackageCompositionContract, ContractError>::Success(std::move(result));
}
}
