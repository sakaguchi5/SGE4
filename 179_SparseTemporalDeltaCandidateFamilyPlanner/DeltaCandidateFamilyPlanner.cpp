#include "DeltaCandidateFamilyPlanner.h"

#include <exception>
#include <utility>

namespace sge4_5::spiral7::family_planner
{
namespace fc = family_candidate;

base::Result<fc::RawDeltaFamilyCandidateV1, std::string>
PlanDeltaFamilyCandidateV1(
    const semantic::SparseTemporalDeltaSemanticV1& semanticValue,
    const semantic::SparseDeltaInvocationV1& invocation,
    fc::DeltaFamilyCandidateKindV1 kind)
{
    try
    {
        const auto artifact = fc::BuildDeltaFamilyArtifactV1(semanticValue, invocation, kind);
        const auto valid = fc::ValidateDeltaFamilyArtifactV1(artifact, semanticValue, invocation);
        if (!valid)
            return base::Result<fc::RawDeltaFamilyCandidateV1, std::string>::Failure(valid.Error());

        fc::RawDeltaFamilyCandidateV1 value;
        value.semanticIdentity = semantic::SparseTemporalDeltaSemanticIdentityV1(semanticValue);
        value.invocationIdentity = invocation.Identity();
        value.previousActiveSetIdentity = invocation.PreviousActiveSet().Identity();
        value.activeSetIdentity = invocation.ActiveSet().Identity();
        value.modifiedSetIdentity = invocation.ModifiedSurvivorSet().Identity();
        value.transitionSetIdentity = invocation.TransitionSet().Identity();
        value.transitionRecordsIdentity = semantic::DeltaTransitionRecordsIdentityV1(
            invocation.TransitionRecords());
        value.targetProfileIdentity = fc::DeltaFamilyTargetProfileIdentityV1();
        value.observationContractIdentity = fc::DeltaFamilyObservationContractIdentityV1();
        value.representationResourceContractIdentity =
            fc::DeltaFamilyRepresentationResourceContractIdentityV1(kind);
        value.completionContractIdentity = fc::DeltaFamilyCompletionContractIdentityV1();
        value.deviceEpochPolicyIdentity = fc::DeltaFamilyDeviceEpochPolicyIdentityV1();
        value.consumerProgramIdentity = fc::DeltaFamilyConsumerProgramIdentityV1(kind);
        value.proposedArtifactIdentity = artifact.ArtifactIdentity();
        value.proposedRepresentationResourceIdentity =
            fc::DeltaFamilyRepresentationResourceIdentityV1(artifact);
        value.writeSetAuthorityIdentity =
            fc::DeltaFamilyWriteSetAuthorityIdentityV1(semanticValue, invocation, artifact);
        value.cu2CompactArtifactIdentity = artifact.Cu2CompactArtifactIdentity();
        value.cu3TransitionAuthorityIdentity = artifact.Cu3TransitionAuthorityIdentity();
        value.kind = kind;
        value.representationRole = artifact.Role();
        value.operationCode = artifact.OperationCode();
        value.dispatchPolicy = artifact.DispatchPolicy();
        value.outputHistoryPolicy = artifact.OutputHistoryPolicy();
        value.legalWriteSet = artifact.LegalWriteSet();
        value.invocationIndex = invocation.InvocationIndex();
        value.universeCount = semantic::WorkUniverseCountV1;
        value.activeCount = artifact.ActiveCount();
        value.transitionCount = artifact.TransitionCount();
        value.updateCount = artifact.UpdateCount();
        value.clearCount = artifact.ClearCount();
        value.retainCount = artifact.RetainCount();
        value.affectedBlockCount = artifact.AffectedBlockCount();
        value.threadsPerGroup = semantic::ThreadsPerGroupV1;
        value.dispatchGroupsX = artifact.DispatchGroupsX();
        value.payloadStrideBytes = artifact.PayloadStrideBytes();
        value.fixedCapacityBytes = artifact.FixedCapacityBytes();
        value.historyBytes = static_cast<std::uint32_t>(
            semantic::BuildCanonicalInactiveHistoryBytesV1().size());
        value.outputCapacity = semanticValue.outputCapacity;
        value.outputStrideBytes = spiral6::semantic::OutputRecordStrideV1;
        value.zeroWorkMapsToZeroDispatch =
            kind == fc::DeltaFamilyCandidateKindV1::FullActiveDenseRecompute ? 0u : 1u;
        value.rawCandidateIdentity = fc::RawDeltaFamilyCandidateIdentityV1(value);
        return base::Result<fc::RawDeltaFamilyCandidateV1, std::string>::Success(
            std::move(value));
    }
    catch (const std::exception& error)
    {
        return base::Result<fc::RawDeltaFamilyCandidateV1, std::string>::Failure(error.what());
    }
}

base::Result<std::vector<fc::RawDeltaFamilyCandidateV1>, std::string>
PlanCanonicalDeltaCandidateFamilyV1(
    const semantic::SparseTemporalDeltaSemanticV1& semanticValue,
    const semantic::SparseDeltaInvocationV1& invocation)
{
    std::vector<fc::RawDeltaFamilyCandidateV1> result;
    for (const auto kind : {
        fc::DeltaFamilyCandidateKindV1::FullActiveDenseRecompute,
        fc::DeltaFamilyCandidateKindV1::CompactDeltaIndexHistoryReuse,
        fc::DeltaFamilyCandidateKindV1::AffectedBlockDeltaHistoryReuse})
    {
        auto planned = PlanDeltaFamilyCandidateV1(semanticValue, invocation, kind);
        if (!planned)
            return base::Result<std::vector<fc::RawDeltaFamilyCandidateV1>, std::string>::Failure(
                planned.Error());
        result.push_back(std::move(planned).Value());
    }
    return base::Result<std::vector<fc::RawDeltaFamilyCandidateV1>, std::string>::Success(
        std::move(result));
}
} // namespace sge4_5::spiral7::family_planner
