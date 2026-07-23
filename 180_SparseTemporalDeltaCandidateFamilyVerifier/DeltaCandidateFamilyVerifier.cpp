#include "DeltaCandidateFamilyVerifier.h"

#include "../00_Foundation/BinaryIO.h"

#include <exception>
#include <span>
#include <string_view>
#include <utility>

namespace sge4_5::spiral7::family_verification
{
namespace fc = family_candidate;
namespace
{
base::Digest256 Literal(std::string_view value)
{
    return base::Sha256(std::as_bytes(std::span(value.data(), value.size())));
}
void Digest(base::BinaryWriter& writer, const base::Digest256& value) { writer.WriteBytes(value); }

DeltaFamilyVerificationContextV1 IndependentCanonicalContext()
{
    DeltaFamilyVerificationContextV1 context;
    context.targetProfileIdentity = Literal(
        "SGE4-5.Spiral7.TargetProfile.D3D12.DeltaCandidateFamily.WarpQualified.V1");
    context.observationContractIdentity = Literal(
        "SGE4-5.Spiral7.Observation.DeltaFamily.PairwiseFullOutput.ExactCandidateWriteSet.V1");
    context.completionContractIdentity = Literal(
        "SGE4-5.Spiral7.Completion.DeltaFamily.ExecuteIndirectCandidateWriteSet.V1");
    context.deviceEpochPolicyIdentity = Literal(
        "SGE4-5.Spiral7.DeviceEpoch.DeltaFamily.ResourcesAndHistory.StaleRejected.V1");
    return context;
}

base::Digest256 IndependentConsumerIdentity(fc::DeltaFamilyCandidateKindV1 kind)
{
    switch (kind)
    {
    case fc::DeltaFamilyCandidateKindV1::FullActiveDenseRecompute:
        return Literal("SGE4-5.Spiral7.Program.FullActiveDenseRecompute.DirectPgaConsumer.V1");
    case fc::DeltaFamilyCandidateKindV1::CompactDeltaIndexHistoryReuse:
        return Literal("SGE4-5.Spiral7.Program.CompactDeltaIndexHistoryReuse.DirectPgaConsumer.V1");
    case fc::DeltaFamilyCandidateKindV1::AffectedBlockDeltaHistoryReuse:
        return Literal("SGE4-5.Spiral7.Program.AffectedBlockDeltaHistoryReuse.DirectPgaConsumer.V1");
    }
    return {};
}

base::Digest256 IndependentResourceContractIdentity(fc::DeltaFamilyCandidateKindV1 kind)
{
    switch (kind)
    {
    case fc::DeltaFamilyCandidateKindV1::FullActiveDenseRecompute:
        return Literal("SGE4-5.Spiral7.Resource.DenseActiveMask.Buffer512.V1");
    case fc::DeltaFamilyCandidateKindV1::CompactDeltaIndexHistoryReuse:
        return Literal("SGE4-5.Spiral7.Resource.CompactDeltaIndexAction.Buffer32768.V1");
    case fc::DeltaFamilyCandidateKindV1::AffectedBlockDeltaHistoryReuse:
        return Literal("SGE4-5.Spiral7.Resource.AffectedBlockUpdateClearMask.Buffer1536.V1");
    }
    return {};
}

base::Result<VerifiedDeltaFamilyPlanV1, std::string> DeriveExpectedPlan(
    const semantic::SparseTemporalDeltaSemanticV1& semanticValue,
    const semantic::SparseDeltaInvocationV1& invocation,
    const DeltaFamilyVerificationContextV1& context,
    fc::DeltaFamilyCandidateKindV1 kind)
{
    try
    {
        if (!(context == IndependentCanonicalContext()))
            return base::Result<VerifiedDeltaFamilyPlanV1, std::string>::Failure(
                "Delta Family verification context is not canonical");

        const auto artifact = fc::BuildDeltaFamilyArtifactV1(semanticValue, invocation, kind);
        const auto valid = fc::ValidateDeltaFamilyArtifactV1(artifact, semanticValue, invocation);
        if (!valid)
            return base::Result<VerifiedDeltaFamilyPlanV1, std::string>::Failure(valid.Error());

        VerifiedDeltaFamilyPlanV1 plan;
        plan.semanticIdentity = semantic::SparseTemporalDeltaSemanticIdentityV1(semanticValue);
        plan.invocationIdentity = invocation.Identity();
        plan.previousActiveSetIdentity = invocation.PreviousActiveSet().Identity();
        plan.activeSetIdentity = invocation.ActiveSet().Identity();
        plan.modifiedSetIdentity = invocation.ModifiedSurvivorSet().Identity();
        plan.transitionSetIdentity = invocation.TransitionSet().Identity();
        plan.transitionRecordsIdentity = semantic::DeltaTransitionRecordsIdentityV1(
            invocation.TransitionRecords());
        plan.context = context;

        auto& operation = plan.operation;
        operation.kind = kind;
        operation.representationRole = artifact.Role();
        operation.operationCode = artifact.OperationCode();
        operation.dispatchPolicy = artifact.DispatchPolicy();
        operation.outputHistoryPolicy = artifact.OutputHistoryPolicy();
        operation.legalWriteSet = artifact.LegalWriteSet();
        operation.invocationIndex = invocation.InvocationIndex();
        operation.universeCount = semantic::WorkUniverseCountV1;
        operation.activeCount = artifact.ActiveCount();
        operation.transitionCount = artifact.TransitionCount();
        operation.updateCount = artifact.UpdateCount();
        operation.clearCount = artifact.ClearCount();
        operation.retainCount = artifact.RetainCount();
        operation.affectedBlockCount = artifact.AffectedBlockCount();
        operation.threadsPerGroup = semantic::ThreadsPerGroupV1;
        operation.dispatchGroupsX = artifact.DispatchGroupsX();
        operation.payloadStrideBytes = artifact.PayloadStrideBytes();
        operation.fixedCapacityBytes = artifact.FixedCapacityBytes();
        operation.historyBytes = static_cast<std::uint32_t>(
            semantic::BuildCanonicalInactiveHistoryBytesV1().size());
        operation.outputCapacity = semanticValue.outputCapacity;
        operation.outputStrideBytes = spiral6::semantic::OutputRecordStrideV1;
        operation.zeroWorkMapsToZeroDispatch =
            kind == fc::DeltaFamilyCandidateKindV1::FullActiveDenseRecompute ? 0u : 1u;
        operation.representationResourceContractIdentity =
            IndependentResourceContractIdentity(kind);
        operation.consumerProgramIdentity = IndependentConsumerIdentity(kind);
        operation.artifactIdentity = artifact.ArtifactIdentity();
        operation.representationResourceIdentity =
            fc::DeltaFamilyRepresentationResourceIdentityV1(artifact);
        operation.writeSetAuthorityIdentity =
            fc::DeltaFamilyWriteSetAuthorityIdentityV1(semanticValue, invocation, artifact);
        operation.cu2CompactArtifactIdentity = artifact.Cu2CompactArtifactIdentity();
        operation.cu3TransitionAuthorityIdentity = artifact.Cu3TransitionAuthorityIdentity();
        operation.operationIdentity = base::Sha256(
            SerializeVerifiedDeltaFamilyOperationV1(operation));
        plan.planIdentity = base::Sha256(SerializeVerifiedDeltaFamilyPlanV1(plan));
        return base::Result<VerifiedDeltaFamilyPlanV1, std::string>::Success(std::move(plan));
    }
    catch (const std::exception& error)
    {
        return base::Result<VerifiedDeltaFamilyPlanV1, std::string>::Failure(error.what());
    }
}

bool SameProposal(
    const fc::RawDeltaFamilyCandidateV1& proposal,
    const VerifiedDeltaFamilyPlanV1& expected)
{
    const auto& operation = expected.operation;
    return proposal.semanticIdentity == expected.semanticIdentity &&
        proposal.invocationIdentity == expected.invocationIdentity &&
        proposal.previousActiveSetIdentity == expected.previousActiveSetIdentity &&
        proposal.activeSetIdentity == expected.activeSetIdentity &&
        proposal.modifiedSetIdentity == expected.modifiedSetIdentity &&
        proposal.transitionSetIdentity == expected.transitionSetIdentity &&
        proposal.transitionRecordsIdentity == expected.transitionRecordsIdentity &&
        proposal.targetProfileIdentity == expected.context.targetProfileIdentity &&
        proposal.observationContractIdentity == expected.context.observationContractIdentity &&
        proposal.representationResourceContractIdentity == operation.representationResourceContractIdentity &&
        proposal.completionContractIdentity == expected.context.completionContractIdentity &&
        proposal.deviceEpochPolicyIdentity == expected.context.deviceEpochPolicyIdentity &&
        proposal.consumerProgramIdentity == operation.consumerProgramIdentity &&
        proposal.proposedArtifactIdentity == operation.artifactIdentity &&
        proposal.proposedRepresentationResourceIdentity == operation.representationResourceIdentity &&
        proposal.writeSetAuthorityIdentity == operation.writeSetAuthorityIdentity &&
        proposal.cu2CompactArtifactIdentity == operation.cu2CompactArtifactIdentity &&
        proposal.cu3TransitionAuthorityIdentity == operation.cu3TransitionAuthorityIdentity &&
        proposal.kind == operation.kind &&
        proposal.representationRole == operation.representationRole &&
        proposal.operationCode == operation.operationCode &&
        proposal.dispatchPolicy == operation.dispatchPolicy &&
        proposal.outputHistoryPolicy == operation.outputHistoryPolicy &&
        proposal.legalWriteSet == operation.legalWriteSet &&
        proposal.completionPolicy == operation.completionPolicy &&
        proposal.deviceEpochPolicy == operation.deviceEpochPolicy &&
        proposal.invocationIndex == operation.invocationIndex &&
        proposal.universeCount == operation.universeCount &&
        proposal.activeCount == operation.activeCount &&
        proposal.transitionCount == operation.transitionCount &&
        proposal.updateCount == operation.updateCount &&
        proposal.clearCount == operation.clearCount &&
        proposal.retainCount == operation.retainCount &&
        proposal.affectedBlockCount == operation.affectedBlockCount &&
        proposal.threadsPerGroup == operation.threadsPerGroup &&
        proposal.dispatchGroupsX == operation.dispatchGroupsX &&
        proposal.payloadStrideBytes == operation.payloadStrideBytes &&
        proposal.fixedCapacityBytes == operation.fixedCapacityBytes &&
        proposal.historyBytes == operation.historyBytes &&
        proposal.outputCapacity == operation.outputCapacity &&
        proposal.outputStrideBytes == operation.outputStrideBytes &&
        proposal.zeroWorkMapsToZeroDispatch == operation.zeroWorkMapsToZeroDispatch;
}

base::Digest256 CertificateIdentity(const VerifiedDeltaFamilyPlanV1& plan)
{
    base::BinaryWriter writer;
    Digest(writer, Literal("SGE4-5.Spiral7.VerifierCertificate.DeltaCandidateFamily.V1"));
    Digest(writer, plan.planIdentity);
    Digest(writer, plan.semanticIdentity);
    Digest(writer, plan.invocationIdentity);
    Digest(writer, plan.transitionSetIdentity);
    Digest(writer, plan.context.targetProfileIdentity);
    Digest(writer, plan.context.observationContractIdentity);
    Digest(writer, plan.operation.operationIdentity);
    Digest(writer, plan.operation.artifactIdentity);
    Digest(writer, plan.operation.representationResourceIdentity);
    Digest(writer, plan.operation.writeSetAuthorityIdentity);
    Digest(writer, plan.operation.cu3TransitionAuthorityIdentity);
    return base::Sha256(writer.Bytes());
}
} // namespace

VerifiedDeltaFamilyCandidateV1::VerifiedDeltaFamilyCandidateV1(
    VerifiedDeltaFamilyPlanV1 plan,
    base::Digest256 certificateIdentity)
    : plan_(std::move(plan)), certificateIdentity_(certificateIdentity)
{
}

DeltaFamilyVerificationContextV1 BuildCanonicalDeltaFamilyVerificationContextV1()
{
    return IndependentCanonicalContext();
}

base::Result<VerifiedDeltaFamilyCandidateV1, std::string>
VerifyDeltaFamilyCandidateV1(
    const semantic::SparseTemporalDeltaSemanticV1& semanticValue,
    const semantic::SparseDeltaInvocationV1& invocation,
    const DeltaFamilyVerificationContextV1& context,
    const fc::RawDeltaFamilyCandidateV1& proposal)
{
    if (proposal.rawCandidateIdentity != fc::RawDeltaFamilyCandidateIdentityV1(proposal))
        return base::Result<VerifiedDeltaFamilyCandidateV1, std::string>::Failure(
            "raw Delta Family candidate identity mismatch");

    auto expected = DeriveExpectedPlan(semanticValue, invocation, context, proposal.kind);
    if (!expected)
        return base::Result<VerifiedDeltaFamilyCandidateV1, std::string>::Failure(expected.Error());
    if (!SameProposal(proposal, expected.Value()))
        return base::Result<VerifiedDeltaFamilyCandidateV1, std::string>::Failure(
            "raw Delta Family candidate differs from the independently derived plan");

    const auto certificate = CertificateIdentity(expected.Value());
    return base::Result<VerifiedDeltaFamilyCandidateV1, std::string>::Success(
        VerifiedDeltaFamilyCandidateV1(std::move(expected).Value(), certificate));
}

base::Result<void, std::string> ValidateVerifiedDeltaFamilyCandidateV1(
    const VerifiedDeltaFamilyCandidateV1& verified,
    const semantic::SparseTemporalDeltaSemanticV1& semanticValue,
    const semantic::SparseDeltaInvocationV1& invocation,
    const DeltaFamilyVerificationContextV1& context)
{
    auto expected = DeriveExpectedPlan(
        semanticValue, invocation, context, verified.Plan().operation.kind);
    if (!expected)
        return base::Result<void, std::string>::Failure(expected.Error());
    if (SerializeVerifiedDeltaFamilyPlanV1(verified.Plan()) !=
            SerializeVerifiedDeltaFamilyPlanV1(expected.Value()) ||
        verified.Plan().planIdentity != expected.Value().planIdentity)
        return base::Result<void, std::string>::Failure(
            "verified Delta Family plan does not match supplied context");
    if (verified.CertificateIdentity() != CertificateIdentity(expected.Value()))
        return base::Result<void, std::string>::Failure(
            "verified Delta Family certificate identity mismatch");
    return base::Result<void, std::string>::Success();
}

std::vector<std::byte> SerializeVerifiedDeltaFamilyOperationV1(
    const VerifiedDeltaFamilyOperationV1& value)
{
    base::BinaryWriter writer;
    Digest(writer, Literal("SGE4-5.Spiral7.VerifiedDeltaFamilyOperation.V1"));
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
    Digest(writer, value.representationResourceContractIdentity);
    Digest(writer, value.consumerProgramIdentity);
    Digest(writer, value.artifactIdentity);
    Digest(writer, value.representationResourceIdentity);
    Digest(writer, value.writeSetAuthorityIdentity);
    Digest(writer, value.cu2CompactArtifactIdentity);
    Digest(writer, value.cu3TransitionAuthorityIdentity);
    return std::move(writer).Take();
}

std::vector<std::byte> SerializeVerifiedDeltaFamilyPlanV1(
    const VerifiedDeltaFamilyPlanV1& value)
{
    base::BinaryWriter writer;
    Digest(writer, Literal("SGE4-5.Spiral7.VerifiedDeltaFamilyPlan.V1"));
    Digest(writer, value.semanticIdentity);
    Digest(writer, value.invocationIdentity);
    Digest(writer, value.previousActiveSetIdentity);
    Digest(writer, value.activeSetIdentity);
    Digest(writer, value.modifiedSetIdentity);
    Digest(writer, value.transitionSetIdentity);
    Digest(writer, value.transitionRecordsIdentity);
    Digest(writer, value.context.targetProfileIdentity);
    Digest(writer, value.context.observationContractIdentity);
    Digest(writer, value.context.completionContractIdentity);
    Digest(writer, value.context.deviceEpochPolicyIdentity);
    const auto operation = SerializeVerifiedDeltaFamilyOperationV1(value.operation);
    writer.WriteU32(static_cast<std::uint32_t>(operation.size()));
    writer.WriteBytes(operation);
    Digest(writer, value.operation.operationIdentity);
    return std::move(writer).Take();
}

std::vector<std::byte> SerializeVerifiedDeltaFamilyCandidateV1(
    const VerifiedDeltaFamilyCandidateV1& value)
{
    base::BinaryWriter writer;
    Digest(writer, Literal("SGE4-5.Spiral7.VerifiedDeltaFamilyCandidate.V1"));
    const auto plan = SerializeVerifiedDeltaFamilyPlanV1(value.Plan());
    writer.WriteU32(static_cast<std::uint32_t>(plan.size()));
    writer.WriteBytes(plan);
    Digest(writer, value.Plan().planIdentity);
    Digest(writer, value.CertificateIdentity());
    return std::move(writer).Take();
}
} // namespace sge4_5::spiral7::family_verification
