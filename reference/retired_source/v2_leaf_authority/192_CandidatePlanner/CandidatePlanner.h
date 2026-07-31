#pragma once

#include "../191_Level4V2AuthorityModel/AuthorityModel.h"

namespace sge4_5::v2::authority
{
class CandidatePlannerV1 final
{
public:
    [[nodiscard]] static PlannerProposalV1 Plan(
        const AuthorityRequestV1& request,
        const ExplicitCandidateInputV1& candidateInput);
};
}
