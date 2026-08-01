base::Expected<void, PackageError> ValidateOperations(const D3D12PackageView& view)
{
    const auto fail = [](const char* message)
    {
        return base::Failure<void, PackageError>(
            Error(PackageErrorCode::InvalidOperationStream, message, SectionKind::OperationTable));
    };
    const auto queueCount = static_cast<std::uint64_t>(view.Profile().directQueueCount) +
                            view.Profile().computeQueueCount + view.Profile().copyQueueCount;
    const auto validQueue = [queueCount](QueueId queue) {
        return queue.IsValid() && queue.value < queueCount;
    };
    const auto isCopyQueue = [&](QueueId queue) {
        return validQueue(queue) &&
               queue.value >= view.Profile().directQueueCount + view.Profile().computeQueueCount;
    };
    const auto inInitialData = [&](std::uint64_t offset, std::uint64_t bytes) {
        return offset <= view.InitialData().size() && bytes <= view.InitialData().size() - offset;
    };
    const auto resourceValid = [&](ResourceId id) { return id.IsValid() && id.value < view.Resources().size(); };
    const auto viewValid = [&](ViewId id) { return id.IsValid() && id.value < view.Views().size(); };

    std::vector<bool> resourceCreated(view.Resources().size(), false);
    std::vector<bool> layoutCreated(view.BindingLayouts().size(), false);
    std::vector<bool> rasterCreated(view.Executables().size(), false);
    std::vector<bool> computeCreated(view.ComputeExecutables().size(), false);
    std::uint32_t descriptorHeapCreationCount = 0;
    const QueueId canonicalLoadQueue{view.Profile().copyQueueCount != 0
        ? view.Profile().directQueueCount + view.Profile().computeQueueCount
        : 0u};

    const auto validateStream = [&](std::span<const OperationView> operations, bool load)
        -> base::Expected<void, PackageError>
    {
        std::vector<bool> batchOpen(static_cast<std::size_t>(queueCount), false);
        std::map<std::uint32_t, std::pair<QueueId, std::size_t>> signalPoints;
        std::vector<std::uint8_t> externalPhase(view.ExternalSlots().size(), 0);
        std::vector<bool> dynamicApplied(view.DynamicSlots().size(), false);
        std::vector<std::uint8_t> surfacePhase(view.SurfaceSlots().size(), 0);
        std::map<std::uint64_t, ResourceState> lastTransitionState;
        bool loadBatchSeen = false;
        std::uint32_t nextSignalPoint = 0;
        for (std::size_t index = 0; index < operations.size(); ++index)
        {
            const auto& operation = operations[index];
            if (operation.opcode != D3D12OperationCode::SignalQueue) continue;
            auto payload = DecodeSignalQueue(operation.payload);
            if (!payload || !payload.value().signalPoint.IsValid() ||
                payload.value().signalPoint.value != nextSignalPoint++ ||
                !validQueue(operation.queue) ||
                !signalPoints.emplace(payload.value().signalPoint.value,
                    std::pair{operation.queue, index}).second)
                return fail("QueueがCanonicalな順序または識別子規則に違反しています。");
        }

        for (std::size_t operationIndex = 0; operationIndex < operations.size(); ++operationIndex)
        {
            const auto& operation = operations[operationIndex];
            const auto expectedVersion = OperationVersion(operation.opcode);
            if (!IsKnownOperation(operation.opcode) || operation.operationVersion != expectedVersion || operation.flags != 0)
                return fail("Operationが無効であるか、契約条件を満たしていません。");

            const bool metadataOperation =
                operation.opcode == D3D12OperationCode::CreateDescriptorHeaps ||
                operation.opcode == D3D12OperationCode::CreateResource ||
                operation.opcode == D3D12OperationCode::CreateRootSignature ||
                operation.opcode == D3D12OperationCode::CreateGraphicsPipeline ||
                operation.opcode == D3D12OperationCode::CreateComputePipeline ||
                operation.opcode == D3D12OperationCode::AcquireSurfaceImage ||
                operation.opcode == D3D12OperationCode::AcquireExternal ||
                operation.opcode == D3D12OperationCode::ApplyDynamicData ||
                operation.opcode == D3D12OperationCode::ReleaseExternal ||
                operation.opcode == D3D12OperationCode::PresentSurface;
            if (metadataOperation ? operation.queue.IsValid() : !validQueue(operation.queue))
                return fail("Queueの値または参照範囲が許容範囲外です。");
            if (load && !metadataOperation && operation.queue != canonicalLoadQueue)
                return fail("PackageがCanonicalな順序または識別子規則に違反しています。");

            switch (operation.opcode)
            {
            case D3D12OperationCode::CreateDescriptorHeaps:
                if (!load || !operation.payload.empty() || operationIndex != 0 ||
                    ++descriptorHeapCreationCount != 1)
                    return fail("Operationが無効であるか、契約条件を満たしていません。");
                break;
            case D3D12OperationCode::CreateResource:
            {
                if (!load) return fail("Resourceが検証または実行の契約に違反しています。");
                auto payload = DecodeCreateResource(operation.payload);
                if (!payload || !resourceValid(payload.value().resource) ||
                    view.Resources()[payload.value().resource.value].origin != ResourceOrigin::PackageOwned ||
                    resourceCreated[payload.value().resource.value])
                    return fail("Resourceに重複または二重処理があります。");
                resourceCreated[payload.value().resource.value] = true;
                break;
            }
            case D3D12OperationCode::UploadBuffer:
            {
                auto payload = DecodeUploadBuffer(operation.payload);
                if (!load || !payload || !resourceValid(payload.value().resource) ||
                    !resourceCreated[payload.value().resource.value] || payload.value().bytes == 0 ||
                    !inInitialData(payload.value().sourceOffset, payload.value().bytes))
                    return fail("Bufferが無効であるか、契約条件を満たしていません。");
                break;
            }
            case D3D12OperationCode::UploadTexture:
            {
                auto payload = DecodeUploadTexture(operation.payload);
                if (!load || !payload || !resourceValid(payload.value().resource) ||
                    !resourceCreated[payload.value().resource.value] ||
                    payload.value().sourceSliceBytes == 0 ||
                    !inInitialData(payload.value().sourceOffset, payload.value().sourceSliceBytes))
                    return fail("Textureが無効であるか、契約条件を満たしていません。");
                break;
            }
            case D3D12OperationCode::InitializeState:
            {
                auto payload = DecodeInitializeState(operation.payload);
                if (!load || !payload || !resourceValid(payload.value().resource) ||
                    !resourceCreated[payload.value().resource.value] ||
                    (isCopyQueue(operation.queue) &&
                     (!IsCopyQueueState(payload.value().before) || !IsCopyQueueState(payload.value().after))))
                    return fail("Payloadが無効であるか、契約条件を満たしていません。");
                break;
            }
            case D3D12OperationCode::VerifyBufferContents:
            {
                auto payload = DecodeVerifyBufferContents(operation.payload);
                if (!load || !payload || !resourceValid(payload.value().resource) ||
                    !resourceCreated[payload.value().resource.value] || payload.value().bytes == 0 ||
                    !inInitialData(payload.value().expectedDataOffset, payload.value().bytes))
                    return fail("Bufferが無効であるか、契約条件を満たしていません。");
                break;
            }
            case D3D12OperationCode::VerifyTextureContents:
            {
                auto payload = DecodeVerifyTextureContents(operation.payload);
                const auto bytes = payload ? static_cast<std::uint64_t>(payload.value().expectedRowBytes) * payload.value().height : 0;
                if (!load || !payload || !resourceValid(payload.value().resource) ||
                    !resourceCreated[payload.value().resource.value] || bytes == 0 ||
                    !inInitialData(payload.value().expectedDataOffset, bytes))
                    return fail("Textureが無効であるか、契約条件を満たしていません。");
                break;
            }
            case D3D12OperationCode::CreateRootSignature:
            {
                auto payload = DecodeCreateRootSignature(operation.payload);
                if (!load || !payload || !payload.value().layout.IsValid() ||
                    payload.value().layout.value >= view.BindingLayouts().size() ||
                    layoutCreated[payload.value().layout.value])
                    return fail("Payloadに重複または二重処理があります。");
                layoutCreated[payload.value().layout.value] = true;
                break;
            }
            case D3D12OperationCode::CreateGraphicsPipeline:
            {
                auto payload = DecodeCreateGraphicsPipeline(operation.payload);
                if (!load || !payload || !payload.value().executable.IsValid() ||
                    payload.value().executable.value >= view.Executables().size() ||
                    rasterCreated[payload.value().executable.value] ||
                    !layoutCreated[view.Executables()[payload.value().executable.value].bindingLayout.value])
                    return fail("Payloadに重複または二重処理があります。");
                rasterCreated[payload.value().executable.value] = true;
                break;
            }
            case D3D12OperationCode::CreateComputePipeline:
            {
                auto payload = DecodeCreateComputePipeline(operation.payload);
                if (!load || !payload || !payload.value().executable.IsValid() ||
                    payload.value().executable.value >= view.ComputeExecutables().size() ||
                    computeCreated[payload.value().executable.value] ||
                    !layoutCreated[view.ComputeExecutables()[payload.value().executable.value].bindingLayout.value])
                    return fail("Payloadに重複または二重処理があります。");
                computeCreated[payload.value().executable.value] = true;
                break;
            }
            case D3D12OperationCode::AcquireSurfaceImage:
            {
                auto payload = DecodeAcquireSurfaceImage(operation.payload);
                if (load || !payload || !payload.value().slot.IsValid() ||
                    payload.value().slot.value >= view.SurfaceSlots().size() ||
                    surfacePhase[payload.value().slot.value] != 0)
                    return fail("Payloadに重複または二重処理があります。");
                surfacePhase[payload.value().slot.value] = 1;
                break;
            }
            case D3D12OperationCode::AcquireExternal:
            {
                auto payload = DecodeAcquireExternal(operation.payload);
                if (load || !payload || !payload.value().slot.IsValid() ||
                    payload.value().slot.value >= view.ExternalSlots().size() ||
                    externalPhase[payload.value().slot.value] != 0)
                    return fail("Payloadに重複または二重処理があります。");
                externalPhase[payload.value().slot.value] = 1;
                break;
            }
            case D3D12OperationCode::WaitExternal:
            {
                auto payload = DecodeWaitExternal(operation.payload);
                if (load || !payload || !payload.value().slot.IsValid() ||
                    payload.value().slot.value >= view.ExternalSlots().size() ||
                    externalPhase[payload.value().slot.value] != 1)
                    return fail("Waitが検証または実行の契約に違反しています。");
                externalPhase[payload.value().slot.value] = 2;
                break;
            }
            case D3D12OperationCode::ApplyDynamicData:
            {
                auto payload = DecodeApplyDynamicData(operation.payload);
                if (load || !payload || !payload.value().slot.IsValid() ||
                    payload.value().slot.value >= view.DynamicSlots().size() ||
                    dynamicApplied[payload.value().slot.value])
                    return fail("Payloadに重複または二重処理があります。");
                dynamicApplied[payload.value().slot.value] = true;
                break;
            }
            case D3D12OperationCode::BeginQueueBatch:
                if (!operation.payload.empty() || batchOpen[operation.queue.value] ||
                    (load && loadBatchSeen))
                    return fail("QueueがCanonicalな順序または識別子規則に違反しています。");
                batchOpen[operation.queue.value] = true;
                if (load) loadBatchSeen = true;
                break;
            case D3D12OperationCode::EndQueueBatch:
                if (!operation.payload.empty() || !batchOpen[operation.queue.value])
                    return fail("Queueが無効であるか、契約条件を満たしていません。");
                batchOpen[operation.queue.value] = false;
                break;
            case D3D12OperationCode::Transition:
            {
                auto payload = DecodeTransition(operation.payload);
                if (load || !payload || !viewValid(payload.value().view) || payload.value().flags != 0 ||
                    !batchOpen[operation.queue.value] ||
                    (isCopyQueue(operation.queue) &&
                     (!IsCopyQueueState(payload.value().before) || !IsCopyQueueState(payload.value().after))))
                    return fail("Payloadが無効であるか、契約条件を満たしていません。");
                const auto& transitionView = view.Views()[payload.value().view.value];
                const auto relation =
                    (transitionView.flags & static_cast<std::uint32_t>(ResourceViewFlags::TemporalPrevious)) != 0 ? 2ull :
                    (transitionView.flags & static_cast<std::uint32_t>(ResourceViewFlags::TemporalCurrent)) != 0 ? 1ull : 0ull;
                const auto stateKey = static_cast<std::uint64_t>(transitionView.resource.value) * 3ull + relation;
                const auto prior = lastTransitionState.find(stateKey);
                if (prior != lastTransitionState.end() && prior->second != payload.value().before)
                    return fail("Viewの状態または世代が実行契約と一致しません。");
                lastTransitionState[stateKey] = payload.value().after;
                break;
            }
            case D3D12OperationCode::ActivateAlias:
            {
                auto payload = DecodeActivateAlias(operation.payload);
                if (!load || !payload || !batchOpen[operation.queue.value] ||
                    !resourceValid(payload.value().after) ||
                    (payload.value().before.IsValid() && !resourceValid(payload.value().before)))
                    return fail("Payloadが無効であるか、契約条件を満たしていません。");
                const auto& afterResource = view.Resources()[payload.value().after.value];
                if (afterResource.origin != ResourceOrigin::PackageOwned ||
                    !afterResource.allocation.IsValid() ||
                    afterResource.allocation.value >= view.Allocations().size() ||
                    view.Allocations()[afterResource.allocation.value].kind != AllocationKind::Placed)
                    return fail("Allocationが検証または実行の契約に違反しています。");
                if (payload.value().before.IsValid())
                {
                    const auto& beforeResource = view.Resources()[payload.value().before.value];
                    if (beforeResource.allocation != afterResource.allocation ||
                        payload.value().before == payload.value().after)
                        return fail("Resourceが検証または実行の契約に違反しています。");
                }
                break;
            }
            case D3D12OperationCode::ExecuteRaster:
            {
                auto payload = DecodeExecuteRaster(operation.payload);
                if (load || !payload || !batchOpen[operation.queue.value] ||
                    operation.queue.value >= view.Profile().directQueueCount ||
                    !payload.value().command.IsValid() || payload.value().command.value >= view.RasterCommands().size())
                    return fail("Queueに必要な情報または実行状態がありません。");
                break;
            }
            case D3D12OperationCode::ExecuteCompute:
            {
                auto payload = DecodeExecuteCompute(operation.payload);
                if (load || !payload || !batchOpen[operation.queue.value] ||
                    isCopyQueue(operation.queue) ||
                    !payload.value().command.IsValid() || payload.value().command.value >= view.ComputeCommands().size())
                    return fail("Queueに必要な情報または実行状態がありません。");
                break;
            }
            case D3D12OperationCode::ExecuteCopy:
            {
                auto payload = DecodeCopyBuffer(operation.payload);
                if (load || !payload || !batchOpen[operation.queue.value] ||
                    !viewValid(payload.value().sourceView) || !viewValid(payload.value().destinationView) ||
                    payload.value().bytes == 0)
                    return fail("Payloadが無効であるか、契約条件を満たしていません。");
                const auto& sourceView = view.Views()[payload.value().sourceView.value];
                const auto& destinationView = view.Views()[payload.value().destinationView.value];
                if (sourceView.viewClass != ViewClass::CopySource ||
                    destinationView.viewClass != ViewClass::CopyDestination ||
                    !resourceValid(sourceView.resource) || !resourceValid(destinationView.resource) ||
                    view.Resources()[sourceView.resource.value].resourceKind != ResourceKind::Buffer ||
                    view.Resources()[destinationView.resource.value].resourceKind != ResourceKind::Buffer ||
                    payload.value().sourceOffset > sourceView.byteSize ||
                    payload.value().bytes > sourceView.byteSize - payload.value().sourceOffset ||
                    payload.value().destinationOffset > destinationView.byteSize ||
                    payload.value().bytes > destinationView.byteSize - payload.value().destinationOffset)
                    return fail("Viewが無効であるか、契約条件を満たしていません。");
                break;
            }
            case D3D12OperationCode::SignalQueue:
            {
                auto payload = DecodeSignalQueue(operation.payload);
                const auto signal = payload ? signalPoints.find(payload.value().signalPoint.value) : signalPoints.end();
                if (!payload || signal == signalPoints.end() || signal->second.first != operation.queue ||
                    signal->second.second != operationIndex || batchOpen[operation.queue.value])
                    return fail("QueueがCanonicalな順序または識別子規則に違反しています。");
                break;
            }
            case D3D12OperationCode::WaitQueue:
            {
                auto payload = DecodeWaitQueue(operation.payload);
                const auto signal = payload ? signalPoints.find(payload.value().signalPoint.value) : signalPoints.end();
                if (!payload || signal == signalPoints.end() || signal->second.second >= operationIndex ||
                    signal->second.first == operation.queue || batchOpen[operation.queue.value])
                    return fail("Queueの参照先または所有関係が無効です。");
                break;
            }
            case D3D12OperationCode::WaitTemporal:
            {
                auto payload = DecodeWaitTemporal(operation.payload);
                const auto signal = payload ? signalPoints.find(payload.value().producerSignalPoint.value) : signalPoints.end();
                if (load || !payload || !resourceValid(payload.value().resource) || signal == signalPoints.end() ||
                    batchOpen[operation.queue.value] ||
                    (view.Resources()[payload.value().resource.value].flags &
                     static_cast<std::uint32_t>(ResourceFlags::Temporal)) == 0)
                    return fail("Resourceの参照先または所有関係が無効です。");
                break;
            }
            case D3D12OperationCode::ReleaseExternal:
            {
                auto payload = DecodeReleaseExternal(operation.payload);
                const auto signal = payload ? signalPoints.find(payload.value().releaseSignalPoint.value) : signalPoints.end();
                if (load || !payload || !payload.value().slot.IsValid() ||
                    payload.value().slot.value >= view.ExternalSlots().size() ||
                    externalPhase[payload.value().slot.value] != 2 ||
                    signal == signalPoints.end() || signal->second.second >= operationIndex)
                    return fail("Signalの参照先または所有関係が無効です。");
                externalPhase[payload.value().slot.value] = 3;
                break;
            }
            case D3D12OperationCode::PresentSurface:
            {
                auto payload = DecodePresentSurface(operation.payload);
                if (load || !payload || !payload.value().slot.IsValid() ||
                    payload.value().slot.value >= view.SurfaceSlots().size() ||
                    surfacePhase[payload.value().slot.value] != 1)
                    return fail("Surfaceが検証または実行の契約に違反しています。");
                surfacePhase[payload.value().slot.value] = 2;
                break;
            }
            default:
                return fail("Operationに未対応または禁止された値が含まれています。");
            }
        }
        if (std::any_of(batchOpen.begin(), batchOpen.end(), [](bool open) { return open; }))
            return fail("Queueが検証または実行の契約に違反しています。");
        if (!load)
        {
            if (std::any_of(externalPhase.begin(), externalPhase.end(),
                    [](std::uint8_t phase) { return phase != 3; }))
                return fail("Waitに必要な情報または実行状態がありません。");
            if (std::any_of(dynamicApplied.begin(), dynamicApplied.end(),
                    [](bool applied) { return !applied; }))
                return fail("Operationに必要な情報または実行状態がありません。");
            if (std::any_of(surfacePhase.begin(), surfacePhase.end(),
                    [](std::uint8_t phase) { return phase != 2; }))
                return fail("Surfaceに必要な情報または実行状態がありません。");
        }
        return base::Success<void, PackageError>();
    };

    auto loadResult = validateStream(view.LoadOperations(), true);
    if (!loadResult) return loadResult;
    if (descriptorHeapCreationCount != 1)
        return fail("Operationが検証または実行の契約に違反しています。");
    for (std::size_t index = 0; index < view.Resources().size(); ++index)
        if (view.Resources()[index].origin == ResourceOrigin::PackageOwned && !resourceCreated[index])
            return fail("Packageに必要な情報または実行状態がありません。");
    if (std::any_of(layoutCreated.begin(), layoutCreated.end(), [](bool value) { return !value; }) ||
        std::any_of(rasterCreated.begin(), rasterCreated.end(), [](bool value) { return !value; }) ||
        std::any_of(computeCreated.begin(), computeCreated.end(), [](bool value) { return !value; }))
        return fail("Bindingに必要な情報または実行状態がありません。");
    return validateStream(view.FrameOperations(), false);
}

