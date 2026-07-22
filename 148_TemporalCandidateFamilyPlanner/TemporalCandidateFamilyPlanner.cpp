#include "TemporalCandidateFamilyPlanner.h"

#include "../135_Spiral5TemporalContracts/Spiral5TemporalContracts.h"

#include <algorithm>
#include <utility>

namespace sge4_5::spiral5::family_planner
{
namespace fc = family_candidate;
namespace
{
std::uint32_t Updates(const semantic::UpdateScheduleV1& schedule)
{
    return static_cast<std::uint32_t>(std::count_if(
        schedule.invocations.begin(), schedule.invocations.end(),
        [](const auto& value) { return value.event == semantic::UpdateEventV1::Update; }));
}
}

base::Result<fc::RawTemporalCandidateV1, std::string>
PlanTemporalCandidateV1(
    const semantic::TemporalStateSemanticV1& semanticValue,
    const semantic::UpdateScheduleV1& schedule,
    fc::TemporalCandidateKindV1 kind)
{
    auto scheduleValid = semantic::ValidateUpdateScheduleV1(schedule);
    if (!scheduleValid)
        return base::Result<fc::RawTemporalCandidateV1, std::string>::Failure(
            scheduleValid.Error().message);

    fc::RawTemporalCandidateV1 value;
    value.semanticIdentity = semantic::TemporalStateSemanticIdentityV1(semanticValue);
    value.scheduleIdentity = semantic::UpdateScheduleIdentityV1(schedule);
    value.targetProfileIdentity = fc::TemporalFamilyTargetProfileIdentityV1();
    value.observationContractIdentity = fc::TemporalFamilyObservationContractIdentityV1();
    value.completionContractIdentity = fc::TemporalFamilyCompletionContractIdentityV1(kind);
    value.deviceEpochPolicyIdentity = fc::TemporalFamilyDeviceEpochPolicyIdentityV1();
    value.argumentProducerProgramIdentity = fc::TemporalFamilyArgumentProducerProgramIdentityV1();
    value.hierarchyProgramIdentity = fc::TemporalFamilyHierarchyProgramIdentityV1();
    value.consumerProgramIdentity = fc::TemporalFamilyConsumerProgramIdentityV1();
    value.proposedArtifactIdentity = fc::TemporalFamilyArtifactIdentityV1(semanticValue, schedule, kind);
    value.proposedHistoryResourceIdentity = fc::TemporalFamilyHistoryResourceIdentityV1(semanticValue, schedule, kind);
    value.invocationAuthorityIdentity = fc::TemporalFamilyInvocationAuthorityIdentityV1(semanticValue, schedule, kind);
    value.kind = kind;
    value.timelineLength = semanticValue.timelineLength;
    value.workCount = semanticValue.workCount;
    value.threadsPerGroup = semanticValue.threadsPerGroup;
    value.updateCount = Updates(schedule);
    value.holdCount = value.timelineLength - value.updateCount;
    value.maximumSourceGeneration = schedule.invocations.back().sourceGeneration;
    value.argumentStrideBytes = 12;

    const auto cu2 = contract::BuildCu2GlobalMotorHistoryArtifactV1(semanticValue, schedule);
    switch (kind)
    {
    case fc::TemporalCandidateKindV1::EveryInvocationRecompute:
        value.historyRole = fc::TemporalFamilyHistoryRoleV1::None;
        value.issuancePolicy = fc::TemporalIssuancePolicyV1::DirectEveryInvocation;
        value.completionPolicy = fc::TemporalCompletionPolicyV1::EveryInvocationOutputCompletion;
        value.holdPolicy = fc::TemporalHoldPolicyV1::RecomputeUsingLatestSource;
        value.historyDepth = 0;
        value.historyBytes = 0;
        value.argumentBufferBytes = 0;
        value.maxCommandCount = 0;
        value.argumentProducerDispatchX = 0;
        value.hierarchyUpdateDispatchX = 1;
        value.hierarchyHoldDispatchX = 1;
        value.consumerUpdateDispatchX = 64;
        value.consumerHoldDispatchX = 64;
        value.spiral4IndirectArtifactIdentity = {};
        value.argumentProducerProgramIdentity = {};
        break;
    case fc::TemporalCandidateKindV1::GlobalMotorHistoryReuse:
        value.historyRole = fc::TemporalFamilyHistoryRoleV1::GlobalMotorHistory;
        value.issuancePolicy = fc::TemporalIssuancePolicyV1::UpdateIndirectHierarchy;
        value.completionPolicy = fc::TemporalCompletionPolicyV1::ConsumerCompletionRetainsGlobalMotor;
        value.holdPolicy = fc::TemporalHoldPolicyV1::ZeroHierarchyReuseGlobalMotor;
        value.historyDepth = 1;
        value.historyBytes = semantic::BoneCountV1 * sizeof(semantic::PgaMotorRecordV1);
        value.argumentBufferBytes = 12;
        value.maxCommandCount = 1;
        value.argumentProducerDispatchX = 1;
        value.hierarchyUpdateDispatchX = 1;
        value.hierarchyHoldDispatchX = 0;
        value.consumerUpdateDispatchX = 64;
        value.consumerHoldDispatchX = 64;
        value.spiral4IndirectArtifactIdentity = cu2.Spiral4IndirectArtifactIdentity();
        break;
    case fc::TemporalCandidateKindV1::FinalOutputHistoryReuse:
        value.historyRole = fc::TemporalFamilyHistoryRoleV1::FinalOutputHistory;
        value.issuancePolicy = fc::TemporalIssuancePolicyV1::UpdateIndirectHierarchyAndConsumer;
        value.completionPolicy = fc::TemporalCompletionPolicyV1::OutputCompletionRetainsFinalOutput;
        value.holdPolicy = fc::TemporalHoldPolicyV1::ZeroHierarchyAndConsumerRetainOutput;
        value.historyDepth = 1;
        value.historyBytes = semantic::WorkCountV1 * sizeof(semantic::PointRecordV1);
        value.argumentBufferBytes = 24;
        value.maxCommandCount = 2;
        value.argumentProducerDispatchX = 1;
        value.hierarchyUpdateDispatchX = 1;
        value.hierarchyHoldDispatchX = 0;
        value.consumerUpdateDispatchX = 64;
        value.consumerHoldDispatchX = 0;
        value.spiral4IndirectArtifactIdentity = cu2.Spiral4IndirectArtifactIdentity();
        break;
    default:
        return base::Result<fc::RawTemporalCandidateV1, std::string>::Failure(
            "Unknown Temporal Candidate kind.");
    }

    value.rawCandidateIdentity = fc::RawTemporalCandidateIdentityV1(value);
    return base::Result<fc::RawTemporalCandidateV1, std::string>::Success(std::move(value));
}

base::Result<std::vector<fc::RawTemporalCandidateV1>, std::string>
PlanCanonicalTemporalCandidateFamilyV1(
    const semantic::TemporalStateSemanticV1& semanticValue,
    const semantic::UpdateScheduleV1& schedule)
{
    std::vector<fc::RawTemporalCandidateV1> result;
    for (const auto kind : {
        fc::TemporalCandidateKindV1::EveryInvocationRecompute,
        fc::TemporalCandidateKindV1::GlobalMotorHistoryReuse,
        fc::TemporalCandidateKindV1::FinalOutputHistoryReuse})
    {
        auto planned = PlanTemporalCandidateV1(semanticValue, schedule, kind);
        if (!planned)
            return base::Result<std::vector<fc::RawTemporalCandidateV1>, std::string>::Failure(planned.Error());
        result.push_back(std::move(planned).Value());
    }
    return base::Result<std::vector<fc::RawTemporalCandidateV1>, std::string>::Success(std::move(result));
}
}
