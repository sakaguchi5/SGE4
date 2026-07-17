#include "Schema18CompositionLinker.h"

#include "../00_Foundation/BinaryIO.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace sge4::composition::schema18::linking
{
namespace
{
constexpr std::uint32_t ArtifactMagic = 0x3643'4753u; // "SGC6"
constexpr std::uint32_t ArtifactVersion = 1;
constexpr std::uint64_t ArtifactHeaderBytes = 128;
constexpr std::uint64_t ArtifactTrailerBytes = 32;
constexpr std::uint32_t ContractMagic = 0x3343'4753u; // "SGC3"
constexpr std::uint32_t ContractVersion = 1;
constexpr std::uint32_t MaximumRecords = 1'000'000;

LinkError Error(std::string stage, std::string message)
{
    return {std::move(stage), std::move(message)};
}

template<class T>
base::Result<T, LinkError> Failure(std::string stage, std::string message)
{
    return base::Result<T, LinkError>::Failure(
        Error(std::move(stage), std::move(message)));
}

void WriteDigest(base::BinaryWriter& writer, const base::Digest256& value)
{
    writer.WriteBytes(value);
}

base::Result<base::Digest256, LinkError> ReadDigest(base::BinaryReader& reader)
{
    auto bytes = reader.ReadBytes(base::Digest256{}.size());
    if (!bytes) return Failure<base::Digest256>("frozen/read", bytes.Error());
    base::Digest256 result{};
    std::copy(bytes.Value().begin(), bytes.Value().end(), result.begin());
    return base::Result<base::Digest256, LinkError>::Success(result);
}

base::Result<package::d3d12_v18::ResourceState, LinkError>
ReadState(base::BinaryReader& reader)
{
    auto stateClass = reader.ReadU16();
    auto reserved = reader.ReadU16();
    auto bits = reader.ReadU32();
    if (!stateClass || !reserved || !bits)
        return Failure<package::d3d12_v18::ResourceState>(
            "frozen/contract", "truncated ResourceState");
    package::d3d12_v18::ResourceState result;
    result.stateClass = static_cast<package::d3d12_v13::StateClass>(
        stateClass.Value());
    result.reserved = reserved.Value();
    result.explicitBits = bits.Value();
    return base::Result<package::d3d12_v18::ResourceState, LinkError>::Success(result);
}

base::Result<PackageCompositionContract, LinkError>
ReadContract(std::span<const std::byte> bytes)
{
    base::BinaryReader reader(bytes);
    auto magic = reader.ReadU32();
    auto version = reader.ReadU32();
    auto leafCount = reader.ReadU32();
    auto endpointCount = reader.ReadU32();
    auto resourceCount = reader.ReadU32();
    auto bindingCount = reader.ReadU32();
    auto presenter = reader.ReadU32();
    if (!magic || !version || !leafCount || !endpointCount ||
        !resourceCount || !bindingCount || !presenter ||
        magic.Value() != ContractMagic || version.Value() != ContractVersion)
        return Failure<PackageCompositionContract>(
            "frozen/contract", "Composition Contract header is invalid");
    if (leafCount.Value() > MaximumRecords || endpointCount.Value() > MaximumRecords ||
        resourceCount.Value() > MaximumRecords || bindingCount.Value() > MaximumRecords)
        return Failure<PackageCompositionContract>(
            "frozen/contract", "Composition Contract count exceeds the supported limit");

    PackageCompositionContract contract;
    contract.presenterLeaf = {presenter.Value()};
    for (std::uint32_t index = 0; index < leafCount.Value(); ++index)
    {
        auto id = reader.ReadU32();
        auto stableKey = ReadDigest(reader);
        auto target = reader.ReadU32();
        auto schema = reader.ReadU32();
        auto runtime = reader.ReadU32();
        auto execution = ReadDigest(reader);
        auto file = ReadDigest(reader);
        auto profile = ReadDigest(reader);
        auto endpointBegin = reader.ReadU32();
        auto endpointCountValue = reader.ReadU32();
        auto surfaceCount = reader.ReadU32();
        auto packageSize = reader.ReadU64();
        if (!id || !stableKey || !target || !schema || !runtime || !execution ||
            !file || !profile || !endpointBegin || !endpointCountValue ||
            !surfaceCount || !packageSize || packageSize.Value() > reader.Remaining())
            return Failure<PackageCompositionContract>(
                "frozen/contract", "Leaf record is truncated");
        auto packageBytes = reader.ReadBytes(static_cast<std::size_t>(packageSize.Value()));
        if (!packageBytes)
            return Failure<PackageCompositionContract>(
                "frozen/contract", packageBytes.Error());
        LeafPackageContract leaf;
        leaf.id = {id.Value()};
        leaf.stableKey = stableKey.Value();
        leaf.targetKind = target.Value();
        leaf.schemaVersion = schema.Value();
        leaf.minimumRuntimeVersion = runtime.Value();
        leaf.executionDigest = execution.Value();
        leaf.fileDigest = file.Value();
        leaf.targetProfileDigest = profile.Value();
        leaf.endpointBegin = endpointBegin.Value();
        leaf.endpointCount = endpointCountValue.Value();
        leaf.surfaceSlotCount = surfaceCount.Value();
        leaf.packageBytes.assign(packageBytes.Value().begin(), packageBytes.Value().end());
        contract.leaves.push_back(std::move(leaf));
    }

    for (std::uint32_t index = 0; index < endpointCount.Value(); ++index)
    {
        auto id = reader.ReadU32();
        auto leaf = reader.ReadU32();
        auto localEndpoint = reader.ReadU32();
        auto stableKey = ReadDigest(reader);
        auto slot = reader.ReadU32();
        auto resource = reader.ReadU32();
        auto kind = reader.ReadU16();
        auto access = reader.ReadU16();
        auto format = reader.ReadU32();
        auto minimumBytes = reader.ReadU64();
        auto incoming = ReadState(reader);
        auto outgoing = ReadState(reader);
        auto synchronization = reader.ReadU32();
        auto frameSemantics = reader.ReadU32();
        auto flags = reader.ReadU32();
        if (!id || !leaf || !localEndpoint || !stableKey || !slot || !resource ||
            !kind || !access || !format || !minimumBytes || !incoming || !outgoing ||
            !synchronization || !frameSemantics || !flags)
            return Failure<PackageCompositionContract>(
                "frozen/contract", "Endpoint record is truncated");
        CompositionEndpointContract endpoint;
        endpoint.id = {id.Value()};
        endpoint.leaf = {leaf.Value()};
        endpoint.localEndpoint = localEndpoint.Value();
        endpoint.stableKey = stableKey.Value();
        endpoint.externalSlot = {slot.Value()};
        endpoint.resource = {resource.Value()};
        endpoint.kind = static_cast<package::d3d12_v18::ResourceKind>(kind.Value());
        endpoint.access = static_cast<package::d3d12_v18::EndpointAccess>(access.Value());
        endpoint.format = static_cast<package::d3d12_v18::Format>(format.Value());
        endpoint.minimumBytes = minimumBytes.Value();
        endpoint.requiredIncomingState = incoming.Value();
        endpoint.guaranteedOutgoingState = outgoing.Value();
        endpoint.synchronization =
            static_cast<package::d3d12_v18::ExternalSynchronizationContract>(
                synchronization.Value());
        endpoint.frameSemantics =
            static_cast<package::d3d12_v18::EndpointFrameSemantics>(
                frameSemantics.Value());
        endpoint.flags = flags.Value();
        contract.endpoints.push_back(endpoint);
    }

    for (std::uint32_t index = 0; index < resourceCount.Value(); ++index)
    {
        auto id = reader.ReadU32();
        auto stableKey = ReadDigest(reader);
        auto boundary = reader.ReadU16();
        auto kind = reader.ReadU16();
        auto format = reader.ReadU32();
        auto size = reader.ReadU64();
        auto producer = reader.ReadU32();
        auto consumerCount = reader.ReadU32();
        if (!id || !stableKey || !boundary || !kind || !format || !size ||
            !producer || !consumerCount || consumerCount.Value() > MaximumRecords)
            return Failure<PackageCompositionContract>(
                "frozen/contract", "Resource Flow record is truncated");
        ResourceFlowContract resource;
        resource.id = {id.Value()};
        resource.stableKey = stableKey.Value();
        resource.boundary = static_cast<ResourceBoundary>(boundary.Value());
        resource.kind = static_cast<package::d3d12_v18::ResourceKind>(kind.Value());
        resource.format = static_cast<package::d3d12_v18::Format>(format.Value());
        resource.sizeBytes = size.Value();
        resource.producer = {producer.Value()};
        for (std::uint32_t consumerIndex = 0;
             consumerIndex < consumerCount.Value(); ++consumerIndex)
        {
            auto consumer = reader.ReadU32();
            if (!consumer)
                return Failure<PackageCompositionContract>(
                    "frozen/contract", "Resource consumer list is truncated");
            resource.consumers.push_back({consumer.Value()});
        }
        contract.resources.push_back(std::move(resource));
    }

    for (std::uint32_t index = 0; index < bindingCount.Value(); ++index)
    {
        auto endpoint = reader.ReadU32();
        auto resource = reader.ReadU32();
        if (!endpoint || !resource)
            return Failure<PackageCompositionContract>(
                "frozen/contract", "Binding record is truncated");
        contract.bindings.push_back({{endpoint.Value()}, {resource.Value()}});
    }
    if (reader.Remaining() != 0)
        return Failure<PackageCompositionContract>(
            "frozen/contract", "Composition Contract has trailing bytes");

    contract.identity = ComputeContractIdentity(contract);
    if (SerializeCompositionContract(contract) !=
        std::vector<std::byte>(bytes.begin(), bytes.end()))
        return Failure<PackageCompositionContract>(
            "frozen/contract", "Composition Contract encoding is non-canonical");
    auto validation = ValidateCompositionContract(contract);
    if (!validation)
        return Failure<PackageCompositionContract>(
            "frozen/contract", validation.Error().stage + ": " +
                               validation.Error().message);
    return base::Result<PackageCompositionContract, LinkError>::Success(
        std::move(contract));
}

std::vector<std::byte> BuildArtifactBytes(
    const PackageCompositionContract& contract,
    const planning::RawResourceFlowPlan& plan,
    const base::Digest256& seal)
{
    const auto contractBytes = SerializeCompositionContract(contract);
    const auto planBytes = planning::SerializeRawResourceFlowPlan(plan);
    const std::uint64_t totalBytes = ArtifactHeaderBytes +
        contractBytes.size() + planBytes.size() + ArtifactTrailerBytes;

    base::BinaryWriter writer;
    writer.WriteU32(ArtifactMagic);
    writer.WriteU32(ArtifactVersion);
    writer.WriteU64(totalBytes);
    writer.WriteU64(static_cast<std::uint64_t>(contractBytes.size()));
    writer.WriteU64(static_cast<std::uint64_t>(planBytes.size()));
    WriteDigest(writer, contract.identity);
    WriteDigest(writer, plan.identity);
    WriteDigest(writer, seal);
    writer.WriteBytes(contractBytes);
    writer.WriteBytes(planBytes);
    const auto digest = base::Sha256(writer.Bytes());
    writer.WriteBytes(digest);
    return std::move(writer).Take();
}
}

base::Result<FrozenCompositionArtifact, LinkError>
FreezeVerifiedComposition(
    const PackageCompositionContract& contract,
    const verification::VerifiedResourceFlowPlan& verifiedPlan)
{
    auto validation = verification::ValidateVerifiedPlan(contract, verifiedPlan);
    if (!validation)
        return Failure<FrozenCompositionArtifact>(
            "frozen/verified-plan", validation.Error().stage + ": " +
                                    validation.Error().message);

    auto bytes = BuildArtifactBytes(contract, verifiedPlan.Plan(), verifiedPlan.Seal());
    return ReadFrozenComposition(bytes);
}

base::Result<FrozenCompositionArtifact, LinkError>
ReadFrozenComposition(std::span<const std::byte> bytes)
{
    if (bytes.size() < ArtifactHeaderBytes + ArtifactTrailerBytes)
        return Failure<FrozenCompositionArtifact>(
            "frozen/read", "Frozen Composition bytes are too small");
    base::BinaryReader reader(bytes);
    auto magic = reader.ReadU32();
    auto version = reader.ReadU32();
    auto totalBytes = reader.ReadU64();
    auto contractSize = reader.ReadU64();
    auto planSize = reader.ReadU64();
    auto contractIdentity = ReadDigest(reader);
    auto planIdentity = ReadDigest(reader);
    auto verifierSeal = ReadDigest(reader);
    if (!magic || !version || !totalBytes || !contractSize || !planSize ||
        !contractIdentity || !planIdentity || !verifierSeal ||
        magic.Value() != ArtifactMagic || version.Value() != ArtifactVersion ||
        totalBytes.Value() != bytes.size() || reader.Position() != ArtifactHeaderBytes)
        return Failure<FrozenCompositionArtifact>(
            "frozen/read", "Frozen Composition header is invalid");

    if (contractSize.Value() >
        std::numeric_limits<std::uint64_t>::max() - planSize.Value())
        return Failure<FrozenCompositionArtifact>(
            "frozen/read", "Frozen Composition section lengths overflow");
    const auto bodyBytes = contractSize.Value() + planSize.Value();
    if (contractSize.Value() > std::numeric_limits<std::size_t>::max() ||
        planSize.Value() > std::numeric_limits<std::size_t>::max() ||
        bodyBytes > reader.Remaining() ||
        bodyBytes + ArtifactTrailerBytes != reader.Remaining())
        return Failure<FrozenCompositionArtifact>(
            "frozen/read", "Frozen Composition section lengths are invalid");

    auto contractBytes = reader.ReadBytes(static_cast<std::size_t>(contractSize.Value()));
    auto planBytes = reader.ReadBytes(static_cast<std::size_t>(planSize.Value()));
    auto artifactDigest = ReadDigest(reader);
    if (!contractBytes || !planBytes || !artifactDigest || reader.Remaining() != 0)
        return Failure<FrozenCompositionArtifact>(
            "frozen/read", "Frozen Composition body is truncated");

    const auto expectedArtifactDigest = base::Sha256(
        bytes.first(bytes.size() - ArtifactTrailerBytes));
    if (artifactDigest.Value() != expectedArtifactDigest)
        return Failure<FrozenCompositionArtifact>(
            "frozen/digest", "Frozen Composition artifact digest is invalid");

    auto contract = ReadContract(contractBytes.Value());
    if (!contract)
        return base::Result<FrozenCompositionArtifact, LinkError>::Failure(
            contract.Error());
    if (contract.Value().identity != contractIdentity.Value())
        return Failure<FrozenCompositionArtifact>(
            "frozen/contract-identity", "embedded Contract identity is invalid");

    auto plan = planning::DeserializeRawResourceFlowPlan(planBytes.Value());
    if (!plan)
        return Failure<FrozenCompositionArtifact>(
            "frozen/plan", plan.Error().stage + ": " + plan.Error().message);
    if (plan.Value().identity != planIdentity.Value() ||
        plan.Value().contractIdentity != contract.Value().identity)
        return Failure<FrozenCompositionArtifact>(
            "frozen/plan-identity", "embedded Plan identity is invalid");

    auto verified = verification::VerifyAndSeal(contract.Value(), plan.Value());
    if (!verified)
        return Failure<FrozenCompositionArtifact>(
            "frozen/authority", verified.Error().stage + ": " +
                                verified.Error().message);
    if (verified.Value().Seal() != verifierSeal.Value())
        return Failure<FrozenCompositionArtifact>(
            "frozen/seal", "embedded Verifier seal is invalid");

    FrozenCompositionArtifact result;
    result.contract = std::move(contract).Value();
    result.plan = std::move(plan).Value();
    result.verifierSeal = verifierSeal.Value();
    result.artifactDigest = artifactDigest.Value();
    result.fileBytes.assign(bytes.begin(), bytes.end());
    return base::Result<FrozenCompositionArtifact, LinkError>::Success(
        std::move(result));
}
}
