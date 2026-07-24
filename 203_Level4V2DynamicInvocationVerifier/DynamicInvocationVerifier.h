#pragma once
#include "../201_Level4V2DynamicInvocationModel/DynamicInvocationModel.h"

namespace sge4_5::v2::dynamic
{
class DynamicInvocationVerifierV1 final
{
public:
    [[nodiscard]] static DynamicVerificationResultV1 Verify(
        const DynamicInvocationRequestV1& request,
        const DynamicPlannerProposalV1& proposal);
};
}
