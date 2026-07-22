#include "SparseLoweringVerifier.h"

#include "../00_Foundation/BinaryIO.h"
#include "../155_Spiral6SparseContracts/Spiral6SparseContracts.h"

#include <exception>
#include <span>
#include <string_view>
#include <utility>

namespace sge4_5::spiral6::verification
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

SparseVerificationContextV1 IndependentCanonicalContext()
{
    SparseVerificationContextV1 context;
    context.targetProfileIdentity = LiteralIdentity(
        "SGE4-5.Spiral6.TargetProfile.D3D12.CompactIndexList.WarpQualified.V1");
    context.observationContractIdentity = LiteralIdentity(
        "SGE4-5.Spiral6.Observation.ExactWriteSet.ActiveReference.InactiveFloat4Zero.V1");
    context.indexResourceContractIdentity = LiteralIdentity(
        "SGE4-5.Spiral6.Resource.CompactSortedIndexList.Buffer16384.FixedTail.V1");
    context.completionContractIdentity = LiteralIdentity(
        "SGE4-5.Spiral6.Completion.ExecuteIndirectAuthorizesExactSparseWriteSet.V1");
    context.deviceEpochPolicyIdentity = LiteralIdentity(
        "SGE4-5.Spiral6.DeviceEpoch.SparseDerivedResource.StaleRejected.RebuildRequired.V1");
    return context;
}

base::Digest256 IndependentCompactIndexResourceIdentity(
    const contract::FrozenCompactIndexListArtifactV1& artifact)
{
    base::BinaryWriter writer;
    WriteDigest(writer, LiteralIdentity(
        "SGE4-5.Spiral6.ActualSparseResource.CompactIndexList.V1"));
    WriteDigest(writer, artifact.ArtifactIdentity());
    WriteDigest(writer, artifact.SparseSetIdentity());
    writer.WriteU32(static_cast<std::uint32_t>(artifact.RepresentationRole()));
    writer.WriteU32(artifact.ActiveCount());
    writer.WriteU32(artifact.IndexStrideBytes());
    writer.WriteU32(artifact.FixedCapacityBytes());
    return base::Sha256(writer.Bytes());
}

base::Digest256 IndependentWriteSetAuthorityIdentity(
    const semantic::SparsePointTransformSemanticV1& semanticValue,
    const semantic::ExactSparseWorkSetV1& set,
    const contract::FrozenCompactIndexListArtifactV1& artifact)
{
    base::BinaryWriter writer;
    WriteDigest(writer, LiteralIdentity(
        "SGE4-5.Spiral6.WriteSetAuthority.ExactSparseSet.V1"));
    WriteDigest(writer, semantic::SparsePointTransformSemanticIdentityV1(semanticValue));
    WriteDigest(writer, set.Identity());
    WriteDigest(writer, artifact.ArtifactIdentity());
    writer.WriteU32(static_cast<std::uint32_t>(artifact.OutputInitialization()));
    writer.WriteU32(static_cast<std::uint32_t>(artifact.WriteSetPolicy()));
    writer.WriteU32(artifact.UniverseCount());
    writer.WriteU32(artifact.ActiveCount());
    return base::Sha256(writer.Bytes());
}

