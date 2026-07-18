#include "CompositionContract.h"

#include "../00_Foundation/BinaryIO.h"
#include "../09_FrozenPackageCore/PackageDigest.h"
#include "../09_FrozenPackageCore/PackageReader.h"
#include "../10_D3D12PackageSchema/D3D12Encoding.h"

#include <algorithm>
#include <map>
#include <set>
#include <tuple>
#include <utility>

namespace sge4::composition::canonical
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
base::Result<T, ContractError> Failure(std::string stage, std::string message)
{
    return base::Result<T, ContractError>::Failure(
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
    writer.WriteU32(static_cast<std::uint32_t>(domain.size()));
    writer.WriteBytes(std::as_bytes(std::span<const char>(domain.data(), domain.size())));
    writer.WriteU32(static_cast<std::uint32_t>(authorKey.size()));
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

base::Result<base::Digest256, ContractError> ReadDigest(base::BinaryReader& reader)
{
    auto bytes = reader.ReadBytes(base::Digest256{}.size());
    if (!bytes) return Failure<base::Digest256>("contract/read", bytes.Error());
    base::Digest256 result{};
    std::copy(bytes.Value().begin(), bytes.Value().end(), result.begin());
    return base::Result<base::Digest256, ContractError>::Success(result);
}

base::Result<pkg::ResourceState, ContractError> ReadState(base::BinaryReader& reader)
{
    auto stateClass = reader.ReadU16();
    auto reserved = reader.ReadU16();
    auto bits = reader.ReadU32();
    if (!stateClass || !reserved || !bits)
        return Failure<pkg::ResourceState>("contract/read", "truncated ResourceState");
    pkg::ResourceState state;
    state.stateClass = static_cast<pkg::StateClass>(stateClass.Value());
    state.reserved = reserved.Value();
    state.explicitBits = bits.Value();
    return base::Result<pkg::ResourceState, ContractError>::Success(state);
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

base::Result<EndpointAccess, ContractError> DeriveAccess(
    const pkg::D3D12PackageView& view,
    const pkg::ExternalResourceSlotArtifact& slot)
{
    if (!slot.resource.IsValid() || slot.resource.value >= view.Resources().size())
        return Failure<EndpointAccess>("contract/leaf-interface", "External slot Resource is invalid");
    const auto& resource = view.Resources()[slot.resource.value];
    const auto end = static_cast<std::uint64_t>(resource.firstView) + resource.viewCount;
    if (resource.origin != pkg::ResourceOrigin::External ||
        resource.resourceKind != pkg::ResourceKind::Buffer ||
        resource.viewCount == 0 || end > view.Views().size())
        return Failure<EndpointAccess>("contract/leaf-interface", "External Buffer view range is invalid");

    bool sawRead = false;
    bool sawWrite = false;
    for (std::uint32_t index = resource.firstView; index < end; ++index)
    {
        const auto& resourceView = view.Views()[index];
        if (resourceView.resource != slot.resource)
            return Failure<EndpointAccess>("contract/leaf-interface", "External Buffer view ownership is inconsistent");
        if (resourceView.viewClass == pkg::ViewClass::ShaderResource) sawRead = true;
        else if (resourceView.viewClass == pkg::ViewClass::UnorderedAccess) sawWrite = true;
        else
            return Failure<EndpointAccess>(
                "contract/leaf-interface",
                "Level 4 v1 endpoints are limited to SRV ReadOnly or UAV WriteOnly Buffer views");
    }
    if (sawRead == sawWrite)
        return Failure<EndpointAccess>(
            "contract/leaf-interface",
            "one Composition endpoint cannot mix read and write views");
    if (sawRead)
    {
        if (slot.requiredIncomingState != slot.guaranteedOutgoingState)
            return Failure<EndpointAccess>(
                "contract/leaf-interface",
                "ReadOnly endpoint must preserve its Resource state");
        return base::Result<EndpointAccess, ContractError>::Success(EndpointAccess::ReadOnly);
    }
    return base::Result<EndpointAccess, ContractError>::Success(EndpointAccess::WriteOnly);
}

base::Result<DecodedLeaf, ContractError> DecodeLeafWithDeclarations(
    LeafPackageDeclaration declaration)
{
    if (!ValidAuthorKey(declaration.stableKey))
        return Failure<DecodedLeaf>(
            "contract/leaf-key",
            "Leaf stable keys must use 1-127 characters from [A-Za-z0-9._/-]");
    if (declaration.packageBytes.empty())
        return Failure<DecodedLeaf>("contract/leaf-read", "Leaf Package bytes are empty");

    auto frozen = package::PackageReader::Read(declaration.packageBytes);
    if (!frozen)
        return Failure<DecodedLeaf>("contract/leaf-read", frozen.Error().message);
    if (frozen.Value().Target() != package::TargetKindD3D12 ||
        frozen.Value().Header().targetSchemaVersion != 17 ||
        frozen.Value().Header().minimumRuntimeVersion != 17)
        return Failure<DecodedLeaf>(
            "contract/leaf-schema",
            "Canonical R2 accepts only D3D12 Schema 17 / Runtime 17 Leaf Packages");
    auto view = pkg::D3D12PackageView::Decode(frozen.Value());
    if (!view)
        return Failure<DecodedLeaf>("contract/leaf-interface", view.Error().message);
    if (view.Value().SurfaceSlots().size() > 1)
        return Failure<DecodedLeaf>("contract/presenter", "one Leaf cannot expose multiple Surface slots");
    if (declaration.endpoints.size() != view.Value().ExternalSlots().size())
        return Failure<DecodedLeaf>(
            "contract/leaf-interface",
            "every Schema 17 External Buffer slot requires exactly one stable endpoint key");

    std::map<std::uint32_t, std::string> keysBySlot;
    std::set<std::string> authoredKeys;
    for (auto& endpoint : declaration.endpoints)
    {
        if (endpoint.externalSlot >= view.Value().ExternalSlots().size() ||
            !ValidAuthorKey(endpoint.stableKey) ||
            !keysBySlot.emplace(endpoint.externalSlot, endpoint.stableKey).second ||
            !authoredKeys.insert(endpoint.stableKey).second)
            return Failure<DecodedLeaf>(
                "contract/endpoint-key",
                "endpoint declarations require unique valid slots and stable keys");
    }

    DecodedLeaf result;
    result.leaf.stableKey = ComputeStableLeafKey(declaration.stableKey);
    result.leaf.targetKind = frozen.Value().Target();
    result.leaf.schemaVersion = frozen.Value().Header().targetSchemaVersion;
    result.leaf.minimumRuntimeVersion = frozen.Value().Header().minimumRuntimeVersion;
    result.leaf.executionDigest = frozen.Value().ExecutionDigest();
    result.leaf.fileDigest = package::ComputeFileDigest(frozen.Value().FileBytes());
    result.leaf.targetProfileDigest = frozen.Value().Header().targetProfileDigest;
    result.leaf.surfaceSlotCount = static_cast<std::uint32_t>(view.Value().SurfaceSlots().size());
    result.canonical.stableKey = result.leaf.stableKey;
    result.canonical.packageBytes = std::move(declaration.packageBytes);

    std::set<StableKey> endpointKeys;
    for (std::uint32_t slotIndex = 0; slotIndex < view.Value().ExternalSlots().size(); ++slotIndex)
    {
        const auto key = keysBySlot.find(slotIndex);
        if (key == keysBySlot.end())
            return Failure<DecodedLeaf>("contract/endpoint-key", "an External slot is missing its stable key");
        const auto& slot = view.Value().ExternalSlots()[slotIndex];
        auto access = DeriveAccess(view.Value(), slot);
        if (!access) return base::Result<DecodedLeaf, ContractError>::Failure(access.Error());
        const auto stableKey = ComputeStableEndpointKey(key->second);
        if (!endpointKeys.insert(stableKey).second)
            return Failure<DecodedLeaf>("contract/endpoint-key", "stable endpoint key digests collided");

        DerivedEndpoint endpoint;
        endpoint.localExternalSlot = slotIndex;
        endpoint.stableKey = stableKey;
        endpoint.resource = slot.resource;
        endpoint.kind = slot.requiredKind;
        endpoint.access = access.Value();
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
                "Level 4 v1 supports only required unformatted Buffer endpoints with completion tokens");
        result.endpoints.push_back(endpoint);
    }
    std::sort(result.endpoints.begin(), result.endpoints.end(), [](const auto& left, const auto& right) {
        return left.stableKey < right.stableKey;
    });
    return base::Result<DecodedLeaf, ContractError>::Success(std::move(result));
}

base::Result<std::pair<LeafPackageContract, std::vector<DerivedEndpoint>>, ContractError>
DecodeLeafForValidation(const CanonicalLeafPackage& leaf)
{
    if (ZeroKey(leaf.stableKey) || leaf.packageBytes.empty())
        return Failure<std::pair<LeafPackageContract, std::vector<DerivedEndpoint>>>(
            "contract/validate-leaf", "canonical Leaf identity or bytes are invalid");
    auto frozen = package::PackageReader::Read(leaf.packageBytes);
    if (!frozen)
        return Failure<std::pair<LeafPackageContract, std::vector<DerivedEndpoint>>>(
            "contract/validate-leaf", frozen.Error().message);
    if (frozen.Value().Target() != package::TargetKindD3D12 ||
        frozen.Value().Header().targetSchemaVersion != 17 ||
        frozen.Value().Header().minimumRuntimeVersion != 17)
        return Failure<std::pair<LeafPackageContract, std::vector<DerivedEndpoint>>>(
            "contract/validate-leaf", "embedded Leaf is not Schema 17 / Runtime 17");
    auto view = pkg::D3D12PackageView::Decode(frozen.Value());
    if (!view)
        return Failure<std::pair<LeafPackageContract, std::vector<DerivedEndpoint>>>(
            "contract/validate-leaf", view.Error().message);
    if (view.Value().SurfaceSlots().size() > 1)
        return Failure<std::pair<LeafPackageContract, std::vector<DerivedEndpoint>>>(
            "contract/validate-presenter", "one Leaf exposes multiple Surface slots");

    LeafPackageContract record;
    record.stableKey = leaf.stableKey;
    record.targetKind = frozen.Value().Target();
    record.schemaVersion = frozen.Value().Header().targetSchemaVersion;
    record.minimumRuntimeVersion = frozen.Value().Header().minimumRuntimeVersion;
    record.executionDigest = frozen.Value().ExecutionDigest();
    record.fileDigest = package::ComputeFileDigest(frozen.Value().FileBytes());
    record.targetProfileDigest = frozen.Value().Header().targetProfileDigest;
    record.surfaceSlotCount = static_cast<std::uint32_t>(view.Value().SurfaceSlots().size());

    std::vector<DerivedEndpoint> endpoints;
    for (std::uint32_t slotIndex = 0; slotIndex < view.Value().ExternalSlots().size(); ++slotIndex)
    {
        const auto& slot = view.Value().ExternalSlots()[slotIndex];
        auto access = DeriveAccess(view.Value(), slot);
        if (!access)
            return base::Result<std::pair<LeafPackageContract, std::vector<DerivedEndpoint>>, ContractError>::Failure(
                access.Error());
        DerivedEndpoint endpoint;
        endpoint.localExternalSlot = slotIndex;
        endpoint.resource = slot.resource;
        endpoint.kind = slot.requiredKind;
        endpoint.access = access.Value();
        endpoint.format = slot.requiredFormat;
        endpoint.minimumBytes = slot.minimumBytes;
        endpoint.requiredIncomingState = slot.requiredIncomingState;
        endpoint.guaranteedOutgoingState = slot.guaranteedOutgoingState;
        endpoint.synchronization = slot.synchronizationContract;
        endpoint.flags = slot.flags;
        endpoints.push_back(endpoint);
    }
    return base::Result<std::pair<LeafPackageContract, std::vector<DerivedEndpoint>>, ContractError>::Success(
        {std::move(record), std::move(endpoints)});
}

bool EndpointShapeMatches(
    const CompositionEndpointContract& left,
    const CompositionEndpointContract& right) noexcept
{
    return left.kind == right.kind && left.format == right.format;
}

using EndpointLookupKey = std::pair<StableKey, StableKey>;

base::Result<CompositionEndpointId, ContractError> ResolveEndpoint(
    const EndpointReferenceDeclaration& reference,
    const std::map<EndpointLookupKey, CompositionEndpointId>& endpointIds)
{
    if (!ValidAuthorKey(reference.leafKey) || !ValidAuthorKey(reference.endpointKey))
        return Failure<CompositionEndpointId>(
            "contract/reference", "Endpoint references require valid Leaf and Endpoint stable keys");
    const EndpointLookupKey key{
        ComputeStableLeafKey(reference.leafKey),
        ComputeStableEndpointKey(reference.endpointKey)};
    const auto found = endpointIds.find(key);
    if (found == endpointIds.end())
        return Failure<CompositionEndpointId>(
            "contract/reference", "Endpoint reference does not exist in the validated Leaf interfaces");
    return base::Result<CompositionEndpointId, ContractError>::Success(found->second);
}

std::vector<std::byte> SerializeBody(const PackageCompositionContract& contract)
{
    base::BinaryWriter writer;
    writer.WriteU32(ContractMagic);
    writer.WriteU32(CompositionContractVersion);
    writer.WriteU32(static_cast<std::uint32_t>(contract.leaves.size()));
    writer.WriteU32(static_cast<std::uint32_t>(contract.endpoints.size()));
    writer.WriteU32(static_cast<std::uint32_t>(contract.resources.size()));
    writer.WriteU32(static_cast<std::uint32_t>(contract.bindings.size()));
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
        writer.WriteU32(static_cast<std::uint32_t>(resource.consumers.size()));
        for (const auto consumer : resource.consumers) writer.WriteU32(consumer.value);
    }
    for (const auto& binding : contract.bindings)
    {
        writer.WriteU32(binding.endpoint.value);
        writer.WriteU32(binding.resource.value);
    }
    return std::move(writer).Take();
}

base::Result<void, ContractError> ValidateShapeInternal(
    const PackageCompositionContract& contract)
{
    if (contract.leaves.size() < 2)
        return base::Result<void, ContractError>::Failure(
            Error("contract/validate-leaves", "Level 4 v1 requires at least two Leaves"));
    if (contract.leaves.size() > MaximumRecords || contract.endpoints.size() > MaximumRecords ||
        contract.resources.size() > MaximumRecords || contract.bindings.size() > MaximumRecords)
        return base::Result<void, ContractError>::Failure(
            Error("contract/validate-count", "Contract record count exceeds supported limit"));

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
            return base::Result<void, ContractError>::Failure(
                Error("contract/validate-leaf", "Leaf metadata or canonical order is invalid"));
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
                return base::Result<void, ContractError>::Failure(
                    Error("contract/validate-endpoint", "Endpoint authority or canonical order is invalid"));
            previousEndpointKey = endpoint.stableKey;
            havePrevious = true;
        }
        endpointCursor += leaf.endpointCount;
    }
    if (endpointCursor != contract.endpoints.size() || presenterCount > 1 ||
        (presenterCount == 0 && contract.presenterLeaf.IsValid()) ||
        (presenterCount == 1 && contract.presenterLeaf != derivedPresenter))
        return base::Result<void, ContractError>::Failure(
            Error("contract/validate-presenter", "Presenter authority is inconsistent"));

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
            return base::Result<void, ContractError>::Failure(
                Error("contract/validate-resource", "Resource Flow identity or boundary is invalid"));
        const bool requiresProducer = resource.boundary != ResourceBoundary::CompositionInput;
        const bool requiresConsumers = resource.boundary != ResourceBoundary::CompositionOutput;
        if ((requiresProducer && !resource.producer.IsValid()) ||
            (!requiresProducer && resource.producer.IsValid()) ||
            (requiresConsumers && resource.consumers.empty()) ||
            (!requiresConsumers && !resource.consumers.empty()) || resource.sizeBytes == 0)
            return base::Result<void, ContractError>::Failure(
                Error("contract/validate-resource", "Resource Flow boundary shape is invalid"));

        std::vector<CompositionEndpointId> members;
        if (resource.producer.IsValid())
        {
            if (resource.producer.value >= contract.endpoints.size() ||
                contract.endpoints[resource.producer.value].access != EndpointAccess::WriteOnly)
                return base::Result<void, ContractError>::Failure(
                    Error("contract/validate-access", "Resource producer is not WriteOnly"));
            members.push_back(resource.producer);
        }
        if (!std::is_sorted(resource.consumers.begin(), resource.consumers.end(),
                [](auto left, auto right) { return left.value < right.value; }) ||
            std::adjacent_find(resource.consumers.begin(), resource.consumers.end()) != resource.consumers.end())
            return base::Result<void, ContractError>::Failure(
                Error("contract/validate-resource", "Resource consumers are not canonical"));
        for (const auto consumer : resource.consumers)
        {
            if (consumer.value >= contract.endpoints.size() ||
                contract.endpoints[consumer.value].access != EndpointAccess::ReadOnly)
                return base::Result<void, ContractError>::Failure(
                    Error("contract/validate-access", "Resource consumer is not ReadOnly"));
            members.push_back(consumer);
        }
        const auto& first = contract.endpoints[members.front().value];
        std::uint64_t requiredBytes = 0;
        for (const auto endpointId : members)
        {
            if (bound[endpointId.value])
                return base::Result<void, ContractError>::Failure(
                    Error("contract/validate-binding", "Endpoint is bound more than once"));
            const auto& endpoint = contract.endpoints[endpointId.value];
            if (!EndpointShapeMatches(first, endpoint))
                return base::Result<void, ContractError>::Failure(
                    Error("contract/validate-shape", "Resource Flow contains incompatible endpoints"));
            requiredBytes = std::max(requiredBytes, endpoint.minimumBytes);
            bound[endpointId.value] = true;
        }
        if (resource.kind != first.kind || resource.format != first.format ||
            resource.sizeBytes != requiredBytes)
            return base::Result<void, ContractError>::Failure(
                Error("contract/validate-shape", "Resource shape or size was not derived from endpoints"));
    }
    if (std::any_of(bound.begin(), bound.end(), [](bool value) { return !value; }))
        return base::Result<void, ContractError>::Failure(
            Error("contract/validate-binding", "a required Endpoint is unbound"));
    if (contract.bindings.size() != contract.endpoints.size())
        return base::Result<void, ContractError>::Failure(
            Error("contract/validate-binding", "binding cardinality differs from Endpoint cardinality"));
    for (std::uint32_t index = 0; index < contract.bindings.size(); ++index)
    {
        const auto& binding = contract.bindings[index];
        if (binding.endpoint.value != index || binding.resource.value >= contract.resources.size())
            return base::Result<void, ContractError>::Failure(
                Error("contract/validate-binding", "bindings are not dense and Endpoint-ordered"));
        const auto& resource = contract.resources[binding.resource.value];
        const bool producer = resource.producer == binding.endpoint;
        const bool consumer = std::binary_search(
            resource.consumers.begin(), resource.consumers.end(), binding.endpoint,
            [](auto left, auto right) { return left.value < right.value; });
        if (producer == consumer)
            return base::Result<void, ContractError>::Failure(
                Error("contract/validate-binding", "binding disagrees with Resource Flow membership"));
    }
    if (contract.identity != ComputeCompositionContractIdentity(contract))
        return base::Result<void, ContractError>::Failure(
            Error("contract/validate-identity", "Composition Contract identity is invalid"));
    return base::Result<void, ContractError>::Success();
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

