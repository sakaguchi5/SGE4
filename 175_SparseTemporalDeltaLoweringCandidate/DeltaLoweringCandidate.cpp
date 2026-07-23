#include "DeltaLoweringCandidate.h"

#include "../00_Foundation/BinaryIO.h"

#include <span>
#include <string_view>

namespace sge4_5::spiral7::candidate
{
namespace
{
base::Digest256 LiteralIdentity(std::string_view value)
{
    return base::Sha256(std::as_bytes(std::span(value.data(), value.size())));
}

void WriteDigest(base::BinaryWriter& writer, const base::Digest256& value)
{
    writer.WriteBytes(value);
}
} // namespace

base::Digest256 ProposedDeltaTargetProfileIdentityV1()
{
    return LiteralIdentity("SGE4-5.Spiral7.TargetProfile.D3D12.WarpCompactDeltaHistory.V1");
}

base::Digest256 ProposedDeltaObservationContractIdentityV1()
{
    return LiteralIdentity("SGE4-5.Spiral7.Observation.UpdateW.ClearR.HoldH.ExactTransitionWriteSet.V1");
}

base::Digest256 ProposedDeltaRecordResourceContractIdentityV1()
{
    return LiteralIdentity("SGE4-5.Spiral7.Resource.CompactDeltaIndexAction.Buffer32768.FixedTail.V1");
}

base::Digest256 ProposedPreviousHistoryResourceContractIdentityV1()
{
    return LiteralIdentity("SGE4-5.Spiral7.Resource.PreviousHistory.Float4x4096.ReadOnly.V1");
}

base::Digest256 ProposedOutputHistoryResourceContractIdentityV1()
{
    return LiteralIdentity("SGE4-5.Spiral7.Resource.OutputHistory.Float4x4096.WriteOnly.V1");
}

base::Digest256 ProposedDeltaCompletionContractIdentityV1()
{
    return LiteralIdentity("SGE4-5.Spiral7.Completion.ExecuteIndirectAuthorizesExactTransitionWriteSet.V1");
}

base::Digest256 ProposedDeltaDeviceEpochPolicyIdentityV1()
{
    return LiteralIdentity("SGE4-5.Spiral7.DeviceEpoch.HistoryAndDeltaResources.StaleRejected.V1");
}

base::Digest256 ProposedDeltaRecordResourceIdentityV1(
    const contract::FrozenCompactDeltaArtifactV1& artifact)
{
    base::BinaryWriter writer;
    WriteDigest(writer, LiteralIdentity("SGE4-5.Spiral7.ActualResource.CompactDeltaRecord.V1"));
    WriteDigest(writer, artifact.ArtifactIdentity());
    WriteDigest(writer, artifact.InvocationIdentity());
    WriteDigest(writer, artifact.TransitionIdentity());
    writer.WriteU32(static_cast<std::uint32_t>(artifact.RepresentationRole()));
    writer.WriteU32(artifact.TransitionCount());
    writer.WriteU32(artifact.RecordStrideBytes());
    writer.WriteU32(artifact.FixedCapacityBytes());
    return base::Sha256(writer.Bytes());
}

base::Digest256 ProposedPreviousHistoryResourceIdentityV1(
    const semantic::SparseDeltaInvocationV1& invocation)
{
    base::BinaryWriter writer;
    WriteDigest(writer, LiteralIdentity("SGE4-5.Spiral7.ActualResource.PreviousHistory.V1"));
    WriteDigest(writer, invocation.PreviousActiveSet().Identity());
    WriteDigest(writer, invocation.Identity());
    writer.WriteU32(invocation.InvocationIndex());
    writer.WriteU32(semantic::WorkUniverseCountV1);
    writer.WriteU32(semantic::WorkUniverseCountV1 *
        static_cast<std::uint32_t>(sizeof(spiral5::semantic::PointRecordV1)));
    return base::Sha256(writer.Bytes());
}

base::Digest256 ProposedOutputHistoryResourceIdentityV1(
    const semantic::SparseDeltaInvocationV1& invocation)
{
    base::BinaryWriter writer;
    WriteDigest(writer, LiteralIdentity("SGE4-5.Spiral7.ActualResource.OutputHistory.V1"));
    WriteDigest(writer, invocation.ActiveSet().Identity());
    WriteDigest(writer, invocation.Identity());
    writer.WriteU32(invocation.InvocationIndex());
    writer.WriteU32(semantic::WorkUniverseCountV1);
    writer.WriteU32(semantic::WorkUniverseCountV1 *
        static_cast<std::uint32_t>(sizeof(spiral5::semantic::PointRecordV1)));
    return base::Sha256(writer.Bytes());
}

base::Digest256 DeltaTransitionAuthorityIdentityV1(
    const semantic::SparseTemporalDeltaSemanticV1& semanticValue,
    const semantic::SparseDeltaInvocationV1& invocation,
    const contract::FrozenCompactDeltaArtifactV1& artifact)
{
    base::BinaryWriter writer;
    WriteDigest(writer, LiteralIdentity("SGE4-5.Spiral7.Authority.UpdateW.ClearR.HoldH.V1"));
    WriteDigest(writer, semantic::SparseTemporalDeltaSemanticIdentityV1(semanticValue));
    WriteDigest(writer, invocation.Identity());
    WriteDigest(writer, invocation.PreviousActiveSet().Identity());
    WriteDigest(writer, invocation.ActiveSet().Identity());
    WriteDigest(writer, invocation.ModifiedSurvivorSet().Identity());
    WriteDigest(writer, invocation.UpdateSet().Identity());
    WriteDigest(writer, invocation.RetainSet().Identity());
    WriteDigest(writer, invocation.DeactivationSet().Identity());
    WriteDigest(writer, invocation.TransitionSet().Identity());
    WriteDigest(writer, semantic::DeltaTransitionRecordsIdentityV1(invocation.TransitionRecords()));
    WriteDigest(writer, artifact.ArtifactIdentity());
    writer.WriteU32(static_cast<std::uint32_t>(artifact.OutputHistoryPolicy()));
    writer.WriteU32(artifact.UpdateCount());
    writer.WriteU32(artifact.ClearCount());
    writer.WriteU32(invocation.RetainSet().Cardinality().Value());
    writer.WriteU32(artifact.TransitionCount());
    return base::Sha256(writer.Bytes());
}

std::vector<std::byte> SerializeRawDeltaLoweringCandidateV1(
    const RawDeltaLoweringCandidateV1& value)
{
    base::BinaryWriter writer;
    WriteDigest(writer, LiteralIdentity("SGE4-5.Spiral7.RawDeltaLoweringCandidate.V1"));
    for (const auto* digest : {
        &value.semanticIdentity, &value.invocationIdentity,
        &value.previousActiveSetIdentity, &value.activeSetIdentity,
        &value.modifiedSetIdentity, &value.transitionSetIdentity,
        &value.transitionRecordsIdentity, &value.targetProfileIdentity,
        &value.observationContractIdentity, &value.deltaResourceContractIdentity,
        &value.previousHistoryResourceContractIdentity,
        &value.outputHistoryResourceContractIdentity, &value.completionContractIdentity,
        &value.deviceEpochPolicyIdentity, &value.spiral4IndirectIdentity,
        &value.spiral5TemporalIdentity, &value.spiral6SparseIdentity,
        &value.consumerProgramIdentity, &value.proposedArtifactIdentity,
        &value.proposedDeltaResourceIdentity,
        &value.proposedPreviousHistoryResourceIdentity,
        &value.proposedOutputHistoryResourceIdentity,
        &value.transitionAuthorityIdentity})
    {
        WriteDigest(writer, *digest);
    }
    writer.WriteU32(static_cast<std::uint32_t>(value.kind));
    writer.WriteU32(static_cast<std::uint32_t>(value.representationRole));
    writer.WriteU32(static_cast<std::uint32_t>(value.operationCode));
    writer.WriteU32(static_cast<std::uint32_t>(value.outputHistoryPolicy));
    writer.WriteU32(static_cast<std::uint32_t>(value.completionPolicy));
    writer.WriteU32(static_cast<std::uint32_t>(value.deviceEpochPolicy));
    writer.WriteU32(value.invocationIndex);
    writer.WriteU32(value.universeCount);
    writer.WriteU32(value.transitionCount);
    writer.WriteU32(value.updateCount);
    writer.WriteU32(value.clearCount);
    writer.WriteU32(value.retainCount);
    writer.WriteU32(value.threadsPerGroup);
    writer.WriteU32(value.dispatchGroupsX);
    writer.WriteU32(value.recordStrideBytes);
    writer.WriteU32(value.fixedCapacityBytes);
    writer.WriteU32(value.historyBytes);
    writer.WriteU32(value.outputCapacity);
    writer.WriteU32(value.outputStrideBytes);
    writer.WriteU32(value.zeroTransitionMapsToZeroDispatch);
    return std::move(writer).Take();
}

base::Digest256 RawDeltaLoweringCandidateIdentityV1(
    const RawDeltaLoweringCandidateV1& value)
{
    return base::Sha256(SerializeRawDeltaLoweringCandidateV1(value));
}
} // namespace sge4_5::spiral7::candidate
