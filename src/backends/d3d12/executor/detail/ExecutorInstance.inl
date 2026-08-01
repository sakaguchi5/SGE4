class Instance final : public runtime::IPackageInstance
{
    friend class sge4::d3d12::Executor;
public:
    Instance(std::shared_ptr<const package::FrozenExecutablePackage> package, pkg::D3D12PackageView view,
        runtime::ISurfaceHost* surface, ExecutorOptions options,
        std::shared_ptr<detail::TimestampProfileCollector> profileCollector,
        DeviceDomain* domain = nullptr)
        : package_(std::move(package)), view_(std::move(view)), surface_(surface), options_(options),
          domain_(domain), profileCollector_(std::move(profileCollector))
    {
        if (options_.enableTimestampProfiling)
        {
            profileRecord_ = std::make_shared<detail::TimestampProfileRecord>();
            profileRecord_->packageExecutionDigest = package_->ExecutionDigest();
            std::scoped_lock lock(profileCollector_->mutex);
            profileRecord_->instanceOrdinal = profileCollector_->nextInstanceOrdinal++;
            profileCollector_->records.push_back(profileRecord_);
        }
    }

    ~Instance() override
    {
        if (runtimeState_ == runtime::DeviceRuntimeState::Active)
            for (const auto& queue : queues_)
                WaitForQueueAtDestruction(queue.id, queue.lastFenceValue, queue.fenceEvent);
        ReleaseDeviceObjectsForRecovery();
    }

    [[nodiscard]] const void* ExternalOwner() const noexcept
    {
        return domain_ ? static_cast<const void*>(domain_) : static_cast<const void*>(this);
    }

    base::Expected<void, runtime::RuntimeError> Initialize()
    {
        if (domain_)
        {
            if (domain_->State() != runtime::DeviceRuntimeState::Active)
                return base::Failure<void, runtime::RuntimeError>(
                    Error("domain/load", "Deviceが検証または実行の契約に違反しています。"));
            deviceEpoch_ = domain_->DeviceEpoch();
            runtimeState_ = runtime::DeviceRuntimeState::Active;
        }
        auto baseObjects = CreateBaseObjects();
        if (!baseObjects) return baseObjects;
        auto timestampObjects = CreateTimestampProfileObjects();
        if (!timestampObjects) return timestampObjects;
        resourceStates_.resize(view_.Resources().size());
        resources_.resize(view_.Resources().size());
        externalNativeResources_.resize(view_.Resources().size());
        externalBindings_.resize(view_.ExternalSlots().size());
        externalAcquired_.resize(view_.ExternalSlots().size());
        externalWaited_.resize(view_.ExternalSlots().size());
        externalReleased_.resize(view_.ExternalSlots().size());
        surfaceAcquired_.resize(view_.SurfaceSlots().size());
        surfacePresented_.resize(view_.SurfaceSlots().size());
        temporalWaitedResources_.resize(view_.Resources().size());
        placedHeaps_.resize(view_.Allocations().size());
        std::uint32_t aliasGroupCount = 0;
        for (const auto& allocation : view_.Allocations())
            if (allocation.aliasGroup != package::InvalidIndex) aliasGroupCount = std::max(aliasGroupCount, allocation.aliasGroup + 1u);
        activeAliasResources_.assign(aliasGroupCount, package::InvalidIndex);
        for (const auto& resource : view_.Resources())
        {
            const std::uint32_t stateCount = resource.origin == pkg::ResourceOrigin::Surface
                ? view_.Profile().surfaceImageCount
                : std::max(1u, resource.physicalInstanceCount);
            resourceStates_[resource.id.value].assign(stateCount, resource.initialState);
            if (resource.origin == pkg::ResourceOrigin::PackageOwned)
                resources_[resource.id.value].resize(resource.physicalInstanceCount);
        }
        for (auto& queue : queues_)
            queue.frameSlotFenceValues.assign(view_.Profile().framesInFlight, 0);
        rootSignatures_.resize(view_.BindingLayouts().size());
        pipelineStates_.resize(view_.Executables().size());
        computePipelineStates_.resize(view_.ComputeExecutables().size());
        dynamicBindings_.resize(view_.DynamicSlots().size());
        dynamicApplied_.resize(view_.DynamicSlots().size(), false);
        indirectArgumentBuffers_.resize(view_.Profile().framesInFlight);

        for (const auto& operation : view_.LoadOperations())
        {
            auto result = ExecuteLoadOperation(operation);
            if (!result) return result;
        }
        if (!descriptorHeapsCreated_)
            return base::Failure<void, runtime::RuntimeError>(Error("load", "Operationが検証または実行の契約に違反しています。"));
        if (hasLoadQueueBatch_ && !loadQueueCompleted_)
            return base::Failure<void, runtime::RuntimeError>(
                Error("load", "検証または実行の契約に違反しています。"));
        if (!hasLoadQueueBatch_ && (loadBatchOpen_ || loadBatchClosed_ || loadQueueCompleted_))
            return base::Failure<void, runtime::RuntimeError>(
                Error("load", "検証または実行の契約に違反しています。"));
        auto verified = CompleteBufferVerifications();
        if (!verified) return verified;
        auto textureVerified = CompleteTextureVerifications();
        if (!textureVerified) return textureVerified;
        uploadResources_.clear();
        return base::Success<void, runtime::RuntimeError>();
    }

    base::Expected<ExternalBufferBinding, runtime::RuntimeError> CreateExternalBuffer(
        std::uint32_t slot,
        std::span<const std::byte> initialBytes)
    {
        if (runtimeState_ != runtime::DeviceRuntimeState::Active || !device_)
            return base::Failure<ExternalBufferBinding, runtime::RuntimeError>(
                Error("external/device-state", "検証または実行の契約に違反しています。"));
        if (slot >= view_.ExternalSlots().size())
            return base::Failure<ExternalBufferBinding, runtime::RuntimeError>(
                Error("external/slot", "Packageが検証または実行の契約に違反しています。"));
        const auto& contract = view_.ExternalSlots()[slot];
        if (!contract.resource.IsValid() || contract.resource.value >= view_.Resources().size())
            return base::Failure<ExternalBufferBinding, runtime::RuntimeError>(
                Error("external/slot", "Packageが検証または実行の契約に違反しています。"));
        const auto& artifact = view_.Resources()[contract.resource.value];
        if (artifact.resourceKind != pkg::ResourceKind::Buffer || contract.minimumBytes == 0 ||
            initialBytes.size() > contract.minimumBytes)
            return base::Failure<ExternalBufferBinding, runtime::RuntimeError>(
                Error("external/shape", "Bufferが検証または実行の契約に違反しています。"));

        const auto incomingState = ToNativeState(contract.requiredIncomingState, D3D12_COMMAND_LIST_TYPE_DIRECT);
        bool requiresUnorderedAccess = false;
        const std::uint64_t viewEnd = static_cast<std::uint64_t>(artifact.firstView) + artifact.viewCount;
        if (viewEnd > view_.Views().size())
            return base::Failure<ExternalBufferBinding, runtime::RuntimeError>(
                Error("external/views", "Bufferが検証または実行の契約に違反しています。"));
        for (std::uint32_t index = artifact.firstView; index < viewEnd; ++index)
            if (view_.Views()[index].viewClass == pkg::ViewClass::UnorderedAccess)
                requiresUnorderedAccess = true;

        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = contract.minimumBytes;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        desc.Flags = requiresUnorderedAccess ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
                                             : D3D12_RESOURCE_FLAG_NONE;

        D3D12_HEAP_PROPERTIES defaultHeap{};
        defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
        defaultHeap.CreationNodeMask = 1;
        defaultHeap.VisibleNodeMask = 1;
        ComPtr<ID3D12Resource> resource;
        HRESULT hr = device_->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&resource));
        if (FAILED(hr))
            return base::Failure<ExternalBufferBinding, runtime::RuntimeError>(
                HResultError("external/create-buffer", hr, device_.Get()));

        D3D12_HEAP_PROPERTIES uploadHeap{};
        uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
        uploadHeap.CreationNodeMask = 1;
        uploadHeap.VisibleNodeMask = 1;
        D3D12_RESOURCE_DESC uploadDesc = desc;
        uploadDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        ComPtr<ID3D12Resource> upload;
        hr = device_->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &uploadDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&upload));
        if (FAILED(hr))
            return base::Failure<ExternalBufferBinding, runtime::RuntimeError>(
                HResultError("external/create-upload", hr, device_.Get()));
        void* mapped = nullptr;
        D3D12_RANGE noRead{0, 0};
        hr = upload->Map(0, &noRead, &mapped);
        if (FAILED(hr))
            return base::Failure<ExternalBufferBinding, runtime::RuntimeError>(
                HResultError("external/map-upload", hr, device_.Get()));
        std::memset(mapped, 0, static_cast<std::size_t>(contract.minimumBytes));
        if (!initialBytes.empty())
            std::memcpy(mapped, initialBytes.data(), initialBytes.size());
        D3D12_RANGE written{0, static_cast<SIZE_T>(contract.minimumBytes)};
        upload->Unmap(0, &written);

        ComPtr<ID3D12CommandAllocator> producerAllocator;
        ComPtr<ID3D12GraphicsCommandList> producerList;
        hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&producerAllocator));
        if (FAILED(hr))
            return base::Failure<ExternalBufferBinding, runtime::RuntimeError>(
                HResultError("external/create-allocator", hr, device_.Get()));
        hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, producerAllocator.Get(), nullptr,
            IID_PPV_ARGS(&producerList));
        if (FAILED(hr))
            return base::Failure<ExternalBufferBinding, runtime::RuntimeError>(
                HResultError("external/create-command-list", hr, device_.Get()));
        const auto toCopyDestination = TransitionBarrier(resource.Get(), D3D12_RESOURCE_STATE_COMMON,
                                                          D3D12_RESOURCE_STATE_COPY_DEST);
        producerList->ResourceBarrier(1, &toCopyDestination);
        producerList->CopyBufferRegion(resource.Get(), 0, upload.Get(), 0, contract.minimumBytes);
        if (incomingState != D3D12_RESOURCE_STATE_COPY_DEST)
        {
            const auto toIncoming = TransitionBarrier(resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, incomingState);
            producerList->ResourceBarrier(1, &toIncoming);
        }
        hr = producerList->Close();
        if (FAILED(hr))
            return base::Failure<ExternalBufferBinding, runtime::RuntimeError>(
                HResultError("external/close-command-list", hr, device_.Get()));
        ID3D12CommandList* lists[] = {producerList.Get()};
        NativeQueue(DirectQueueId())->ExecuteCommandLists(1, lists);

        ComPtr<ID3D12Fence> producerFence;
        hr = device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&producerFence));
        if (FAILED(hr))
            return base::Failure<ExternalBufferBinding, runtime::RuntimeError>(
                HResultError("external/create-producer-fence", hr, device_.Get()));
        hr = NativeQueue(DirectQueueId())->Signal(producerFence.Get(), 1);
        if (FAILED(hr))
            return base::Failure<ExternalBufferBinding, runtime::RuntimeError>(
                HResultError("external/signal-producer", hr, device_.Get()));
        HANDLE producerEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!producerEvent)
            return base::Failure<ExternalBufferBinding, runtime::RuntimeError>(
                Error("external/create-event", "入力または内部状態が検証または実行の契約に違反しています。"));
        if (producerFence->GetCompletedValue() < 1)
        {
            hr = producerFence->SetEventOnCompletion(1, producerEvent);
            if (FAILED(hr))
            {
                CloseHandle(producerEvent);
                return base::Failure<ExternalBufferBinding, runtime::RuntimeError>(
                    HResultError("external/set-producer-event", hr, device_.Get()));
            }
            WaitForSingleObject(producerEvent, INFINITE);
        }
        CloseHandle(producerEvent);

        TrackedState(contract.resource) = contract.requiredIncomingState;
        ExternalBufferBinding result;
        result.resource = std::make_shared<ExternalBufferResource>(resource, ExternalOwner(), deviceEpoch_,
            contract.minimumBytes, slot, contract.requiredIncomingState, contract.guaranteedOutgoingState);
        result.availableAfter = std::make_shared<CompletionToken>(producerFence, 1, deviceEpoch_, ExternalOwner(), slot);
        return base::Success<ExternalBufferBinding, runtime::RuntimeError>(std::move(result));
    }

    base::Expected<ExternalBufferReadback, runtime::RuntimeError> ReadExternalBuffer(
        const std::shared_ptr<runtime::IExternalResource>& resource,
        const std::shared_ptr<runtime::ICompletionToken>& safeAfter)
    {
        if (runtimeState_ != runtime::DeviceRuntimeState::Active || !device_)
            return base::Failure<ExternalBufferReadback, runtime::RuntimeError>(
                Error("external/readback-device-state", "Packageが検証または実行の契約に違反しています。"));
        auto* native = dynamic_cast<ExternalBufferResource*>(resource.get());
        auto* token = dynamic_cast<CompletionToken*>(safeAfter.get());
        if (!native || !token || native->Owner() != ExternalOwner() || token->Owner() != ExternalOwner() ||
            native->DeviceEpoch() != deviceEpoch_ || token->DeviceEpoch() != deviceEpoch_ ||
            token->Slot() != native->Slot())
            return base::Failure<ExternalBufferReadback, runtime::RuntimeError>(
                Error("external/readback-owner", "Resourceが検証または実行の契約に違反しています。"));
        if (native->Slot() >= view_.ExternalSlots().size())
            return base::Failure<ExternalBufferReadback, runtime::RuntimeError>(
                Error("external/readback-slot", "検証または実行の契約に違反しています。"));
        const auto& contract = view_.ExternalSlots()[native->Slot()];
        if (native->CurrentState() != contract.guaranteedOutgoingState)
            return base::Failure<ExternalBufferReadback, runtime::RuntimeError>(
                Error("external/readback-state", "検証または実行の契約に違反しています。"));

        HANDLE sourceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!sourceEvent)
            return base::Failure<ExternalBufferReadback, runtime::RuntimeError>(
                Error("external/readback-event", "入力または内部状態が検証または実行の契約に違反しています。"));
        HRESULT hr = S_OK;
        if (token->NativeFence()->GetCompletedValue() < token->Value())
        {
            hr = token->NativeFence()->SetEventOnCompletion(token->Value(), sourceEvent);
            if (FAILED(hr))
            {
                CloseHandle(sourceEvent);
                return base::Failure<ExternalBufferReadback, runtime::RuntimeError>(
                    HResultError("external/readback-set-source-event", hr, device_.Get()));
            }
            WaitForSingleObject(sourceEvent, INFINITE);
        }
        CloseHandle(sourceEvent);

        D3D12_RESOURCE_DESC readbackDesc{};
        readbackDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        readbackDesc.Width = native->SizeBytes();
        readbackDesc.Height = 1;
        readbackDesc.DepthOrArraySize = 1;
        readbackDesc.MipLevels = 1;
        readbackDesc.SampleDesc.Count = 1;
        readbackDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        D3D12_HEAP_PROPERTIES readbackHeap{};
        readbackHeap.Type = D3D12_HEAP_TYPE_READBACK;
        readbackHeap.CreationNodeMask = 1;
        readbackHeap.VisibleNodeMask = 1;
        ComPtr<ID3D12Resource> readback;
        hr = device_->CreateCommittedResource(&readbackHeap, D3D12_HEAP_FLAG_NONE, &readbackDesc,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback));
        if (FAILED(hr))
            return base::Failure<ExternalBufferReadback, runtime::RuntimeError>(
                HResultError("external/readback-create-buffer", hr, device_.Get()));

        ComPtr<ID3D12CommandAllocator> allocator;
        ComPtr<ID3D12GraphicsCommandList> list;
        hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator));
        if (FAILED(hr))
            return base::Failure<ExternalBufferReadback, runtime::RuntimeError>(
                HResultError("external/readback-create-allocator", hr, device_.Get()));
        hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr,
            IID_PPV_ARGS(&list));
        if (FAILED(hr))
            return base::Failure<ExternalBufferReadback, runtime::RuntimeError>(
                HResultError("external/readback-create-command-list", hr, device_.Get()));

        const auto outgoingState = ToNativeState(contract.guaranteedOutgoingState, D3D12_COMMAND_LIST_TYPE_DIRECT);
        const auto incomingState = ToNativeState(contract.requiredIncomingState, D3D12_COMMAND_LIST_TYPE_DIRECT);
        if (outgoingState != D3D12_RESOURCE_STATE_COPY_SOURCE)
        {
            const auto toSource = TransitionBarrier(native->Native(), outgoingState, D3D12_RESOURCE_STATE_COPY_SOURCE);
            list->ResourceBarrier(1, &toSource);
        }
        list->CopyBufferRegion(readback.Get(), 0, native->Native(), 0, native->SizeBytes());
        if (incomingState != D3D12_RESOURCE_STATE_COPY_SOURCE)
        {
            const auto toIncoming = TransitionBarrier(native->Native(), D3D12_RESOURCE_STATE_COPY_SOURCE, incomingState);
            list->ResourceBarrier(1, &toIncoming);
        }
        hr = list->Close();
        if (FAILED(hr))
            return base::Failure<ExternalBufferReadback, runtime::RuntimeError>(
                HResultError("external/readback-close-command-list", hr, device_.Get()));
        ID3D12CommandList* lists[] = {list.Get()};
        NativeQueue(DirectQueueId())->ExecuteCommandLists(1, lists);

        ComPtr<ID3D12Fence> readbackFence;
        hr = device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&readbackFence));
        if (FAILED(hr))
            return base::Failure<ExternalBufferReadback, runtime::RuntimeError>(
                HResultError("external/readback-create-fence", hr, device_.Get()));
        hr = NativeQueue(DirectQueueId())->Signal(readbackFence.Get(), 1);
        if (FAILED(hr))
            return base::Failure<ExternalBufferReadback, runtime::RuntimeError>(
                HResultError("external/readback-signal", hr, device_.Get()));
        HANDLE readbackEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!readbackEvent)
            return base::Failure<ExternalBufferReadback, runtime::RuntimeError>(
                Error("external/readback-create-event", "入力または内部状態が検証または実行の契約に違反しています。"));
        if (readbackFence->GetCompletedValue() < 1)
        {
            hr = readbackFence->SetEventOnCompletion(1, readbackEvent);
            if (FAILED(hr))
            {
                CloseHandle(readbackEvent);
                return base::Failure<ExternalBufferReadback, runtime::RuntimeError>(
                    HResultError("external/readback-set-event", hr, device_.Get()));
            }
            WaitForSingleObject(readbackEvent, INFINITE);
        }
        CloseHandle(readbackEvent);

        ExternalBufferReadback result;
        result.bytes.resize(static_cast<std::size_t>(native->SizeBytes()));
        void* mapped = nullptr;
        D3D12_RANGE readRange{0, static_cast<SIZE_T>(native->SizeBytes())};
        hr = readback->Map(0, &readRange, &mapped);
        if (FAILED(hr))
            return base::Failure<ExternalBufferReadback, runtime::RuntimeError>(
                HResultError("external/readback-map", hr, device_.Get()));
        std::memcpy(result.bytes.data(), mapped, result.bytes.size());
        D3D12_RANGE noWrite{0, 0};
        readback->Unmap(0, &noWrite);

        native->SetCurrentState(contract.requiredIncomingState);
        TrackedState(contract.resource) = contract.requiredIncomingState;
        result.availableAfter = std::make_shared<CompletionToken>(readbackFence, 1, deviceEpoch_, ExternalOwner(), native->Slot());
        return base::Success<ExternalBufferReadback, runtime::RuntimeError>(std::move(result));
    }

    base::Expected<ExternalBufferBinding, runtime::RuntimeError> CreateExternalColorBuffer(
        const std::array<float, 4>& color)
    {
        return CreateExternalBuffer(0, std::as_bytes(std::span<const float>(color)));
    }

    base::Expected<runtime::DeviceRecoveryReport, runtime::RuntimeError> RecoverDevice(runtime::DeviceRecoveryMode mode)
    {
        runtime::DeviceRecoveryReport report;
        report.previousDeviceEpoch = deviceEpoch_;
        report.newDeviceEpoch = deviceEpoch_;
        report.mode = mode;
        report.stateBefore = runtimeState_;
        report.stateAfter = runtimeState_;
        report.forcedRemoval = mode == runtime::DeviceRecoveryMode::ForceRemovalForTest;
        report.externalRebindRequired = !view_.ExternalSlots().empty();

        if (mode == runtime::DeviceRecoveryMode::RetryAdapterReacquisition)
        {
            if (runtimeState_ != runtime::DeviceRuntimeState::AwaitingAdapter)
                return base::Failure<runtime::DeviceRecoveryReport, runtime::RuntimeError>(
                    Error("recovery/retry", "検証または実行の契約に違反しています。"));

            auto rebuilt = Initialize();
            if (!rebuilt)
            {
                if (rebuilt.error().stage == "device/no-eligible-adapter")
                {
                    report.stateAfter = runtime::DeviceRuntimeState::AwaitingAdapter;
                    return base::Success<runtime::DeviceRecoveryReport, runtime::RuntimeError>(report);
                }
                return base::Failure<runtime::DeviceRecoveryReport, runtime::RuntimeError>(
                    Error("recovery/retry", rebuilt.error().stage + "：" + rebuilt.error().message));
            }

            ++deviceEpoch_;
            runtimeState_ = runtime::DeviceRuntimeState::Active;
            report.newDeviceEpoch = deviceEpoch_;
            report.stateAfter = runtimeState_;
            report.adapterReacquired = true;
            report.packageObjectsRebuilt = true;
            report.temporalHistoryReset = true;
            return base::Success<runtime::DeviceRecoveryReport, runtime::RuntimeError>(report);
        }

        if (runtimeState_ != runtime::DeviceRuntimeState::Active || !device_)
            return base::Failure<runtime::DeviceRecoveryReport, runtime::RuntimeError>(
                Error("recovery", "Deviceが検証または実行の契約に違反しています。"));

        if (mode == runtime::DeviceRecoveryMode::ControlledRebuild)
        {
            for (const auto& queue : queues_)
            {
                auto waited = WaitForQueueFence(queue.id, queue.lastFenceValue);
                if (!waited) return base::Failure<runtime::DeviceRecoveryReport, runtime::RuntimeError>(waited.error());
            }
            const HRESULT sourceDeviceReason = device_->GetDeviceRemovedReason();
            if (FAILED(sourceDeviceReason))
                return base::Failure<runtime::DeviceRecoveryReport, runtime::RuntimeError>(
                    Error("recovery/controlled-source-device",
                        "Packageが検証または実行の契約に違反しています。" +
                        HResultText(sourceDeviceReason)));

            ReleaseDeviceObjectsForRecovery();
            auto rebuilt = Initialize();
            if (!rebuilt)
                return base::Failure<runtime::DeviceRecoveryReport, runtime::RuntimeError>(
                    Error("recovery/controlled-rebuild", rebuilt.error().stage + "：" + rebuilt.error().message));

            ++deviceEpoch_;
            runtimeState_ = runtime::DeviceRuntimeState::Active;
            report.newDeviceEpoch = deviceEpoch_;
            report.stateAfter = runtimeState_;
            report.adapterReacquired = true;
            report.packageObjectsRebuilt = true;
            report.temporalHistoryReset = true;
            return base::Success<runtime::DeviceRecoveryReport, runtime::RuntimeError>(report);
        }

        const bool forceRemoval = mode == runtime::DeviceRecoveryMode::ForceRemovalForTest;
        const bool recoverDetectedLoss = mode == runtime::DeviceRecoveryMode::RecoverDetectedLoss;
        if (!forceRemoval && !recoverDetectedLoss)
            return base::Failure<runtime::DeviceRecoveryReport, runtime::RuntimeError>(
                Error("recovery", "入力または内部状態が検証または実行の契約に違反しています。"));

        if (forceRemoval)
        {
            ComPtr<ID3D12Device5> removable;
            const HRESULT query = device_.As(&removable);
            if (FAILED(query))
                return base::Failure<runtime::DeviceRecoveryReport, runtime::RuntimeError>(
                    HResultError("recovery/query-device5", query, device_.Get()));
            removable->RemoveDevice();
        }

        const HRESULT reason = device_->GetDeviceRemovedReason();
        report.removalReason = static_cast<std::int64_t>(reason);
        if (SUCCEEDED(reason))
            return base::Failure<runtime::DeviceRecoveryReport, runtime::RuntimeError>(
                Error(forceRemoval ? "recovery/remove-device" : "recovery/detected-loss",
                    forceRemoval
                        ? "検証または実行の契約に違反しています。"
                        : "Deviceが検証または実行の契約に違反しています。"));
        runtimeState_ = runtime::DeviceRuntimeState::Lost;
        if (hasActiveAdapterLuid_)
        {
            excludedAdapterLuid_ = activeAdapterLuid_;
            hasExcludedAdapterLuid_ = true;
            report.removedAdapterLuidLow = activeAdapterLuid_.LowPart;
            report.removedAdapterLuidHigh = activeAdapterLuid_.HighPart;
        }

        ComPtr<ID3D12DeviceRemovedExtendedData> dred;
        if (SUCCEEDED(device_.As(&dred)))
        {
            D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT breadcrumbs{};
            if (SUCCEEDED(dred->GetAutoBreadcrumbsOutput(&breadcrumbs)))
                for (auto* node = breadcrumbs.pHeadAutoBreadcrumbNode; node; node = node->pNext)
                    ++report.dredBreadcrumbNodeCount;

            D3D12_DRED_PAGE_FAULT_OUTPUT pageFault{};
            if (SUCCEEDED(dred->GetPageFaultAllocationOutput(&pageFault)))
            {
                for (auto* node = pageFault.pHeadExistingAllocationNode; node; node = node->pNext)
                    ++report.dredPageFaultAllocationCount;
                for (auto* node = pageFault.pHeadRecentFreedAllocationNode; node; node = node->pNext)
                    ++report.dredPageFaultAllocationCount;
            }
        }

        ReleaseDeviceObjectsForRecovery();
        runtimeState_ = runtime::DeviceRuntimeState::AwaitingAdapter;

        auto rebuilt = Initialize();
        if (!rebuilt)
        {
            if (rebuilt.error().stage == "device/no-eligible-adapter")
            {
                report.stateAfter = runtimeState_;
                return base::Success<runtime::DeviceRecoveryReport, runtime::RuntimeError>(report);
            }
            return base::Failure<runtime::DeviceRecoveryReport, runtime::RuntimeError>(
                Error("recovery/reacquire-adapter", rebuilt.error().stage + "：" + rebuilt.error().message));
        }

        ++deviceEpoch_;
        runtimeState_ = runtime::DeviceRuntimeState::Active;
        report.newDeviceEpoch = deviceEpoch_;
        report.stateAfter = runtimeState_;
        report.adapterReacquired = true;
        report.packageObjectsRebuilt = true;
        report.temporalHistoryReset = true;
        return base::Success<runtime::DeviceRecoveryReport, runtime::RuntimeError>(report);
    }

    base::Expected<runtime::FrameSubmission, runtime::RuntimeError> Submit(const runtime::FrameInvocation& invocation)
    {
        if (runtimeState_ != runtime::DeviceRuntimeState::Active || !device_)
            return base::Failure<runtime::FrameSubmission, runtime::RuntimeError>(
                Error("submit/device-state", "Package instanceがActiveではありません。Adapterの再取得が必要です。"));
        const bool hasTemporalResources = std::any_of(view_.Resources().begin(), view_.Resources().end(),
            [&](const pkg::ResourceArtifact& resource) { return IsTemporal(resource); });
        if (hasTemporalResources && ((!hasSubmittedFrame_ && invocation.frameNumber != 0) ||
            (hasSubmittedFrame_ && invocation.frameNumber != lastSubmittedFrameNumber_ + 1u)))
            return base::Failure<runtime::FrameSubmission, runtime::RuntimeError>(
                Error("invocation", "入力または内部状態の数値条件を満たしていません。"));

        currentFrameSlot_ = static_cast<std::uint32_t>(invocation.frameNumber % view_.Profile().framesInFlight);
        std::uint64_t reusedSlotFenceValue = 0;
        for (auto& queue : queues_)
        {
            if (currentFrameSlot_ >= queue.frameSlotFenceValues.size())
                return base::Failure<runtime::FrameSubmission, runtime::RuntimeError>(
                    Error("submit", "検証または実行の契約に違反しています。"));
            const auto reused = queue.frameSlotFenceValues[currentFrameSlot_];
            reusedSlotFenceValue = std::max(reusedSlotFenceValue, reused);
            auto waited = WaitForQueueFence(queue.id, reused);
            if (!waited) return base::Failure<runtime::FrameSubmission, runtime::RuntimeError>(waited.error());
        }

        auto prepared = PrepareInvocation(invocation);
        if (!prepared) return base::Failure<runtime::FrameSubmission, runtime::RuntimeError>(prepared.error());

        temporalPreviousInstance_ = (currentFrameSlot_ + view_.Profile().framesInFlight - 1u) % view_.Profile().framesInFlight;
        temporalCurrentInstance_ = currentFrameSlot_;
        temporalDependencyFenceValue_ = 0;
        currentFrameSignalPoints_.clear();
        for (auto& queue : queues_)
        {
            queue.frameFenceValue = 0;
            queue.frameBatchCursor = 0;
            queue.frameSubmitted = false;
            queue.commandOpen = false;
        }
        allocator_.Reset();
        commandList_.Reset();
        commandOpen_ = false;
        activeCommandQueue_ = package::InvalidIndex;
        std::fill(externalAcquired_.begin(), externalAcquired_.end(), false);
        std::fill(externalWaited_.begin(), externalWaited_.end(), false);
        std::fill(externalReleased_.begin(), externalReleased_.end(), false);
        std::fill(surfaceAcquired_.begin(), surfaceAcquired_.end(), false);
        std::fill(surfacePresented_.begin(), surfacePresented_.end(), false);
        std::fill(temporalWaitedResources_.begin(), temporalWaitedResources_.end(), false);
        frameExternalReleases_.clear();
        std::fill(dynamicApplied_.begin(), dynamicApplied_.end(), false);
        timestampQueryIssued_ = false;
        timestampQueryResolved_ = false;
        if (profileRecord_) profileRecord_->ready = false;

        for (const auto& operation : view_.FrameOperations())
        {
            auto result = ExecuteFrameOperation(operation);
            if (!result) return base::Failure<runtime::FrameSubmission, runtime::RuntimeError>(result.error());
        }

        if (std::any_of(queues_.begin(), queues_.end(), [](const QueueRuntimeState& queue) { return queue.commandOpen; }))
            return base::Failure<runtime::FrameSubmission, runtime::RuntimeError>(
                Error("frame", "検証または実行の契約に違反しています。"));
        if (std::any_of(queues_.begin(), queues_.end(), [](const QueueRuntimeState& queue) { return queue.frameSubmitted; }))
            return base::Failure<runtime::FrameSubmission, runtime::RuntimeError>(
                Error("frame", "検証または実行の契約に違反しています。"));
        if (std::find(externalReleased_.begin(), externalReleased_.end(), false) != externalReleased_.end())
            return base::Failure<runtime::FrameSubmission, runtime::RuntimeError>(
                Error("frame", "検証または実行の契約に違反しています。"));
        if (std::find(surfacePresented_.begin(), surfacePresented_.end(), false) != surfacePresented_.end())
            return base::Failure<runtime::FrameSubmission, runtime::RuntimeError>(
                Error("frame", "検証または実行の契約に違反しています。"));
        if (options_.enableTimestampProfiling && (!timestampQueryIssued_ || !timestampQueryResolved_))
            return base::Failure<runtime::FrameSubmission, runtime::RuntimeError>(
                Error("profile/frame", "検証または実行の契約に違反しています。"));
        if (indirectDispatchPresent_ && !indirectDispatchApplied_)
            return base::Failure<runtime::FrameSubmission, runtime::RuntimeError>(
                Error("frame/indirect-dispatch",
                    "Seal済みVerified indirect dispatchが対象Compute Commandへ適用されませんでした。"));

        runtime::FrameSubmission submission;
        submission.deviceEpoch = deviceEpoch_;
        submission.frameSlot = currentFrameSlot_;
        submission.framesInFlight = view_.Profile().framesInFlight;
        submission.reusedSlotFenceValue = reusedSlotFenceValue;
        submission.temporalPreviousInstance = temporalPreviousInstance_;
        submission.temporalCurrentInstance = temporalCurrentInstance_;
        submission.temporalDependencyFenceValue = temporalDependencyFenceValue_;
        for (auto& queue : queues_)
        {
            queue.frameSlotFenceValues[currentFrameSlot_] = queue.frameFenceValue;
            if (queue.frameFenceValue != 0)
                submission.queues.push_back({queue.id.value, queue.frameFenceValue});
        }
        submission.releasedExternalResources = frameExternalReleases_;
        previousFrameSignalPoints_ = currentFrameSignalPoints_;
        hasSubmittedFrame_ = true;
        lastSubmittedFrameNumber_ = invocation.frameNumber;
        return base::Success<runtime::FrameSubmission, runtime::RuntimeError>(std::move(submission));
    }

