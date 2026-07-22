#include "TemporalLoweringVerifier.h"

#include "../00_Foundation/BinaryIO.h"
#include "../135_Spiral5TemporalContracts/Spiral5TemporalContracts.h"

#include <algorithm>
#include <exception>
#include <span>
#include <string_view>
#include <utility>

namespace sge4_5::spiral5::verification
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

TemporalVerificationContextV1 IndependentCanonicalContext()
{
    TemporalVerificationContextV1 context;
    context.targetProfileIdentity = LiteralIdentity(
        "SGE4-5.Spiral5.TargetProfile.D3D12.GlobalMotorHistory.WarpQualified.V1");
    context.observationContractIdentity = LiteralIdentity(
        "SGE4-5.Spiral5.Observation.LastUpdateWins.ExactTimeline.V1");
    context.historyResourceContractIdentity = LiteralIdentity(
        "SGE4-5.Spiral5.HistoryResource.GlobalMotor.Buffer256.Depth1.V1");
    context.retainedCompletionContractIdentity = LiteralIdentity(
        "SGE4-5.Spiral5.Completion.ConsumerRetainsGlobalMotorHistory.V1");
    context.deviceEpochPolicyIdentity = LiteralIdentity(
        "SGE4-5.Spiral5.DeviceEpoch.HistoryBinding.StaleRejected.ReseedRequired.V1");
    return context;
}

