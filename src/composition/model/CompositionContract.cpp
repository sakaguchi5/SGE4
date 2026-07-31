#include "./CompositionContract.h"

#include "../../canonical/base/BinaryIO.h"
#include "../../leaf/artifact/package/PackageDigest.h"
#include "../../leaf/artifact/package/PackageReader.h"
#include "../../backends/d3d12/artifact/D3D12Encoding.h"

#include <algorithm>
#include <map>
#include <set>
#include <tuple>
#include <utility>

namespace sge4::composition
{
namespace
{
namespace pkg = package::d3d12_v13;

constexpr std::uint32_t ContractMagic = 0x3243'4753u; // "SGC2" little-endian
constexpr std::uint32_t MaximumRecords = 1'000'000;
constexpr std::string_view LeafKeyDomain = "SGE4-Level4v1-Canonical-R2-Leaf-Key-v1";
constexpr std::string_view EndpointKeyDomain = "SGE4-Level4v1-Canonical-R2-Endpoint-Key-v1";
constexpr std::string_view ResourceKeyDomain = "SGE4-Level4v1-Canonical-R2-Resource-Key-v1";

ContractError Error(std::string stage, std::string message)
{
    return {std::move(stage), std::move(message)};
}

template<class T>
base::Expected<T, ContractError> Failure(std::string stage, std::string message)
{
    return base::Failure<T, ContractError>(
        Error(std::move(stage), std::move(message)));
}

bool ValidAuthorKey(std::string_view key) noexcept
{
    if (key.empty() || key.size() > 127) return false;
    return std::all_of(key.begin(), key.end(), [](unsigned char value) {
        const bool alpha = (value >= 'A' && value <= 'Z') ||
                           (value >= 'a' && value <= 'z');
        const bool digit = value >= '0' && value <= '9';
        return alpha || digit || value == '.' || value == '_' ||
               value == '-' || value == '/';
    });
}

StableKey ComputeScopedKey(std::string_view domain, std::string_view authorKey)
{
    base::BinaryWriter writer;
    writer.WriteCountU32(domain.size());
    writer.WriteBytes(std::as_bytes(std::span<const char>(domain.data(), domain.size())));
    writer.WriteCountU32(authorKey.size());
    writer.WriteBytes(std::as_bytes(std::span<const char>(authorKey.data(), authorKey.size())));
    return base::Sha256(writer.Bytes());
}

bool ZeroKey(const StableKey& key) noexcept
{
    return std::all_of(key.begin(), key.end(), [](std::byte value) {
        return value == std::byte{0};
    });
}

void WriteDigest(base::BinaryWriter& writer, const base::Digest256& value)
{
    writer.WriteBytes(value);
}

void WriteState(base::BinaryWriter& writer, const pkg::ResourceState& value)
{
    writer.WriteU16(static_cast<std::uint16_t>(value.stateClass));
    writer.WriteU16(value.reserved);
    writer.WriteU32(value.explicitBits);
}

base::Expected<base::Digest256, ContractError> ReadDigest(base::BinaryReader& reader)
{
    auto bytes = reader.ReadBytes(base::Digest256{}.size());
    if (!bytes) return Failure<base::Digest256>("contract/read", bytes.error());
    base::Digest256 result{};
    std::copy(bytes.value().begin(), bytes.value().end(), result.begin());
    return base::Success<base::Digest256, ContractError>(result);
}

base::Expected<pkg::ResourceState, ContractError> ReadState(base::BinaryReader& reader)
{
    auto stateClass = reader.ReadU16();
    auto reserved = reader.ReadU16();
    auto bits = reader.ReadU32();
    if (!stateClass || !reserved || !bits)
        return Failure<pkg::ResourceState>("contract/read", "検証または実行の契約に違反しています。");
    pkg::ResourceState state;
    state.stateClass = static_cast<pkg::StateClass>(stateClass.value());
    state.reserved = reserved.value();
    state.explicitBits = bits.value();
    return base::Success<pkg::ResourceState, ContractError>(state);
}

struct DerivedEndpoint final
{
    std::uint32_t localExternalSlot = InvalidIndex;
    StableKey stableKey{};
    pkg::ResourceId resource;
    pkg::ResourceKind kind = pkg::ResourceKind::Buffer;
    EndpointAccess access = EndpointAccess::ReadOnly;
    pkg::Format format = pkg::Format::Unknown;
    std::uint64_t minimumBytes = 0;
    pkg::ResourceState requiredIncomingState;
    pkg::ResourceState guaranteedOutgoingState;
    pkg::ExternalSynchronizationContract synchronization =
        pkg::ExternalSynchronizationContract::CompletionTokenRequired;
    std::uint32_t flags = static_cast<std::uint32_t>(pkg::ExternalSlotFlags::Required);
};

struct DecodedLeaf final
{
    LeafPackageContract leaf;
    CanonicalLeafPackage canonical;
    std::vector<DerivedEndpoint> endpoints;
};

base::Expected<EndpointAccess, ContractError> DeriveAccess(
    const pkg::D3D12PackageView& view,
    const pkg::ExternalResourceSlotArtifact& slot)
{
    if (!slot.resource.IsValid() || slot.resource.value >= view.Resources().size())
        return Failure<EndpointAccess>("contract/leaf-interface", "Resourceが検証または実行の契約に違反しています。");
    const auto& resource = view.Resources()[slot.resource.value];
    const auto end = static_cast<std::uint64_t>(resource.firstView) + resource.viewCount;
    if (resource.origin != pkg::ResourceOrigin::External ||
        resource.resourceKind != pkg::ResourceKind::Buffer ||
        resource.viewCount == 0 || end > view.Views().size())
        return Failure<EndpointAccess>("contract/leaf-interface", "Bufferが検証または実行の契約に違反しています。");

    bool sawRead = false;
    bool sawWrite = false;
    for (std::uint32_t index = resource.firstView; index < end; ++index)
    {
        const auto& resourceView = view.Views()[index];
        if (resourceView.resource != slot.resource)
            return Failure<EndpointAccess>("contract/leaf-interface", "検証または実行の契約に違反しています。");
        if (resourceView.viewClass == pkg::ViewClass::ShaderResource) sawRead = true;
        else if (resourceView.viewClass == pkg::ViewClass::UnorderedAccess) sawWrite = true;
        else
            return Failure<EndpointAccess>(
                "contract/leaf-interface",
                "検証または実行の契約に違反しています。");
    }
    if (sawRead == sawWrite)
        return Failure<EndpointAccess>(
            "contract/leaf-interface",
            "Compositionが検証または実行の契約に違反しています。");
    if (sawRead)
    {
        if (slot.requiredIncomingState != slot.guaranteedOutgoingState)
            return Failure<EndpointAccess>(
                "contract/leaf-interface",
                "Resourceが検証または実行の契約に違反しています。");
        return base::Success<EndpointAccess, ContractError>(EndpointAccess::ReadOnly);
    }
    return base::Success<EndpointAccess, ContractError>(EndpointAccess::WriteOnly);
}

base::Expected<DecodedLeaf, ContractError> DecodeLeafWithDeclarations(
    LeafPackageDeclaration declaration)
{
    if (!ValidAuthorKey(declaration.stableKey))
        return Failure<DecodedLeaf>(
            "contract/leaf-key",
            "Leafが検証または実行の契約に違反しています。");
    if (declaration.packageBytes.empty())
        return Failure<DecodedLeaf>("contract/leaf-read", "検証または実行の契約に違反しています。");

    auto frozen = package::PackageReader::Read(declaration.packageBytes);
    if (!frozen)
        return Failure<DecodedLeaf>("contract/leaf-read", frozen.error().message);
    if (frozen.value().Target() != package::TargetKindD3D12 ||
        frozen.value().Header().targetSchemaVersion != 17 ||
        frozen.value().Header().minimumRuntimeVersion != 17)
        return Failure<DecodedLeaf>(
            "contract/leaf-schema",
            "検証または実行の契約に違反しています。");
    auto view = pkg::D3D12PackageView::Decode(frozen.value());
    if (!view)
        return Failure<DecodedLeaf>("contract/leaf-interface", view.error().message);
    if (view.value().SurfaceSlots().size() > 1)
        return Failure<DecodedLeaf>("contract/presenter", "Leafが検証または実行の契約に違反しています。");
    if (declaration.endpoints.size() != view.value().ExternalSlots().size())
        return Failure<DecodedLeaf>(
            "contract/leaf-interface",
            "Bufferが検証または実行の契約に違反しています。");

    std::map<std::uint32_t, std::string> keysBySlot;
    std::set<std::string> authoredKeys;
    for (auto& endpoint : declaration.endpoints)
    {
        if (endpoint.externalSlot >= view.value().ExternalSlots().size() ||
            !ValidAuthorKey(endpoint.stableKey) ||
            !keysBySlot.emplace(endpoint.externalSlot, endpoint.stableKey).second ||
            !authoredKeys.insert(endpoint.stableKey).second)
            return Failure<DecodedLeaf>(
                "contract/endpoint-key",
                "Endpointが検証または実行の契約に違反しています。");
    }

    DecodedLeaf result;
    result.leaf.stableKey = ComputeStableLeafKey(declaration.stableKey);
    result.leaf.targetKind = frozen.value().Target();
    result.leaf.schemaVersion = frozen.value().Header().targetSchemaVersion;
    result.leaf.minimumRuntimeVersion = frozen.value().Header().minimumRuntimeVersion;
    result.leaf.executionDigest = frozen.value().ExecutionDigest();
    result.leaf.fileDigest = package::ComputeFileDigest(frozen.value().FileBytes());
    result.leaf.targetProfileDigest = frozen.value().Header().targetProfileDigest;
    result.leaf.surfaceSlotCount = static_cast<std::uint32_t>(view.value().SurfaceSlots().size());
    result.canonical.stableKey = result.leaf.stableKey;
    result.canonical.packageBytes = std::move(declaration.packageBytes);

    std::set<StableKey> endpointKeys;
    for (std::uint32_t slotIndex = 0; slotIndex < view.value().ExternalSlots().size(); ++slotIndex)
    {
        const auto key = keysBySlot.find(slotIndex);
        if (key == keysBySlot.end())
            return Failure<DecodedLeaf>("contract/endpoint-key", "Tableが検証または実行の契約に違反しています。");
        const auto& slot = view.value().ExternalSlots()[slotIndex];
        auto access = DeriveAccess(view.value(), slot);
        if (!access) return base::Failure<DecodedLeaf, ContractError>(access.error());
        const auto stableKey = ComputeStableEndpointKey(key->second);
        if (!endpointKeys.insert(stableKey).second)
            return Failure<DecodedLeaf>("contract/endpoint-key", "検証または実行の契約に違反しています。");

        DerivedEndpoint endpoint;
        endpoint.localExternalSlot = slotIndex;
        endpoint.stableKey = stableKey;
        endpoint.resource = slot.resource;
        endpoint.kind = slot.requiredKind;
        endpoint.access = access.value();
        endpoint.format = slot.requiredFormat;
        endpoint.minimumBytes = slot.minimumBytes;
        endpoint.requiredIncomingState = slot.requiredIncomingState;
        endpoint.guaranteedOutgoingState = slot.guaranteedOutgoingState;
        endpoint.synchronization = slot.synchronizationContract;
        endpoint.flags = slot.flags;
        if (endpoint.kind != pkg::ResourceKind::Buffer ||
            endpoint.format != pkg::Format::Unknown || endpoint.minimumBytes == 0 ||
            endpoint.synchronization != pkg::ExternalSynchronizationContract::CompletionTokenRequired ||
            endpoint.flags != static_cast<std::uint32_t>(pkg::ExternalSlotFlags::Required))
            return Failure<DecodedLeaf>(
                "contract/leaf-interface",
                "Bufferが検証または実行の契約に違反しています。");
        result.endpoints.push_back(endpoint);
    }
    std::sort(result.endpoints.begin(), result.endpoints.end(), [](const auto& left, const auto& right) {
        return left.stableKey < right.stableKey;
    });
    return base::Success<DecodedLeaf, ContractError>(std::move(result));
}

base::Expected<std::pair<LeafPackageContract, std::vector<DerivedEndpoint>>, ContractError>
DecodeLeafForValidation(const CanonicalLeafPackage& leaf)
{
    if (ZeroKey(leaf.stableKey) || leaf.packageBytes.empty())
        return Failure<std::pair<LeafPackageContract, std::vector<DerivedEndpoint>>>(
            "contract/validate-leaf", "LeafがCanonicalな順序または識別子規則に違反しています。");
    auto frozen = package::PackageReader::Read(leaf.packageBytes);
    if (!frozen)
        return Failure<std::pair<LeafPackageContract, std::vector<DerivedEndpoint>>>(
            "contract/validate-leaf", frozen.error().message);
    if (frozen.value().Target() != package::TargetKindD3D12 ||
        frozen.value().Header().targetSchemaVersion != 17 ||
        frozen.value().Header().minimumRuntimeVersion != 17)
        return Failure<std::pair<LeafPackageContract, std::vector<DerivedEndpoint>>>(
            "contract/validate-leaf", "検証または実行の契約に違反しています。");
    auto view = pkg::D3D12PackageView::Decode(frozen.value());
    if (!view)
        return Failure<std::pair<LeafPackageContract, std::vector<DerivedEndpoint>>>(
            "contract/validate-leaf", view.error().message);
    if (view.value().SurfaceSlots().size() > 1)
        return Failure<std::pair<LeafPackageContract, std::vector<DerivedEndpoint>>>(
            "contract/validate-presenter", "検証または実行の契約に違反しています。");

    LeafPackageContract record;
    record.stableKey = leaf.stableKey;
    record.targetKind = frozen.value().Target();
    record.schemaVersion = frozen.value().Header().targetSchemaVersion;
    record.minimumRuntimeVersion = frozen.value().Header().minimumRuntimeVersion;
    record.executionDigest = frozen.value().ExecutionDigest();
    record.fileDigest = package::ComputeFileDigest(frozen.value().FileBytes());
    record.targetProfileDigest = frozen.value().Header().targetProfileDigest;
    record.surfaceSlotCount = static_cast<std::uint32_t>(view.value().SurfaceSlots().size());

    std::vector<DerivedEndpoint> endpoints;
    for (std::uint32_t slotIndex = 0; slotIndex < view.value().ExternalSlots().size(); ++slotIndex)
    {
        const auto& slot = view.value().ExternalSlots()[slotIndex];
        auto access = DeriveAccess(view.value(), slot);
        if (!access)
            return base::Failure<std::pair<LeafPackageContract, std::vector<DerivedEndpoint>>, ContractError>(
                access.error());
        DerivedEndpoint endpoint;
        endpoint.localExternalSlot = slotIndex;
        endpoint.resource = slot.resource;
        endpoint.kind = slot.requiredKind;
        endpoint.access = access.value();
        endpoint.format = slot.requiredFormat;
        endpoint.minimumBytes = slot.minimumBytes;
        endpoint.requiredIncomingState = slot.requiredIncomingState;
        endpoint.guaranteedOutgoingState = slot.guaranteedOutgoingState;
        endpoint.synchronization = slot.synchronizationContract;
        endpoint.flags = slot.flags;
        endpoints.push_back(endpoint);
    }
    return base::Success<std::pair<LeafPackageContract, std::vector<DerivedEndpoint>>, ContractError>(
        {std::move(record), std::move(endpoints)});
}

bool EndpointShapeMatches(
    const CompositionEndpointContract& left,
    const CompositionEndpointContract& right) noexcept
{
    return left.kind == right.kind && left.format == right.format;
}

using EndpointLookupKey = std::pair<StableKey, StableKey>;

base::Expected<CompositionEndpointId, ContractError> ResolveEndpoint(
    const EndpointReferenceDeclaration& reference,
    const std::map<EndpointLookupKey, CompositionEndpointId>& endpointIds)
{
    if (!ValidAuthorKey(reference.leafKey) || !ValidAuthorKey(reference.endpointKey))
        return Failure<CompositionEndpointId>(
            "contract/reference", "Endpointが検証または実行の契約に違反しています。");
    const EndpointLookupKey key{
        ComputeStableLeafKey(reference.leafKey),
        ComputeStableEndpointKey(reference.endpointKey)};
    const auto found = endpointIds.find(key);
    if (found == endpointIds.end())
        return Failure<CompositionEndpointId>(
            "contract/reference", "Endpointが検証または実行の契約に違反しています。");
    return base::Success<CompositionEndpointId, ContractError>(found->second);
}

std::vector<std::byte> SerializeBody(const PackageCompositionContract& contract)
{
    base::BinaryWriter writer;
    writer.WriteU32(ContractMagic);
    writer.WriteU32(CompositionContractVersion);
    writer.WriteCountU32(contract.leaves.size());
    writer.WriteCountU32(contract.endpoints.size());
    writer.WriteCountU32(contract.resources.size());
    writer.WriteCountU32(contract.bindings.size());
    writer.WriteU32(contract.presenterLeaf.value);
    writer.WriteU32(0);

    for (const auto& leaf : contract.leaves)
    {
        writer.WriteU32(leaf.id.value);
        WriteDigest(writer, leaf.stableKey);
        writer.WriteU32(leaf.targetKind);
        writer.WriteU32(leaf.schemaVersion);
        writer.WriteU32(leaf.minimumRuntimeVersion);
        WriteDigest(writer, leaf.executionDigest);
        WriteDigest(writer, leaf.fileDigest);
        WriteDigest(writer, leaf.targetProfileDigest);
        writer.WriteU32(leaf.endpointBegin);
        writer.WriteU32(leaf.endpointCount);
        writer.WriteU32(leaf.surfaceSlotCount);
        writer.WriteU32(0);
    }
    for (const auto& endpoint : contract.endpoints)
    {
        writer.WriteU32(endpoint.id.value);
        writer.WriteU32(endpoint.leaf.value);
        writer.WriteU32(endpoint.localExternalSlot);
        writer.WriteU32(endpoint.resource.value);
        WriteDigest(writer, endpoint.stableKey);
        writer.WriteU16(static_cast<std::uint16_t>(endpoint.kind));
        writer.WriteU16(static_cast<std::uint16_t>(endpoint.access));
        writer.WriteU32(static_cast<std::uint32_t>(endpoint.format));
        writer.WriteU64(endpoint.minimumBytes);
        WriteState(writer, endpoint.requiredIncomingState);
        WriteState(writer, endpoint.guaranteedOutgoingState);
        writer.WriteU32(static_cast<std::uint32_t>(endpoint.synchronization));
        writer.WriteU32(endpoint.flags);
    }
    for (const auto& resource : contract.resources)
    {
        writer.WriteU32(resource.id.value);
        WriteDigest(writer, resource.stableKey);
        writer.WriteU16(static_cast<std::uint16_t>(resource.boundary));
        writer.WriteU16(static_cast<std::uint16_t>(resource.kind));
        writer.WriteU32(static_cast<std::uint32_t>(resource.format));
        writer.WriteU64(resource.sizeBytes);
        writer.WriteU32(resource.producer.value);
        writer.WriteCountU32(resource.consumers.size());
        for (const auto consumer : resource.consumers) writer.WriteU32(consumer.value);
    }
    for (const auto& binding : contract.bindings)
    {
        writer.WriteU32(binding.endpoint.value);
        writer.WriteU32(binding.resource.value);
    }
    return std::move(writer).Take();
}

base::Expected<void, ContractError> ValidateShapeInternal(
    const PackageCompositionContract& contract)
{
    if (contract.leaves.size() < 2)
        return base::Failure<void, ContractError>(
            Error("contract/validate-leaves", "入力または内部状態が検証または実行の契約に違反しています。"));
    if (contract.leaves.size() > MaximumRecords || contract.endpoints.size() > MaximumRecords ||
        contract.resources.size() > MaximumRecords || contract.bindings.size() > MaximumRecords)
        return base::Failure<void, ContractError>(
            Error("contract/validate-count", "Contractが検証または実行の契約に違反しています。"));

    std::uint32_t endpointCursor = 0;
    std::uint32_t presenterCount = 0;
    LeafPackageId derivedPresenter;
    std::set<StableKey> leafKeys;
    for (std::uint32_t index = 0; index < contract.leaves.size(); ++index)
    {
        const auto& leaf = contract.leaves[index];
        if (leaf.id.value != index || ZeroKey(leaf.stableKey) ||
            !leafKeys.insert(leaf.stableKey).second ||
            (index > 0 && !(contract.leaves[index - 1].stableKey < leaf.stableKey)) ||
            leaf.schemaVersion != 17 || leaf.minimumRuntimeVersion != 17 ||
            leaf.endpointBegin != endpointCursor ||
            leaf.endpointCount > contract.endpoints.size() - endpointCursor ||
            leaf.surfaceSlotCount > 1)
            return base::Failure<void, ContractError>(
                Error("contract/validate-leaf", "LeafがCanonicalな順序または識別子規則に違反しています。"));
        if (leaf.surfaceSlotCount == 1)
        {
            ++presenterCount;
            derivedPresenter = leaf.id;
        }
        StableKey previousEndpointKey{};
        bool havePrevious = false;
        std::set<std::uint32_t> localSlots;
        for (std::uint32_t offset = 0; offset < leaf.endpointCount; ++offset)
        {
            const auto& endpoint = contract.endpoints[leaf.endpointBegin + offset];
            if (endpoint.id.value != leaf.endpointBegin + offset || endpoint.leaf != leaf.id ||
                endpoint.localExternalSlot == InvalidIndex || ZeroKey(endpoint.stableKey) ||
                !localSlots.insert(endpoint.localExternalSlot).second ||
                (havePrevious && !(previousEndpointKey < endpoint.stableKey)) ||
                endpoint.kind != pkg::ResourceKind::Buffer || endpoint.format != pkg::Format::Unknown ||
                endpoint.minimumBytes == 0 ||
                (endpoint.access != EndpointAccess::ReadOnly &&
                 endpoint.access != EndpointAccess::WriteOnly) ||
                endpoint.synchronization != pkg::ExternalSynchronizationContract::CompletionTokenRequired ||
                endpoint.flags != static_cast<std::uint32_t>(pkg::ExternalSlotFlags::Required) ||
                (endpoint.access == EndpointAccess::ReadOnly &&
                 endpoint.requiredIncomingState != endpoint.guaranteedOutgoingState))
                return base::Failure<void, ContractError>(
                    Error("contract/validate-endpoint", "EndpointがCanonicalな順序または識別子規則に違反しています。"));
            previousEndpointKey = endpoint.stableKey;
            havePrevious = true;
        }
        endpointCursor += leaf.endpointCount;
    }
    if (endpointCursor != contract.endpoints.size() || presenterCount > 1 ||
        (presenterCount == 0 && contract.presenterLeaf.IsValid()) ||
        (presenterCount == 1 && contract.presenterLeaf != derivedPresenter))
        return base::Failure<void, ContractError>(
            Error("contract/validate-presenter", "検証または実行の契約に違反しています。"));

    std::vector<bool> bound(contract.endpoints.size(), false);
    std::set<StableKey> resourceKeys;
    for (std::uint32_t index = 0; index < contract.resources.size(); ++index)
    {
        const auto& resource = contract.resources[index];
        if (resource.id.value != index || ZeroKey(resource.stableKey) ||
            !resourceKeys.insert(resource.stableKey).second ||
            (index > 0 && !(contract.resources[index - 1].stableKey < resource.stableKey)) ||
            (resource.boundary != ResourceBoundary::Internal &&
             resource.boundary != ResourceBoundary::CompositionInput &&
             resource.boundary != ResourceBoundary::CompositionOutput))
            return base::Failure<void, ContractError>(
                Error("contract/validate-resource", "Resource Flowが検証または実行の契約に違反しています。"));
        const bool requiresProducer = resource.boundary != ResourceBoundary::CompositionInput;
        const bool requiresConsumers = resource.boundary != ResourceBoundary::CompositionOutput;
        if ((requiresProducer && !resource.producer.IsValid()) ||
            (!requiresProducer && resource.producer.IsValid()) ||
            (requiresConsumers && resource.consumers.empty()) ||
            (!requiresConsumers && !resource.consumers.empty()) || resource.sizeBytes == 0)
            return base::Failure<void, ContractError>(
                Error("contract/validate-resource", "Resource Flowが検証または実行の契約に違反しています。"));

        std::vector<CompositionEndpointId> members;
        if (resource.producer.IsValid())
        {
            if (resource.producer.value >= contract.endpoints.size() ||
                contract.endpoints[resource.producer.value].access != EndpointAccess::WriteOnly)
                return base::Failure<void, ContractError>(
                    Error("contract/validate-access", "検証または実行の契約に違反しています。"));
            members.push_back(resource.producer);
        }
        if (!std::is_sorted(resource.consumers.begin(), resource.consumers.end(),
                [](auto left, auto right) { return left.value < right.value; }) ||
            std::adjacent_find(resource.consumers.begin(), resource.consumers.end()) != resource.consumers.end())
            return base::Failure<void, ContractError>(
                Error("contract/validate-resource", "ResourceがCanonicalな順序または識別子規則に違反しています。"));
        for (const auto consumer : resource.consumers)
        {
            if (consumer.value >= contract.endpoints.size() ||
                contract.endpoints[consumer.value].access != EndpointAccess::ReadOnly)
                return base::Failure<void, ContractError>(
                    Error("contract/validate-access", "検証または実行の契約に違反しています。"));
            members.push_back(consumer);
        }
        const auto& first = contract.endpoints[members.front().value];
        std::uint64_t requiredBytes = 0;
        for (const auto endpointId : members)
        {
            if (bound[endpointId.value])
                return base::Failure<void, ContractError>(
                    Error("contract/validate-binding", "検証または実行の契約に違反しています。"));
            const auto& endpoint = contract.endpoints[endpointId.value];
            if (!EndpointShapeMatches(first, endpoint))
                return base::Failure<void, ContractError>(
                    Error("contract/validate-shape", "Resource Flowが検証または実行の契約に違反しています。"));
            requiredBytes = std::max(requiredBytes, endpoint.minimumBytes);
            bound[endpointId.value] = true;
        }
        if (resource.kind != first.kind || resource.format != first.format ||
            resource.sizeBytes != requiredBytes)
            return base::Failure<void, ContractError>(
                Error("contract/validate-shape", "Resourceが検証または実行の契約に違反しています。"));
    }
    if (std::any_of(bound.begin(), bound.end(), [](bool value) { return !value; }))
        return base::Failure<void, ContractError>(
            Error("contract/validate-binding", "Endpointが検証または実行の契約に違反しています。"));
    if (contract.bindings.size() != contract.endpoints.size())
        return base::Failure<void, ContractError>(
            Error("contract/validate-binding", "検証または実行の契約に違反しています。"));
    for (std::uint32_t index = 0; index < contract.bindings.size(); ++index)
    {
        const auto& binding = contract.bindings[index];
        if (binding.endpoint.value != index || binding.resource.value >= contract.resources.size())
            return base::Failure<void, ContractError>(
                Error("contract/validate-binding", "検証または実行の契約に違反しています。"));
        const auto& resource = contract.resources[binding.resource.value];
        const bool producer = resource.producer == binding.endpoint;
        const bool consumer = std::binary_search(
            resource.consumers.begin(), resource.consumers.end(), binding.endpoint,
            [](auto left, auto right) { return left.value < right.value; });
        if (producer == consumer)
            return base::Failure<void, ContractError>(
                Error("contract/validate-binding", "検証または実行の契約に違反しています。"));
    }
    if (contract.identity != ComputeCompositionContractIdentity(contract))
        return base::Failure<void, ContractError>(
            Error("contract/validate-identity", "Compositionが検証または実行の契約に違反しています。"));
    return base::Success<void, ContractError>();
}
}

StableKey ComputeStableLeafKey(std::string_view authorKey)
{
    return ComputeScopedKey(LeafKeyDomain, authorKey);
}

StableKey ComputeStableEndpointKey(std::string_view authorKey)
{
    return ComputeScopedKey(EndpointKeyDomain, authorKey);
}

StableKey ComputeStableResourceKey(std::string_view authorKey)
{
    return ComputeScopedKey(ResourceKeyDomain, authorKey);
}

base::Expected<ValidatedCompositionContract, ContractError>
BuildCompositionContract(ContractBuildInput input)
{
    if (input.leaves.size() < 2)
        return Failure<ValidatedCompositionContract>(
            "contract/leaves", "Compositionが検証または実行の契約に違反しています。");
    if (input.resources.empty())
        return Failure<ValidatedCompositionContract>(
            "contract/resources", "Compositionが検証または実行の契約に違反しています。");

    std::vector<DecodedLeaf> decodedLeaves;
    decodedLeaves.reserve(input.leaves.size());
    for (auto& declaration : input.leaves)
    {
        auto decoded = DecodeLeafWithDeclarations(std::move(declaration));
        if (!decoded)
            return base::Failure<ValidatedCompositionContract, ContractError>(decoded.error());
        decodedLeaves.push_back(std::move(decoded).value());
    }
    std::sort(decodedLeaves.begin(), decodedLeaves.end(), [](const auto& left, const auto& right) {
        return left.leaf.stableKey < right.leaf.stableKey;
    });
    if (std::adjacent_find(decodedLeaves.begin(), decodedLeaves.end(), [](const auto& left, const auto& right) {
            return left.leaf.stableKey == right.leaf.stableKey;
        }) != decodedLeaves.end())
        return Failure<ValidatedCompositionContract>("contract/leaves", "Leafが検証または実行の契約に違反しています。");

    PackageCompositionContract contract;
    std::vector<CanonicalLeafPackage> canonicalLeaves;
    std::map<EndpointLookupKey, CompositionEndpointId> endpointIds;
    std::uint32_t surfaceCount = 0;
    for (std::uint32_t leafIndex = 0; leafIndex < decodedLeaves.size(); ++leafIndex)
    {
        auto& decoded = decodedLeaves[leafIndex];
        decoded.leaf.id = {leafIndex};
        decoded.leaf.endpointBegin = static_cast<std::uint32_t>(contract.endpoints.size());
        decoded.leaf.endpointCount = static_cast<std::uint32_t>(decoded.endpoints.size());
        if (decoded.leaf.surfaceSlotCount == 1)
        {
            ++surfaceCount;
            contract.presenterLeaf = decoded.leaf.id;
        }
        contract.leaves.push_back(decoded.leaf);
        canonicalLeaves.push_back(std::move(decoded.canonical));
        for (auto& source : decoded.endpoints)
        {
            CompositionEndpointContract endpoint;
            endpoint.id = {static_cast<std::uint32_t>(contract.endpoints.size())};
            endpoint.leaf = decoded.leaf.id;
            endpoint.localExternalSlot = source.localExternalSlot;
            endpoint.stableKey = source.stableKey;
            endpoint.resource = source.resource;
            endpoint.kind = source.kind;
            endpoint.access = source.access;
            endpoint.format = source.format;
            endpoint.minimumBytes = source.minimumBytes;
            endpoint.requiredIncomingState = source.requiredIncomingState;
            endpoint.guaranteedOutgoingState = source.guaranteedOutgoingState;
            endpoint.synchronization = source.synchronization;
            endpoint.flags = source.flags;
            if (!endpointIds.emplace(
                    EndpointLookupKey{decoded.leaf.stableKey, endpoint.stableKey}, endpoint.id).second)
                return Failure<ValidatedCompositionContract>(
                    "contract/endpoints", "Endpointが検証または実行の契約に違反しています。");
            contract.endpoints.push_back(endpoint);
        }
    }
    if (surfaceCount > 1)
        return Failure<ValidatedCompositionContract>(
            "contract/presenter", "検証または実行の契約に違反しています。");

    struct PendingResource final
    {
        StableKey stableKey{};
        ResourceFlowDeclaration declaration;
    };
    std::vector<PendingResource> pending;
    std::set<StableKey> resourceKeys;
    for (auto& declaration : input.resources)
    {
        if (!ValidAuthorKey(declaration.stableKey))
            return Failure<ValidatedCompositionContract>(
                "contract/resource-key", "Resource Flowが検証または実行の契約に違反しています。");
        const auto key = ComputeStableResourceKey(declaration.stableKey);
        if (!resourceKeys.insert(key).second)
            return Failure<ValidatedCompositionContract>(
                "contract/resource-key", "Resource Flowが検証または実行の契約に違反しています。");
        pending.push_back({key, std::move(declaration)});
    }
    std::sort(pending.begin(), pending.end(), [](const auto& left, const auto& right) {
        return left.stableKey < right.stableKey;
    });

    std::vector<bool> bound(contract.endpoints.size(), false);
    for (auto& item : pending)
    {
        ResourceFlowContract resource;
        resource.id = {static_cast<std::uint32_t>(contract.resources.size())};
        resource.stableKey = item.stableKey;
        resource.boundary = item.declaration.boundary;
        const bool needsProducer = resource.boundary != ResourceBoundary::CompositionInput;
        const bool needsConsumers = resource.boundary != ResourceBoundary::CompositionOutput;
        if ((needsProducer && !item.declaration.producer.has_value()) ||
            (!needsProducer && item.declaration.producer.has_value()) ||
            (needsConsumers && item.declaration.consumers.empty()) ||
            (!needsConsumers && !item.declaration.consumers.empty()))
            return Failure<ValidatedCompositionContract>(
                "contract/resource-shape", "検証または実行の契約に違反しています。");

        std::vector<CompositionEndpointId> members;
        if (item.declaration.producer)
        {
            auto resolved = ResolveEndpoint(*item.declaration.producer, endpointIds);
            if (!resolved)
                return base::Failure<ValidatedCompositionContract, ContractError>(resolved.error());
            if (contract.endpoints[resolved.value().value].access != EndpointAccess::WriteOnly)
                return Failure<ValidatedCompositionContract>(
                    "contract/access", "Resource Flowが検証または実行の契約に違反しています。");
            resource.producer = resolved.value();
            members.push_back(resolved.value());
        }
        for (const auto& reference : item.declaration.consumers)
        {
            auto resolved = ResolveEndpoint(reference, endpointIds);
            if (!resolved)
                return base::Failure<ValidatedCompositionContract, ContractError>(resolved.error());
            if (contract.endpoints[resolved.value().value].access != EndpointAccess::ReadOnly)
                return Failure<ValidatedCompositionContract>(
                    "contract/access", "Resource Flowが検証または実行の契約に違反しています。");
            resource.consumers.push_back(resolved.value());
            members.push_back(resolved.value());
        }
        std::sort(resource.consumers.begin(), resource.consumers.end(), [](auto left, auto right) {
            return left.value < right.value;
        });
        if (std::adjacent_find(resource.consumers.begin(), resource.consumers.end()) != resource.consumers.end())
            return Failure<ValidatedCompositionContract>(
                "contract/resource-shape", "Resource Flowが検証または実行の契約に違反しています。");
        const auto& first = contract.endpoints[members.front().value];
        resource.kind = first.kind;
        resource.format = first.format;
        for (const auto endpointId : members)
        {
            if (bound[endpointId.value])
                return Failure<ValidatedCompositionContract>(
                    "contract/binding", "Resource Flowが検証または実行の契約に違反しています。");
            const auto& endpoint = contract.endpoints[endpointId.value];
            if (!EndpointShapeMatches(first, endpoint))
                return Failure<ValidatedCompositionContract>(
                    "contract/shape", "検証または実行の契約に違反しています。");
            resource.sizeBytes = std::max(resource.sizeBytes, endpoint.minimumBytes);
            bound[endpointId.value] = true;
        }
        contract.resources.push_back(std::move(resource));
    }
    if (std::any_of(bound.begin(), bound.end(), [](bool value) { return !value; }))
        return Failure<ValidatedCompositionContract>(
            "contract/binding", "Endpointが検証または実行の契約に違反しています。");

    contract.bindings.resize(contract.endpoints.size());
    for (const auto& resource : contract.resources)
    {
        if (resource.producer.IsValid())
            contract.bindings[resource.producer.value] = {resource.producer, resource.id};
        for (const auto consumer : resource.consumers)
            contract.bindings[consumer.value] = {consumer, resource.id};
    }
    contract.identity = ComputeCompositionContractIdentity(contract);
    auto validation = ValidateShapeInternal(contract);
    if (!validation)
        return base::Failure<ValidatedCompositionContract, ContractError>(validation.error());
    return base::Success<ValidatedCompositionContract, ContractError>(
        ValidatedCompositionContract(
            std::move(contract), std::move(canonicalLeaves),
            ValidatedCompositionContract::ConstructionToken{}));
}

base::Expected<ValidatedCompositionContract, ContractError>
ValidateCompositionContractAgainstLeaves(
    PackageCompositionContract contract,
    std::vector<CanonicalLeafPackage> leaves)
{
    auto shape = ValidateShapeInternal(contract);
    if (!shape)
        return base::Failure<ValidatedCompositionContract, ContractError>(shape.error());
    std::sort(leaves.begin(), leaves.end(), [](const auto& left, const auto& right) {
        return left.stableKey < right.stableKey;
    });
    if (leaves.size() != contract.leaves.size())
        return Failure<ValidatedCompositionContract>(
            "contract/validate-leaf", "検証または実行の契約に違反しています。");

    std::uint32_t surfaceCount = 0;
    LeafPackageId presenter;
    for (std::uint32_t leafIndex = 0; leafIndex < leaves.size(); ++leafIndex)
    {
        if (leaves[leafIndex].stableKey != contract.leaves[leafIndex].stableKey)
            return Failure<ValidatedCompositionContract>(
                "contract/validate-leaf", "検証または実行の契約に違反しています。");
        auto decoded = DecodeLeafForValidation(leaves[leafIndex]);
        if (!decoded)
            return base::Failure<ValidatedCompositionContract, ContractError>(decoded.error());
        auto sourceLeaf = std::move(decoded).value();
        const auto& expectedLeaf = contract.leaves[leafIndex];
        const auto& actualLeaf = sourceLeaf.first;
        if (expectedLeaf.id.value != leafIndex || expectedLeaf.stableKey != actualLeaf.stableKey ||
            expectedLeaf.targetKind != actualLeaf.targetKind ||
            expectedLeaf.schemaVersion != actualLeaf.schemaVersion ||
            expectedLeaf.minimumRuntimeVersion != actualLeaf.minimumRuntimeVersion ||
            expectedLeaf.executionDigest != actualLeaf.executionDigest ||
            expectedLeaf.fileDigest != actualLeaf.fileDigest ||
            expectedLeaf.targetProfileDigest != actualLeaf.targetProfileDigest ||
            expectedLeaf.endpointCount != sourceLeaf.second.size() ||
            expectedLeaf.surfaceSlotCount != actualLeaf.surfaceSlotCount)
            return Failure<ValidatedCompositionContract>(
                "contract/validate-leaf", "検証または実行の契約に違反しています。");
        if (actualLeaf.surfaceSlotCount == 1)
        {
            ++surfaceCount;
            presenter = expectedLeaf.id;
        }

        std::vector<bool> slots(sourceLeaf.second.size(), false);
        for (std::uint32_t offset = 0; offset < expectedLeaf.endpointCount; ++offset)
        {
            const auto& endpoint = contract.endpoints[expectedLeaf.endpointBegin + offset];
            if (endpoint.localExternalSlot >= sourceLeaf.second.size() ||
                slots[endpoint.localExternalSlot])
                return Failure<ValidatedCompositionContract>(
                    "contract/validate-endpoint", "Endpointが検証または実行の契約に違反しています。");
            slots[endpoint.localExternalSlot] = true;
            const auto& source = sourceLeaf.second[endpoint.localExternalSlot];
            if (endpoint.resource != source.resource || endpoint.kind != source.kind ||
                endpoint.access != source.access || endpoint.format != source.format ||
                endpoint.minimumBytes != source.minimumBytes ||
                endpoint.requiredIncomingState != source.requiredIncomingState ||
                endpoint.guaranteedOutgoingState != source.guaranteedOutgoingState ||
                endpoint.synchronization != source.synchronization || endpoint.flags != source.flags)
                return Failure<ValidatedCompositionContract>(
                    "contract/validate-endpoint",
                    "検証または実行の契約に違反しています。");
        }
        if (std::any_of(slots.begin(), slots.end(), [](bool value) { return !value; }))
            return Failure<ValidatedCompositionContract>(
                "contract/validate-endpoint", "検証または実行の契約に違反しています。");
    }
    if (surfaceCount > 1 ||
        (surfaceCount == 0 && contract.presenterLeaf.IsValid()) ||
        (surfaceCount == 1 && contract.presenterLeaf != presenter))
        return Failure<ValidatedCompositionContract>(
            "contract/validate-presenter", "Leafが検証または実行の契約に違反しています。");

    return base::Success<ValidatedCompositionContract, ContractError>(
        ValidatedCompositionContract(
            std::move(contract), std::move(leaves),
            ValidatedCompositionContract::ConstructionToken{}));
}

base::Expected<void, ContractError>
ValidateCompositionContractShape(const PackageCompositionContract& contract)
{
    return ValidateShapeInternal(contract);
}

std::vector<std::byte>
SerializeCompositionContract(const PackageCompositionContract& contract)
{
    auto bytes = SerializeBody(contract);
    bytes.insert(bytes.end(), contract.identity.begin(), contract.identity.end());
    return bytes;
}

base::Digest256
ComputeCompositionContractIdentity(const PackageCompositionContract& contract)
{
    const auto body = SerializeBody(contract);
    return base::Sha256(body);
}

base::Expected<PackageCompositionContract, ContractError>
DeserializeCompositionContract(std::span<const std::byte> bytes)
{
    if (bytes.size() < 32 + base::Digest256{}.size())
        return Failure<PackageCompositionContract>("contract/read", "検証または実行の契約に違反しています。");
    base::BinaryReader reader(bytes);
    auto magic = reader.ReadU32();
    auto version = reader.ReadU32();
    auto leafCount = reader.ReadU32();
    auto endpointCount = reader.ReadU32();
    auto resourceCount = reader.ReadU32();
    auto bindingCount = reader.ReadU32();
    auto presenter = reader.ReadU32();
    auto reserved = reader.ReadU32();
    if (!magic || !version || !leafCount || !endpointCount || !resourceCount ||
        !bindingCount || !presenter || !reserved || magic.value() != ContractMagic ||
        version.value() != CompositionContractVersion || reserved.value() != 0 ||
        leafCount.value() > MaximumRecords || endpointCount.value() > MaximumRecords ||
        resourceCount.value() > MaximumRecords || bindingCount.value() > MaximumRecords)
        return Failure<PackageCompositionContract>("contract/read", "Contractが検証または実行の契約に違反しています。");

    PackageCompositionContract contract;
    contract.presenterLeaf = {presenter.value()};
    for (std::uint32_t index = 0; index < leafCount.value(); ++index)
    {
        LeafPackageContract leaf;
        auto id = reader.ReadU32();
        auto stableKey = ReadDigest(reader);
        auto target = reader.ReadU32();
        auto schema = reader.ReadU32();
        auto runtime = reader.ReadU32();
        auto execution = ReadDigest(reader);
        auto file = ReadDigest(reader);
        auto profile = ReadDigest(reader);
        auto begin = reader.ReadU32();
        auto count = reader.ReadU32();
        auto surfaces = reader.ReadU32();
        auto tail = reader.ReadU32();
        if (!id || !stableKey || !target || !schema || !runtime || !execution || !file ||
            !profile || !begin || !count || !surfaces || !tail || tail.value() != 0)
            return Failure<PackageCompositionContract>("contract/read", "検証または実行の契約に違反しています。");
        leaf.id = {id.value()};
        leaf.stableKey = stableKey.value();
        leaf.targetKind = target.value();
        leaf.schemaVersion = schema.value();
        leaf.minimumRuntimeVersion = runtime.value();
        leaf.executionDigest = execution.value();
        leaf.fileDigest = file.value();
        leaf.targetProfileDigest = profile.value();
        leaf.endpointBegin = begin.value();
        leaf.endpointCount = count.value();
        leaf.surfaceSlotCount = surfaces.value();
        contract.leaves.push_back(leaf);
    }
    for (std::uint32_t index = 0; index < endpointCount.value(); ++index)
    {
        CompositionEndpointContract endpoint;
        auto id = reader.ReadU32();
        auto leaf = reader.ReadU32();
        auto slot = reader.ReadU32();
        auto resource = reader.ReadU32();
        auto stableKey = ReadDigest(reader);
        auto kind = reader.ReadU16();
        auto access = reader.ReadU16();
        auto format = reader.ReadU32();
        auto size = reader.ReadU64();
        auto incoming = ReadState(reader);
        auto outgoing = ReadState(reader);
        auto synchronization = reader.ReadU32();
        auto flags = reader.ReadU32();
        if (!id || !leaf || !slot || !resource || !stableKey || !kind || !access ||
            !format || !size || !incoming || !outgoing || !synchronization || !flags)
            return Failure<PackageCompositionContract>("contract/read", "Endpointが検証または実行の契約に違反しています。");
        endpoint.id = {id.value()};
        endpoint.leaf = {leaf.value()};
        endpoint.localExternalSlot = slot.value();
        endpoint.resource = {resource.value()};
        endpoint.stableKey = stableKey.value();
        endpoint.kind = static_cast<pkg::ResourceKind>(kind.value());
        endpoint.access = static_cast<EndpointAccess>(access.value());
        endpoint.format = static_cast<pkg::Format>(format.value());
        endpoint.minimumBytes = size.value();
        endpoint.requiredIncomingState = incoming.value();
        endpoint.guaranteedOutgoingState = outgoing.value();
        endpoint.synchronization = static_cast<pkg::ExternalSynchronizationContract>(synchronization.value());
        endpoint.flags = flags.value();
        contract.endpoints.push_back(endpoint);
    }
    for (std::uint32_t index = 0; index < resourceCount.value(); ++index)
    {
        ResourceFlowContract resource;
        auto id = reader.ReadU32();
        auto stableKey = ReadDigest(reader);
        auto boundary = reader.ReadU16();
        auto kind = reader.ReadU16();
        auto format = reader.ReadU32();
        auto size = reader.ReadU64();
        auto producer = reader.ReadU32();
        auto consumers = reader.ReadU32();
        if (!id || !stableKey || !boundary || !kind || !format || !size || !producer ||
            !consumers || consumers.value() > MaximumRecords)
            return Failure<PackageCompositionContract>("contract/read", "Resource Flowが検証または実行の契約に違反しています。");
        resource.id = {id.value()};
        resource.stableKey = stableKey.value();
        resource.boundary = static_cast<ResourceBoundary>(boundary.value());
        resource.kind = static_cast<pkg::ResourceKind>(kind.value());
        resource.format = static_cast<pkg::Format>(format.value());
        resource.sizeBytes = size.value();
        resource.producer = {producer.value()};
        for (std::uint32_t consumerIndex = 0; consumerIndex < consumers.value(); ++consumerIndex)
        {
            auto consumer = reader.ReadU32();
            if (!consumer)
                return Failure<PackageCompositionContract>("contract/read", "Resourceが検証または実行の契約に違反しています。");
            resource.consumers.push_back({consumer.value()});
        }
        contract.resources.push_back(std::move(resource));
    }
    for (std::uint32_t index = 0; index < bindingCount.value(); ++index)
    {
        auto endpoint = reader.ReadU32();
        auto resource = reader.ReadU32();
        if (!endpoint || !resource)
            return Failure<PackageCompositionContract>("contract/read", "Bindingが検証または実行の契約に違反しています。");
        contract.bindings.push_back({{endpoint.value()}, {resource.value()}});
    }
    auto identity = ReadDigest(reader);
    if (!identity || reader.Remaining() != 0)
        return Failure<PackageCompositionContract>("contract/read", "Contractが検証または実行の契約に違反しています。");
    contract.identity = identity.value();
    auto validation = ValidateShapeInternal(contract);
    if (!validation)
        return base::Failure<PackageCompositionContract, ContractError>(validation.error());
    return base::Success<PackageCompositionContract, ContractError>(std::move(contract));
}
}
