#pragma once

#include "../206_Level4V2RuntimeModel/RuntimeModel.h"

namespace sge4_5::v2::runtime_recovery
{
class RuntimeRecoveryCoordinatorV1;
struct RuntimeMaterializationResultV1;
struct RuntimeSubmissionResultV1;

class LoadedRuntimeV1 final
{
public:
    LoadedRuntimeV1(LoadedRuntimeV1&&) noexcept = default;
    LoadedRuntimeV1& operator=(LoadedRuntimeV1&&) noexcept = default;
    LoadedRuntimeV1(const LoadedRuntimeV1&) = delete;
    LoadedRuntimeV1& operator=(const LoadedRuntimeV1&) = delete;

    [[nodiscard]] RuntimeStateV1 State() const noexcept { return state_; }
    [[nodiscard]] canonical::DeviceEpoch Epoch() const noexcept { return deviceEpoch_; }
    [[nodiscard]] const AdapterIdentity& Adapter() const noexcept { return adapterIdentity_; }
    [[nodiscard]] const RuntimeInstanceIdentity& Identity() const noexcept { return runtimeIdentity_; }
    [[nodiscard]] const frozen_composition::OpaqueFrozenCompositionV1& Composition() const noexcept { return composition_; }
    [[nodiscard]] const std::vector<AdapterIdentity>& ExcludedAdapters() const noexcept { return excludedAdapters_; }
    [[nodiscard]] bool RequiresRecoverySeed() const noexcept { return firstSubmissionRequiresRecoverySeed_; }
    [[nodiscard]] bool ExternalStateBound() const noexcept { return externalBindings_.has_value(); }
    [[nodiscard]] std::optional<canonical::HistoryValidityIdentity> AcceptedHistoryIdentity() const noexcept { return acceptedHistoryIdentity_; }
    [[nodiscard]] const canonical::RepresentationHandleV1& RepresentationHandle() const noexcept { return representationHandle_; }
    [[nodiscard]] const canonical::HistoryHandleV1& HistoryHandle() const noexcept { return historyHandle_; }

private:
    friend struct RuntimeMaterializationResultV1;
    friend RuntimeMaterializationResultV1 MaterializeRuntimeV1(
        const frozen_composition::OpaqueFrozenCompositionV1&, const AdapterIdentity&, IRuntimeBackendV1&);
    friend RuntimeErrorV1 RebindExternalStateV1(LoadedRuntimeV1&, ExternalBindingSetV1);
    friend class RuntimeRecoveryCoordinatorV1;
    friend struct RuntimeSubmissionResultV1;
    friend RuntimeSubmissionResultV1 SubmitFrozenInvocationV1(
        LoadedRuntimeV1&, const frozen_dynamic::OpaqueFrozenDynamicInvocationV1&);

    LoadedRuntimeV1(
        frozen_composition::OpaqueFrozenCompositionV1 composition,
        AdapterIdentity adapterIdentity,
        canonical::DeviceEpoch deviceEpoch,
        RuntimeInstanceIdentity runtimeIdentity,
        canonical::RepresentationHandleV1 representationHandle,
        canonical::HistoryHandleV1 historyHandle,
        IRuntimeBackendV1& backend) noexcept
        : composition_(std::move(composition)), adapterIdentity_(std::move(adapterIdentity)),
          deviceEpoch_(deviceEpoch), runtimeIdentity_(std::move(runtimeIdentity)),
          representationHandle_(std::move(representationHandle)), historyHandle_(std::move(historyHandle)),
          backend_(&backend) {}

    frozen_composition::OpaqueFrozenCompositionV1 composition_;
    AdapterIdentity adapterIdentity_;
    canonical::DeviceEpoch deviceEpoch_;
    RuntimeInstanceIdentity runtimeIdentity_;
    RuntimeStateV1 state_ = RuntimeStateV1::ActiveNeedsExternalRebind;
    canonical::RepresentationHandleV1 representationHandle_;
    canonical::HistoryHandleV1 historyHandle_;
    std::optional<ExternalBindingSetV1> externalBindings_;
    std::optional<canonical::HistoryValidityIdentity> acceptedHistoryIdentity_;
    std::vector<AdapterIdentity> excludedAdapters_;
    bool firstSubmissionRequiresRecoverySeed_ = false;
    IRuntimeBackendV1* backend_ = nullptr;
};

struct RuntimeMaterializationResultV1 final
{
    RuntimeErrorV1 error = RuntimeErrorV1::None;
    std::optional<LoadedRuntimeV1> loaded;
    [[nodiscard]] bool Accepted() const noexcept { return error == RuntimeErrorV1::None && loaded.has_value(); }
};

[[nodiscard]] RuntimeMaterializationResultV1 MaterializeRuntimeV1(
    const frozen_composition::OpaqueFrozenCompositionV1& composition,
    const AdapterIdentity& explicitAdapter,
    IRuntimeBackendV1& backend);
[[nodiscard]] RuntimeErrorV1 RebindExternalStateV1(
    LoadedRuntimeV1& loaded,
    ExternalBindingSetV1 bindings);
[[nodiscard]] RuntimeErrorV1 ValidateRuntimeHandleEpochV1(
    const LoadedRuntimeV1& loaded,
    const canonical::RepresentationHandleV1& handle) noexcept;
[[nodiscard]] RuntimeErrorV1 ValidateRuntimeHandleEpochV1(
    const LoadedRuntimeV1& loaded,
    const canonical::HistoryHandleV1& handle) noexcept;
[[nodiscard]] RuntimeErrorV1 ValidateRuntimeHandleEpochV1(
    const LoadedRuntimeV1& loaded,
    const canonical::CompletionHandleV1& handle) noexcept;
[[nodiscard]] RuntimeErrorV1 ValidateRuntimeHandleEpochV1(
    const LoadedRuntimeV1& loaded,
    const canonical::SubmissionHandleV1& handle) noexcept;
}
