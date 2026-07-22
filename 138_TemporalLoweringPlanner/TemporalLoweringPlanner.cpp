#include "TemporalLoweringPlanner.h"

#include "../135_Spiral5TemporalContracts/Spiral5TemporalContracts.h"

#include <algorithm>
#include <exception>
#include <utility>

namespace sge4_5::spiral5::planner
{
base::Result<candidate::RawTemporalLoweringCandidateV1, std::string>
PlanGlobalMotorHistoryReuseCandidateV1(
    const semantic::TemporalStateSemanticV1& semanticValue,
    const semantic::UpdateScheduleV1& schedule)
{
    try
    {
        auto scheduleValidation = semantic::ValidateUpdateScheduleV1(schedule);
        if (!scheduleValidation)
            return base::Result<candidate::RawTemporalLoweringCandidateV1, std::string>::Failure(
                scheduleValidation.Error().stage + ": " + scheduleValidation.Error().message);

        const auto artifact = contract::BuildCu2GlobalMotorHistoryArtifactV1(
            semanticValue, schedule);
        const auto invocations = contract::BuildTemporalInvocationArtifactsV1(
            artifact, schedule);

        candidate::RawTemporalLoweringCandidateV1 proposal;
        proposal.semanticIdentity = semantic::TemporalStateSemanticIdentityV1(semanticValue);
        proposal.scheduleIdentity = semantic::UpdateScheduleIdentityV1(schedule);
        proposal.targetProfileIdentity = candidate::ProposedTemporalTargetProfileIdentityV1();
        proposal.observationContractIdentity =
            candidate::ProposedTemporalObservationContractIdentityV1();
        proposal.historyResourceContractIdentity =
            candidate::ProposedGlobalMotorHistoryResourceContractIdentityV1();
        proposal.retainedCompletionContractIdentity =
            candidate::ProposedRetainedHistoryCompletionContractIdentityV1();
        proposal.deviceEpochPolicyIdentity =
            candidate::ProposedTemporalDeviceEpochPolicyIdentityV1();
        proposal.spiral4IndirectArtifactIdentity = artifact.Spiral4IndirectArtifactIdentity();
        proposal.argumentProducerProgramIdentity = artifact.ArgumentProducerProgramIdentity();
        proposal.hierarchyProgramIdentity = artifact.HierarchyProgramIdentity();
        proposal.consumerProgramIdentity = artifact.ConsumerProgramIdentity();
        proposal.proposedArtifactIdentity = artifact.ArtifactIdentity();
        proposal.proposedHistoryResourceIdentity =
            candidate::ProposedGlobalMotorHistoryResourceIdentityV1(artifact);
        proposal.invocationAuthorityIdentity =
            candidate::TemporalInvocationAuthorityIdentityV1(artifact, schedule);

        proposal.timelineLength = artifact.TimelineLength();
        proposal.workCount = artifact.WorkCount();
        proposal.threadsPerGroup = artifact.ThreadsPerGroup();
        proposal.historyDepth = artifact.HistoryDepth();
        proposal.historyBytes = artifact.HistoryBytes();
        proposal.localMotorBytesPerGeneration = artifact.LocalMotorBytesPerGeneration();
        proposal.argumentStrideBytes = artifact.ArgumentStrideBytes();
        proposal.maxCommandCount = artifact.MaxCommandCount();
        proposal.argumentProducerDispatchX = artifact.ArgumentProducerDispatchX();
        proposal.hierarchyUpdateDispatchX = artifact.HierarchyUpdateDispatchX();
        proposal.hierarchyHoldDispatchX = artifact.HierarchyHoldDispatchX();
        proposal.consumerDispatchX = artifact.ConsumerDispatchX();
        proposal.updateCount = static_cast<std::uint32_t>(std::count_if(
            invocations.begin(), invocations.end(), [](const auto& invocation) {
                return invocation.event == semantic::UpdateEventV1::Update;
            }));
        proposal.holdCount = proposal.timelineLength - proposal.updateCount;
        proposal.maximumSourceGeneration = invocations.empty()
            ? 0u
            : invocations.back().sourceGeneration;
        proposal.rawCandidateIdentity =
            candidate::RawTemporalLoweringCandidateIdentityV1(proposal);

        return base::Result<candidate::RawTemporalLoweringCandidateV1, std::string>::Success(
            std::move(proposal));
    }
    catch (const std::exception& error)
    {
        return base::Result<candidate::RawTemporalLoweringCandidateV1, std::string>::Failure(
            error.what());
    }
}
}
