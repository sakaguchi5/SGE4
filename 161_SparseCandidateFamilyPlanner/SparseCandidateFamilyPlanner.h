#pragma once

#include "../00_Foundation/Result.h"
#include "../154_SparseWorkSetSemantic/SparseWorkSetSemantic.h"
#include "../160_SparseCandidateFamily/SparseCandidateFamily.h"

#include <string>
#include <vector>

namespace sge4_5::spiral6::family_planner
{
[[nodiscard]] base::Result<family_candidate::RawSparseFamilyCandidateV1, std::string>
PlanSparseFamilyCandidateV1(
    const semantic::SparsePointTransformSemanticV1& semantic,
    const semantic::ExactSparseWorkSetV1& set,
    family_candidate::SparseFamilyCandidateKindV1 kind);

[[nodiscard]] base::Result<std::vector<family_candidate::RawSparseFamilyCandidateV1>, std::string>
PlanCanonicalSparseCandidateFamilyV1(
    const semantic::SparsePointTransformSemanticV1& semantic,
    const semantic::ExactSparseWorkSetV1& set);
}