base::Result<VerifiedSparseExecutionPlanV1, std::string>
DeriveExpectedPlan(
    const semantic::SparsePointTransformSemanticV1& semanticValue,
    const semantic::ExactSparseWorkSetV1& set,
    const SparseVerificationContextV1& context)
{
    try
    {
        if (!(context == IndependentCanonicalContext()))
            return base::Result<VerifiedSparseExecutionPlanV1, std::string>::Failure(
                "sparse verification context is not canonical");

        const auto semanticIdentity =
            semantic::SparsePointTransformSemanticIdentityV1(semanticValue);
        const auto setIdentity = semantic::ExactSparseWorkSetIdentityV1(set);
        const auto artifact = contract::BuildCompactIndexListArtifactV1(
            semanticValue, set);
        auto artifactValidation =
            contract::ValidateCompactIndexListArtifactForExecutionV1(
                artifact, semanticValue, set);
        if (!artifactValidation)
            return base::Result<VerifiedSparseExecutionPlanV1, std::string>::Failure(
                artifactValidation.Error().stage + ": " + artifactValidation.Error().message);

        VerifiedSparseExecutionPlanV1 plan;
        plan.semanticIdentity = semanticIdentity;
        plan.sparseSetIdentity = setIdentity;
        plan.context = context;
        auto& operation = plan.operation;
        operation.universeCount = artifact.UniverseCount();
        operation.activeCount = artifact.ActiveCount();
        operation.threadsPerGroup = artifact.ThreadsPerGroup();
        operation.dispatchGroupsX = artifact.DispatchGroupsX();
        operation.indexStrideBytes = artifact.IndexStrideBytes();
        operation.fixedCapacityBytes = artifact.FixedCapacityBytes();
        operation.outputCapacity = semanticValue.outputCapacity;
        operation.outputStrideBytes = semantic::OutputRecordStrideV1;
        operation.unusedIndexSentinel = semantic::InvalidUniverseIndexV1;
        operation.zeroActiveMapsToZeroDispatch = 1;
        operation.spiral4IndirectArtifactIdentity =
            artifact.Spiral4IndirectIdentity();
        operation.consumerProgramIdentity = artifact.ConsumerProgramIdentity();
        operation.artifactIdentity = artifact.ArtifactIdentity();
        operation.indexResourceIdentity =
            IndependentCompactIndexResourceIdentity(artifact);
        operation.writeSetAuthorityIdentity =
            IndependentWriteSetAuthorityIdentity(semanticValue, set, artifact);
        operation.operationIdentity = base::Sha256(
            SerializeVerifiedCompactIndexOperationV1(operation));
        plan.planIdentity = base::Sha256(
            SerializeVerifiedSparseExecutionPlanV1(plan));
        return base::Result<VerifiedSparseExecutionPlanV1, std::string>::Success(
            std::move(plan));
    }
    catch (const std::exception& error)
    {
        return base::Result<VerifiedSparseExecutionPlanV1, std::string>::Failure(
            error.what());
    }
}

bool SameProposal(
    const candidate::RawSparseLoweringCandidateV1& proposal,
    const VerifiedSparseExecutionPlanV1& expected)
{
    const auto& operation = expected.operation;
    return proposal.semanticIdentity == expected.semanticIdentity &&
        proposal.sparseSetIdentity == expected.sparseSetIdentity &&
        proposal.targetProfileIdentity == expected.context.targetProfileIdentity &&
        proposal.observationContractIdentity == expected.context.observationContractIdentity &&
        proposal.indexResourceContractIdentity == expected.context.indexResourceContractIdentity &&
        proposal.completionContractIdentity == expected.context.completionContractIdentity &&
        proposal.deviceEpochPolicyIdentity == expected.context.deviceEpochPolicyIdentity &&
        proposal.spiral4IndirectArtifactIdentity == operation.spiral4IndirectArtifactIdentity &&
        proposal.consumerProgramIdentity == operation.consumerProgramIdentity &&
        proposal.proposedArtifactIdentity == operation.artifactIdentity &&
        proposal.proposedIndexResourceIdentity == operation.indexResourceIdentity &&
        proposal.writeSetAuthorityIdentity == operation.writeSetAuthorityIdentity &&
        proposal.kind == operation.kind &&
        proposal.representationRole == operation.representationRole &&
        proposal.operationCode == operation.operationCode &&
        proposal.outputInitialization == operation.outputInitialization &&
        proposal.writeSetPolicy == operation.writeSetPolicy &&
        proposal.completionPolicy == operation.completionPolicy &&
        proposal.deviceEpochPolicy == operation.deviceEpochPolicy &&
        proposal.universeCount == operation.universeCount &&
        proposal.activeCount == operation.activeCount &&
        proposal.threadsPerGroup == operation.threadsPerGroup &&
        proposal.dispatchGroupsX == operation.dispatchGroupsX &&
        proposal.indexStrideBytes == operation.indexStrideBytes &&
        proposal.fixedCapacityBytes == operation.fixedCapacityBytes &&
        proposal.outputCapacity == operation.outputCapacity &&
        proposal.outputStrideBytes == operation.outputStrideBytes &&
        proposal.unusedIndexSentinel == operation.unusedIndexSentinel &&
        proposal.zeroActiveMapsToZeroDispatch == operation.zeroActiveMapsToZeroDispatch;
}

