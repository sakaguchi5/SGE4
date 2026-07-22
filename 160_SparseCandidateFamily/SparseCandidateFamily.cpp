#include "SparseCandidateFamily.h"

#include "../00_Foundation/BinaryIO.h"
#include "../155_Spiral6SparseContracts/Spiral6SparseContracts.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace sge4_5::spiral6::family_candidate
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
    SparseFamilyRepresentationRoleV1 role{};
    SparseFamilyOperationCodeV1 operation{};
    SparseFamilyDispatchPolicyV1 dispatch{};
    std::uint32_t payloadStride = 0;
    std::uint32_t capacityBytes = 0;
};

Shape PhysicalShape(SparseFamilyCandidateKindV1 kind)
{
    switch (kind)
    {
    case SparseFamilyCandidateKindV1::DenseMembershipMaskPredicate:
        return {SparseFamilyRepresentationRoleV1::DenseMembershipMask4096,
                SparseFamilyOperationCodeV1::ExecuteDenseMaskPredicate,
                SparseFamilyDispatchPolicyV1::FixedUniverseGroups,
                4u, DenseMaskCapacityBytesV1};
    case SparseFamilyCandidateKindV1::CompactIndexListDispatch:
        return {SparseFamilyRepresentationRoleV1::CompactSortedUniverseIndexList,
                SparseFamilyOperationCodeV1::ExecuteCompactIndexList,
                SparseFamilyDispatchPolicyV1::CeilCardinalityGroups,
                4u, CompactIndexCapacityBytesV1};
    case SparseFamilyCandidateKindV1::ActiveBlockListLocalMask:
        return {SparseFamilyRepresentationRoleV1::ActiveBlockListWithLocal64BitMask,
                SparseFamilyOperationCodeV1::ExecuteActiveBlockLocalMask,
                SparseFamilyDispatchPolicyV1::OneGroupPerActiveBlock,
                ActiveBlockRecordStrideBytesV1, ActiveBlockCapacityBytesV1};
    }
    throw std::invalid_argument("Unknown Sparse Family Candidate kind.");
}

std::uint32_t CountActiveBlocks(const semantic::ExactSparseWorkSetV1& set)
{
    std::array<bool, ActiveBlockCapacityV1> seen{};
    for (const std::uint32_t index : set.Indices()) seen[index / semantic::ThreadsPerGroupV1] = true;
    return static_cast<std::uint32_t>(std::count(seen.begin(), seen.end(), true));
}

std::uint32_t DispatchGroups(
    SparseFamilyCandidateKindV1 kind,
    const semantic::ExactSparseWorkSetV1& set,
    std::uint32_t activeBlockCount)
{
    switch (kind)
    {
    case SparseFamilyCandidateKindV1::DenseMembershipMaskPredicate:
        return semantic::WorkUniverseCountV1 / semantic::ThreadsPerGroupV1;
    case SparseFamilyCandidateKindV1::CompactIndexListDispatch:
        return set.Cardinality().Value() == 0 ? 0u :
            1u + ((set.Cardinality().Value() - 1u) / semantic::ThreadsPerGroupV1);
    case SparseFamilyCandidateKindV1::ActiveBlockListLocalMask:
        return activeBlockCount;
    }
    throw std::invalid_argument("Unknown Sparse Family Candidate kind.");
}

std::vector<std::byte> BuildPayload(
    SparseFamilyCandidateKindV1 kind,
    const semantic::ExactSparseWorkSetV1& set)
{
    switch (kind)
    {
    case SparseFamilyCandidateKindV1::DenseMembershipMaskPredicate:
    {
        std::vector<std::uint32_t> words(DenseMaskCapacityBytesV1 / sizeof(std::uint32_t), 0u);
        for (const std::uint32_t index : set.Indices()) words[index / 32u] |= 1u << (index % 32u);
        std::vector<std::byte> bytes(DenseMaskCapacityBytesV1);
        std::memcpy(bytes.data(), words.data(), bytes.size());
        return bytes;
    }
    case SparseFamilyCandidateKindV1::CompactIndexListDispatch:
    {
        std::vector<std::uint32_t> indices(semantic::WorkUniverseCountV1, semantic::InvalidUniverseIndexV1);
        std::copy(set.Indices().begin(), set.Indices().end(), indices.begin());
        std::vector<std::byte> bytes(CompactIndexCapacityBytesV1);
        std::memcpy(bytes.data(), indices.data(), bytes.size());
        return bytes;
    }
    case SparseFamilyCandidateKindV1::ActiveBlockListLocalMask:
    {
        std::array<ActiveBlockRecordV1, ActiveBlockCapacityV1> records{};
        for (auto& record : records) record = {};
        std::array<std::uint64_t, ActiveBlockCapacityV1> masks{};
        for (const std::uint32_t index : set.Indices())
            masks[index / semantic::ThreadsPerGroupV1] |= std::uint64_t{1} << (index % semantic::ThreadsPerGroupV1);
        std::uint32_t ordinal = 0;
        for (std::uint32_t block = 0; block < ActiveBlockCapacityV1; ++block)
        {
            if (masks[block] == 0) continue;
            records[ordinal].blockIndex = block;
            records[ordinal].maskLow = static_cast<std::uint32_t>(masks[block]);
            records[ordinal].maskHigh = static_cast<std::uint32_t>(masks[block] >> 32u);
            records[ordinal].activeCount = static_cast<std::uint32_t>(std::popcount(masks[block]));
            ++ordinal;
        }
        std::vector<std::byte> bytes(ActiveBlockCapacityBytesV1);
        std::memcpy(bytes.data(), records.data(), bytes.size());
        return bytes;
    }
    }
    throw std::invalid_argument("Unknown Sparse Family Candidate kind.");
}

