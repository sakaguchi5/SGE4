#include "RuntimeSubmission.h"

namespace sge4_5::v2::runtime_recovery
{
namespace
{
canonical::ResourceInstanceIdentity MakeSubmissionResourceIdentity(
    RuntimeSubmissionIdentity submissionIdentity,
    std::uint8_t kind)
{
    std::vector<std::byte> payload(submissionIdentity.Digest().begin(), submissionIdentity.Digest().end());
    payload.push_back(static_cast<std::byte>(kind));
    return canonical::MakeCanonicalIdentityV1<canonical::ResourceInstanceIdentity>(
        {"SGE.V2.Runtime.SubmissionResource.V1", RuntimeRecoverySchemaVersionV1, payload});
}
}

RuntimeSubmissionResultV1 SubmitFrozenInvocationV1(
    LoadedRuntimeV1& loaded,
    const frozen_dynamic::OpaqueFrozenDynamicInvocationV1& invocation)
{
    if (loaded.state_ == RuntimeStateV1::ActiveNeedsExternalRebind)
        return {RuntimeErrorV1::ExternalRebindRequired, std::nullopt};
    if (loaded.state_ != RuntimeStateV1::Active)
        return {RuntimeErrorV1::RuntimeNotActive, std::nullopt};
    if (invocation.CompositionIdentity() != loaded.composition_.Identity())
        return {RuntimeErrorV1::InvocationCompositionMismatch, std::nullopt};
    const auto& history = invocation.NextHistory();
    if (history.Descriptor().deviceEpoch != loaded.deviceEpoch_)
        return {RuntimeErrorV1::InvocationEpochMismatch, std::nullopt};
    if (loaded.firstSubmissionRequiresRecoverySeed_)
    {
        if (invocation.Mode() != dynamic::InvocationModeV1::RecoverySeed ||
            invocation.IndirectWorkCount().Value() != history.ActiveSet().Count())
            return {RuntimeErrorV1::RecoverySeedRequired, std::nullopt};
    }
    NativeSubmissionRequestV1 request{
        invocation.Identity(), invocation.CompositionIdentity(), invocation.WriteSetIdentity(),
        invocation.IndirectWorkCount(), loaded.deviceEpoch_};
    if (!loaded.backend_->Submit(request))
        return {RuntimeErrorV1::SubmissionFailed, std::nullopt};
    const auto identity = ComputeRuntimeSubmissionIdentityV1(
        loaded.runtimeIdentity_, invocation.Identity(), invocation.WriteSetIdentity(),
        invocation.IndirectWorkCount(), loaded.deviceEpoch_);
    auto submissionHandle = canonical::SubmissionHandleV1(MakeSubmissionResourceIdentity(identity, 1u), loaded.deviceEpoch_);
    auto completionHandle = canonical::CompletionHandleV1(MakeSubmissionResourceIdentity(identity, 2u), loaded.deviceEpoch_);
    loaded.acceptedHistoryIdentity_ = history.Descriptor().identity;
    loaded.firstSubmissionRequiresRecoverySeed_ = false;
    return {RuntimeErrorV1::None, RuntimeSubmissionV1{
        identity, std::move(submissionHandle), std::move(completionHandle), history.Descriptor().identity,
        invocation.IndirectWorkCount()}};
}
}
