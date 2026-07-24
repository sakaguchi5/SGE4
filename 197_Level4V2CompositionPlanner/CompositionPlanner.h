#pragma once
#include "../196_Level4V2CompositionModel/CompositionModel.h"

namespace sge4_5::v2::composition
{
enum class CompositionPlanningErrorV1 : std::uint8_t
{
    None = 0,
    InvalidContract = 1,
    GraphCycle = 2
};
struct CompositionPlanningResultV1 final
{
    CompositionPlanningErrorV1 error;
    std::optional<CompositionPlanProposalV1> proposal;
    [[nodiscard]] bool Planned() const noexcept { return error==CompositionPlanningErrorV1::None && proposal.has_value(); }
};
class CompositionPlannerV1 final
{
public:
    [[nodiscard]] static CompositionPlanningResultV1 Plan(const RawCompositionContractV1& contract);
};
}