std::vector<std::byte> BuildArtifactBytes(
    SparseFamilyCandidateKindV1 kind,
    SparseFamilyRepresentationRoleV1 role,
    SparseFamilyOperationCodeV1 operation,
    SparseFamilyDispatchPolicyV1 dispatch,
    std::uint32_t activeCount,
    std::uint32_t activeBlockCount,
    std::uint32_t dispatchGroups,
    std::uint32_t payloadStride,
    std::uint32_t capacityBytes,
    const base::Digest256& semanticIdentity,
    const base::Digest256& setIdentity,
    const base::Digest256& consumerProgramIdentity,
    const base::Digest256& observationIdentity,
    const base::Digest256& cu2Identity,
    std::span<const std::byte> payload)
{
    base::BinaryWriter writer;
    Digest(writer, Literal("SGE4-5.Spiral6.FrozenSparseCandidateFamilyArtifact.V1"));
    writer.WriteU32(static_cast<std::uint32_t>(kind));
    writer.WriteU32(static_cast<std::uint32_t>(role));
    writer.WriteU32(static_cast<std::uint32_t>(operation));
    writer.WriteU32(static_cast<std::uint32_t>(dispatch));
    writer.WriteU32(semantic::WorkUniverseCountV1);
    writer.WriteU32(activeCount);
    writer.WriteU32(activeBlockCount);
    writer.WriteU32(semantic::ThreadsPerGroupV1);
    writer.WriteU32(dispatchGroups);
    writer.WriteU32(payloadStride);
    writer.WriteU32(capacityBytes);
    writer.WriteU32(semantic::WorkUniverseCountV1);
    writer.WriteU32(semantic::OutputRecordStrideV1);
    Digest(writer, semanticIdentity);
    Digest(writer, setIdentity);
    Digest(writer, consumerProgramIdentity);
    Digest(writer, observationIdentity);
    Digest(writer, cu2Identity);
    writer.WriteU32(static_cast<std::uint32_t>(payload.size()));
    writer.WriteBytes(payload);
    const auto digest = base::Sha256(writer.Bytes());
    Digest(writer, digest);
    return std::move(writer).Take();
}
}

FrozenSparseFamilyArtifactV1::FrozenSparseFamilyArtifactV1(
    SparseFamilyCandidateKindV1 kind,
    SparseFamilyRepresentationRoleV1 role,
    SparseFamilyOperationCodeV1 operationCode,
    SparseFamilyDispatchPolicyV1 dispatchPolicy,
    std::uint32_t activeCount,
    std::uint32_t activeBlockCount,
    std::uint32_t dispatchGroupsX,
    std::uint32_t payloadStrideBytes,
    std::uint32_t fixedCapacityBytes,
    base::Digest256 semanticIdentity,
    base::Digest256 sparseSetIdentity,
    base::Digest256 consumerProgramIdentity,
    base::Digest256 observationContractIdentity,
    base::Digest256 cu2CompactArtifactIdentity,
    base::Digest256 artifactIdentity,
    std::vector<std::byte> payloadBytes,
    std::vector<std::byte> bytes)
    : kind_(kind), role_(role), operationCode_(operationCode), dispatchPolicy_(dispatchPolicy),
      activeCount_(activeCount), activeBlockCount_(activeBlockCount), dispatchGroupsX_(dispatchGroupsX),
      payloadStrideBytes_(payloadStrideBytes), fixedCapacityBytes_(fixedCapacityBytes),
      semanticIdentity_(semanticIdentity), sparseSetIdentity_(sparseSetIdentity),
      consumerProgramIdentity_(consumerProgramIdentity), observationContractIdentity_(observationContractIdentity),
      cu2CompactArtifactIdentity_(cu2CompactArtifactIdentity), artifactIdentity_(artifactIdentity),
      payloadBytes_(std::move(payloadBytes)), bytes_(std::move(bytes))
{
}

