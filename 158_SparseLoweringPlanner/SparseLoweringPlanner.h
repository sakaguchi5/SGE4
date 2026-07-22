#pragma once

#include "../00_Foundation/Result.h"
#include "../154_SparseWorkSetSemantic/SparseWorkSetSemantic.h"
#include "../157_SparseLoweringCandidate/SparseLoweringCandidate.h"

#include <string>

namespace sge4_5::spiral6::planner
{
[[nodiscard]] base::Result<candidate::RawSparseLoweringCandidateV1, std::string>
PlanCompactIndexListCandidateV1(
    const semantic::SparsePointTransformSemanticV1& semantic,
    const semantic::ExactSparseWorkSetV1& set);
}
