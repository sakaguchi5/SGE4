#include "ActiveWorkLoweringPlanner.h"

#include "../121_Spiral4Contracts/Spiral4Contracts.h"

#include <exception>
#include <string>
#include <utility>

namespace sge4_5::spiral4::planner
{
base::Result<candidate::RawActiveWorkLoweringCandidateV1, std::string>
PlanSingleExecuteIndirectCandidateV1(
    const semantic::ActiveWorkSemanticV1& semantic)
{
    try
    {
        const auto artifact =
            contract::BuildCu2SingleIndirectArtifactV1(semantic);

        candidate::RawActiveWorkLoweringCandidateV1 proposal;
        proposal.semanticIdentity =
            semantic::ActiveWorkSemanticIdentityV1(semantic);
        proposal.targetProfileIdentity =
            candidate::ProposedTargetProfileIdentityV1();
        proposal.activeCountContractIdentity =
            candidate::ProposedActiveCountContractIdentityV1();
        proposal.observationContractIdentity =
            candidate::ProposedObservationContractIdentityV1();
        proposal.commandSignatureContractIdentity =
            candidate::ProposedCommandSignatureContractIdentityV1();
        proposal.producerProgramTemplateIdentity =
            candidate::ProposedProducerProgramTemplateIdentityV1();
        proposal.consumerProgramTemplateIdentity =
            candidate::ProposedConsumerProgramTemplateIdentityV1();
        proposal.proposedArtifactIdentity = artifact.ArtifactIdentity();

        proposal.kind =
            candidate::ActiveWorkLoweringKindV1::
                SingleExecuteIndirectDispatch;
        proposal.commandKind = artifact.CommandKind();
        proposal.operationCode = artifact.OperationCode();
        proposal.activeRangePolicy =
            candidate::ActiveRangePolicyV1::PrefixZeroToActiveCount;
        proposal.statePolicy =
            candidate::IndirectStatePolicyV1::
                UavBarrierThenIndirectArgument;
        proposal.zeroWorkPolicy =
            candidate::ZeroWorkPolicyV1::ZeroDispatchGroups;

        proposal.maxWorkCount = semantic.maxWorkCount.Value();
        proposal.threadsPerGroup = semantic.threadsPerGroup.Value();
        proposal.workRecordStride = semantic.workRecordStride;
        proposal.outputRecordStride = semantic.outputRecordStride;
        proposal.argumentStrideBytes = artifact.ArgumentStrideBytes();
        proposal.argumentOffsetBytes = artifact.ArgumentOffsetBytes();
        proposal.maxCommandCount = artifact.MaxCommandCount();
        proposal.producerDispatchX = artifact.ProducerDispatchX();
        proposal.producerDispatchY = artifact.ProducerDispatchY();
        proposal.producerDispatchZ = artifact.ProducerDispatchZ();
        proposal.rawCandidateIdentity =
            candidate::RawActiveWorkLoweringCandidateIdentityV1(
                proposal);

        return base::Result<
            candidate::RawActiveWorkLoweringCandidateV1,
            std::string>::Success(std::move(proposal));
    }
    catch (const std::exception& error)
    {
        return base::Result<
            candidate::RawActiveWorkLoweringCandidateV1,
            std::string>::Failure(error.what());
    }
}
}