const char* SparseFamilyCandidateNameV1(SparseFamilyCandidateKindV1 value) noexcept
{
    switch (value)
    {
    case SparseFamilyCandidateKindV1::DenseMembershipMaskPredicate: return "DenseMembershipMaskPredicate";
    case SparseFamilyCandidateKindV1::CompactIndexListDispatch: return "CompactIndexListDispatch";
    case SparseFamilyCandidateKindV1::ActiveBlockListLocalMask: return "ActiveBlockListLocalMask";
    }
    return "Unknown";
}

base::Digest256 SparseFamilyTargetProfileIdentityV1()
{
    return Literal("SGE4-5.Spiral6.TargetProfile.D3D12.SparseCandidateFamily.WarpQualified.V1");
}
base::Digest256 SparseFamilyObservationContractIdentityV1()
{
    return Literal("SGE4-5.Spiral6.Observation.SparseFamily.ExactWriteSet.PairwiseFullOutput.V1");
}
base::Digest256 SparseFamilyCompletionContractIdentityV1()
{
    return Literal("SGE4-5.Spiral6.Completion.SparseFamily.ExecuteIndirectExactWriteSet.V1");
}
base::Digest256 SparseFamilyDeviceEpochPolicyIdentityV1()
{
    return Literal("SGE4-5.Spiral6.DeviceEpoch.SparseFamily.DerivedResource.StaleRejected.V1");
}
base::Digest256 SparseFamilyConsumerProgramIdentityV1()
{
    return Literal("SGE4-5.Spiral6.Program.SparseFamily.DirectPgaConsumer4096.V1");
}
base::Digest256 SparseFamilyRepresentationResourceContractIdentityV1(
    SparseFamilyCandidateKindV1 kind)
{
    switch (kind)
    {
    case SparseFamilyCandidateKindV1::DenseMembershipMaskPredicate:
        return Literal("SGE4-5.Spiral6.Resource.DenseMembershipMask.Buffer512.V1");
    case SparseFamilyCandidateKindV1::CompactIndexListDispatch:
        return Literal("SGE4-5.Spiral6.Resource.CompactSortedIndexList.Buffer16384.V1");
    case SparseFamilyCandidateKindV1::ActiveBlockListLocalMask:
        return Literal("SGE4-5.Spiral6.Resource.ActiveBlockList.LocalMask.Buffer1024.V1");
    }
    throw std::invalid_argument("Unknown Sparse Family Candidate kind.");
}

FrozenSparseFamilyArtifactV1 BuildSparseFamilyArtifactV1(
    const semantic::SparsePointTransformSemanticV1& semanticValue,
    const semantic::ExactSparseWorkSetV1& set,
    SparseFamilyCandidateKindV1 kind)
{
    const auto shape = PhysicalShape(kind);
    const auto activeBlockCount = CountActiveBlocks(set);
    const auto dispatchGroups = DispatchGroups(kind, set, activeBlockCount);
    auto payload = BuildPayload(kind, set);
    const auto semanticIdentity = semantic::SparsePointTransformSemanticIdentityV1(semanticValue);
    const auto consumerIdentity = SparseFamilyConsumerProgramIdentityV1();
    const auto observationIdentity = SparseFamilyObservationContractIdentityV1();
    const auto cu2Identity = kind == SparseFamilyCandidateKindV1::CompactIndexListDispatch
        ? contract::BuildCompactIndexListArtifactV1(semanticValue, set).ArtifactIdentity()
        : base::Digest256{};
    auto bytes = BuildArtifactBytes(kind, shape.role, shape.operation, shape.dispatch,
        set.Cardinality().Value(), activeBlockCount, dispatchGroups, shape.payloadStride,
        shape.capacityBytes, semanticIdentity, set.Identity(), consumerIdentity,
        observationIdentity, cu2Identity, payload);
    const auto artifactIdentity = base::Sha256(bytes);
    return FrozenSparseFamilyArtifactV1(kind, shape.role, shape.operation, shape.dispatch,
        set.Cardinality().Value(), activeBlockCount, dispatchGroups, shape.payloadStride,
        shape.capacityBytes, semanticIdentity, set.Identity(), consumerIdentity,
        observationIdentity, cu2Identity, artifactIdentity, std::move(payload), std::move(bytes));
}

base::Digest256 SparseFamilyRepresentationResourceIdentityV1(
    const FrozenSparseFamilyArtifactV1& artifact)
{
    base::BinaryWriter writer;
    Digest(writer, Literal("SGE4-5.Spiral6.ActualSparseFamilyRepresentationResource.V1"));
    Digest(writer, artifact.ArtifactIdentity());
    Digest(writer, artifact.SparseSetIdentity());
    writer.WriteU32(static_cast<std::uint32_t>(artifact.Kind()));
    writer.WriteU32(static_cast<std::uint32_t>(artifact.Role()));
    writer.WriteU32(artifact.ActiveCount());
    writer.WriteU32(artifact.ActiveBlockCount());
    writer.WriteU32(artifact.PayloadStrideBytes());
    writer.WriteU32(artifact.FixedCapacityBytes());
    return base::Sha256(writer.Bytes());
}

