#pragma once

#include "../00_Foundation/Result.h"
#include "../172_SparseTemporalDeltaSemantic/SparseTemporalDeltaSemantic.h"
#include "../175_SparseTemporalDeltaLoweringCandidate/DeltaLoweringCandidate.h"

#include <string>

namespace sge4_5::spiral7::planner
{
[[nodiscard]] base::Result<candidate::RawDeltaLoweringCandidateV1, std::string>
PlanCompactDeltaHistoryCandidateV1(
    const semantic::SparseTemporalDeltaSemanticV1& semantic,
    const semantic::SparseDeltaInvocationV1& invocation);
} // namespace sge4_5::spiral7::planner
