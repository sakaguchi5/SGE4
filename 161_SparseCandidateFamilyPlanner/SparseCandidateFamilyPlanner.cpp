#include "SparseCandidateFamilyPlanner.h"

#include "../155_Spiral6SparseContracts/Spiral6SparseContracts.h"

#include <exception>
#include <utility>

namespace sge4_5::spiral6::family_planner
{
namespace fc = family_candidate;

base::Result<fc::RawSparseFamilyCandidateV1, std::string>
PlanSparseFamilyCandidateV1(
    const semantic::SparsePointTransformSemanticV1& semanticValue,
    const semantic::ExactSparseWorkSetV1& set,
    fc::SparseFamilyCandidateKindV1 kind)
{
    try
    {
        const auto artifact = fc::BuildSparseFamilyArtifactV1(semanticValue, set, kind);
        auto valid = fc::ValidateSparseFamilyArtifactV1(artifact, semanticValue, set);
        if (!valid) return base::Result<fc::RawSparseFamilyCandidateV1, std::string>::Failure(valid.Error());

        fc::RawSparseFamilyCandidateV1 value;
        value.semanticIdentity = semantic::SparsePointTransformSemanticIdentityV1(semanticValue);
        value.sparseSetIdentity = set.Identity();
        value.targetProfileIdentity = fc::SparseFamilyTargetProfileIdentityV1();
        value.observationContractIdentity = fc::SparseFamilyObservationContractIdentityV1();
        value.representationResourceContractIdentity = fc::SparseFamilyRepresentationResourceContractIdentityV1(kind);
        value.completionContractIdentity = fc::SparseFamilyCompletionContractIdentityV1();
        value.deviceEpochPolicyIdentity = fc::SparseFamilyDeviceEpochPolicyIdentityV1();
        value.spiral4IndirectArtifactIdentity = contract::Spiral4DispatchContractIdentityV1();
        value.consumerProgramIdentity = fc::SparseFamilyConsumerProgramIdentityV1();
        value.proposedArtifactIdentity = artifact.ArtifactIdentity();
        value.proposedRepresentationResourceIdentity = fc::SparseFamilyRepresentationResourceIdentityV1(artifact);
        value.writeSetAuthorityIdentity = fc::SparseFamilyWriteSetAuthorityIdentityV1(semanticValue, set, artifact);
        value.cu2CompactArtifactIdentity = artifact.Cu2CompactArtifactIdentity();
        value.kind = kind;
        value.representationRole = artifact.Role();
        value.operationCode = artifact.OperationCode();
        value.dispatchPolicy = artifact.DispatchPolicy();
        value.universeCount = semantic::WorkUniverseCountV1;
        value.activeCount = artifact.ActiveCount();
        value.activeBlockCount = artifact.ActiveBlockCount();
        value.threadsPerGroup = semantic::ThreadsPerGroupV1;
        value.dispatchGroupsX = artifact.DispatchGroupsX();
        value.payloadStrideBytes = artifact.PayloadStrideBytes();
        value.fixedCapacityBytes = artifact.FixedCapacityBytes();
        value.outputCapacity = semanticValue.outputCapacity;
        value.outputStrideBytes = semantic::OutputRecordStrideV1;
        value.zeroActiveMapsToZeroDispatch = kind == fc::SparseFamilyCandidateKindV1::DenseMembershipMaskPredicate ? 0u : 1u;
        value.rawCandidateIdentity = fc::RawSparseFamilyCandidateIdentityV1(value);
        return base::Result<fc::RawSparseFamilyCandidateV1, std::string>::Success(std::move(value));
    }
    catch (const std::exception& error)
    {
        return base::Result<fc::RawSparseFamilyCandidateV1, std::string>::Failure(error.what());
    }
}

base::Result<std::vector<fc::RawSparseFamilyCandidateV1>, std::string>
PlanCanonicalSparseCandidateFamilyV1(
    const semantic::SparsePointTransformSemanticV1& semanticValue,
    const semantic::ExactSparseWorkSetV1& set)
{
    std::vector<fc::RawSparseFamilyCandidateV1> result;
    for (const auto kind : {
        fc::SparseFamilyCandidateKindV1::DenseMembershipMaskPredicate,
        fc::SparseFamilyCandidateKindV1::CompactIndexListDispatch,
        fc::SparseFamilyCandidateKindV1::ActiveBlockListLocalMask})
    {
        auto planned = PlanSparseFamilyCandidateV1(semanticValue, set, kind);
        if (!planned) return base::Result<std::vector<fc::RawSparseFamilyCandidateV1>, std::string>::Failure(planned.Error());
        result.push_back(std::move(planned).Value());
    }
    return base::Result<std::vector<fc::RawSparseFamilyCandidateV1>, std::string>::Success(std::move(result));
}
}
