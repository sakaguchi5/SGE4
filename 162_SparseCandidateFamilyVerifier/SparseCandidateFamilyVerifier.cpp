#include "SparseCandidateFamilyVerifier.h"

#include "../00_Foundation/BinaryIO.h"
#include "../155_Spiral6SparseContracts/Spiral6SparseContracts.h"

#include <exception>
#include <span>
#include <string_view>
#include <utility>

namespace sge4_5::spiral6::family_verification
{
namespace fc = family_candidate;
namespace
{
base::Digest256 Literal(std::string_view value)
{
    return base::Sha256(std::as_bytes(std::span(value.data(), value.size())));
}
void Digest(base::BinaryWriter& writer, const base::Digest256& value) { writer.WriteBytes(value); }

SparseFamilyVerificationContextV1 IndependentContext()
{
    SparseFamilyVerificationContextV1 context;
    context.targetProfileIdentity = Literal("SGE4-5.Spiral6.TargetProfile.D3D12.SparseCandidateFamily.WarpQualified.V1");
    context.observationContractIdentity = Literal("SGE4-5.Spiral6.Observation.SparseFamily.ExactWriteSet.PairwiseFullOutput.V1");
    context.completionContractIdentity = Literal("SGE4-5.Spiral6.Completion.SparseFamily.ExecuteIndirectExactWriteSet.V1");
    context.deviceEpochPolicyIdentity = Literal("SGE4-5.Spiral6.DeviceEpoch.SparseFamily.DerivedResource.StaleRejected.V1");
    context.spiral4IndirectArtifactIdentity = contract::Spiral4DispatchContractIdentityV1();
    return context;
}

base::Result<VerifiedSparseFamilyPlanV1, std::string> ExpectedPlan(
    const semantic::SparsePointTransformSemanticV1& semanticValue,
    const semantic::ExactSparseWorkSetV1& set,
    const SparseFamilyVerificationContextV1& context,
    fc::SparseFamilyCandidateKindV1 kind)
{
    try
    {
        if (context != IndependentContext())
            return base::Result<VerifiedSparseFamilyPlanV1, std::string>::Failure("Sparse Family verification context is not canonical.");
        const auto artifact = fc::BuildSparseFamilyArtifactV1(semanticValue, set, kind);
        auto valid = fc::ValidateSparseFamilyArtifactV1(artifact, semanticValue, set);
        if (!valid) return base::Result<VerifiedSparseFamilyPlanV1, std::string>::Failure(valid.Error());

        VerifiedSparseFamilyPlanV1 plan;
        plan.semanticIdentity = semantic::SparsePointTransformSemanticIdentityV1(semanticValue);
        plan.sparseSetIdentity = set.Identity();
        plan.context = context;
        auto& operation = plan.operation;
        operation.kind = kind;
        operation.representationRole = artifact.Role();
        operation.operationCode = artifact.OperationCode();
        operation.dispatchPolicy = artifact.DispatchPolicy();
        operation.universeCount = semantic::WorkUniverseCountV1;
        operation.activeCount = artifact.ActiveCount();
        operation.activeBlockCount = artifact.ActiveBlockCount();
        operation.threadsPerGroup = semantic::ThreadsPerGroupV1;
        operation.dispatchGroupsX = artifact.DispatchGroupsX();
        operation.payloadStrideBytes = artifact.PayloadStrideBytes();
        operation.fixedCapacityBytes = artifact.FixedCapacityBytes();
        operation.outputCapacity = semanticValue.outputCapacity;
        operation.outputStrideBytes = semantic::OutputRecordStrideV1;
        operation.zeroActiveMapsToZeroDispatch = kind == fc::SparseFamilyCandidateKindV1::DenseMembershipMaskPredicate ? 0u : 1u;
        operation.representationResourceContractIdentity = fc::SparseFamilyRepresentationResourceContractIdentityV1(kind);
        operation.consumerProgramIdentity = fc::SparseFamilyConsumerProgramIdentityV1();
        operation.artifactIdentity = artifact.ArtifactIdentity();
        operation.representationResourceIdentity = fc::SparseFamilyRepresentationResourceIdentityV1(artifact);
        operation.writeSetAuthorityIdentity = fc::SparseFamilyWriteSetAuthorityIdentityV1(semanticValue, set, artifact);
        operation.cu2CompactArtifactIdentity = artifact.Cu2CompactArtifactIdentity();
        operation.operationIdentity = base::Sha256(SerializeVerifiedSparseFamilyOperationV1(operation));
        plan.planIdentity = base::Sha256(SerializeVerifiedSparseFamilyPlanV1(plan));
        return base::Result<VerifiedSparseFamilyPlanV1, std::string>::Success(std::move(plan));
    }
    catch (const std::exception& error)
    {
        return base::Result<VerifiedSparseFamilyPlanV1, std::string>::Failure(error.what());
    }
}

bool SameProposal(const fc::RawSparseFamilyCandidateV1& proposal, const VerifiedSparseFamilyPlanV1& expected)
{
    const auto& operation = expected.operation;
    return proposal.semanticIdentity == expected.semanticIdentity &&
        proposal.sparseSetIdentity == expected.sparseSetIdentity &&
        proposal.targetProfileIdentity == expected.context.targetProfileIdentity &&
        proposal.observationContractIdentity == expected.context.observationContractIdentity &&
        proposal.completionContractIdentity == expected.context.completionContractIdentity &&
        proposal.deviceEpochPolicyIdentity == expected.context.deviceEpochPolicyIdentity &&
        proposal.spiral4IndirectArtifactIdentity == expected.context.spiral4IndirectArtifactIdentity &&
        proposal.representationResourceContractIdentity == operation.representationResourceContractIdentity &&
        proposal.consumerProgramIdentity == operation.consumerProgramIdentity &&
        proposal.proposedArtifactIdentity == operation.artifactIdentity &&
        proposal.proposedRepresentationResourceIdentity == operation.representationResourceIdentity &&
        proposal.writeSetAuthorityIdentity == operation.writeSetAuthorityIdentity &&
        proposal.cu2CompactArtifactIdentity == operation.cu2CompactArtifactIdentity &&
        proposal.kind == operation.kind && proposal.representationRole == operation.representationRole &&
        proposal.operationCode == operation.operationCode && proposal.dispatchPolicy == operation.dispatchPolicy &&
        proposal.completionPolicy == operation.completionPolicy && proposal.deviceEpochPolicy == operation.deviceEpochPolicy &&
        proposal.universeCount == operation.universeCount && proposal.activeCount == operation.activeCount &&
        proposal.activeBlockCount == operation.activeBlockCount && proposal.threadsPerGroup == operation.threadsPerGroup &&
        proposal.dispatchGroupsX == operation.dispatchGroupsX && proposal.payloadStrideBytes == operation.payloadStrideBytes &&
        proposal.fixedCapacityBytes == operation.fixedCapacityBytes && proposal.outputCapacity == operation.outputCapacity &&
        proposal.outputStrideBytes == operation.outputStrideBytes && proposal.zeroActiveMapsToZeroDispatch == operation.zeroActiveMapsToZeroDispatch;
}

base::Digest256 Certificate(const VerifiedSparseFamilyPlanV1& plan)
{
    base::BinaryWriter writer;
    Digest(writer, Literal("SGE4-5.Spiral6.VerifierCertificate.SparseCandidateFamily.V1"));
    Digest(writer, plan.planIdentity);
    Digest(writer, plan.semanticIdentity);
    Digest(writer, plan.sparseSetIdentity);
    Digest(writer, plan.context.targetProfileIdentity);
    Digest(writer, plan.context.observationContractIdentity);
    Digest(writer, plan.context.completionContractIdentity);
    Digest(writer, plan.context.deviceEpochPolicyIdentity);
    Digest(writer, plan.context.spiral4IndirectArtifactIdentity);
    Digest(writer, plan.operation.operationIdentity);
    Digest(writer, plan.operation.artifactIdentity);
    Digest(writer, plan.operation.representationResourceIdentity);
    Digest(writer, plan.operation.writeSetAuthorityIdentity);
    return base::Sha256(writer.Bytes());
}
}

VerifiedSparseFamilyCandidateV1::VerifiedSparseFamilyCandidateV1(
    VerifiedSparseFamilyPlanV1 plan, base::Digest256 certificateIdentity)
    : plan_(std::move(plan)), certificateIdentity_(certificateIdentity)
{
}

SparseFamilyVerificationContextV1 BuildCanonicalSparseFamilyVerificationContextV1()
{
    return IndependentContext();
}

base::Result<VerifiedSparseFamilyCandidateV1, std::string> VerifySparseFamilyCandidateV1(
    const semantic::SparsePointTransformSemanticV1& semanticValue,
    const semantic::ExactSparseWorkSetV1& set,
    const SparseFamilyVerificationContextV1& context,
    const fc::RawSparseFamilyCandidateV1& proposal)
{
    if (proposal.rawCandidateIdentity != fc::RawSparseFamilyCandidateIdentityV1(proposal))
        return base::Result<VerifiedSparseFamilyCandidateV1, std::string>::Failure("raw Sparse Family Candidate identity mismatch");
    auto expected = ExpectedPlan(semanticValue, set, context, proposal.kind);
    if (!expected) return base::Result<VerifiedSparseFamilyCandidateV1, std::string>::Failure(expected.Error());
    if (!SameProposal(proposal, expected.Value()))
        return base::Result<VerifiedSparseFamilyCandidateV1, std::string>::Failure("raw Sparse Family Candidate differs from independently derived plan");
    const auto certificate = Certificate(expected.Value());
    return base::Result<VerifiedSparseFamilyCandidateV1, std::string>::Success(
        VerifiedSparseFamilyCandidateV1(std::move(expected).Value(), certificate));
}

base::Result<void, std::string> ValidateVerifiedSparseFamilyCandidateV1(
    const VerifiedSparseFamilyCandidateV1& verified,
    const semantic::SparsePointTransformSemanticV1& semanticValue,
    const semantic::ExactSparseWorkSetV1& set,
    const SparseFamilyVerificationContextV1& context)
{
    auto expected = ExpectedPlan(semanticValue, set, context, verified.Plan().operation.kind);
    if (!expected) return base::Result<void, std::string>::Failure(expected.Error());
    if (SerializeVerifiedSparseFamilyPlanV1(verified.Plan()) != SerializeVerifiedSparseFamilyPlanV1(expected.Value()) ||
        verified.Plan().planIdentity != expected.Value().planIdentity)
        return base::Result<void, std::string>::Failure("verified Sparse Family plan does not match supplied context");
    if (verified.CertificateIdentity() != Certificate(expected.Value()))
        return base::Result<void, std::string>::Failure("verified Sparse Family certificate mismatch");
    return base::Result<void, std::string>::Success();
}

std::vector<std::byte> SerializeVerifiedSparseFamilyOperationV1(
    const VerifiedSparseFamilyOperationV1& value)
{
    base::BinaryWriter writer;
    Digest(writer, Literal("SGE4-5.Spiral6.VerifiedSparseFamilyOperation.V1"));
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
    Digest(writer, value.representationResourceContractIdentity);
    Digest(writer, value.consumerProgramIdentity);
    Digest(writer, value.artifactIdentity);
    Digest(writer, value.representationResourceIdentity);
    Digest(writer, value.writeSetAuthorityIdentity);
    Digest(writer, value.cu2CompactArtifactIdentity);
    return std::move(writer).Take();
}

std::vector<std::byte> SerializeVerifiedSparseFamilyPlanV1(const VerifiedSparseFamilyPlanV1& value)
{
    base::BinaryWriter writer;
    Digest(writer, Literal("SGE4-5.Spiral6.VerifiedSparseFamilyPlan.V1"));
    Digest(writer, value.semanticIdentity);
    Digest(writer, value.sparseSetIdentity);
    Digest(writer, value.context.targetProfileIdentity);
    Digest(writer, value.context.observationContractIdentity);
    Digest(writer, value.context.completionContractIdentity);
    Digest(writer, value.context.deviceEpochPolicyIdentity);
    Digest(writer, value.context.spiral4IndirectArtifactIdentity);
    const auto operation = SerializeVerifiedSparseFamilyOperationV1(value.operation);
    writer.WriteU32(static_cast<std::uint32_t>(operation.size()));
    writer.WriteBytes(operation);
    Digest(writer, value.operation.operationIdentity);
    return std::move(writer).Take();
}

std::vector<std::byte> SerializeVerifiedSparseFamilyCandidateV1(
    const VerifiedSparseFamilyCandidateV1& value)
{
    base::BinaryWriter writer;
    Digest(writer, Literal("SGE4-5.Spiral6.VerifiedSparseFamilyCandidate.V1"));
    const auto plan = SerializeVerifiedSparseFamilyPlanV1(value.Plan());
    writer.WriteU32(static_cast<std::uint32_t>(plan.size()));
    writer.WriteBytes(plan);
    Digest(writer, value.Plan().planIdentity);
    Digest(writer, value.CertificateIdentity());
    return std::move(writer).Take();
}
}
