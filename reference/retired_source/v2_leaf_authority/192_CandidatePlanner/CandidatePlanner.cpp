#include "CandidatePlanner.h"

namespace sge4_5::v2::authority
{
namespace
{
std::vector<AuthorityOperationV1> BuildPlannerOperationsV1(
    const AuthorityRequestV1& request,
    canonical::CandidateIdentity candidateIdentity)
{
    std::vector<AuthorityOperationV1> operations;
    operations.reserve(AuthorityOperationCountV1);
    operations.push_back({OperationOrdinal{0u}, AuthorityOperationKindV1::BindSemantic,
        request.semantic.descriptor.identity.Digest()});
    operations.push_back({OperationOrdinal{1u}, AuthorityOperationKindV1::BindInvocation,
        request.invocation.identity.Digest()});
    operations.push_back({OperationOrdinal{2u}, AuthorityOperationKindV1::BindCandidate,
        candidateIdentity.Digest()});
    operations.push_back({OperationOrdinal{3u}, AuthorityOperationKindV1::BindTargetProfile,
        request.targetProfile.identity.Digest()});
    operations.push_back({OperationOrdinal{4u}, AuthorityOperationKindV1::BindResourceContract,
        request.resourceContract.identity.Digest()});
    operations.push_back({OperationOrdinal{5u}, AuthorityOperationKindV1::BindLegalWriteSet,
        request.legalWriteSet.identity.Digest()});
    return operations;
}
}

PlannerProposalV1 CandidatePlannerV1::Plan(
    const AuthorityRequestV1& request,
    const ExplicitCandidateInputV1& candidateInput)
{
    const auto candidateIdentity = ComputeCandidateIdentityV1(request, candidateInput);
    canonical::RawCandidateProposalV1 rawCandidate{
        candidateIdentity,
        request.semantic.descriptor.identity,
        request.invocation.identity,
        request.targetProfile.identity,
        request.resourceContract.identity,
        request.legalWriteSet.identity};
    auto operations = BuildPlannerOperationsV1(request, candidateIdentity);
    const auto operationSequenceIdentity = ComputeOperationSequenceIdentityV1(operations);
    const auto proposalIdentity = ComputePlannerProposalIdentityV1(rawCandidate, operationSequenceIdentity);
    return {proposalIdentity, std::move(rawCandidate), std::move(operations)};
}
}
