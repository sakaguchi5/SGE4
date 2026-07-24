#pragma once
#include "../201_Level4V2DynamicInvocationModel/DynamicInvocationModel.h"

namespace sge4_5::v2::dynamic
{
class DynamicInvocationPlannerV1 final
{
public:
    [[nodiscard]] static DynamicPlanningResultV1 Plan(const DynamicInvocationRequestV1& request);
};
}
