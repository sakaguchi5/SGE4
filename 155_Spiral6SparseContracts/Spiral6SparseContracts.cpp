#include "Spiral6SparseContracts.h"

#include "../00_Foundation/BinaryIO.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace sge4_5::spiral6::contract
{
namespace
{
constexpr std::uint32_t ArtifactMagic = 0x314C3653u; // "S6L1"
constexpr std::uint16_t ArtifactVersion = 1;
constexpr std::uint16_t ArtifactKind = 1;
constexpr std::uint32_t ExtensionStrategy = 1; // VersionedSidecarSparseExtensionV1
constexpr std::uint32_t OutputOrderPolicy = 1; // UniverseIndexOrder
constexpr std::size_t DigestBytes = 32;
constexpr std::size_t HeaderU32Count = 15;
constexpr std::size_t FixedIndexCount = semantic::WorkUniverseCountV1;
constexpr std::size_t PayloadBytes = 4 + 2 + 2 + HeaderU32Count * 4 + 6 * DigestBytes + FixedIndexCount * 4;
constexpr std::size_t CompleteBytes = PayloadBytes + DigestBytes;

SparseContractErrorV1 Error(std::string stage, std::string message)
{
    return {std::move(stage), std::move(message)};
}

base::Digest256 LiteralIdentity(std::string_view value)
{
    return base::Sha256(std::as_bytes(std::span(value.data(), value.size())));
}

void WriteDigest(base::BinaryWriter& writer, const base::Digest256& value)
{
    writer.WriteBytes(value);
}

base::Result<base::Digest256, SparseContractErrorV1> ReadDigest(base::BinaryReader& reader)
{
    auto bytes = reader.ReadBytes(DigestBytes);
    if (!bytes)
        return base::Result<base::Digest256, SparseContractErrorV1>::Failure(
            Error("parse/digest", bytes.Error()));
    base::Digest256 value{};
    std::memcpy(value.data(), bytes.Value().data(), value.size());
    return base::Result<base::Digest256, SparseContractErrorV1>::Success(value);
}

std::uint32_t DispatchGroups(std::uint32_t activeCount) noexcept
{
    return activeCount == 0 ? 0u : 1u + ((activeCount - 1u) / semantic::ThreadsPerGroupV1);
}
}

FrozenCompactIndexListArtifactV1::FrozenCompactIndexListArtifactV1(
    SparseCandidateKindV1 candidateKind,
    SparseRepresentationRoleV1 representationRole,
    SparseOperationCodeV1 operationCode,
    std::uint32_t universeCount,
    std::uint32_t activeCount,
    std::uint32_t threadsPerGroup,
    std::uint32_t dispatchGroupsX,
    std::uint32_t indexStrideBytes,
    std::uint32_t fixedCapacityBytes,
    SparseOutputInitializationV1 outputInitialization,
    SparseWriteSetPolicyV1 writeSetPolicy,
    base::Digest256 semanticIdentity,
    base::Digest256 sparseSetIdentity,
    base::Digest256 spiral4IndirectIdentity,
    base::Digest256 targetProfileIdentity,
    base::Digest256 consumerProgramIdentity,
    base::Digest256 observationContractIdentity,
    base::Digest256 artifactIdentity,
    std::vector<std::uint32_t> fixedCapacityIndices,
    std::vector<std::byte> bytes)
    : candidateKind_(candidateKind),
      representationRole_(representationRole),
      operationCode_(operationCode),
      universeCount_(universeCount),
      activeCount_(activeCount),
      threadsPerGroup_(threadsPerGroup),
      dispatchGroupsX_(dispatchGroupsX),
      indexStrideBytes_(indexStrideBytes),
      fixedCapacityBytes_(fixedCapacityBytes),
      outputInitialization_(outputInitialization),
      writeSetPolicy_(writeSetPolicy),
      semanticIdentity_(semanticIdentity),
      sparseSetIdentity_(sparseSetIdentity),
      spiral4IndirectIdentity_(spiral4IndirectIdentity),
      targetProfileIdentity_(targetProfileIdentity),
      consumerProgramIdentity_(consumerProgramIdentity),
      observationContractIdentity_(observationContractIdentity),
      artifactIdentity_(artifactIdentity),
      fixedCapacityIndices_(std::move(fixedCapacityIndices)),
      bytes_(std::move(bytes))
{
}

base::Digest256 SparseTargetProfileIdentityV1()
{
    return LiteralIdentity("SGE4-5.Spiral6.TargetProfile.D3D12.WarpCompactIndexList.V1");
}

base::Digest256 CompactIndexConsumerProgramIdentityV1()
{
    return LiteralIdentity("SGE4-5.Spiral6.Program.CompactSortedIndexList.DirectPgaConsumer.V1");
}

base::Digest256 SparseObservationContractIdentityV1()
{
    return LiteralIdentity("SGE4-5.Spiral6.Observation.ExactWriteSet.ActiveReference.InactiveFloat4Zero.V1");
}

base::Digest256 Spiral4DispatchContractIdentityV1()
{
    const auto active = spiral4::semantic::BuildCanonicalActiveWorkSemanticV1();
    const auto artifact = spiral4::contract::BuildCu2SingleIndirectArtifactV1(active);
    return artifact.ArtifactIdentity();
}

FrozenCompactIndexListArtifactV1 BuildCompactIndexListArtifactV1(
    const semantic::SparsePointTransformSemanticV1& semanticValue,
    const semantic::ExactSparseWorkSetV1& set)
{
    const auto semanticIdentity = semantic::SparsePointTransformSemanticIdentityV1(semanticValue);
    const auto setIdentity = semantic::ExactSparseWorkSetIdentityV1(set);
    const auto spiral4Identity = Spiral4DispatchContractIdentityV1();
    const auto targetIdentity = SparseTargetProfileIdentityV1();
    const auto consumerIdentity = CompactIndexConsumerProgramIdentityV1();
    const auto observationIdentity = SparseObservationContractIdentityV1();
    const std::uint32_t activeCount = set.Cardinality().Value();
    const std::uint32_t dispatchGroupsX = DispatchGroups(activeCount);

    std::vector<std::uint32_t> fixed(FixedIndexCount, semantic::InvalidUniverseIndexV1);
    std::copy(set.Indices().begin(), set.Indices().end(), fixed.begin());

    base::BinaryWriter payload;
    payload.WriteU32(ArtifactMagic);
    payload.WriteU16(ArtifactVersion);
    payload.WriteU16(ArtifactKind);
    payload.WriteU32(ExtensionStrategy); // VersionedSidecarSparseExtensionV1
    payload.WriteU32(static_cast<std::uint32_t>(SparseCandidateKindV1::CompactIndexListDispatch));
    payload.WriteU32(static_cast<std::uint32_t>(SparseRepresentationRoleV1::CompactSortedUniverseIndexList));
    payload.WriteU32(static_cast<std::uint32_t>(SparseOperationCodeV1::ExecuteCompactIndexList));
    payload.WriteU32(semantic::WorkUniverseCountV1);
    payload.WriteU32(activeCount);
    payload.WriteU32(semantic::ThreadsPerGroupV1);
    payload.WriteU32(dispatchGroupsX);
    payload.WriteU32(semantic::CompactIndexStrideV1);
    payload.WriteU32(semantic::CompactIndexCapacityBytesV1);
    payload.WriteU32(static_cast<std::uint32_t>(SparseOutputInitializationV1::CanonicalFloat4Zero));
    payload.WriteU32(static_cast<std::uint32_t>(SparseWriteSetPolicyV1::ExactlyVerifiedSparseSet));
    payload.WriteU32(OutputOrderPolicy);
    payload.WriteU32(1); // inactive bytes must remain byte-identical
    payload.WriteU32(1); // zero active count maps to zero Dispatch groups
    WriteDigest(payload, semanticIdentity);
    WriteDigest(payload, setIdentity);
    WriteDigest(payload, spiral4Identity);
    WriteDigest(payload, targetIdentity);
    WriteDigest(payload, consumerIdentity);
    WriteDigest(payload, observationIdentity);
    for (std::uint32_t index : fixed) payload.WriteU32(index);

    if (payload.Size() != PayloadBytes)
        throw std::runtime_error("Compact index-list artifact payload size is not canonical.");
    const auto artifactIdentity = base::Sha256(payload.Bytes());
    base::BinaryWriter complete;
    complete.WriteBytes(payload.Bytes());
    WriteDigest(complete, artifactIdentity);
    auto bytes = std::move(complete).Take();

    return FrozenCompactIndexListArtifactV1(
        SparseCandidateKindV1::CompactIndexListDispatch,
        SparseRepresentationRoleV1::CompactSortedUniverseIndexList,
        SparseOperationCodeV1::ExecuteCompactIndexList,
        semantic::WorkUniverseCountV1,
        activeCount,
        semantic::ThreadsPerGroupV1,
        dispatchGroupsX,
        semantic::CompactIndexStrideV1,
        semantic::CompactIndexCapacityBytesV1,
        SparseOutputInitializationV1::CanonicalFloat4Zero,
        SparseWriteSetPolicyV1::ExactlyVerifiedSparseSet,
        semanticIdentity,
        setIdentity,
        spiral4Identity,
        targetIdentity,
        consumerIdentity,
        observationIdentity,
        artifactIdentity,
        std::move(fixed),
        std::move(bytes));
}

base::Result<FrozenCompactIndexListArtifactV1, SparseContractErrorV1>
ParseCompactIndexListArtifactV1(std::span<const std::byte> bytes)
{
    if (bytes.size() != CompleteBytes)
        return base::Result<FrozenCompactIndexListArtifactV1, SparseContractErrorV1>::Failure(
            Error("parse/size", "Compact index-list artifact has a non-canonical byte size."));

    base::BinaryReader reader(bytes);
    auto magic = reader.ReadU32();
    auto version = reader.ReadU16();
    auto artifactKind = reader.ReadU16();
    std::array<base::Result<std::uint32_t, std::string>, HeaderU32Count> fields{
        reader.ReadU32(), reader.ReadU32(), reader.ReadU32(), reader.ReadU32(), reader.ReadU32(),
        reader.ReadU32(), reader.ReadU32(), reader.ReadU32(), reader.ReadU32(), reader.ReadU32(),
        reader.ReadU32(), reader.ReadU32(), reader.ReadU32(), reader.ReadU32(), reader.ReadU32()};
    if (!magic || !version || !artifactKind)
        return base::Result<FrozenCompactIndexListArtifactV1, SparseContractErrorV1>::Failure(
            Error("parse/header", "Compact index-list artifact header is truncated."));
    for (const auto& field : fields)
        if (!field)
            return base::Result<FrozenCompactIndexListArtifactV1, SparseContractErrorV1>::Failure(
                Error("parse/header", "Compact index-list artifact field list is truncated."));

    auto semanticIdentity = ReadDigest(reader);
    auto setIdentity = ReadDigest(reader);
    auto spiral4Identity = ReadDigest(reader);
    auto targetIdentity = ReadDigest(reader);
    auto consumerIdentity = ReadDigest(reader);
    auto observationIdentity = ReadDigest(reader);
    if (!semanticIdentity || !setIdentity || !spiral4Identity || !targetIdentity ||
        !consumerIdentity || !observationIdentity)
    {
        return base::Result<FrozenCompactIndexListArtifactV1, SparseContractErrorV1>::Failure(
            Error("parse/identity", "Compact index-list identity block is truncated."));
    }

    std::vector<std::uint32_t> fixed;
    fixed.reserve(FixedIndexCount);
    for (std::size_t i = 0; i < FixedIndexCount; ++i)
    {
        auto index = reader.ReadU32();
        if (!index)
            return base::Result<FrozenCompactIndexListArtifactV1, SparseContractErrorV1>::Failure(
                Error("parse/indices", "Compact index-list payload is truncated."));
        fixed.push_back(index.Value());
    }
    auto storedArtifactIdentity = ReadDigest(reader);
    if (!storedArtifactIdentity || reader.Remaining() != 0)
        return base::Result<FrozenCompactIndexListArtifactV1, SparseContractErrorV1>::Failure(
            Error("parse/tail", "Compact index-list digest tail is invalid."));

    const auto calculatedIdentity = base::Sha256(bytes.first(PayloadBytes));
    if (calculatedIdentity != storedArtifactIdentity.Value())
        return base::Result<FrozenCompactIndexListArtifactV1, SparseContractErrorV1>::Failure(
            Error("parse/checksum", "Compact index-list artifact digest mismatch."));

    const auto f = [&](std::size_t i) { return fields[i].Value(); };
    const bool canonicalHeader =
        magic.Value() == ArtifactMagic && version.Value() == ArtifactVersion &&
        artifactKind.Value() == ArtifactKind && f(0) == ExtensionStrategy &&
        f(1) == static_cast<std::uint32_t>(SparseCandidateKindV1::CompactIndexListDispatch) &&
        f(2) == static_cast<std::uint32_t>(SparseRepresentationRoleV1::CompactSortedUniverseIndexList) &&
        f(3) == static_cast<std::uint32_t>(SparseOperationCodeV1::ExecuteCompactIndexList) &&
        f(4) == semantic::WorkUniverseCountV1 && f(5) <= semantic::WorkUniverseCountV1 &&
        f(6) == semantic::ThreadsPerGroupV1 && f(7) == DispatchGroups(f(5)) &&
        f(8) == semantic::CompactIndexStrideV1 && f(9) == semantic::CompactIndexCapacityBytesV1 &&
        f(10) == static_cast<std::uint32_t>(SparseOutputInitializationV1::CanonicalFloat4Zero) &&
        f(11) == static_cast<std::uint32_t>(SparseWriteSetPolicyV1::ExactlyVerifiedSparseSet) &&
        f(12) == OutputOrderPolicy && f(13) == 1 && f(14) == 1;
    if (!canonicalHeader)
        return base::Result<FrozenCompactIndexListArtifactV1, SparseContractErrorV1>::Failure(
            Error("parse/canonical", "Compact index-list artifact contains a non-canonical field."));

    const std::uint32_t activeCount = f(5);
    std::vector<std::uint32_t> active(fixed.begin(), fixed.begin() + activeCount);
    auto rebuiltSet = semantic::BuildExactSparseWorkSetV1(active);
    if (!rebuiltSet || rebuiltSet.Value().Identity() != setIdentity.Value())
        return base::Result<FrozenCompactIndexListArtifactV1, SparseContractErrorV1>::Failure(
            Error("parse/set", "Compact index-list payload does not reconstruct its Sparse set identity."));
    if (std::any_of(fixed.begin() + activeCount, fixed.end(), [](std::uint32_t value)
        { return value != semantic::InvalidUniverseIndexV1; }))
    {
        return base::Result<FrozenCompactIndexListArtifactV1, SparseContractErrorV1>::Failure(
            Error("parse/tail-indices", "Unused compact index-list capacity is not canonical."));
    }

    std::vector<std::byte> owned(bytes.begin(), bytes.end());
    return base::Result<FrozenCompactIndexListArtifactV1, SparseContractErrorV1>::Success(
        FrozenCompactIndexListArtifactV1(
            SparseCandidateKindV1::CompactIndexListDispatch,
            SparseRepresentationRoleV1::CompactSortedUniverseIndexList,
            SparseOperationCodeV1::ExecuteCompactIndexList,
            f(4), activeCount, f(6), f(7), f(8), f(9),
            SparseOutputInitializationV1::CanonicalFloat4Zero,
            SparseWriteSetPolicyV1::ExactlyVerifiedSparseSet,
            semanticIdentity.Value(), setIdentity.Value(), spiral4Identity.Value(),
            targetIdentity.Value(), consumerIdentity.Value(), observationIdentity.Value(),
            storedArtifactIdentity.Value(), std::move(fixed), std::move(owned)));
}

base::Result<void, SparseContractErrorV1>
ValidateCompactIndexListArtifactForExecutionV1(
    const FrozenCompactIndexListArtifactV1& artifact,
    const semantic::SparsePointTransformSemanticV1& semanticValue,
    const semantic::ExactSparseWorkSetV1& set)
{
    auto parsed = ParseCompactIndexListArtifactV1(artifact.Bytes());
    if (!parsed)
        return base::Result<void, SparseContractErrorV1>::Failure(parsed.Error());

    const auto semanticIdentity = semantic::SparsePointTransformSemanticIdentityV1(semanticValue);
    const auto setIdentity = semantic::ExactSparseWorkSetIdentityV1(set);
    if (artifact.SemanticIdentity() != semanticIdentity || parsed.Value().SemanticIdentity() != semanticIdentity)
        return base::Result<void, SparseContractErrorV1>::Failure(
            Error("execution/semantic", "Compact index-list artifact is bound to another Sparse Semantic."));
    if (artifact.SparseSetIdentity() != setIdentity || parsed.Value().SparseSetIdentity() != setIdentity)
        return base::Result<void, SparseContractErrorV1>::Failure(
            Error("execution/set", "Compact index-list artifact is bound to another Sparse set."));
    if (artifact.ArtifactIdentity() != parsed.Value().ArtifactIdentity())
        return base::Result<void, SparseContractErrorV1>::Failure(
            Error("execution/identity", "Compact index-list artifact object does not match its canonical bytes."));
    if (artifact.Spiral4IndirectIdentity() != Spiral4DispatchContractIdentityV1() ||
        artifact.TargetProfileIdentity() != SparseTargetProfileIdentityV1() ||
        artifact.ConsumerProgramIdentity() != CompactIndexConsumerProgramIdentityV1() ||
        artifact.ObservationContractIdentity() != SparseObservationContractIdentityV1())
    {
        return base::Result<void, SparseContractErrorV1>::Failure(
            Error("execution/context", "Compact index-list artifact context identity mismatch."));
    }
    if (artifact.ActiveCount() != set.Cardinality().Value() ||
        artifact.DispatchGroupsX() != DispatchGroups(set.Cardinality().Value()) ||
        artifact.FixedCapacityIndices().size() != FixedIndexCount ||
        !std::equal(set.Indices().begin(), set.Indices().end(), artifact.FixedCapacityIndices().begin()))
    {
        return base::Result<void, SparseContractErrorV1>::Failure(
            Error("execution/indices", "Compact index-list artifact execution fields do not match the Sparse set."));
    }
    return base::Result<void, SparseContractErrorV1>::Success();
}
}
