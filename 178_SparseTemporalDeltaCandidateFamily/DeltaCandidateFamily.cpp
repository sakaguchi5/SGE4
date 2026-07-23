#include "DeltaCandidateFamily.h"

#include "../00_Foundation/BinaryIO.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace sge4_5::spiral7::family_candidate
{
namespace
{
base::Digest256 Literal(std::string_view value)
{
    return base::Sha256(std::as_bytes(std::span(value.data(), value.size())));
}
void Digest(base::BinaryWriter& writer, const base::Digest256& value) { writer.WriteBytes(value); }

struct Shape final
{
    DeltaFamilyRepresentationRoleV1 role{};
    DeltaFamilyOperationCodeV1 operation{};
    DeltaFamilyDispatchPolicyV1 dispatch{};
    DeltaFamilyOutputHistoryPolicyV1 history{};
    DeltaFamilyLegalWriteSetV1 writeSet{};
    std::uint32_t payloadStride = 0;
    std::uint32_t capacityBytes = 0;
};

Shape PhysicalShape(DeltaFamilyCandidateKindV1 kind)
{
    switch (kind)
    {
    case DeltaFamilyCandidateKindV1::FullActiveDenseRecompute:
        return {DeltaFamilyRepresentationRoleV1::DenseActiveMask4096,
                DeltaFamilyOperationCodeV1::ExecuteFullActiveDenseRecompute,
                DeltaFamilyDispatchPolicyV1::FixedUniverseGroups,
                DeltaFamilyOutputHistoryPolicyV1::WriteEntireUniverseFromCurrentActiveMask,
                DeltaFamilyLegalWriteSetV1::EntireUniverse,
                4u, DenseActiveMaskCapacityBytesV1};
    case DeltaFamilyCandidateKindV1::CompactDeltaIndexHistoryReuse:
        return {DeltaFamilyRepresentationRoleV1::CanonicalSortedIndexActionList,
                DeltaFamilyOperationCodeV1::ExecuteCompactDeltaHistory,
                DeltaFamilyDispatchPolicyV1::CeilTransitionGroups,
                DeltaFamilyOutputHistoryPolicyV1::CopyPreviousHistoryThenWriteExactlyTransitionSet,
                DeltaFamilyLegalWriteSetV1::ExactlyTransitionSet,
                semantic::DeltaRecordStrideBytesV1, CompactDeltaCapacityBytesV1};
    case DeltaFamilyCandidateKindV1::AffectedBlockDeltaHistoryReuse:
        return {DeltaFamilyRepresentationRoleV1::AffectedBlockUpdateClearMasks,
                DeltaFamilyOperationCodeV1::ExecuteAffectedBlockDeltaHistory,
                DeltaFamilyDispatchPolicyV1::OneGroupPerAffectedBlock,
                DeltaFamilyOutputHistoryPolicyV1::CopyPreviousHistoryThenWriteExactlyTransitionSet,
                DeltaFamilyLegalWriteSetV1::ExactlyTransitionSet,
                AffectedBlockRecordStrideBytesV1, AffectedBlockCapacityBytesV1};
    }
    throw std::invalid_argument("Unknown Delta Family Candidate kind.");
}

std::array<std::uint64_t, AffectedBlockCapacityV1> BuildMasks(
    const semantic::ExactSparseWorkSetV1& set)
{
    std::array<std::uint64_t, AffectedBlockCapacityV1> result{};
    for (const std::uint32_t index : set.Indices())
        result[index / semantic::ThreadsPerGroupV1] |=
            std::uint64_t{1} << (index % semantic::ThreadsPerGroupV1);
    return result;
}

std::uint32_t CountAffectedBlocks(const semantic::SparseDeltaInvocationV1& invocation)
{
    const auto update = BuildMasks(invocation.UpdateSet());
    const auto clear = BuildMasks(invocation.DeactivationSet());
    std::uint32_t count = 0;
    for (std::uint32_t block = 0; block < AffectedBlockCapacityV1; ++block)
        if ((update[block] | clear[block]) != 0) ++count;
    return count;
}

std::uint32_t DispatchGroups(
    DeltaFamilyCandidateKindV1 kind,
    const semantic::SparseDeltaInvocationV1& invocation,
    std::uint32_t affectedBlockCount)
{
    switch (kind)
    {
    case DeltaFamilyCandidateKindV1::FullActiveDenseRecompute:
        return semantic::WorkUniverseCountV1 / semantic::ThreadsPerGroupV1;
    case DeltaFamilyCandidateKindV1::CompactDeltaIndexHistoryReuse:
    {
        const auto count = invocation.TransitionSet().Cardinality().Value();
        return count == 0 ? 0u : 1u + ((count - 1u) / semantic::ThreadsPerGroupV1);
    }
    case DeltaFamilyCandidateKindV1::AffectedBlockDeltaHistoryReuse:
        return affectedBlockCount;
    }
    throw std::invalid_argument("Unknown Delta Family Candidate kind.");
}

std::vector<std::byte> BuildPayload(
    DeltaFamilyCandidateKindV1 kind,
    const semantic::SparseDeltaInvocationV1& invocation)
{
    switch (kind)
    {
    case DeltaFamilyCandidateKindV1::FullActiveDenseRecompute:
    {
        std::array<std::uint32_t, DenseActiveMaskCapacityBytesV1 / 4u> words{};
        for (const std::uint32_t index : invocation.ActiveSet().Indices())
            words[index / 32u] |= 1u << (index % 32u);
        std::vector<std::byte> bytes(DenseActiveMaskCapacityBytesV1);
        std::memcpy(bytes.data(), words.data(), bytes.size());
        return bytes;
    }
    case DeltaFamilyCandidateKindV1::CompactDeltaIndexHistoryReuse:
    {
        std::vector<semantic::DeltaIndexActionRecordV1> records(
            semantic::WorkUniverseCountV1);
        std::copy(invocation.TransitionRecords().begin(),
                  invocation.TransitionRecords().end(), records.begin());
        std::vector<std::byte> bytes(CompactDeltaCapacityBytesV1);
        std::memcpy(bytes.data(), records.data(), bytes.size());
        return bytes;
    }
    case DeltaFamilyCandidateKindV1::AffectedBlockDeltaHistoryReuse:
    {
        const auto update = BuildMasks(invocation.UpdateSet());
        const auto clear = BuildMasks(invocation.DeactivationSet());
        std::array<AffectedBlockDeltaRecordV1, AffectedBlockCapacityV1> records{};
        for (auto& record : records) record = {};
        std::uint32_t ordinal = 0;
        for (std::uint32_t block = 0; block < AffectedBlockCapacityV1; ++block)
        {
            if ((update[block] & clear[block]) != 0)
                throw std::runtime_error("Affected Block update and clear masks overlap.");
            if ((update[block] | clear[block]) == 0) continue;
            records[ordinal].blockIndex = block;
            records[ordinal].reserved = 0;
            records[ordinal].updateMask = update[block];
            records[ordinal].clearMask = clear[block];
            ++ordinal;
        }
        std::vector<std::byte> bytes(AffectedBlockCapacityBytesV1);
        std::memcpy(bytes.data(), records.data(), bytes.size());
        return bytes;
    }
    }
    throw std::invalid_argument("Unknown Delta Family Candidate kind.");
}

std::vector<std::byte> BuildArtifactBytes(
    DeltaFamilyCandidateKindV1 kind,
    const Shape& shape,
    const semantic::SparseTemporalDeltaSemanticV1& semanticValue,
    const semantic::SparseDeltaInvocationV1& invocation,
    std::uint32_t affectedBlockCount,
    std::uint32_t dispatchGroups,
    const base::Digest256& semanticIdentity,
    const base::Digest256& transitionRecordsIdentity,
    const base::Digest256& cu2Identity,
    const base::Digest256& cu3AuthorityIdentity,
    const base::Digest256& consumerIdentity,
    const base::Digest256& observationIdentity,
    std::span<const std::byte> payload)
{
    base::BinaryWriter writer;
    Digest(writer, Literal("SGE4-5.Spiral7.FrozenDeltaCandidateFamilyArtifact.V1"));
    writer.WriteU32(static_cast<std::uint32_t>(kind));
    writer.WriteU32(static_cast<std::uint32_t>(shape.role));
    writer.WriteU32(static_cast<std::uint32_t>(shape.operation));
    writer.WriteU32(static_cast<std::uint32_t>(shape.dispatch));
    writer.WriteU32(static_cast<std::uint32_t>(shape.history));
    writer.WriteU32(static_cast<std::uint32_t>(shape.writeSet));
    writer.WriteU32(invocation.InvocationIndex());
    writer.WriteU32(semantic::WorkUniverseCountV1);
    writer.WriteU32(invocation.ActiveSet().Cardinality().Value());
    writer.WriteU32(invocation.TransitionSet().Cardinality().Value());
    writer.WriteU32(invocation.UpdateSet().Cardinality().Value());
    writer.WriteU32(invocation.DeactivationSet().Cardinality().Value());
    writer.WriteU32(invocation.RetainSet().Cardinality().Value());
    writer.WriteU32(affectedBlockCount);
    writer.WriteU32(semantic::ThreadsPerGroupV1);
    writer.WriteU32(dispatchGroups);
    writer.WriteU32(shape.payloadStride);
    writer.WriteU32(shape.capacityBytes);
    writer.WriteU32(static_cast<std::uint32_t>(semantic::BuildCanonicalInactiveHistoryBytesV1().size()));
    writer.WriteU32(semanticValue.outputCapacity);
    writer.WriteU32(spiral6::semantic::OutputRecordStrideV1);
    writer.WriteU32(kind == DeltaFamilyCandidateKindV1::FullActiveDenseRecompute ? 0u : 1u);
    Digest(writer, semanticIdentity);
    Digest(writer, invocation.Identity());
    Digest(writer, invocation.PreviousActiveSet().Identity());
    Digest(writer, invocation.ActiveSet().Identity());
    Digest(writer, invocation.ModifiedSurvivorSet().Identity());
    Digest(writer, invocation.TransitionSet().Identity());
    Digest(writer, transitionRecordsIdentity);
    Digest(writer, cu2Identity);
    Digest(writer, cu3AuthorityIdentity);
    Digest(writer, consumerIdentity);
    Digest(writer, observationIdentity);
    writer.WriteU32(static_cast<std::uint32_t>(payload.size()));
    writer.WriteBytes(payload);
    const auto payloadDigest = base::Sha256(payload);
    Digest(writer, payloadDigest);
    const auto bodyDigest = base::Sha256(writer.Bytes());
    Digest(writer, bodyDigest);
    return std::move(writer).Take();
}
} // namespace

FrozenDeltaFamilyArtifactV1::FrozenDeltaFamilyArtifactV1(
    DeltaFamilyCandidateKindV1 kind,
    DeltaFamilyRepresentationRoleV1 role,
    DeltaFamilyOperationCodeV1 operationCode,
    DeltaFamilyDispatchPolicyV1 dispatchPolicy,
    DeltaFamilyOutputHistoryPolicyV1 outputHistoryPolicy,
    DeltaFamilyLegalWriteSetV1 legalWriteSet,
    std::uint32_t activeCount,
    std::uint32_t transitionCount,
    std::uint32_t updateCount,
    std::uint32_t clearCount,
    std::uint32_t retainCount,
    std::uint32_t affectedBlockCount,
    std::uint32_t dispatchGroupsX,
    std::uint32_t payloadStrideBytes,
    std::uint32_t fixedCapacityBytes,
    base::Digest256 semanticIdentity,
    base::Digest256 invocationIdentity,
    base::Digest256 previousActiveSetIdentity,
    base::Digest256 activeSetIdentity,
    base::Digest256 modifiedSetIdentity,
    base::Digest256 transitionSetIdentity,
    base::Digest256 transitionRecordsIdentity,
    base::Digest256 cu2CompactArtifactIdentity,
    base::Digest256 cu3TransitionAuthorityIdentity,
    base::Digest256 consumerProgramIdentity,
    base::Digest256 observationContractIdentity,
    base::Digest256 artifactIdentity,
    std::vector<std::byte> payloadBytes,
    std::vector<std::byte> bytes)
    : kind_(kind), role_(role), operationCode_(operationCode), dispatchPolicy_(dispatchPolicy),
      outputHistoryPolicy_(outputHistoryPolicy), legalWriteSet_(legalWriteSet),
      activeCount_(activeCount), transitionCount_(transitionCount), updateCount_(updateCount),
      clearCount_(clearCount), retainCount_(retainCount), affectedBlockCount_(affectedBlockCount),
      dispatchGroupsX_(dispatchGroupsX), payloadStrideBytes_(payloadStrideBytes),
      fixedCapacityBytes_(fixedCapacityBytes), semanticIdentity_(semanticIdentity),
      invocationIdentity_(invocationIdentity), previousActiveSetIdentity_(previousActiveSetIdentity),
      activeSetIdentity_(activeSetIdentity), modifiedSetIdentity_(modifiedSetIdentity),
      transitionSetIdentity_(transitionSetIdentity), transitionRecordsIdentity_(transitionRecordsIdentity),
      cu2CompactArtifactIdentity_(cu2CompactArtifactIdentity),
      cu3TransitionAuthorityIdentity_(cu3TransitionAuthorityIdentity),
      consumerProgramIdentity_(consumerProgramIdentity), observationContractIdentity_(observationContractIdentity),
      artifactIdentity_(artifactIdentity), payloadBytes_(std::move(payloadBytes)), bytes_(std::move(bytes))
{
}

const char* DeltaFamilyCandidateNameV1(DeltaFamilyCandidateKindV1 value) noexcept
{
    switch (value)
    {
    case DeltaFamilyCandidateKindV1::FullActiveDenseRecompute: return "FullActiveDenseRecompute";
    case DeltaFamilyCandidateKindV1::CompactDeltaIndexHistoryReuse: return "CompactDeltaIndexHistoryReuse";
    case DeltaFamilyCandidateKindV1::AffectedBlockDeltaHistoryReuse: return "AffectedBlockDeltaHistoryReuse";
    }
    return "Unknown";
}

base::Digest256 DeltaFamilyTargetProfileIdentityV1()
{
    return Literal("SGE4-5.Spiral7.TargetProfile.D3D12.DeltaCandidateFamily.WarpQualified.V1");
}
base::Digest256 DeltaFamilyObservationContractIdentityV1()
{
    return Literal("SGE4-5.Spiral7.Observation.DeltaFamily.PairwiseFullOutput.ExactCandidateWriteSet.V1");
}
base::Digest256 DeltaFamilyCompletionContractIdentityV1()
{
    return Literal("SGE4-5.Spiral7.Completion.DeltaFamily.ExecuteIndirectCandidateWriteSet.V1");
}
base::Digest256 DeltaFamilyDeviceEpochPolicyIdentityV1()
{
    return Literal("SGE4-5.Spiral7.DeviceEpoch.DeltaFamily.ResourcesAndHistory.StaleRejected.V1");
}
base::Digest256 DeltaFamilyConsumerProgramIdentityV1(DeltaFamilyCandidateKindV1 kind)
{
    switch (kind)
    {
    case DeltaFamilyCandidateKindV1::FullActiveDenseRecompute:
        return Literal("SGE4-5.Spiral7.Program.FullActiveDenseRecompute.DirectPgaConsumer.V1");
    case DeltaFamilyCandidateKindV1::CompactDeltaIndexHistoryReuse:
        return Literal("SGE4-5.Spiral7.Program.CompactDeltaIndexHistoryReuse.DirectPgaConsumer.V1");
    case DeltaFamilyCandidateKindV1::AffectedBlockDeltaHistoryReuse:
        return Literal("SGE4-5.Spiral7.Program.AffectedBlockDeltaHistoryReuse.DirectPgaConsumer.V1");
    }
    throw std::invalid_argument("Unknown Delta Family Candidate kind.");
}
base::Digest256 DeltaFamilyRepresentationResourceContractIdentityV1(DeltaFamilyCandidateKindV1 kind)
{
    switch (kind)
    {
    case DeltaFamilyCandidateKindV1::FullActiveDenseRecompute:
        return Literal("SGE4-5.Spiral7.Resource.DenseActiveMask.Buffer512.V1");
    case DeltaFamilyCandidateKindV1::CompactDeltaIndexHistoryReuse:
        return Literal("SGE4-5.Spiral7.Resource.CompactDeltaIndexAction.Buffer32768.V1");
    case DeltaFamilyCandidateKindV1::AffectedBlockDeltaHistoryReuse:
        return Literal("SGE4-5.Spiral7.Resource.AffectedBlockUpdateClearMask.Buffer1536.V1");
    }
    throw std::invalid_argument("Unknown Delta Family Candidate kind.");
}

FrozenDeltaFamilyArtifactV1 BuildDeltaFamilyArtifactV1(
    const semantic::SparseTemporalDeltaSemanticV1& semanticValue,
    const semantic::SparseDeltaInvocationV1& invocation,
    DeltaFamilyCandidateKindV1 kind)
{
    const auto shape = PhysicalShape(kind);
    const auto affectedBlockCount = CountAffectedBlocks(invocation);
    const auto dispatchGroups = DispatchGroups(kind, invocation, affectedBlockCount);
    auto payload = BuildPayload(kind, invocation);
    const auto semanticIdentity = semantic::SparseTemporalDeltaSemanticIdentityV1(semanticValue);
    const auto transitionRecordsIdentity = semantic::DeltaTransitionRecordsIdentityV1(
        invocation.TransitionRecords());
    const auto compactArtifact = contract::BuildCompactDeltaArtifactV1(semanticValue, invocation);
    const auto cu2Identity = kind == DeltaFamilyCandidateKindV1::CompactDeltaIndexHistoryReuse
        ? compactArtifact.ArtifactIdentity() : base::Digest256{};
    const auto cu3Authority = candidate::DeltaTransitionAuthorityIdentityV1(
        semanticValue, invocation, compactArtifact);
    const auto consumerIdentity = DeltaFamilyConsumerProgramIdentityV1(kind);
    const auto observationIdentity = DeltaFamilyObservationContractIdentityV1();
    auto bytes = BuildArtifactBytes(kind, shape, semanticValue, invocation,
        affectedBlockCount, dispatchGroups, semanticIdentity, transitionRecordsIdentity,
        cu2Identity, cu3Authority, consumerIdentity, observationIdentity, payload);
    const auto artifactIdentity = base::Sha256(bytes);
    return FrozenDeltaFamilyArtifactV1(
        kind, shape.role, shape.operation, shape.dispatch, shape.history, shape.writeSet,
        invocation.ActiveSet().Cardinality().Value(),
        invocation.TransitionSet().Cardinality().Value(),
        invocation.UpdateSet().Cardinality().Value(),
        invocation.DeactivationSet().Cardinality().Value(),
        invocation.RetainSet().Cardinality().Value(),
        affectedBlockCount, dispatchGroups, shape.payloadStride, shape.capacityBytes,
        semanticIdentity, invocation.Identity(), invocation.PreviousActiveSet().Identity(),
        invocation.ActiveSet().Identity(), invocation.ModifiedSurvivorSet().Identity(),
        invocation.TransitionSet().Identity(), transitionRecordsIdentity, cu2Identity,
        cu3Authority, consumerIdentity, observationIdentity, artifactIdentity,
        std::move(payload), std::move(bytes));
}

base::Digest256 DeltaFamilyRepresentationResourceIdentityV1(
    const FrozenDeltaFamilyArtifactV1& artifact)
{
    base::BinaryWriter writer;
    Digest(writer, Literal("SGE4-5.Spiral7.ActualDeltaFamilyRepresentationResource.V1"));
    Digest(writer, artifact.ArtifactIdentity());
    Digest(writer, artifact.InvocationIdentity());
    writer.WriteU32(static_cast<std::uint32_t>(artifact.Kind()));
    writer.WriteU32(static_cast<std::uint32_t>(artifact.Role()));
    writer.WriteU32(artifact.ActiveCount());
    writer.WriteU32(artifact.TransitionCount());
    writer.WriteU32(artifact.AffectedBlockCount());
    writer.WriteU32(artifact.PayloadStrideBytes());
    writer.WriteU32(artifact.FixedCapacityBytes());
    return base::Sha256(writer.Bytes());
}

base::Digest256 DeltaFamilyWriteSetAuthorityIdentityV1(
    const semantic::SparseTemporalDeltaSemanticV1& semanticValue,
    const semantic::SparseDeltaInvocationV1& invocation,
    const FrozenDeltaFamilyArtifactV1& artifact)
{
    base::BinaryWriter writer;
    Digest(writer, Literal("SGE4-5.Spiral7.WriteSetAuthority.DeltaCandidateFamily.V1"));
    Digest(writer, semantic::SparseTemporalDeltaSemanticIdentityV1(semanticValue));
    Digest(writer, invocation.Identity());
    Digest(writer, invocation.TransitionSet().Identity());
    Digest(writer, artifact.ArtifactIdentity());
    writer.WriteU32(static_cast<std::uint32_t>(artifact.Kind()));
    writer.WriteU32(static_cast<std::uint32_t>(artifact.LegalWriteSet()));
    writer.WriteU32(artifact.Kind() == DeltaFamilyCandidateKindV1::FullActiveDenseRecompute
        ? semantic::WorkUniverseCountV1 : artifact.TransitionCount());
    return base::Sha256(writer.Bytes());
}

base::Result<void, std::string> ValidateDeltaFamilyArtifactV1(
    const FrozenDeltaFamilyArtifactV1& artifact,
    const semantic::SparseTemporalDeltaSemanticV1& semanticValue,
    const semantic::SparseDeltaInvocationV1& invocation)
{
    try
    {
        const auto expected = BuildDeltaFamilyArtifactV1(semanticValue, invocation, artifact.Kind());
        if (artifact.Bytes() != expected.Bytes() || artifact.ArtifactIdentity() != expected.ArtifactIdentity())
            return base::Result<void, std::string>::Failure(
                "Delta Family artifact bytes differ from canonical derivation.");
        if (artifact.PayloadBytes().size() != artifact.FixedCapacityBytes())
            return base::Result<void, std::string>::Failure(
                "Delta Family payload capacity mismatch.");
        if (artifact.ActiveCount() != invocation.ActiveSet().Cardinality().Value() ||
            artifact.TransitionCount() != invocation.TransitionSet().Cardinality().Value() ||
            artifact.UpdateCount() != invocation.UpdateSet().Cardinality().Value() ||
            artifact.ClearCount() != invocation.DeactivationSet().Cardinality().Value() ||
            artifact.RetainCount() != invocation.RetainSet().Cardinality().Value())
            return base::Result<void, std::string>::Failure(
                "Delta Family artifact set cardinality mismatch.");

        if (artifact.Kind() == DeltaFamilyCandidateKindV1::AffectedBlockDeltaHistoryReuse)
        {
            std::array<AffectedBlockDeltaRecordV1, AffectedBlockCapacityV1> records{};
            std::memcpy(records.data(), artifact.PayloadBytes().data(),
                        artifact.PayloadBytes().size());
            std::vector<std::uint32_t> updates;
            std::vector<std::uint32_t> clears;
            for (std::uint32_t ordinal = 0; ordinal < artifact.AffectedBlockCount(); ++ordinal)
            {
                const auto& record = records[ordinal];
                if (record.blockIndex >= AffectedBlockCapacityV1 || record.reserved != 0 ||
                    (record.updateMask & record.clearMask) != 0)
                    return base::Result<void, std::string>::Failure(
                        "Affected Block record is non-canonical or masks overlap.");
                for (std::uint32_t lane = 0; lane < semantic::ThreadsPerGroupV1; ++lane)
                {
                    const auto bit = std::uint64_t{1} << lane;
                    const auto index = record.blockIndex * semantic::ThreadsPerGroupV1 + lane;
                    if ((record.updateMask & bit) != 0) updates.push_back(index);
                    if ((record.clearMask & bit) != 0) clears.push_back(index);
                }
            }
            if (updates != invocation.UpdateSet().Indices() ||
                clears != invocation.DeactivationSet().Indices())
                return base::Result<void, std::string>::Failure(
                    "Affected Block masks do not reconstruct exact W_t and R_t.");
            for (std::uint32_t ordinal = artifact.AffectedBlockCount();
                 ordinal < AffectedBlockCapacityV1; ++ordinal)
            {
                const auto& record = records[ordinal];
                if (record.blockIndex != UnusedBlockSentinelV1 || record.reserved != 0 ||
                    record.updateMask != 0 || record.clearMask != 0)
                    return base::Result<void, std::string>::Failure(
                        "Affected Block unused tail is not canonical.");
            }
        }
        return base::Result<void, std::string>::Success();
    }
    catch (const std::exception& error)
    {
        return base::Result<void, std::string>::Failure(error.what());
    }
}

std::vector<std::byte> SerializeRawDeltaFamilyCandidateV1(
    const RawDeltaFamilyCandidateV1& value)
{
    base::BinaryWriter writer;
    Digest(writer, Literal("SGE4-5.Spiral7.RawDeltaCandidateFamily.V1"));
    Digest(writer, value.semanticIdentity);
    Digest(writer, value.invocationIdentity);
    Digest(writer, value.previousActiveSetIdentity);
    Digest(writer, value.activeSetIdentity);
    Digest(writer, value.modifiedSetIdentity);
    Digest(writer, value.transitionSetIdentity);
    Digest(writer, value.transitionRecordsIdentity);
    Digest(writer, value.targetProfileIdentity);
    Digest(writer, value.observationContractIdentity);
    Digest(writer, value.representationResourceContractIdentity);
    Digest(writer, value.completionContractIdentity);
    Digest(writer, value.deviceEpochPolicyIdentity);
    Digest(writer, value.consumerProgramIdentity);
    Digest(writer, value.proposedArtifactIdentity);
    Digest(writer, value.proposedRepresentationResourceIdentity);
    Digest(writer, value.writeSetAuthorityIdentity);
    Digest(writer, value.cu2CompactArtifactIdentity);
    Digest(writer, value.cu3TransitionAuthorityIdentity);
    writer.WriteU32(static_cast<std::uint32_t>(value.kind));
    writer.WriteU32(static_cast<std::uint32_t>(value.representationRole));
    writer.WriteU32(static_cast<std::uint32_t>(value.operationCode));
    writer.WriteU32(static_cast<std::uint32_t>(value.dispatchPolicy));
    writer.WriteU32(static_cast<std::uint32_t>(value.outputHistoryPolicy));
    writer.WriteU32(static_cast<std::uint32_t>(value.legalWriteSet));
    writer.WriteU32(static_cast<std::uint32_t>(value.completionPolicy));
    writer.WriteU32(static_cast<std::uint32_t>(value.deviceEpochPolicy));
    writer.WriteU32(value.invocationIndex);
    writer.WriteU32(value.universeCount);
    writer.WriteU32(value.activeCount);
    writer.WriteU32(value.transitionCount);
    writer.WriteU32(value.updateCount);
    writer.WriteU32(value.clearCount);
    writer.WriteU32(value.retainCount);
    writer.WriteU32(value.affectedBlockCount);
    writer.WriteU32(value.threadsPerGroup);
    writer.WriteU32(value.dispatchGroupsX);
    writer.WriteU32(value.payloadStrideBytes);
    writer.WriteU32(value.fixedCapacityBytes);
    writer.WriteU32(value.historyBytes);
    writer.WriteU32(value.outputCapacity);
    writer.WriteU32(value.outputStrideBytes);
    writer.WriteU32(value.zeroWorkMapsToZeroDispatch);
    return std::move(writer).Take();
}

base::Digest256 RawDeltaFamilyCandidateIdentityV1(
    const RawDeltaFamilyCandidateV1& value)
{
    return base::Sha256(SerializeRawDeltaFamilyCandidateV1(value));
}
} // namespace sge4_5::spiral7::family_candidate
