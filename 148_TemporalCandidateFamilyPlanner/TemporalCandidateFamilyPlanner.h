#pragma once

#include "../00_Foundation/Result.h"
#include "../134_TemporalStateSemantic/TemporalStateSemantic.h"
#include "../147_TemporalCandidateFamily/TemporalCandidateFamily.h"

#include <vector>

namespace sge4_5::spiral5::family_planner
{
[[nodiscard]] base::Result<family_candidate::RawTemporalCandidateV1, std::string>
PlanTemporalCandidateV1(
    const semantic::TemporalStateSemanticV1& semantic,
    const semantic::UpdateScheduleV1& schedule,
    family_candidate::TemporalCandidateKindV1 kind);

[[nodiscard]] base::Result<std::vector<family_candidate::RawTemporalCandidateV1>, std::string>
PlanCanonicalTemporalCandidateFamilyV1(
    const semantic::TemporalStateSemanticV1& semantic,
    const semantic::UpdateScheduleV1& schedule);
}
