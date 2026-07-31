#pragma once
#include "../196_Level4V2CompositionModel/CompositionModel.h"

namespace sge4_5::v2::composition
{
class CompositionVerifierV1 final
{
public:
    [[nodiscard]] static CompositionVerificationResultV1 Verify(
        const RawCompositionContractV1& contract,
        const CompositionPlanProposalV1& proposal);
};
}
