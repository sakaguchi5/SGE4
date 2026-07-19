#include "CompositionRecovery.h"

#include <algorithm>
#include <utility>

namespace sge4_5::composition::canonical::runtime_v1
{
namespace
{
StaticRuntimeError Error(std::string stage, std::string message)
{
    return {std::move(stage), std::move(message)};
}
StaticRuntimeError Error(const DomainError& value) { return {value.stage, value.message}; }
StaticRuntimeError Error(const SharedResourceError& value) { return {value.stage, value.message}; }
template<class T> base::Result<T, StaticRuntimeError> Failure(std::string stage, std::string message)
{
    return base::Result<T, StaticRuntimeError>::Failure(Error(std::move(stage), std::move(message)));
}
}

base::Result<WholeCompositionRecoveryReport, StaticRuntimeError>
WholeCompositionRecovery::Recover(
    LoadedStaticComposition& loaded,
    ::sge4_5::runtime::DeviceRecoveryMode mode)
{
    if (!loaded.domain_)
        return Failure<WholeCompositionRecoveryReport>(
            "composition-recovery/domain", "Composition DeviceDomain is unavailable");

    const auto state = loaded.domain_->State();
    if (mode == ::sge4_5::runtime::DeviceRecoveryMode::RetryAdapterReacquisition)
    {
        if (state != ::sge4_5::runtime::DeviceRuntimeState::AwaitingAdapter)
            return Failure<WholeCompositionRecoveryReport>(
                "composition-recovery/state", "retry requires AwaitingAdapter state");
    }
    else if (state != ::sge4_5::runtime::DeviceRuntimeState::Active)
    {
        return Failure<WholeCompositionRecoveryReport>(
            "composition-recovery/state", "recovery requires an Active DeviceDomain");
    }

    WholeCompositionRecoveryReport report;
    report.leafCountBefore = loaded.domain_->LeafCount();
    report.resourceCountBefore = loaded.resources_ ? loaded.resources_->ResourceCount() : 0;

    // The whole Composition is the recovery unit. No Package instance, shared Buffer,
    // or completion token survives across the native DeviceDomain transition.
    loaded.resources_.reset();
    std::fill(loaded.endpointTokens_.begin(), loaded.endpointTokens_.end(), nullptr);
    loaded.domain_->ClearLeafInstances();
    report.allRuntimeObjectsReleased =
        loaded.domain_->LeafCount() == 0 && loaded.resources_ == nullptr &&
        std::ranges::all_of(loaded.endpointTokens_, [](const auto& value) { return !value; });
    if (!report.allRuntimeObjectsReleased)
        return Failure<WholeCompositionRecoveryReport>(
            "composition-recovery/release", "runtime objects survived the recovery boundary");

    auto native = loaded.domain_->RecoverNativeDomain(mode);
    if (!native)
        return base::Result<WholeCompositionRecoveryReport, StaticRuntimeError>::Failure(
            Error(native.Error()));
    report.device = std::move(native).Value();

    if (loaded.domain_->State() != ::sge4_5::runtime::DeviceRuntimeState::Active)
    {
        report.leafCountAfter = loaded.domain_->LeafCount();
        report.resourceCountAfter = 0;
        if (loaded.domain_->State() != ::sge4_5::runtime::DeviceRuntimeState::AwaitingAdapter ||
            report.leafCountAfter != 0)
            return Failure<WholeCompositionRecoveryReport>(
                "composition-recovery/awaiting", "non-Active recovery retained runtime objects");
        return base::Result<WholeCompositionRecoveryReport, StaticRuntimeError>::Success(
            std::move(report));
    }

    auto leaves = loaded.domain_->RematerializeLeaves();
    if (!leaves)
        return base::Result<WholeCompositionRecoveryReport, StaticRuntimeError>::Failure(
            Error(leaves.Error()));
    report.frozenArtifactRevalidated = true;

    auto resources = MaterializeSharedResources(
        *loaded.domain_, loaded.recoveryInitialResources_);
    if (!resources)
        return base::Result<WholeCompositionRecoveryReport, StaticRuntimeError>::Failure(
            Error(resources.Error()));
    loaded.resources_ = std::make_unique<SharedResourceTable>(
        std::move(resources).Value());
    loaded.endpointTokens_.assign(
        loaded.domain_->Artifact().ValidatedContract().Contract().endpoints.size(), nullptr);

    report.leafCountAfter = loaded.domain_->LeafCount();
    report.resourceCountAfter = loaded.resources_->ResourceCount();
    const auto& contract = loaded.domain_->Artifact().ValidatedContract().Contract();
    report.allRuntimeObjectsRematerialized =
        report.leafCountAfter == contract.leaves.size() &&
        report.resourceCountAfter == contract.resources.size() &&
        loaded.domain_->DeviceEpoch() == report.device.newDeviceEpoch &&
        std::ranges::all_of(loaded.endpointTokens_, [](const auto& value) { return !value; });
    if (!report.allRuntimeObjectsRematerialized)
        return Failure<WholeCompositionRecoveryReport>(
            "composition-recovery/rematerialize", "whole Composition was not rematerialized exactly");

    report.device.packageObjectsRebuilt = true;
    report.device.temporalHistoryReset = true;
    report.device.externalRebindRequired = true;
    return base::Result<WholeCompositionRecoveryReport, StaticRuntimeError>::Success(
        std::move(report));
}

base::Result<WholeCompositionRecoveryReport, StaticRuntimeError> RecoverStaticComposition(
    LoadedStaticComposition& loaded,
    ::sge4_5::runtime::DeviceRecoveryMode mode)
{
    return WholeCompositionRecovery::Recover(loaded, mode);
}
}
