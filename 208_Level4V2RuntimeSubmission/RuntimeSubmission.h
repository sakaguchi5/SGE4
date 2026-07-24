#pragma once

#include "../207_Level4V2RuntimeMaterialization/RuntimeMaterialization.h"

namespace sge4_5::v2::runtime_recovery
{
struct RuntimeSubmissionV1 final
{
    RuntimeSubmissionIdentity identity;
    canonical::SubmissionHandleV1 submissionHandle;
    canonical::CompletionHandleV1 completionHandle;
    canonical::HistoryValidityIdentity acceptedHistoryIdentity;
    canonical::TransitionCount indirectWorkCount;
};

struct RuntimeSubmissionResultV1 final
{
    RuntimeErrorV1 error = RuntimeErrorV1::None;
    std::optional<RuntimeSubmissionV1> submission;
    [[nodiscard]] bool Accepted() const noexcept { return error == RuntimeErrorV1::None && submission.has_value(); }
};

[[nodiscard]] RuntimeSubmissionResultV1 SubmitFrozenInvocationV1(
    LoadedRuntimeV1& loaded,
    const frozen_dynamic::OpaqueFrozenDynamicInvocationV1& invocation);
}