base::Digest256 IndependentHistoryResourceIdentity(
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

base::Digest256 IndependentInvocationAuthorityIdentity(
    const contract::FrozenGlobalMotorHistoryArtifactV1& artifact,
    const semantic::UpdateScheduleV1& schedule)
{
    const auto invocations = contract::BuildTemporalInvocationArtifactsV1(
        artifact, schedule);
    return base::Sha256(
        contract::SerializeTemporalInvocationArtifactsV1(invocations));
}

base::Result<VerifiedTemporalExecutionPlanV1, std::string>
DeriveExpectedPlan(
    const semantic::TemporalStateSemanticV1& semanticValue,
    const semantic::UpdateScheduleV1& schedule,
    const TemporalVerificationContextV1& context)
{
    try
    {
        if (!(context == IndependentCanonicalContext()))
            return base::Result<VerifiedTemporalExecutionPlanV1, std::string>::Failure(
                "temporal verification context is not canonical");

        auto scheduleValidation = semantic::ValidateUpdateScheduleV1(schedule);
        if (!scheduleValidation)
            return base::Result<VerifiedTemporalExecutionPlanV1, std::string>::Failure(
                scheduleValidation.Error().stage + ": " + scheduleValidation.Error().message);

        const auto semanticIdentity =
            semantic::TemporalStateSemanticIdentityV1(semanticValue);
        const auto scheduleIdentity =
            semantic::UpdateScheduleIdentityV1(schedule);
        const auto artifact = contract::BuildCu2GlobalMotorHistoryArtifactV1(
            semanticValue, schedule);
        const auto invocations = contract::BuildTemporalInvocationArtifactsV1(
            artifact, schedule);

        VerifiedTemporalExecutionPlanV1 plan;
        plan.semanticIdentity = semanticIdentity;
        plan.scheduleIdentity = scheduleIdentity;
        plan.context = context;
        auto& operation = plan.operation;
        operation.timelineLength = artifact.TimelineLength();
        operation.workCount = artifact.WorkCount();
        operation.threadsPerGroup = artifact.ThreadsPerGroup();
        operation.historyDepth = artifact.HistoryDepth();
        operation.historyBytes = artifact.HistoryBytes();
        operation.localMotorBytesPerGeneration = artifact.LocalMotorBytesPerGeneration();
        operation.argumentStrideBytes = artifact.ArgumentStrideBytes();
        operation.maxCommandCount = artifact.MaxCommandCount();
        operation.argumentProducerDispatchX = artifact.ArgumentProducerDispatchX();
        operation.hierarchyUpdateDispatchX = artifact.HierarchyUpdateDispatchX();
        operation.hierarchyHoldDispatchX = artifact.HierarchyHoldDispatchX();
        operation.consumerDispatchX = artifact.ConsumerDispatchX();
        operation.updateCount = static_cast<std::uint32_t>(std::count_if(
            invocations.begin(), invocations.end(), [](const auto& invocation) {
                return invocation.event == semantic::UpdateEventV1::Update;
            }));
        operation.holdCount = operation.timelineLength - operation.updateCount;
        operation.maximumSourceGeneration = invocations.empty()
            ? 0u
            : invocations.back().sourceGeneration;
        operation.spiral4IndirectArtifactIdentity =
            artifact.Spiral4IndirectArtifactIdentity();
        operation.argumentProducerProgramIdentity =
            artifact.ArgumentProducerProgramIdentity();
        operation.hierarchyProgramIdentity = artifact.HierarchyProgramIdentity();
        operation.consumerProgramIdentity = artifact.ConsumerProgramIdentity();
        operation.artifactIdentity = artifact.ArtifactIdentity();
        operation.historyResourceIdentity =
            IndependentHistoryResourceIdentity(artifact);
        operation.invocationAuthorityIdentity =
            IndependentInvocationAuthorityIdentity(artifact, schedule);
        operation.operationIdentity = base::Sha256(
            SerializeVerifiedGlobalMotorHistoryOperationV1(operation));
        plan.planIdentity = base::Sha256(
            SerializeVerifiedTemporalExecutionPlanV1(plan));
        return base::Result<VerifiedTemporalExecutionPlanV1, std::string>::Success(
            std::move(plan));
    }
    catch (const std::exception& error)
    {
        return base::Result<VerifiedTemporalExecutionPlanV1, std::string>::Failure(
            error.what());
    }
}

bool SameProposal(
    const candidate::RawTemporalLoweringCandidateV1& proposal,
    const VerifiedTemporalExecutionPlanV1& expected)
{
    const auto& operation = expected.operation;
    return proposal.semanticIdentity == expected.semanticIdentity &&
        proposal.scheduleIdentity == expected.scheduleIdentity &&
        proposal.targetProfileIdentity == expected.context.targetProfileIdentity &&
        proposal.observationContractIdentity == expected.context.observationContractIdentity &&
        proposal.historyResourceContractIdentity == expected.context.historyResourceContractIdentity &&
        proposal.retainedCompletionContractIdentity == expected.context.retainedCompletionContractIdentity &&
        proposal.deviceEpochPolicyIdentity == expected.context.deviceEpochPolicyIdentity &&
        proposal.spiral4IndirectArtifactIdentity == operation.spiral4IndirectArtifactIdentity &&
        proposal.argumentProducerProgramIdentity == operation.argumentProducerProgramIdentity &&
        proposal.hierarchyProgramIdentity == operation.hierarchyProgramIdentity &&
        proposal.consumerProgramIdentity == operation.consumerProgramIdentity &&
        proposal.proposedArtifactIdentity == operation.artifactIdentity &&
        proposal.proposedHistoryResourceIdentity == operation.historyResourceIdentity &&
        proposal.invocationAuthorityIdentity == operation.invocationAuthorityIdentity &&
        proposal.kind == operation.kind &&
        proposal.extensionStrategy == operation.extensionStrategy &&
        proposal.historyRole == operation.historyRole &&
        proposal.completionPolicy == operation.completionPolicy &&
        proposal.generationPolicy == operation.generationPolicy &&
        proposal.holdPolicy == operation.holdPolicy &&
        proposal.timelineLength == operation.timelineLength &&
        proposal.workCount == operation.workCount &&
        proposal.threadsPerGroup == operation.threadsPerGroup &&
        proposal.historyDepth == operation.historyDepth &&
        proposal.historyBytes == operation.historyBytes &&
        proposal.localMotorBytesPerGeneration == operation.localMotorBytesPerGeneration &&
        proposal.argumentStrideBytes == operation.argumentStrideBytes &&
        proposal.maxCommandCount == operation.maxCommandCount &&
        proposal.argumentProducerDispatchX == operation.argumentProducerDispatchX &&
        proposal.hierarchyUpdateDispatchX == operation.hierarchyUpdateDispatchX &&
        proposal.hierarchyHoldDispatchX == operation.hierarchyHoldDispatchX &&
        proposal.consumerDispatchX == operation.consumerDispatchX &&
        proposal.updateCount == operation.updateCount &&
        proposal.holdCount == operation.holdCount &&
        proposal.maximumSourceGeneration == operation.maximumSourceGeneration;
}

base::Digest256 CertificateIdentity(
    const VerifiedTemporalExecutionPlanV1& plan)
{
    base::BinaryWriter writer;
    WriteDigest(writer, LiteralIdentity(
        "SGE4-5.Spiral5.VerifierCertificate.TemporalLowering.V1"));
    WriteDigest(writer, plan.planIdentity);
    WriteDigest(writer, plan.semanticIdentity);
    WriteDigest(writer, plan.scheduleIdentity);
    WriteDigest(writer, plan.context.targetProfileIdentity);
    WriteDigest(writer, plan.context.historyResourceContractIdentity);
    WriteDigest(writer, plan.context.retainedCompletionContractIdentity);
    WriteDigest(writer, plan.context.deviceEpochPolicyIdentity);
    WriteDigest(writer, plan.operation.operationIdentity);
    WriteDigest(writer, plan.operation.artifactIdentity);
    WriteDigest(writer, plan.operation.historyResourceIdentity);
    WriteDigest(writer, plan.operation.invocationAuthorityIdentity);
    return base::Sha256(writer.Bytes());
}
}

VerifiedTemporalLoweringV1::VerifiedTemporalLoweringV1(
    VerifiedTemporalExecutionPlanV1 plan,
    base::Digest256 certificateIdentity)
    : plan_(std::move(plan)), certificateIdentity_(certificateIdentity)
{
}

TemporalVerificationContextV1 BuildCanonicalTemporalVerificationContextV1()
{
    return IndependentCanonicalContext();
}

