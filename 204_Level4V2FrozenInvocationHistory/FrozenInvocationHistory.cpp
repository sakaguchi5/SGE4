#include "FrozenInvocationHistory.h"

namespace sge4_5::v2::dynamic::frozen_dynamic_detail
{
OpaqueFrozenDynamicInvocationV1 FrozenDynamicInvocationBuilderV1::Freeze(
    const VerifiedDynamicInvocationV1& verified)
{
    const auto& request = verified.Request();
    const auto& decision = verified.Decision();
    const auto historyIdentity = ComputeHistoryValidityIdentityV1(
        request.compositionIdentity,
        request.semanticIdentity,
        request.identity,
        decision.nextHistoryGeneration,
        request.deviceEpoch,
        request.activeSet,
        decision.generationVectorIdentity);

    canonical::HistoryValidityDescriptorV1 descriptor{
        historyIdentity,
        request.semanticIdentity,
        request.identity,
        decision.nextHistoryGeneration,
        request.deviceEpoch,
        canonical::HistoryStateV1::Valid};

    VerifiedHistoryStateV1 history(
        std::move(descriptor),
        request.compositionIdentity,
        request.universe,
        request.activeSet,
        decision.generationVectorIdentity,
        decision.itemGenerations);

    const auto frozenIdentity = ComputeFrozenDynamicInvocationIdentityV1(
        request.compositionIdentity,
        request.identity,
        decision.identity,
        verified.SealIdentity(),
        historyIdentity,
        decision.dynamicWriteSetIdentity);

    return OpaqueFrozenDynamicInvocationV1(
        frozenIdentity,
        request.compositionIdentity,
        request.identity,
        decision.identity,
        verified.SealIdentity(),
        decision.dynamicWriteSetIdentity,
        decision.indirectWorkCount,
        request.mode,
        std::move(history));
}
}
