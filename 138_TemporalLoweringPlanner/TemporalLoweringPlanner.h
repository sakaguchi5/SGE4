#pragma once

#include "../00_Foundation/Result.h"
#include "../134_TemporalStateSemantic/TemporalStateSemantic.h"
#include "../137_TemporalLoweringCandidate/TemporalLoweringCandidate.h"

#include <string>

namespace sge4_5::spiral5::planner
{
[[nodiscard]] base::Result<candidate::RawTemporalLoweringCandidateV1, std::string>
PlanGlobalMotorHistoryReuseCandidateV1(
    const semantic::TemporalStateSemanticV1& semantic,
    const semantic::UpdateScheduleV1& schedule);
}
