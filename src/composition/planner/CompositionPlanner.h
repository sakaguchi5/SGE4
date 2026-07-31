#pragma once

#include "../model/plan/CompositionPlan.h"

namespace sge4::composition::planning
{
// Produces a raw proposal only. It cannot create a verification certificate or Frozen Composition.
[[nodiscard]] base::Expected<RawCompositionPlan, PlanError>
ProposeCompositionPlan(const ValidatedCompositionContract& validatedContract);
}
