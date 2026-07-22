#pragma once

#include "../123_ActiveWorkLoweringCandidate/ActiveWorkLoweringCandidate.h"

namespace sge4_5::spiral4::planner
{
[[nodiscard]] base::Result<
    candidate::RawActiveWorkLoweringCandidateV1,
    std::string>
PlanSingleExecuteIndirectCandidateV1(
    const semantic::ActiveWorkSemanticV1& semantic);
}