base::Digest256 CertificateIdentity(const VerifiedSparseExecutionPlanV1& plan)
{
    base::BinaryWriter writer;
    WriteDigest(writer, LiteralIdentity(
        "SGE4-5.Spiral6.VerifierCertificate.SparseLowering.V1"));
    WriteDigest(writer, plan.planIdentity);
    WriteDigest(writer, plan.semanticIdentity);
    WriteDigest(writer, plan.sparseSetIdentity);
    WriteDigest(writer, plan.context.targetProfileIdentity);
    WriteDigest(writer, plan.context.indexResourceContractIdentity);
    WriteDigest(writer, plan.context.completionContractIdentity);
    WriteDigest(writer, plan.context.deviceEpochPolicyIdentity);
    WriteDigest(writer, plan.operation.operationIdentity);
    WriteDigest(writer, plan.operation.artifactIdentity);
    WriteDigest(writer, plan.operation.indexResourceIdentity);
    WriteDigest(writer, plan.operation.writeSetAuthorityIdentity);
    return base::Sha256(writer.Bytes());
}
}

VerifiedSparseLoweringV1::VerifiedSparseLoweringV1(
    VerifiedSparseExecutionPlanV1 plan,
    base::Digest256 certificateIdentity)
    : plan_(std::move(plan)), certificateIdentity_(certificateIdentity)
{
}

SparseVerificationContextV1 BuildCanonicalSparseVerificationContextV1()
{
    return IndependentCanonicalContext();
}

base::Result<VerifiedSparseLoweringV1, std::string>
VerifySparseLoweringCandidateV1(
    const semantic::SparsePointTransformSemanticV1& semanticValue,
    const semantic::ExactSparseWorkSetV1& set,
    const SparseVerificationContextV1& context,
    const candidate::RawSparseLoweringCandidateV1& proposal)
{
    if (proposal.rawCandidateIdentity !=
        candidate::RawSparseLoweringCandidateIdentityV1(proposal))
    {
        return base::Result<VerifiedSparseLoweringV1, std::string>::Failure(
            "raw sparse candidate identity mismatch");
    }

    auto expected = DeriveExpectedPlan(semanticValue, set, context);
    if (!expected)
        return base::Result<VerifiedSparseLoweringV1, std::string>::Failure(
            expected.Error());
    if (!SameProposal(proposal, expected.Value()))
        return base::Result<VerifiedSparseLoweringV1, std::string>::Failure(
            "raw sparse candidate differs from the independently derived plan");

    const auto certificate = CertificateIdentity(expected.Value());
    return base::Result<VerifiedSparseLoweringV1, std::string>::Success(
        VerifiedSparseLoweringV1(std::move(expected).Value(), certificate));
}

base::Result<void, std::string>
ValidateVerifiedSparseLoweringContextV1(
    const VerifiedSparseLoweringV1& verified,
    const semantic::SparsePointTransformSemanticV1& semanticValue,
    const semantic::ExactSparseWorkSetV1& set,
    const SparseVerificationContextV1& context)
{
    auto expected = DeriveExpectedPlan(semanticValue, set, context);
    if (!expected)
        return base::Result<void, std::string>::Failure(expected.Error());
    if (SerializeVerifiedSparseExecutionPlanV1(verified.Plan()) !=
            SerializeVerifiedSparseExecutionPlanV1(expected.Value()) ||
        verified.Plan().planIdentity != expected.Value().planIdentity)
    {
        return base::Result<void, std::string>::Failure(
            "verified sparse plan does not match the supplied context");
    }
    if (verified.CertificateIdentity() != CertificateIdentity(expected.Value()))
        return base::Result<void, std::string>::Failure(
            "verified sparse certificate identity mismatch");
    return base::Result<void, std::string>::Success();
}

