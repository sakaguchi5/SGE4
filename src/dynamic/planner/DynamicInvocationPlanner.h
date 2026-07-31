#pragma once
#include "../model/DynamicInvocationModel.h"

namespace sge4::dynamic
{
class DynamicInvocationPlannerV1 final
{
public:
    [[nodiscard]] static DynamicPlanningResultV1 Plan(const DynamicInvocationRequestV1& request);
};
}
