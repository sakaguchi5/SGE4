#include "SparseLoweringPlanner.h"

#include "../155_Spiral6SparseContracts/Spiral6SparseContracts.h"

#include <exception>
#include <utility>

namespace sge4_5::spiral6::planner
{
base::Result<candidate::RawSparseLoweringCandidateV1, std::string>
PlanCompactIndexListCandidateV1(
    const semantic::SparsePointTransformSemanticV1& semanticValue,
    const semantic::ExactSparseWorkSetV1& set)
{
    try
    {
        const auto artifact = contract::BuildCompactIndexListArtifactV1(
            semanticValue, set);
        auto validation = contract::ValidateCompactIndexListArtifactForExecutionV1(
            artifact, semanticValue, set);
        if (!validation)
            return base::Result<candidate::RawSparseLoweringCandidateV1, std::string>::Failure(
                validation.Error().stage + ": " + validation.Error().message);

        candidate::RawSparseLoweringCandidateV1 proposal;
        proposal.semanticIdentity =
            semantic::SparsePointTransformSemanticIdentityV1(semanticValue);
        proposal.sparseSetIdentity = set.Identity();
        proposal.targetProfileIdentity =
            candidate::ProposedSparseTargetProfileIdentityV1();
        proposal.observationContractIdentity =
            candidate::ProposedSparseObservationContractIdentityV1();
        proposal.indexResourceContractIdentity =
            candidate::ProposedCompactIndexResourceContractIdentityV1();
        proposal.completionContractIdentity =
            candidate::ProposedSparseCompletionContractIdentityV1();
        proposal.deviceEpochPolicyIdentity =
            candidate::ProposedSparseDeviceEpochPolicyIdentityV1();
        proposal.spiral4IndirectArtifactIdentity =
            artifact.Spiral4IndirectIdentity();
        proposal.consumerProgramIdentity = artifact.ConsumerProgramIdentity();
        proposal.proposedArtifactIdentity = artifact.ArtifactIdentity();
        proposal.proposedIndexResourceIdentity =
            candidate::ProposedCompactIndexResourceIdentityV1(artifact);
        proposal.writeSetAuthorityIdentity =
            candidate::SparseWriteSetAuthorityIdentityV1(
                semanticValue, set, artifact);

        proposal.universeCount = artifact.UniverseCount();
        proposal.activeCount = artifact.ActiveCount();
        proposal.threadsPerGroup = artifact.ThreadsPerGroup();
        proposal.dispatchGroupsX = artifact.DispatchGroupsX();
        proposal.indexStrideBytes = artifact.IndexStrideBytes();
        proposal.fixedCapacityBytes = artifact.FixedCapacityBytes();
        proposal.outputCapacity = semanticValue.outputCapacity;
        proposal.outputStrideBytes = semantic::OutputRecordStrideV1;
        proposal.unusedIndexSentinel = semantic::InvalidUniverseIndexV1;
        proposal.zeroActiveMapsToZeroDispatch = 1;
        proposal.rawCandidateIdentity =
            candidate::RawSparseLoweringCandidateIdentityV1(proposal);
        return base::Result<candidate::RawSparseLoweringCandidateV1, std::string>::Success(
            std::move(proposal));
    }
    catch (const std::exception& error)
    {
        return base::Result<candidate::RawSparseLoweringCandidateV1, std::string>::Failure(
            error.what());
    }
}
}
