#include "TemporalLoweringCandidate.h"

#include "../00_Foundation/BinaryIO.h"

#include <span>
#include <string_view>
#include <utility>

namespace sge4_5::spiral5::candidate
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
}

base::Digest256 ProposedTemporalTargetProfileIdentityV1()
{
    return LiteralIdentity(
        "SGE4-5.Spiral5.TargetProfile.D3D12.GlobalMotorHistory.WarpQualified.V1");
}

base::Digest256 ProposedTemporalObservationContractIdentityV1()
{
    return LiteralIdentity(
        "SGE4-5.Spiral5.Observation.LastUpdateWins.ExactTimeline.V1");
}

base::Digest256 ProposedGlobalMotorHistoryResourceContractIdentityV1()
{
    return LiteralIdentity(
        "SGE4-5.Spiral5.HistoryResource.GlobalMotor.Buffer256.Depth1.V1");
}

base::Digest256 ProposedRetainedHistoryCompletionContractIdentityV1()
{
    return LiteralIdentity(
        "SGE4-5.Spiral5.Completion.ConsumerRetainsGlobalMotorHistory.V1");
}

base::Digest256 ProposedTemporalDeviceEpochPolicyIdentityV1()
{
    return LiteralIdentity(
        "SGE4-5.Spiral5.DeviceEpoch.HistoryBinding.StaleRejected.ReseedRequired.V1");
}

base::Digest256 ProposedGlobalMotorHistoryResourceIdentityV1(
    const contract::FrozenGlobalMotorHistoryArtifactV1& artifact)
{
    base::BinaryWriter writer;
    WriteDigest(writer, LiteralIdentity(
        "SGE4-5.Spiral5.ActualHistoryResource.GlobalMotor.V1"));
    WriteDigest(writer, artifact.ArtifactIdentity());
    WriteDigest(writer, artifact.HierarchyProgramIdentity());
    WriteDigest(writer, artifact.ConsumerProgramIdentity());
    writer.WriteU32(static_cast<std::uint32_t>(artifact.HistoryRole()));
    writer.WriteU32(artifact.HistoryDepth());
    writer.WriteU32(artifact.HistoryBytes());
    return base::Sha256(writer.Bytes());
}

base::Digest256 TemporalInvocationAuthorityIdentityV1(
    const contract::FrozenGlobalMotorHistoryArtifactV1& artifact,
    const semantic::UpdateScheduleV1& schedule)
{
    const auto invocations = contract::BuildTemporalInvocationArtifactsV1(
        artifact, schedule);
    const auto bytes = contract::SerializeTemporalInvocationArtifactsV1(invocations);
    return base::Sha256(bytes);
}

std::vector<std::byte> SerializeRawTemporalLoweringCandidateV1(
    const RawTemporalLoweringCandidateV1& value)
{
    base::BinaryWriter writer;
    WriteDigest(writer, LiteralIdentity(
        "SGE4-5.Spiral5.RawTemporalLoweringCandidate.V1"));
    WriteDigest(writer, value.semanticIdentity);
    WriteDigest(writer, value.scheduleIdentity);
    WriteDigest(writer, value.targetProfileIdentity);
    WriteDigest(writer, value.observationContractIdentity);
    WriteDigest(writer, value.historyResourceContractIdentity);
    WriteDigest(writer, value.retainedCompletionContractIdentity);
    WriteDigest(writer, value.deviceEpochPolicyIdentity);
    WriteDigest(writer, value.spiral4IndirectArtifactIdentity);
    WriteDigest(writer, value.argumentProducerProgramIdentity);
    WriteDigest(writer, value.hierarchyProgramIdentity);
    WriteDigest(writer, value.consumerProgramIdentity);
    WriteDigest(writer, value.proposedArtifactIdentity);
    WriteDigest(writer, value.proposedHistoryResourceIdentity);
    WriteDigest(writer, value.invocationAuthorityIdentity);
    writer.WriteU32(static_cast<std::uint32_t>(value.kind));
    writer.WriteU32(static_cast<std::uint32_t>(value.extensionStrategy));
    writer.WriteU32(static_cast<std::uint32_t>(value.historyRole));
    writer.WriteU32(static_cast<std::uint32_t>(value.completionPolicy));
    writer.WriteU32(static_cast<std::uint32_t>(value.generationPolicy));
    writer.WriteU32(static_cast<std::uint32_t>(value.holdPolicy));
    writer.WriteU32(value.timelineLength);
    writer.WriteU32(value.workCount);
    writer.WriteU32(value.threadsPerGroup);
    writer.WriteU32(value.historyDepth);
    writer.WriteU32(value.historyBytes);
    writer.WriteU32(value.localMotorBytesPerGeneration);
    writer.WriteU32(value.argumentStrideBytes);
    writer.WriteU32(value.maxCommandCount);
    writer.WriteU32(value.argumentProducerDispatchX);
    writer.WriteU32(value.hierarchyUpdateDispatchX);
    writer.WriteU32(value.hierarchyHoldDispatchX);
    writer.WriteU32(value.consumerDispatchX);
    writer.WriteU32(value.updateCount);
    writer.WriteU32(value.holdCount);
    writer.WriteU32(value.maximumSourceGeneration);
    return std::move(writer).Take();
}

base::Digest256 RawTemporalLoweringCandidateIdentityV1(
    const RawTemporalLoweringCandidateV1& value)
{
    const auto bytes = SerializeRawTemporalLoweringCandidateV1(value);
    return base::Sha256(bytes);
}
}
