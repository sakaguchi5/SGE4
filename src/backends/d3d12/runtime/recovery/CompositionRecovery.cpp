#include "./CompositionRecovery.h"

#include <algorithm>
#include <utility>

namespace sge4::d3d12::runtime_detail
{
namespace
{
StaticRuntimeError Error(std::string stage, std::string message)
{
    return {std::move(stage), std::move(message)};
}
StaticRuntimeError Error(const DomainError& value) { return {value.stage, value.message}; }
StaticRuntimeError Error(const SharedResourceError& value) { return {value.stage, value.message}; }
template<class T> base::Expected<T, StaticRuntimeError> Failure(std::string stage, std::string message)
{
    return base::Failure<T, StaticRuntimeError>(Error(std::move(stage), std::move(message)));
}
}

base::Expected<WholeCompositionRecoveryReport, StaticRuntimeError>
WholeCompositionRecovery::Recover(
    LoadedStaticComposition& loaded,
    ::sge4::runtime::DeviceRecoveryMode mode)
{
    if (!loaded.domain_)
        return Failure<WholeCompositionRecoveryReport>(
            "composition-recovery/domain", "Compositionが検証または実行の契約に違反しています。");

    const auto state = loaded.domain_->State();
    if (mode == ::sge4::runtime::DeviceRecoveryMode::RetryAdapterReacquisition)
    {
        if (state != ::sge4::runtime::DeviceRuntimeState::AwaitingAdapter)
            return Failure<WholeCompositionRecoveryReport>(
                "composition-recovery/state", "Adapterが検証または実行の契約に違反しています。");
    }
    else if (state != ::sge4::runtime::DeviceRuntimeState::Active)
    {
        return Failure<WholeCompositionRecoveryReport>(
            "composition-recovery/state", "Deviceが検証または実行の契約に違反しています。");
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
            "composition-recovery/release", "検証または実行の契約に違反しています。");

    auto native = loaded.domain_->RecoverNativeDomain(mode);
    if (!native)
        return base::Failure<WholeCompositionRecoveryReport, StaticRuntimeError>(
            Error(native.error()));
    report.device = std::move(native).value();

    if (loaded.domain_->State() != ::sge4::runtime::DeviceRuntimeState::Active)
    {
        report.leafCountAfter = loaded.domain_->LeafCount();
        report.resourceCountAfter = 0;
        if (loaded.domain_->State() != ::sge4::runtime::DeviceRuntimeState::AwaitingAdapter ||
            report.leafCountAfter != 0)
            return Failure<WholeCompositionRecoveryReport>(
                "composition-recovery/awaiting", "検証または実行の契約に違反しています。");
        return base::Success<WholeCompositionRecoveryReport, StaticRuntimeError>(
            std::move(report));
    }

    auto leaves = loaded.domain_->RematerializeLeaves();
    if (!leaves)
        return base::Failure<WholeCompositionRecoveryReport, StaticRuntimeError>(
            Error(leaves.error()));
    report.frozenArtifactRevalidated = true;

    auto resources = MaterializeSharedResources(
        *loaded.domain_, loaded.recoveryInitialResources_);
    if (!resources)
        return base::Failure<WholeCompositionRecoveryReport, StaticRuntimeError>(
            Error(resources.error()));
    loaded.resources_ = std::make_unique<SharedResourceTable>(
        std::move(resources).value());
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
            "composition-recovery/rematerialize", "Compositionが検証または実行の契約に違反しています。");

    report.device.packageObjectsRebuilt = true;
    report.device.temporalHistoryReset = true;
    report.device.externalRebindRequired = true;
    return base::Success<WholeCompositionRecoveryReport, StaticRuntimeError>(
        std::move(report));
}

base::Expected<WholeCompositionRecoveryReport, StaticRuntimeError> RecoverStaticComposition(
    LoadedStaticComposition& loaded,
    ::sge4::runtime::DeviceRecoveryMode mode)
{
    return WholeCompositionRecovery::Recover(loaded, mode);
}
}
