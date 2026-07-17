#include "Schema18CompositionContract.h"

#include "../00_Foundation/BinaryIO.h"
#include "../09_FrozenPackageCore/PackageDigest.h"
#include "../09_FrozenPackageCore/PackageReader.h"
#include "../10_D3D12PackageSchema/D3D12Encoding.h"
#include "../10A_D3D12CompositionSchema18/D3D12CompositionEncoding18.h"

#include <algorithm>
#include <map>
#include <set>
#include <string_view>
#include <tuple>
#include <utility>

namespace sge4::composition::schema18
{
namespace
{
namespace pkg18 = package::d3d12_v18;
namespace pkg17 = package::d3d12_v13;

constexpr std::uint32_t ContractMagic = 0x3343'4753u; // "SGC3" little-endian
constexpr std::uint32_t ContractVersion = 1;
constexpr std::string_view LeafKeyDomain = "SGE4-L4V1-F3-Leaf-Key-v1";
constexpr std::string_view ResourceKeyDomain = "SGE4-L4V1-F3-Resource-Key-v1";

ContractError Error(std::string stage, std::string message)
{
    return {std::move(stage), std::move(message)};
}

template<class T>
base::Result<T, ContractError> Failure(std::string stage, std::string message)
{
    return base::Result<T, ContractError>::Failure(Error(std::move(stage), std::move(message)));
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

void WriteState(base::BinaryWriter& writer, const pkg18::ResourceState& value)
{
    writer.WriteU16(static_cast<std::uint16_t>(value.stateClass));
    writer.WriteU16(value.reserved);
    writer.WriteU32(value.explicitBits);
}

struct DecodedLeaf final
{
    LeafPackageContract leaf;
    std::vector<pkg18::LeafCompositionEndpointArtifact> endpoints;
};

base::Result<DecodedLeaf, ContractError> DecodeLeaf(
    std::string_view stableAuthorKey,
    std::span<const std::byte> packageBytes)
{
    if (!ValidAuthorKey(stableAuthorKey))
        return Failure<DecodedLeaf>("contract/leaf-key",
            "Leaf stable keys must use 1-127 characters from [A-Za-z0-9._/-]");
    if (packageBytes.empty())
        return Failure<DecodedLeaf>("contract/leaf-read", "Leaf Package bytes are empty");

    auto frozen = package::PackageReader::Read(packageBytes);
    if (!frozen)
        return Failure<DecodedLeaf>("contract/leaf-read", frozen.Error().message);
    if (frozen.Value().Target() != package::TargetKindD3D12 ||
        frozen.Value().Header().targetSchemaVersion != pkg18::TargetSchemaVersion ||
        frozen.Value().Header().minimumRuntimeVersion != pkg18::MinimumRuntimeVersion)
        return Failure<DecodedLeaf>("contract/leaf-schema",
            "F3 accepts only D3D12 Schema-18 / Runtime-18 Leaf Packages");

    auto compositionView = pkg18::CompositionLeafView::Decode(frozen.Value());
    if (!compositionView)
        return Failure<DecodedLeaf>("contract/leaf-interface", compositionView.Error().message);
    auto basePackage = package::PackageReader::Read(
        compositionView.Value().Schema17BasePackageBytes());
    if (!basePackage)
        return Failure<DecodedLeaf>("contract/leaf-base", basePackage.Error().message);
    auto baseView = pkg17::D3D12PackageView::Decode(basePackage.Value());
    if (!baseView)
        return Failure<DecodedLeaf>("contract/leaf-base", baseView.Error().message);

    DecodedLeaf decoded;
    decoded.leaf.stableKey = ComputeStableLeafKey(stableAuthorKey);
    decoded.leaf.targetKind = frozen.Value().Target();
    decoded.leaf.schemaVersion = frozen.Value().Header().targetSchemaVersion;
    decoded.leaf.minimumRuntimeVersion = frozen.Value().Header().minimumRuntimeVersion;
    decoded.leaf.executionDigest = frozen.Value().ExecutionDigest();
    decoded.leaf.fileDigest = package::ComputeFileDigest(frozen.Value().FileBytes());
    decoded.leaf.targetProfileDigest = frozen.Value().Header().targetProfileDigest;
    decoded.leaf.surfaceSlotCount =
        static_cast<std::uint32_t>(baseView.Value().SurfaceSlots().size());
    decoded.leaf.packageBytes.assign(packageBytes.begin(), packageBytes.end());
    decoded.endpoints.assign(compositionView.Value().Endpoints().begin(),
                             compositionView.Value().Endpoints().end());
    return base::Result<DecodedLeaf, ContractError>::Success(std::move(decoded));
}

bool EndpointShapeMatches(
    const CompositionEndpointContract& left,
    const CompositionEndpointContract& right) noexcept
{
    return left.kind == right.kind && left.format == right.format;
}

using EndpointLookupKey = std::pair<StableKey, pkg18::StableEndpointKey>;

base::Result<CompositionEndpointId, ContractError> ResolveEndpoint(
    const EndpointReferenceDeclaration& reference,
    const std::map<EndpointLookupKey, CompositionEndpointId>& endpointIds)
{
    if (!ValidAuthorKey(reference.leafKey) || !ValidAuthorKey(reference.endpointKey))
        return Failure<CompositionEndpointId>("contract/reference",
            "Endpoint references require valid Leaf and Endpoint stable keys");
    const EndpointLookupKey key{
        ComputeStableLeafKey(reference.leafKey),
        pkg18::ComputeStableEndpointKey(reference.endpointKey)};
    const auto found = endpointIds.find(key);
    if (found == endpointIds.end())
        return Failure<CompositionEndpointId>("contract/reference",
            "Endpoint reference does not exist in the authoritative Schema-18 Leaf interfaces");
    return base::Result<CompositionEndpointId, ContractError>::Success(found->second);
}

std::vector<std::byte> SerializeUnchecked(const PackageCompositionContract& contract)
{
    base::BinaryWriter writer;
    writer.WriteU32(ContractMagic);
    writer.WriteU32(ContractVersion);
    writer.WriteU32(static_cast<std::uint32_t>(contract.leaves.size()));
    writer.WriteU32(static_cast<std::uint32_t>(contract.endpoints.size()));
    writer.WriteU32(static_cast<std::uint32_t>(contract.resources.size()));
    writer.WriteU32(static_cast<std::uint32_t>(contract.bindings.size()));
    writer.WriteU32(contract.presenterLeaf.value);

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
        writer.WriteU64(static_cast<std::uint64_t>(leaf.packageBytes.size()));
        writer.WriteBytes(leaf.packageBytes);
    }

    for (const auto& endpoint : contract.endpoints)
    {
        writer.WriteU32(endpoint.id.value);
        writer.WriteU32(endpoint.leaf.value);
        writer.WriteU32(endpoint.localEndpoint);
        WriteDigest(writer, endpoint.stableKey);
        writer.WriteU32(endpoint.externalSlot.value);
        writer.WriteU32(endpoint.resource.value);
        writer.WriteU16(static_cast<std::uint16_t>(endpoint.kind));
        writer.WriteU16(static_cast<std::uint16_t>(endpoint.access));
        writer.WriteU32(static_cast<std::uint32_t>(endpoint.format));
        writer.WriteU64(endpoint.minimumBytes);
        WriteState(writer, endpoint.requiredIncomingState);
        WriteState(writer, endpoint.guaranteedOutgoingState);
        writer.WriteU32(static_cast<std::uint32_t>(endpoint.synchronization));
        writer.WriteU32(static_cast<std::uint32_t>(endpoint.frameSemantics));
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
}

StableKey ComputeStableLeafKey(std::string_view authorKey)
{
    return ComputeScopedKey(LeafKeyDomain, authorKey);
}

StableKey ComputeStableResourceKey(std::string_view authorKey)
{
    return ComputeScopedKey(ResourceKeyDomain, authorKey);
}

base::Result<PackageCompositionContract, ContractError>
BuildSchema18CompositionContract(ContractBuildInput input)
{
    if (input.leaves.size() < 2)
        return Failure<PackageCompositionContract>("contract/leaves",
            "Level 4 v1 composition requires at least two Leaf Packages");

    std::set<std::string> leafAuthorKeys;
    std::set<StableKey> leafStableKeys;
    std::vector<DecodedLeaf> decodedLeaves;
    decodedLeaves.reserve(input.leaves.size());
    for (const auto& declaration : input.leaves)
    {
        if (!leafAuthorKeys.insert(declaration.stableKey).second)
            return Failure<PackageCompositionContract>("contract/leaf-key",
                "Leaf stable keys must be unique before hashing");
        auto decoded = DecodeLeaf(declaration.stableKey, declaration.packageBytes);
        if (!decoded)
            return base::Result<PackageCompositionContract, ContractError>::Failure(decoded.Error());
        if (!leafStableKeys.insert(decoded.Value().leaf.stableKey).second)
            return Failure<PackageCompositionContract>("contract/leaf-key",
                "Leaf stable-key digests collided");
        decodedLeaves.push_back(std::move(decoded).Value());
    }

    std::sort(decodedLeaves.begin(), decodedLeaves.end(), [](const auto& left, const auto& right) {
        return left.leaf.stableKey < right.leaf.stableKey;
    });

    PackageCompositionContract result;
    std::uint32_t totalSurfaceSlots = 0;
    for (std::uint32_t index = 0; index < decodedLeaves.size(); ++index)
    {
        auto& decoded = decodedLeaves[index];
        decoded.leaf.id = {index};
        if (decoded.leaf.surfaceSlotCount > 1)
            return Failure<PackageCompositionContract>("contract/presenter",
                "one Leaf may expose at most one Surface slot in Level 4 v1");
        if (decoded.leaf.surfaceSlotCount == 1)
        {
            ++totalSurfaceSlots;
            result.presenterLeaf = {index};
        }
        result.leaves.push_back(std::move(decoded.leaf));
    }
    if (totalSurfaceSlots > 1)
        return Failure<PackageCompositionContract>("contract/presenter",
            "Level 4 v1 permits at most one presenter Leaf");

    struct EndpointSource final
    {
        LeafPackageId leaf;
        pkg18::LeafCompositionEndpointArtifact artifact;
    };
    std::vector<EndpointSource> endpointSources;
    for (std::uint32_t leafIndex = 0; leafIndex < decodedLeaves.size(); ++leafIndex)
    {
        for (const auto& artifact : decodedLeaves[leafIndex].endpoints)
            endpointSources.push_back({{leafIndex}, artifact});
    }
    std::sort(endpointSources.begin(), endpointSources.end(), [](const auto& left, const auto& right) {
        return std::tie(left.leaf.value, left.artifact.stableKey) <
               std::tie(right.leaf.value, right.artifact.stableKey);
    });

    std::map<EndpointLookupKey, CompositionEndpointId> endpointIds;
    for (std::uint32_t index = 0; index < endpointSources.size(); ++index)
    {
        const auto& source = endpointSources[index];
        const auto& artifact = source.artifact;
        CompositionEndpointContract endpoint;
        endpoint.id = {index};
        endpoint.leaf = source.leaf;
        endpoint.localEndpoint = artifact.id;
        endpoint.stableKey = artifact.stableKey;
        endpoint.externalSlot = artifact.externalSlot;
        endpoint.resource = artifact.resource;
        endpoint.kind = artifact.kind;
        endpoint.access = artifact.access;
        endpoint.format = artifact.format;
        endpoint.minimumBytes = artifact.minimumBytes;
        endpoint.requiredIncomingState = artifact.requiredIncomingState;
        endpoint.guaranteedOutgoingState = artifact.guaranteedOutgoingState;
        endpoint.synchronization = artifact.synchronization;
        endpoint.frameSemantics = artifact.frameSemantics;
        endpoint.flags = artifact.flags;
        const EndpointLookupKey lookup{
            result.leaves[source.leaf.value].stableKey, endpoint.stableKey};
        if (!endpointIds.emplace(lookup, endpoint.id).second)
            return Failure<PackageCompositionContract>("contract/endpoint",
                "one Leaf contains duplicate stable Endpoint keys");
        result.endpoints.push_back(endpoint);
    }

    std::uint32_t endpointCursor = 0;
    for (auto& leaf : result.leaves)
    {
        leaf.endpointBegin = endpointCursor;
        while (endpointCursor < result.endpoints.size() &&
               result.endpoints[endpointCursor].leaf == leaf.id)
            ++endpointCursor;
        leaf.endpointCount = endpointCursor - leaf.endpointBegin;
    }

    std::set<std::string> resourceAuthorKeys;
    std::set<StableKey> resourceStableKeys;
    for (const auto& declaration : input.resources)
    {
        if (!ValidAuthorKey(declaration.stableKey) ||
            !resourceAuthorKeys.insert(declaration.stableKey).second)
            return Failure<PackageCompositionContract>("contract/resource-key",
                "Resource Flow stable keys must be valid and unique before hashing");
        if (!resourceStableKeys.insert(ComputeStableResourceKey(declaration.stableKey)).second)
            return Failure<PackageCompositionContract>("contract/resource-key",
                "Resource Flow stable-key digests collided");
    }
    std::sort(input.resources.begin(), input.resources.end(), [](const auto& left, const auto& right) {
        return ComputeStableResourceKey(left.stableKey) < ComputeStableResourceKey(right.stableKey);
    });

    std::vector<bool> endpointBound(result.endpoints.size(), false);
    for (std::uint32_t resourceIndex = 0; resourceIndex < input.resources.size(); ++resourceIndex)
    {
        const auto& declaration = input.resources[resourceIndex];
        if (declaration.boundary != ResourceBoundary::Internal &&
            declaration.boundary != ResourceBoundary::CompositionInput &&
            declaration.boundary != ResourceBoundary::CompositionOutput)
            return Failure<PackageCompositionContract>("contract/resource",
                "Resource Flow boundary is invalid");

        const bool requiresProducer = declaration.boundary != ResourceBoundary::CompositionInput;
        const bool requiresConsumers = declaration.boundary != ResourceBoundary::CompositionOutput;
        if (requiresProducer != declaration.producer.has_value() ||
            (requiresConsumers && declaration.consumers.empty()) ||
            (!requiresConsumers && !declaration.consumers.empty()))
            return Failure<PackageCompositionContract>("contract/resource",
                "Resource Flow producer/consumer shape disagrees with its boundary");

        ResourceFlowContract resource;
        resource.id = {resourceIndex};
        resource.stableKey = ComputeStableResourceKey(declaration.stableKey);
        resource.boundary = declaration.boundary;

        std::vector<CompositionEndpointId> members;
        if (declaration.producer)
        {
            auto resolved = ResolveEndpoint(*declaration.producer, endpointIds);
            if (!resolved)
                return base::Result<PackageCompositionContract, ContractError>::Failure(resolved.Error());
            resource.producer = resolved.Value();
            const auto& endpoint = result.endpoints[resource.producer.value];
            if (endpoint.access != pkg18::EndpointAccess::WriteOnly)
                return Failure<PackageCompositionContract>("contract/access",
                    "Resource Flow producers must be authoritative WriteOnly endpoints");
            members.push_back(resource.producer);
        }

        for (const auto& consumerDeclaration : declaration.consumers)
        {
            auto resolved = ResolveEndpoint(consumerDeclaration, endpointIds);
            if (!resolved)
                return base::Result<PackageCompositionContract, ContractError>::Failure(resolved.Error());
            const auto endpointId = resolved.Value();
            const auto& endpoint = result.endpoints[endpointId.value];
            if (endpoint.access != pkg18::EndpointAccess::ReadOnly)
                return Failure<PackageCompositionContract>("contract/access",
                    "Resource Flow consumers must be authoritative ReadOnly endpoints");
            resource.consumers.push_back(endpointId);
            members.push_back(endpointId);
        }
        std::sort(resource.consumers.begin(), resource.consumers.end(), [](auto left, auto right) {
            return left.value < right.value;
        });
        if (std::adjacent_find(resource.consumers.begin(), resource.consumers.end()) !=
            resource.consumers.end())
            return Failure<PackageCompositionContract>("contract/binding",
                "one Resource Flow lists a consumer endpoint more than once");

        if (members.empty())
            return Failure<PackageCompositionContract>("contract/resource",
                "Resource Flow has no authoritative endpoints");
        const auto& shape = result.endpoints[members.front().value];
        resource.kind = shape.kind;
        resource.format = shape.format;
        for (const auto endpointId : members)
        {
            if (endpointId.value >= result.endpoints.size() || endpointBound[endpointId.value])
                return Failure<PackageCompositionContract>("contract/binding",
                    "each Schema-18 endpoint must belong to exactly one Resource Flow");
            const auto& endpoint = result.endpoints[endpointId.value];
            if (!EndpointShapeMatches(shape, endpoint))
                return Failure<PackageCompositionContract>("contract/shape",
                    "connected Schema-18 endpoints have incompatible Resource shapes");
            resource.sizeBytes = std::max(resource.sizeBytes, endpoint.minimumBytes);
            endpointBound[endpointId.value] = true;
            result.bindings.push_back({endpointId, resource.id});
        }
        result.resources.push_back(std::move(resource));
    }

    if (std::any_of(endpointBound.begin(), endpointBound.end(), [](bool bound) { return !bound; }))
        return Failure<PackageCompositionContract>("contract/binding",
            "every required Schema-18 endpoint must belong to exactly one Resource Flow");

    std::sort(result.bindings.begin(), result.bindings.end(), [](const auto& left, const auto& right) {
        return left.endpoint.value < right.endpoint.value;
    });
    result.identity = ComputeContractIdentity(result);
    auto validation = ValidateCompositionContract(result);
    if (!validation)
        return base::Result<PackageCompositionContract, ContractError>::Failure(validation.Error());
    return base::Result<PackageCompositionContract, ContractError>::Success(std::move(result));
}

base::Result<void, ContractError>
ValidateCompositionContract(const PackageCompositionContract& contract)
{
    if (contract.leaves.size() < 2)
        return base::Result<void, ContractError>::Failure(
            Error("contract/validate", "contract contains fewer than two Leaves"));

    std::set<StableKey> leafKeys;
    std::uint32_t surfaceCount = 0;
    LeafPackageId derivedPresenter;
    std::uint32_t endpointCursor = 0;
    for (std::uint32_t index = 0; index < contract.leaves.size(); ++index)
    {
        const auto& leaf = contract.leaves[index];
        if (leaf.id.value != index || ZeroKey(leaf.stableKey) ||
            !leafKeys.insert(leaf.stableKey).second ||
            (index > 0 && !(contract.leaves[index - 1].stableKey < leaf.stableKey)))
            return base::Result<void, ContractError>::Failure(
                Error("contract/validate-leaf", "Leaf IDs or stable-key order are non-canonical"));
        if (leaf.endpointBegin != endpointCursor ||
            static_cast<std::uint64_t>(leaf.endpointBegin) + leaf.endpointCount > contract.endpoints.size())
            return base::Result<void, ContractError>::Failure(
                Error("contract/validate-leaf", "Leaf endpoint range is non-canonical"));

        auto frozen = package::PackageReader::Read(leaf.packageBytes);
        if (!frozen)
            return base::Result<void, ContractError>::Failure(
                Error("contract/validate-leaf", frozen.Error().message));
        auto view = pkg18::CompositionLeafView::Decode(frozen.Value());
        if (!view)
            return base::Result<void, ContractError>::Failure(
                Error("contract/validate-leaf", view.Error().message));
        auto basePackage = package::PackageReader::Read(view.Value().Schema17BasePackageBytes());
        if (!basePackage)
            return base::Result<void, ContractError>::Failure(
                Error("contract/validate-leaf", basePackage.Error().message));
        auto baseView = pkg17::D3D12PackageView::Decode(basePackage.Value());
        if (!baseView)
            return base::Result<void, ContractError>::Failure(
                Error("contract/validate-leaf", baseView.Error().message));
        if (leaf.targetKind != frozen.Value().Target() ||
            leaf.schemaVersion != pkg18::TargetSchemaVersion ||
            leaf.minimumRuntimeVersion != pkg18::MinimumRuntimeVersion ||
            leaf.executionDigest != frozen.Value().ExecutionDigest() ||
            leaf.fileDigest != package::ComputeFileDigest(frozen.Value().FileBytes()) ||
            leaf.targetProfileDigest != frozen.Value().Header().targetProfileDigest ||
            leaf.endpointCount != view.Value().Endpoints().size() ||
            leaf.surfaceSlotCount != baseView.Value().SurfaceSlots().size())
            return base::Result<void, ContractError>::Failure(
                Error("contract/validate-leaf", "Leaf metadata disagrees with its Package bytes"));
        if (leaf.surfaceSlotCount > 1)
            return base::Result<void, ContractError>::Failure(
                Error("contract/validate-presenter", "one Leaf exposes multiple Surface slots"));
        if (leaf.surfaceSlotCount == 1)
        {
            ++surfaceCount;
            derivedPresenter = leaf.id;
        }

        std::map<pkg18::StableEndpointKey, const pkg18::LeafCompositionEndpointArtifact*> local;
        for (const auto& endpoint : view.Value().Endpoints()) local.emplace(endpoint.stableKey, &endpoint);
        pkg18::StableEndpointKey previousKey{};
        bool havePrevious = false;
        for (std::uint32_t offset = 0; offset < leaf.endpointCount; ++offset)
        {
            const auto& endpoint = contract.endpoints[leaf.endpointBegin + offset];
            if (endpoint.id.value != leaf.endpointBegin + offset || endpoint.leaf != leaf.id ||
                (havePrevious && !(previousKey < endpoint.stableKey)))
                return base::Result<void, ContractError>::Failure(
                    Error("contract/validate-endpoint", "Endpoint IDs, ownership, or order are non-canonical"));
            const auto found = local.find(endpoint.stableKey);
            if (found == local.end())
                return base::Result<void, ContractError>::Failure(
                    Error("contract/validate-endpoint", "Endpoint is not present in the Leaf interface"));
            const auto& source = *found->second;
            if (endpoint.localEndpoint != source.id || endpoint.externalSlot != source.externalSlot ||
                endpoint.resource != source.resource || endpoint.kind != source.kind ||
                endpoint.access != source.access || endpoint.format != source.format ||
                endpoint.minimumBytes != source.minimumBytes ||
                endpoint.requiredIncomingState != source.requiredIncomingState ||
                endpoint.guaranteedOutgoingState != source.guaranteedOutgoingState ||
                endpoint.synchronization != source.synchronization ||
                endpoint.frameSemantics != source.frameSemantics || endpoint.flags != source.flags)
                return base::Result<void, ContractError>::Failure(
                    Error("contract/validate-endpoint", "Endpoint contract was forged outside the Leaf Package"));
            previousKey = endpoint.stableKey;
            havePrevious = true;
        }
        endpointCursor += leaf.endpointCount;
    }
    if (endpointCursor != contract.endpoints.size() || surfaceCount > 1 ||
        (surfaceCount == 0 && contract.presenterLeaf.IsValid()) ||
        (surfaceCount == 1 && contract.presenterLeaf != derivedPresenter))
        return base::Result<void, ContractError>::Failure(
            Error("contract/validate-presenter", "presenter authority is inconsistent"));

    std::vector<bool> bound(contract.endpoints.size(), false);
    std::set<StableKey> resourceKeys;
    for (std::uint32_t index = 0; index < contract.resources.size(); ++index)
    {
        const auto& resource = contract.resources[index];
        if (resource.id.value != index || ZeroKey(resource.stableKey) ||
            !resourceKeys.insert(resource.stableKey).second ||
            (index > 0 && !(contract.resources[index - 1].stableKey < resource.stableKey)))
            return base::Result<void, ContractError>::Failure(
                Error("contract/validate-resource", "Resource Flow IDs or stable-key order are non-canonical"));
        if (resource.boundary != ResourceBoundary::Internal &&
            resource.boundary != ResourceBoundary::CompositionInput &&
            resource.boundary != ResourceBoundary::CompositionOutput)
            return base::Result<void, ContractError>::Failure(
                Error("contract/validate-resource", "Resource Flow boundary is invalid"));
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
                contract.endpoints[resource.producer.value].access != pkg18::EndpointAccess::WriteOnly)
                return base::Result<void, ContractError>::Failure(
                    Error("contract/validate-access", "Resource producer is not WriteOnly"));
            members.push_back(resource.producer);
        }
        if (!std::is_sorted(resource.consumers.begin(), resource.consumers.end(),
            [](auto left, auto right) { return left.value < right.value; }) ||
            std::adjacent_find(resource.consumers.begin(), resource.consumers.end()) != resource.consumers.end())
            return base::Result<void, ContractError>::Failure(
                Error("contract/validate-resource", "Resource consumers are non-canonical"));
        for (const auto consumer : resource.consumers)
        {
            if (consumer.value >= contract.endpoints.size() ||
                contract.endpoints[consumer.value].access != pkg18::EndpointAccess::ReadOnly)
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
            Error("contract/validate-binding", "required Endpoint is unbound"));

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
        const bool consumer = std::binary_search(resource.consumers.begin(), resource.consumers.end(),
            binding.endpoint, [](auto left, auto right) { return left.value < right.value; });
        if (producer == consumer)
            return base::Result<void, ContractError>::Failure(
                Error("contract/validate-binding", "binding disagrees with Resource Flow membership"));
    }

    if (contract.identity != ComputeContractIdentity(contract))
        return base::Result<void, ContractError>::Failure(
            Error("contract/validate-identity", "Composition Contract identity is invalid"));
    return base::Result<void, ContractError>::Success();
}

std::vector<std::byte>
SerializeCompositionContract(const PackageCompositionContract& contract)
{
    return SerializeUnchecked(contract);
}

base::Digest256
ComputeContractIdentity(const PackageCompositionContract& contract)
{
    const auto bytes = SerializeUnchecked(contract);
    return base::Sha256(bytes);
}
}
