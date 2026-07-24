#include "RuntimeRecovery.h"

#include <limits>

namespace sge4_5::v2::runtime_recovery
{
namespace
{
canonical::ResourceInstanceIdentity MakeResourceInstanceIdentity(
    RuntimeInstanceIdentity runtimeIdentity,
    std::uint8_t kind)
{
    std::vector<std::byte> payload(runtimeIdentity.Digest().begin(), runtimeIdentity.Digest().end());
    payload.push_back(static_cast<std::byte>(kind));
    return canonical::MakeCanonicalIdentityV1<canonical::ResourceInstanceIdentity>(
        {"SGE.V2.Runtime.ResourceInstance.V1", RuntimeRecoverySchemaVersionV1, payload});
}

RecoveryReportV1 MakeReport(
    RuntimeInstanceIdentity previousRuntimeIdentity,
    RecoveryModeV1 mode,
    RuntimeStateV1 finalState,
    std::uint64_t previousEpoch,
    std::uint64_t newEpoch,
    AdapterIdentity previousAdapter,
    std::optional<AdapterIdentity> newAdapter,
    bool released,
    bool rematerialized)
{
    const auto identity = ComputeRecoveryOperationIdentityV1(
        previousRuntimeIdentity, mode, previousAdapter, newAdapter,
        previousEpoch, newEpoch, finalState);
    return {identity, mode, finalState, previousEpoch, newEpoch, previousAdapter,
        std::move(newAdapter), released, rematerialized,
        finalState == RuntimeStateV1::ActiveNeedsExternalRebind,
        finalState == RuntimeStateV1::ActiveNeedsExternalRebind};
}

}

RuntimeErrorV1 RuntimeRecoveryCoordinatorV1::RematerializeOnAdapter(LoadedRuntimeV1& loaded, const AdapterIdentity& adapter)
{
    if (!loaded.backend_->AcquireAdapter(adapter)) return RuntimeErrorV1::AdapterAcquireFailed;
    if (loaded.deviceEpoch_.Value() == std::numeric_limits<std::uint64_t>::max())
        return RuntimeErrorV1::MaterializationFailed;
    const auto nextEpoch = canonical::DeviceEpoch::TryCreate(loaded.deviceEpoch_.Value() + 1u);
    if (!nextEpoch) return RuntimeErrorV1::MaterializationFailed;
    NativeMaterializationRequestV1 request{
        loaded.composition_.Identity(), adapter, *nextEpoch,
        loaded.composition_.LeafCount(), loaded.composition_.FlowCount()};
    if (!loaded.backend_->Materialize(request)) return RuntimeErrorV1::MaterializationFailed;
    loaded.adapterIdentity_ = adapter;
    loaded.deviceEpoch_ = *nextEpoch;
    loaded.runtimeIdentity_ = ComputeRuntimeInstanceIdentityV1(loaded.composition_.Identity(), adapter, *nextEpoch);
    loaded.representationHandle_ = canonical::RepresentationHandleV1(
        MakeResourceInstanceIdentity(loaded.runtimeIdentity_, 1u), *nextEpoch);
    loaded.historyHandle_ = canonical::HistoryHandleV1(
        MakeResourceInstanceIdentity(loaded.runtimeIdentity_, 2u), *nextEpoch);
    loaded.externalBindings_.reset();
    loaded.acceptedHistoryIdentity_.reset();
    loaded.firstSubmissionRequiresRecoverySeed_ = true;
    loaded.state_ = RuntimeStateV1::ActiveNeedsExternalRebind;
    return RuntimeErrorV1::None;
}

RecoveryResultV1 RuntimeRecoveryCoordinatorV1::ControlledRebuild(LoadedRuntimeV1& loaded)
{
    if (loaded.state_ != RuntimeStateV1::Active && loaded.state_ != RuntimeStateV1::ActiveNeedsExternalRebind)
        return {RuntimeErrorV1::RuntimeNotActive, std::nullopt};
    const auto previousRuntime = loaded.runtimeIdentity_;
    const auto previousAdapter = loaded.adapterIdentity_;
    const auto previousEpoch = loaded.deviceEpoch_.Value();
    loaded.state_ = RuntimeStateV1::Lost;
    loaded.backend_->ReleaseAllRuntimeObjects();
    loaded.externalBindings_.reset();
    loaded.acceptedHistoryIdentity_.reset();
    const auto error = RematerializeOnAdapter(loaded, previousAdapter);
    if (error != RuntimeErrorV1::None)
    {
        loaded.state_ = RuntimeStateV1::Lost;
        return {error, std::nullopt};
    }
    return {RuntimeErrorV1::None, MakeReport(previousRuntime, RecoveryModeV1::ControlledRebuild,
        loaded.state_, previousEpoch, loaded.deviceEpoch_.Value(), previousAdapter,
        loaded.adapterIdentity_, true, true)};
}

RecoveryResultV1 RuntimeRecoveryCoordinatorV1::ForceRemovalForQualification(LoadedRuntimeV1& loaded)
{
    if (loaded.state_ != RuntimeStateV1::Active && loaded.state_ != RuntimeStateV1::ActiveNeedsExternalRebind)
        return {RuntimeErrorV1::RuntimeNotActive, std::nullopt};
    const auto previousRuntime = loaded.runtimeIdentity_;
    const auto previousAdapter = loaded.adapterIdentity_;
    const auto previousEpoch = loaded.deviceEpoch_.Value();
    if (!loaded.backend_->ForceRemoveCurrentAdapter())
        return {RuntimeErrorV1::RemovalFailed, std::nullopt};
    if (!ContainsExcludedAdapterV1(loaded.excludedAdapters_, previousAdapter))
        loaded.excludedAdapters_.push_back(previousAdapter);
    loaded.backend_->ReleaseAllRuntimeObjects();
    loaded.externalBindings_.reset();
    loaded.acceptedHistoryIdentity_.reset();
    loaded.firstSubmissionRequiresRecoverySeed_ = true;
    loaded.state_ = RuntimeStateV1::AwaitingAdapter;
    return {RuntimeErrorV1::None, MakeReport(previousRuntime,
        RecoveryModeV1::ForceRemovalForQualification, loaded.state_, previousEpoch,
        previousEpoch, previousAdapter, std::nullopt, true, false)};
}

RecoveryResultV1 RuntimeRecoveryCoordinatorV1::RetryAdapterReacquisition(
    LoadedRuntimeV1& loaded,
    std::span<const AdapterIdentity> explicitlyOrderedCandidates)
{
    if (loaded.state_ != RuntimeStateV1::AwaitingAdapter)
        return {RuntimeErrorV1::RetryNotAwaitingAdapter, std::nullopt};
    const auto previousRuntime = loaded.runtimeIdentity_;
    const auto previousAdapter = loaded.adapterIdentity_;
    const auto previousEpoch = loaded.deviceEpoch_.Value();
    const auto available = loaded.backend_->EnumerateAdapters();
    std::optional<AdapterIdentity> selected;
    for (const auto& candidate : explicitlyOrderedCandidates)
    {
        if (ContainsExcludedAdapterV1(loaded.excludedAdapters_, candidate)) continue;
        if (!ContainsAdapterIdentityV1(available, candidate)) continue;
        selected = candidate;
        break;
    }
    if (!selected.has_value())
    {
        auto report = MakeReport(previousRuntime, RecoveryModeV1::RetryAdapterReacquisition,
            RuntimeStateV1::AwaitingAdapter, previousEpoch, previousEpoch,
            previousAdapter, std::nullopt, false, false);
        return {RuntimeErrorV1::None, std::move(report)};
    }
    const auto error = RematerializeOnAdapter(loaded, *selected);
    if (error != RuntimeErrorV1::None) return {error, std::nullopt};
    return {RuntimeErrorV1::None, MakeReport(previousRuntime,
        RecoveryModeV1::RetryAdapterReacquisition, loaded.state_, previousEpoch,
        loaded.deviceEpoch_.Value(), previousAdapter, selected, false, true)};
}
}
