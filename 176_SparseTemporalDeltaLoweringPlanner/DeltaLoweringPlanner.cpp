#include "DeltaLoweringPlanner.h"

#include "../173_Spiral7DeltaContracts/Spiral7DeltaContracts.h"

#include <exception>
#include <utility>

namespace sge4_5::spiral7::planner
{
base::Result<candidate::RawDeltaLoweringCandidateV1, std::string>
PlanCompactDeltaHistoryCandidateV1(
    const semantic::SparseTemporalDeltaSemanticV1& semanticValue,
    const semantic::SparseDeltaInvocationV1& invocation)
{
    try
    {
        const auto artifact = contract::BuildCompactDeltaArtifactV1(semanticValue, invocation);
        const auto valid = contract::ValidateCompactDeltaArtifactForExecutionV1(
            artifact, semanticValue, invocation);
        if (!valid)
        {
            return base::Result<candidate::RawDeltaLoweringCandidateV1, std::string>::Failure(
                valid.Error().stage + ": " + valid.Error().message);
        }

        candidate::RawDeltaLoweringCandidateV1 proposal;
        proposal.semanticIdentity = semantic::SparseTemporalDeltaSemanticIdentityV1(semanticValue);
        proposal.invocationIdentity = invocation.Identity();
        proposal.previousActiveSetIdentity = invocation.PreviousActiveSet().Identity();
        proposal.activeSetIdentity = invocation.ActiveSet().Identity();
        proposal.modifiedSetIdentity = invocation.ModifiedSurvivorSet().Identity();
        proposal.transitionSetIdentity = invocation.TransitionSet().Identity();
        proposal.transitionRecordsIdentity =
            semantic::DeltaTransitionRecordsIdentityV1(invocation.TransitionRecords());
        proposal.targetProfileIdentity = candidate::ProposedDeltaTargetProfileIdentityV1();
        proposal.observationContractIdentity =
            candidate::ProposedDeltaObservationContractIdentityV1();
        proposal.deltaResourceContractIdentity =
            candidate::ProposedDeltaRecordResourceContractIdentityV1();
        proposal.previousHistoryResourceContractIdentity =
            candidate::ProposedPreviousHistoryResourceContractIdentityV1();
        proposal.outputHistoryResourceContractIdentity =
            candidate::ProposedOutputHistoryResourceContractIdentityV1();
        proposal.completionContractIdentity =
            candidate::ProposedDeltaCompletionContractIdentityV1();
        proposal.deviceEpochPolicyIdentity =
            candidate::ProposedDeltaDeviceEpochPolicyIdentityV1();
        proposal.spiral4IndirectIdentity = artifact.Spiral4IndirectIdentity();
        proposal.spiral5TemporalIdentity = artifact.Spiral5TemporalIdentity();
        proposal.spiral6SparseIdentity = artifact.Spiral6SparseIdentity();
        proposal.consumerProgramIdentity = artifact.ConsumerProgramIdentity();
        proposal.proposedArtifactIdentity = artifact.ArtifactIdentity();
        proposal.proposedDeltaResourceIdentity =
            candidate::ProposedDeltaRecordResourceIdentityV1(artifact);
        proposal.proposedPreviousHistoryResourceIdentity =
            candidate::ProposedPreviousHistoryResourceIdentityV1(invocation);
        proposal.proposedOutputHistoryResourceIdentity =
            candidate::ProposedOutputHistoryResourceIdentityV1(invocation);
        proposal.transitionAuthorityIdentity =
            candidate::DeltaTransitionAuthorityIdentityV1(semanticValue, invocation, artifact);

        proposal.invocationIndex = invocation.InvocationIndex();
        proposal.universeCount = artifact.UniverseCount();
        proposal.transitionCount = artifact.TransitionCount();
        proposal.updateCount = artifact.UpdateCount();
        proposal.clearCount = artifact.ClearCount();
        proposal.retainCount = invocation.RetainSet().Cardinality().Value();
        proposal.threadsPerGroup = artifact.ThreadsPerGroup();
        proposal.dispatchGroupsX = artifact.DispatchGroupsX();
        proposal.recordStrideBytes = artifact.RecordStrideBytes();
        proposal.fixedCapacityBytes = artifact.FixedCapacityBytes();
        proposal.historyBytes = semantic::WorkUniverseCountV1 *
            static_cast<std::uint32_t>(sizeof(spiral5::semantic::PointRecordV1));
        proposal.outputCapacity = semanticValue.outputCapacity;
        proposal.outputStrideBytes = static_cast<std::uint32_t>(
            sizeof(spiral5::semantic::PointRecordV1));
        proposal.zeroTransitionMapsToZeroDispatch = 1;
        proposal.rawCandidateIdentity =
            candidate::RawDeltaLoweringCandidateIdentityV1(proposal);

        return base::Result<candidate::RawDeltaLoweringCandidateV1, std::string>::Success(
            std::move(proposal));
    }
    catch (const std::exception& error)
    {
        return base::Result<candidate::RawDeltaLoweringCandidateV1, std::string>::Failure(
            error.what());
    }
}
} // namespace sge4_5::spiral7::planner
