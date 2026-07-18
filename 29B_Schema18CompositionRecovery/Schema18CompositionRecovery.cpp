#include "Schema18CompositionRecovery.h"

#include <algorithm>
#include <memory>
#include <utility>

namespace sge4::composition::schema18::runtime_v1
{
namespace
{
CompositionRecoveryError Error(std::string stage, std::string message)
{
    return {std::move(stage), std::move(message)};
}

CompositionRecoveryError Error(const DomainError& value)
{
    return {value.stage, value.message};
}

CompositionRecoveryError Error(const SharedResourceError& value)
{
    return {value.stage, value.message};
}

template<class T>
base::Result<T, CompositionRecoveryError> Failure(
    std::string stage, std::string message)
{
    return base::Result<T, CompositionRecoveryError>::Failure(
        Error(std::move(stage), std::move(message)));
}
}

base::Result<CompositionRecoveryReport, CompositionRecoveryError>
StaticCompositionRecoveryAccess::Recover(
    LoadedStaticComposition& loaded,
    runtime::DeviceRecoveryMode mode)
{
    if (!loaded.domain_)
        return Failure<CompositionRecoveryReport>(
            "composition-recovery/domain", "Static Composition has no DeviceDomain");

    const auto stateBefore = loaded.domain_->State();
    if (mode == runtime::DeviceRecoveryMode::RetryAdapterReacquisition)
    {
        if (stateBefore != runtime::DeviceRuntimeState::AwaitingAdapter)
            return Failure<CompositionRecoveryReport>(
                "composition-recovery/retry", "Retry requires AwaitingAdapter state");
    }
    else if (stateBefore != runtime::DeviceRuntimeState::Active)
        return Failure<CompositionRecoveryReport>(
            "composition-recovery/state", "recovery requires an Active Composition");

    // Shared resources and tokens must disappear before native Device loss.
    loaded.resources_.reset();
    loaded.endpointTokens_.assign(
        loaded.domain_->Artifact().contract.endpoints.size(), nullptr);

    auto recovered = loaded.domain_->Recover(mode);
    if (!recovered)
        return base::Result<CompositionRecoveryReport, CompositionRecoveryError>::Failure(
            Error(recovered.Error()));

    CompositionRecoveryReport result;
    result.device = std::move(recovered).Value();
    result.endpointTokensReset = std::ranges::all_of(
        loaded.endpointTokens_, [](const auto& value) { return !value; });
    result.leafCountAfter = loaded.domain_->LeafCount();

    if (result.device.stateAfter != runtime::DeviceRuntimeState::Active)
    {
        if (loaded.resources_ || result.leafCountAfter != 0 ||
            result.device.packageObjectsRebuilt || !result.endpointTokensReset)
            return Failure<CompositionRecoveryReport>(
                "composition-recovery/inactive", "inactive recovery retained Runtime objects");
        return base::Result<CompositionRecoveryReport, CompositionRecoveryError>::Success(
            std::move(result));
    }

    auto resources = MaterializeSharedResources(
        *loaded.domain_, loaded.recoveryInitialResources_);
    if (!resources)
        return base::Result<CompositionRecoveryReport, CompositionRecoveryError>::Failure(
            Error(resources.Error()));
    loaded.resources_ = std::make_unique<SharedResourceTable>(
        std::move(resources).Value());
    result.resourceCountAfter = loaded.resources_->ResourceCount();
    result.resourcesRematerialized =
        result.resourceCountAfter == loaded.domain_->Artifact().contract.resources.size();

    if (result.leafCountAfter != loaded.domain_->Artifact().contract.leaves.size() ||
        !result.resourcesRematerialized || !result.endpointTokensReset ||
        loaded.domain_->DeviceEpoch() != result.device.newDeviceEpoch)
        return Failure<CompositionRecoveryReport>(
            "composition-recovery/rematerialize", "whole-domain rematerialization is incomplete");

    result.device.packageObjectsRebuilt = true;
    result.device.temporalHistoryReset = true;
    result.device.externalRebindRequired = true;
    return base::Result<CompositionRecoveryReport, CompositionRecoveryError>::Success(
        std::move(result));
}

base::Result<CompositionRecoveryReport, CompositionRecoveryError>
RecoverStaticComposition(
    LoadedStaticComposition& loaded,
    runtime::DeviceRecoveryMode mode)
{
    return StaticCompositionRecoveryAccess::Recover(loaded, mode);
}
}