std::vector<std::byte> SerializeVerifiedCompactIndexOperationV1(
    const VerifiedCompactIndexOperationV1& value)
{
    base::BinaryWriter writer;
    WriteDigest(writer, LiteralIdentity(
        "SGE4-5.Spiral6.VerifiedCompactIndexOperation.V1"));
    writer.WriteU32(static_cast<std::uint32_t>(value.kind));
    writer.WriteU32(static_cast<std::uint32_t>(value.representationRole));
    writer.WriteU32(static_cast<std::uint32_t>(value.operationCode));
    writer.WriteU32(static_cast<std::uint32_t>(value.outputInitialization));
    writer.WriteU32(static_cast<std::uint32_t>(value.writeSetPolicy));
    writer.WriteU32(static_cast<std::uint32_t>(value.completionPolicy));
    writer.WriteU32(static_cast<std::uint32_t>(value.deviceEpochPolicy));
    writer.WriteU32(value.universeCount);
    writer.WriteU32(value.activeCount);
    writer.WriteU32(value.threadsPerGroup);
    writer.WriteU32(value.dispatchGroupsX);
    writer.WriteU32(value.indexStrideBytes);
    writer.WriteU32(value.fixedCapacityBytes);
    writer.WriteU32(value.outputCapacity);
    writer.WriteU32(value.outputStrideBytes);
    writer.WriteU32(value.unusedIndexSentinel);
    writer.WriteU32(value.zeroActiveMapsToZeroDispatch);
    WriteDigest(writer, value.spiral4IndirectArtifactIdentity);
    WriteDigest(writer, value.consumerProgramIdentity);
    WriteDigest(writer, value.artifactIdentity);
    WriteDigest(writer, value.indexResourceIdentity);
    WriteDigest(writer, value.writeSetAuthorityIdentity);
    return std::move(writer).Take();
}

std::vector<std::byte> SerializeVerifiedSparseExecutionPlanV1(
    const VerifiedSparseExecutionPlanV1& value)
{
    base::BinaryWriter writer;
    WriteDigest(writer, LiteralIdentity(
        "SGE4-5.Spiral6.VerifiedSparseExecutionPlan.V1"));
    WriteDigest(writer, value.semanticIdentity);
    WriteDigest(writer, value.sparseSetIdentity);
    WriteDigest(writer, value.context.targetProfileIdentity);
    WriteDigest(writer, value.context.observationContractIdentity);
    WriteDigest(writer, value.context.indexResourceContractIdentity);
    WriteDigest(writer, value.context.completionContractIdentity);
    WriteDigest(writer, value.context.deviceEpochPolicyIdentity);
    const auto operation = SerializeVerifiedCompactIndexOperationV1(value.operation);
    writer.WriteU32(static_cast<std::uint32_t>(operation.size()));
    writer.WriteBytes(operation);
    WriteDigest(writer, value.operation.operationIdentity);
    return std::move(writer).Take();
}

std::vector<std::byte> SerializeVerifiedSparseLoweringV1(
    const VerifiedSparseLoweringV1& value)
{
    base::BinaryWriter writer;
    WriteDigest(writer, LiteralIdentity(
        "SGE4-5.Spiral6.VerifiedSparseLowering.V1"));
    const auto plan = SerializeVerifiedSparseExecutionPlanV1(value.Plan());
    writer.WriteU32(static_cast<std::uint32_t>(plan.size()));
    writer.WriteBytes(plan);
    WriteDigest(writer, value.Plan().planIdentity);
    WriteDigest(writer, value.CertificateIdentity());
    return std::move(writer).Take();
}
}
