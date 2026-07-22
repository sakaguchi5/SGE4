#include "ActiveWorkLoweringVerifier.h"

#include "../00_Foundation/BinaryIO.h"
#include "../121_Spiral4Contracts/Spiral4Contracts.h"

#include <span>
#include <string_view>
#include <utility>

namespace sge4_5::spiral4::verification
{
namespace
{
base::Digest256 LiteralIdentity(std::string_view value)
{
    return base::Sha256(std::as_bytes(std::span(value.data(), value.size())));
}

ActiveWorkVerificationContextV1 IndependentCanonicalContext()
{
    ActiveWorkVerificationContextV1 context;
    context.targetProfileIdentity = LiteralIdentity(
        "SGE4-5.Spiral4.TargetProfile.D3D12.SingleIndirect.WarpQualified.V1");
    context.activeCountContractIdentity = LiteralIdentity(
        "SGE4-5.Spiral4.ActiveCountContract.Prefix.Nmax4096.V1");
    context.observationContractIdentity = LiteralIdentity(
        "SGE4-5.Spiral4.Observation.ActiveReference.InactiveSentinel.V1");
    context.commandSignatureContractIdentity = LiteralIdentity(
        "SGE4-5.Spiral4.CommandSignature.Dispatch.Stride12.OneCommand.V1");
    return context;
}

base::Digest256 IndependentProducerTemplateIdentity()
{
    return LiteralIdentity(
        "SGE4-5.Spiral4.Program.ArgumentProducer.CeilNfOver64.V1");
}

base::Digest256 IndependentConsumerTemplateIdentity()
{
    return LiteralIdentity(
        "SGE4-5.Spiral4.Program.DirectPgaConsumer.ActivePrefix.V1");
}

base::Result<VerifiedActiveWorkExecutionPlanV1, std::string>
DeriveExpectedPlan(
    const semantic::ActiveWorkSemanticV1& semantic,
    const ActiveWorkVerificationContextV1& context)
{
    const auto canonicalContext = IndependentCanonicalContext();
    if (!(context == canonicalContext))
    {
        return base::Result<
            VerifiedActiveWorkExecutionPlanV1,
            std::string>::Failure(
                "verification context is not canonical");
    }

    const auto semanticIdentity =
        semantic::ActiveWorkSemanticIdentityV1(semantic);
    const auto artifact =
        contract::BuildCu2SingleIndirectArtifactV1(semantic);

    VerifiedActiveWorkExecutionPlanV1 plan;
    plan.semanticIdentity = semanticIdentity;
    plan.context = canonicalContext;

    auto& operation = plan.operation;
    operation.kind =
        candidate::ActiveWorkLoweringKindV1::
            SingleExecuteIndirectDispatch;
    operation.commandKind = contract::IndirectCommandKindV1::Dispatch;
    operation.operationCode =
        contract::IndirectOperationCodeV1::ExecuteDispatch;
    operation.activeRangePolicy =
        candidate::ActiveRangePolicyV1::PrefixZeroToActiveCount;
    operation.statePolicy =
        candidate::IndirectStatePolicyV1::
            UavBarrierThenIndirectArgument;
    operation.zeroWorkPolicy =
        candidate::ZeroWorkPolicyV1::ZeroDispatchGroups;
    operation.maxWorkCount = semantic.maxWorkCount.Value();
    operation.threadsPerGroup = semantic.threadsPerGroup.Value();
    operation.workRecordStride = semantic.workRecordStride;
    operation.outputRecordStride = semantic.outputRecordStride;
    operation.argumentStrideBytes = 12;
    operation.argumentOffsetBytes = 0;
    operation.maxCommandCount = 1;
    operation.producerDispatchX = 1;
    operation.producerDispatchY = 1;
    operation.producerDispatchZ = 1;
    operation.producerProgramTemplateIdentity =
        IndependentProducerTemplateIdentity();
    operation.consumerProgramTemplateIdentity =
        IndependentConsumerTemplateIdentity();
    operation.artifactIdentity = artifact.ArtifactIdentity();
    operation.operationIdentity = base::Sha256(
        SerializeVerifiedSingleIndirectOperationV1(operation));

    plan.planIdentity = base::Sha256(
        SerializeVerifiedActiveWorkExecutionPlanV1(plan));

    return base::Result<
        VerifiedActiveWorkExecutionPlanV1,
        std::string>::Success(std::move(plan));
}

bool SameProposal(
    const candidate::RawActiveWorkLoweringCandidateV1& proposal,
    const VerifiedActiveWorkExecutionPlanV1& expected)
{
    const auto& operation = expected.operation;
    return
        proposal.semanticIdentity == expected.semanticIdentity &&
        proposal.targetProfileIdentity ==
            expected.context.targetProfileIdentity &&
        proposal.activeCountContractIdentity ==
            expected.context.activeCountContractIdentity &&
        proposal.observationContractIdentity ==
            expected.context.observationContractIdentity &&
        proposal.commandSignatureContractIdentity ==
            expected.context.commandSignatureContractIdentity &&
        proposal.producerProgramTemplateIdentity ==
            operation.producerProgramTemplateIdentity &&
        proposal.consumerProgramTemplateIdentity ==
            operation.consumerProgramTemplateIdentity &&
        proposal.proposedArtifactIdentity ==
            operation.artifactIdentity &&
        proposal.kind == operation.kind &&
        proposal.commandKind == operation.commandKind &&
        proposal.operationCode == operation.operationCode &&
        proposal.activeRangePolicy == operation.activeRangePolicy &&
        proposal.statePolicy == operation.statePolicy &&
        proposal.zeroWorkPolicy == operation.zeroWorkPolicy &&
        proposal.maxWorkCount == operation.maxWorkCount &&
        proposal.threadsPerGroup == operation.threadsPerGroup &&
        proposal.workRecordStride == operation.workRecordStride &&
        proposal.outputRecordStride ==
            operation.outputRecordStride &&
        proposal.argumentStrideBytes ==
            operation.argumentStrideBytes &&
        proposal.argumentOffsetBytes ==
            operation.argumentOffsetBytes &&
        proposal.maxCommandCount ==
            operation.maxCommandCount &&
        proposal.producerDispatchX ==
            operation.producerDispatchX &&
        proposal.producerDispatchY ==
            operation.producerDispatchY &&
        proposal.producerDispatchZ ==
            operation.producerDispatchZ;
}

base::Digest256 CertificateIdentity(
    const VerifiedActiveWorkExecutionPlanV1& plan)
{
    base::BinaryWriter writer;
    writer.WriteBytes(LiteralIdentity(
        "SGE4-5.Spiral4.ActiveWorkVerifierCertificate.V1"));
    writer.WriteBytes(plan.planIdentity);
    writer.WriteBytes(plan.semanticIdentity);
    writer.WriteBytes(plan.context.targetProfileIdentity);
    writer.WriteBytes(plan.context.activeCountContractIdentity);
    writer.WriteBytes(plan.context.observationContractIdentity);
    writer.WriteBytes(plan.context.commandSignatureContractIdentity);
    writer.WriteBytes(plan.operation.operationIdentity);
    writer.WriteBytes(plan.operation.artifactIdentity);
    return base::Sha256(std::move(writer).Take());
}
}

VerifiedActiveWorkLoweringV1::VerifiedActiveWorkLoweringV1(
    VerifiedActiveWorkExecutionPlanV1 plan,
    base::Digest256 certificateIdentity)
    : plan_(std::move(plan)),
      certificateIdentity_(certificateIdentity)
{
}

ActiveWorkVerificationContextV1
BuildCanonicalActiveWorkVerificationContextV1()
{
    return IndependentCanonicalContext();
}

std::vector<std::byte>
SerializeVerifiedSingleIndirectOperationV1(
    const VerifiedSingleIndirectOperationV1& value)
{
    base::BinaryWriter writer;
    writer.WriteBytes(LiteralIdentity(
        "SGE4-5.Spiral4.VerifiedSingleIndirectOperation.V1"));
    writer.WriteU32(static_cast<std::uint32_t>(value.kind));
    writer.WriteU32(static_cast<std::uint32_t>(value.commandKind));
    writer.WriteU32(static_cast<std::uint32_t>(value.operationCode));
    writer.WriteU32(
        static_cast<std::uint32_t>(value.activeRangePolicy));
    writer.WriteU32(static_cast<std::uint32_t>(value.statePolicy));
    writer.WriteU32(static_cast<std::uint32_t>(value.zeroWorkPolicy));
    writer.WriteU32(value.maxWorkCount);
    writer.WriteU32(value.threadsPerGroup);
    writer.WriteU32(value.workRecordStride);
    writer.WriteU32(value.outputRecordStride);
    writer.WriteU32(value.argumentStrideBytes);
    writer.WriteU32(value.argumentOffsetBytes);
    writer.WriteU32(value.maxCommandCount);
    writer.WriteU32(value.producerDispatchX);
    writer.WriteU32(value.producerDispatchY);
    writer.WriteU32(value.producerDispatchZ);
    writer.WriteBytes(value.producerProgramTemplateIdentity);
    writer.WriteBytes(value.consumerProgramTemplateIdentity);
    writer.WriteBytes(value.artifactIdentity);
    return std::move(writer).Take();
}

std::vector<std::byte>
SerializeVerifiedActiveWorkExecutionPlanV1(
    const VerifiedActiveWorkExecutionPlanV1& value)
{
    base::BinaryWriter writer;
    writer.WriteBytes(LiteralIdentity(
        "SGE4-5.Spiral4.VerifiedActiveWorkExecutionPlan.V1"));
    writer.WriteBytes(value.semanticIdentity);
    writer.WriteBytes(value.context.targetProfileIdentity);
    writer.WriteBytes(value.context.activeCountContractIdentity);
    writer.WriteBytes(value.context.observationContractIdentity);
    writer.WriteBytes(value.context.commandSignatureContractIdentity);

    const auto operationBytes =
        SerializeVerifiedSingleIndirectOperationV1(value.operation);
    writer.WriteU32(
        static_cast<std::uint32_t>(operationBytes.size()));
    writer.WriteBytes(operationBytes);
    writer.WriteBytes(value.operation.operationIdentity);
    return std::move(writer).Take();
}

std::vector<std::byte>
SerializeVerifiedActiveWorkLoweringV1(
    const VerifiedActiveWorkLoweringV1& value)
{
    base::BinaryWriter writer;
    writer.WriteBytes(LiteralIdentity(
        "SGE4-5.Spiral4.VerifiedActiveWorkLowering.V1"));
    const auto planBytes =
        SerializeVerifiedActiveWorkExecutionPlanV1(value.Plan());
    writer.WriteU32(static_cast<std::uint32_t>(planBytes.size()));
    writer.WriteBytes(planBytes);
    writer.WriteBytes(value.Plan().planIdentity);
    writer.WriteBytes(value.CertificateIdentity());
    return std::move(writer).Take();
}

base::Result<VerifiedActiveWorkLoweringV1, std::string>
VerifyActiveWorkLoweringCandidateV1(
    const semantic::ActiveWorkSemanticV1& semantic,
    const ActiveWorkVerificationContextV1& context,
    const candidate::RawActiveWorkLoweringCandidateV1& proposal)
{
    const auto rawIdentity =
        candidate::RawActiveWorkLoweringCandidateIdentityV1(proposal);
    if (proposal.rawCandidateIdentity != rawIdentity)
    {
        return base::Result<
            VerifiedActiveWorkLoweringV1,
            std::string>::Failure(
                "raw candidate identity mismatch");
    }

    auto expected = DeriveExpectedPlan(semantic, context);
    if (!expected)
    {
        return base::Result<
            VerifiedActiveWorkLoweringV1,
            std::string>::Failure(expected.Error());
    }

    if (!SameProposal(proposal, expected.Value()))
    {
        return base::Result<
            VerifiedActiveWorkLoweringV1,
            std::string>::Failure(
                "candidate execution decision mismatch");
    }

    const auto certificate = CertificateIdentity(expected.Value());
    return base::Result<
        VerifiedActiveWorkLoweringV1,
        std::string>::Success(
            VerifiedActiveWorkLoweringV1(
                std::move(expected.Value()),
                certificate));
}

base::Result<void, std::string>
ValidateVerifiedActiveWorkLoweringContextV1(
    const VerifiedActiveWorkLoweringV1& verified,
    const semantic::ActiveWorkSemanticV1& semantic,
    const ActiveWorkVerificationContextV1& context)
{
    auto expected = DeriveExpectedPlan(semantic, context);
    if (!expected)
    {
        return base::Result<void, std::string>::Failure(
            expected.Error());
    }

    const auto expectedBytes =
        SerializeVerifiedActiveWorkExecutionPlanV1(expected.Value());
    const auto actualBytes =
        SerializeVerifiedActiveWorkExecutionPlanV1(verified.Plan());

    if (expectedBytes != actualBytes ||
        expected.Value().planIdentity !=
            verified.Plan().planIdentity)
    {
        return base::Result<void, std::string>::Failure(
            "verified plan context replay mismatch");
    }

    const auto expectedCertificate =
        CertificateIdentity(expected.Value());
    if (expectedCertificate != verified.CertificateIdentity())
    {
        return base::Result<void, std::string>::Failure(
            "verified certificate context mismatch");
    }

    return base::Result<void, std::string>::Success();
}
}
