#pragma once
#include "../203_Level4V2DynamicInvocationVerifier/DynamicInvocationVerifier.h"

namespace sge4_5::v2::dynamic::frozen_dynamic_detail
{
class OpaqueFrozenDynamicInvocationV1 final
{
public:
    OpaqueFrozenDynamicInvocationV1(const OpaqueFrozenDynamicInvocationV1&) = default;
    OpaqueFrozenDynamicInvocationV1& operator=(const OpaqueFrozenDynamicInvocationV1&) = default;

    [[nodiscard]] const FrozenDynamicInvocationIdentity& Identity() const noexcept { return identity_; }
    [[nodiscard]] const composition::FrozenCompositionIdentity& CompositionIdentity() const noexcept { return compositionIdentity_; }
    [[nodiscard]] const canonical::InvocationIdentity& InvocationIdentity() const noexcept { return invocationIdentity_; }
    [[nodiscard]] const DynamicDecisionIdentity& DecisionIdentity() const noexcept { return decisionIdentity_; }
    [[nodiscard]] const DynamicSealIdentity& SealIdentity() const noexcept { return sealIdentity_; }
    [[nodiscard]] const DynamicWriteSetIdentity& WriteSetIdentity() const noexcept { return dynamicWriteSetIdentity_; }
    [[nodiscard]] canonical::TransitionCount IndirectWorkCount() const noexcept { return indirectWorkCount_; }
    [[nodiscard]] InvocationModeV1 Mode() const noexcept { return mode_; }
    [[nodiscard]] const VerifiedHistoryStateV1& NextHistory() const noexcept { return nextHistory_; }

private:
    friend class FrozenDynamicInvocationBuilderV1;

    OpaqueFrozenDynamicInvocationV1(
        FrozenDynamicInvocationIdentity identity,
        composition::FrozenCompositionIdentity compositionIdentity,
        canonical::InvocationIdentity invocationIdentity,
        DynamicDecisionIdentity decisionIdentity,
        DynamicSealIdentity sealIdentity,
        DynamicWriteSetIdentity dynamicWriteSetIdentity,
        canonical::TransitionCount indirectWorkCount,
        InvocationModeV1 mode,
        VerifiedHistoryStateV1 nextHistory) noexcept
        : identity_(std::move(identity)), compositionIdentity_(std::move(compositionIdentity)),
          invocationIdentity_(std::move(invocationIdentity)), decisionIdentity_(std::move(decisionIdentity)),
          sealIdentity_(std::move(sealIdentity)), dynamicWriteSetIdentity_(std::move(dynamicWriteSetIdentity)),
          indirectWorkCount_(indirectWorkCount), mode_(mode), nextHistory_(std::move(nextHistory)) {}

    FrozenDynamicInvocationIdentity identity_;
    composition::FrozenCompositionIdentity compositionIdentity_;
    canonical::InvocationIdentity invocationIdentity_;
    DynamicDecisionIdentity decisionIdentity_;
    DynamicSealIdentity sealIdentity_;
    DynamicWriteSetIdentity dynamicWriteSetIdentity_;
    canonical::TransitionCount indirectWorkCount_;
    InvocationModeV1 mode_;
    VerifiedHistoryStateV1 nextHistory_;
};

class FrozenDynamicInvocationBuilderV1 final
{
public:
    [[nodiscard]] static OpaqueFrozenDynamicInvocationV1 Freeze(const VerifiedDynamicInvocationV1& verified);
};
}