base::Result<VerifiedTemporalLoweringV1, std::string>
VerifyTemporalLoweringCandidateV1(
    const semantic::TemporalStateSemanticV1& semanticValue,
    const semantic::UpdateScheduleV1& schedule,
    const TemporalVerificationContextV1& context,
    const candidate::RawTemporalLoweringCandidateV1& proposal)
{
    const auto expectedIdentity =
        candidate::RawTemporalLoweringCandidateIdentityV1(proposal);
    if (proposal.rawCandidateIdentity != expectedIdentity)
        return base::Result<VerifiedTemporalLoweringV1, std::string>::Failure(
            "raw temporal candidate identity mismatch");

    auto expected = DeriveExpectedPlan(semanticValue, schedule, context);
    if (!expected)
        return base::Result<VerifiedTemporalLoweringV1, std::string>::Failure(
            expected.Error());
    if (!SameProposal(proposal, expected.Value()))
        return base::Result<VerifiedTemporalLoweringV1, std::string>::Failure(
            "raw temporal candidate differs from the independently derived plan");

    const auto certificate = CertificateIdentity(expected.Value());
    return base::Result<VerifiedTemporalLoweringV1, std::string>::Success(
        VerifiedTemporalLoweringV1(std::move(expected).Value(), certificate));
}

base::Result<void, std::string>
ValidateVerifiedTemporalLoweringContextV1(
    const VerifiedTemporalLoweringV1& verified,
    const semantic::TemporalStateSemanticV1& semanticValue,
    const semantic::UpdateScheduleV1& schedule,
    const TemporalVerificationContextV1& context)
{
    auto expected = DeriveExpectedPlan(semanticValue, schedule, context);
    if (!expected)
        return base::Result<void, std::string>::Failure(expected.Error());
    if (SerializeVerifiedTemporalExecutionPlanV1(verified.Plan()) !=
            SerializeVerifiedTemporalExecutionPlanV1(expected.Value()) ||
        verified.Plan().planIdentity != expected.Value().planIdentity)
        return base::Result<void, std::string>::Failure(
            "verified temporal plan does not match the supplied context");
    if (verified.CertificateIdentity() != CertificateIdentity(expected.Value()))
        return base::Result<void, std::string>::Failure(
            "verified temporal certificate identity mismatch");
    return base::Result<void, std::string>::Success();
}

std::vector<std::byte> SerializeVerifiedGlobalMotorHistoryOperationV1(
    const VerifiedGlobalMotorHistoryOperationV1& value)
{
    base::BinaryWriter writer;
    WriteDigest(writer, LiteralIdentity(
        "SGE4-5.Spiral5.VerifiedGlobalMotorHistoryOperation.V1"));
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
    WriteDigest(writer, value.spiral4IndirectArtifactIdentity);
    WriteDigest(writer, value.argumentProducerProgramIdentity);
    WriteDigest(writer, value.hierarchyProgramIdentity);
    WriteDigest(writer, value.consumerProgramIdentity);
    WriteDigest(writer, value.artifactIdentity);
    WriteDigest(writer, value.historyResourceIdentity);
    WriteDigest(writer, value.invocationAuthorityIdentity);
    return std::move(writer).Take();
}

std::vector<std::byte> SerializeVerifiedTemporalExecutionPlanV1(
    const VerifiedTemporalExecutionPlanV1& value)
{
    base::BinaryWriter writer;
    WriteDigest(writer, LiteralIdentity(
        "SGE4-5.Spiral5.VerifiedTemporalExecutionPlan.V1"));
    WriteDigest(writer, value.semanticIdentity);
    WriteDigest(writer, value.scheduleIdentity);
    WriteDigest(writer, value.context.targetProfileIdentity);
    WriteDigest(writer, value.context.observationContractIdentity);
    WriteDigest(writer, value.context.historyResourceContractIdentity);
    WriteDigest(writer, value.context.retainedCompletionContractIdentity);
    WriteDigest(writer, value.context.deviceEpochPolicyIdentity);
    const auto operation = SerializeVerifiedGlobalMotorHistoryOperationV1(
        value.operation);
    writer.WriteU32(static_cast<std::uint32_t>(operation.size()));
    writer.WriteBytes(operation);
    WriteDigest(writer, value.operation.operationIdentity);
    return std::move(writer).Take();
}

std::vector<std::byte> SerializeVerifiedTemporalLoweringV1(
    const VerifiedTemporalLoweringV1& value)
{
    base::BinaryWriter writer;
    WriteDigest(writer, LiteralIdentity(
        "SGE4-5.Spiral5.VerifiedTemporalLowering.V1"));
    const auto plan = SerializeVerifiedTemporalExecutionPlanV1(value.Plan());
    writer.WriteU32(static_cast<std::uint32_t>(plan.size()));
    writer.WriteBytes(plan);
    WriteDigest(writer, value.Plan().planIdentity);
    WriteDigest(writer, value.CertificateIdentity());
    return std::move(writer).Take();
}
}