private:
    [[nodiscard]] pkg::QueueId DirectQueueId() const noexcept { return pkg::QueueId{0}; }
    [[nodiscard]] pkg::QueueId CopyQueueId() const noexcept
    {
        return pkg::QueueId{view_.Profile().directQueueCount + view_.Profile().computeQueueCount};
    }
    [[nodiscard]] pkg::QueueId LoadQueueId() const noexcept
    {
        return HasCopyQueue() ? CopyQueueId() : DirectQueueId();
    }
    [[nodiscard]] bool HasCopyQueue() const noexcept { return view_.Profile().copyQueueCount != 0; }
    [[nodiscard]] QueueRuntimeState* QueueState(pkg::QueueId queue) noexcept
    {
        return queue.IsValid() && queue.value < queues_.size() ? &queues_[queue.value] : nullptr;
    }
    [[nodiscard]] const QueueRuntimeState* QueueState(pkg::QueueId queue) const noexcept
    {
        return queue.IsValid() && queue.value < queues_.size() ? &queues_[queue.value] : nullptr;
    }
    [[nodiscard]] bool IsDirectQueue(pkg::QueueId queue) const noexcept
    {
        const auto* state = QueueState(queue);
        return state != nullptr && state->type == D3D12_COMMAND_LIST_TYPE_DIRECT;
    }
    [[nodiscard]] bool IsComputeQueue(pkg::QueueId queue) const noexcept
    {
        const auto* state = QueueState(queue);
        return state != nullptr && state->type == D3D12_COMMAND_LIST_TYPE_COMPUTE;
    }
    [[nodiscard]] bool IsCopyQueue(pkg::QueueId queue) const noexcept
    {
        const auto* state = QueueState(queue);
        return state != nullptr && state->type == D3D12_COMMAND_LIST_TYPE_COPY;
    }
    [[nodiscard]] bool IsSupportedQueue(pkg::QueueId queue) const noexcept
    {
        return QueueState(queue) != nullptr;
    }
    [[nodiscard]] D3D12_COMMAND_LIST_TYPE NativeCommandListType(pkg::QueueId queue) const noexcept
    {
        const auto* state = QueueState(queue);
        return state ? state->type : D3D12_COMMAND_LIST_TYPE_DIRECT;
    }
    [[nodiscard]] ID3D12CommandQueue* NativeQueue(pkg::QueueId queue) const noexcept
    {
        const auto* state = QueueState(queue);
        return state ? state->nativeQueue.Get() : nullptr;
    }
    [[nodiscard]] ID3D12Fence* NativeFence(pkg::QueueId queue) const noexcept
    {
        const auto* state = QueueState(queue);
        return state ? state->fence.Get() : nullptr;
    }
    [[nodiscard]] HANDLE NativeFenceEvent(pkg::QueueId queue) const noexcept
    {
        const auto* state = QueueState(queue);
        return state ? state->fenceEvent : nullptr;
    }
    [[nodiscard]] ComPtr<ID3D12Fence> FenceReference(pkg::QueueId queue) const noexcept
    {
        const auto* state = QueueState(queue);
        return state ? state->fence : ComPtr<ID3D12Fence>{};
    }
    [[nodiscard]] bool FrameQueueSubmitted(pkg::QueueId queue) const noexcept
    {
        const auto* state = QueueState(queue);
        return state != nullptr && state->frameSubmitted;
    }
    void SetFrameQueueSubmitted(pkg::QueueId queue, bool value) noexcept
    {
        if (auto* state = QueueState(queue)) state->frameSubmitted = value;
    }
    void SetFrameFenceValue(pkg::QueueId queue, std::uint64_t value) noexcept
    {
        if (auto* state = QueueState(queue))
        {
            state->lastFenceValue = value;
            state->frameFenceValue = value;
        }
    }
    [[nodiscard]] std::uint64_t NextFenceValue(pkg::QueueId queue) noexcept
    {
        auto* state = QueueState(queue);
        return state ? state->nextFenceValue++ : 0;
    }
    base::Expected<void, runtime::RuntimeError> WaitForQueueFence(pkg::QueueId queue, std::uint64_t value)
    {
        if (value == 0) return base::Success<void, runtime::RuntimeError>();
        auto* state = QueueState(queue);
        if (!state || !state->fence || !state->fenceEvent)
            return base::Failure<void, runtime::RuntimeError>(Error("queue/wait", "Packageが検証または実行の契約に違反しています。"));
        if (state->fence->GetCompletedValue() >= value) return base::Success<void, runtime::RuntimeError>();
        const HRESULT hr = state->fence->SetEventOnCompletion(value, state->fenceEvent);
        if (FAILED(hr)) return base::Failure<void, runtime::RuntimeError>(HResultError("queue/set-fence-event", hr, device_.Get()));
        WaitForSingleObject(state->fenceEvent, INFINITE);
        return base::Success<void, runtime::RuntimeError>();
    }
    void WaitForQueueAtDestruction(pkg::QueueId queue, std::uint64_t value, HANDLE eventHandle) noexcept
    {
        auto* fence = NativeFence(queue);
        if (!fence || value == 0 || !eventHandle || fence->GetCompletedValue() >= value) return;
        if (SUCCEEDED(fence->SetEventOnCompletion(value, eventHandle))) WaitForSingleObject(eventHandle, INFINITE);
    }
    [[nodiscard]] const pkg::ResourceViewArtifact* FindShaderResourceView(pkg::ResourceId resource) const noexcept
    {
        if (!resource.IsValid() || resource.value >= view_.Resources().size()) return nullptr;
        const auto& artifact = view_.Resources()[resource.value];
        const std::uint64_t end = static_cast<std::uint64_t>(artifact.firstView) + artifact.viewCount;
        if (end > view_.Views().size()) return nullptr;
        for (std::uint32_t index = artifact.firstView; index < end; ++index)
        {
            const auto& candidate = view_.Views()[index];
            if (candidate.resource == resource && candidate.viewClass == pkg::ViewClass::ShaderResource)
                return &candidate;
        }
        return nullptr;
    }

    base::Expected<void, runtime::RuntimeError> PrepareVerifiedIndirectDispatch(
        const runtime::FrameInvocation& invocation)
    {
        indirectDispatchPresent_ = false;
        indirectDispatchApplied_ = false;
        indirectDispatch_ = {};

        if (invocation.indirectDispatches.size() > 1)
            return base::Failure<void, runtime::RuntimeError>(
                Error("invocation/indirect-dispatch",
                    "一つのLeaf Invocationへ複数のVerified indirect dispatchを適用できません。"));
        if (invocation.indirectDispatches.empty())
            return base::Success<void, runtime::RuntimeError>();

        const auto& binding = invocation.indirectDispatches.front();
        if (binding.computeCommand == package::InvalidIndex ||
            binding.computeCommand >= view_.ComputeCommands().size() ||
            binding.workCount != binding.threadGroupCountX ||
            binding.threadGroupCountY != 1 || binding.threadGroupCountZ != 1)
            return base::Failure<void, runtime::RuntimeError>(
                Error("invocation/indirect-dispatch",
                    "Verified indirect dispatch引数がPackage契約に違反しています。"));

        const auto& command = view_.ComputeCommands()[binding.computeCommand];
        if (command.flags != 0 || command.threadGroupCountX == 0 ||
            command.threadGroupCountY != 1 || command.threadGroupCountZ != 1 ||
            binding.threadGroupCountX > command.threadGroupCountX)
            return base::Failure<void, runtime::RuntimeError>(
                Error("invocation/indirect-dispatch",
                    "Verified indirect dispatch引数が固定Compute Commandの上限を満たしません。"));

        std::uint32_t operationCount = 0;
        for (const auto& operation : view_.FrameOperations())
        {
            if (operation.opcode != pkg::D3D12OperationCode::ExecuteCompute) continue;
            auto payload = pkg::DecodeExecuteCompute(operation.payload);
            if (!payload)
                return PackageFailure("invocation/indirect-dispatch", payload.error());
            if (payload.value().command.value == binding.computeCommand) ++operationCount;
        }
        if (operationCount != 1)
            return base::Failure<void, runtime::RuntimeError>(
                Error("invocation/indirect-dispatch",
                    "対象Compute CommandはFrame Operationに一度だけ存在しなければなりません。"));

        if (!dispatchCommandSignature_)
        {
            D3D12_INDIRECT_ARGUMENT_DESC argument{};
            argument.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
            D3D12_COMMAND_SIGNATURE_DESC description{};
            description.ByteStride = sizeof(D3D12_DISPATCH_ARGUMENTS);
            description.NumArgumentDescs = 1;
            description.pArgumentDescs = &argument;
            const HRESULT hr = device_->CreateCommandSignature(
                &description, nullptr, IID_PPV_ARGS(&dispatchCommandSignature_));
            if (FAILED(hr))
                return base::Failure<void, runtime::RuntimeError>(
                    HResultError("invocation/create-dispatch-signature", hr, device_.Get()));
        }

        if (currentFrameSlot_ >= indirectArgumentBuffers_.size())
            return base::Failure<void, runtime::RuntimeError>(
                Error("invocation/indirect-dispatch",
                    "Frame slotがIndirect Argument Buffer契約に違反しています。"));
        auto& argumentBuffer = indirectArgumentBuffers_[currentFrameSlot_];
        if (!argumentBuffer)
        {
            D3D12_HEAP_PROPERTIES heap{};
            heap.Type = D3D12_HEAP_TYPE_UPLOAD;
            heap.CreationNodeMask = 1;
            heap.VisibleNodeMask = 1;
            D3D12_RESOURCE_DESC resource{};
            resource.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            resource.Width = sizeof(D3D12_DISPATCH_ARGUMENTS);
            resource.Height = 1;
            resource.DepthOrArraySize = 1;
            resource.MipLevels = 1;
            resource.Format = DXGI_FORMAT_UNKNOWN;
            resource.SampleDesc.Count = 1;
            resource.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            const HRESULT hr = device_->CreateCommittedResource(
                &heap, D3D12_HEAP_FLAG_NONE, &resource,
                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                IID_PPV_ARGS(&argumentBuffer));
            if (FAILED(hr))
                return base::Failure<void, runtime::RuntimeError>(
                    HResultError("invocation/create-dispatch-argument", hr, device_.Get()));
        }

        D3D12_DISPATCH_ARGUMENTS arguments{
            binding.threadGroupCountX,
            binding.threadGroupCountY,
            binding.threadGroupCountZ};
        void* mapped = nullptr;
        D3D12_RANGE noRead{0, 0};
        HRESULT hr = argumentBuffer->Map(0, &noRead, &mapped);
        if (FAILED(hr) || !mapped)
            return base::Failure<void, runtime::RuntimeError>(
                HResultError("invocation/map-dispatch-argument", hr, device_.Get()));
        std::memcpy(mapped, &arguments, sizeof(arguments));
        D3D12_RANGE written{0, sizeof(arguments)};
        argumentBuffer->Unmap(0, &written);

        indirectDispatch_ = binding;
        indirectDispatchPresent_ = true;
        return base::Success<void, runtime::RuntimeError>();
    }

    base::Expected<void, runtime::RuntimeError> PrepareInvocation(const runtime::FrameInvocation& invocation)
    {
        if (invocation.dynamicData.size() != view_.DynamicSlots().size())
            return base::Failure<void, runtime::RuntimeError>(Error("invocation", "Packageが検証または実行の契約に違反しています。"));
        std::fill(dynamicBindings_.begin(), dynamicBindings_.end(), std::span<const std::byte>{});
        for (const auto& binding : invocation.dynamicData)
        {
            if (binding.slot == package::InvalidIndex || binding.slot >= view_.DynamicSlots().size())
                return base::Failure<void, runtime::RuntimeError>(Error("invocation", "Bindingが検証または実行の契約に違反しています。"));
            if (!dynamicBindings_[binding.slot].empty())
                return base::Failure<void, runtime::RuntimeError>(Error("invocation", "Bindingが検証または実行の契約に違反しています。"));
            const auto& contract = view_.DynamicSlots()[binding.slot];
            if (binding.bytes.size() != contract.requiredBytes)
                return base::Failure<void, runtime::RuntimeError>(Error("invocation", "Bindingが検証または実行の契約に違反しています。"));
            dynamicBindings_[binding.slot] = binding.bytes;
        }
        for (const auto bytes : dynamicBindings_)
            if (bytes.empty()) return base::Failure<void, runtime::RuntimeError>(Error("invocation", "Bindingが検証または実行の契約に違反しています。"));

        if (invocation.externalResources.size() != view_.ExternalSlots().size())
            return base::Failure<void, runtime::RuntimeError>(Error("invocation", "Packageが検証または実行の契約に違反しています。"));
        std::fill(externalBindings_.begin(), externalBindings_.end(), runtime::ExternalResourceBinding{});
        for (const auto& binding : invocation.externalResources)
        {
            if (binding.slot == package::InvalidIndex || binding.slot >= view_.ExternalSlots().size())
                return base::Failure<void, runtime::RuntimeError>(Error("invocation", "Bindingが検証または実行の契約に違反しています。"));
            if (externalBindings_[binding.slot].resource)
                return base::Failure<void, runtime::RuntimeError>(Error("invocation", "Bindingが検証または実行の契約に違反しています。"));
            if (!binding.resource || !binding.availableAfter)
                return base::Failure<void, runtime::RuntimeError>(Error("invocation", "Resourceが検証または実行の契約に違反しています。"));
            const auto& contract = view_.ExternalSlots()[binding.slot];
            if (binding.resource->DeviceEpoch() != deviceEpoch_ || binding.availableAfter->DeviceEpoch() != deviceEpoch_)
                return base::Failure<void, runtime::RuntimeError>(Error("invocation", "Bindingが検証または実行の契約に違反しています。"));
            auto* nativeResource = dynamic_cast<ExternalResourceBase*>(binding.resource.get());
            auto* nativeToken = dynamic_cast<CompletionToken*>(binding.availableAfter.get());
            if (!contract.resource.IsValid() || contract.resource.value >= view_.Resources().size() ||
                !nativeResource || !nativeToken ||
                nativeResource->Owner() != ExternalOwner() ||
                nativeToken->Owner() != ExternalOwner() ||
                (domain_
                    ? nativeToken->Slot() != nativeResource->Slot()
                    : nativeToken->Slot() != binding.slot))
                return base::Failure<void, runtime::RuntimeError>(
                    Error("invocation", "Resourceが検証または実行の契約に違反しています。"));
            if (!domain_ && nativeResource->Slot() != binding.slot)
                return base::Failure<void, runtime::RuntimeError>(
                    Error("invocation", "検証または実行の契約に違反しています。"));
            const auto& expectedResource = view_.Resources()[contract.resource.value];
            const bool bufferMatches = contract.requiredKind == pkg::ResourceKind::Buffer &&
                nativeResource->Kind() == pkg::ResourceKind::Buffer &&
                nativeResource->Format() == pkg::Format::Unknown &&
                nativeResource->SizeBytes() >= contract.minimumBytes;
            const bool textureMatches = contract.requiredKind == pkg::ResourceKind::Texture2D &&
                nativeResource->Kind() == pkg::ResourceKind::Texture2D &&
                nativeResource->Format() == contract.requiredFormat &&
                nativeResource->Width() == expectedResource.width &&
                nativeResource->Height() == expectedResource.height &&
                static_cast<std::uint64_t>(nativeResource->RowBytes()) ==
                    static_cast<std::uint64_t>(expectedResource.width) * 4u;
            if (!bufferMatches && !textureMatches)
                return base::Failure<void, runtime::RuntimeError>(
                    Error("invocation", "Resourceが検証または実行の契約に違反しています。"));
            externalBindings_[binding.slot] = binding;
        }
        for (const auto& binding : externalBindings_)
            if (!binding.resource) return base::Failure<void, runtime::RuntimeError>(Error("invocation", "Bindingが検証または実行の契約に違反しています。"));
        return PrepareVerifiedIndirectDispatch(invocation);
    }

    base::Expected<void, runtime::RuntimeError> CreateTimestampProfileObjects()
    {
        if (!options_.enableTimestampProfiling)
            return base::Success<void, runtime::RuntimeError>();

        std::uint32_t dispatchCount = 0;
        std::uint32_t barrierCount = 0;
        pkg::QueueId timestampQueue{};
        for (const auto& operation : view_.FrameOperations())
        {
            if (operation.opcode == pkg::D3D12OperationCode::ExecuteCompute)
            {
                timestampQueue = operation.queue;
                ++dispatchCount;
            }
            else if (operation.opcode == pkg::D3D12OperationCode::Transition ||
                     operation.opcode == pkg::D3D12OperationCode::ActivateAlias)
                ++barrierCount;
        }
        if (dispatchCount != 1 || !IsSupportedQueue(timestampQueue))
            return base::Failure<void, runtime::RuntimeError>(
                Error("profile/contract", "Packageが検証または実行の契約に違反しています。"));

        D3D12_QUERY_HEAP_DESC queryDescription{};
        queryDescription.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
        queryDescription.Count = 2;
        HRESULT hr = device_->CreateQueryHeap(&queryDescription, IID_PPV_ARGS(&timestampQueryHeap_));
        if (FAILED(hr))
            return base::Failure<void, runtime::RuntimeError>(
                HResultError("profile/create-query-heap", hr, device_.Get()));

        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_READBACK;
        heap.CreationNodeMask = 1;
        heap.VisibleNodeMask = 1;
        D3D12_RESOURCE_DESC description{};
        description.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        description.Width = sizeof(std::uint64_t) * 2u;
        description.Height = 1;
        description.DepthOrArraySize = 1;
        description.MipLevels = 1;
        description.Format = DXGI_FORMAT_UNKNOWN;
        description.SampleDesc.Count = 1;
        description.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        hr = device_->CreateCommittedResource(
            &heap, D3D12_HEAP_FLAG_NONE, &description, D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr, IID_PPV_ARGS(&timestampReadback_));
        if (FAILED(hr))
            return base::Failure<void, runtime::RuntimeError>(
                HResultError("profile/create-readback", hr, device_.Get()));
        D3D12_RANGE readRange{0, sizeof(std::uint64_t) * 2u};
        hr = timestampReadback_->Map(0, &readRange, reinterpret_cast<void**>(&mappedTimestampValues_));
        if (FAILED(hr))
            return base::Failure<void, runtime::RuntimeError>(
                HResultError("profile/map-readback", hr, device_.Get()));
        std::uint64_t frequency = 0;
        hr = NativeQueue(timestampQueue)->GetTimestampFrequency(&frequency);
        if (FAILED(hr) || frequency == 0)
            return base::Failure<void, runtime::RuntimeError>(
                HResultError("profile/timestamp-frequency", hr, device_.Get()));
        timestampQueue_ = timestampQueue;
        profileRecord_->mappedQueryValues = mappedTimestampValues_;
        profileRecord_->timestampFrequency = frequency;
        profileRecord_->dispatchCount = dispatchCount;
        profileRecord_->barrierCount = barrierCount;
        return base::Success<void, runtime::RuntimeError>();
    }

    void CompleteTimestampProfile(std::uint64_t frameNumber, double commandRecordingNanoseconds)
    {
        if (!profileRecord_) return;
        std::scoped_lock lock(profileCollector_->mutex);
        profileRecord_->frameNumber = frameNumber;
        profileRecord_->submissionOrdinal = profileCollector_->nextSubmissionOrdinal++;
        profileRecord_->commandRecordingNanoseconds = commandRecordingNanoseconds;
        profileRecord_->ready = true;
    }

    void ReleaseDeviceObjectsForRecovery() noexcept
    {
        if (timestampReadback_ && mappedTimestampValues_)
            timestampReadback_->Unmap(0, nullptr);
        mappedTimestampValues_ = nullptr;
        if (profileRecord_)
        {
            profileRecord_->mappedQueryValues = nullptr;
            profileRecord_->ready = false;
        }
        timestampReadback_.Reset();
        timestampQueryHeap_.Reset();
        dispatchCommandSignature_.Reset();
        indirectArgumentBuffers_.clear();
        indirectDispatchPresent_ = false;
        indirectDispatchApplied_ = false;
        indirectDispatch_ = {};
        externalNativeResources_.clear();
        externalBindings_.clear();
        externalAcquired_.clear();
        externalWaited_.clear();
        externalReleased_.clear();
        surfaceAcquired_.clear();
        surfacePresented_.clear();
        temporalWaitedResources_.clear();
        frameExternalReleases_.clear();
        currentFrameSignalPoints_.clear();
        previousFrameSignalPoints_.clear();

        commandList_.Reset();
        allocator_.Reset();
        commandOpen_ = false;
        activeCommandQueue_ = package::InvalidIndex;
        loadCommandList_.Reset();
        loadAllocator_.Reset();
        for (auto& queue : queues_)
        {
            queue.frameCommandLists.clear();
            queue.frameAllocators.clear();
            queue.fence.Reset();
            if (queue.fenceEvent)
            {
                CloseHandle(queue.fenceEvent);
                queue.fenceEvent = nullptr;
            }
            queue.nativeQueue.Reset();
        }
        queues_.clear();

        backBuffers_.clear();
        swapChain_.Reset();
        pendingBufferVerifications_.clear();
        pendingTextureVerifications_.clear();
        uploadResources_.clear();
        resources_.clear();
        placedHeaps_.clear();
        activeAliasResources_.clear();
        rootSignatures_.clear();
        pipelineStates_.clear();
        computePipelineStates_.clear();
        rtvHeap_.Reset();
        dsvHeap_.Reset();
        shaderHeap_.Reset();

        device_.Reset();
        factory_.Reset();

        resourceStates_.clear();
        dynamicBindings_.clear();
        dynamicApplied_.clear();

        currentFrameSlot_ = 0;
        temporalPreviousInstance_ = 1;
        temporalCurrentInstance_ = 0;
        temporalDependencyFenceValue_ = 0;
        lastSubmittedFrameNumber_ = 0;
        hasSubmittedFrame_ = false;
        descriptorHeapsCreated_ = false;
        loadBatchOpen_ = false;
        loadBatchClosed_ = false;
        loadQueueCompleted_ = false;
        hasLoadQueueBatch_ = false;
        rtvIncrement_ = 0;
        dsvIncrement_ = 0;
        shaderDescriptorIncrement_ = 0;
        currentBackBuffer_ = 0;
        hasActiveAdapterLuid_ = false;
        activeAdapterLuid_ = {};
    }

    base::Expected<void, runtime::RuntimeError> CreateBaseObjects()
    {
        const auto& profile = view_.Profile();
        if (profile.minimumFeatureLevel != 0xb000 || profile.shaderModelMajor != 5 || profile.shaderModelMinor != 1 ||
            profile.rootSignatureMajor != 1 || profile.rootSignatureMinor != 0 ||
            profile.framesInFlight == 0 || profile.directQueueCount != 1 ||
            profile.computeQueueCount > 1 || profile.copyQueueCount > 1)
            return base::Failure<void, runtime::RuntimeError>(Error("target-profile",
                "検証または実行の契約に違反しています。"));

        const bool packageHasSurface = !view_.SurfaceSlots().empty();
        if (packageHasSurface != (profile.surfaceImageCount != 0))
            return base::Failure<void, runtime::RuntimeError>(Error("target-profile",
                "Packageが検証または実行の契約に違反しています。"));
        if (packageHasSurface && surface_ == nullptr)
            return base::Failure<void, runtime::RuntimeError>(Error("surface",
                "Packageが検証または実行の契約に違反しています。"));

        queues_.clear();
        queues_.reserve(profile.directQueueCount + profile.computeQueueCount + profile.copyQueueCount);
        const auto addQueueMetadata = [&](D3D12_COMMAND_LIST_TYPE type)
        {
            QueueRuntimeState state;
            state.id = pkg::QueueId{static_cast<std::uint32_t>(queues_.size())};
            state.type = type;
            queues_.push_back(std::move(state));
        };
        for (std::uint32_t index = 0; index < profile.directQueueCount; ++index) addQueueMetadata(D3D12_COMMAND_LIST_TYPE_DIRECT);
        for (std::uint32_t index = 0; index < profile.computeQueueCount; ++index) addQueueMetadata(D3D12_COMMAND_LIST_TYPE_COMPUTE);
        for (std::uint32_t index = 0; index < profile.copyQueueCount; ++index) addQueueMetadata(D3D12_COMMAND_LIST_TYPE_COPY);

        std::vector<std::uint32_t> frameBatchCounts(queues_.size(), 0);
        for (const auto& operation : view_.FrameOperations())
        {
            if (operation.opcode != pkg::D3D12OperationCode::BeginQueueBatch) continue;
            if (!IsSupportedQueue(operation.queue))
                return base::Failure<void, runtime::RuntimeError>(
                    Error("target-profile", "Queueが検証または実行の契約に違反しています。"));
            ++frameBatchCounts[operation.queue.value];
        }
        const auto& diagnostics = ConfigureD3D12DiagnosticsOnce(options_.enableDebugLayer);
        HRESULT hr = S_OK;
        if (domain_)
        {
            factory_ = domain_->Factory();
            device_ = domain_->Device();
            activeAdapterLuid_ = domain_->AdapterLuid();
            hasActiveAdapterLuid_ = true;
            if (!factory_ || !device_)
                return base::Failure<void, runtime::RuntimeError>(
                    Error("domain/load", "Deviceが検証または実行の契約に違反しています。"));
        }
        else
        {
        UINT factoryFlags = 0;
#if defined(_DEBUG)
        if (options_.enableDebugLayer && diagnostics.debugLayerEnabled)
            factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#else
        (void)diagnostics;
#endif
        hr = CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&factory_));
        if (FAILED(hr)) return base::Failure<void, runtime::RuntimeError>(HResultError("device/create-factory", hr));

        ComPtr<IDXGIAdapter1> adapter;
        DXGI_ADAPTER_DESC1 selectedDesc{};
        const auto isExcluded = [&](const DXGI_ADAPTER_DESC1& desc) noexcept
        {
            return hasExcludedAdapterLuid_ && SameLuid(desc.AdapterLuid, excludedAdapterLuid_);
        };
        const auto acceptCandidate = [&](ComPtr<IDXGIAdapter1> candidate) -> bool
        {
            if (!candidate) return false;
            DXGI_ADAPTER_DESC1 desc{};
            if (FAILED(candidate->GetDesc1(&desc)) || isExcluded(desc)) return false;
            if (FAILED(D3D12CreateDevice(candidate.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr))) return false;
            adapter = std::move(candidate);
            selectedDesc = desc;
            return true;
        };

        if (options_.forceWarp)
        {
            ComPtr<IDXGIAdapter1> warp;
            hr = factory_->EnumWarpAdapter(IID_PPV_ARGS(&warp));
            if (FAILED(hr)) return base::Failure<void, runtime::RuntimeError>(HResultError("device/warp-adapter", hr));
            acceptCandidate(std::move(warp));
        }
        else
        {
            for (UINT index = 0; ; ++index)
            {
                ComPtr<IDXGIAdapter1> candidate;
                hr = factory_->EnumAdapterByGpuPreference(index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&candidate));
                if (hr == DXGI_ERROR_NOT_FOUND) break;
                if (FAILED(hr)) continue;
                DXGI_ADAPTER_DESC1 desc{};
                if (FAILED(candidate->GetDesc1(&desc)) || (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) continue;
                if (acceptCandidate(std::move(candidate))) break;
            }
            if (!adapter)
            {
                ComPtr<IDXGIAdapter1> warp;
                hr = factory_->EnumWarpAdapter(IID_PPV_ARGS(&warp));
                if (SUCCEEDED(hr)) acceptCandidate(std::move(warp));
            }
        }
        if (!adapter)
            return base::Failure<void, runtime::RuntimeError>(
                Error("device/no-eligible-adapter", "検証または実行の契約に違反しています。"));

        hr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device_));
        if (FAILED(hr)) return base::Failure<void, runtime::RuntimeError>(HResultError("device/create-device", hr));
        activeAdapterLuid_ = selectedDesc.AdapterLuid;
        hasActiveAdapterLuid_ = true;
        }

        for (auto& queue : queues_)
        {
            D3D12_COMMAND_QUEUE_DESC queueDesc{};
            queueDesc.Type = queue.type;
            hr = device_->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue.nativeQueue));
            if (FAILED(hr)) return base::Failure<void, runtime::RuntimeError>(
                HResultError("device/create-package-queue", hr, device_.Get()));
        }

        if (packageHasSurface)
        {
            const auto width = std::max(1u, surface_->ClientWidth());
            const auto height = std::max(1u, surface_->ClientHeight());
            DXGI_SWAP_CHAIN_DESC1 swapDesc{};
            swapDesc.Width = width;
            swapDesc.Height = height;
            swapDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            swapDesc.SampleDesc.Count = 1;
            swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            swapDesc.BufferCount = profile.surfaceImageCount;
            swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
            ComPtr<IDXGISwapChain1> swapChain1;
            hr = factory_->CreateSwapChainForHwnd(NativeQueue(DirectQueueId()),
                static_cast<HWND>(surface_->NativeWindowHandle()), &swapDesc, nullptr, nullptr, &swapChain1);
            if (FAILED(hr)) return base::Failure<void, runtime::RuntimeError>(HResultError("surface/create-swap-chain", hr, device_.Get()));
            factory_->MakeWindowAssociation(static_cast<HWND>(surface_->NativeWindowHandle()), DXGI_MWA_NO_ALT_ENTER);
            hr = swapChain1.As(&swapChain_);
            if (FAILED(hr)) return base::Failure<void, runtime::RuntimeError>(HResultError("surface/query-swap-chain3", hr, device_.Get()));
        }

        hasLoadQueueBatch_ = std::any_of(view_.LoadOperations().begin(), view_.LoadOperations().end(),
            [](const pkg::OperationView& operation) {
                return operation.opcode == pkg::D3D12OperationCode::BeginQueueBatch;
            });
        if (hasLoadQueueBatch_)
        {
            const auto loadQueue = LoadQueueId();
            const auto loadType = NativeCommandListType(loadQueue);
            hr = device_->CreateCommandAllocator(loadType, IID_PPV_ARGS(&loadAllocator_));
            if (FAILED(hr))
                return base::Failure<void, runtime::RuntimeError>(
                    HResultError("device/create-load-command-allocator", hr, device_.Get()));
            hr = device_->CreateCommandList(0, loadType, loadAllocator_.Get(), nullptr,
                IID_PPV_ARGS(&loadCommandList_));
            if (FAILED(hr))
                return base::Failure<void, runtime::RuntimeError>(
                    HResultError("device/create-load-command-list", hr, device_.Get()));
            hr = loadCommandList_->Close();
            if (FAILED(hr))
                return base::Failure<void, runtime::RuntimeError>(
                    HResultError("device/close-load-command-list", hr, device_.Get()));
        }

        for (auto& queue : queues_)
        {
            queue.frameAllocators.resize(profile.framesInFlight);
            queue.frameCommandLists.resize(profile.framesInFlight);
            const auto batchCount = frameBatchCounts[queue.id.value];
            for (std::uint32_t slot = 0; slot < profile.framesInFlight; ++slot)
            {
                queue.frameAllocators[slot].resize(batchCount);
                queue.frameCommandLists[slot].resize(batchCount);
                for (std::uint32_t batch = 0; batch < batchCount; ++batch)
                {
                    hr = device_->CreateCommandAllocator(queue.type, IID_PPV_ARGS(&queue.frameAllocators[slot][batch]));
                    if (FAILED(hr)) return base::Failure<void, runtime::RuntimeError>(
                        HResultError("device/create-package-frame-command-allocator", hr, device_.Get()));
                    hr = device_->CreateCommandList(0, queue.type, queue.frameAllocators[slot][batch].Get(), nullptr,
                        IID_PPV_ARGS(&queue.frameCommandLists[slot][batch]));
                    if (FAILED(hr)) return base::Failure<void, runtime::RuntimeError>(
                        HResultError("device/create-package-frame-command-list", hr, device_.Get()));
                    hr = queue.frameCommandLists[slot][batch]->Close();
                    if (FAILED(hr)) return base::Failure<void, runtime::RuntimeError>(
                        HResultError("device/close-package-frame-command-list", hr, device_.Get()));
                }
            }

            hr = device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&queue.fence));
            if (FAILED(hr)) return base::Failure<void, runtime::RuntimeError>(
                HResultError("device/create-package-queue-fence", hr, device_.Get()));
            queue.fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
            if (!queue.fenceEvent)
                return base::Failure<void, runtime::RuntimeError>(
                    Error("device/create-package-queue-fence-event", "入力または内部状態が検証または実行の契約に違反しています。"));
        }
        return base::Success<void, runtime::RuntimeError>();
    }

    base::Expected<void, runtime::RuntimeError> ExecuteLoadOperation(const pkg::OperationView& operation)
    {
        switch (operation.opcode)
        {
        case pkg::D3D12OperationCode::CreateDescriptorHeaps:
            if (!operation.payload.empty()) return base::Failure<void, runtime::RuntimeError>(Error("load/CreateDescriptorHeaps", "Payloadが検証または実行の契約に違反しています。"));
            return CreateDescriptorHeaps();
        case pkg::D3D12OperationCode::CreateResource:
        {
            auto payload = pkg::DecodeCreateResource(operation.payload);
            if (!payload) return PackageFailure("load/CreateResource", payload.error());
            return CreateResource(payload.value().resource);
        }
        case pkg::D3D12OperationCode::BeginQueueBatch:
        {
            if (operation.queue != LoadQueueId() || !operation.payload.empty() || loadBatchOpen_ || loadBatchClosed_)
                return base::Failure<void, runtime::RuntimeError>(Error("load/BeginQueueBatch", "Packageが検証または実行の契約に違反しています。"));
            HRESULT hr = loadAllocator_->Reset();
            if (FAILED(hr)) return base::Failure<void, runtime::RuntimeError>(HResultError("load/reset-allocator", hr, device_.Get()));
            hr = loadCommandList_->Reset(loadAllocator_.Get(), nullptr);
            if (FAILED(hr)) return base::Failure<void, runtime::RuntimeError>(HResultError("load/reset-command-list", hr, device_.Get()));
            loadBatchOpen_ = true;
            return base::Success<void, runtime::RuntimeError>();
        }
        case pkg::D3D12OperationCode::UploadBuffer:
        {
            auto payload = pkg::DecodeUploadBuffer(operation.payload);
            if (!payload) return PackageFailure("load/UploadBuffer", payload.error());
            return UploadBuffer(payload.value());
        }
        case pkg::D3D12OperationCode::UploadTexture:
        {
            auto payload = pkg::DecodeUploadTexture(operation.payload);
            if (!payload) return PackageFailure("load/UploadTexture", payload.error());
            return UploadTexture(payload.value());
        }
        case pkg::D3D12OperationCode::InitializeState:
        {
            auto payload = pkg::DecodeInitializeState(operation.payload);
            if (!payload) return PackageFailure("load/InitializeState", payload.error());
            if (!loadBatchOpen_ || operation.queue != LoadQueueId())
                return base::Failure<void, runtime::RuntimeError>(Error("load/InitializeState", "検証または実行の契約に違反しています。"));
            if (IsCopyQueue(operation.queue) && (!IsCopyQueueState(payload.value().before) || !IsCopyQueueState(payload.value().after)))
                return base::Failure<void, runtime::RuntimeError>(Error("load/InitializeState", "Queueが検証または実行の契約に違反しています。"));
            const auto& artifact = view_.Resources()[payload.value().resource.value];
            if (IsTemporal(artifact))
            {
                for (std::uint32_t instanceIndex = 0; instanceIndex < artifact.physicalInstanceCount; ++instanceIndex)
                {
                    auto transitioned = TransitionResourceAt(payload.value().resource, instanceIndex,
                        payload.value().before, payload.value().after, loadCommandList_.Get(),
                        NativeCommandListType(operation.queue));
                    if (!transitioned) return transitioned;
                }
                return base::Success<void, runtime::RuntimeError>();
            }
            return TransitionResource(payload.value().resource, payload.value().before, payload.value().after,
                loadCommandList_.Get(), NativeCommandListType(operation.queue));
        }
        case pkg::D3D12OperationCode::ActivateAlias:
        {
            auto payload = pkg::DecodeActivateAlias(operation.payload);
            if (!payload) return PackageFailure("load/ActivateAlias", payload.error());
            if (!loadBatchOpen_ || operation.queue != LoadQueueId())
                return base::Failure<void, runtime::RuntimeError>(Error("load/ActivateAlias", "検証または実行の契約に違反しています。"));
            return ActivateAlias(payload.value(), loadCommandList_.Get());
        }
        case pkg::D3D12OperationCode::VerifyBufferContents:
        {
            auto payload = pkg::DecodeVerifyBufferContents(operation.payload);
            if (!payload) return PackageFailure("load/VerifyBufferContents", payload.error());
            return ScheduleBufferVerification(payload.value());
        }
        case pkg::D3D12OperationCode::VerifyTextureContents:
        {
            auto payload = pkg::DecodeVerifyTextureContents(operation.payload);
            if (!payload) return PackageFailure("load/VerifyTextureContents", payload.error());
            return ScheduleTextureVerification(payload.value());
        }
        case pkg::D3D12OperationCode::ExecuteCopy:
        {
            auto payload = pkg::DecodeCopyBuffer(operation.payload);
            if (!payload) return PackageFailure("load/ExecuteCopy", payload.error());
            return ExecuteCopy(payload.value());
        }
        case pkg::D3D12OperationCode::EndQueueBatch:
        {
            if (!loadBatchOpen_ || operation.queue != LoadQueueId() || !operation.payload.empty())
                return base::Failure<void, runtime::RuntimeError>(Error("load/EndQueueBatch", "検証または実行の契約に違反しています。"));
            const HRESULT hr = loadCommandList_->Close();
            if (FAILED(hr)) return base::Failure<void, runtime::RuntimeError>(HResultError("load/close-command-list", hr, device_.Get()));
            loadBatchOpen_ = false;
            loadBatchClosed_ = true;
            return base::Success<void, runtime::RuntimeError>();
        }
        case pkg::D3D12OperationCode::SignalQueue:
        {
            const auto loadQueue = LoadQueueId();
            auto payload = pkg::DecodeSignalQueue(operation.payload);
            if (!payload || !payload.value().signalPoint.IsValid() ||
                !loadBatchClosed_ || operation.queue != loadQueue || loadQueueCompleted_)
                return base::Failure<void, runtime::RuntimeError>(Error("load/SignalQueue", "Packageが検証または実行の契約に違反しています。"));
            ID3D12CommandList* lists[] = {loadCommandList_.Get()};
            NativeQueue(loadQueue)->ExecuteCommandLists(1, lists);
            const auto value = NextFenceValue(loadQueue);
            HRESULT hr = NativeQueue(loadQueue)->Signal(NativeFence(loadQueue), value);
            if (FAILED(hr)) return base::Failure<void, runtime::RuntimeError>(HResultError("load/signal-queue", hr, device_.Get()));
            SetFrameFenceValue(loadQueue, value);
            auto waited = WaitForQueueFence(loadQueue, value);
            if (!waited) return waited;
            for (auto& queue : queues_) queue.frameFenceValue = 0;
            loadQueueCompleted_ = true;
            return base::Success<void, runtime::RuntimeError>();
        }
        case pkg::D3D12OperationCode::CreateRootSignature:
        {
            auto payload = pkg::DecodeCreateRootSignature(operation.payload);
            if (!payload) return PackageFailure("load/CreateRootSignature", payload.error());
            return CreateRootSignature(payload.value().layout);
        }
        case pkg::D3D12OperationCode::CreateGraphicsPipeline:
        {
            auto payload = pkg::DecodeCreateGraphicsPipeline(operation.payload);
            if (!payload) return PackageFailure("load/CreateGraphicsPipeline", payload.error());
            return CreateGraphicsPipeline(payload.value().executable);
        }
        case pkg::D3D12OperationCode::CreateComputePipeline:
        {
            auto payload = pkg::DecodeCreateComputePipeline(operation.payload);
            if (!payload) return PackageFailure("load/CreateComputePipeline", payload.error());
            return CreateComputePipeline(payload.value().executable);
        }
        default:
            return base::Failure<void, runtime::RuntimeError>(Error("load", "Operationが検証または実行の契約に違反しています。"));
        }
    }

    base::Expected<void, runtime::RuntimeError> ExecuteFrameOperation(const pkg::OperationView& operation)
    {
        switch (operation.opcode)
        {
        case pkg::D3D12OperationCode::ApplyDynamicData:
        {
            auto payload = pkg::DecodeApplyDynamicData(operation.payload);
            if (!payload) return PackageFailure("frame/ApplyDynamicData", payload.error());
            return ApplyDynamicData(payload.value().slot);
        }
        case pkg::D3D12OperationCode::AcquireExternal:
        {
            auto payload = pkg::DecodeAcquireExternal(operation.payload);
            if (!payload) return PackageFailure("frame/AcquireExternal", payload.error());
            const auto slot = payload.value().slot.value;
            if (!payload.value().slot.IsValid() || slot >= view_.ExternalSlots().size() || externalAcquired_[slot])
                return base::Failure<void, runtime::RuntimeError>(
                    Error("frame/AcquireExternal", "入力または内部状態に重複または二重処理があります。"));
            const auto& contract = view_.ExternalSlots()[slot];
            auto* native = dynamic_cast<ExternalResourceBase*>(externalBindings_[slot].resource.get());
            if (!native || native->Owner() != ExternalOwner() || (!domain_ && native->Slot() != slot) ||
                !contract.resource.IsValid() || contract.resource.value >= externalNativeResources_.size())
                return base::Failure<void, runtime::RuntimeError>(
                    Error("frame/AcquireExternal", "Packageが検証または実行の契約に違反しています。"));
            if (native->CurrentState() != contract.requiredIncomingState ||
                !(TrackedState(contract.resource) == contract.requiredIncomingState))
                return base::Failure<void, runtime::RuntimeError>(
                    Error("frame/AcquireExternal", "Packageが検証または実行の契約に違反しています。"));

            externalNativeResources_[contract.resource.value] = externalBindings_[slot].resource;
            const auto& resource = view_.Resources()[contract.resource.value];
            const std::uint64_t viewEnd = static_cast<std::uint64_t>(resource.firstView) + resource.viewCount;
            if (viewEnd > view_.Views().size())
                return base::Failure<void, runtime::RuntimeError>(
                    Error("frame/AcquireExternal", "Resourceが検証または実行の契約に違反しています。"));
            for (std::uint32_t index = resource.firstView; index < viewEnd; ++index)
            {
                const auto& externalView = view_.Views()[index];
                if (resource.resourceKind == pkg::ResourceKind::Buffer)
                {
                    if (externalView.viewClass != pkg::ViewClass::ShaderResource &&
                        externalView.viewClass != pkg::ViewClass::UnorderedAccess)
                        continue;
                    if (!shaderHeap_ || externalView.strideBytes == 0 || externalView.byteSize == 0 ||
                        externalView.byteOffset % externalView.strideBytes != 0 ||
                        externalView.byteSize % externalView.strideBytes != 0)
                        return base::Failure<void, runtime::RuntimeError>(
                            Error("frame/AcquireExternal", "Bufferが検証または実行の契約に違反しています。"));
                    auto cpu = shaderHeap_->GetCPUDescriptorHandleForHeapStart();
                    cpu.ptr += static_cast<SIZE_T>(DescriptorIndex(externalView)) * shaderDescriptorIncrement_;
                    if (externalView.viewClass == pkg::ViewClass::ShaderResource)
                    {
                        D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
                        srv.Format = ToDxgi(externalView.format);
                        srv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
                        srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                        srv.Buffer.FirstElement = externalView.byteOffset / externalView.strideBytes;
                        srv.Buffer.NumElements = static_cast<UINT>(externalView.byteSize / externalView.strideBytes);
                        srv.Buffer.StructureByteStride = externalView.strideBytes;
                        device_->CreateShaderResourceView(native->Native(), &srv, cpu);
                    }
                    else
                    {
                        D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};
                        uav.Format = ToDxgi(externalView.format);
                        uav.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
                        uav.Buffer.FirstElement = externalView.byteOffset / externalView.strideBytes;
                        uav.Buffer.NumElements = static_cast<UINT>(externalView.byteSize / externalView.strideBytes);
                        uav.Buffer.StructureByteStride = externalView.strideBytes;
                        device_->CreateUnorderedAccessView(native->Native(), nullptr, &uav, cpu);
                    }
                }
                else if (resource.resourceKind == pkg::ResourceKind::Texture2D)
                {
                    if (externalView.viewClass == pkg::ViewClass::ShaderResource)
                    {
                        if (!shaderHeap_)
                            return base::Failure<void, runtime::RuntimeError>(
                                Error("frame/AcquireExternal", "Textureが検証または実行の契約に違反しています。"));
                        auto cpu = shaderHeap_->GetCPUDescriptorHandleForHeapStart();
                        cpu.ptr += static_cast<SIZE_T>(DescriptorIndex(externalView)) * shaderDescriptorIncrement_;
                        D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
                        srv.Format = ToDxgi(externalView.format);
                        srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                        srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                        srv.Texture2D.MostDetailedMip = externalView.firstMip;
                        srv.Texture2D.MipLevels = externalView.mipCount;
                        srv.Texture2D.PlaneSlice = externalView.firstPlane;
                        device_->CreateShaderResourceView(native->Native(), &srv, cpu);
                    }
                    else if (externalView.viewClass == pkg::ViewClass::RenderTarget)
                    {
                        if (!rtvHeap_)
                            return base::Failure<void, runtime::RuntimeError>(
                                Error("frame/AcquireExternal", "Textureが検証または実行の契約に違反しています。"));
                        auto cpu = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
                        cpu.ptr += static_cast<SIZE_T>(DescriptorIndex(externalView)) * rtvIncrement_;
                        D3D12_RENDER_TARGET_VIEW_DESC rtv{};
                        rtv.Format = ToDxgi(externalView.format);
                        rtv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
                        rtv.Texture2D.MipSlice = externalView.firstMip;
                        rtv.Texture2D.PlaneSlice = externalView.firstPlane;
                        device_->CreateRenderTargetView(native->Native(), &rtv, cpu);
                    }
                    else
                        return base::Failure<void, runtime::RuntimeError>(
                            Error("frame/AcquireExternal", "Textureが検証または実行の契約に違反しています。"));
                }
                else
                    return base::Failure<void, runtime::RuntimeError>(
                        Error("frame/AcquireExternal", "Resourceが検証または実行の契約に違反しています。"));
            }
            externalAcquired_[slot] = true;
            return base::Success<void, runtime::RuntimeError>();
        }
        case pkg::D3D12OperationCode::WaitExternal:
        {
            auto payload = pkg::DecodeWaitExternal(operation.payload);
            if (!payload) return PackageFailure("frame/WaitExternal", payload.error());
            const auto slot = payload.value().slot.value;
            if (!IsSupportedQueue(operation.queue) || !payload.value().slot.IsValid() || slot >= view_.ExternalSlots().size() ||
                !externalAcquired_[slot] || externalWaited_[slot])
                return base::Failure<void, runtime::RuntimeError>(Error("frame/WaitExternal", "PackageがCanonicalな順序または識別子規則に違反しています。"));
            auto* token = dynamic_cast<CompletionToken*>(externalBindings_[slot].availableAfter.get());
            auto* nativeResource = dynamic_cast<ExternalResourceBase*>(
                externalBindings_[slot].resource.get());
            if (!token || !nativeResource || token->Owner() != ExternalOwner() ||
                (domain_ ? token->Slot() != nativeResource->Slot() : token->Slot() != slot))
                return base::Failure<void, runtime::RuntimeError>(
                    Error("frame/WaitExternal", "Identityが検証または実行の契約に違反しています。"));
            const HRESULT hr = NativeQueue(operation.queue)->Wait(token->NativeFence(), token->Value());
            if (FAILED(hr)) return base::Failure<void, runtime::RuntimeError>(HResultError("frame/wait-external", hr, device_.Get()));
            externalWaited_[slot] = true;
            return base::Success<void, runtime::RuntimeError>();
        }
        case pkg::D3D12OperationCode::AcquireSurfaceImage:
        {
            auto payload = pkg::DecodeAcquireSurfaceImage(operation.payload);
            if (!payload) return PackageFailure("frame/AcquireSurfaceImage", payload.error());
            const auto slot = payload.value().slot.value;
            if (!payload.value().slot.IsValid() || slot >= view_.SurfaceSlots().size() || surfaceAcquired_[slot] || !swapChain_)
                return base::Failure<void, runtime::RuntimeError>(Error("frame/AcquireSurfaceImage", "Surfaceが検証または実行の契約に違反しています。"));
            currentBackBuffer_ = swapChain_->GetCurrentBackBufferIndex();
            surfaceAcquired_[slot] = true;
            return base::Success<void, runtime::RuntimeError>();
        }
        case pkg::D3D12OperationCode::BeginQueueBatch:
        {
            auto* queue = QueueState(operation.queue);
            if (!operation.payload.empty() || queue == nullptr)
                return base::Failure<void, runtime::RuntimeError>(Error("frame/BeginQueueBatch", "Packageが検証または実行の契約に違反しています。"));
            if (commandOpen_)
                return base::Failure<void, runtime::RuntimeError>(Error("frame/BeginQueueBatch", "検証または実行の契約に違反しています。"));
            if (currentFrameSlot_ >= queue->frameAllocators.size() ||
                queue->frameBatchCursor >= queue->frameAllocators[currentFrameSlot_].size())
                return base::Failure<void, runtime::RuntimeError>(Error("frame/BeginQueueBatch", "Packageが検証または実行の契約に違反しています。"));
            allocator_ = queue->frameAllocators[currentFrameSlot_][queue->frameBatchCursor];
            commandList_ = queue->frameCommandLists[currentFrameSlot_][queue->frameBatchCursor++];
            HRESULT hr = allocator_->Reset();
            if (FAILED(hr)) return base::Failure<void, runtime::RuntimeError>(HResultError("frame/reset-package-queue-allocator", hr, device_.Get()));
            hr = commandList_->Reset(allocator_.Get(), nullptr);
            if (FAILED(hr)) return base::Failure<void, runtime::RuntimeError>(HResultError("frame/reset-package-queue-command-list", hr, device_.Get()));
            commandOpen_ = true;
            activeCommandQueue_ = operation.queue.value;
            queue->commandOpen = true;
            return base::Success<void, runtime::RuntimeError>();
        }
        case pkg::D3D12OperationCode::Transition:
        {
            auto payload = pkg::DecodeTransition(operation.payload);
            if (!payload) return PackageFailure("frame/Transition", payload.error());
            if (!payload.value().view.IsValid() || payload.value().view.value >= view_.Views().size())
                return base::Failure<void, runtime::RuntimeError>(Error("frame/Transition", "Viewが検証または実行の契約に違反しています。"));
            const auto* queue = QueueState(operation.queue);
            if (!queue || !commandOpen_ || activeCommandQueue_ != operation.queue.value)
                return base::Failure<void, runtime::RuntimeError>(Error("frame/Transition", "Packageが検証または実行の契約に違反しています。"));
            if (queue->type == D3D12_COMMAND_LIST_TYPE_COPY &&
                (!IsCopyQueueState(payload.value().before) || !IsCopyQueueState(payload.value().after)))
                return base::Failure<void, runtime::RuntimeError>(Error("frame/Transition", "Queueが検証または実行の契約に違反しています。"));
            return TransitionResource(view_.Views()[payload.value().view.value], payload.value().before,
                payload.value().after, commandList_.Get(), queue->type);
        }
        case pkg::D3D12OperationCode::ExecuteCopy:
        {
            const auto* queue = QueueState(operation.queue);
            if (!queue || !commandOpen_ || activeCommandQueue_ != operation.queue.value ||
                (queue->type != D3D12_COMMAND_LIST_TYPE_DIRECT && queue->type != D3D12_COMMAND_LIST_TYPE_COPY))
                return base::Failure<void, runtime::RuntimeError>(Error("frame/ExecuteCopy", "検証または実行の契約に違反しています。"));
            auto payload = pkg::DecodeCopyBuffer(operation.payload);
            if (!payload) return PackageFailure("frame/ExecuteCopy", payload.error());
            return ExecuteCopy(payload.value(), "frame/ExecuteCopy");
        }
        case pkg::D3D12OperationCode::ExecuteCompute:
        {
            const auto* queue = QueueState(operation.queue);
            if (!queue || !commandOpen_ || activeCommandQueue_ != operation.queue.value ||
                (queue->type != D3D12_COMMAND_LIST_TYPE_DIRECT && queue->type != D3D12_COMMAND_LIST_TYPE_COMPUTE))
                return base::Failure<void, runtime::RuntimeError>(Error("frame/ExecuteCompute", "検証または実行の契約に違反しています。"));
            auto payload = pkg::DecodeExecuteCompute(operation.payload);
            if (!payload) return PackageFailure("frame/ExecuteCompute", payload.error());
            if (options_.enableTimestampProfiling)
            {
                if (timestampQueryIssued_ || operation.queue != timestampQueue_)
                    return base::Failure<void, runtime::RuntimeError>(
                        Error("profile/dispatch", "Contractが検証または実行の契約に違反しています。"));
                commandList_->EndQuery(timestampQueryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0);
            }
            auto executed = ExecuteCompute(payload.value().command);
            if (!executed) return executed;
            if (options_.enableTimestampProfiling)
            {
                commandList_->EndQuery(timestampQueryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 1);
                timestampQueryIssued_ = true;
            }
            return base::Success<void, runtime::RuntimeError>();
        }
        case pkg::D3D12OperationCode::ExecuteRaster:
        {
            if (!IsDirectQueue(operation.queue) || !commandOpen_ || activeCommandQueue_ != operation.queue.value)
                return base::Failure<void, runtime::RuntimeError>(Error("frame/ExecuteRaster", "検証または実行の契約に違反しています。"));
            auto payload = pkg::DecodeExecuteRaster(operation.payload);
            if (!payload) return PackageFailure("frame/ExecuteRaster", payload.error());
            return ExecuteRaster(payload.value().command);
        }
        case pkg::D3D12OperationCode::EndQueueBatch:
        {
            auto* queue = QueueState(operation.queue);
            if (!operation.payload.empty() || queue == nullptr)
                return base::Failure<void, runtime::RuntimeError>(Error("frame/EndQueueBatch", "Packageが検証または実行の契約に違反しています。"));
            if (!commandOpen_ || activeCommandQueue_ != operation.queue.value)
                return base::Failure<void, runtime::RuntimeError>(Error("frame/EndQueueBatch", "検証または実行の契約に違反しています。"));
            if (options_.enableTimestampProfiling && timestampQueryIssued_ &&
                !timestampQueryResolved_ && operation.queue == timestampQueue_)
            {
                commandList_->ResolveQueryData(timestampQueryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP,
                    0, 2, timestampReadback_.Get(), 0);
                timestampQueryResolved_ = true;
            }
            const HRESULT hr = commandList_->Close();
            if (FAILED(hr)) return base::Failure<void, runtime::RuntimeError>(HResultError("frame/close-package-queue-command-list", hr, device_.Get()));
            commandOpen_ = false;
            activeCommandQueue_ = package::InvalidIndex;
            queue->commandOpen = false;
            ID3D12CommandList* lists[] = {commandList_.Get()};
            queue->nativeQueue->ExecuteCommandLists(1, lists);
            queue->frameSubmitted = true;
            return base::Success<void, runtime::RuntimeError>();
        }
        case pkg::D3D12OperationCode::SignalQueue:
        {
            auto payload = pkg::DecodeSignalQueue(operation.payload);
            if (!payload || !payload.value().signalPoint.IsValid() || !IsSupportedQueue(operation.queue))
                return base::Failure<void, runtime::RuntimeError>(Error("frame/SignalQueue", "Packageが検証または実行の契約に違反しています。"));
            if ((commandOpen_ && activeCommandQueue_ == operation.queue.value) ||
                !FrameQueueSubmitted(operation.queue) ||
                currentFrameSignalPoints_.contains(payload.value().signalPoint.value))
                return base::Failure<void, runtime::RuntimeError>(Error("frame/SignalQueue", "Packageが検証または実行の契約に違反しています。"));
            const auto value = NextFenceValue(operation.queue);
            const HRESULT hr = NativeQueue(operation.queue)->Signal(NativeFence(operation.queue), value);
            if (FAILED(hr)) return base::Failure<void, runtime::RuntimeError>(HResultError("frame/signal-queue", hr, device_.Get()));
            SetFrameFenceValue(operation.queue, value);
            currentFrameSignalPoints_[payload.value().signalPoint.value] = {operation.queue, value};
            SetFrameQueueSubmitted(operation.queue, false);
            return base::Success<void, runtime::RuntimeError>();
        }
        case pkg::D3D12OperationCode::WaitQueue:
        {
            auto payload = pkg::DecodeWaitQueue(operation.payload);
            if (!payload) return PackageFailure("frame/WaitQueue", payload.error());
            const auto signal = currentFrameSignalPoints_.find(payload.value().signalPoint.value);
            if (!IsSupportedQueue(operation.queue) || signal == currentFrameSignalPoints_.end() ||
                !IsSupportedQueue(signal->second.queue) || operation.queue == signal->second.queue)
                return base::Failure<void, runtime::RuntimeError>(Error("frame/WaitQueue", "Queueが検証または実行の契約に違反しています。"));
            const HRESULT hr = NativeQueue(operation.queue)->Wait(
                NativeFence(signal->second.queue), signal->second.fenceValue);
            if (FAILED(hr)) return base::Failure<void, runtime::RuntimeError>(HResultError("frame/wait-queue", hr, device_.Get()));
            return base::Success<void, runtime::RuntimeError>();
        }
        case pkg::D3D12OperationCode::WaitTemporal:
        {
            auto payload = pkg::DecodeWaitTemporal(operation.payload);
            if (!payload) return PackageFailure("frame/WaitTemporal", payload.error());
            const auto resource = payload.value().resource;
            if (!IsSupportedQueue(operation.queue) || !resource.IsValid() || resource.value >= view_.Resources().size() ||
                !IsTemporal(view_.Resources()[resource.value]))
                return base::Failure<void, runtime::RuntimeError>(Error("frame/WaitTemporal", "Packageが検証または実行の契約に違反しています。"));
            std::uint64_t dependency = 0;
            if (hasSubmittedFrame_)
            {
                const auto signal = previousFrameSignalPoints_.find(payload.value().producerSignalPoint.value);
                if (signal == previousFrameSignalPoints_.end() || !IsSupportedQueue(signal->second.queue))
                    return base::Failure<void, runtime::RuntimeError>(Error("frame/WaitTemporal", "Signalが検証または実行の契約に違反しています。"));
                dependency = signal->second.fenceValue;
                const HRESULT hr = NativeQueue(operation.queue)->Wait(
                    NativeFence(signal->second.queue), dependency);
                if (FAILED(hr)) return base::Failure<void, runtime::RuntimeError>(HResultError("frame/wait-temporal", hr, device_.Get()));
            }
            temporalWaitedResources_[resource.value] = true;
            temporalDependencyFenceValue_ = std::max(temporalDependencyFenceValue_, dependency);
            return base::Success<void, runtime::RuntimeError>();
        }
        case pkg::D3D12OperationCode::ReleaseExternal:
        {
            auto payload = pkg::DecodeReleaseExternal(operation.payload);
            if (!payload) return PackageFailure("frame/ReleaseExternal", payload.error());
            const auto slot = payload.value().slot.value;
            if (!payload.value().slot.IsValid() || slot >= view_.ExternalSlots().size() || !externalWaited_[slot] || externalReleased_[slot])
                return base::Failure<void, runtime::RuntimeError>(Error("frame/ReleaseExternal", "入力または内部状態がCanonicalな順序または識別子規則に違反しています。"));
            const auto signal = currentFrameSignalPoints_.find(payload.value().releaseSignalPoint.value);
            if (signal == currentFrameSignalPoints_.end() || !IsSupportedQueue(signal->second.queue))
                return base::Failure<void, runtime::RuntimeError>(Error("frame/ReleaseExternal", "Packageが検証または実行の契約に違反しています。"));
            const auto& contract = view_.ExternalSlots()[slot];
            if (!(TrackedState(contract.resource) == contract.guaranteedOutgoingState))
                return base::Failure<void, runtime::RuntimeError>(Error("frame/ReleaseExternal", "Packageが検証または実行の契約に違反しています。"));
            auto* native = dynamic_cast<ExternalResourceBase*>(externalBindings_[slot].resource.get());
            if (!native || native->Owner() != ExternalOwner())
                return base::Failure<void, runtime::RuntimeError>(
                    Error("frame/ReleaseExternal", "Resourceが検証または実行の契約に違反しています。"));
            native->SetCurrentState(contract.guaranteedOutgoingState);
            const auto releaseIdentity = domain_ ? native->Slot() : slot;
            frameExternalReleases_.push_back({slot, std::make_shared<CompletionToken>(
                FenceReference(signal->second.queue), signal->second.fenceValue, deviceEpoch_,
                ExternalOwner(), releaseIdentity)});
            externalReleased_[slot] = true;
            return base::Success<void, runtime::RuntimeError>();
        }
        case pkg::D3D12OperationCode::PresentSurface:
        {
            auto payload = pkg::DecodePresentSurface(operation.payload);
            if (!payload) return PackageFailure("frame/PresentSurface", payload.error());
            const auto slot = payload.value().slot.value;
            if (!payload.value().slot.IsValid() || slot >= view_.SurfaceSlots().size() ||
                !surfaceAcquired_[slot] || surfacePresented_[slot] || !swapChain_)
                return base::Failure<void, runtime::RuntimeError>(Error("frame/PresentSurface", "Surfaceが検証または実行の契約に違反しています。"));
            const auto& contract = view_.SurfaceSlots()[slot];
            if (!(TrackedState(contract.imageResource) == contract.presentedState))
                return base::Failure<void, runtime::RuntimeError>(Error("frame/PresentSurface", "Packageが検証または実行の契約に違反しています。"));
            const HRESULT hr = swapChain_->Present(1, 0);
            if (FAILED(hr)) return base::Failure<void, runtime::RuntimeError>(HResultError("frame/present", hr, device_.Get()));
            surfacePresented_[slot] = true;
            // DXGI Present is ordered on the Direct queue.  The following Package
            // SignalQueue must therefore be allowed to fence presentation work even
            // though no new command-list batch was closed after the preceding signal.
            SetFrameQueueSubmitted(DirectQueueId(), true);
            return base::Success<void, runtime::RuntimeError>();
        }
        default:
            return base::Failure<void, runtime::RuntimeError>(Error("frame", "Operationが検証または実行の契約に違反しています。"));
        }
    }

    base::Expected<void, runtime::RuntimeError> CreateDescriptorHeaps()
    {
        if (descriptorHeapsCreated_)
            return base::Failure<void, runtime::RuntimeError>(
                Error("load/CreateDescriptorHeaps", "検証または実行の契約に違反しています。"));

        HRESULT hr = S_OK;
        if (view_.Profile().rtvDescriptorCount != 0)
        {
            D3D12_DESCRIPTOR_HEAP_DESC rtvDesc{};
            rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            rtvDesc.NumDescriptors = view_.Profile().rtvDescriptorCount;
            rtvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            hr = device_->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&rtvHeap_));
            if (FAILED(hr))
                return base::Failure<void, runtime::RuntimeError>(
                    HResultError("load/create-rtv-heap", hr, device_.Get()));
            rtvIncrement_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        }

        if (view_.Profile().dsvDescriptorCount != 0)
        {
            D3D12_DESCRIPTOR_HEAP_DESC dsvDesc{};
            dsvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
            dsvDesc.NumDescriptors = view_.Profile().dsvDescriptorCount;
            dsvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            hr = device_->CreateDescriptorHeap(&dsvDesc, IID_PPV_ARGS(&dsvHeap_));
            if (FAILED(hr))
                return base::Failure<void, runtime::RuntimeError>(
                    HResultError("load/create-dsv-heap", hr, device_.Get()));
            dsvIncrement_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
        }

        if (view_.Profile().shaderDescriptorCount != 0)
        {
            D3D12_DESCRIPTOR_HEAP_DESC shaderDesc{};
            shaderDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            shaderDesc.NumDescriptors = view_.Profile().shaderDescriptorCount;
            shaderDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            hr = device_->CreateDescriptorHeap(&shaderDesc, IID_PPV_ARGS(&shaderHeap_));
            if (FAILED(hr))
                return base::Failure<void, runtime::RuntimeError>(
                    HResultError("load/create-shader-descriptor-heap", hr, device_.Get()));
            shaderDescriptorIncrement_ = device_->GetDescriptorHandleIncrementSize(
                D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        }

        backBuffers_.clear();
        if (!view_.SurfaceSlots().empty())
        {
            if (!swapChain_ || !rtvHeap_)
                return base::Failure<void, runtime::RuntimeError>(
                    Error("load/create-descriptor-heaps", "検証または実行の契約に違反しています。"));
            backBuffers_.resize(view_.Profile().surfaceImageCount);
            for (UINT instance = 0; instance < backBuffers_.size(); ++instance)
            {
                hr = swapChain_->GetBuffer(instance, IID_PPV_ARGS(&backBuffers_[instance]));
                if (FAILED(hr))
                    return base::Failure<void, runtime::RuntimeError>(
                        HResultError("load/get-surface-image", hr, device_.Get()));
            }
            for (const auto& resourceView : view_.Views())
            {
                if (resourceView.viewClass != pkg::ViewClass::RenderTarget) continue;
                const auto& resource = view_.Resources()[resourceView.resource.value];
                if (resource.origin != pkg::ResourceOrigin::Surface)
                    return base::Failure<void, runtime::RuntimeError>(
                        Error("load/create-descriptor-heaps",
                              "Packageが検証または実行の契約に違反しています。"));
                for (UINT instance = 0; instance < backBuffers_.size(); ++instance)
                {
                    auto handle = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
                    const auto descriptor = resourceView.descriptorIndex +
                        resourceView.descriptorInstanceStride * instance;
                    handle.ptr += static_cast<SIZE_T>(descriptor) * rtvIncrement_;
                    device_->CreateRenderTargetView(backBuffers_[instance].Get(), nullptr, handle);
                }
            }
        }
        descriptorHeapsCreated_ = true;
        return base::Success<void, runtime::RuntimeError>();
    }

    [[nodiscard]] bool IsFrameLocal(const pkg::ResourceArtifact& artifact) const noexcept
    {
        return (artifact.flags & static_cast<std::uint32_t>(pkg::ResourceFlags::FrameLocal)) != 0;
    }

    [[nodiscard]] bool IsTemporal(const pkg::ResourceArtifact& artifact) const noexcept
    {
        return (artifact.flags & static_cast<std::uint32_t>(pkg::ResourceFlags::Temporal)) != 0;
    }

    [[nodiscard]] std::uint32_t PhysicalInstanceIndex(pkg::ResourceId id) const noexcept
    {
        const auto& artifact = view_.Resources()[id.value];
        if (artifact.origin == pkg::ResourceOrigin::Surface) return currentBackBuffer_;
        if (IsFrameLocal(artifact) || IsTemporal(artifact)) return temporalCurrentInstance_;
        return 0u;
    }

    [[nodiscard]] std::uint32_t PhysicalInstanceIndex(const pkg::ResourceViewArtifact& resourceView) const noexcept
    {
        const auto& artifact = view_.Resources()[resourceView.resource.value];
        if (artifact.origin == pkg::ResourceOrigin::Surface) return currentBackBuffer_;
        if ((resourceView.flags & static_cast<std::uint32_t>(pkg::ResourceViewFlags::TemporalPrevious)) != 0)
            return temporalPreviousInstance_;
        if ((resourceView.flags & static_cast<std::uint32_t>(pkg::ResourceViewFlags::TemporalCurrent)) != 0)
            return temporalCurrentInstance_;
        if (IsFrameLocal(artifact) || IsTemporal(artifact)) return currentFrameSlot_;
        return 0u;
    }

    [[nodiscard]] std::uint32_t DescriptorIndex(const pkg::ResourceViewArtifact& resourceView) const noexcept
    {
        const auto& artifact = view_.Resources()[resourceView.resource.value];
        const std::uint32_t instance = artifact.origin == pkg::ResourceOrigin::External
            ? currentFrameSlot_
            : PhysicalInstanceIndex(resourceView);
        return resourceView.descriptorIndex + resourceView.descriptorInstanceStride * instance;
    }

    pkg::ResourceState& TrackedState(pkg::ResourceId id)
    {
        return resourceStates_[id.value][PhysicalInstanceIndex(id)];
    }

    const pkg::ResourceState& TrackedState(pkg::ResourceId id) const
    {
        return resourceStates_[id.value][PhysicalInstanceIndex(id)];
    }

    pkg::ResourceState& TrackedState(const pkg::ResourceViewArtifact& resourceView)
    {
        return resourceStates_[resourceView.resource.value][PhysicalInstanceIndex(resourceView)];
    }

    const pkg::ResourceState& TrackedState(const pkg::ResourceViewArtifact& resourceView) const
    {
        return resourceStates_[resourceView.resource.value][PhysicalInstanceIndex(resourceView)];
    }

    ID3D12Resource* ResourceObjectAt(pkg::ResourceId id, std::uint32_t instanceIndex)
    {
        if (!id.IsValid() || id.value >= view_.Resources().size()) return nullptr;
        const auto& artifact = view_.Resources()[id.value];
        if (artifact.origin == pkg::ResourceOrigin::Surface)
            return instanceIndex < backBuffers_.size() ? backBuffers_[instanceIndex].Get() : nullptr;
        if (artifact.origin == pkg::ResourceOrigin::External)
        {
            if (id.value >= externalNativeResources_.size()) return nullptr;
            auto* native = dynamic_cast<ExternalResourceBase*>(externalNativeResources_[id.value].get());
            return native ? native->Native() : nullptr;
        }
        return instanceIndex < resources_[id.value].size() ? resources_[id.value][instanceIndex].Get() : nullptr;
    }

    ID3D12Resource* ResourceObject(const pkg::ResourceViewArtifact& resourceView)
    {
        return ResourceObjectAt(resourceView.resource, PhysicalInstanceIndex(resourceView));
    }

    base::Expected<void, runtime::RuntimeError> CreateResource(pkg::ResourceId id)
    {
        if (!id.IsValid() || id.value >= view_.Resources().size())
            return base::Failure<void, runtime::RuntimeError>(Error("load/CreateResource", "Resourceが検証または実行の契約に違反しています。"));
        const auto& artifact = view_.Resources()[id.value];
        if (artifact.origin != pkg::ResourceOrigin::PackageOwned)
            return base::Failure<void, runtime::RuntimeError>(Error("load/CreateResource", "検証または実行の契約に違反しています。"));
        if (!artifact.allocation.IsValid() || artifact.allocation.value >= view_.Allocations().size())
            return base::Failure<void, runtime::RuntimeError>(Error("load/CreateResource", "Resourceが検証または実行の契約に違反しています。"));
        if (resources_[id.value].size() != artifact.physicalInstanceCount ||
            std::any_of(resources_[id.value].begin(), resources_[id.value].end(), [](const auto& value) { return value.Get() != nullptr; }))
            return base::Failure<void, runtime::RuntimeError>(Error("load/CreateResource", "Resourceが検証または実行の契約に違反しています。"));

        const auto& allocation = view_.Allocations()[artifact.allocation.value];
        if ((allocation.kind != pkg::AllocationKind::Committed && allocation.kind != pkg::AllocationKind::Placed) ||
            allocation.physicalInstanceCount != artifact.physicalInstanceCount ||
            (artifact.physicalInstanceCount != 1 && artifact.physicalInstanceCount != view_.Profile().framesInFlight))
            return base::Failure<void, runtime::RuntimeError>(Error("load/CreateResource", "Allocationが検証または実行の契約に違反しています。"));

        D3D12_HEAP_PROPERTIES heap{};
        heap.CreationNodeMask = 1;
        heap.VisibleNodeMask = 1;
        D3D12_RESOURCE_DESC desc{};
        D3D12_CLEAR_VALUE clearValue{};
        const D3D12_CLEAR_VALUE* optimizedClearValue = nullptr;
        D3D12_RESOURCE_STATES initialState = ToNativeState(artifact.initialState);

        if (artifact.resourceKind == pkg::ResourceKind::Buffer)
        {
            if ((allocation.kind == pkg::AllocationKind::Committed && allocation.sizeBytes != artifact.sizeBytes) ||
                (allocation.kind == pkg::AllocationKind::Placed && allocation.sizeBytes < artifact.sizeBytes) ||
                (allocation.heapClass != pkg::HeapClass::DefaultBuffer && allocation.heapClass != pkg::HeapClass::Upload))
                return base::Failure<void, runtime::RuntimeError>(Error("load/CreateResource", "Bufferが検証または実行の契約に違反しています。"));
            heap.Type = allocation.heapClass == pkg::HeapClass::Upload ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_DEFAULT;
            desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            desc.Width = artifact.sizeBytes;
            desc.Height = 1;
            desc.DepthOrArraySize = 1;
            desc.MipLevels = 1;
            desc.Format = DXGI_FORMAT_UNKNOWN;
            desc.SampleDesc.Count = 1;
            desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            for (std::uint32_t i = 0; i < artifact.viewCount; ++i)
            {
                const auto& resourceView = view_.Views()[artifact.firstView + i];
                if (resourceView.viewClass == pkg::ViewClass::UnorderedAccess)
                    desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            }
            initialState = allocation.heapClass == pkg::HeapClass::Upload
                ? D3D12_RESOURCE_STATE_GENERIC_READ
                : D3D12_RESOURCE_STATE_COMMON;
        }
        else if (artifact.resourceKind == pkg::ResourceKind::Texture2D)
        {
            if (artifact.physicalInstanceCount != 1 || artifact.depthOrArraySize != 1 || artifact.mipLevels != 1 || artifact.sampleCount != 1 || artifact.planeCount != 1)
                return base::Failure<void, runtime::RuntimeError>(Error("load/CreateResource", "Texture2Dが検証または実行の契約に違反しています。"));
            heap.Type = D3D12_HEAP_TYPE_DEFAULT;
            desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            desc.DepthOrArraySize = artifact.depthOrArraySize;
            desc.MipLevels = artifact.mipLevels;
            desc.Format = ToDxgi(artifact.format);
            desc.SampleDesc.Count = artifact.sampleCount;
            desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            if (artifact.format == pkg::Format::B8G8R8A8Unorm)
            {
                if (allocation.heapClass != pkg::HeapClass::DefaultTexture || artifact.extentMode != pkg::ExtentMode::Fixed ||
                    artifact.width == 0 || artifact.height == 0 || artifact.initialDataSize == 0)
                    return base::Failure<void, runtime::RuntimeError>(Error("load/CreateResource", "Texture2Dが検証または実行の契約に違反しています。"));
                desc.Width = artifact.width;
                desc.Height = artifact.height;
            }
            else if (artifact.format == pkg::Format::D32Float)
            {
                if (allocation.heapClass != pkg::HeapClass::RenderTargetOrDepth || artifact.extentMode != pkg::ExtentMode::SurfaceRelative ||
                    artifact.width != 0 || artifact.height != 0 || artifact.initialDataSize != 0)
                    return base::Failure<void, runtime::RuntimeError>(Error("load/CreateResource", "Contractが検証または実行の契約に違反しています。"));
                if (!surface_)
                    return base::Failure<void, runtime::RuntimeError>(Error("load/CreateResource", "Resourceが検証または実行の契約に違反しています。"));
                desc.Width = std::max(1u, surface_->ClientWidth());
                desc.Height = std::max(1u, surface_->ClientHeight());
                desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
                clearValue.Format = DXGI_FORMAT_D32_FLOAT;
                clearValue.DepthStencil.Depth = 1.0f;
                clearValue.DepthStencil.Stencil = 0;
                optimizedClearValue = &clearValue;
            }
            else
            {
                return base::Failure<void, runtime::RuntimeError>(Error("load/CreateResource", "Texture2Dが検証または実行の契約に違反しています。"));
            }
        }
        else
        {
            return base::Failure<void, runtime::RuntimeError>(Error("load/CreateResource", "Packageが検証または実行の契約に違反しています。"));
        }

        for (std::uint32_t instanceIndex = 0; instanceIndex < artifact.physicalInstanceCount; ++instanceIndex)
        {
            HRESULT hr = S_OK;
            if (allocation.kind == pkg::AllocationKind::Committed)
            {
                hr = device_->CreateCommittedResource(
                    &heap, D3D12_HEAP_FLAG_NONE, &desc, initialState, optimizedClearValue,
                    IID_PPV_ARGS(&resources_[id.value][instanceIndex]));
            }
            else
            {
                if (allocation.heapClass != pkg::HeapClass::DefaultBuffer || allocation.aliasGroup == package::InvalidIndex)
                    return base::Failure<void, runtime::RuntimeError>(Error("load/CreateResource", "検証または実行の契約に違反しています。"));
                if (placedHeaps_[artifact.allocation.value].empty())
                    placedHeaps_[artifact.allocation.value].resize(allocation.physicalInstanceCount);
                if (!placedHeaps_[artifact.allocation.value][instanceIndex])
                {
                    D3D12_HEAP_DESC heapDesc{};
                    heapDesc.SizeInBytes = allocation.sizeBytes;
                    heapDesc.Alignment = allocation.alignment;
                    heapDesc.Properties = heap;
                    heapDesc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;
                    hr = device_->CreateHeap(&heapDesc, IID_PPV_ARGS(&placedHeaps_[artifact.allocation.value][instanceIndex]));
                    if (FAILED(hr)) return base::Failure<void, runtime::RuntimeError>(HResultError("load/create-alias-heap", hr, device_.Get()));
                }
                hr = device_->CreatePlacedResource(
                    placedHeaps_[artifact.allocation.value][instanceIndex].Get(), 0, &desc, initialState, optimizedClearValue,
                    IID_PPV_ARGS(&resources_[id.value][instanceIndex]));
            }
            if (FAILED(hr)) return base::Failure<void, runtime::RuntimeError>(HResultError("load/create-resource", hr, device_.Get()));

            for (std::uint32_t i = 0; i < artifact.viewCount; ++i)
            {
                const auto& resourceView = view_.Views()[artifact.firstView + i];
                const std::uint32_t descriptorIndex = resourceView.descriptorIndex + resourceView.descriptorInstanceStride * instanceIndex;
                if (resourceView.viewClass == pkg::ViewClass::ShaderResource)
                {
                    if (!shaderHeap_)
                        return base::Failure<void, runtime::RuntimeError>(Error("load/CreateResource", "Shaderが検証または実行の契約に違反しています。"));
                    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
                    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                    if (artifact.resourceKind == pkg::ResourceKind::Texture2D)
                    {
                        srv.Format = ToDxgi(resourceView.format);
                        srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                        srv.Texture2D.MostDetailedMip = resourceView.firstMip;
                        srv.Texture2D.MipLevels = resourceView.mipCount;
                        srv.Texture2D.PlaneSlice = resourceView.firstPlane;
                        srv.Texture2D.ResourceMinLODClamp = 0.0f;
                    }
                    else if (artifact.resourceKind == pkg::ResourceKind::Buffer)
                    {
                        if (resourceView.strideBytes == 0 || resourceView.byteSize == 0 || resourceView.byteSize % resourceView.strideBytes != 0)
                            return base::Failure<void, runtime::RuntimeError>(Error("load/CreateResource", "Bufferが検証または実行の契約に違反しています。"));
                        srv.Format = DXGI_FORMAT_UNKNOWN;
                        srv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
                        srv.Buffer.FirstElement = resourceView.byteOffset / resourceView.strideBytes;
                        srv.Buffer.NumElements = static_cast<UINT>(resourceView.byteSize / resourceView.strideBytes);
                        srv.Buffer.StructureByteStride = resourceView.strideBytes;
                        srv.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
                    }
                    else
                    {
                        return base::Failure<void, runtime::RuntimeError>(Error("load/CreateResource", "Resourceが検証または実行の契約に違反しています。"));
                    }
                    auto handle = shaderHeap_->GetCPUDescriptorHandleForHeapStart();
                    handle.ptr += static_cast<SIZE_T>(descriptorIndex) * shaderDescriptorIncrement_;
                    device_->CreateShaderResourceView(resources_[id.value][instanceIndex].Get(), &srv, handle);
                }
                else if (resourceView.viewClass == pkg::ViewClass::UnorderedAccess)
                {
                    if (!shaderHeap_ || artifact.resourceKind != pkg::ResourceKind::Buffer || resourceView.strideBytes == 0 ||
                        resourceView.byteSize == 0 || resourceView.byteSize % resourceView.strideBytes != 0)
                        return base::Failure<void, runtime::RuntimeError>(Error("load/CreateResource", "Bufferが検証または実行の契約に違反しています。"));
                    D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};
                    uav.Format = DXGI_FORMAT_UNKNOWN;
                    uav.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
                    uav.Buffer.FirstElement = resourceView.byteOffset / resourceView.strideBytes;
                    uav.Buffer.NumElements = static_cast<UINT>(resourceView.byteSize / resourceView.strideBytes);
                    uav.Buffer.StructureByteStride = resourceView.strideBytes;
                    uav.Buffer.CounterOffsetInBytes = 0;
                    uav.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
                    auto handle = shaderHeap_->GetCPUDescriptorHandleForHeapStart();
                    handle.ptr += static_cast<SIZE_T>(descriptorIndex) * shaderDescriptorIncrement_;
                    device_->CreateUnorderedAccessView(resources_[id.value][instanceIndex].Get(), nullptr, &uav, handle);
                }
                else if (resourceView.viewClass == pkg::ViewClass::DepthStencil)
                {
                    if (!dsvHeap_ || artifact.physicalInstanceCount != 1)
                        return base::Failure<void, runtime::RuntimeError>(Error("load/CreateResource", "Contractが検証または実行の契約に違反しています。"));
                    D3D12_DEPTH_STENCIL_VIEW_DESC dsv{};
                    dsv.Format = ToDxgi(resourceView.format);
                    dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
                    dsv.Flags = D3D12_DSV_FLAG_NONE;
                    dsv.Texture2D.MipSlice = resourceView.firstMip;
                    auto handle = dsvHeap_->GetCPUDescriptorHandleForHeapStart();
                    handle.ptr += static_cast<SIZE_T>(descriptorIndex) * dsvIncrement_;
                    device_->CreateDepthStencilView(resources_[id.value][instanceIndex].Get(), &dsv, handle);
                }
            }
        }
        return base::Success<void, runtime::RuntimeError>();
    }

    base::Expected<void, runtime::RuntimeError> ActivateAlias(const pkg::ActivateAliasPayload& payload, ID3D12GraphicsCommandList* list)
    {
        if (!list || !payload.after.IsValid() || payload.after.value >= view_.Resources().size())
            return base::Failure<void, runtime::RuntimeError>(Error("load/ActivateAlias", "Payloadが検証または実行の契約に違反しています。"));
        const auto& after = view_.Resources()[payload.after.value];
        if (!after.allocation.IsValid() || after.allocation.value >= view_.Allocations().size())
            return base::Failure<void, runtime::RuntimeError>(Error("load/ActivateAlias", "Resourceが検証または実行の契約に違反しています。"));
        const auto& allocation = view_.Allocations()[after.allocation.value];
        if (allocation.kind != pkg::AllocationKind::Placed || allocation.aliasGroup == package::InvalidIndex ||
            allocation.aliasGroup >= activeAliasResources_.size())
            return base::Failure<void, runtime::RuntimeError>(Error("load/ActivateAlias", "検証または実行の契約に違反しています。"));
        ID3D12Resource* beforeObject = nullptr;
        if (payload.before.IsValid())
        {
            if (payload.before.value >= view_.Resources().size())
                return base::Failure<void, runtime::RuntimeError>(Error("load/ActivateAlias", "Resourceが検証または実行の契約に違反しています。"));
            const auto& before = view_.Resources()[payload.before.value];
            if (before.allocation != after.allocation || before.flags != after.flags ||
                activeAliasResources_[allocation.aliasGroup] != payload.before.value)
                return base::Failure<void, runtime::RuntimeError>(Error("load/ActivateAlias", "Allocationが検証または実行の契約に違反しています。"));
            beforeObject = ResourceObjectAt(payload.before, 0);
        }
        else if (activeAliasResources_[allocation.aliasGroup] != package::InvalidIndex)
        {
            return base::Failure<void, runtime::RuntimeError>(Error("load/ActivateAlias", "検証または実行の契約に違反しています。"));
        }
        ID3D12Resource* afterObject = ResourceObjectAt(payload.after, 0);
        if (!afterObject) return base::Failure<void, runtime::RuntimeError>(Error("load/ActivateAlias", "Resourceが検証または実行の契約に違反しています。"));
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_ALIASING;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Aliasing.pResourceBefore = beforeObject;
        barrier.Aliasing.pResourceAfter = afterObject;
        list->ResourceBarrier(1, &barrier);
        activeAliasResources_[allocation.aliasGroup] = payload.after.value;
        return base::Success<void, runtime::RuntimeError>();
    }

    base::Expected<void, runtime::RuntimeError> UploadBuffer(const pkg::UploadBufferPayload& payload)
    {
        if (!loadBatchOpen_)
            return base::Failure<void, runtime::RuntimeError>(Error("load/UploadBuffer", "検証または実行の契約に違反しています。"));
        if (!payload.resource.IsValid() || payload.resource.value >= resources_.size())
            return base::Failure<void, runtime::RuntimeError>(Error("load/UploadBuffer", "Resourceが検証または実行の契約に違反しています。"));
        const auto& artifact = view_.Resources()[payload.resource.value];
        if (artifact.resourceKind != pkg::ResourceKind::Buffer || artifact.origin != pkg::ResourceOrigin::PackageOwned ||
            payload.bytes == 0 || payload.bytes > artifact.sizeBytes || payload.sourceOffset > view_.InitialData().size() ||
            payload.bytes > view_.InitialData().size() - payload.sourceOffset)
            return base::Failure<void, runtime::RuntimeError>(Error("load/UploadBuffer", "Bufferが検証または実行の契約に違反しています。"));

        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_UPLOAD;
        heap.CreationNodeMask = 1;
        heap.VisibleNodeMask = 1;
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = payload.bytes;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        ComPtr<ID3D12Resource> upload;
        HRESULT hr = device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&upload));
        if (FAILED(hr))
            return base::Failure<void, runtime::RuntimeError>(HResultError("load/create-upload-buffer", hr, device_.Get()));
        void* mapped = nullptr;
        D3D12_RANGE readRange{0, 0};
        hr = upload->Map(0, &readRange, &mapped);
        if (FAILED(hr))
            return base::Failure<void, runtime::RuntimeError>(HResultError("load/map-upload-buffer", hr, device_.Get()));
        std::memcpy(mapped, view_.InitialData().data() + payload.sourceOffset, static_cast<std::size_t>(payload.bytes));
        upload->Unmap(0, nullptr);

        const std::uint32_t instances = IsTemporal(artifact) ? artifact.physicalInstanceCount : 1u;
        for (std::uint32_t instanceIndex = 0; instanceIndex < instances; ++instanceIndex)
        {
            ID3D12Resource* destination = ResourceObjectAt(payload.resource, instanceIndex);
            if (!destination)
                return base::Failure<void, runtime::RuntimeError>(Error("load/UploadBuffer", "Resourceが検証または実行の契約に違反しています。"));
            loadCommandList_->CopyBufferRegion(destination, 0, upload.Get(), 0, payload.bytes);
        }
        uploadResources_.push_back(std::move(upload));
        return base::Success<void, runtime::RuntimeError>();
    }

    base::Expected<void, runtime::RuntimeError> ExecuteCopy(const pkg::CopyBufferPayload& payload, const char* stage = "load/ExecuteCopy")
    {
        ID3D12GraphicsCommandList* list = nullptr;
        if (loadBatchOpen_) list = loadCommandList_.Get();
        else if (commandOpen_)
        {
            const auto* queue = QueueState(pkg::QueueId{activeCommandQueue_});
            if (queue && (queue->type == D3D12_COMMAND_LIST_TYPE_DIRECT || queue->type == D3D12_COMMAND_LIST_TYPE_COPY))
                list = commandList_.Get();
        }
        if (!list)
            return base::Failure<void, runtime::RuntimeError>(
                Error(stage, "検証または実行の契約に違反しています。"));
        if (!payload.sourceView.IsValid() || !payload.destinationView.IsValid() ||
            payload.sourceView.value >= view_.Views().size() ||
            payload.destinationView.value >= view_.Views().size() || payload.bytes == 0)
            return base::Failure<void, runtime::RuntimeError>(
                Error(stage, "Viewが検証または実行の契約に違反しています。"));
        const auto& sourceView = view_.Views()[payload.sourceView.value];
        const auto& destinationView = view_.Views()[payload.destinationView.value];
        if (sourceView.viewClass != pkg::ViewClass::CopySource ||
            destinationView.viewClass != pkg::ViewClass::CopyDestination ||
            !sourceView.resource.IsValid() || !destinationView.resource.IsValid() ||
            sourceView.resource.value >= view_.Resources().size() ||
            destinationView.resource.value >= view_.Resources().size() ||
            ResourceObject(sourceView) == nullptr || ResourceObject(destinationView) == nullptr)
            return base::Failure<void, runtime::RuntimeError>(
                Error(stage, "Viewが検証または実行の契約に違反しています。"));
        const auto& sourceArtifact = view_.Resources()[sourceView.resource.value];
        const auto& destinationArtifact = view_.Resources()[destinationView.resource.value];
        if (sourceArtifact.resourceKind != pkg::ResourceKind::Buffer ||
            destinationArtifact.resourceKind != pkg::ResourceKind::Buffer ||
            payload.sourceOffset > sourceView.byteSize ||
            payload.bytes > sourceView.byteSize - payload.sourceOffset ||
            payload.destinationOffset > destinationView.byteSize ||
            payload.bytes > destinationView.byteSize - payload.destinationOffset)
            return base::Failure<void, runtime::RuntimeError>(Error(stage, "入力または内部状態が検証または実行の契約に違反しています。"));
        const pkg::ResourceState copySourceState{pkg::StateClass::Explicit, 0,
            static_cast<std::uint32_t>(pkg::ExplicitStateBits::CopySource)};
        const pkg::ResourceState copyDestinationState{pkg::StateClass::Explicit, 0,
            static_cast<std::uint32_t>(pkg::ExplicitStateBits::CopyDestination)};
        if (TrackedState(sourceView) != copySourceState || TrackedState(destinationView) != copyDestinationState)
            return base::Failure<void, runtime::RuntimeError>(
                Error(stage, "検証または実行の契約に違反しています。"));
        list->CopyBufferRegion(ResourceObject(destinationView),
            destinationView.byteOffset + payload.destinationOffset,
            ResourceObject(sourceView), sourceView.byteOffset + payload.sourceOffset, payload.bytes);
        return base::Success<void, runtime::RuntimeError>();
    }


    base::Expected<void, runtime::RuntimeError> UploadTexture(const pkg::UploadTexturePayload& payload)
    {
        if (!loadBatchOpen_)
            return base::Failure<void, runtime::RuntimeError>(Error("load/UploadTexture", "検証または実行の契約に違反しています。"));
        if (!payload.resource.IsValid() || payload.resource.value >= resources_.size() || ResourceObject(payload.resource) == nullptr)
            return base::Failure<void, runtime::RuntimeError>(Error("load/UploadTexture", "検証または実行の契約に違反しています。"));
        const auto& artifact = view_.Resources()[payload.resource.value];
        if (artifact.resourceKind != pkg::ResourceKind::Texture2D || artifact.origin != pkg::ResourceOrigin::PackageOwned)
            return base::Failure<void, runtime::RuntimeError>(Error("load/UploadTexture", "検証または実行の契約に違反しています。"));
        if (payload.sourceOffset > view_.InitialData().size() || payload.sourceSliceBytes > view_.InitialData().size() - payload.sourceOffset ||
            payload.sourceRowBytes != artifact.width * 4u || payload.sourceSliceBytes != payload.sourceRowBytes * artifact.height ||
            payload.mipLevel != 0 || payload.arrayLayer != 0 || payload.plane != 0)
            return base::Failure<void, runtime::RuntimeError>(Error("load/UploadTexture", "Textureが検証または実行の契約に違反しています。"));

        const auto textureDesc = ResourceObject(payload.resource)->GetDesc();
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
        UINT rows = 0;
        UINT64 rowBytes = 0;
        UINT64 uploadBytes = 0;
        device_->GetCopyableFootprints(&textureDesc, 0, 1, 0, &footprint, &rows, &rowBytes, &uploadBytes);
        if (rows != artifact.height || rowBytes != payload.sourceRowBytes || uploadBytes == 0)
            return base::Failure<void, runtime::RuntimeError>(Error("load/UploadTexture", "Packageが検証または実行の契約に違反しています。"));

        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_UPLOAD;
        heap.CreationNodeMask = 1;
        heap.VisibleNodeMask = 1;
        D3D12_RESOURCE_DESC uploadDesc{};
        uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        uploadDesc.Width = uploadBytes;
        uploadDesc.Height = 1;
        uploadDesc.DepthOrArraySize = 1;
        uploadDesc.MipLevels = 1;
        uploadDesc.Format = DXGI_FORMAT_UNKNOWN;
        uploadDesc.SampleDesc.Count = 1;
        uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        ComPtr<ID3D12Resource> upload;
        HRESULT hr = device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &uploadDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&upload));
        if (FAILED(hr)) return base::Failure<void, runtime::RuntimeError>(HResultError("load/create-texture-upload", hr, device_.Get()));

        void* mapped = nullptr;
        D3D12_RANGE readRange{0, 0};
        hr = upload->Map(0, &readRange, &mapped);
        if (FAILED(hr)) return base::Failure<void, runtime::RuntimeError>(HResultError("load/map-texture-upload", hr, device_.Get()));
        const auto* source = view_.InitialData().data() + payload.sourceOffset;
        auto* destination = static_cast<std::byte*>(mapped) + footprint.Offset;
        for (UINT row = 0; row < rows; ++row)
        {
            std::memcpy(destination + static_cast<std::size_t>(row) * footprint.Footprint.RowPitch,
                source + static_cast<std::size_t>(row) * payload.sourceRowBytes,
                payload.sourceRowBytes);
        }
        upload->Unmap(0, nullptr);

        D3D12_TEXTURE_COPY_LOCATION destinationLocation{};
        destinationLocation.pResource = ResourceObject(payload.resource);
        destinationLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        destinationLocation.SubresourceIndex = 0;
        D3D12_TEXTURE_COPY_LOCATION sourceLocation{};
        sourceLocation.pResource = upload.Get();
        sourceLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        sourceLocation.PlacedFootprint = footprint;
        loadCommandList_->CopyTextureRegion(&destinationLocation, 0, 0, 0, &sourceLocation, nullptr);
        uploadResources_.push_back(std::move(upload));
        return base::Success<void, runtime::RuntimeError>();
    }

    base::Expected<void, runtime::RuntimeError> ScheduleBufferVerification(const pkg::VerifyBufferContentsPayload& payload)
    {
        if (!payload.resource.IsValid() || payload.resource.value >= resources_.size())
            return base::Failure<void, runtime::RuntimeError>(Error("load/VerifyBufferContents", "Bufferが検証または実行の契約に違反しています。"));
        if (payload.bytes == 0)
            return base::Failure<void, runtime::RuntimeError>(Error("load/VerifyBufferContents", "入力または内部状態の数値条件を満たしていません。"));
        const auto& artifact = view_.Resources()[payload.resource.value];
        if (artifact.resourceKind != pkg::ResourceKind::Buffer || artifact.origin != pkg::ResourceOrigin::PackageOwned ||
            payload.resourceOffset > artifact.sizeBytes || payload.bytes > artifact.sizeBytes - payload.resourceOffset ||
            payload.expectedDataOffset > view_.InitialData().size() || payload.bytes > view_.InitialData().size() - payload.expectedDataOffset)
            return base::Failure<void, runtime::RuntimeError>(Error("load/VerifyBufferContents", "Bufferが検証または実行の契約に違反しています。"));
        const pkg::ResourceState requiredState{
            pkg::StateClass::Explicit, 0, static_cast<std::uint32_t>(pkg::ExplicitStateBits::CopySource)};

        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_READBACK;
        heap.CreationNodeMask = 1;
        heap.VisibleNodeMask = 1;
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = payload.bytes;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        const std::uint32_t instances = IsTemporal(artifact) ? artifact.physicalInstanceCount : 1u;
        for (std::uint32_t instanceIndex = 0; instanceIndex < instances; ++instanceIndex)
        {
            if (resourceStates_[payload.resource.value][instanceIndex] != requiredState)
                return base::Failure<void, runtime::RuntimeError>(Error("load/VerifyBufferContents", "検証または実行の契約に違反しています。"));
            ID3D12Resource* source = ResourceObjectAt(payload.resource, instanceIndex);
            if (!source)
                return base::Failure<void, runtime::RuntimeError>(Error("load/VerifyBufferContents", "Bufferが検証または実行の契約に違反しています。"));
            PendingBufferVerification pending;
            pending.resource = payload.resource;
            pending.instanceIndex = instanceIndex;
            pending.expectedDataOffset = payload.expectedDataOffset;
            pending.bytes = payload.bytes;
            HRESULT hr = device_->CreateCommittedResource(
                &heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                IID_PPV_ARGS(&pending.readback));
            if (FAILED(hr))
                return base::Failure<void, runtime::RuntimeError>(HResultError("load/create-readback-buffer", hr, device_.Get()));
            loadCommandList_->CopyBufferRegion(pending.readback.Get(), 0, source, payload.resourceOffset, payload.bytes);
            pendingBufferVerifications_.push_back(std::move(pending));
        }
        return base::Success<void, runtime::RuntimeError>();
    }

    base::Expected<void, runtime::RuntimeError> CompleteBufferVerifications()
    {
        for (auto& pending : pendingBufferVerifications_)
        {
            void* mapped = nullptr;
            const D3D12_RANGE readRange{0, static_cast<SIZE_T>(pending.bytes)};
            const HRESULT hr = pending.readback->Map(0, &readRange, &mapped);
            if (FAILED(hr))
                return base::Failure<void, runtime::RuntimeError>(HResultError("load/map-readback-buffer", hr, device_.Get()));

            const auto expected = view_.InitialData().subspan(
                static_cast<std::size_t>(pending.expectedDataOffset),
                static_cast<std::size_t>(pending.bytes));
            const auto* actual = static_cast<const std::byte*>(mapped);
            std::size_t firstMismatch = expected.size();
            for (std::size_t i = 0; i < expected.size(); ++i)
            {
                if (actual[i] != expected[i])
                {
                    firstMismatch = i;
                    break;
                }
            }
            unsigned int expectedValue = 0;
            unsigned int actualValue = 0;
            if (firstMismatch != expected.size())
            {
                expectedValue = std::to_integer<unsigned int>(expected[firstMismatch]);
                actualValue = std::to_integer<unsigned int>(actual[firstMismatch]);
            }
            const D3D12_RANGE noWrite{0, 0};
            pending.readback->Unmap(0, &noWrite);
            if (firstMismatch != expected.size())
            {
                std::ostringstream message;
                message << "PackageがCanonicalな順序または識別子規則に違反しています。"
                        << pending.resource.value << "入力または内部状態が検証または実行の契約に違反しています。" << pending.instanceIndex << "入力または内部状態が検証または実行の契約に違反しています。" << firstMismatch
                        << "入力または内部状態が検証または実行の契約に違反しています。" << expectedValue
                        << "入力または内部状態が検証または実行の契約に違反しています。" << actualValue << ')';
                return base::Failure<void, runtime::RuntimeError>(
                    Error("load/VerifyBufferContents", message.str()));
            }
        }
        pendingBufferVerifications_.clear();
        return base::Success<void, runtime::RuntimeError>();
    }

    base::Expected<void, runtime::RuntimeError> ScheduleTextureVerification(const pkg::VerifyTextureContentsPayload& payload)
    {
        if (!payload.resource.IsValid() || payload.resource.value >= resources_.size() || ResourceObject(payload.resource) == nullptr)
            return base::Failure<void, runtime::RuntimeError>(Error("load/VerifyTextureContents", "検証または実行の契約に違反しています。"));
        const auto& artifact = view_.Resources()[payload.resource.value];
        if (artifact.resourceKind != pkg::ResourceKind::Texture2D || artifact.origin != pkg::ResourceOrigin::PackageOwned ||
            payload.expectedRowBytes != artifact.width * 4u || payload.width != artifact.width || payload.height != artifact.height ||
            payload.expectedDataOffset > view_.InitialData().size() ||
            static_cast<std::uint64_t>(payload.expectedRowBytes) * payload.height > view_.InitialData().size() - payload.expectedDataOffset)
            return base::Failure<void, runtime::RuntimeError>(Error("load/VerifyTextureContents", "Texture2Dが検証または実行の契約に違反しています。"));
        const pkg::ResourceState requiredState{pkg::StateClass::Explicit, 0, static_cast<std::uint32_t>(pkg::ExplicitStateBits::CopySource)};
        if (TrackedState(payload.resource) != requiredState)
            return base::Failure<void, runtime::RuntimeError>(Error("load/VerifyTextureContents", "検証または実行の契約に違反しています。"));

        const auto textureDesc = ResourceObject(payload.resource)->GetDesc();
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
        UINT rows = 0;
        UINT64 rowBytes = 0;
        UINT64 readbackBytes = 0;
        device_->GetCopyableFootprints(&textureDesc, 0, 1, 0, &footprint, &rows, &rowBytes, &readbackBytes);
        if (rows != payload.height || rowBytes != payload.expectedRowBytes || readbackBytes == 0)
            return base::Failure<void, runtime::RuntimeError>(Error("load/VerifyTextureContents", "検証または実行の契約に違反しています。"));

        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_READBACK;
        heap.CreationNodeMask = 1;
        heap.VisibleNodeMask = 1;
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = readbackBytes;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        PendingTextureVerification pending;
        pending.resource = payload.resource;
        pending.expectedDataOffset = payload.expectedDataOffset;
        pending.expectedRowBytes = payload.expectedRowBytes;
        pending.width = payload.width;
        pending.height = payload.height;
        pending.footprint = footprint;
        HRESULT hr = device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&pending.readback));
        if (FAILED(hr)) return base::Failure<void, runtime::RuntimeError>(HResultError("load/create-texture-readback", hr, device_.Get()));

        D3D12_TEXTURE_COPY_LOCATION destination{};
        destination.pResource = pending.readback.Get();
        destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        destination.PlacedFootprint = footprint;
        D3D12_TEXTURE_COPY_LOCATION source{};
        source.pResource = ResourceObject(payload.resource);
        source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        source.SubresourceIndex = 0;
        loadCommandList_->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
        pendingTextureVerifications_.push_back(std::move(pending));
        return base::Success<void, runtime::RuntimeError>();
    }

    base::Expected<void, runtime::RuntimeError> CompleteTextureVerifications()
    {
        for (auto& pending : pendingTextureVerifications_)
        {
            const auto readSize = static_cast<SIZE_T>(pending.footprint.Offset +
                static_cast<UINT64>(pending.footprint.Footprint.RowPitch) * (pending.height - 1u) +
                pending.expectedRowBytes);
            void* mapped = nullptr;
            const D3D12_RANGE readRange{0, readSize};
            const HRESULT hr = pending.readback->Map(0, &readRange, &mapped);
            if (FAILED(hr)) return base::Failure<void, runtime::RuntimeError>(HResultError("load/map-texture-readback", hr, device_.Get()));

            const auto* actualBase = static_cast<const std::byte*>(mapped) + pending.footprint.Offset;
            const auto* expectedBase = view_.InitialData().data() + pending.expectedDataOffset;
            std::size_t mismatch = static_cast<std::size_t>(pending.expectedRowBytes) * pending.height;
            unsigned int expectedValue = 0;
            unsigned int actualValue = 0;
            for (std::uint32_t row = 0; row < pending.height && mismatch == static_cast<std::size_t>(pending.expectedRowBytes) * pending.height; ++row)
            {
                const auto* actualRow = actualBase + static_cast<std::size_t>(row) * pending.footprint.Footprint.RowPitch;
                const auto* expectedRow = expectedBase + static_cast<std::size_t>(row) * pending.expectedRowBytes;
                for (std::uint32_t columnByte = 0; columnByte < pending.expectedRowBytes; ++columnByte)
                {
                    if (actualRow[columnByte] != expectedRow[columnByte])
                    {
                        mismatch = static_cast<std::size_t>(row) * pending.expectedRowBytes + columnByte;
                        expectedValue = std::to_integer<unsigned int>(expectedRow[columnByte]);
                        actualValue = std::to_integer<unsigned int>(actualRow[columnByte]);
                        break;
                    }
                }
            }
            const D3D12_RANGE noWrite{0, 0};
            pending.readback->Unmap(0, &noWrite);
            if (mismatch != static_cast<std::size_t>(pending.expectedRowBytes) * pending.height)
            {
                std::ostringstream message;
                message << "PackageがCanonicalな順序または識別子規則に違反しています。" << pending.resource.value
                        << "入力または内部状態が検証または実行の契約に違反しています。" << mismatch << "入力または内部状態が検証または実行の契約に違反しています。" << expectedValue << "入力または内部状態が検証または実行の契約に違反しています。" << actualValue << ')';
                return base::Failure<void, runtime::RuntimeError>(Error("load/VerifyTextureContents", message.str()));
            }
        }
        pendingTextureVerifications_.clear();
        return base::Success<void, runtime::RuntimeError>();
    }

    base::Expected<void, runtime::RuntimeError> ApplyDynamicData(pkg::DynamicSlotId id)
    {
        if (!id.IsValid() || id.value >= view_.DynamicSlots().size())
            return base::Failure<void, runtime::RuntimeError>(Error("frame/ApplyDynamicData", "入力または内部状態が検証または実行の契約に違反しています。"));
        if (dynamicApplied_[id.value])
            return base::Failure<void, runtime::RuntimeError>(Error("frame/ApplyDynamicData", "検証または実行の契約に違反しています。"));

        const auto& slot = view_.DynamicSlots()[id.value];
        const auto bytes = dynamicBindings_[id.value];
        if (bytes.size() != slot.requiredBytes)
            return base::Failure<void, runtime::RuntimeError>(Error("frame/ApplyDynamicData", "Invocationが検証または実行の契約に違反しています。"));
        if (!slot.destinationResource.IsValid() || slot.destinationResource.value >= resources_.size() || ResourceObject(slot.destinationResource) == nullptr)
            return base::Failure<void, runtime::RuntimeError>(Error("frame/ApplyDynamicData", "Resourceが検証または実行の契約に違反しています。"));

        void* mapped = nullptr;
        D3D12_RANGE readRange{0, 0};
        const HRESULT hr = ResourceObject(slot.destinationResource)->Map(0, &readRange, &mapped);
        if (FAILED(hr))
            return base::Failure<void, runtime::RuntimeError>(HResultError("frame/map-dynamic-buffer", hr, device_.Get()));
        auto* destination = static_cast<std::byte*>(mapped) + slot.destinationOffset;
        std::memcpy(destination, bytes.data(), bytes.size());
        D3D12_RANGE writtenRange{
            static_cast<SIZE_T>(slot.destinationOffset),
            static_cast<SIZE_T>(slot.destinationOffset + slot.requiredBytes)};
        ResourceObject(slot.destinationResource)->Unmap(0, &writtenRange);
        dynamicApplied_[id.value] = true;
        return base::Success<void, runtime::RuntimeError>();
    }

    base::Expected<void, runtime::RuntimeError> TransitionResourceAt(
        pkg::ResourceId id,
        std::uint32_t instanceIndex,
        const pkg::ResourceState& before,
        const pkg::ResourceState& after,
        ID3D12GraphicsCommandList* list,
        D3D12_COMMAND_LIST_TYPE queueType)
    {
        if (!id.IsValid() || id.value >= resourceStates_.size() || instanceIndex >= resourceStates_[id.value].size())
            return base::Failure<void, runtime::RuntimeError>(Error("transition", "Resourceが検証または実行の契約に違反しています。"));
        if (resourceStates_[id.value][instanceIndex] != before)
            return base::Failure<void, runtime::RuntimeError>(Error("transition", "Packageが検証または実行の契約に違反しています。"));
        ID3D12Resource* resource = ResourceObjectAt(id, instanceIndex);
        if (!resource) return base::Failure<void, runtime::RuntimeError>(Error("transition", "Resourceが検証または実行の契約に違反しています。"));
        const auto nativeBefore = ToNativeState(before, queueType);
        const auto nativeAfter = ToNativeState(after, queueType);
        if (nativeBefore != nativeAfter)
        {
            auto barrier = TransitionBarrier(resource, nativeBefore, nativeAfter);
            list->ResourceBarrier(1, &barrier);
        }
        resourceStates_[id.value][instanceIndex] = after;
        return base::Success<void, runtime::RuntimeError>();
    }

    base::Expected<void, runtime::RuntimeError> TransitionResource(
        pkg::ResourceId id,
        const pkg::ResourceState& before,
        const pkg::ResourceState& after,
        ID3D12GraphicsCommandList* list,
        D3D12_COMMAND_LIST_TYPE queueType)
    {
        return TransitionResourceAt(id, PhysicalInstanceIndex(id), before, after, list, queueType);
    }

    base::Expected<void, runtime::RuntimeError> TransitionResource(
        const pkg::ResourceViewArtifact& resourceView,
        const pkg::ResourceState& before,
        const pkg::ResourceState& after,
        ID3D12GraphicsCommandList* list,
        D3D12_COMMAND_LIST_TYPE queueType)
    {
        return TransitionResourceAt(resourceView.resource, PhysicalInstanceIndex(resourceView), before, after, list, queueType);
    }

    base::Expected<void, runtime::RuntimeError> CreateRootSignature(pkg::BindingLayoutId id)
    {
        if (!id.IsValid() || id.value >= view_.BindingLayouts().size()) return base::Failure<void, runtime::RuntimeError>(Error("load/CreateRootSignature", "Bindingが検証または実行の契約に違反しています。"));
        const auto blob = view_.ResolveBlob(view_.BindingLayouts()[id.value].serializedRootSignature);
        if (!blob) return PackageFailure("load/CreateRootSignature", blob.error());
        const HRESULT hr = device_->CreateRootSignature(0, blob.value().data(), blob.value().size(), IID_PPV_ARGS(&rootSignatures_[id.value]));
        if (FAILED(hr)) return base::Failure<void, runtime::RuntimeError>(HResultError("load/create-root-signature", hr, device_.Get()));
        return base::Success<void, runtime::RuntimeError>();
    }

    base::Expected<void, runtime::RuntimeError> CreateGraphicsPipeline(pkg::ExecutableId id)
    {
        if (!id.IsValid() || id.value >= view_.Executables().size())
            return base::Failure<void, runtime::RuntimeError>(Error("load/CreateGraphicsPipeline", "Tableが検証または実行の契約に違反しています。"));
        const auto& executable = view_.Executables()[id.value];
        const bool hasDepth = executable.depthFormat == pkg::Format::D32Float && executable.depthStateId == 1;
        if (executable.rasterStateId != 0 || executable.blendStateId != 0 ||
            (!hasDepth && (executable.depthFormat != pkg::Format::Unknown || executable.depthStateId != 0)) ||
            executable.sampleCount != 1 || executable.sampleQuality != 0 ||
            executable.primitiveTopology != pkg::PrimitiveTopology::TriangleList ||
            executable.primitiveTopologyType != pkg::PrimitiveTopologyType::Triangle ||
            executable.colorFormat != pkg::Format::B8G8R8A8Unorm || executable.colorFormatRange.count != 1)
            return base::Failure<void, runtime::RuntimeError>(Error("load/CreateGraphicsPipeline", "Tableが検証または実行の契約に違反しています。"));
        if (!executable.program.IsValid() || executable.program.value >= view_.Programs().size() ||
            !executable.bindingLayout.IsValid() || executable.bindingLayout.value >= rootSignatures_.size())
            return base::Failure<void, runtime::RuntimeError>(Error("load/CreateGraphicsPipeline", "Programが検証または実行の契約に違反しています。"));
        const auto& program = view_.Programs()[executable.program.value];
        if (program.kind != pkg::ProgramKind::Raster || !program.vertexShader.IsValid() || !program.pixelShader.IsValid() ||
            program.vertexShader.value >= view_.Shaders().size() || program.pixelShader.value >= view_.Shaders().size())
            return base::Failure<void, runtime::RuntimeError>(Error("load/CreateGraphicsPipeline", "Programが検証または実行の契約に違反しています。"));
        const auto vs = view_.ResolveBlob(view_.Shaders()[program.vertexShader.value].bytecode);
        const auto ps = view_.ResolveBlob(view_.Shaders()[program.pixelShader.value].bytecode);
        if (!vs) return PackageFailure("load/CreateGraphicsPipeline/VS", vs.error());
        if (!ps) return PackageFailure("load/CreateGraphicsPipeline/PS", ps.error());
        if (!rootSignatures_[executable.bindingLayout.value])
            return base::Failure<void, runtime::RuntimeError>(Error("load/CreateGraphicsPipeline", "検証または実行の契約に違反しています。"));

        std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout;
        for (std::uint32_t i = 0; i < executable.vertexElementRange.count; ++i)
        {
            const auto& element = view_.VertexElements()[executable.vertexElementRange.first + i];
            const char* semantic = SemanticName(element.meaning);
            if (!semantic || element.inputSlot != 0 || element.instanceStepRate != 0 || element.flags != 0)
                return base::Failure<void, runtime::RuntimeError>(Error("load/CreateGraphicsPipeline", "入力または内部状態が検証または実行の契約に違反しています。"));
            D3D12_INPUT_ELEMENT_DESC native{};
            native.SemanticName = semantic;
            native.SemanticIndex = element.semanticIndex;
            native.Format = ToDxgi(element.format);
            native.InputSlot = element.inputSlot;
            native.AlignedByteOffset = element.alignedByteOffset;
            native.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
            inputLayout.push_back(native);
        }

        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
        desc.pRootSignature = rootSignatures_[executable.bindingLayout.value].Get();
        desc.VS = {vs.value().data(), vs.value().size()};
        desc.PS = {ps.value().data(), ps.value().size()};
        auto& rt = desc.BlendState.RenderTarget[0];
        rt.SrcBlend = D3D12_BLEND_ONE; rt.DestBlend = D3D12_BLEND_ZERO; rt.BlendOp = D3D12_BLEND_OP_ADD;
        rt.SrcBlendAlpha = D3D12_BLEND_ONE; rt.DestBlendAlpha = D3D12_BLEND_ZERO; rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        rt.LogicOp = D3D12_LOGIC_OP_NOOP; rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        desc.SampleMask = UINT_MAX;
        desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        desc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
        desc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        desc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        desc.RasterizerState.DepthClipEnable = TRUE;
        desc.DepthStencilState.DepthEnable = hasDepth;
        desc.DepthStencilState.DepthWriteMask = hasDepth ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
        desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
        desc.DepthStencilState.StencilEnable = FALSE;
        desc.InputLayout = {inputLayout.data(), static_cast<UINT>(inputLayout.size())};
        desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        desc.NumRenderTargets = 1;
        desc.RTVFormats[0] = ToDxgi(executable.colorFormat);
        desc.DSVFormat = hasDepth ? ToDxgi(executable.depthFormat) : DXGI_FORMAT_UNKNOWN;
        desc.SampleDesc.Count = executable.sampleCount;
        desc.SampleDesc.Quality = executable.sampleQuality;
        const HRESULT hr = device_->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipelineStates_[id.value]));
        if (FAILED(hr)) return base::Failure<void, runtime::RuntimeError>(HResultError("load/create-graphics-pipeline", hr, device_.Get()));
        return base::Success<void, runtime::RuntimeError>();
    }

    base::Expected<void, runtime::RuntimeError> CreateComputePipeline(pkg::ComputeExecutableId id)
    {
        if (!id.IsValid() || id.value >= view_.ComputeExecutables().size())
            return base::Failure<void, runtime::RuntimeError>(
                Error("load/CreateComputePipeline", "Tableが検証または実行の契約に違反しています。"));
        const auto& executable = view_.ComputeExecutables()[id.value];
        if (executable.flags != 0 || !executable.program.IsValid() ||
            executable.program.value >= view_.Programs().size() ||
            !executable.bindingLayout.IsValid() ||
            executable.bindingLayout.value >= rootSignatures_.size())
            return base::Failure<void, runtime::RuntimeError>(
                Error("load/CreateComputePipeline", "Tableが検証または実行の契約に違反しています。"));
        const auto& program = view_.Programs()[executable.program.value];
        if (program.kind != pkg::ProgramKind::Compute ||
            !program.computeShader.IsValid() || program.computeShader.value >= view_.Shaders().size() ||
            program.bindingLayout != executable.bindingLayout)
            return base::Failure<void, runtime::RuntimeError>(
                Error("load/CreateComputePipeline", "Programが検証または実行の契約に違反しています。"));
        const auto cs = view_.ResolveBlob(view_.Shaders()[program.computeShader.value].bytecode);
        if (!cs) return PackageFailure("load/CreateComputePipeline/CS", cs.error());
        if (!rootSignatures_[executable.bindingLayout.value])
            return base::Failure<void, runtime::RuntimeError>(
                Error("load/CreateComputePipeline", "検証または実行の契約に違反しています。"));

        D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
        desc.pRootSignature = rootSignatures_[executable.bindingLayout.value].Get();
        desc.CS = {cs.value().data(), cs.value().size()};
        const HRESULT hr = device_->CreateComputePipelineState(
            &desc, IID_PPV_ARGS(&computePipelineStates_[id.value]));
        if (FAILED(hr))
            return base::Failure<void, runtime::RuntimeError>(
                HResultError("load/create-compute-pipeline", hr, device_.Get()));
        return base::Success<void, runtime::RuntimeError>();
    }

    base::Expected<void, runtime::RuntimeError> ExecuteCompute(pkg::ComputeCommandId id)
    {
        if (!id.IsValid() || id.value >= view_.ComputeCommands().size())
            return base::Failure<void, runtime::RuntimeError>(Error("frame/ExecuteCompute", "入力または内部状態が検証または実行の契約に違反しています。"));
        const auto& command = view_.ComputeCommands()[id.value];
        if (!command.executable.IsValid() || command.executable.value >= view_.ComputeExecutables().size() ||
            command.threadGroupCountX == 0 || command.threadGroupCountY == 0 || command.threadGroupCountZ == 0 || command.flags != 0)
            return base::Failure<void, runtime::RuntimeError>(Error("frame/ExecuteCompute", "Contractが検証または実行の契約に違反しています。"));
        const auto& executable = view_.ComputeExecutables()[command.executable.value];
        if (!computePipelineStates_[command.executable.value] || !rootSignatures_[executable.bindingLayout.value])
            return base::Failure<void, runtime::RuntimeError>(Error("frame/ExecuteCompute", "検証または実行の契約に違反しています。"));

        commandList_->SetPipelineState(computePipelineStates_[command.executable.value].Get());
        commandList_->SetComputeRootSignature(rootSignatures_[executable.bindingLayout.value].Get());
        const auto& layout = view_.BindingLayouts()[executable.bindingLayout.value];
        if (layout.descriptorRange.count != 0)
        {
            if (!shaderHeap_) return base::Failure<void, runtime::RuntimeError>(Error("frame/ExecuteCompute", "Shaderが検証または実行の契約に違反しています。"));
            ID3D12DescriptorHeap* heaps[] = {shaderHeap_.Get()};
            commandList_->SetDescriptorHeaps(1, heaps);
        }
        const pkg::ResourceState nonPixelShaderReadState{
            pkg::StateClass::Explicit,
            0,
            static_cast<std::uint32_t>(pkg::ExplicitStateBits::NonPixelShaderRead)};
        const pkg::ResourceState unorderedWriteState{pkg::StateClass::Explicit, 0, static_cast<std::uint32_t>(pkg::ExplicitStateBits::UnorderedWrite)};
        for (std::uint32_t index = 0; index < layout.parameterRange.count; ++index)
        {
            const auto& parameter = view_.RootParameters()[layout.parameterRange.first + index];
            if (parameter.kind == pkg::RootParameterKind::ConstantBuffer)
            {
                if (!parameter.dynamicSlot.IsValid() || parameter.dynamicSlot.value >= dynamicApplied_.size() || !dynamicApplied_[parameter.dynamicSlot.value])
                    return base::Failure<void, runtime::RuntimeError>(Error("frame/ExecuteCompute", "Bindingが検証または実行の契約に違反しています。"));
                const auto& slot = view_.DynamicSlots()[parameter.dynamicSlot.value];
                ID3D12Resource* resource = ResourceObject(slot.destinationResource);
                if (!resource) return base::Failure<void, runtime::RuntimeError>(Error("frame/ExecuteCompute", "Resourceが検証または実行の契約に違反しています。"));
                commandList_->SetComputeRootConstantBufferView(parameter.rootParameterIndex, resource->GetGPUVirtualAddress() + slot.destinationOffset);
            }
            else if (parameter.kind == pkg::RootParameterKind::ShaderResourceTable)
            {
                if (!parameter.staticView.IsValid() || parameter.staticView.value >= view_.Views().size())
                    return base::Failure<void, runtime::RuntimeError>(Error("frame/ExecuteCompute", "Resourceが検証または実行の契約に違反しています。"));
                const auto& resourceView = view_.Views()[parameter.staticView.value];
                if (resourceView.viewClass != pkg::ViewClass::ShaderResource || TrackedState(resourceView) != nonPixelShaderReadState || !ResourceObject(resourceView))
                    return base::Failure<void, runtime::RuntimeError>(Error("frame/ExecuteCompute", "Resourceが検証または実行の契約に違反しています。"));
                auto gpu = shaderHeap_->GetGPUDescriptorHandleForHeapStart();
                gpu.ptr += static_cast<UINT64>(DescriptorIndex(resourceView)) * shaderDescriptorIncrement_;
                commandList_->SetComputeRootDescriptorTable(parameter.rootParameterIndex, gpu);
            }
            else if (parameter.kind == pkg::RootParameterKind::UnorderedAccessTable)
            {
                if (!parameter.staticView.IsValid() || parameter.staticView.value >= view_.Views().size())
                    return base::Failure<void, runtime::RuntimeError>(Error("frame/ExecuteCompute", "ViewがCanonicalな順序または識別子規則に違反しています。"));
                const auto& resourceView = view_.Views()[parameter.staticView.value];
                if (resourceView.viewClass != pkg::ViewClass::UnorderedAccess || TrackedState(resourceView) != unorderedWriteState || !ResourceObject(resourceView))
                    return base::Failure<void, runtime::RuntimeError>(Error("frame/ExecuteCompute", "ResourceがCanonicalな順序または識別子規則に違反しています。"));
                auto gpu = shaderHeap_->GetGPUDescriptorHandleForHeapStart();
                gpu.ptr += static_cast<UINT64>(DescriptorIndex(resourceView)) * shaderDescriptorIncrement_;
                commandList_->SetComputeRootDescriptorTable(parameter.rootParameterIndex, gpu);
            }
            else return base::Failure<void, runtime::RuntimeError>(Error("frame/ExecuteCompute", "入力または内部状態が検証または実行の契約に違反しています。"));
        }
        if (indirectDispatchPresent_ && indirectDispatch_.computeCommand == id.value)
        {
            if (indirectDispatchApplied_ || !dispatchCommandSignature_ ||
                currentFrameSlot_ >= indirectArgumentBuffers_.size() ||
                !indirectArgumentBuffers_[currentFrameSlot_])
                return base::Failure<void, runtime::RuntimeError>(
                    Error("frame/ExecuteIndirect",
                        "Verified indirect dispatchの物理引数が実行契約と一致しません。"));
            commandList_->ExecuteIndirect(
                dispatchCommandSignature_.Get(), 1,
                indirectArgumentBuffers_[currentFrameSlot_].Get(), 0,
                nullptr, 0);
            indirectDispatchApplied_ = true;
        }
        else
        {
            commandList_->Dispatch(
                command.threadGroupCountX,
                command.threadGroupCountY,
                command.threadGroupCountZ);
        }
        return base::Success<void, runtime::RuntimeError>();
    }

    base::Expected<void, runtime::RuntimeError> ExecuteRaster(pkg::RasterCommandId id)
    {
        if (!id.IsValid() || id.value >= view_.RasterCommands().size())
            return base::Failure<void, runtime::RuntimeError>(Error("frame/ExecuteRaster", "入力または内部状態が検証または実行の契約に違反しています。"));
        const auto& command = view_.RasterCommands()[id.value];
        if (!command.executable.IsValid() || command.executable.value >= view_.Executables().size() ||
            !command.attachmentOperation.IsValid() || command.attachmentOperation.value >= view_.AttachmentOperations().size())
            return base::Failure<void, runtime::RuntimeError>(Error("frame/ExecuteRaster", "入力または内部状態の参照先または所有関係が無効です。"));
        const auto& executable = view_.Executables()[command.executable.value];
        const auto& attachment = view_.AttachmentOperations()[command.attachmentOperation.value];
        if (!pipelineStates_[command.executable.value] || !rootSignatures_[executable.bindingLayout.value])
            return base::Failure<void, runtime::RuntimeError>(Error("frame/ExecuteRaster", "検証または実行の契約に違反しています。"));
        if (command.vertexViewRange.count != 1 || command.colorAttachmentRange.count != 1 || command.indexView.IsValid() ||
            command.viewportId != 0 || command.scissorId != 0 || command.vertexCount == 0 || command.instanceCount == 0)
            return base::Failure<void, runtime::RuntimeError>(Error("frame/ExecuteRaster", "Contractが検証または実行の契約に違反しています。"));
        if (attachment.colorLoad != pkg::AttachmentLoadOp::Clear || attachment.colorStore != pkg::AttachmentStoreOp::Store)
            return base::Failure<void, runtime::RuntimeError>(Error("frame/ExecuteRaster", "Operationが検証または実行の契約に違反しています。"));

        const auto& vertexView = view_.Views()[command.vertexViewRange.first];
        const auto& colorView = view_.Views()[command.colorAttachmentRange.first];
        if (vertexView.viewClass != pkg::ViewClass::VertexBuffer || colorView.viewClass != pkg::ViewClass::RenderTarget)
            return base::Failure<void, runtime::RuntimeError>(Error("frame/ExecuteRaster", "Viewが検証または実行の契約に違反しています。"));
        const bool hasDepth = command.depthAttachment.IsValid();
        const pkg::ResourceViewArtifact* depthView = nullptr;
        if (hasDepth)
        {
            if (command.depthAttachment.value >= view_.Views().size())
                return base::Failure<void, runtime::RuntimeError>(Error("frame/ExecuteRaster", "Viewが検証または実行の契約に違反しています。"));
            depthView = &view_.Views()[command.depthAttachment.value];
            if (depthView->viewClass != pkg::ViewClass::DepthStencil ||
                attachment.depthLoad != pkg::AttachmentLoadOp::Clear || attachment.depthStore != pkg::AttachmentStoreOp::Store)
                return base::Failure<void, runtime::RuntimeError>(Error("frame/ExecuteRaster", "Contractが検証または実行の契約に違反しています。"));
        }
        else if (attachment.depthLoad != pkg::AttachmentLoadOp::Discard || attachment.depthStore != pkg::AttachmentStoreOp::Discard)
            return base::Failure<void, runtime::RuntimeError>(Error("frame/ExecuteRaster", "Operationが検証または実行の契約に違反しています。"));

        const pkg::ResourceState vertexState{pkg::StateClass::Explicit, 0, static_cast<std::uint32_t>(pkg::ExplicitStateBits::VertexBuffer)};
        const pkg::ResourceState renderTargetState{pkg::StateClass::Explicit, 0, static_cast<std::uint32_t>(pkg::ExplicitStateBits::RenderTarget)};
        const pkg::ResourceState depthState{pkg::StateClass::Explicit, 0, static_cast<std::uint32_t>(pkg::ExplicitStateBits::DepthWrite)};
        if (TrackedState(vertexView) != vertexState || TrackedState(colorView) != renderTargetState)
            return base::Failure<void, runtime::RuntimeError>(Error("frame/ExecuteRaster", "検証または実行の契約に違反しています。"));
        if (depthView && TrackedState(*depthView) != depthState)
            return base::Failure<void, runtime::RuntimeError>(Error("frame/ExecuteRaster", "検証または実行の契約に違反しています。"));
        ID3D12Resource* vertexResource = ResourceObject(vertexView);
        if (!vertexResource) return base::Failure<void, runtime::RuntimeError>(Error("frame/ExecuteRaster", "Resourceが検証または実行の契約に違反しています。"));

        D3D12_VERTEX_BUFFER_VIEW vbv{};
        vbv.BufferLocation = vertexResource->GetGPUVirtualAddress() + vertexView.byteOffset;
        vbv.SizeInBytes = static_cast<UINT>(vertexView.byteSize);
        vbv.StrideInBytes = vertexView.strideBytes;
        if (!ResourceObject(colorView))
            return base::Failure<void, runtime::RuntimeError>(Error("frame/ExecuteRaster", "Resourceが検証または実行の契約に違反しています。"));
        auto rtv = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
        rtv.ptr += static_cast<SIZE_T>(DescriptorIndex(colorView)) * rtvIncrement_;
        D3D12_CPU_DESCRIPTOR_HANDLE dsv{};
        if (depthView)
        {
            dsv = dsvHeap_->GetCPUDescriptorHandleForHeapStart();
            dsv.ptr += static_cast<SIZE_T>(DescriptorIndex(*depthView)) * dsvIncrement_;
            commandList_->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
        }
        else commandList_->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
        commandList_->ClearRenderTargetView(rtv, attachment.clearColor.data(), 0, nullptr);
        if (depthView) commandList_->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, attachment.clearDepth, static_cast<UINT8>(attachment.clearStencil), 0, nullptr);

        if (!colorView.resource.IsValid() || colorView.resource.value >= view_.Resources().size())
            return base::Failure<void, runtime::RuntimeError>(Error("frame/ExecuteRaster", "RenderTargetが検証または実行の契約に違反しています。"));
        const auto& colorResource = view_.Resources()[colorView.resource.value];
        std::uint32_t targetWidth = 0;
        std::uint32_t targetHeight = 0;
        if (colorResource.resourceKind == pkg::ResourceKind::SurfaceImage)
        {
            if (!surface_)
                return base::Failure<void, runtime::RuntimeError>(Error("frame/ExecuteRaster", "Surfaceが検証または実行の契約に違反しています。"));
            targetWidth = std::max(1u, surface_->ClientWidth());
            targetHeight = std::max(1u, surface_->ClientHeight());
        }
        else if (colorResource.resourceKind == pkg::ResourceKind::Texture2D &&
                 colorResource.extentMode == pkg::ExtentMode::Fixed &&
                 colorResource.width > 0 && colorResource.height > 0)
        {
            targetWidth = colorResource.width;
            targetHeight = colorResource.height;
        }
        else
        {
            return base::Failure<void, runtime::RuntimeError>(Error("frame/ExecuteRaster", "RenderTargetのextent契約が無効です。"));
        }
        D3D12_VIEWPORT viewport{};
        viewport.Width = static_cast<float>(targetWidth);
        viewport.Height = static_cast<float>(targetHeight);
        viewport.MinDepth = 0.0f; viewport.MaxDepth = 1.0f;
        D3D12_RECT scissor{0, 0, static_cast<LONG>(targetWidth), static_cast<LONG>(targetHeight)};
        commandList_->RSSetViewports(1, &viewport);
        commandList_->RSSetScissorRects(1, &scissor);
        commandList_->SetPipelineState(pipelineStates_[command.executable.value].Get());
        commandList_->SetGraphicsRootSignature(rootSignatures_[executable.bindingLayout.value].Get());
        const auto& layout = view_.BindingLayouts()[executable.bindingLayout.value];
        if (layout.descriptorRange.count != 0)
        {
            if (!shaderHeap_) return base::Failure<void, runtime::RuntimeError>(Error("frame/ExecuteRaster", "Shaderが検証または実行の契約に違反しています。"));
            ID3D12DescriptorHeap* heaps[] = {shaderHeap_.Get()};
            commandList_->SetDescriptorHeaps(1, heaps);
        }
        const pkg::ResourceState pixelShaderReadState{
            pkg::StateClass::Explicit,
            0,
            static_cast<std::uint32_t>(pkg::ExplicitStateBits::PixelShaderRead)};
        for (std::uint32_t i = 0; i < layout.parameterRange.count; ++i)
        {
            const auto& parameter = view_.RootParameters()[layout.parameterRange.first + i];
            if (parameter.kind == pkg::RootParameterKind::ConstantBuffer)
            {
                if (!parameter.dynamicSlot.IsValid() || parameter.dynamicSlot.value >= dynamicApplied_.size() || !dynamicApplied_[parameter.dynamicSlot.value])
                    return base::Failure<void, runtime::RuntimeError>(Error("frame/ExecuteRaster", "Bindingが検証または実行の契約に違反しています。"));
                const auto& slot = view_.DynamicSlots()[parameter.dynamicSlot.value];
                ID3D12Resource* resource = ResourceObject(slot.destinationResource);
                if (!resource) return base::Failure<void, runtime::RuntimeError>(Error("frame/ExecuteRaster", "Resourceが検証または実行の契約に違反しています。"));
                commandList_->SetGraphicsRootConstantBufferView(parameter.rootParameterIndex, resource->GetGPUVirtualAddress() + slot.destinationOffset);
            }
            else if (parameter.kind == pkg::RootParameterKind::ShaderResourceTable)
            {
                if (!parameter.staticView.IsValid() || parameter.staticView.value >= view_.Views().size())
                    return base::Failure<void, runtime::RuntimeError>(Error("frame/ExecuteRaster", "Resourceが検証または実行の契約に違反しています。"));
                const auto& resourceView = view_.Views()[parameter.staticView.value];
                if (resourceView.viewClass != pkg::ViewClass::ShaderResource || TrackedState(resourceView) != pixelShaderReadState || !ResourceObject(resourceView))
                    return base::Failure<void, runtime::RuntimeError>(Error("frame/ExecuteRaster", "Resourceが検証または実行の契約に違反しています。"));
                auto gpu = shaderHeap_->GetGPUDescriptorHandleForHeapStart();
                gpu.ptr += static_cast<UINT64>(DescriptorIndex(resourceView)) * shaderDescriptorIncrement_;
                commandList_->SetGraphicsRootDescriptorTable(parameter.rootParameterIndex, gpu);
            }
            else return base::Failure<void, runtime::RuntimeError>(Error("frame/ExecuteRaster", "入力または内部状態が検証または実行の契約に違反しています。"));
        }
        commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        commandList_->IASetVertexBuffers(0, 1, &vbv);
        commandList_->DrawInstanced(command.vertexCount, command.instanceCount, command.firstVertex, command.firstInstance);
        return base::Success<void, runtime::RuntimeError>();
    }

    ID3D12Resource* ResourceObject(pkg::ResourceId id)
    {
        return ResourceObjectAt(id, PhysicalInstanceIndex(id));
    }

    base::Expected<void, runtime::RuntimeError> PackageFailure(std::string stage, const package::PackageError& error)
    {
        return base::Failure<void, runtime::RuntimeError>(Error(std::move(stage), error.message));
    }

    struct PendingBufferVerification final
    {
        pkg::ResourceId resource;
        std::uint32_t instanceIndex = 0;
        std::uint64_t expectedDataOffset = 0;
        std::uint64_t bytes = 0;
        ComPtr<ID3D12Resource> readback;
    };

    struct PendingTextureVerification final
    {
        pkg::ResourceId resource;
        std::uint32_t instanceIndex = 0;
        std::uint64_t expectedDataOffset = 0;
        std::uint32_t expectedRowBytes = 0;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
        ComPtr<ID3D12Resource> readback;
    };

    std::shared_ptr<const package::FrozenExecutablePackage> package_;
    pkg::D3D12PackageView view_;
    runtime::ISurfaceHost* surface_ = nullptr;
    ExecutorOptions options_;
    DeviceDomain* domain_ = nullptr;
    std::shared_ptr<detail::TimestampProfileCollector> profileCollector_;
    std::shared_ptr<detail::TimestampProfileRecord> profileRecord_;
    std::uint64_t deviceEpoch_ = 1;
    runtime::DeviceRuntimeState runtimeState_ = runtime::DeviceRuntimeState::Active;
    LUID activeAdapterLuid_{};
    LUID excludedAdapterLuid_{};
    bool hasActiveAdapterLuid_ = false;
    bool hasExcludedAdapterLuid_ = false;

    ComPtr<IDXGIFactory6> factory_;
    ComPtr<ID3D12Device> device_;
    std::vector<QueueRuntimeState> queues_;
    ComPtr<IDXGISwapChain3> swapChain_;
    ComPtr<ID3D12DescriptorHeap> rtvHeap_;
    ComPtr<ID3D12DescriptorHeap> dsvHeap_;
    ComPtr<ID3D12DescriptorHeap> shaderHeap_;
    ComPtr<ID3D12QueryHeap> timestampQueryHeap_;
    ComPtr<ID3D12Resource> timestampReadback_;
    ComPtr<ID3D12CommandSignature> dispatchCommandSignature_;
    std::vector<ComPtr<ID3D12Resource>> indirectArgumentBuffers_;
    runtime::VerifiedIndirectDispatchBinding indirectDispatch_{};
    bool indirectDispatchPresent_ = false;
    bool indirectDispatchApplied_ = false;
    std::uint64_t* mappedTimestampValues_ = nullptr;
    pkg::QueueId timestampQueue_{};
    bool timestampQueryIssued_ = false;
    bool timestampQueryResolved_ = false;
    std::vector<ComPtr<ID3D12Resource>> backBuffers_;
    std::vector<std::vector<ComPtr<ID3D12Resource>>> resources_;
    std::vector<std::shared_ptr<runtime::IExternalResource>> externalNativeResources_;
    std::vector<runtime::ExternalResourceBinding> externalBindings_;
    std::vector<bool> externalAcquired_;
    std::vector<bool> externalWaited_;
    std::vector<bool> externalReleased_;
    std::vector<bool> surfaceAcquired_;
    std::vector<bool> surfacePresented_;
    std::vector<bool> temporalWaitedResources_;
    std::vector<runtime::ExternalRelease> frameExternalReleases_;
    std::map<std::uint32_t, SignalPointRuntime> currentFrameSignalPoints_;
    std::map<std::uint32_t, SignalPointRuntime> previousFrameSignalPoints_;
    std::vector<std::vector<ComPtr<ID3D12Heap>>> placedHeaps_;
    std::vector<std::uint32_t> activeAliasResources_;
    std::vector<ComPtr<ID3D12Resource>> uploadResources_;
    std::vector<PendingBufferVerification> pendingBufferVerifications_;
    std::vector<PendingTextureVerification> pendingTextureVerifications_;
    std::vector<ComPtr<ID3D12RootSignature>> rootSignatures_;
    std::vector<ComPtr<ID3D12PipelineState>> pipelineStates_;
    std::vector<ComPtr<ID3D12PipelineState>> computePipelineStates_;
    std::vector<std::vector<pkg::ResourceState>> resourceStates_;
    std::vector<std::span<const std::byte>> dynamicBindings_;
    std::vector<bool> dynamicApplied_;
    ComPtr<ID3D12CommandAllocator> allocator_;
    ComPtr<ID3D12GraphicsCommandList> commandList_;
    ComPtr<ID3D12CommandAllocator> loadAllocator_;
    ComPtr<ID3D12GraphicsCommandList> loadCommandList_;
    UINT rtvIncrement_ = 0;
    UINT dsvIncrement_ = 0;
    UINT shaderDescriptorIncrement_ = 0;
    UINT currentBackBuffer_ = 0;
    std::uint32_t currentFrameSlot_ = 0;
    std::uint32_t temporalPreviousInstance_ = 1;
    std::uint32_t temporalCurrentInstance_ = 0;
    std::uint64_t temporalDependencyFenceValue_ = 0;
    std::uint64_t lastSubmittedFrameNumber_ = 0;
    bool hasSubmittedFrame_ = false;
    bool descriptorHeapsCreated_ = false;
    bool commandOpen_ = false;
    std::uint32_t activeCommandQueue_ = package::InvalidIndex;
    bool loadBatchOpen_ = false;
    bool loadBatchClosed_ = false;
    bool loadQueueCompleted_ = false;
    bool hasLoadQueueBatch_ = false;
};