base::Expected<void, PackageError> ValidateReferences(D3D12PackageView& view)
{
    const auto fail = [](PackageErrorCode code, const char* message,
                         SectionKind section = SectionKind::Manifest)
    {
        return base::Failure<void, PackageError>(Error(code, message, section));
    };
    const auto& manifest = view.Manifest();
    if (manifest.flags != 0 ||
        manifest.resourceCount != view.Resources().size() ||
        manifest.allocationCount != view.Allocations().size() ||
        manifest.viewCount != view.Views().size() || manifest.shaderCount != view.Shaders().size() ||
        manifest.programCount != view.Programs().size() ||
        manifest.bindingLayoutCount != view.BindingLayouts().size() ||
        manifest.executableCount != view.Executables().size() ||
        manifest.rasterCommandCount != view.RasterCommands().size() ||
        manifest.computeExecutableCount != view.ComputeExecutables().size() ||
        manifest.computeCommandCount != view.ComputeCommands().size() ||
        manifest.vertexElementCount != view.VertexElements().size() ||
        manifest.attachmentOperationCount != view.AttachmentOperations().size() ||
        manifest.dynamicSlotCount != view.DynamicSlots().size() ||
        manifest.externalSlotCount != view.ExternalSlots().size() ||
        manifest.surfaceSlotCount != view.SurfaceSlots().size())
        return fail(PackageErrorCode::InvalidReference, "ManifestがCanonicalな契約と一致しません。");

    if (!Dense(view.Resources()) || !Dense(view.Allocations()) || !Dense(view.Views()) ||
        !Dense(view.Shaders()) || !Dense(view.Programs()) || !Dense(view.BindingLayouts()) ||
        !Dense(view.RootParameters()) || !Dense(view.Executables()) ||
        !Dense(view.ComputeExecutables()) || !Dense(view.ComputeCommands()) ||
        !Dense(view.AttachmentOperations()) || !Dense(view.RasterCommands()) ||
        !Dense(view.DynamicSlots()) || !Dense(view.ExternalSlots()) || !Dense(view.SurfaceSlots()))
        return fail(PackageErrorCode::InvalidIdSequence, "PackageがCanonicalな順序または識別子規則に違反しています。");
    for (std::size_t index = 0; index < view.VertexElements().size(); ++index)
        if (view.VertexElements()[index].id != index)
            return fail(PackageErrorCode::InvalidIdSequence, "入力または内部状態がCanonicalな順序または識別子規則に違反しています。",
                        SectionKind::D3D12VertexElementTable);

    const auto rangeValid = [](IndexRange range, std::size_t count) {
        return range.first <= count && range.count <= count - range.first;
    };
    const auto blobValid = [&](const BlobRef& blob, const base::Digest256& digest) {
        auto bytes = view.ResolveBlob(blob);
        return bytes && base::Sha256(bytes.value()) == digest;
    };
    constexpr std::uint32_t KnownResourceFlags =
        static_cast<std::uint32_t>(ResourceFlags::FrameLocal) |
        static_cast<std::uint32_t>(ResourceFlags::Temporal) |
        static_cast<std::uint32_t>(ResourceFlags::Aliased);
    constexpr std::uint32_t KnownViewFlags =
        static_cast<std::uint32_t>(ResourceViewFlags::TemporalCurrent) |
        static_cast<std::uint32_t>(ResourceViewFlags::TemporalPrevious);
    const auto hasResourceFlag = [](const ResourceArtifact& resource, ResourceFlags flag) {
        return (resource.flags & static_cast<std::uint32_t>(flag)) != 0;
    };
    const auto hasViewFlag = [](const ResourceViewArtifact& resourceView, ResourceViewFlags flag) {
        return (resourceView.flags & static_cast<std::uint32_t>(flag)) != 0;
    };
    const auto descriptorBacked = [](ViewClass viewClass) {
        return viewClass == ViewClass::ShaderResource || viewClass == ViewClass::UnorderedAccess ||
               viewClass == ViewClass::RenderTarget || viewClass == ViewClass::DepthStencil;
    };

    std::vector<std::vector<std::uint32_t>> allocationUsers(view.Allocations().size());
    std::uint32_t expectedFirstView = 0;
    for (std::size_t index = 0; index < view.Resources().size(); ++index)
    {
        const auto& resource = view.Resources()[index];
        if (resource.firstView != expectedFirstView ||
            resource.firstView > view.Views().size() ||
            resource.viewCount > view.Views().size() - resource.firstView ||
            resource.initialDataOffset > view.InitialData().size() ||
            resource.initialDataSize > view.InitialData().size() - resource.initialDataOffset)
            return fail(PackageErrorCode::InvalidReference, "ResourceがCanonicalな順序または識別子規則に違反しています。",
                        SectionKind::D3D12ResourceTable);
        expectedFirstView += resource.viewCount;
        for (std::uint32_t viewIndex = resource.firstView;
             viewIndex < resource.firstView + resource.viewCount; ++viewIndex)
            if (view.Views()[viewIndex].resource.value != index)
                return fail(PackageErrorCode::InvalidReference, "Resourceが検証または実行の契約に違反しています。",
                            SectionKind::D3D12ResourceTable);

        if ((resource.flags & ~KnownResourceFlags) != 0 || resource.usageFlags != 0 ||
            (hasResourceFlag(resource, ResourceFlags::FrameLocal) &&
             hasResourceFlag(resource, ResourceFlags::Temporal)))
            return fail(PackageErrorCode::InvalidReference, "Resourceに未対応または禁止された値が含まれています。",
                        SectionKind::D3D12ResourceTable);
        if (resource.resourceKind != ResourceKind::Buffer &&
            resource.resourceKind != ResourceKind::Texture2D &&
            resource.resourceKind != ResourceKind::SurfaceImage)
            return fail(PackageErrorCode::InvalidEnumValue, "Resourceの値または参照範囲が許容範囲外です。",
                        SectionKind::D3D12ResourceTable);
        if (resource.resourceKind == ResourceKind::Buffer)
        {
            if (resource.extentMode != ExtentMode::Fixed || resource.format != Format::Unknown ||
                resource.sizeBytes == 0 || resource.width != 0 || resource.height != 0 ||
                resource.depthOrArraySize != 0 || resource.mipLevels != 0 ||
                resource.sampleCount != 1 || resource.planeCount != 1 ||
                resource.initialDataSize > resource.sizeBytes)
                return fail(PackageErrorCode::InvalidReference, "Resourceが無効であるか、契約条件を満たしていません。",
                            SectionKind::D3D12ResourceTable);
        }
        else if (resource.resourceKind == ResourceKind::Texture2D)
        {
            if (resource.sizeBytes != 0 || resource.depthOrArraySize != 1 || resource.mipLevels != 1 ||
                resource.sampleCount != 1 || resource.planeCount != 1 ||
                (resource.extentMode == ExtentMode::Fixed &&
                 (resource.width == 0 || resource.height == 0 || resource.format != Format::B8G8R8A8Unorm)) ||
                (resource.extentMode == ExtentMode::SurfaceRelative &&
                 (resource.width != 0 || resource.height != 0 || resource.format != Format::D32Float)) ||
                (resource.extentMode != ExtentMode::Fixed && resource.extentMode != ExtentMode::SurfaceRelative))
                return fail(PackageErrorCode::InvalidReference, "Resourceが無効であるか、契約条件を満たしていません。",
                            SectionKind::D3D12ResourceTable);
        }
        else
        {
            if (resource.origin != ResourceOrigin::Surface || resource.extentMode != ExtentMode::SurfaceRelative ||
                resource.format != Format::B8G8R8A8Unorm || resource.sizeBytes != 0 ||
                resource.width != 0 || resource.height != 0 || resource.depthOrArraySize != 0 ||
                resource.mipLevels != 0 || resource.sampleCount != 1 || resource.planeCount != 1)
                return fail(PackageErrorCode::InvalidReference, "Resourceが無効であるか、契約条件を満たしていません。",
                            SectionKind::D3D12ResourceTable);
        }

        if (resource.origin == ResourceOrigin::PackageOwned)
        {
            if (!resource.allocation.IsValid() || resource.allocation.value >= view.Allocations().size() ||
                resource.rebuildPolicy != RebuildPolicy::RecreateFromPackage ||
                resource.physicalInstanceCount == 0)
                return fail(PackageErrorCode::InvalidReference, "Packageが無効であるか、契約条件を満たしていません。",
                            SectionKind::D3D12ResourceTable);
            const auto& allocation = view.Allocations()[resource.allocation.value];
            if (allocation.physicalInstanceCount != resource.physicalInstanceCount)
                return fail(PackageErrorCode::InvalidReference, "Resourceが検証または実行の契約に違反しています。",
                            SectionKind::D3D12AllocationTable);
            allocationUsers[resource.allocation.value].push_back(static_cast<std::uint32_t>(index));
        }
        else if (resource.origin == ResourceOrigin::External)
        {
            if ((resource.resourceKind != ResourceKind::Buffer &&
                 resource.resourceKind != ResourceKind::Texture2D) ||
                resource.flags != 0 || resource.allocation.IsValid() ||
                resource.rebuildPolicy != RebuildPolicy::RequireExternalRebind ||
                resource.physicalInstanceCount != 1 || resource.initialDataSize != 0)
                return fail(PackageErrorCode::InvalidReference, "Resourceが無効であるか、契約条件を満たしていません。",
                            SectionKind::D3D12ResourceTable);
        }
        else if (resource.origin == ResourceOrigin::Surface)
        {
            if (resource.flags != 0 || resource.allocation.IsValid() ||
                resource.rebuildPolicy != RebuildPolicy::RuntimeManaged ||
                resource.physicalInstanceCount != 0 || resource.initialDataSize != 0)
                return fail(PackageErrorCode::InvalidReference, "Resourceが無効であるか、契約条件を満たしていません。",
                            SectionKind::D3D12ResourceTable);
        }
        else
        {
            return fail(PackageErrorCode::InvalidEnumValue, "Resourceに未対応または禁止された値が含まれています。",
                        SectionKind::D3D12ResourceTable);
        }
    }
    if (expectedFirstView != view.Views().size())
        return fail(PackageErrorCode::InvalidReference, "ResourceがCanonicalな順序または識別子規則に違反しています。",
                    SectionKind::D3D12ResourceTable);

    std::set<std::uint32_t> aliasGroups;
    for (std::size_t index = 0; index < view.Allocations().size(); ++index)
    {
        const auto& allocation = view.Allocations()[index];
        if ((allocation.kind != AllocationKind::Committed && allocation.kind != AllocationKind::Placed) ||
            (allocation.heapClass != HeapClass::DefaultBuffer &&
             allocation.heapClass != HeapClass::DefaultTexture &&
             allocation.heapClass != HeapClass::RenderTargetOrDepth &&
             allocation.heapClass != HeapClass::Upload) ||
            allocation.flags != 0 || allocation.physicalInstanceCount == 0 ||
            allocation.alignment == 0 || !std::has_single_bit(allocation.alignment) ||
            allocationUsers[index].empty())
            return fail(PackageErrorCode::InvalidReference, "Allocationが無効であるか、契約条件を満たしていません。",
                        SectionKind::D3D12AllocationTable);
        if (allocation.kind == AllocationKind::Committed)
        {
            if (allocation.aliasGroup != InvalidIndex || allocationUsers[index].size() != 1)
                return fail(PackageErrorCode::InvalidReference, "Allocationが無効であるか、契約条件を満たしていません。",
                            SectionKind::D3D12AllocationTable);
        }
        else
        {
            if (allocation.aliasGroup == InvalidIndex || allocationUsers[index].size() != 2 ||
                !aliasGroups.insert(allocation.aliasGroup).second)
                return fail(PackageErrorCode::InvalidReference, "Allocationが無効であるか、契約条件を満たしていません。",
                            SectionKind::D3D12AllocationTable);
            for (const auto resourceIndex : allocationUsers[index])
                if (!hasResourceFlag(view.Resources()[resourceIndex], ResourceFlags::Aliased))
                    return fail(PackageErrorCode::InvalidReference, "Allocationが検証または実行の契約に違反しています。",
                                SectionKind::D3D12AllocationTable);
        }
        for (const auto resourceIndex : allocationUsers[index])
        {
            const auto& resource = view.Resources()[resourceIndex];
            if (resource.resourceKind == ResourceKind::Buffer && allocation.sizeBytes < resource.sizeBytes)
                return fail(PackageErrorCode::InvalidReference, "Resourceが検証または実行の契約に違反しています。",
                            SectionKind::D3D12AllocationTable);
            if (resource.resourceKind != ResourceKind::Buffer && allocation.sizeBytes != 0)
                return fail(PackageErrorCode::InvalidReference, "Textureが検証または実行の契約に違反しています。",
                            SectionKind::D3D12AllocationTable);
            const auto expectedHeap = resource.resourceKind == ResourceKind::Buffer ?
                (resource.initialState.stateClass == StateClass::Explicit &&
                 resource.initialState.explicitBits == static_cast<std::uint32_t>(ExplicitStateBits::ConstantBuffer) ?
                    HeapClass::Upload : HeapClass::DefaultBuffer) :
                (resource.format == Format::D32Float ? HeapClass::RenderTargetOrDepth : HeapClass::DefaultTexture);
            if (allocation.heapClass != expectedHeap)
                return fail(PackageErrorCode::InvalidReference, "ResourceがCanonicalな契約と一致しません。",
                            SectionKind::D3D12AllocationTable);
            if ((allocation.heapClass == HeapClass::Upload) !=
                (resource.initialState.stateClass == StateClass::Explicit &&
                 resource.initialState.explicitBits == static_cast<std::uint32_t>(ExplicitStateBits::ConstantBuffer)))
                return fail(PackageErrorCode::InvalidReference, "ResourceがCanonicalな契約と一致しません。",
                            SectionKind::D3D12AllocationTable);
        }
    }

    std::array<std::set<std::uint32_t>, 4> descriptorIndices;
    for (const auto& resourceView : view.Views())
    {
        if (!resourceView.resource.IsValid() || resourceView.resource.value >= view.Resources().size() ||
            (resourceView.flags & ~KnownViewFlags) != 0 ||
            (hasViewFlag(resourceView, ResourceViewFlags::TemporalCurrent) &&
             hasViewFlag(resourceView, ResourceViewFlags::TemporalPrevious)))
            return fail(PackageErrorCode::InvalidReference, "Viewが無効であるか、契約条件を満たしていません。",
                        SectionKind::D3D12ViewTable);
        const auto& resource = view.Resources()[resourceView.resource.value];
        const bool temporal = hasResourceFlag(resource, ResourceFlags::Temporal);
        if (temporal != (resourceView.flags != 0))
            return fail(PackageErrorCode::InvalidReference, "Resourceの情報が途中で切れているか不足しています。",
                        SectionKind::D3D12ViewTable);

        const bool bufferView = resourceView.viewClass == ViewClass::VertexBuffer ||
            resourceView.viewClass == ViewClass::IndexBuffer ||
            resourceView.viewClass == ViewClass::ConstantBuffer ||
            resourceView.viewClass == ViewClass::UnorderedAccess ||
            resourceView.viewClass == ViewClass::CopySource ||
            resourceView.viewClass == ViewClass::CopyDestination ||
            (resourceView.viewClass == ViewClass::ShaderResource && resource.resourceKind == ResourceKind::Buffer);
        const bool textureView = resourceView.viewClass == ViewClass::RenderTarget ||
            resourceView.viewClass == ViewClass::DepthStencil ||
            resourceView.viewClass == ViewClass::PresentSource ||
            (resourceView.viewClass == ViewClass::ShaderResource && resource.resourceKind == ResourceKind::Texture2D);
        if ((bufferView && resource.resourceKind != ResourceKind::Buffer) ||
            (textureView && resource.resourceKind == ResourceKind::Buffer) || (!bufferView && !textureView))
            return fail(PackageErrorCode::InvalidReference, "ResourceがCanonicalな契約と一致しません。",
                        SectionKind::D3D12ViewTable);
        if (resourceView.viewClass == ViewClass::RenderTarget &&
            resource.resourceKind != ResourceKind::SurfaceImage &&
            !(resource.resourceKind == ResourceKind::Texture2D &&
              resource.origin == ResourceOrigin::External &&
              resource.format == Format::B8G8R8A8Unorm))
            return fail(PackageErrorCode::InvalidReference, "RenderTargetの参照先または所有関係が無効です。",
                        SectionKind::D3D12ViewTable);
        if (resourceView.viewClass == ViewClass::DepthStencil &&
            (resource.resourceKind != ResourceKind::Texture2D || resource.format != Format::D32Float))
            return fail(PackageErrorCode::InvalidReference, "Texture2Dの参照先または所有関係が無効です。",
                        SectionKind::D3D12ViewTable);
        if (resourceView.viewClass == ViewClass::PresentSource && resource.resourceKind != ResourceKind::SurfaceImage)
            return fail(PackageErrorCode::InvalidReference, "Surfaceの参照先または所有関係が無効です。",
                        SectionKind::D3D12ViewTable);

        if (resource.resourceKind == ResourceKind::Buffer)
        {
            if (resourceView.byteSize == 0 || resourceView.byteOffset > resource.sizeBytes ||
                resourceView.byteSize > resource.sizeBytes - resourceView.byteOffset ||
                resourceView.firstMip != 0 || resourceView.mipCount != 0 ||
                resourceView.firstArrayLayer != 0 || resourceView.arrayLayerCount != 0 ||
                resourceView.firstPlane != 0 || resourceView.planeCount != 0)
                return fail(PackageErrorCode::InvalidReference, "Bufferが無効であるか、契約条件を満たしていません。",
                            SectionKind::D3D12ViewTable);
        }
        else if (resourceView.byteOffset != 0 || resourceView.byteSize != 0 || resourceView.strideBytes != 0 ||
                 resourceView.firstMip != 0 || resourceView.mipCount != 1 ||
                 resourceView.firstArrayLayer != 0 || resourceView.arrayLayerCount != 1 ||
                 resourceView.firstPlane != 0 || resourceView.planeCount != 1 ||
                 resourceView.format != resource.format)
        {
            return fail(PackageErrorCode::InvalidReference, "Textureが無効であるか、契約条件を満たしていません。",
                        SectionKind::D3D12ViewTable);
        }

        const std::uint32_t expectedHeap =
            resourceView.viewClass == ViewClass::RenderTarget ? 1u :
            (resourceView.viewClass == ViewClass::ShaderResource ||
             resourceView.viewClass == ViewClass::UnorderedAccess) ? 2u :
            resourceView.viewClass == ViewClass::DepthStencil ? 3u : 0u;
        if (resourceView.descriptorHeapClass != expectedHeap ||
            descriptorBacked(resourceView.viewClass) != (resourceView.descriptorIndex != InvalidIndex))
            return fail(PackageErrorCode::InvalidReference, "ViewがCanonicalな順序または識別子規則に違反しています。",
                        SectionKind::D3D12ViewTable);
        if (resourceView.descriptorIndex != InvalidIndex)
        {
            std::uint32_t capacity = expectedHeap == 1 ? view.Profile().rtvDescriptorCount :
                expectedHeap == 2 ? view.Profile().shaderDescriptorCount : view.Profile().dsvDescriptorCount;
            const auto instances = resource.origin == ResourceOrigin::Surface ?
                view.Profile().surfaceImageCount : resource.physicalInstanceCount;
            const auto expectedStride = instances > 1 ? 1u : 0u;
            const auto last = static_cast<std::uint64_t>(resourceView.descriptorIndex) +
                static_cast<std::uint64_t>(resourceView.descriptorInstanceStride) *
                (instances == 0 ? 0 : instances - 1);
            if (capacity == 0 || resourceView.descriptorInstanceStride != expectedStride || last >= capacity)
                return fail(PackageErrorCode::InvalidReference, "Viewの値または参照範囲が許容範囲外です。",
                            SectionKind::D3D12ViewTable);
            for (std::uint32_t instance = 0; instance < std::max(1u, instances); ++instance)
            {
                const auto descriptor = resourceView.descriptorIndex + resourceView.descriptorInstanceStride * instance;
                if (!descriptorIndices[expectedHeap].insert(descriptor).second)
                    return fail(PackageErrorCode::InvalidReference, "Planが検証または実行の契約に違反しています。",
                                SectionKind::D3D12ViewTable);
            }
        }
        else if (resourceView.descriptorHeapClass != 0 || resourceView.descriptorInstanceStride != 0)
        {
            return fail(PackageErrorCode::InvalidReference, "Viewが検証または実行の契約に違反しています。",
                        SectionKind::D3D12ViewTable);
        }
    }

    for (const auto& shader : view.Shaders())
    {
        if ((shader.stage != ShaderStage::Vertex && shader.stage != ShaderStage::Pixel &&
             shader.stage != ShaderStage::Compute) || shader.format != ShaderBinaryFormat::Dxbc ||
            shader.shaderModelMajor != view.Profile().shaderModelMajor ||
            shader.shaderModelMinor != view.Profile().shaderModelMinor || shader.flags != 0 ||
            !blobValid(shader.bytecode, shader.bytecodeDigest))
            return fail(PackageErrorCode::DigestMismatch, "Shaderが無効であるか、契約条件を満たしていません。",
                        SectionKind::D3D12ShaderTable);
    }

    std::uint32_t expectedParameterFirst = 0;
    std::uint32_t expectedDescriptorFirst = 0;
    std::uint32_t expectedSamplerFirst = 0;
    for (const auto& layout : view.BindingLayouts())
    {
        if (layout.rootSignatureMajor != view.Profile().rootSignatureMajor ||
            layout.rootSignatureMinor != view.Profile().rootSignatureMinor || layout.flags != 0 ||
            layout.parameterRange.first != expectedParameterFirst ||
            layout.descriptorRange.first != expectedDescriptorFirst ||
            layout.staticSamplerRange.first != expectedSamplerFirst ||
            !rangeValid(layout.parameterRange, view.RootParameters().size()) ||
            layout.staticSamplerRange.count > 1 ||
            !blobValid(layout.serializedRootSignature, layout.layoutDigest))
            return fail(PackageErrorCode::InvalidReference, "Bindingが無効であるか、契約条件を満たしていません。",
                        SectionKind::D3D12BindingLayoutTable);
        std::uint32_t descriptorCount = 0;
        for (std::uint32_t index = layout.parameterRange.first;
             index < layout.parameterRange.first + layout.parameterRange.count; ++index)
        {
            const auto& parameter = view.RootParameters()[index];
            if (parameter.rootParameterIndex != index - layout.parameterRange.first ||
                parameter.registerSpace != 0 || parameter.flags != 0 ||
                (parameter.visibility != ShaderVisibility::All &&
                 parameter.visibility != ShaderVisibility::Vertex &&
                 parameter.visibility != ShaderVisibility::Pixel))
                return fail(PackageErrorCode::InvalidReference, "入力または内部状態が無効であるか、契約条件を満たしていません。",
                            SectionKind::D3D12RootParameterTable);
            if (parameter.kind == RootParameterKind::ConstantBuffer)
            {
                if (!parameter.dynamicSlot.IsValid() || parameter.dynamicSlot.value >= view.DynamicSlots().size() ||
                    parameter.staticView.IsValid())
                    return fail(PackageErrorCode::InvalidReference, "入力または内部状態が無効であるか、契約条件を満たしていません。",
                                SectionKind::D3D12RootParameterTable);
            }
            else if (parameter.kind == RootParameterKind::ShaderResourceTable ||
                     parameter.kind == RootParameterKind::UnorderedAccessTable)
            {
                if (!parameter.staticView.IsValid() || parameter.staticView.value >= view.Views().size() ||
                    parameter.dynamicSlot.IsValid())
                    return fail(PackageErrorCode::InvalidReference, "入力または内部状態が無効であるか、契約条件を満たしていません。",
                                SectionKind::D3D12RootParameterTable);
                const auto expectedViewClass = parameter.kind == RootParameterKind::ShaderResourceTable ?
                    ViewClass::ShaderResource : ViewClass::UnorderedAccess;
                if (view.Views()[parameter.staticView.value].viewClass != expectedViewClass)
                    return fail(PackageErrorCode::InvalidReference, "ViewがCanonicalな契約と一致しません。",
                                SectionKind::D3D12RootParameterTable);
                ++descriptorCount;
            }
            else
            {
                return fail(PackageErrorCode::InvalidEnumValue, "入力または内部状態に未対応または禁止された値が含まれています。",
                            SectionKind::D3D12RootParameterTable);
            }
        }
        if (layout.descriptorRange.count != descriptorCount)
            return fail(PackageErrorCode::InvalidReference, "Bindingが検証または実行の契約に違反しています。",
                        SectionKind::D3D12BindingLayoutTable);
        expectedParameterFirst += layout.parameterRange.count;
        expectedDescriptorFirst += layout.descriptorRange.count;
        expectedSamplerFirst += layout.staticSamplerRange.count;
    }
    if (expectedParameterFirst != view.RootParameters().size())
        return fail(PackageErrorCode::InvalidReference, "BindingがCanonicalな順序または識別子規則に違反しています。",
                    SectionKind::D3D12BindingLayoutTable);

    for (const auto& program : view.Programs())
    {
        if (!program.bindingLayout.IsValid() || program.bindingLayout.value >= view.BindingLayouts().size() ||
            program.flags != 0)
            return fail(PackageErrorCode::InvalidReference, "Programが無効であるか、契約条件を満たしていません。",
                        SectionKind::D3D12ProgramTable);
        if (program.kind == ProgramKind::Raster)
        {
            if (!program.vertexShader.IsValid() || program.vertexShader.value >= view.Shaders().size() ||
                !program.pixelShader.IsValid() || program.pixelShader.value >= view.Shaders().size() ||
                program.computeShader.IsValid() ||
                view.Shaders()[program.vertexShader.value].stage != ShaderStage::Vertex ||
                view.Shaders()[program.pixelShader.value].stage != ShaderStage::Pixel)
                return fail(PackageErrorCode::InvalidReference, "Shaderが無効であるか、契約条件を満たしていません。",
                            SectionKind::D3D12ProgramTable);
        }
        else if (program.kind == ProgramKind::Compute)
        {
            if (!program.computeShader.IsValid() || program.computeShader.value >= view.Shaders().size() ||
                program.vertexShader.IsValid() || program.pixelShader.IsValid() ||
                view.Shaders()[program.computeShader.value].stage != ShaderStage::Compute)
                return fail(PackageErrorCode::InvalidReference, "Shaderが無効であるか、契約条件を満たしていません。",
                            SectionKind::D3D12ProgramTable);
        }
        else
            return fail(PackageErrorCode::InvalidEnumValue, "Programに未対応または禁止された値が含まれています。", SectionKind::D3D12ProgramTable);
    }

    for (const auto& executable : view.Executables())
    {
        if (!executable.program.IsValid() || executable.program.value >= view.Programs().size() ||
            view.Programs()[executable.program.value].kind != ProgramKind::Raster ||
            !executable.bindingLayout.IsValid() ||
            executable.bindingLayout != view.Programs()[executable.program.value].bindingLayout ||
            !rangeValid(executable.vertexElementRange, view.VertexElements().size()) ||
            executable.vertexElementRange.count == 0 || executable.colorFormatRange.first != 0 ||
            executable.colorFormatRange.count != 1 || executable.colorFormat != Format::B8G8R8A8Unorm ||
            (executable.depthFormat != Format::Unknown && executable.depthFormat != Format::D32Float) ||
            executable.primitiveTopology != PrimitiveTopology::TriangleList ||
            executable.primitiveTopologyType != PrimitiveTopologyType::Triangle ||
            executable.sampleCount != 1 || executable.sampleQuality != 0)
            return fail(PackageErrorCode::InvalidReference, "Contractが無効であるか、契約条件を満たしていません。",
                        SectionKind::D3D12ExecutableTable);
    }
    for (const auto& executable : view.ComputeExecutables())
        if (!executable.program.IsValid() || executable.program.value >= view.Programs().size() ||
            view.Programs()[executable.program.value].kind != ProgramKind::Compute ||
            !executable.bindingLayout.IsValid() ||
            executable.bindingLayout != view.Programs()[executable.program.value].bindingLayout ||
            executable.flags != 0)
            return fail(PackageErrorCode::InvalidReference, "Contractが無効であるか、契約条件を満たしていません。",
                        SectionKind::D3D12ComputeExecutableTable);
    for (const auto& command : view.ComputeCommands())
        if (!command.executable.IsValid() || command.executable.value >= view.ComputeExecutables().size() ||
            command.threadGroupCountX == 0 || command.threadGroupCountY == 0 ||
            command.threadGroupCountZ == 0 || command.flags != 0)
            return fail(PackageErrorCode::InvalidReference, "入力または内部状態が無効であるか、契約条件を満たしていません。",
                        SectionKind::D3D12ComputeCommandTable);
    for (const auto& command : view.RasterCommands())
    {
        if (!command.executable.IsValid() || command.executable.value >= view.Executables().size() ||
            !rangeValid(command.vertexViewRange, view.Views().size()) || command.vertexViewRange.count != 1 ||
            !rangeValid(command.colorAttachmentRange, view.Views().size()) || command.colorAttachmentRange.count != 1 ||
            view.Views()[command.vertexViewRange.first].viewClass != ViewClass::VertexBuffer ||
            view.Views()[command.colorAttachmentRange.first].viewClass != ViewClass::RenderTarget ||
            command.indexView.IsValid() || command.indexCount != 0 || command.vertexCount == 0 ||
            command.instanceCount != 1 || command.firstVertex != 0 || command.firstInstance != 0 ||
            command.firstIndex != 0 || command.baseVertex != 0 ||
            (command.depthAttachment.IsValid() &&
             (command.depthAttachment.value >= view.Views().size() ||
              view.Views()[command.depthAttachment.value].viewClass != ViewClass::DepthStencil)) ||
            !command.attachmentOperation.IsValid() ||
            command.attachmentOperation.value >= view.AttachmentOperations().size())
            return fail(PackageErrorCode::InvalidReference, "入力または内部状態が無効であるか、契約条件を満たしていません。",
                        SectionKind::D3D12RasterCommandTable);
    }

    std::vector<bool> dynamicResources(view.Resources().size(), false);
    for (const auto& slot : view.DynamicSlots())
    {
        if (!slot.destinationResource.IsValid() || slot.destinationResource.value >= view.Resources().size() ||
            slot.requiredBytes == 0 || slot.requiredAlignment == 0 ||
            !std::has_single_bit(slot.requiredAlignment) || slot.flags != 0 ||
            dynamicResources[slot.destinationResource.value])
            return fail(PackageErrorCode::InvalidInvocationSchema, "Contractが無効であるか、契約条件を満たしていません。",
                        SectionKind::D3D12DynamicSlotTable);
        const auto& resource = view.Resources()[slot.destinationResource.value];
        if (resource.origin != ResourceOrigin::PackageOwned || resource.resourceKind != ResourceKind::Buffer ||
            resource.allocation.value >= view.Allocations().size() ||
            view.Allocations()[resource.allocation.value].heapClass != HeapClass::Upload ||
            slot.destinationOffset > resource.sizeBytes ||
            slot.requiredBytes > resource.sizeBytes - slot.destinationOffset ||
            slot.destinationOffset % slot.requiredAlignment != 0)
            return fail(PackageErrorCode::InvalidInvocationSchema, "Resourceが無効であるか、契約条件を満たしていません。",
                        SectionKind::D3D12DynamicSlotTable);
        dynamicResources[slot.destinationResource.value] = true;
    }
    for (std::size_t index = 0; index < view.Resources().size(); ++index)
        if ((view.Resources()[index].allocation.IsValid() &&
             view.Allocations()[view.Resources()[index].allocation.value].heapClass == HeapClass::Upload) !=
            dynamicResources[index])
            return fail(PackageErrorCode::InvalidInvocationSchema, "Resourceが検証または実行の契約に違反しています。",
                        SectionKind::D3D12DynamicSlotTable);

    std::vector<bool> externalResources(view.Resources().size(), false);
    for (const auto& slot : view.ExternalSlots())
    {
        if (!slot.resource.IsValid() || slot.resource.value >= view.Resources().size() ||
            externalResources[slot.resource.value] ||
            slot.synchronizationContract != ExternalSynchronizationContract::CompletionTokenRequired ||
            slot.flags != static_cast<std::uint32_t>(ExternalSlotFlags::Required))
            return fail(PackageErrorCode::InvalidInvocationSchema, "Contractが無効であるか、契約条件を満たしていません。",
                        SectionKind::D3D12ExternalSlotTable);
        const auto& resource = view.Resources()[slot.resource.value];
        const bool validBuffer = resource.resourceKind == ResourceKind::Buffer &&
            slot.requiredKind == ResourceKind::Buffer && slot.requiredFormat == Format::Unknown &&
            slot.minimumBytes > 0 && resource.format == Format::Unknown &&
            resource.sizeBytes == slot.minimumBytes;
        const bool validTexture = resource.resourceKind == ResourceKind::Texture2D &&
            slot.requiredKind == ResourceKind::Texture2D &&
            slot.requiredFormat == Format::B8G8R8A8Unorm && slot.minimumBytes == 0 &&
            resource.format == Format::B8G8R8A8Unorm && resource.sizeBytes == 0 &&
            resource.width > 0 && resource.height > 0 && resource.depthOrArraySize == 1 &&
            resource.mipLevels == 1 && resource.sampleCount == 1 && resource.planeCount == 1;
        if (resource.origin != ResourceOrigin::External || (!validBuffer && !validTexture))
            return fail(PackageErrorCode::InvalidInvocationSchema, "Resourceが無効であるか、契約条件を満たしていません。",
                        SectionKind::D3D12ExternalSlotTable);
        externalResources[slot.resource.value] = true;
    }
    for (std::size_t index = 0; index < view.Resources().size(); ++index)
        if ((view.Resources()[index].origin == ResourceOrigin::External) != externalResources[index])
            return fail(PackageErrorCode::InvalidInvocationSchema, "Resourceが検証または実行の契約に違反しています。",
                        SectionKind::D3D12ExternalSlotTable);

    std::vector<bool> surfaceResources(view.Resources().size(), false);
    for (const auto& slot : view.SurfaceSlots())
    {
        if (!slot.imageResource.IsValid() || slot.imageResource.value >= view.Resources().size() ||
            surfaceResources[slot.imageResource.value] || slot.flags != 0 ||
            slot.requiredFormat != Format::B8G8R8A8Unorm ||
            slot.acquiredState.stateClass != StateClass::Present ||
            slot.presentedState.stateClass != StateClass::Present)
            return fail(PackageErrorCode::InvalidInvocationSchema, "Contractが無効であるか、契約条件を満たしていません。",
                        SectionKind::D3D12SurfaceSlotTable);
        const auto& resource = view.Resources()[slot.imageResource.value];
        if (resource.origin != ResourceOrigin::Surface || resource.format != slot.requiredFormat)
            return fail(PackageErrorCode::InvalidInvocationSchema, "Resourceが無効であるか、契約条件を満たしていません。",
                        SectionKind::D3D12SurfaceSlotTable);
        surfaceResources[slot.imageResource.value] = true;
    }
    for (std::size_t index = 0; index < view.Resources().size(); ++index)
        if ((view.Resources()[index].origin == ResourceOrigin::Surface) != surfaceResources[index])
            return fail(PackageErrorCode::InvalidInvocationSchema, "Resourceが検証または実行の契約に違反しています。",
                        SectionKind::D3D12SurfaceSlotTable);

    const bool hasSurfaceSlot = !view.SurfaceSlots().empty();
    if (hasSurfaceSlot != (view.Profile().surfaceImageCount != 0))
        return fail(PackageErrorCode::InvalidTargetProfile,
                    "Packageに必要な情報または実行状態がありません。",
                    SectionKind::D3D12TargetProfile);
    if (!hasSurfaceSlot && std::any_of(view.Resources().begin(), view.Resources().end(),
        [](const ResourceArtifact& resource) { return resource.extentMode == ExtentMode::SurfaceRelative; }))
        return fail(PackageErrorCode::InvalidReference,
                    "Packageに必要な情報または実行状態がありません。",
                    SectionKind::D3D12ResourceTable);

    return base::Success<void, PackageError>();
}