base::Digest256 SparseFamilyWriteSetAuthorityIdentityV1(
    const semantic::SparsePointTransformSemanticV1& semanticValue,
    const semantic::ExactSparseWorkSetV1& set,
    const FrozenSparseFamilyArtifactV1& artifact)
{
    base::BinaryWriter writer;
    Digest(writer, Literal("SGE4-5.Spiral6.WriteSetAuthority.SparseFamily.ExactSet.V1"));
    Digest(writer, semantic::SparsePointTransformSemanticIdentityV1(semanticValue));
    Digest(writer, set.Identity());
    Digest(writer, artifact.ArtifactIdentity());
    writer.WriteU32(static_cast<std::uint32_t>(artifact.Kind()));
    writer.WriteU32(artifact.ActiveCount());
    writer.WriteU32(semantic::WorkUniverseCountV1);
    writer.WriteU32(1u); // canonical Float4Zero initialization
    writer.WriteU32(1u); // exactly S
    return base::Sha256(writer.Bytes());
}

base::Result<void, std::string> ValidateSparseFamilyArtifactV1(
    const FrozenSparseFamilyArtifactV1& artifact,
    const semantic::SparsePointTransformSemanticV1& semanticValue,
    const semantic::ExactSparseWorkSetV1& set)
{
    try
    {
        const auto expected = BuildSparseFamilyArtifactV1(semanticValue, set, artifact.Kind());
        if (artifact.Bytes() != expected.Bytes() || artifact.ArtifactIdentity() != expected.ArtifactIdentity())
            return base::Result<void, std::string>::Failure("Sparse Family artifact bytes differ from canonical derivation.");
        if (artifact.PayloadBytes().size() != artifact.FixedCapacityBytes())
            return base::Result<void, std::string>::Failure("Sparse Family payload capacity mismatch.");
        if (artifact.ActiveCount() != set.Cardinality().Value())
            return base::Result<void, std::string>::Failure("Sparse Family artifact active count mismatch.");
        return base::Result<void, std::string>::Success();
    }
    catch (const std::exception& error)
    {
        return base::Result<void, std::string>::Failure(error.what());
    }
}

std::vector<std::byte> SerializeRawSparseFamilyCandidateV1(
    const RawSparseFamilyCandidateV1& value)
{
    base::BinaryWriter writer;
    Digest(writer, Literal("SGE4-5.Spiral6.RawSparseCandidateFamily.V1"));
    Digest(writer, value.semanticIdentity);
    Digest(writer, value.sparseSetIdentity);
    Digest(writer, value.targetProfileIdentity);
    Digest(writer, value.observationContractIdentity);
    Digest(writer, value.representationResourceContractIdentity);
    Digest(writer, value.completionContractIdentity);
    Digest(writer, value.deviceEpochPolicyIdentity);
    Digest(writer, value.spiral4IndirectArtifactIdentity);
    Digest(writer, value.consumerProgramIdentity);
    Digest(writer, value.proposedArtifactIdentity);
    Digest(writer, value.proposedRepresentationResourceIdentity);
    Digest(writer, value.writeSetAuthorityIdentity);
    Digest(writer, value.cu2CompactArtifactIdentity);
    writer.WriteU32(static_cast<std::uint32_t>(value.kind));
    writer.WriteU32(static_cast<std::uint32_t>(value.representationRole));
    writer.WriteU32(static_cast<std::uint32_t>(value.operationCode));
    writer.WriteU32(static_cast<std::uint32_t>(value.dispatchPolicy));
    writer.WriteU32(static_cast<std::uint32_t>(value.completionPolicy));
    writer.WriteU32(static_cast<std::uint32_t>(value.deviceEpochPolicy));
    writer.WriteU32(value.universeCount);
    writer.WriteU32(value.activeCount);
    writer.WriteU32(value.activeBlockCount);
    writer.WriteU32(value.threadsPerGroup);
    writer.WriteU32(value.dispatchGroupsX);
    writer.WriteU32(value.payloadStrideBytes);
    writer.WriteU32(value.fixedCapacityBytes);
    writer.WriteU32(value.outputCapacity);
    writer.WriteU32(value.outputStrideBytes);
    writer.WriteU32(value.zeroActiveMapsToZeroDispatch);
    return std::move(writer).Take();
}

base::Digest256 RawSparseFamilyCandidateIdentityV1(
    const RawSparseFamilyCandidateV1& value)
{
    return base::Sha256(SerializeRawSparseFamilyCandidateV1(value));
}
}