base::Result<ValidatedCompositionContract, ContractError>
BuildCompositionContract(ContractBuildInput input)
{
    if (input.leaves.size() < 2)
        return Failure<ValidatedCompositionContract>(
            "contract/leaves", "Level 4 v1 composition requires at least two Leaves");
    if (input.resources.empty())
        return Failure<ValidatedCompositionContract>(
            "contract/resources", "Composition Contract requires at least one Resource Flow");

    std::vector<DecodedLeaf> decodedLeaves;
    decodedLeaves.reserve(input.leaves.size());
    for (auto& declaration : input.leaves)
    {
        auto decoded = DecodeLeafWithDeclarations(std::move(declaration));
        if (!decoded)
            return base::Result<ValidatedCompositionContract, ContractError>::Failure(decoded.Error());
        decodedLeaves.push_back(std::move(decoded).Value());
    }
    std::sort(decodedLeaves.begin(), decodedLeaves.end(), [](const auto& left, const auto& right) {
        return left.leaf.stableKey < right.leaf.stableKey;
    });
    if (std::adjacent_find(decodedLeaves.begin(), decodedLeaves.end(), [](const auto& left, const auto& right) {
            return left.leaf.stableKey == right.leaf.stableKey;
        }) != decodedLeaves.end())
        return Failure<ValidatedCompositionContract>("contract/leaves", "duplicate Leaf stable key");

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
                    "contract/endpoints", "duplicate endpoint identity inside one Leaf");
            contract.endpoints.push_back(endpoint);
        }
    }
    if (surfaceCount > 1)
        return Failure<ValidatedCompositionContract>(
            "contract/presenter", "Level 4 v1 allows at most one presenting Leaf");

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
                "contract/resource-key", "Resource Flow stable key is invalid");
        const auto key = ComputeStableResourceKey(declaration.stableKey);
        if (!resourceKeys.insert(key).second)
            return Failure<ValidatedCompositionContract>(
                "contract/resource-key", "duplicate Resource Flow stable key");
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
                "contract/resource-shape", "Resource Flow declaration contradicts its boundary");

        std::vector<CompositionEndpointId> members;
        if (item.declaration.producer)
        {
            auto resolved = ResolveEndpoint(*item.declaration.producer, endpointIds);
            if (!resolved)
                return base::Result<ValidatedCompositionContract, ContractError>::Failure(resolved.Error());
            if (contract.endpoints[resolved.Value().value].access != EndpointAccess::WriteOnly)
                return Failure<ValidatedCompositionContract>(
                    "contract/access", "Resource Flow producer must be a Leaf-derived WriteOnly endpoint");
            resource.producer = resolved.Value();
            members.push_back(resolved.Value());
        }
        for (const auto& reference : item.declaration.consumers)
        {
            auto resolved = ResolveEndpoint(reference, endpointIds);
            if (!resolved)
                return base::Result<ValidatedCompositionContract, ContractError>::Failure(resolved.Error());
            if (contract.endpoints[resolved.Value().value].access != EndpointAccess::ReadOnly)
                return Failure<ValidatedCompositionContract>(
                    "contract/access", "Resource Flow consumer must be a Leaf-derived ReadOnly endpoint");
            resource.consumers.push_back(resolved.Value());
            members.push_back(resolved.Value());
        }
        std::sort(resource.consumers.begin(), resource.consumers.end(), [](auto left, auto right) {
            return left.value < right.value;
        });
        if (std::adjacent_find(resource.consumers.begin(), resource.consumers.end()) != resource.consumers.end())
            return Failure<ValidatedCompositionContract>(
                "contract/resource-shape", "Resource Flow contains a duplicate consumer");
        const auto& first = contract.endpoints[members.front().value];
        resource.kind = first.kind;
        resource.format = first.format;
        for (const auto endpointId : members)
        {
            if (bound[endpointId.value])
                return Failure<ValidatedCompositionContract>(
                    "contract/binding", "one Endpoint cannot belong to two Resource Flows");
            const auto& endpoint = contract.endpoints[endpointId.value];
            if (!EndpointShapeMatches(first, endpoint))
                return Failure<ValidatedCompositionContract>(
                    "contract/shape", "Resource Flow endpoint shapes are incompatible");
            resource.sizeBytes = std::max(resource.sizeBytes, endpoint.minimumBytes);
            bound[endpointId.value] = true;
        }
        contract.resources.push_back(std::move(resource));
    }
    if (std::any_of(bound.begin(), bound.end(), [](bool value) { return !value; }))
        return Failure<ValidatedCompositionContract>(
            "contract/binding", "every required Leaf endpoint must be connected exactly once");

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
        return base::Result<ValidatedCompositionContract, ContractError>::Failure(validation.Error());
    return base::Result<ValidatedCompositionContract, ContractError>::Success(
        ValidatedCompositionContract(
            std::move(contract), std::move(canonicalLeaves),
            ValidatedCompositionContract::ConstructionToken{}));
}

