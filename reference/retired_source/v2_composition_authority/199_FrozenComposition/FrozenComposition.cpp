#include "FrozenComposition.h"
namespace sge4_5::v2::composition::frozen_composition_detail
{
OpaqueFrozenCompositionV1 FrozenCompositionBuilderV1::Freeze(const VerifiedCompositionV1& verified)
{
    return OpaqueFrozenCompositionV1(ComputeFrozenCompositionIdentityV1(verified),verified.ContractIdentity(),verified.PlanIdentity(),verified.SealIdentity(),verified.LeafCount(),verified.FlowCount());
}
}
