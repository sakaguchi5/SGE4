#include "DeltaLoweringVerifier.h"

#include "../00_Foundation/BinaryIO.h"
#include "../173_Spiral7DeltaContracts/Spiral7DeltaContracts.h"

#include <exception>
#include <span>
#include <string_view>
#include <utility>

namespace sge4_5::spiral7::verification
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

DeltaVerificationContextV1 IndependentCanonicalContext()
{
    DeltaVerificationContextV1 context;
    context.targetProfileIdentity = LiteralIdentity(
        "SGE4-5.Spiral7.TargetProfile.D3D12.WarpCompactDeltaHistory.V1");
    context.observationContractIdentity = LiteralIdentity(
        "SGE4-5.Spiral7.Observation.UpdateW.ClearR.HoldH.ExactTransitionWriteSet.V1");
    context.deltaResourceContractIdentity = LiteralIdentity(
        "SGE4-5.Spiral7.Resource.CompactDeltaIndexAction.Buffer32768.FixedTail.V1");
    context.previousHistoryResourceContractIdentity = LiteralIdentity(
        "SGE4-5.Spiral7.Resource.PreviousHistory.Float4x4096.ReadOnly.V1");
    context.outputHistoryResourceContractIdentity = LiteralIdentity(
        "SGE4-5.Spiral7.Resource.OutputHistory.Float4x4096.WriteOnly.V1");
    context.completionContractIdentity = LiteralIdentity(
        "SGE4-5.Spiral7.Completion.ExecuteIndirectAuthorizesExactTransitionWriteSet.V1");
    context.deviceEpochPolicyIdentity = LiteralIdentity(
        "SGE4-5.Spiral7.DeviceEpoch.HistoryAndDeltaResources.StaleRejected.V1");
    return context;
}

