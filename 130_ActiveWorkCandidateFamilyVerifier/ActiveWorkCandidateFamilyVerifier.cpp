#include "ActiveWorkCandidateFamilyVerifier.h"

#include "../00_Foundation/BinaryIO.h"

#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace sge4_5::spiral4::family_verification
{
namespace
{
base::Digest256 LiteralIdentity(std::string_view value)
{
    return base::Sha256(std::as_bytes(std::span(value.data(), value.size())));
}

CandidateFamilyVerificationContextV1 IndependentContext()
{
    CandidateFamilyVerificationContextV1 context;
    context.targetProfileIdentity = LiteralIdentity(
        "SGE4-5.Spiral4.TargetProfile.D3D12.CandidateFamily.WarpQualified.V1");
    context.activeCountContractIdentity = LiteralIdentity(
        "SGE4-5.Spiral4.ActiveCountContract.Prefix.Nmax4096.V1");
    context.observationContractIdentity = LiteralIdentity(
        "SGE4-5.Spiral4.Observation.ActiveReference.InactiveSentinel.V1");
    return context;
}

base::Digest256 IndependentCommandSignatureIdentity(
    family_contract::CandidateFamilyKindV1 kind,
    std::uint32_t)
{
    using K = family_contract::CandidateFamilyKindV1;
    switch (kind)
    {
    case K::FixedMaximumGuarded:
        return LiteralIdentity(
            "SGE4-5.Spiral4.CommandSignature.None.FixedDirect.V1");
    case K::SingleExecuteIndirectDispatch:
        return LiteralIdentity(
            "SGE4-5.Spiral4.CommandSignature.Dispatch.Stride12.OneCommand.V1");
    case K::BatchedExecuteIndirectDispatch:
        return LiteralIdentity(
            "SGE4-5.Spiral4.CommandSignature.RootConstantDispatch.Stride16.V1");
    default:
        return {};
    }
}

base::Digest256 IndependentProducerIdentity(
    family_contract::CandidateFamilyKindV1 kind,
    std::uint32_t)
{
    using K = family_contract::CandidateFamilyKindV1;
    switch (kind)
    {
    case K::FixedMaximumGuarded:
        return LiteralIdentity(
            "SGE4-5.Spiral4.Program.NoArgumentProducer.FixedMaximum.V1");
    case K::SingleExecuteIndirectDispatch:
        return LiteralIdentity(
            "SGE4-5.Spiral4.Program.ArgumentProducer.CeilNfOver64.V1");
    case K::BatchedExecuteIndirectDispatch:
        return LiteralIdentity(
            "SGE4-5.Spiral4.Program.BatchArgumentProducer.FixedSlots.V1");
    default:
        return {};
    }
}

base::Digest256 IndependentConsumerIdentity()
{
    return LiteralIdentity(
        "SGE4-5.Spiral4.Program.DirectPgaConsumer.ActivePrefix.BaseWork.V1");
}

base::Result<VerifiedCandidateFamilyPlanV1, std::string>
DeriveExpected(
    const semantic::ActiveWorkSemanticV1& semantic,
    const CandidateFamilyVerificationContextV1& context,
    family_contract::CandidateFamilyKindV1 kind,
    std::uint32_t batchSize)
{
    if (!(context == IndependentContext()))
    {
        return base::Result<VerifiedCandidateFamilyPlanV1, std::string>::Failure(
            "candidate family verification context is not canonical");
    }

    auto artifact = family_contract::BuildCandidateFamilyArtifactV1(
        semantic, kind, batchSize);
    if (!artifact)
    {
        return base::Result<VerifiedCandidateFamilyPlanV1, std::string>::Failure(
            artifact.Error().stage + ": " + artifact.Error().message);
    }

    VerifiedCandidateFamilyPlanV1 plan;
    plan.semanticIdentity = semantic::ActiveWorkSemanticIdentityV1(semantic);
    plan.context = context;
    plan.kind = kind;
    plan.commandSignaturePolicy = artifact.Value().CommandSignaturePolicy();
    plan.issuancePolicy = artifact.Value().IssuancePolicy();
    plan.batchSize = artifact.Value().BatchSize();
    plan.maxBatchSlots = artifact.Value().MaxBatchSlots();
    plan.maxWorkCount = artifact.Value().MaxWorkCount();
    plan.threadsPerGroup = artifact.Value().ThreadsPerGroup();
    plan.directDispatchX = artifact.Value().DirectDispatchX();
    plan.argumentStrideBytes = artifact.Value().ArgumentStrideBytes();
    plan.argumentBufferBytes = artifact.Value().ArgumentBufferBytes();
    plan.maxCommandCount = artifact.Value().MaxCommandCount();
    plan.producerDispatchX = artifact.Value().ProducerDispatchX();
    plan.commandSignatureContractIdentity =
        IndependentCommandSignatureIdentity(kind, batchSize);
    plan.producerProgramTemplateIdentity =
        IndependentProducerIdentity(kind, batchSize);
    plan.consumerProgramTemplateIdentity = IndependentConsumerIdentity();
    plan.artifactIdentity = artifact.Value().ArtifactIdentity();
    plan.planIdentity = base::Sha256(
        SerializeVerifiedCandidateFamilyPlanV1(plan));
    return base::Result<VerifiedCandidateFamilyPlanV1, std::string>::Success(
        std::move(plan));
}

bool Same(
    const family_candidate::RawCandidateFamilyV1& proposal,
    const VerifiedCandidateFamilyPlanV1& expected)
{
    return
        proposal.semanticIdentity == expected.semanticIdentity &&
        proposal.targetProfileIdentity == expected.context.targetProfileIdentity &&
        proposal.activeCountContractIdentity == expected.context.activeCountContractIdentity &&
        proposal.observationContractIdentity == expected.context.observationContractIdentity &&
        proposal.commandSignatureContractIdentity == expected.commandSignatureContractIdentity &&
        proposal.producerProgramTemplateIdentity == expected.producerProgramTemplateIdentity &&
        proposal.consumerProgramTemplateIdentity == expected.consumerProgramTemplateIdentity &&
        proposal.proposedArtifactIdentity == expected.artifactIdentity &&
        proposal.kind == expected.kind &&
        proposal.commandSignaturePolicy == expected.commandSignaturePolicy &&
        proposal.issuancePolicy == expected.issuancePolicy &&
        proposal.batchSize == expected.batchSize &&
        proposal.maxBatchSlots == expected.maxBatchSlots &&
        proposal.maxWorkCount == expected.maxWorkCount &&
        proposal.threadsPerGroup == expected.threadsPerGroup &&
        proposal.directDispatchX == expected.directDispatchX &&
        proposal.argumentStrideBytes == expected.argumentStrideBytes &&
        proposal.argumentBufferBytes == expected.argumentBufferBytes &&
        proposal.maxCommandCount == expected.maxCommandCount &&
        proposal.producerDispatchX == expected.producerDispatchX;
}

base::Digest256 Certificate(const VerifiedCandidateFamilyPlanV1& plan)
{
    base::BinaryWriter writer;
    writer.WriteBytes(LiteralIdentity(
        "SGE4-5.Spiral4.CandidateFamilyVerifierCertificate.V1"));
    writer.WriteBytes(plan.planIdentity);
    writer.WriteBytes(plan.semanticIdentity);
    writer.WriteBytes(plan.context.targetProfileIdentity);
    writer.WriteBytes(plan.context.activeCountContractIdentity);
    writer.WriteBytes(plan.context.observationContractIdentity);
    writer.WriteBytes(plan.commandSignatureContractIdentity);
    writer.WriteBytes(plan.producerProgramTemplateIdentity);
    writer.WriteBytes(plan.consumerProgramTemplateIdentity);
    writer.WriteBytes(plan.artifactIdentity);
    return base::Sha256(std::move(writer).Take());
}
}

VerifiedCandidateFamilyV1::VerifiedCandidateFamilyV1(
    VerifiedCandidateFamilyPlanV1 plan,
    base::Digest256 certificateIdentity)
    : plan_(std::move(plan)),
      certificateIdentity_(certificateIdentity)
{
}

CandidateFamilyVerificationContextV1
BuildCanonicalCandidateFamilyVerificationContextV1()
{
    return IndependentContext();
}

std::vector<std::byte>
SerializeVerifiedCandidateFamilyPlanV1(
    const VerifiedCandidateFamilyPlanV1& value)
{
    base::BinaryWriter writer;
    writer.WriteBytes(LiteralIdentity(
        "SGE4-5.Spiral4.VerifiedCandidateFamilyPlan.V1"));
    writer.WriteBytes(value.semanticIdentity);
    writer.WriteBytes(value.context.targetProfileIdentity);
    writer.WriteBytes(value.context.activeCountContractIdentity);
    writer.WriteBytes(value.context.observationContractIdentity);
    writer.WriteU32(static_cast<std::uint32_t>(value.kind));
    writer.WriteU32(static_cast<std::uint32_t>(value.commandSignaturePolicy));
    writer.WriteU32(static_cast<std::uint32_t>(value.issuancePolicy));
    writer.WriteU32(value.batchSize);
    writer.WriteU32(value.maxBatchSlots);
    writer.WriteU32(value.maxWorkCount);
    writer.WriteU32(value.threadsPerGroup);
    writer.WriteU32(value.directDispatchX);
    writer.WriteU32(value.argumentStrideBytes);
    writer.WriteU32(value.argumentBufferBytes);
    writer.WriteU32(value.maxCommandCount);
    writer.WriteU32(value.producerDispatchX);
    writer.WriteBytes(value.commandSignatureContractIdentity);
    writer.WriteBytes(value.producerProgramTemplateIdentity);
    writer.WriteBytes(value.consumerProgramTemplateIdentity);
    writer.WriteBytes(value.artifactIdentity);
    return std::move(writer).Take();
}

std::vector<std::byte>
SerializeVerifiedCandidateFamilyV1(
    const VerifiedCandidateFamilyV1& value)
{
    base::BinaryWriter writer;
    writer.WriteBytes(LiteralIdentity(
        "SGE4-5.Spiral4.VerifiedCandidateFamily.V1"));
    const auto planBytes =
        SerializeVerifiedCandidateFamilyPlanV1(value.Plan());
    writer.WriteU32(static_cast<std::uint32_t>(planBytes.size()));
    writer.WriteBytes(planBytes);
    writer.WriteBytes(value.Plan().planIdentity);
    writer.WriteBytes(value.CertificateIdentity());
    return std::move(writer).Take();
}

base::Result<VerifiedCandidateFamilyV1, std::string>
VerifyCandidateFamilyV1(
    const semantic::ActiveWorkSemanticV1& semantic,
    const CandidateFamilyVerificationContextV1& context,
    const family_candidate::RawCandidateFamilyV1& proposal)
{
    if (proposal.rawCandidateIdentity !=
        family_candidate::RawCandidateFamilyIdentityV1(proposal))
    {
        return base::Result<VerifiedCandidateFamilyV1, std::string>::Failure(
            "candidate family Raw identity mismatch");
    }

    auto expected = DeriveExpected(
        semantic, context, proposal.kind, proposal.batchSize);
    if (!expected)
    {
        return base::Result<VerifiedCandidateFamilyV1, std::string>::Failure(
            expected.Error());
    }
    if (!Same(proposal, expected.Value()))
    {
        return base::Result<VerifiedCandidateFamilyV1, std::string>::Failure(
            "candidate family execution decision mismatch");
    }
    const auto certificate = Certificate(expected.Value());
    return base::Result<VerifiedCandidateFamilyV1, std::string>::Success(
        VerifiedCandidateFamilyV1(
            std::move(expected.Value()), certificate));
}

base::Result<void, std::string>
ValidateVerifiedCandidateFamilyContextV1(
    const VerifiedCandidateFamilyV1& verified,
    const semantic::ActiveWorkSemanticV1& semantic,
    const CandidateFamilyVerificationContextV1& context)
{
    auto expected = DeriveExpected(
        semantic, context, verified.Plan().kind, verified.Plan().batchSize);
    if (!expected)
    {
        return base::Result<void, std::string>::Failure(expected.Error());
    }
    if (SerializeVerifiedCandidateFamilyPlanV1(expected.Value()) !=
            SerializeVerifiedCandidateFamilyPlanV1(verified.Plan()) ||
        expected.Value().planIdentity != verified.Plan().planIdentity)
    {
        return base::Result<void, std::string>::Failure(
            "candidate family Verified plan replay mismatch");
    }
    if (Certificate(expected.Value()) != verified.CertificateIdentity())
    {
        return base::Result<void, std::string>::Failure(
            "candidate family certificate replay mismatch");
    }
    return base::Result<void, std::string>::Success();
}

base::Result<
    std::vector<family_contract::BatchDispatchRecordV1>,
    std::string>
DeriveVerifiedBatchDispatchRecordsV1(
    const VerifiedCandidateFamilyV1& verified,
    semantic::ActiveWorkCountV1 activeCount)
{
    if (verified.Plan().kind !=
        family_contract::CandidateFamilyKindV1::BatchedExecuteIndirectDispatch)
    {
        return base::Result<
            std::vector<family_contract::BatchDispatchRecordV1>,
            std::string>::Failure("verified Candidate is not Batched Indirect");
    }
    try
    {
        const family_contract::BatchSizeV1 batchSize(
            verified.Plan().batchSize);
        auto records =
            family_contract::BuildExpectedBatchDispatchRecordsV1(
                activeCount,
                semantic::ThreadGroupSizeV1(verified.Plan().threadsPerGroup),
                batchSize,
                verified.Plan().maxBatchSlots);
        auto valid = family_contract::ValidateBatchPartitionV1(
            activeCount,
            semantic::ThreadGroupSizeV1(verified.Plan().threadsPerGroup),
            batchSize,
            records);
        if (!valid)
        {
            return base::Result<
                std::vector<family_contract::BatchDispatchRecordV1>,
                std::string>::Failure(
                    valid.Error().stage + ": " + valid.Error().message);
        }
        return base::Result<
            std::vector<family_contract::BatchDispatchRecordV1>,
            std::string>::Success(std::move(records));
    }
    catch (const std::exception& exception)
    {
        return base::Result<
            std::vector<family_contract::BatchDispatchRecordV1>,
            std::string>::Failure(exception.what());
    }
}
}