base::Result<ValidatedCompositionContract, ContractError>
ValidateCompositionContractAgainstLeaves(
    PackageCompositionContract contract,
    std::vector<CanonicalLeafPackage> leaves)
{
    auto shape = ValidateShapeInternal(contract);
    if (!shape)
        return base::Result<ValidatedCompositionContract, ContractError>::Failure(shape.Error());
    std::sort(leaves.begin(), leaves.end(), [](const auto& left, const auto& right) {
        return left.stableKey < right.stableKey;
    });
    if (leaves.size() != contract.leaves.size())
        return Failure<ValidatedCompositionContract>(
            "contract/validate-leaf", "embedded Leaf cardinality differs from Contract");

    std::uint32_t surfaceCount = 0;
    LeafPackageId presenter;
    for (std::uint32_t leafIndex = 0; leafIndex < leaves.size(); ++leafIndex)
    {
        if (leaves[leafIndex].stableKey != contract.leaves[leafIndex].stableKey)
            return Failure<ValidatedCompositionContract>(
                "contract/validate-leaf", "embedded Leaf stable-key order differs from Contract");
        auto decoded = DecodeLeafForValidation(leaves[leafIndex]);
        if (!decoded)
            return base::Result<ValidatedCompositionContract, ContractError>::Failure(decoded.Error());
        auto sourceLeaf = std::move(decoded).Value();
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
                "contract/validate-leaf", "Contract Leaf metadata disagrees with Package bytes");
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
                    "contract/validate-endpoint", "Contract endpoint slot is invalid or duplicated");
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
                    "Contract endpoint authority was forged outside the embedded Leaf Package");
        }
        if (std::any_of(slots.begin(), slots.end(), [](bool value) { return !value; }))
            return Failure<ValidatedCompositionContract>(
                "contract/validate-endpoint", "an embedded Leaf External slot is absent from Contract");
    }
    if (surfaceCount > 1 ||
        (surfaceCount == 0 && contract.presenterLeaf.IsValid()) ||
        (surfaceCount == 1 && contract.presenterLeaf != presenter))
        return Failure<ValidatedCompositionContract>(
            "contract/validate-presenter", "Presenter was not derived from embedded Leaf Surface slots");

    return base::Result<ValidatedCompositionContract, ContractError>::Success(
        ValidatedCompositionContract(
            std::move(contract), std::move(leaves),
            ValidatedCompositionContract::ConstructionToken{}));
}

