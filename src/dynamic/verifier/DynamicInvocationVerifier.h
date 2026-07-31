#pragma once
#include "../model/DynamicInvocationModel.h"

namespace sge4::dynamic
{
class DynamicInvocationVerifierV1 final
{
public:
    [[nodiscard]] static DynamicVerificationResultV1 Verify(
        const DynamicInvocationRequestV1& request,
        const DynamicPlannerProposalV1& proposal);
};
}
