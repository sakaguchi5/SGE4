#include "Spiral7DeltaContracts.h"

#include "../00_Foundation/BinaryIO.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace sge4_5::spiral7::contract
{
namespace
{
constexpr std::uint32_t ArtifactMagic = 0x314C3753u; // "S7L1"
constexpr std::uint16_t ArtifactVersion = 1;
constexpr std::uint16_t ArtifactKind = 1;
constexpr std::uint32_t ExtensionStrategy = 1; // VersionedSparseTemporalDeltaExtensionV1
constexpr std::uint32_t OutputOrderPolicy = 1; // UniverseIndexOrder
constexpr std::uint32_t RetainWritePolicy = 1; // Forbidden
constexpr std::uint32_t UnusedTailPolicy = 1; // 0xFFFFFFFF / Unused
constexpr std::uint32_t ZeroTransitionPolicy = 1; // zero Dispatch groups
constexpr std::size_t HeaderU32Count = 17;
constexpr std::size_t DigestCount = 12;
constexpr std::size_t DigestBytes = 32;
constexpr std::size_t FixedRecordCount = semantic::WorkUniverseCountV1;
constexpr std::size_t PayloadBytes =
    4 + 2 + 2 + HeaderU32Count * 4 + DigestCount * DigestBytes +
    FixedRecordCount * semantic::DeltaRecordStrideBytesV1;
constexpr std::size_t CompleteBytes = PayloadBytes + DigestBytes;

DeltaContractErrorV1 Error(std::string stage, std::string message)
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

base::Result<base::Digest256, DeltaContractErrorV1> ReadDigest(base::BinaryReader& reader)
{
    auto bytes = reader.ReadBytes(DigestBytes);
    if (!bytes)
    {
        return base::Result<base::Digest256, DeltaContractErrorV1>::Failure(
            Error("parse/digest", bytes.Error()));
    }
    base::Digest256 value{};
    std::memcpy(value.data(), bytes.Value().data(), value.size());
    return base::Result<base::Digest256, DeltaContractErrorV1>::Success(value);
}

std::uint32_t DispatchGroups(std::uint32_t transitionCount) noexcept
{
    return transitionCount == 0 ? 0u :
        1u + ((transitionCount - 1u) / semantic::ThreadsPerGroupV1);
}

std::pair<std::uint32_t, std::uint32_t> CountActions(
    std::span<const semantic::DeltaIndexActionRecordV1> records)
{
    std::uint32_t updates = 0;
    std::uint32_t clears = 0;
    for (const auto& record : records)
    {
        if (record.action == semantic::DeltaActionV1::Update) ++updates;
        else if (record.action == semantic::DeltaActionV1::Clear) ++clears;
        else throw std::invalid_argument("Compact Delta record contains an invalid action.");
    }
    return {updates, clears};
}
} // namespace

FrozenCompactDeltaArtifactV1::FrozenCompactDeltaArtifactV1(
    DeltaCandidateKindV1 candidateKind,
    DeltaRepresentationRoleV1 representationRole,
    DeltaOperationCodeV1 operationCode,
    DeltaOutputHistoryPolicyV1 outputHistoryPolicy,
    std::uint32_t universeCount,
    std::uint32_t transitionCount,
    std::uint32_t updateCount,
    std::uint32_t clearCount,
    std::uint32_t threadsPerGroup,
    std::uint32_t dispatchGroupsX,
    std::uint32_t recordStrideBytes,
    std::uint32_t fixedCapacityBytes,
    base::Digest256 semanticIdentity,
    base::Digest256 invocationIdentity,
    base::Digest256 previousActiveSetIdentity,
    base::Digest256 activeSetIdentity,
    base::Digest256 modifiedSetIdentity,
    base::Digest256 transitionIdentity,
    base::Digest256 spiral4IndirectIdentity,
    base::Digest256 spiral5TemporalIdentity,
    base::Digest256 spiral6SparseIdentity,
    base::Digest256 targetProfileIdentity,
    base::Digest256 consumerProgramIdentity,
    base::Digest256 observationContractIdentity,
    base::Digest256 artifactIdentity,
    std::vector<semantic::DeltaIndexActionRecordV1> fixedCapacityRecords,
    std::vector<std::byte> bytes)
    : candidateKind_(candidateKind),
      representationRole_(representationRole),
      operationCode_(operationCode),
      outputHistoryPolicy_(outputHistoryPolicy),
      universeCount_(universeCount),
      transitionCount_(transitionCount),
      updateCount_(updateCount),
      clearCount_(clearCount),
      threadsPerGroup_(threadsPerGroup),
      dispatchGroupsX_(dispatchGroupsX),
      recordStrideBytes_(recordStrideBytes),
      fixedCapacityBytes_(fixedCapacityBytes),
      semanticIdentity_(semanticIdentity),
      invocationIdentity_(invocationIdentity),
      previousActiveSetIdentity_(previousActiveSetIdentity),
      activeSetIdentity_(activeSetIdentity),
      modifiedSetIdentity_(modifiedSetIdentity),
      transitionIdentity_(transitionIdentity),
      spiral4IndirectIdentity_(spiral4IndirectIdentity),
      spiral5TemporalIdentity_(spiral5TemporalIdentity),
      spiral6SparseIdentity_(spiral6SparseIdentity),
      targetProfileIdentity_(targetProfileIdentity),
      consumerProgramIdentity_(consumerProgramIdentity),
      observationContractIdentity_(observationContractIdentity),
      artifactIdentity_(artifactIdentity),
      fixedCapacityRecords_(std::move(fixedCapacityRecords)),
      bytes_(std::move(bytes))
{
}

base::Digest256 Spiral4IndirectExtensionIdentityV1()
{
    return spiral6::contract::Spiral4DispatchContractIdentityV1();
}

base::Digest256 Spiral5TemporalExtensionIdentityV1()
{
    return LiteralIdentity("SGE4-5.VersionedSidecarTemporalExtensionV1");
}

base::Digest256 Spiral6SparseExtensionIdentityV1()
{
    return LiteralIdentity("SGE4-5.VersionedSidecarSparseExtensionV1");
}

base::Digest256 DeltaTargetProfileIdentityV1()
{
    return LiteralIdentity("SGE4-5.Spiral7.TargetProfile.D3D12.WarpCompactDeltaHistory.V1");
}

base::Digest256 CompactDeltaConsumerProgramIdentityV1()
{
    return LiteralIdentity("SGE4-5.Spiral7.Program.CompactDeltaIndexAction.DirectPgaHistoryConsumer.V1");
}

base::Digest256 DeltaObservationContractIdentityV1()
{
    return LiteralIdentity("SGE4-5.Spiral7.Observation.UpdateW.ClearR.HoldH.ExactTransitionWriteSet.V1");
}

FrozenCompactDeltaArtifactV1 BuildCompactDeltaArtifactV1(
    const semantic::SparseTemporalDeltaSemanticV1& semanticValue,
    const semantic::SparseDeltaInvocationV1& invocation)
{
    const auto semanticIdentity = semantic::SparseTemporalDeltaSemanticIdentityV1(semanticValue);
    const auto invocationIdentity = semantic::SparseDeltaInvocationIdentityV1(invocation);
    const auto transitionIdentity = semantic::DeltaTransitionRecordsIdentityV1(invocation.TransitionRecords());
    const auto [updateCount, clearCount] = CountActions(invocation.TransitionRecords());
    const std::uint32_t transitionCount = static_cast<std::uint32_t>(invocation.TransitionRecords().size());

    std::vector<semantic::DeltaIndexActionRecordV1> fixed(
        FixedRecordCount,
        {semantic::InvalidUniverseIndexV1, semantic::DeltaActionV1::Unused});
    std::copy(invocation.TransitionRecords().begin(), invocation.TransitionRecords().end(), fixed.begin());

    const auto spiral4Identity = Spiral4IndirectExtensionIdentityV1();
    const auto spiral5Identity = Spiral5TemporalExtensionIdentityV1();
    const auto spiral6Identity = Spiral6SparseExtensionIdentityV1();
    const auto targetIdentity = DeltaTargetProfileIdentityV1();
    const auto programIdentity = CompactDeltaConsumerProgramIdentityV1();
    const auto observationIdentity = DeltaObservationContractIdentityV1();

    base::BinaryWriter payload;
    payload.WriteU32(ArtifactMagic);
    payload.WriteU16(ArtifactVersion);
    payload.WriteU16(ArtifactKind);
    payload.WriteU32(ExtensionStrategy);
    payload.WriteU32(static_cast<std::uint32_t>(DeltaCandidateKindV1::CompactDeltaIndexHistoryReuse));
    payload.WriteU32(static_cast<std::uint32_t>(DeltaRepresentationRoleV1::CanonicalSortedIndexActionList));
    payload.WriteU32(static_cast<std::uint32_t>(DeltaOperationCodeV1::ExecuteCompactDeltaHistory));
    payload.WriteU32(static_cast<std::uint32_t>(
        DeltaOutputHistoryPolicyV1::CopyPreviousHistoryThenWriteExactlyTransitionSet));
    payload.WriteU32(semantic::WorkUniverseCountV1);
    payload.WriteU32(transitionCount);
    payload.WriteU32(updateCount);
    payload.WriteU32(clearCount);
    payload.WriteU32(semantic::ThreadsPerGroupV1);
    payload.WriteU32(DispatchGroups(transitionCount));
    payload.WriteU32(semantic::DeltaRecordStrideBytesV1);
    payload.WriteU32(semantic::DeltaRecordCapacityBytesV1);
    payload.WriteU32(OutputOrderPolicy);
    payload.WriteU32(RetainWritePolicy);
    payload.WriteU32(UnusedTailPolicy);
    payload.WriteU32(ZeroTransitionPolicy);
    WriteDigest(payload, semanticIdentity);
    WriteDigest(payload, invocationIdentity);
    WriteDigest(payload, invocation.PreviousActiveSet().Identity());
    WriteDigest(payload, invocation.ActiveSet().Identity());
    WriteDigest(payload, invocation.ModifiedSurvivorSet().Identity());
    WriteDigest(payload, transitionIdentity);
    WriteDigest(payload, spiral4Identity);
    WriteDigest(payload, spiral5Identity);
    WriteDigest(payload, spiral6Identity);
    WriteDigest(payload, targetIdentity);
    WriteDigest(payload, programIdentity);
    WriteDigest(payload, observationIdentity);
    for (const auto& record : fixed)
    {
        payload.WriteU32(record.universeIndex);
        payload.WriteU32(static_cast<std::uint32_t>(record.action));
    }
    if (payload.Size() != PayloadBytes)
        throw std::runtime_error("Compact Delta artifact payload size is not canonical.");

    const auto artifactIdentity = base::Sha256(payload.Bytes());
    base::BinaryWriter complete;
    complete.WriteBytes(payload.Bytes());
    WriteDigest(complete, artifactIdentity);
    auto bytes = std::move(complete).Take();

    return FrozenCompactDeltaArtifactV1(
        DeltaCandidateKindV1::CompactDeltaIndexHistoryReuse,
        DeltaRepresentationRoleV1::CanonicalSortedIndexActionList,
        DeltaOperationCodeV1::ExecuteCompactDeltaHistory,
        DeltaOutputHistoryPolicyV1::CopyPreviousHistoryThenWriteExactlyTransitionSet,
        semantic::WorkUniverseCountV1,
        transitionCount,
        updateCount,
        clearCount,
        semantic::ThreadsPerGroupV1,
        DispatchGroups(transitionCount),
        semantic::DeltaRecordStrideBytesV1,
        semantic::DeltaRecordCapacityBytesV1,
        semanticIdentity,
        invocationIdentity,
        invocation.PreviousActiveSet().Identity(),
        invocation.ActiveSet().Identity(),
        invocation.ModifiedSurvivorSet().Identity(),
        transitionIdentity,
        spiral4Identity,
        spiral5Identity,
        spiral6Identity,
        targetIdentity,
        programIdentity,
        observationIdentity,
        artifactIdentity,
        std::move(fixed),
        std::move(bytes));
}

base::Result<FrozenCompactDeltaArtifactV1, DeltaContractErrorV1>
ParseCompactDeltaArtifactV1(std::span<const std::byte> bytes)
{
    if (bytes.size() != CompleteBytes)
    {
        return base::Result<FrozenCompactDeltaArtifactV1, DeltaContractErrorV1>::Failure(
            Error("parse/size", "Compact Delta artifact has a non-canonical byte size."));
    }

    base::BinaryReader reader(bytes);
    auto magic = reader.ReadU32();
    auto version = reader.ReadU16();
    auto kind = reader.ReadU16();
    std::array<base::Result<std::uint32_t, std::string>, HeaderU32Count> fields{
        reader.ReadU32(), reader.ReadU32(), reader.ReadU32(), reader.ReadU32(),
        reader.ReadU32(), reader.ReadU32(), reader.ReadU32(), reader.ReadU32(),
        reader.ReadU32(), reader.ReadU32(), reader.ReadU32(), reader.ReadU32(),
        reader.ReadU32(), reader.ReadU32(), reader.ReadU32(), reader.ReadU32(),
        reader.ReadU32()};
    if (!magic || !version || !kind)
    {
        return base::Result<FrozenCompactDeltaArtifactV1, DeltaContractErrorV1>::Failure(
            Error("parse/header", "Compact Delta artifact header is truncated."));
    }
    for (const auto& field : fields)
    {
        if (!field)
        {
            return base::Result<FrozenCompactDeltaArtifactV1, DeltaContractErrorV1>::Failure(
                Error("parse/header", "Compact Delta artifact field list is truncated."));
        }
    }

    std::array<base::Digest256, DigestCount> digests{};
    for (std::size_t i = 0; i < digests.size(); ++i)
    {
        auto digest = ReadDigest(reader);
        if (!digest)
        {
            return base::Result<FrozenCompactDeltaArtifactV1, DeltaContractErrorV1>::Failure(
                Error("parse/identity", "Compact Delta identity block is truncated."));
        }
        digests[i] = digest.Value();
    }

    std::vector<semantic::DeltaIndexActionRecordV1> fixed;
    fixed.reserve(FixedRecordCount);
    for (std::size_t i = 0; i < FixedRecordCount; ++i)
    {
        auto index = reader.ReadU32();
        auto action = reader.ReadU32();
        if (!index || !action)
        {
            return base::Result<FrozenCompactDeltaArtifactV1, DeltaContractErrorV1>::Failure(
                Error("parse/records", "Compact Delta record payload is truncated."));
        }
        fixed.push_back({index.Value(), static_cast<semantic::DeltaActionV1>(action.Value())});
    }

    auto storedIdentity = ReadDigest(reader);
    if (!storedIdentity || reader.Remaining() != 0)
    {
        return base::Result<FrozenCompactDeltaArtifactV1, DeltaContractErrorV1>::Failure(
            Error("parse/tail", "Compact Delta digest tail is invalid."));
    }
    const auto calculatedIdentity = base::Sha256(bytes.first(PayloadBytes));
    if (calculatedIdentity != storedIdentity.Value())
    {
        return base::Result<FrozenCompactDeltaArtifactV1, DeltaContractErrorV1>::Failure(
            Error("parse/checksum", "Compact Delta artifact digest mismatch."));
    }

    const auto f = [&](std::size_t index) { return fields[index].Value(); };
    const bool canonicalHeader =
        magic.Value() == ArtifactMagic && version.Value() == ArtifactVersion &&
        kind.Value() == ArtifactKind && f(0) == ExtensionStrategy &&
        f(1) == static_cast<std::uint32_t>(DeltaCandidateKindV1::CompactDeltaIndexHistoryReuse) &&
        f(2) == static_cast<std::uint32_t>(DeltaRepresentationRoleV1::CanonicalSortedIndexActionList) &&
        f(3) == static_cast<std::uint32_t>(DeltaOperationCodeV1::ExecuteCompactDeltaHistory) &&
        f(4) == static_cast<std::uint32_t>(
            DeltaOutputHistoryPolicyV1::CopyPreviousHistoryThenWriteExactlyTransitionSet) &&
        f(5) == semantic::WorkUniverseCountV1 && f(6) <= semantic::WorkUniverseCountV1 &&
        f(7) + f(8) == f(6) && f(9) == semantic::ThreadsPerGroupV1 &&
        f(10) == DispatchGroups(f(6)) && f(11) == semantic::DeltaRecordStrideBytesV1 &&
        f(12) == semantic::DeltaRecordCapacityBytesV1 && f(13) == OutputOrderPolicy &&
        f(14) == RetainWritePolicy && f(15) == UnusedTailPolicy &&
        f(16) == ZeroTransitionPolicy;
    if (!canonicalHeader)
    {
        return base::Result<FrozenCompactDeltaArtifactV1, DeltaContractErrorV1>::Failure(
            Error("parse/canonical", "Compact Delta artifact contains a non-canonical field."));
    }

    const std::uint32_t transitionCount = f(6);
    std::vector<semantic::DeltaIndexActionRecordV1> activeRecords(
        fixed.begin(), fixed.begin() + transitionCount);
    try
    {
        if (semantic::DeltaTransitionRecordsIdentityV1(activeRecords) != digests[5])
        {
            return base::Result<FrozenCompactDeltaArtifactV1, DeltaContractErrorV1>::Failure(
                Error("parse/transition", "Compact Delta records do not match Transition identity."));
        }
    }
    catch (const std::exception& error)
    {
        return base::Result<FrozenCompactDeltaArtifactV1, DeltaContractErrorV1>::Failure(
            Error("parse/transition", error.what()));
    }

    std::uint32_t updates = 0;
    std::uint32_t clears = 0;
    for (const auto& record : activeRecords)
    {
        if (record.action == semantic::DeltaActionV1::Update) ++updates;
        else if (record.action == semantic::DeltaActionV1::Clear) ++clears;
    }
    if (updates != f(7) || clears != f(8))
    {
        return base::Result<FrozenCompactDeltaArtifactV1, DeltaContractErrorV1>::Failure(
            Error("parse/action-count", "Compact Delta action counts are inconsistent."));
    }
    if (std::any_of(fixed.begin() + transitionCount, fixed.end(), [](const auto& record)
        {
            return record.universeIndex != semantic::InvalidUniverseIndexV1 ||
                   record.action != semantic::DeltaActionV1::Unused;
        }))
    {
        return base::Result<FrozenCompactDeltaArtifactV1, DeltaContractErrorV1>::Failure(
            Error("parse/tail-records", "Unused Compact Delta capacity is not canonical."));
    }

    std::vector<std::byte> owned(bytes.begin(), bytes.end());
    return base::Result<FrozenCompactDeltaArtifactV1, DeltaContractErrorV1>::Success(
        FrozenCompactDeltaArtifactV1(
            DeltaCandidateKindV1::CompactDeltaIndexHistoryReuse,
            DeltaRepresentationRoleV1::CanonicalSortedIndexActionList,
            DeltaOperationCodeV1::ExecuteCompactDeltaHistory,
            DeltaOutputHistoryPolicyV1::CopyPreviousHistoryThenWriteExactlyTransitionSet,
            f(5), f(6), f(7), f(8), f(9), f(10), f(11), f(12),
            digests[0], digests[1], digests[2], digests[3], digests[4], digests[5],
            digests[6], digests[7], digests[8], digests[9], digests[10], digests[11],
            storedIdentity.Value(), std::move(fixed), std::move(owned)));
}

base::Result<void, DeltaContractErrorV1>
ValidateCompactDeltaArtifactForExecutionV1(
    const FrozenCompactDeltaArtifactV1& artifact,
    const semantic::SparseTemporalDeltaSemanticV1& semanticValue,
    const semantic::SparseDeltaInvocationV1& invocation)
{
    const auto fail = [](std::string stage, std::string message)
    {
        return base::Result<void, DeltaContractErrorV1>::Failure(
            Error(std::move(stage), std::move(message)));
    };

    if (artifact.SemanticIdentity() != semantic::SparseTemporalDeltaSemanticIdentityV1(semanticValue))
        return fail("validate/semantic", "Compact Delta Semantic identity mismatch.");
    if (artifact.InvocationIdentity() != invocation.Identity())
        return fail("validate/invocation", "Compact Delta invocation identity mismatch.");
    if (artifact.PreviousActiveSetIdentity() != invocation.PreviousActiveSet().Identity() ||
        artifact.ActiveSetIdentity() != invocation.ActiveSet().Identity() ||
        artifact.ModifiedSetIdentity() != invocation.ModifiedSurvivorSet().Identity())
        return fail("validate/input-sets", "Compact Delta input set identity mismatch.");
    if (artifact.TransitionIdentity() !=
        semantic::DeltaTransitionRecordsIdentityV1(invocation.TransitionRecords()))
        return fail("validate/transition", "Compact Delta Transition identity mismatch.");
    if (artifact.Spiral4IndirectIdentity() != Spiral4IndirectExtensionIdentityV1() ||
        artifact.Spiral5TemporalIdentity() != Spiral5TemporalExtensionIdentityV1() ||
        artifact.Spiral6SparseIdentity() != Spiral6SparseExtensionIdentityV1())
        return fail("validate/prior-extensions", "Prior extension identity mismatch.");
    if (artifact.TargetProfileIdentity() != DeltaTargetProfileIdentityV1() ||
        artifact.ConsumerProgramIdentity() != CompactDeltaConsumerProgramIdentityV1() ||
        artifact.ObservationContractIdentity() != DeltaObservationContractIdentityV1())
        return fail("validate/program", "Compact Delta target/program/observation identity mismatch.");
    if (artifact.TransitionCount() !=
        static_cast<std::uint32_t>(invocation.TransitionRecords().size()) ||
        artifact.DispatchGroupsX() != DispatchGroups(artifact.TransitionCount()))
        return fail("validate/dispatch", "Compact Delta Dispatch count mismatch.");
    if (!std::equal(invocation.TransitionRecords().begin(), invocation.TransitionRecords().end(),
                    artifact.FixedCapacityRecords().begin()))
        return fail("validate/records", "Compact Delta records do not match the verified invocation.");
    auto parsed = ParseCompactDeltaArtifactV1(artifact.Bytes());
    if (!parsed || parsed.Value().ArtifactIdentity() != artifact.ArtifactIdentity())
        return fail("validate/bytes", "Compact Delta artifact bytes failed canonical parse.");
    return base::Result<void, DeltaContractErrorV1>::Success();
}
} // namespace sge4_5::spiral7::contract
