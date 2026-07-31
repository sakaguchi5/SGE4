#include "IndependentVerifier.h"

namespace sge4_5::v2::authority
{
namespace
{
VerificationResultV1 Reject(VerificationErrorV1 error)
{
    return {error, std::nullopt};
}

std::vector<AuthorityOperationV1> BuildExpectedOperationsV1(
    const AuthorityRequestV1& request,
    canonical::CandidateIdentity candidateIdentity)
{
    std::vector<AuthorityOperationV1> expected;
    expected.reserve(AuthorityOperationCountV1);
    expected.emplace_back(AuthorityOperationV1{OperationOrdinal{0u}, AuthorityOperationKindV1::BindSemantic,
        request.semantic.descriptor.identity.Digest()});
    expected.emplace_back(AuthorityOperationV1{OperationOrdinal{1u}, AuthorityOperationKindV1::BindInvocation,
        request.invocation.identity.Digest()});
    expected.emplace_back(AuthorityOperationV1{OperationOrdinal{2u}, AuthorityOperationKindV1::BindCandidate,
        candidateIdentity.Digest()});
    expected.emplace_back(AuthorityOperationV1{OperationOrdinal{3u}, AuthorityOperationKindV1::BindTargetProfile,
        request.targetProfile.identity.Digest()});
    expected.emplace_back(AuthorityOperationV1{OperationOrdinal{4u}, AuthorityOperationKindV1::BindResourceContract,
        request.resourceContract.identity.Digest()});
    expected.emplace_back(AuthorityOperationV1{OperationOrdinal{5u}, AuthorityOperationKindV1::BindLegalWriteSet,
        request.legalWriteSet.identity.Digest()});
    return expected;
}
}

VerificationResultV1 IndependentVerifierV1::Verify(
    const AuthorityRequestV1& request,
    const ExplicitCandidateInputV1& candidateInput,
    const PlannerProposalV1& proposal)
{
    const auto semanticIdentity = ComputeSemanticIdentityV1(request.semantic);
    if (semanticIdentity != request.semantic.descriptor.identity)
        return Reject(VerificationErrorV1::SemanticDefinitionIdentityMismatch);
    if (request.invocation.semanticIdentity != semanticIdentity)
        return Reject(VerificationErrorV1::InvocationSemanticBindingMismatch);

    const auto invocationIdentity = ComputeInvocationIdentityV1(request.invocation);
    if (invocationIdentity != request.invocation.identity)
        return Reject(VerificationErrorV1::InvocationDefinitionIdentityMismatch);

    const auto targetProfileIdentity = ComputeTargetProfileIdentityV1(request.targetProfile);
    if (targetProfileIdentity != request.targetProfile.identity)
        return Reject(VerificationErrorV1::TargetProfileDefinitionIdentityMismatch);
    const auto resourceContractIdentity = ComputeResourceContractIdentityV1(request.resourceContract);
    if (resourceContractIdentity != request.resourceContract.identity)
        return Reject(VerificationErrorV1::ResourceContractDefinitionIdentityMismatch);
    const auto writeSetIdentity = ComputeWriteSetIdentityV1(request.legalWriteSet);
    if (writeSetIdentity != request.legalWriteSet.identity)
        return Reject(VerificationErrorV1::WriteSetDefinitionIdentityMismatch);

    if (proposal.rawCandidate.semanticIdentity != semanticIdentity)
        return Reject(VerificationErrorV1::CandidateSemanticBindingMismatch);
    if (proposal.rawCandidate.invocationIdentity != invocationIdentity)
        return Reject(VerificationErrorV1::CandidateInvocationBindingMismatch);
    if (proposal.rawCandidate.targetProfileIdentity != targetProfileIdentity)
        return Reject(VerificationErrorV1::CandidateTargetBindingMismatch);
    if (proposal.rawCandidate.resourceContractIdentity != resourceContractIdentity)
        return Reject(VerificationErrorV1::CandidateResourceBindingMismatch);
    if (proposal.rawCandidate.proposedWriteSetIdentity != writeSetIdentity)
        return Reject(VerificationErrorV1::CandidateWriteSetBindingMismatch);

    const auto candidateIdentity = ComputeCandidateIdentityV1(request, candidateInput);
    if (proposal.rawCandidate.identity != candidateIdentity)
        return Reject(VerificationErrorV1::CandidateIdentityMismatch);

    const auto expectedOperations = BuildExpectedOperationsV1(request, candidateIdentity);
    if (proposal.operations.size() != expectedOperations.size())
        return Reject(VerificationErrorV1::OperationCountMismatch);
    for (std::size_t index = 0; index < expectedOperations.size(); ++index)
    {
        if (proposal.operations[index].ordinal != expectedOperations[index].ordinal)
            return Reject(VerificationErrorV1::OperationOrdinalMismatch);
        if (proposal.operations[index].kind != expectedOperations[index].kind)
            return Reject(VerificationErrorV1::OperationKindMismatch);
        if (proposal.operations[index].subjectIdentity != expectedOperations[index].subjectIdentity)
            return Reject(VerificationErrorV1::OperationSubjectMismatch);
    }

    const auto operationSequenceIdentity = ComputeOperationSequenceIdentityV1(expectedOperations);
    const auto plannerProposalIdentity = ComputePlannerProposalIdentityV1(
        proposal.rawCandidate, operationSequenceIdentity);
    if (proposal.identity != plannerProposalIdentity)
        return Reject(VerificationErrorV1::PlannerProposalIdentityMismatch);

    const auto contextIdentity = ComputeAuthorityContextIdentityV1(request);
    const auto verifiedPlanIdentity = ComputeVerifiedPlanIdentityV1(
        contextIdentity, candidateIdentity, operationSequenceIdentity);
    const auto sealIdentity = ComputeVerificationSealIdentityV1(
        verifiedPlanIdentity, contextIdentity, candidateIdentity, operationSequenceIdentity);
    auto plan = canonical::VerifiedPlanConstructionAccessV1::Create(verifiedPlanIdentity);
    VerifiedAuthorityV1 verified{
        std::move(plan),
        sealIdentity,
        contextIdentity,
        semanticIdentity,
        invocationIdentity,
        candidateIdentity,
        targetProfileIdentity,
        resourceContractIdentity,
        writeSetIdentity,
        operationSequenceIdentity};
    return {VerificationErrorV1::None, std::optional<VerifiedAuthorityV1>{verified}};
}
}
