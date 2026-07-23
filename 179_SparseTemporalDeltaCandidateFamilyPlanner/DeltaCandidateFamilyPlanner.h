#pragma once

#include "../00_Foundation/Result.h"
#include "../172_SparseTemporalDeltaSemantic/SparseTemporalDeltaSemantic.h"
#include "../178_SparseTemporalDeltaCandidateFamily/DeltaCandidateFamily.h"

#include <string>
#include <vector>

namespace sge4_5::spiral7::family_planner
{
[[nodiscard]] base::Result<family_candidate::RawDeltaFamilyCandidateV1, std::string>
PlanDeltaFamilyCandidateV1(
    const semantic::SparseTemporalDeltaSemanticV1& semantic,
    const semantic::SparseDeltaInvocationV1& invocation,
    family_candidate::DeltaFamilyCandidateKindV1 kind);

[[nodiscard]] base::Result<std::vector<family_candidate::RawDeltaFamilyCandidateV1>, std::string>
PlanCanonicalDeltaCandidateFamilyV1(
    const semantic::SparseTemporalDeltaSemanticV1& semantic,
    const semantic::SparseDeltaInvocationV1& invocation);
} // namespace sge4_5::spiral7::family_planner
