#pragma once

#include "../191_Level4V2AuthorityModel/AuthorityModel.h"

namespace sge4_5::v2::authority
{
class IndependentVerifierV1 final
{
public:
    [[nodiscard]] static VerificationResultV1 Verify(
        const AuthorityRequestV1& request,
        const ExplicitCandidateInputV1& candidateInput,
        const PlannerProposalV1& proposal);
};
}
