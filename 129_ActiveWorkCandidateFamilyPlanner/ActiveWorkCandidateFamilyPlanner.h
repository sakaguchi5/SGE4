#pragma once

#include "../00_Foundation/Result.h"
#include "../120_ActiveWorkSemantic/ActiveWorkSemantic.h"
#include "../128_ActiveWorkCandidateFamily/ActiveWorkCandidateFamily.h"

#include <string>
#include <vector>

namespace sge4_5::spiral4::family_planner
{
[[nodiscard]] base::Result<
    family_candidate::RawCandidateFamilyV1,
    std::string>
PlanCandidateFamilyV1(
    const semantic::ActiveWorkSemanticV1& semantic,
    family_contract::CandidateFamilyKindV1 kind,
    std::uint32_t batchSize);

[[nodiscard]] base::Result<
    std::vector<family_candidate::RawCandidateFamilyV1>,
    std::string>
PlanCanonicalCandidateFamilyCorpusV1(
    const semantic::ActiveWorkSemanticV1& semantic);
}
