#include "ActiveWorkCandidateFamilyPlanner.h"

#include "../127_Spiral4CandidateFamilyContracts/CandidateFamilyContracts.h"

#include <utility>

namespace sge4_5::spiral4::family_planner
{
base::Result<family_candidate::RawCandidateFamilyV1, std::string>
PlanCandidateFamilyV1(
    const semantic::ActiveWorkSemanticV1& semantic,
    family_contract::CandidateFamilyKindV1 kind,
    std::uint32_t batchSize)
{
    auto artifact = family_contract::BuildCandidateFamilyArtifactV1(
        semantic, kind, batchSize);
    if (!artifact)
    {
        return base::Result<family_candidate::RawCandidateFamilyV1, std::string>::Failure(
            artifact.Error().stage + ": " + artifact.Error().message);
    }

    family_candidate::RawCandidateFamilyV1 proposal;
    proposal.semanticIdentity = semantic::ActiveWorkSemanticIdentityV1(semantic);
    proposal.targetProfileIdentity =
        family_candidate::ProposedFamilyTargetProfileIdentityV1();
    proposal.activeCountContractIdentity =
        family_candidate::ProposedFamilyActiveCountContractIdentityV1();
    proposal.observationContractIdentity =
        family_candidate::ProposedFamilyObservationContractIdentityV1();
    proposal.commandSignatureContractIdentity =
        family_candidate::ProposedCommandSignatureContractIdentityV1(kind, batchSize);
    proposal.producerProgramTemplateIdentity =
        family_candidate::ProposedProducerProgramTemplateIdentityV1(kind, batchSize);
    proposal.consumerProgramTemplateIdentity =
        family_candidate::ProposedConsumerProgramTemplateIdentityV1();
    proposal.proposedArtifactIdentity = artifact.Value().ArtifactIdentity();
    proposal.kind = kind;
    proposal.commandSignaturePolicy = artifact.Value().CommandSignaturePolicy();
    proposal.issuancePolicy = artifact.Value().IssuancePolicy();
    proposal.batchSize = artifact.Value().BatchSize();
    proposal.maxBatchSlots = artifact.Value().MaxBatchSlots();
    proposal.maxWorkCount = artifact.Value().MaxWorkCount();
    proposal.threadsPerGroup = artifact.Value().ThreadsPerGroup();
    proposal.directDispatchX = artifact.Value().DirectDispatchX();
    proposal.argumentStrideBytes = artifact.Value().ArgumentStrideBytes();
    proposal.argumentBufferBytes = artifact.Value().ArgumentBufferBytes();
    proposal.maxCommandCount = artifact.Value().MaxCommandCount();
    proposal.producerDispatchX = artifact.Value().ProducerDispatchX();
    proposal.rawCandidateIdentity =
        family_candidate::RawCandidateFamilyIdentityV1(proposal);
    return base::Result<family_candidate::RawCandidateFamilyV1, std::string>::Success(
        std::move(proposal));
}

base::Result<std::vector<family_candidate::RawCandidateFamilyV1>, std::string>
PlanCanonicalCandidateFamilyCorpusV1(
    const semantic::ActiveWorkSemanticV1& semantic)
{
    std::vector<family_candidate::RawCandidateFamilyV1> result;
    auto add = [&](family_contract::CandidateFamilyKindV1 kind,
                   std::uint32_t batchSize) -> bool
    {
        auto proposal = PlanCandidateFamilyV1(semantic, kind, batchSize);
        if (!proposal) return false;
        result.push_back(std::move(proposal.Value()));
        return true;
    };

    if (!add(family_contract::CandidateFamilyKindV1::FixedMaximumGuarded, 0) ||
        !add(family_contract::CandidateFamilyKindV1::SingleExecuteIndirectDispatch, 0))
    {
        return base::Result<std::vector<family_candidate::RawCandidateFamilyV1>, std::string>::Failure(
            "failed to plan fixed or single candidate");
    }
    for (const auto size : family_contract::QualifiedBatchSizesV1())
    {
        if (!add(family_contract::CandidateFamilyKindV1::BatchedExecuteIndirectDispatch, size))
        {
            return base::Result<std::vector<family_candidate::RawCandidateFamilyV1>, std::string>::Failure(
                "failed to plan Batched candidate");
        }
    }
    return base::Result<std::vector<family_candidate::RawCandidateFamilyV1>, std::string>::Success(
        std::move(result));
}
}