base::Result<void, ContractError>
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

base::Result<PackageCompositionContract, ContractError>
DeserializeCompositionContract(std::span<const std::byte> bytes)
{
    if (bytes.size() < 32 + base::Digest256{}.size())
        return Failure<PackageCompositionContract>("contract/read", "Contract bytes are too small");
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
        !bindingCount || !presenter || !reserved || magic.Value() != ContractMagic ||
        version.Value() != CompositionContractVersion || reserved.Value() != 0 ||
        leafCount.Value() > MaximumRecords || endpointCount.Value() > MaximumRecords ||
        resourceCount.Value() > MaximumRecords || bindingCount.Value() > MaximumRecords)
        return Failure<PackageCompositionContract>("contract/read", "Contract header is invalid");

    PackageCompositionContract contract;
    contract.presenterLeaf = {presenter.Value()};
    for (std::uint32_t index = 0; index < leafCount.Value(); ++index)
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
            !profile || !begin || !count || !surfaces || !tail || tail.Value() != 0)
            return Failure<PackageCompositionContract>("contract/read", "Leaf record is truncated or reserved bits are set");
        leaf.id = {id.Value()};
        leaf.stableKey = stableKey.Value();
        leaf.targetKind = target.Value();
        leaf.schemaVersion = schema.Value();
        leaf.minimumRuntimeVersion = runtime.Value();
        leaf.executionDigest = execution.Value();
        leaf.fileDigest = file.Value();
        leaf.targetProfileDigest = profile.Value();
        leaf.endpointBegin = begin.Value();
        leaf.endpointCount = count.Value();
        leaf.surfaceSlotCount = surfaces.Value();
        contract.leaves.push_back(leaf);
    }
    for (std::uint32_t index = 0; index < endpointCount.Value(); ++index)
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
            return Failure<PackageCompositionContract>("contract/read", "Endpoint record is truncated");
        endpoint.id = {id.Value()};
        endpoint.leaf = {leaf.Value()};
        endpoint.localExternalSlot = slot.Value();
        endpoint.resource = {resource.Value()};
        endpoint.stableKey = stableKey.Value();
        endpoint.kind = static_cast<pkg::ResourceKind>(kind.Value());
        endpoint.access = static_cast<EndpointAccess>(access.Value());
        endpoint.format = static_cast<pkg::Format>(format.Value());
        endpoint.minimumBytes = size.Value();
        endpoint.requiredIncomingState = incoming.Value();
        endpoint.guaranteedOutgoingState = outgoing.Value();
        endpoint.synchronization = static_cast<pkg::ExternalSynchronizationContract>(synchronization.Value());
        endpoint.flags = flags.Value();
        contract.endpoints.push_back(endpoint);
    }
    for (std::uint32_t index = 0; index < resourceCount.Value(); ++index)
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
            !consumers || consumers.Value() > MaximumRecords)
            return Failure<PackageCompositionContract>("contract/read", "Resource Flow record is invalid");
        resource.id = {id.Value()};
        resource.stableKey = stableKey.Value();
        resource.boundary = static_cast<ResourceBoundary>(boundary.Value());
        resource.kind = static_cast<pkg::ResourceKind>(kind.Value());
        resource.format = static_cast<pkg::Format>(format.Value());
        resource.sizeBytes = size.Value();
        resource.producer = {producer.Value()};
        for (std::uint32_t consumerIndex = 0; consumerIndex < consumers.Value(); ++consumerIndex)
        {
            auto consumer = reader.ReadU32();
            if (!consumer)
                return Failure<PackageCompositionContract>("contract/read", "Resource consumer list is truncated");
            resource.consumers.push_back({consumer.Value()});
        }
        contract.resources.push_back(std::move(resource));
    }
    for (std::uint32_t index = 0; index < bindingCount.Value(); ++index)
    {
        auto endpoint = reader.ReadU32();
        auto resource = reader.ReadU32();
        if (!endpoint || !resource)
            return Failure<PackageCompositionContract>("contract/read", "Binding record is truncated");
        contract.bindings.push_back({{endpoint.Value()}, {resource.Value()}});
    }
    auto identity = ReadDigest(reader);
    if (!identity || reader.Remaining() != 0)
        return Failure<PackageCompositionContract>("contract/read", "Contract identity is missing or trailing bytes exist");
    contract.identity = identity.Value();
    auto validation = ValidateShapeInternal(contract);
    if (!validation)
        return base::Result<PackageCompositionContract, ContractError>::Failure(validation.Error());
    return base::Result<PackageCompositionContract, ContractError>::Success(std::move(contract));
}
}
