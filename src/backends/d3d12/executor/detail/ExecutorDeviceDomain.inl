class DeviceDomain final : public runtime::IPackageDeviceDomain
{
public:
    explicit DeviceDomain(ExecutorOptions options) : options_(options) {}

    [[nodiscard]] std::uint64_t DeviceEpoch() const noexcept override { return epoch_; }
    [[nodiscard]] runtime::DeviceRuntimeState State() const noexcept override { return state_; }
    [[nodiscard]] IDXGIFactory6* Factory() const noexcept { return factory_.Get(); }
    [[nodiscard]] ID3D12Device* Device() const noexcept { return device_.Get(); }
    [[nodiscard]] const LUID& AdapterLuid() const noexcept { return adapterLuid_; }

    base::Expected<void, runtime::RuntimeError> Initialize()
    {
        const auto& diagnostics = ConfigureD3D12DiagnosticsOnce(options_.enableDebugLayer);
        UINT flags = 0;
#if defined(_DEBUG)
        if (options_.enableDebugLayer && diagnostics.debugLayerEnabled)
            flags |= DXGI_CREATE_FACTORY_DEBUG;
#else
        (void)diagnostics;
#endif
        factory_.Reset();
        device_.Reset();
        HRESULT hr = CreateDXGIFactory2(flags, IID_PPV_ARGS(&factory_));
        if (FAILED(hr))
            return base::Failure<void, runtime::RuntimeError>(
                HResultError("domain/create-factory", hr));

        ComPtr<IDXGIAdapter1> selectedAdapter;
        DXGI_ADAPTER_DESC1 selectedDescription{};
        HRESULT candidateFailure = DXGI_ERROR_NOT_FOUND;
        const auto accept = [&](ComPtr<IDXGIAdapter1> candidate)
        {
            if (!candidate) return false;
            DXGI_ADAPTER_DESC1 description{};
            candidateFailure = candidate->GetDesc1(&description);
            if (FAILED(candidateFailure)) return false;
            if (hasExcludedAdapterLuid_ && SameLuid(description.AdapterLuid, excludedAdapterLuid_))
            {
                candidateFailure = DXGI_ERROR_NOT_FOUND;
                return false;
            }
            ComPtr<ID3D12Device> candidateDevice;
            candidateFailure = D3D12CreateDevice(
                candidate.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&candidateDevice));
            if (FAILED(candidateFailure)) return false;
            selectedAdapter = std::move(candidate);
            selectedDescription = description;
            device_ = std::move(candidateDevice);
            return true;
        };

        if (options_.forceWarp)
        {
            ComPtr<IDXGIAdapter1> warp;
            hr = factory_->EnumWarpAdapter(IID_PPV_ARGS(&warp));
            if (FAILED(hr))
                return base::Failure<void, runtime::RuntimeError>(
                    HResultError("domain/warp-adapter", hr));
            (void)accept(std::move(warp));
        }
        else
        {
            for (UINT index = 0; ; ++index)
            {
                ComPtr<IDXGIAdapter1> candidate;
                hr = factory_->EnumAdapterByGpuPreference(
                    index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                    IID_PPV_ARGS(&candidate));
                if (hr == DXGI_ERROR_NOT_FOUND) break;
                if (FAILED(hr)) continue;
                DXGI_ADAPTER_DESC1 description{};
                if (FAILED(candidate->GetDesc1(&description)) ||
                    (description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE))
                    continue;
                if (accept(std::move(candidate))) break;
            }
            if (!selectedAdapter)
            {
                ComPtr<IDXGIAdapter1> warp;
                if (SUCCEEDED(factory_->EnumWarpAdapter(IID_PPV_ARGS(&warp))))
                    (void)accept(std::move(warp));
            }
        }

        if (!selectedAdapter || !device_)
            return base::Failure<void, runtime::RuntimeError>(
                Error("domain/no-eligible-adapter",
                    "検証または実行の契約に違反しています。"));
        adapterLuid_ = selectedDescription.AdapterLuid;
        state_ = runtime::DeviceRuntimeState::Active;
        return base::Success<void, runtime::RuntimeError>();
    }

    base::Expected<runtime::DeviceRecoveryReport, runtime::RuntimeError> Recover(
        runtime::DeviceRecoveryMode mode)
    {
        runtime::DeviceRecoveryReport report;
        report.previousDeviceEpoch = epoch_;
        report.newDeviceEpoch = epoch_;
        report.mode = mode;
        report.stateBefore = state_;
        report.stateAfter = state_;
        report.forcedRemoval = mode == runtime::DeviceRecoveryMode::ForceRemovalForTest;
        report.externalRebindRequired = true;

        if (mode == runtime::DeviceRecoveryMode::RetryAdapterReacquisition)
        {
            if (state_ != runtime::DeviceRuntimeState::AwaitingAdapter)
                return base::Failure<runtime::DeviceRecoveryReport, runtime::RuntimeError>(
                    Error("domain/recovery-retry", "検証または実行の契約に違反しています。"));
            auto rebuilt = Initialize();
            if (!rebuilt)
            {
                state_ = runtime::DeviceRuntimeState::AwaitingAdapter;
                report.stateAfter = state_;
                return base::Success<runtime::DeviceRecoveryReport, runtime::RuntimeError>(report);
            }
            ++epoch_;
            report.newDeviceEpoch = epoch_;
            report.stateAfter = state_;
            report.adapterReacquired = true;
            report.temporalHistoryReset = true;
            return base::Success<runtime::DeviceRecoveryReport, runtime::RuntimeError>(report);
        }

        if (state_ != runtime::DeviceRuntimeState::Active || !device_)
            return base::Failure<runtime::DeviceRecoveryReport, runtime::RuntimeError>(
                Error("domain/recovery", "Deviceが検証または実行の契約に違反しています。"));

        if (mode == runtime::DeviceRecoveryMode::ControlledRebuild)
        {
            const HRESULT reason = device_->GetDeviceRemovedReason();
            if (FAILED(reason))
                return base::Failure<runtime::DeviceRecoveryReport, runtime::RuntimeError>(
                    HResultError("domain/controlled-source-device", reason, device_.Get()));
        }
        else if (mode == runtime::DeviceRecoveryMode::ForceRemovalForTest)
        {
            ComPtr<ID3D12Device5> removable;
            const HRESULT query = device_.As(&removable);
            if (FAILED(query))
                return base::Failure<runtime::DeviceRecoveryReport, runtime::RuntimeError>(
                    HResultError("domain/recovery-query-device5", query, device_.Get()));
            removable->RemoveDevice();
            const HRESULT reason = device_->GetDeviceRemovedReason();
            report.removalReason = static_cast<std::int64_t>(reason);
            if (SUCCEEDED(reason))
                return base::Failure<runtime::DeviceRecoveryReport, runtime::RuntimeError>(
                    Error("domain/remove-device", "検証または実行の契約に違反しています。"));
        }
        else if (mode == runtime::DeviceRecoveryMode::RecoverDetectedLoss)
        {
            const HRESULT reason = device_->GetDeviceRemovedReason();
            if (SUCCEEDED(reason))
                return base::Failure<runtime::DeviceRecoveryReport, runtime::RuntimeError>(
                    Error("domain/recovery-detected", "検証または実行の契約に違反しています。"));
            report.removalReason = static_cast<std::int64_t>(reason);
        }
        else
            return base::Failure<runtime::DeviceRecoveryReport, runtime::RuntimeError>(
                Error("domain/recovery", "Deviceが検証または実行の契約に違反しています。"));

        if (mode != runtime::DeviceRecoveryMode::ControlledRebuild)
        {
            excludedAdapterLuid_ = adapterLuid_;
            hasExcludedAdapterLuid_ = true;
            report.removedAdapterLuidLow = adapterLuid_.LowPart;
            report.removedAdapterLuidHigh = adapterLuid_.HighPart;
        }

        state_ = runtime::DeviceRuntimeState::Lost;
        device_.Reset();
        factory_.Reset();
        auto rebuilt = Initialize();
        if (!rebuilt)
        {
            if (mode == runtime::DeviceRecoveryMode::ControlledRebuild)
                return base::Failure<runtime::DeviceRecoveryReport, runtime::RuntimeError>(
                    rebuilt.error());
            state_ = runtime::DeviceRuntimeState::AwaitingAdapter;
            report.stateAfter = state_;
            return base::Success<runtime::DeviceRecoveryReport, runtime::RuntimeError>(report);
        }

        ++epoch_;
        report.newDeviceEpoch = epoch_;
        report.stateAfter = state_;
        report.adapterReacquired = true;
        report.temporalHistoryReset = true;
        return base::Success<runtime::DeviceRecoveryReport, runtime::RuntimeError>(report);
    }

private:
    ExecutorOptions options_;
    std::uint64_t epoch_ = 1;
    runtime::DeviceRuntimeState state_ = runtime::DeviceRuntimeState::Active;
    LUID adapterLuid_{};
    LUID excludedAdapterLuid_{};
    bool hasExcludedAdapterLuid_ = false;
    ComPtr<IDXGIFactory6> factory_;
    ComPtr<ID3D12Device> device_;
};