base::Digest256 IndependentDeltaResourceIdentity(
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

base::Digest256 IndependentPreviousHistoryResourceIdentity(
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

base::Digest256 IndependentOutputHistoryResourceIdentity(
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

base::Digest256 IndependentTransitionAuthorityIdentity(
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

base::Result<VerifiedDeltaExecutionPlanV1, std::string> DeriveExpectedPlan(
    const semantic::SparseTemporalDeltaSemanticV1& semanticValue,
    const semantic::SparseDeltaInvocationV1& invocation,
    const DeltaVerificationContextV1& context)
{
    try
    {
        if (!(context == IndependentCanonicalContext()))
        {
            return base::Result<VerifiedDeltaExecutionPlanV1, std::string>::Failure(
                "delta verification context is not canonical");
        }

        const auto artifact = contract::BuildCompactDeltaArtifactV1(semanticValue, invocation);
        const auto valid = contract::ValidateCompactDeltaArtifactForExecutionV1(
            artifact, semanticValue, invocation);
        if (!valid)
        {
            return base::Result<VerifiedDeltaExecutionPlanV1, std::string>::Failure(
                valid.Error().stage + ": " + valid.Error().message);
        }

        VerifiedDeltaExecutionPlanV1 plan;
        plan.semanticIdentity = semantic::SparseTemporalDeltaSemanticIdentityV1(semanticValue);
        plan.invocationIdentity = invocation.Identity();
        plan.previousActiveSetIdentity = invocation.PreviousActiveSet().Identity();
        plan.activeSetIdentity = invocation.ActiveSet().Identity();
        plan.modifiedSetIdentity = invocation.ModifiedSurvivorSet().Identity();
        plan.transitionSetIdentity = invocation.TransitionSet().Identity();
        plan.transitionRecordsIdentity =
            semantic::DeltaTransitionRecordsIdentityV1(invocation.TransitionRecords());
        plan.context = context;

        auto& operation = plan.operation;
        operation.invocationIndex = invocation.InvocationIndex();
        operation.universeCount = artifact.UniverseCount();
        operation.transitionCount = artifact.TransitionCount();
        operation.updateCount = artifact.UpdateCount();
        operation.clearCount = artifact.ClearCount();
        operation.retainCount = invocation.RetainSet().Cardinality().Value();
        operation.threadsPerGroup = artifact.ThreadsPerGroup();
        operation.dispatchGroupsX = artifact.DispatchGroupsX();
        operation.recordStrideBytes = artifact.RecordStrideBytes();
        operation.fixedCapacityBytes = artifact.FixedCapacityBytes();
        operation.historyBytes = semantic::WorkUniverseCountV1 *
            static_cast<std::uint32_t>(sizeof(spiral5::semantic::PointRecordV1));
        operation.outputCapacity = semanticValue.outputCapacity;
        operation.outputStrideBytes =
            static_cast<std::uint32_t>(sizeof(spiral5::semantic::PointRecordV1));
        operation.zeroTransitionMapsToZeroDispatch = 1;
        operation.spiral4IndirectIdentity = artifact.Spiral4IndirectIdentity();
        operation.spiral5TemporalIdentity = artifact.Spiral5TemporalIdentity();
        operation.spiral6SparseIdentity = artifact.Spiral6SparseIdentity();
        operation.consumerProgramIdentity = artifact.ConsumerProgramIdentity();
        operation.artifactIdentity = artifact.ArtifactIdentity();
        operation.deltaResourceIdentity = IndependentDeltaResourceIdentity(artifact);
        operation.previousHistoryResourceIdentity =
            IndependentPreviousHistoryResourceIdentity(invocation);
        operation.outputHistoryResourceIdentity =
            IndependentOutputHistoryResourceIdentity(invocation);
        operation.transitionAuthorityIdentity =
            IndependentTransitionAuthorityIdentity(semanticValue, invocation, artifact);
        operation.operationIdentity = base::Sha256(
            SerializeVerifiedCompactDeltaOperationV1(operation));
        plan.planIdentity = base::Sha256(SerializeVerifiedDeltaExecutionPlanV1(plan));
        return base::Result<VerifiedDeltaExecutionPlanV1, std::string>::Success(
            std::move(plan));
    }
    catch (const std::exception& error)
    {
        return base::Result<VerifiedDeltaExecutionPlanV1, std::string>::Failure(error.what());
    }
}

bool SameProposal(
    const candidate::RawDeltaLoweringCandidateV1& proposal,
    const VerifiedDeltaExecutionPlanV1& expected)
{
    const auto& op = expected.operation;
    return proposal.semanticIdentity == expected.semanticIdentity &&
        proposal.invocationIdentity == expected.invocationIdentity &&
        proposal.previousActiveSetIdentity == expected.previousActiveSetIdentity &&
        proposal.activeSetIdentity == expected.activeSetIdentity &&
        proposal.modifiedSetIdentity == expected.modifiedSetIdentity &&
        proposal.transitionSetIdentity == expected.transitionSetIdentity &&
        proposal.transitionRecordsIdentity == expected.transitionRecordsIdentity &&
        proposal.targetProfileIdentity == expected.context.targetProfileIdentity &&
        proposal.observationContractIdentity == expected.context.observationContractIdentity &&
        proposal.deltaResourceContractIdentity == expected.context.deltaResourceContractIdentity &&
        proposal.previousHistoryResourceContractIdentity ==
            expected.context.previousHistoryResourceContractIdentity &&
        proposal.outputHistoryResourceContractIdentity ==
            expected.context.outputHistoryResourceContractIdentity &&
        proposal.completionContractIdentity == expected.context.completionContractIdentity &&
        proposal.deviceEpochPolicyIdentity == expected.context.deviceEpochPolicyIdentity &&
        proposal.spiral4IndirectIdentity == op.spiral4IndirectIdentity &&
        proposal.spiral5TemporalIdentity == op.spiral5TemporalIdentity &&
        proposal.spiral6SparseIdentity == op.spiral6SparseIdentity &&
        proposal.consumerProgramIdentity == op.consumerProgramIdentity &&
        proposal.proposedArtifactIdentity == op.artifactIdentity &&
        proposal.proposedDeltaResourceIdentity == op.deltaResourceIdentity &&
        proposal.proposedPreviousHistoryResourceIdentity == op.previousHistoryResourceIdentity &&
        proposal.proposedOutputHistoryResourceIdentity == op.outputHistoryResourceIdentity &&
        proposal.transitionAuthorityIdentity == op.transitionAuthorityIdentity &&
        proposal.kind == op.kind &&
        proposal.representationRole == op.representationRole &&
        proposal.operationCode == op.operationCode &&
        proposal.outputHistoryPolicy == op.outputHistoryPolicy &&
        proposal.completionPolicy == op.completionPolicy &&
        proposal.deviceEpochPolicy == op.deviceEpochPolicy &&
        proposal.invocationIndex == op.invocationIndex &&
        proposal.universeCount == op.universeCount &&
        proposal.transitionCount == op.transitionCount &&
        proposal.updateCount == op.updateCount &&
        proposal.clearCount == op.clearCount &&
        proposal.retainCount == op.retainCount &&
        proposal.threadsPerGroup == op.threadsPerGroup &&
        proposal.dispatchGroupsX == op.dispatchGroupsX &&
        proposal.recordStrideBytes == op.recordStrideBytes &&
        proposal.fixedCapacityBytes == op.fixedCapacityBytes &&
        proposal.historyBytes == op.historyBytes &&
        proposal.outputCapacity == op.outputCapacity &&
        proposal.outputStrideBytes == op.outputStrideBytes &&
        proposal.zeroTransitionMapsToZeroDispatch == op.zeroTransitionMapsToZeroDispatch;
}

base::Digest256 CertificateIdentity(const VerifiedDeltaExecutionPlanV1& plan)
{
    base::BinaryWriter writer;
    WriteDigest(writer, LiteralIdentity("SGE4-5.Spiral7.VerifierCertificate.DeltaLowering.V1"));
    WriteDigest(writer, plan.planIdentity);
    WriteDigest(writer, plan.semanticIdentity);
    WriteDigest(writer, plan.invocationIdentity);
    WriteDigest(writer, plan.previousActiveSetIdentity);
    WriteDigest(writer, plan.activeSetIdentity);
    WriteDigest(writer, plan.modifiedSetIdentity);
    WriteDigest(writer, plan.transitionSetIdentity);
    WriteDigest(writer, plan.transitionRecordsIdentity);
    WriteDigest(writer, plan.context.targetProfileIdentity);
    WriteDigest(writer, plan.context.deltaResourceContractIdentity);
    WriteDigest(writer, plan.context.previousHistoryResourceContractIdentity);
    WriteDigest(writer, plan.context.outputHistoryResourceContractIdentity);
    WriteDigest(writer, plan.context.completionContractIdentity);
    WriteDigest(writer, plan.context.deviceEpochPolicyIdentity);
    WriteDigest(writer, plan.operation.operationIdentity);
    WriteDigest(writer, plan.operation.artifactIdentity);
    WriteDigest(writer, plan.operation.deltaResourceIdentity);
    WriteDigest(writer, plan.operation.previousHistoryResourceIdentity);
    WriteDigest(writer, plan.operation.outputHistoryResourceIdentity);
    WriteDigest(writer, plan.operation.transitionAuthorityIdentity);
    return base::Sha256(writer.Bytes());
}
} // namespace

VerifiedDeltaLoweringV1::VerifiedDeltaLoweringV1(
    VerifiedDeltaExecutionPlanV1 plan,
    base::Digest256 certificateIdentity)
    : plan_(std::move(plan)), certificateIdentity_(certificateIdentity)
{
}

DeltaVerificationContextV1 BuildCanonicalDeltaVerificationContextV1()
{
    return IndependentCanonicalContext();
}

base::Result<VerifiedDeltaLoweringV1, std::string>
VerifyDeltaLoweringCandidateV1(
    const semantic::SparseTemporalDeltaSemanticV1& semanticValue,
    const semantic::SparseDeltaInvocationV1& invocation,
    const DeltaVerificationContextV1& context,
    const candidate::RawDeltaLoweringCandidateV1& proposal)
{
    if (proposal.rawCandidateIdentity !=
        candidate::RawDeltaLoweringCandidateIdentityV1(proposal))
    {
        return base::Result<VerifiedDeltaLoweringV1, std::string>::Failure(
            "raw delta candidate identity mismatch");
    }

    auto expected = DeriveExpectedPlan(semanticValue, invocation, context);
    if (!expected)
        return base::Result<VerifiedDeltaLoweringV1, std::string>::Failure(expected.Error());
    if (!SameProposal(proposal, expected.Value()))
    {
        return base::Result<VerifiedDeltaLoweringV1, std::string>::Failure(
            "raw delta candidate differs from the independently derived plan");
    }

    const auto certificate = CertificateIdentity(expected.Value());
    return base::Result<VerifiedDeltaLoweringV1, std::string>::Success(
        VerifiedDeltaLoweringV1(std::move(expected).Value(), certificate));
}

base::Result<void, std::string>
ValidateVerifiedDeltaLoweringContextV1(
    const VerifiedDeltaLoweringV1& verified,
    const semantic::SparseTemporalDeltaSemanticV1& semanticValue,
    const semantic::SparseDeltaInvocationV1& invocation,
    const DeltaVerificationContextV1& context)
{
    auto expected = DeriveExpectedPlan(semanticValue, invocation, context);
    if (!expected) return base::Result<void, std::string>::Failure(expected.Error());
    if (SerializeVerifiedDeltaExecutionPlanV1(verified.Plan()) !=
            SerializeVerifiedDeltaExecutionPlanV1(expected.Value()) ||
        verified.Plan().planIdentity != expected.Value().planIdentity)
    {
        return base::Result<void, std::string>::Failure(
            "verified delta plan does not match the supplied context");
    }
    if (verified.CertificateIdentity() != CertificateIdentity(expected.Value()))
    {
        return base::Result<void, std::string>::Failure(
            "verified delta certificate identity mismatch");
    }
    return base::Result<void, std::string>::Success();
}

std::vector<std::byte> SerializeVerifiedCompactDeltaOperationV1(
    const VerifiedCompactDeltaOperationV1& value)
{
    base::BinaryWriter writer;
    WriteDigest(writer, LiteralIdentity("SGE4-5.Spiral7.VerifiedCompactDeltaOperation.V1"));
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
    for (const auto* digest : {
        &value.spiral4IndirectIdentity, &value.spiral5TemporalIdentity,
        &value.spiral6SparseIdentity, &value.consumerProgramIdentity,
        &value.artifactIdentity, &value.deltaResourceIdentity,
        &value.previousHistoryResourceIdentity, &value.outputHistoryResourceIdentity,
        &value.transitionAuthorityIdentity})
    {
        WriteDigest(writer, *digest);
    }
    return std::move(writer).Take();
}

std::vector<std::byte> SerializeVerifiedDeltaExecutionPlanV1(
    const VerifiedDeltaExecutionPlanV1& value)
{
    base::BinaryWriter writer;
    WriteDigest(writer, LiteralIdentity("SGE4-5.Spiral7.VerifiedDeltaExecutionPlan.V1"));
    for (const auto* digest : {
        &value.semanticIdentity, &value.invocationIdentity,
        &value.previousActiveSetIdentity, &value.activeSetIdentity,
        &value.modifiedSetIdentity, &value.transitionSetIdentity,
        &value.transitionRecordsIdentity,
        &value.context.targetProfileIdentity, &value.context.observationContractIdentity,
        &value.context.deltaResourceContractIdentity,
        &value.context.previousHistoryResourceContractIdentity,
        &value.context.outputHistoryResourceContractIdentity,
        &value.context.completionContractIdentity,
        &value.context.deviceEpochPolicyIdentity})
    {
        WriteDigest(writer, *digest);
    }
    const auto operation = SerializeVerifiedCompactDeltaOperationV1(value.operation);
    writer.WriteU32(static_cast<std::uint32_t>(operation.size()));
    writer.WriteBytes(operation);
    WriteDigest(writer, value.operation.operationIdentity);
    return std::move(writer).Take();
}

std::vector<std::byte> SerializeVerifiedDeltaLoweringV1(
    const VerifiedDeltaLoweringV1& value)
{
    base::BinaryWriter writer;
    WriteDigest(writer, LiteralIdentity("SGE4-5.Spiral7.VerifiedDeltaLowering.V1"));
    const auto plan = SerializeVerifiedDeltaExecutionPlanV1(value.Plan());
    writer.WriteU32(static_cast<std::uint32_t>(plan.size()));
    writer.WriteBytes(plan);
    WriteDigest(writer, value.Plan().planIdentity);
    WriteDigest(writer, value.CertificateIdentity());
    return std::move(writer).Take();
}
} // namespace sge4_5::spiral7::verification
