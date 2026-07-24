#pragma once
#include "../198_Level4V2CompositionVerifier/CompositionVerifier.h"

namespace sge4_5::v2::composition::frozen_composition_detail
{
class OpaqueFrozenCompositionV1 final
{
public:
    OpaqueFrozenCompositionV1(const OpaqueFrozenCompositionV1&) = default;
    OpaqueFrozenCompositionV1& operator=(const OpaqueFrozenCompositionV1&) = default;

    [[nodiscard]] const FrozenCompositionIdentity& Identity() const noexcept { return identity_; }
    [[nodiscard]] const CompositionContractIdentity& ContractIdentity() const noexcept { return contractIdentity_; }
    [[nodiscard]] const CompositionPlanIdentity& PlanIdentity() const noexcept { return planIdentity_; }
    [[nodiscard]] const CompositionSealIdentity& SealIdentity() const noexcept { return sealIdentity_; }
    [[nodiscard]] std::uint32_t LeafCount() const noexcept { return leafCount_; }
    [[nodiscard]] std::uint32_t FlowCount() const noexcept { return flowCount_; }

private:
    friend class FrozenCompositionBuilderV1;
    OpaqueFrozenCompositionV1(FrozenCompositionIdentity identity,CompositionContractIdentity contractIdentity,CompositionPlanIdentity planIdentity,CompositionSealIdentity sealIdentity,std::uint32_t leafCount,std::uint32_t flowCount) noexcept
        : identity_(std::move(identity)),contractIdentity_(std::move(contractIdentity)),planIdentity_(std::move(planIdentity)),sealIdentity_(std::move(sealIdentity)),leafCount_(leafCount),flowCount_(flowCount) {}
    FrozenCompositionIdentity identity_;
    CompositionContractIdentity contractIdentity_;
    CompositionPlanIdentity planIdentity_;
    CompositionSealIdentity sealIdentity_;
    std::uint32_t leafCount_;
    std::uint32_t flowCount_;
};
class FrozenCompositionBuilderV1 final
{
public:
    [[nodiscard]] static OpaqueFrozenCompositionV1 Freeze(const VerifiedCompositionV1& verified);
};
}
