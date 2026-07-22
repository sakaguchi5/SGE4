#include "TemporalCandidateFamilyVerifier.h"

#include "../00_Foundation/BinaryIO.h"
#include "../135_Spiral5TemporalContracts/Spiral5TemporalContracts.h"

#include <algorithm>
#include <span>
#include <string_view>
#include <utility>

namespace sge4_5::spiral5::family_verification
{
namespace fc = family_candidate;
namespace
{
base::Digest256 Literal(std::string_view value)
{
    return base::Sha256(std::as_bytes(std::span(value.data(), value.size())));
}
void Digest(base::BinaryWriter& writer, const base::Digest256& value) { writer.WriteBytes(value); }
std::uint32_t Updates(const semantic::UpdateScheduleV1& schedule)
{
    return static_cast<std::uint32_t>(std::count_if(
        schedule.invocations.begin(), schedule.invocations.end(),
        [](const auto& value) { return value.event == semantic::UpdateEventV1::Update; }));
}

fc::RawTemporalCandidateV1 Expected(
    const semantic::TemporalStateSemanticV1& semanticValue,
    const semantic::UpdateScheduleV1& schedule,
    fc::TemporalCandidateKindV1 kind)
{
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
    value.argumentStrideBytes = 12;
    value.updateCount = Updates(schedule);
    value.holdCount = value.timelineLength - value.updateCount;
    value.maximumSourceGeneration = schedule.invocations.back().sourceGeneration;
    const auto cu2 = contract::BuildCu2GlobalMotorHistoryArtifactV1(semanticValue, schedule);

    switch (kind)
    {
    case fc::TemporalCandidateKindV1::EveryInvocationRecompute:
        value.historyRole = fc::TemporalFamilyHistoryRoleV1::None;
        value.issuancePolicy = fc::TemporalIssuancePolicyV1::DirectEveryInvocation;
        value.completionPolicy = fc::TemporalCompletionPolicyV1::EveryInvocationOutputCompletion;
        value.holdPolicy = fc::TemporalHoldPolicyV1::RecomputeUsingLatestSource;
        value.hierarchyUpdateDispatchX = 1; value.hierarchyHoldDispatchX = 1;
        value.consumerUpdateDispatchX = 64; value.consumerHoldDispatchX = 64;
        value.argumentProducerProgramIdentity = {};
        break;
    case fc::TemporalCandidateKindV1::GlobalMotorHistoryReuse:
        value.historyRole = fc::TemporalFamilyHistoryRoleV1::GlobalMotorHistory;
        value.issuancePolicy = fc::TemporalIssuancePolicyV1::UpdateIndirectHierarchy;
        value.completionPolicy = fc::TemporalCompletionPolicyV1::ConsumerCompletionRetainsGlobalMotor;
        value.holdPolicy = fc::TemporalHoldPolicyV1::ZeroHierarchyReuseGlobalMotor;
        value.historyDepth = 1;
        value.historyBytes = semantic::BoneCountV1 * sizeof(semantic::PgaMotorRecordV1);
        value.argumentBufferBytes = 12; value.maxCommandCount = 1; value.argumentProducerDispatchX = 1;
        value.hierarchyUpdateDispatchX = 1; value.hierarchyHoldDispatchX = 0;
        value.consumerUpdateDispatchX = 64; value.consumerHoldDispatchX = 64;
        value.spiral4IndirectArtifactIdentity = cu2.Spiral4IndirectArtifactIdentity();
        break;
    case fc::TemporalCandidateKindV1::FinalOutputHistoryReuse:
        value.historyRole = fc::TemporalFamilyHistoryRoleV1::FinalOutputHistory;
        value.issuancePolicy = fc::TemporalIssuancePolicyV1::UpdateIndirectHierarchyAndConsumer;
        value.completionPolicy = fc::TemporalCompletionPolicyV1::OutputCompletionRetainsFinalOutput;
        value.holdPolicy = fc::TemporalHoldPolicyV1::ZeroHierarchyAndConsumerRetainOutput;
        value.historyDepth = 1;
        value.historyBytes = semantic::WorkCountV1 * sizeof(semantic::PointRecordV1);
        value.argumentBufferBytes = 24; value.maxCommandCount = 2; value.argumentProducerDispatchX = 1;
        value.hierarchyUpdateDispatchX = 1; value.hierarchyHoldDispatchX = 0;
        value.consumerUpdateDispatchX = 64; value.consumerHoldDispatchX = 0;
        value.spiral4IndirectArtifactIdentity = cu2.Spiral4IndirectArtifactIdentity();
        break;
    default: break;
    }
    value.rawCandidateIdentity = fc::RawTemporalCandidateIdentityV1(value);
    return value;
}

VerifiedTemporalCandidateOperationV1 Operation(const fc::RawTemporalCandidateV1& value)
{
    VerifiedTemporalCandidateOperationV1 out;
    out.kind=value.kind; out.historyRole=value.historyRole; out.issuancePolicy=value.issuancePolicy;
    out.completionPolicy=value.completionPolicy; out.holdPolicy=value.holdPolicy;
    out.timelineLength=value.timelineLength; out.workCount=value.workCount; out.threadsPerGroup=value.threadsPerGroup;
    out.historyDepth=value.historyDepth; out.historyBytes=value.historyBytes; out.argumentBufferBytes=value.argumentBufferBytes;
    out.argumentStrideBytes=value.argumentStrideBytes; out.maxCommandCount=value.maxCommandCount;
    out.argumentProducerDispatchX=value.argumentProducerDispatchX;
    out.hierarchyUpdateDispatchX=value.hierarchyUpdateDispatchX; out.hierarchyHoldDispatchX=value.hierarchyHoldDispatchX;
    out.consumerUpdateDispatchX=value.consumerUpdateDispatchX; out.consumerHoldDispatchX=value.consumerHoldDispatchX;
    out.updateCount=value.updateCount; out.holdCount=value.holdCount; out.maximumSourceGeneration=value.maximumSourceGeneration;
    out.completionContractIdentity=value.completionContractIdentity;
    out.spiral4IndirectArtifactIdentity=value.spiral4IndirectArtifactIdentity;
    out.argumentProducerProgramIdentity=value.argumentProducerProgramIdentity;
    out.hierarchyProgramIdentity=value.hierarchyProgramIdentity; out.consumerProgramIdentity=value.consumerProgramIdentity;
    out.artifactIdentity=value.proposedArtifactIdentity; out.historyResourceIdentity=value.proposedHistoryResourceIdentity;
    out.invocationAuthorityIdentity=value.invocationAuthorityIdentity;
    const auto bytes=SerializeVerifiedTemporalCandidateOperationV1(out); out.operationIdentity=base::Sha256(bytes);
    return out;
}

base::Digest256 PlanIdentity(const VerifiedTemporalCandidatePlanV1& plan)
{
    base::BinaryWriter writer;
    Digest(writer, Literal("SGE4-5.Spiral5.VerifiedTemporalCandidatePlan.V1"));
    Digest(writer, plan.semanticIdentity); Digest(writer, plan.scheduleIdentity);
    Digest(writer, plan.context.targetProfileIdentity); Digest(writer, plan.context.observationContractIdentity);
    Digest(writer, plan.context.deviceEpochPolicyIdentity); Digest(writer, plan.operation.operationIdentity);
    return base::Sha256(writer.Bytes());
}
base::Digest256 Certificate(const VerifiedTemporalCandidatePlanV1& plan)
{
    base::BinaryWriter writer;
    Digest(writer, Literal("SGE4-5.Spiral5.VerifierCertificate.TemporalCandidateFamily.V1"));
    Digest(writer, plan.planIdentity); Digest(writer, plan.operation.artifactIdentity);
    Digest(writer, plan.operation.historyResourceIdentity); Digest(writer, plan.operation.invocationAuthorityIdentity);
    return base::Sha256(writer.Bytes());
}
}

VerifiedTemporalCandidateV1::VerifiedTemporalCandidateV1(
    VerifiedTemporalCandidatePlanV1 plan, base::Digest256 certificateIdentity)
    : plan_(std::move(plan)), certificateIdentity_(certificateIdentity) {}

TemporalCandidateFamilyVerificationContextV1 BuildCanonicalTemporalCandidateFamilyVerificationContextV1()
{
    return {fc::TemporalFamilyTargetProfileIdentityV1(),
            fc::TemporalFamilyObservationContractIdentityV1(),
            fc::TemporalFamilyDeviceEpochPolicyIdentityV1()};
}

base::Result<VerifiedTemporalCandidateV1, std::string> VerifyTemporalCandidateV1(
    const semantic::TemporalStateSemanticV1& semanticValue,
    const semantic::UpdateScheduleV1& schedule,
    const TemporalCandidateFamilyVerificationContextV1& context,
    const fc::RawTemporalCandidateV1& proposal)
{
    auto scheduleValid=semantic::ValidateUpdateScheduleV1(schedule);
    if(!scheduleValid) return base::Result<VerifiedTemporalCandidateV1,std::string>::Failure(scheduleValid.Error().message);
    if(context!=BuildCanonicalTemporalCandidateFamilyVerificationContextV1())
        return base::Result<VerifiedTemporalCandidateV1,std::string>::Failure("Temporal Candidate verification context is not canonical.");
    if (proposal.kind != fc::TemporalCandidateKindV1::EveryInvocationRecompute &&
        proposal.kind != fc::TemporalCandidateKindV1::GlobalMotorHistoryReuse &&
        proposal.kind != fc::TemporalCandidateKindV1::FinalOutputHistoryReuse)
        return base::Result<VerifiedTemporalCandidateV1,std::string>::Failure("Unknown Temporal Candidate kind.");
    const auto expected=Expected(semanticValue,schedule,proposal.kind);
    if(proposal.rawCandidateIdentity!=fc::RawTemporalCandidateIdentityV1(proposal))
        return base::Result<VerifiedTemporalCandidateV1,std::string>::Failure("Raw Temporal Candidate identity mismatch.");
    if(fc::SerializeRawTemporalCandidateV1(proposal)!=fc::SerializeRawTemporalCandidateV1(expected))
        return base::Result<VerifiedTemporalCandidateV1,std::string>::Failure("Raw Temporal Candidate differs from independent derivation.");
    VerifiedTemporalCandidatePlanV1 plan;
    plan.semanticIdentity=expected.semanticIdentity; plan.scheduleIdentity=expected.scheduleIdentity; plan.context=context;
    plan.operation=Operation(expected); plan.planIdentity=PlanIdentity(plan);
    return base::Result<VerifiedTemporalCandidateV1,std::string>::Success(
        VerifiedTemporalCandidateV1(plan,Certificate(plan)));
}

base::Result<void,std::string> ValidateVerifiedTemporalCandidateContextV1(
    const VerifiedTemporalCandidateV1& verified,
    const semantic::TemporalStateSemanticV1& semanticValue,
    const semantic::UpdateScheduleV1& schedule,
    const TemporalCandidateFamilyVerificationContextV1& context)
{
    if(context!=BuildCanonicalTemporalCandidateFamilyVerificationContextV1())
        return base::Result<void,std::string>::Failure("Temporal Candidate context differs.");
    const auto expected=Expected(semanticValue,schedule,verified.Plan().operation.kind);
    const auto operation=Operation(expected);
    if(verified.Plan().semanticIdentity!=expected.semanticIdentity || verified.Plan().scheduleIdentity!=expected.scheduleIdentity ||
       verified.Plan().context!=context || verified.Plan().operation.operationIdentity!=operation.operationIdentity ||
       verified.Plan().planIdentity!=PlanIdentity(verified.Plan()) || verified.CertificateIdentity()!=Certificate(verified.Plan()))
        return base::Result<void,std::string>::Failure("Verified Temporal Candidate context or seal mismatch.");
    return base::Result<void,std::string>::Success();
}

std::vector<std::byte> SerializeVerifiedTemporalCandidateOperationV1(const VerifiedTemporalCandidateOperationV1& v)
{
    base::BinaryWriter w;
    Digest(w,Literal("SGE4-5.Spiral5.VerifiedTemporalCandidateOperation.V1"));
    w.WriteU32(static_cast<std::uint32_t>(v.kind)); w.WriteU32(static_cast<std::uint32_t>(v.historyRole));
    w.WriteU32(static_cast<std::uint32_t>(v.issuancePolicy)); w.WriteU32(static_cast<std::uint32_t>(v.completionPolicy));
    w.WriteU32(static_cast<std::uint32_t>(v.holdPolicy)); w.WriteU32(v.timelineLength); w.WriteU32(v.workCount);
    w.WriteU32(v.threadsPerGroup); w.WriteU32(v.historyDepth); w.WriteU32(v.historyBytes); w.WriteU32(v.argumentBufferBytes);
    w.WriteU32(v.argumentStrideBytes); w.WriteU32(v.maxCommandCount); w.WriteU32(v.argumentProducerDispatchX);
    w.WriteU32(v.hierarchyUpdateDispatchX); w.WriteU32(v.hierarchyHoldDispatchX); w.WriteU32(v.consumerUpdateDispatchX);
    w.WriteU32(v.consumerHoldDispatchX); w.WriteU32(v.updateCount); w.WriteU32(v.holdCount); w.WriteU32(v.maximumSourceGeneration);
    Digest(w,v.completionContractIdentity); Digest(w,v.spiral4IndirectArtifactIdentity); Digest(w,v.argumentProducerProgramIdentity);
    Digest(w,v.hierarchyProgramIdentity); Digest(w,v.consumerProgramIdentity); Digest(w,v.artifactIdentity);
    Digest(w,v.historyResourceIdentity); Digest(w,v.invocationAuthorityIdentity);
    return std::move(w).Take();
}
std::vector<std::byte> SerializeVerifiedTemporalCandidateV1(const VerifiedTemporalCandidateV1& v)
{
    base::BinaryWriter w;
    Digest(w,Literal("SGE4-5.Spiral5.VerifiedTemporalCandidate.V1"));
    Digest(w,v.Plan().semanticIdentity); Digest(w,v.Plan().scheduleIdentity);
    Digest(w,v.Plan().context.targetProfileIdentity); Digest(w,v.Plan().context.observationContractIdentity);
    Digest(w,v.Plan().context.deviceEpochPolicyIdentity);
    const auto op=SerializeVerifiedTemporalCandidateOperationV1(v.Plan().operation);
    w.WriteU32(static_cast<std::uint32_t>(op.size())); w.WriteBytes(op);
    Digest(w,v.Plan().operation.operationIdentity); Digest(w,v.Plan().planIdentity); Digest(w,v.CertificateIdentity());
    return std::move(w).Take();
}
}
