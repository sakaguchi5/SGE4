Executor::Executor(ExecutorOptions options)
    : options_(options), timestampProfileCollector_(std::make_shared<detail::TimestampProfileCollector>())
{
}

base::Expected<std::unique_ptr<runtime::IPackageInstance>, runtime::RuntimeError> Executor::Load(
    std::shared_ptr<const package::FrozenExecutablePackage> package,
    runtime::ISurfaceHost* surface)
{
    auto view = pkg::D3D12PackageView::Decode(*package);
    if (!view) return base::Failure<std::unique_ptr<runtime::IPackageInstance>, runtime::RuntimeError>(Error("package/decode", view.error().message));
    auto instance = std::make_unique<Instance>(std::move(package), std::move(view).value(), surface,
        options_, timestampProfileCollector_);
    auto initialized = instance->Initialize();
    if (!initialized) return base::Failure<std::unique_ptr<runtime::IPackageInstance>, runtime::RuntimeError>(initialized.error());
    return base::Success<std::unique_ptr<runtime::IPackageInstance>, runtime::RuntimeError>(std::move(instance));
}

base::Expected<std::unique_ptr<runtime::IPackageDeviceDomain>, runtime::RuntimeError>
Executor::CreateDeviceDomain()
{
    auto domain = std::make_unique<DeviceDomain>(options_);
    auto initialized = domain->Initialize();
    if (!initialized)
        return base::Failure<std::unique_ptr<runtime::IPackageDeviceDomain>, runtime::RuntimeError>(
            initialized.error());
    return base::Success<std::unique_ptr<runtime::IPackageDeviceDomain>, runtime::RuntimeError>(
        std::move(domain));
}

base::Expected<std::unique_ptr<runtime::IPackageInstance>, runtime::RuntimeError>
Executor::LoadIntoDomain(
    runtime::IPackageDeviceDomain& domain,
    std::shared_ptr<const package::FrozenExecutablePackage> package,
    runtime::ISurfaceHost* surface)
{
    auto* nativeDomain = dynamic_cast<DeviceDomain*>(&domain);
    if (!nativeDomain || nativeDomain->State() != runtime::DeviceRuntimeState::Active)
        return base::Failure<std::unique_ptr<runtime::IPackageInstance>, runtime::RuntimeError>(
            Error("domain/load", "Deviceが検証または実行の契約に違反しています。"));
    auto view = pkg::D3D12PackageView::Decode(*package);
    if (!view)
        return base::Failure<std::unique_ptr<runtime::IPackageInstance>, runtime::RuntimeError>(
            Error("domain/package-decode", view.error().message));
    auto instance = std::make_unique<Instance>(
        std::move(package), std::move(view).value(), surface, options_, timestampProfileCollector_, nativeDomain);
    auto initialized = instance->Initialize();
    if (!initialized)
        return base::Failure<std::unique_ptr<runtime::IPackageInstance>, runtime::RuntimeError>(
            initialized.error());
    return base::Success<std::unique_ptr<runtime::IPackageInstance>, runtime::RuntimeError>(
        std::move(instance));
}

base::Expected<ExternalBufferBinding, runtime::RuntimeError>
Executor::CreateSharedBuffer(
    runtime::IPackageDeviceDomain& domain,
    std::uint32_t resourceIdentity,
    std::uint64_t sizeBytes,
    pkg::ResourceState initialState,
    std::span<const std::byte> initialBytes)
{
    auto* nativeDomain = dynamic_cast<DeviceDomain*>(&domain);
    if (!nativeDomain || nativeDomain->State() != runtime::DeviceRuntimeState::Active ||
        sizeBytes == 0 || initialBytes.size() > sizeBytes)
        return base::Failure<ExternalBufferBinding, runtime::RuntimeError>(
            Error("domain/shared-buffer", "Bufferが検証または実行の契約に違反しています。"));

    auto* device = nativeDomain->Device();
    D3D12_RESOURCE_DESC description{};
    description.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    description.Width = sizeBytes;
    description.Height = 1;
    description.DepthOrArraySize = 1;
    description.MipLevels = 1;
    description.SampleDesc.Count = 1;
    description.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    description.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    D3D12_HEAP_PROPERTIES defaultHeap{};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
    defaultHeap.CreationNodeMask = 1;
    defaultHeap.VisibleNodeMask = 1;
    ComPtr<ID3D12Resource> resource;
    HRESULT hr = device->CreateCommittedResource(
        &defaultHeap, D3D12_HEAP_FLAG_NONE, &description,
        D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&resource));
    if (FAILED(hr))
        return base::Failure<ExternalBufferBinding, runtime::RuntimeError>(
            HResultError("domain/create-shared-buffer", hr, device));

    D3D12_HEAP_PROPERTIES uploadHeap{};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
    uploadHeap.CreationNodeMask = 1;
    uploadHeap.VisibleNodeMask = 1;
    auto uploadDescription = description;
    uploadDescription.Flags = D3D12_RESOURCE_FLAG_NONE;
    ComPtr<ID3D12Resource> upload;
    hr = device->CreateCommittedResource(
        &uploadHeap, D3D12_HEAP_FLAG_NONE, &uploadDescription,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&upload));
    if (FAILED(hr))
        return base::Failure<ExternalBufferBinding, runtime::RuntimeError>(
            HResultError("domain/create-shared-upload", hr, device));
    void* mapped = nullptr;
    D3D12_RANGE noRead{0, 0};
    hr = upload->Map(0, &noRead, &mapped);
    if (FAILED(hr))
        return base::Failure<ExternalBufferBinding, runtime::RuntimeError>(
            HResultError("domain/map-shared-upload", hr, device));
    std::memset(mapped, 0, static_cast<std::size_t>(sizeBytes));
    if (!initialBytes.empty()) std::memcpy(mapped, initialBytes.data(), initialBytes.size());
    upload->Unmap(0, nullptr);

    D3D12_COMMAND_QUEUE_DESC queueDescription{};
    queueDescription.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> list;
    ComPtr<ID3D12Fence> fence;
    if (FAILED(hr = device->CreateCommandQueue(&queueDescription, IID_PPV_ARGS(&queue))) ||
        FAILED(hr = device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator))) ||
        FAILED(hr = device->CreateCommandList(
            0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr,
            IID_PPV_ARGS(&list))))
        return base::Failure<ExternalBufferBinding, runtime::RuntimeError>(
            HResultError("domain/create-shared-upload-commands", hr, device));

    auto toCopy = TransitionBarrier(
        resource.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
    list->ResourceBarrier(1, &toCopy);
    list->CopyBufferRegion(resource.Get(), 0, upload.Get(), 0, sizeBytes);
    const auto nativeInitial = ToNativeState(initialState);
    if (nativeInitial != D3D12_RESOURCE_STATE_COPY_DEST)
    {
        auto toInitial = TransitionBarrier(
            resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, nativeInitial);
        list->ResourceBarrier(1, &toInitial);
    }
    hr = list->Close();
    if (FAILED(hr))
        return base::Failure<ExternalBufferBinding, runtime::RuntimeError>(
            HResultError("domain/close-shared-upload", hr, device));
    ID3D12CommandList* lists[] = {list.Get()};
    queue->ExecuteCommandLists(1, lists);
    if (FAILED(hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))) ||
        FAILED(hr = queue->Signal(fence.Get(), 1)))
        return base::Failure<ExternalBufferBinding, runtime::RuntimeError>(
            HResultError("domain/signal-shared-upload", hr, device));
    HANDLE eventHandle = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!eventHandle)
        return base::Failure<ExternalBufferBinding, runtime::RuntimeError>(
            Error("domain/shared-upload-event", "入力または内部状態が検証または実行の契約に違反しています。"));
    DWORD waitResult = WAIT_OBJECT_0;
    if (fence->GetCompletedValue() < 1)
    {
        hr = fence->SetEventOnCompletion(1, eventHandle);
        if (SUCCEEDED(hr)) waitResult = WaitForSingleObject(eventHandle, INFINITE);
    }
    CloseHandle(eventHandle);
    if (FAILED(hr) || waitResult != WAIT_OBJECT_0)
        return base::Failure<ExternalBufferBinding, runtime::RuntimeError>(
            FAILED(hr) ? HResultError("domain/wait-shared-upload", hr, device)
                       : Error("domain/wait-shared-upload", "Waitが検証または実行の契約に違反しています。"));

    ExternalBufferBinding result;
    result.resource = std::make_shared<ExternalBufferResource>(
        resource, nativeDomain, nativeDomain->DeviceEpoch(), sizeBytes,
        resourceIdentity, initialState, initialState);
    result.availableAfter = std::make_shared<CompletionToken>(
        fence, 1, nativeDomain->DeviceEpoch(), nativeDomain, resourceIdentity);
    return base::Success<ExternalBufferBinding, runtime::RuntimeError>(
        std::move(result));
}

base::Expected<ExternalBufferBinding, runtime::RuntimeError>
Executor::CreateSharedTexture2D(
    runtime::IPackageDeviceDomain& domain,
    std::uint32_t resourceIdentity,
    std::uint32_t width,
    std::uint32_t height,
    std::uint32_t rowBytes,
    pkg::Format format,
    pkg::ResourceState initialState,
    std::span<const std::byte> initialBytes)
{
    auto* nativeDomain = dynamic_cast<DeviceDomain*>(&domain);
    const std::uint32_t bytesPerPixel =
        format == pkg::Format::B8G8R8A8Unorm ? 4u :
        format == pkg::Format::R32G32B32A32Float ? 16u : 0u;
    const std::uint64_t packedBytes = static_cast<std::uint64_t>(rowBytes) * height;
    const pkg::ResourceState renderTargetState{
        pkg::StateClass::Explicit, 0,
        static_cast<std::uint32_t>(pkg::ExplicitStateBits::RenderTarget)};
    const pkg::ResourceState unorderedWriteState{
        pkg::StateClass::Explicit, 0,
        static_cast<std::uint32_t>(pkg::ExplicitStateBits::UnorderedWrite)};
    const pkg::ResourceState pixelShaderReadState{
        pkg::StateClass::Explicit, 0,
        static_cast<std::uint32_t>(pkg::ExplicitStateBits::PixelShaderRead)};
    const pkg::ResourceState nonPixelShaderReadState{
        pkg::StateClass::Explicit, 0,
        static_cast<std::uint32_t>(pkg::ExplicitStateBits::NonPixelShaderRead)};
    const bool renderTarget = initialState == renderTargetState;
    const bool unorderedWrite = initialState == unorderedWriteState;
    const bool shaderRead = initialState == pixelShaderReadState ||
        initialState == nonPixelShaderReadState;
    if (!nativeDomain || nativeDomain->State() != runtime::DeviceRuntimeState::Active ||
        width == 0 || height == 0 || bytesPerPixel == 0 ||
        width > std::numeric_limits<std::uint32_t>::max() / bytesPerPixel ||
        static_cast<std::uint64_t>(rowBytes) !=
            static_cast<std::uint64_t>(width) * bytesPerPixel ||
        packedBytes > std::numeric_limits<std::size_t>::max() ||
        (!renderTarget && !unorderedWrite && !shaderRead) ||
        (renderTarget && format != pkg::Format::B8G8R8A8Unorm) ||
        (unorderedWrite && format != pkg::Format::R32G32B32A32Float) ||
        (!initialBytes.empty() && initialBytes.size() != packedBytes))
        return base::Failure<ExternalBufferBinding, runtime::RuntimeError>(
            Error("domain/shared-texture", "Textureが検証または実行の契約に違反しています。"));

    auto* device = nativeDomain->Device();
    D3D12_RESOURCE_DESC description{};
    description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    description.Width = width;
    description.Height = height;
    description.DepthOrArraySize = 1;
    description.MipLevels = 1;
    description.Format = ToDxgi(format);
    description.SampleDesc.Count = 1;
    description.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    description.Flags = renderTarget ? D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET :
        unorderedWrite ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS :
        D3D12_RESOURCE_FLAG_NONE;

    D3D12_HEAP_PROPERTIES defaultHeap{};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
    defaultHeap.CreationNodeMask = 1;
    defaultHeap.VisibleNodeMask = 1;
    ComPtr<ID3D12Resource> resource;
    HRESULT hr = device->CreateCommittedResource(
        &defaultHeap, D3D12_HEAP_FLAG_NONE, &description,
        D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&resource));
    if (FAILED(hr))
        return base::Failure<ExternalBufferBinding, runtime::RuntimeError>(
            HResultError("domain/create-shared-texture", hr, device));

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT rowCount = 0;
    UINT64 rowSize = 0;
    UINT64 uploadBytes = 0;
    device->GetCopyableFootprints(
        &description, 0, 1, 0, &footprint, &rowCount, &rowSize, &uploadBytes);
    if (rowCount != height || rowSize < rowBytes || uploadBytes == 0)
        return base::Failure<ExternalBufferBinding, runtime::RuntimeError>(
            Error("domain/shared-texture-footprint", "Textureが検証または実行の契約に違反しています。"));

    D3D12_RESOURCE_DESC uploadDescription{};
    uploadDescription.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    uploadDescription.Width = uploadBytes;
    uploadDescription.Height = 1;
    uploadDescription.DepthOrArraySize = 1;
    uploadDescription.MipLevels = 1;
    uploadDescription.SampleDesc.Count = 1;
    uploadDescription.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    D3D12_HEAP_PROPERTIES uploadHeap{};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
    uploadHeap.CreationNodeMask = 1;
    uploadHeap.VisibleNodeMask = 1;
    ComPtr<ID3D12Resource> upload;
    hr = device->CreateCommittedResource(
        &uploadHeap, D3D12_HEAP_FLAG_NONE, &uploadDescription,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&upload));
    if (FAILED(hr))
        return base::Failure<ExternalBufferBinding, runtime::RuntimeError>(
            HResultError("domain/create-shared-texture-upload", hr, device));

    void* mapped = nullptr;
    D3D12_RANGE noRead{0, 0};
    if (FAILED(hr = upload->Map(0, &noRead, &mapped)))
        return base::Failure<ExternalBufferBinding, runtime::RuntimeError>(
            HResultError("domain/map-shared-texture-upload", hr, device));
    std::memset(mapped, 0, static_cast<std::size_t>(uploadBytes));
    if (!initialBytes.empty())
    {
        auto* destination = static_cast<std::byte*>(mapped) + footprint.Offset;
        for (std::uint32_t row = 0; row < height; ++row)
            std::memcpy(destination + static_cast<std::size_t>(row) * footprint.Footprint.RowPitch,
                        initialBytes.data() + static_cast<std::size_t>(row) * rowBytes,
                        rowBytes);
    }
    upload->Unmap(0, nullptr);

    D3D12_COMMAND_QUEUE_DESC queueDescription{};
    queueDescription.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> list;
    ComPtr<ID3D12Fence> fence;
    if (FAILED(hr = device->CreateCommandQueue(&queueDescription, IID_PPV_ARGS(&queue))) ||
        FAILED(hr = device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator))) ||
        FAILED(hr = device->CreateCommandList(
            0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr,
            IID_PPV_ARGS(&list))))
        return base::Failure<ExternalBufferBinding, runtime::RuntimeError>(
            HResultError("domain/create-shared-texture-commands", hr, device));

    auto toCopy = TransitionBarrier(
        resource.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
    list->ResourceBarrier(1, &toCopy);
    D3D12_TEXTURE_COPY_LOCATION destination{};
    destination.pResource = resource.Get();
    destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    destination.SubresourceIndex = 0;
    D3D12_TEXTURE_COPY_LOCATION source{};
    source.pResource = upload.Get();
    source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    source.PlacedFootprint = footprint;
    list->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
    const auto nativeInitial = ToNativeState(initialState);
    if (nativeInitial != D3D12_RESOURCE_STATE_COPY_DEST)
    {
        auto toInitial = TransitionBarrier(
            resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, nativeInitial);
        list->ResourceBarrier(1, &toInitial);
    }
    if (FAILED(hr = list->Close()))
        return base::Failure<ExternalBufferBinding, runtime::RuntimeError>(
            HResultError("domain/close-shared-texture", hr, device));
    ID3D12CommandList* lists[] = {list.Get()};
    queue->ExecuteCommandLists(1, lists);
    if (FAILED(hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))) ||
        FAILED(hr = queue->Signal(fence.Get(), 1)))
        return base::Failure<ExternalBufferBinding, runtime::RuntimeError>(
            HResultError("domain/signal-shared-texture", hr, device));
    HANDLE eventHandle = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!eventHandle)
        return base::Failure<ExternalBufferBinding, runtime::RuntimeError>(
            Error("domain/shared-texture-event", "入力または内部状態が検証または実行の契約に違反しています。"));
    DWORD waitResult = WAIT_OBJECT_0;
    if (fence->GetCompletedValue() < 1)
    {
        hr = fence->SetEventOnCompletion(1, eventHandle);
        if (SUCCEEDED(hr)) waitResult = WaitForSingleObject(eventHandle, INFINITE);
    }
    CloseHandle(eventHandle);
    if (FAILED(hr) || waitResult != WAIT_OBJECT_0)
        return base::Failure<ExternalBufferBinding, runtime::RuntimeError>(
            FAILED(hr) ? HResultError("domain/wait-shared-texture", hr, device)
                       : Error("domain/wait-shared-texture", "Waitが検証または実行の契約に違反しています。"));

    ExternalBufferBinding result;
    result.resource = std::make_shared<ExternalTexture2DResource>(
        resource, nativeDomain, nativeDomain->DeviceEpoch(), width, height,
        rowBytes, format, resourceIdentity, initialState, initialState);
    result.availableAfter = std::make_shared<CompletionToken>(
        fence, 1, nativeDomain->DeviceEpoch(), nativeDomain, resourceIdentity);
    return base::Success<ExternalBufferBinding, runtime::RuntimeError>(std::move(result));
}

base::Expected<std::shared_ptr<runtime::ICompletionToken>, runtime::RuntimeError>
Executor::TransitionSharedResource(
    runtime::IPackageDeviceDomain& domain,
    const std::shared_ptr<runtime::IExternalResource>& resource,
    const std::shared_ptr<runtime::ICompletionToken>& safeAfter,
    pkg::ResourceState beforeState,
    pkg::ResourceState afterState)
{
    auto* nativeDomain = dynamic_cast<DeviceDomain*>(&domain);
    auto* native = dynamic_cast<ExternalResourceBase*>(resource.get());
    auto* token = dynamic_cast<CompletionToken*>(safeAfter.get());
    if (!nativeDomain || !native || !token ||
        nativeDomain->State() != runtime::DeviceRuntimeState::Active ||
        native->Owner() != nativeDomain || token->Owner() != nativeDomain ||
        native->DeviceEpoch() != nativeDomain->DeviceEpoch() ||
        token->DeviceEpoch() != nativeDomain->DeviceEpoch() ||
        native->Slot() != token->Slot() || !(native->CurrentState() == beforeState))
        return base::Failure<std::shared_ptr<runtime::ICompletionToken>, runtime::RuntimeError>(
            Error("domain/transition-shared-resource",
                "Resourceが検証または実行の契約に違反しています。"));
    if (beforeState == afterState)
        return base::Success<std::shared_ptr<runtime::ICompletionToken>, runtime::RuntimeError>(
            safeAfter);

    auto* device = nativeDomain->Device();
    D3D12_COMMAND_QUEUE_DESC queueDescription{};
    queueDescription.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> list;
    ComPtr<ID3D12Fence> fence;
    HRESULT hr = device->CreateCommandQueue(&queueDescription, IID_PPV_ARGS(&queue));
    if (FAILED(hr))
        return base::Failure<std::shared_ptr<runtime::ICompletionToken>, runtime::RuntimeError>(
            HResultError("domain/create-transition-queue", hr, device));
    if (FAILED(hr = device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator))) ||
        FAILED(hr = device->CreateCommandList(
            0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr,
            IID_PPV_ARGS(&list))))
        return base::Failure<std::shared_ptr<runtime::ICompletionToken>, runtime::RuntimeError>(
            HResultError("domain/create-transition-commands", hr, device));
    if (FAILED(hr = queue->Wait(token->NativeFence(), token->Value())))
        return base::Failure<std::shared_ptr<runtime::ICompletionToken>, runtime::RuntimeError>(
            HResultError("domain/wait-transition-source", hr, device));
    const auto barrier = TransitionBarrier(
        native->Native(), ToNativeState(beforeState), ToNativeState(afterState));
    list->ResourceBarrier(1, &barrier);
    if (FAILED(hr = list->Close()))
        return base::Failure<std::shared_ptr<runtime::ICompletionToken>, runtime::RuntimeError>(
            HResultError("domain/close-transition-list", hr, device));
    if (FAILED(hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))))
        return base::Failure<std::shared_ptr<runtime::ICompletionToken>, runtime::RuntimeError>(
            HResultError("domain/create-transition-fence", hr, device));
    ID3D12CommandList* lists[] = {list.Get()};
    queue->ExecuteCommandLists(1, lists);
    if (FAILED(hr = queue->Signal(fence.Get(), 1)))
        return base::Failure<std::shared_ptr<runtime::ICompletionToken>, runtime::RuntimeError>(
            HResultError("domain/signal-transition", hr, device));
    HANDLE eventHandle = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!eventHandle)
        return base::Failure<std::shared_ptr<runtime::ICompletionToken>, runtime::RuntimeError>(
            Error("domain/transition-event", "入力または内部状態が検証または実行の契約に違反しています。"));
    DWORD waitResult = WAIT_OBJECT_0;
    if (fence->GetCompletedValue() < 1)
    {
        hr = fence->SetEventOnCompletion(1, eventHandle);
        if (SUCCEEDED(hr)) waitResult = WaitForSingleObject(eventHandle, INFINITE);
    }
    CloseHandle(eventHandle);
    if (FAILED(hr) || waitResult != WAIT_OBJECT_0)
        return base::Failure<std::shared_ptr<runtime::ICompletionToken>, runtime::RuntimeError>(
            FAILED(hr) ? HResultError("domain/wait-transition", hr, device)
                       : Error("domain/wait-transition", "Waitが検証または実行の契約に違反しています。"));
    native->SetCurrentState(afterState);
    return base::Success<std::shared_ptr<runtime::ICompletionToken>, runtime::RuntimeError>(
        std::make_shared<CompletionToken>(
            fence, 1, nativeDomain->DeviceEpoch(), nativeDomain, native->Slot()));
}

base::Expected<std::shared_ptr<runtime::ICompletionToken>, runtime::RuntimeError>
Executor::TransitionSharedBuffer(
    runtime::IPackageDeviceDomain& domain,
    const std::shared_ptr<runtime::IExternalResource>& resource,
    const std::shared_ptr<runtime::ICompletionToken>& safeAfter,
    pkg::ResourceState beforeState,
    pkg::ResourceState afterState)
{
    if (!dynamic_cast<ExternalBufferResource*>(resource.get()))
        return base::Failure<std::shared_ptr<runtime::ICompletionToken>, runtime::RuntimeError>(
            Error("domain/transition-shared-buffer", "Bufferが検証または実行の契約に違反しています。"));
    return TransitionSharedResource(domain, resource, safeAfter, beforeState, afterState);
}

base::Expected<ExternalBufferReadback, runtime::RuntimeError>
Executor::ReadSharedBuffer(
    runtime::IPackageDeviceDomain& domain,
    const std::shared_ptr<runtime::IExternalResource>& resource,
    const std::shared_ptr<runtime::ICompletionToken>& safeAfter,
    pkg::ResourceState restoreState)
{
    auto* nativeDomain = dynamic_cast<DeviceDomain*>(&domain);
    auto* native = dynamic_cast<ExternalBufferResource*>(resource.get());
    auto* token = dynamic_cast<CompletionToken*>(safeAfter.get());
    if (!nativeDomain || !native || !token ||
        nativeDomain->State() != runtime::DeviceRuntimeState::Active ||
        native->Owner() != nativeDomain || token->Owner() != nativeDomain ||
        native->DeviceEpoch() != nativeDomain->DeviceEpoch() ||
        token->DeviceEpoch() != nativeDomain->DeviceEpoch() ||
        native->Slot() != token->Slot() || native->CurrentState() != restoreState)
        return base::Failure<ExternalBufferReadback, runtime::RuntimeError>(
            Error("domain/readback",
                "Resourceが検証または実行の契約に違反しています。"));

    auto* device = nativeDomain->Device();
    D3D12_RESOURCE_DESC description{};
    description.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    description.Width = native->SizeBytes();
    description.Height = 1;
    description.DepthOrArraySize = 1;
    description.MipLevels = 1;
    description.SampleDesc.Count = 1;
    description.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    D3D12_HEAP_PROPERTIES readbackHeap{};
    readbackHeap.Type = D3D12_HEAP_TYPE_READBACK;
    readbackHeap.CreationNodeMask = 1;
    readbackHeap.VisibleNodeMask = 1;
    ComPtr<ID3D12Resource> readback;
    HRESULT hr = device->CreateCommittedResource(
        &readbackHeap, D3D12_HEAP_FLAG_NONE, &description,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback));
    if (FAILED(hr))
        return base::Failure<ExternalBufferReadback, runtime::RuntimeError>(
            HResultError("domain/create-readback", hr, device));

    D3D12_COMMAND_QUEUE_DESC queueDescription{};
    queueDescription.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> list;
    ComPtr<ID3D12Fence> fence;
    if (FAILED(hr = device->CreateCommandQueue(&queueDescription, IID_PPV_ARGS(&queue))) ||
        FAILED(hr = device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator))) ||
        FAILED(hr = device->CreateCommandList(
            0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr,
            IID_PPV_ARGS(&list))))
        return base::Failure<ExternalBufferReadback, runtime::RuntimeError>(
            HResultError("domain/create-readback-commands", hr, device));
    if (FAILED(hr = queue->Wait(token->NativeFence(), token->Value())))
        return base::Failure<ExternalBufferReadback, runtime::RuntimeError>(
            HResultError("domain/wait-readback-source", hr, device));
    const auto nativeState = ToNativeState(restoreState);
    if (nativeState != D3D12_RESOURCE_STATE_COPY_SOURCE)
    {
        auto toCopy = TransitionBarrier(
            native->Native(), nativeState, D3D12_RESOURCE_STATE_COPY_SOURCE);
        list->ResourceBarrier(1, &toCopy);
    }
    list->CopyBufferRegion(readback.Get(), 0, native->Native(), 0, native->SizeBytes());
    if (nativeState != D3D12_RESOURCE_STATE_COPY_SOURCE)
    {
        auto restore = TransitionBarrier(
            native->Native(), D3D12_RESOURCE_STATE_COPY_SOURCE, nativeState);
        list->ResourceBarrier(1, &restore);
    }
    if (FAILED(hr = list->Close()))
        return base::Failure<ExternalBufferReadback, runtime::RuntimeError>(
            HResultError("domain/close-readback", hr, device));
    ID3D12CommandList* lists[] = {list.Get()};
    queue->ExecuteCommandLists(1, lists);
    if (FAILED(hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))) ||
        FAILED(hr = queue->Signal(fence.Get(), 1)))
        return base::Failure<ExternalBufferReadback, runtime::RuntimeError>(
            HResultError("domain/signal-readback", hr, device));
    HANDLE eventHandle = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!eventHandle)
        return base::Failure<ExternalBufferReadback, runtime::RuntimeError>(
            Error("domain/readback-event", "入力または内部状態が検証または実行の契約に違反しています。"));
    DWORD waitResult = WAIT_OBJECT_0;
    if (fence->GetCompletedValue() < 1)
    {
        hr = fence->SetEventOnCompletion(1, eventHandle);
        if (SUCCEEDED(hr)) waitResult = WaitForSingleObject(eventHandle, INFINITE);
    }
    CloseHandle(eventHandle);
    if (FAILED(hr) || waitResult != WAIT_OBJECT_0)
        return base::Failure<ExternalBufferReadback, runtime::RuntimeError>(
            FAILED(hr) ? HResultError("domain/wait-readback", hr, device)
                       : Error("domain/wait-readback", "Waitが検証または実行の契約に違反しています。"));

    ExternalBufferReadback result;
    result.bytes.resize(static_cast<std::size_t>(native->SizeBytes()));
    void* mapped = nullptr;
    D3D12_RANGE readRange{0, static_cast<SIZE_T>(native->SizeBytes())};
    if (FAILED(hr = readback->Map(0, &readRange, &mapped)))
        return base::Failure<ExternalBufferReadback, runtime::RuntimeError>(
            HResultError("domain/map-readback", hr, device));
    std::memcpy(result.bytes.data(), mapped, result.bytes.size());
    D3D12_RANGE noWrite{0, 0};
    readback->Unmap(0, &noWrite);
    result.availableAfter = std::make_shared<CompletionToken>(
        fence, 1, nativeDomain->DeviceEpoch(), nativeDomain, native->Slot());
    return base::Success<ExternalBufferReadback, runtime::RuntimeError>(
        std::move(result));
}

base::Expected<ExternalTexture2DReadback, runtime::RuntimeError>
Executor::ReadSharedTexture2D(
    runtime::IPackageDeviceDomain& domain,
    const std::shared_ptr<runtime::IExternalResource>& resource,
    const std::shared_ptr<runtime::ICompletionToken>& safeAfter,
    pkg::ResourceState restoreState)
{
    auto* nativeDomain = dynamic_cast<DeviceDomain*>(&domain);
    auto* native = dynamic_cast<ExternalTexture2DResource*>(resource.get());
    auto* token = dynamic_cast<CompletionToken*>(safeAfter.get());
    if (!nativeDomain || !native || !token ||
        nativeDomain->State() != runtime::DeviceRuntimeState::Active ||
        native->Owner() != nativeDomain || token->Owner() != nativeDomain ||
        native->DeviceEpoch() != nativeDomain->DeviceEpoch() ||
        token->DeviceEpoch() != nativeDomain->DeviceEpoch() ||
        native->Slot() != token->Slot() || native->CurrentState() != restoreState)
        return base::Failure<ExternalTexture2DReadback, runtime::RuntimeError>(
            Error("domain/readback-texture", "Resourceが検証または実行の契約に違反しています。"));

    auto* device = nativeDomain->Device();
    const auto description = native->Native()->GetDesc();
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT rowCount = 0;
    UINT64 rowSize = 0;
    UINT64 readbackBytes = 0;
    device->GetCopyableFootprints(
        &description, 0, 1, 0, &footprint, &rowCount, &rowSize, &readbackBytes);
    if (rowCount != native->Height() || rowSize < native->RowBytes() || readbackBytes == 0)
        return base::Failure<ExternalTexture2DReadback, runtime::RuntimeError>(
            Error("domain/readback-texture-footprint", "Textureが検証または実行の契約に違反しています。"));

    D3D12_RESOURCE_DESC readbackDescription{};
    readbackDescription.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    readbackDescription.Width = readbackBytes;
    readbackDescription.Height = 1;
    readbackDescription.DepthOrArraySize = 1;
    readbackDescription.MipLevels = 1;
    readbackDescription.SampleDesc.Count = 1;
    readbackDescription.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    D3D12_HEAP_PROPERTIES readbackHeap{};
    readbackHeap.Type = D3D12_HEAP_TYPE_READBACK;
    readbackHeap.CreationNodeMask = 1;
    readbackHeap.VisibleNodeMask = 1;
    ComPtr<ID3D12Resource> readback;
    HRESULT hr = device->CreateCommittedResource(
        &readbackHeap, D3D12_HEAP_FLAG_NONE, &readbackDescription,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback));
    if (FAILED(hr))
        return base::Failure<ExternalTexture2DReadback, runtime::RuntimeError>(
            HResultError("domain/create-texture-readback", hr, device));

    D3D12_COMMAND_QUEUE_DESC queueDescription{};
    queueDescription.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> list;
    ComPtr<ID3D12Fence> fence;
    if (FAILED(hr = device->CreateCommandQueue(&queueDescription, IID_PPV_ARGS(&queue))) ||
        FAILED(hr = device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator))) ||
        FAILED(hr = device->CreateCommandList(
            0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr,
            IID_PPV_ARGS(&list))))
        return base::Failure<ExternalTexture2DReadback, runtime::RuntimeError>(
            HResultError("domain/create-texture-readback-commands", hr, device));
    if (FAILED(hr = queue->Wait(token->NativeFence(), token->Value())))
        return base::Failure<ExternalTexture2DReadback, runtime::RuntimeError>(
            HResultError("domain/wait-texture-readback-source", hr, device));
    const auto nativeState = ToNativeState(restoreState);
    if (nativeState != D3D12_RESOURCE_STATE_COPY_SOURCE)
    {
        auto toCopy = TransitionBarrier(
            native->Native(), nativeState, D3D12_RESOURCE_STATE_COPY_SOURCE);
        list->ResourceBarrier(1, &toCopy);
    }
    D3D12_TEXTURE_COPY_LOCATION destination{};
    destination.pResource = readback.Get();
    destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    destination.PlacedFootprint = footprint;
    D3D12_TEXTURE_COPY_LOCATION source{};
    source.pResource = native->Native();
    source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    source.SubresourceIndex = 0;
    list->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
    if (nativeState != D3D12_RESOURCE_STATE_COPY_SOURCE)
    {
        auto restore = TransitionBarrier(
            native->Native(), D3D12_RESOURCE_STATE_COPY_SOURCE, nativeState);
        list->ResourceBarrier(1, &restore);
    }
    if (FAILED(hr = list->Close()))
        return base::Failure<ExternalTexture2DReadback, runtime::RuntimeError>(
            HResultError("domain/close-texture-readback", hr, device));
    ID3D12CommandList* lists[] = {list.Get()};
    queue->ExecuteCommandLists(1, lists);
    if (FAILED(hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))) ||
        FAILED(hr = queue->Signal(fence.Get(), 1)))
        return base::Failure<ExternalTexture2DReadback, runtime::RuntimeError>(
            HResultError("domain/signal-texture-readback", hr, device));
    HANDLE eventHandle = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!eventHandle)
        return base::Failure<ExternalTexture2DReadback, runtime::RuntimeError>(
            Error("domain/texture-readback-event", "入力または内部状態が検証または実行の契約に違反しています。"));
    DWORD waitResult = WAIT_OBJECT_0;
    if (fence->GetCompletedValue() < 1)
    {
        hr = fence->SetEventOnCompletion(1, eventHandle);
        if (SUCCEEDED(hr)) waitResult = WaitForSingleObject(eventHandle, INFINITE);
    }
    CloseHandle(eventHandle);
    if (FAILED(hr) || waitResult != WAIT_OBJECT_0)
        return base::Failure<ExternalTexture2DReadback, runtime::RuntimeError>(
            FAILED(hr) ? HResultError("domain/wait-texture-readback", hr, device)
                       : Error("domain/wait-texture-readback", "Waitが検証または実行の契約に違反しています。"));

    ExternalTexture2DReadback result;
    result.width = native->Width();
    result.height = native->Height();
    result.rowBytes = native->RowBytes();
    result.format = native->Format();
    result.bytes.resize(static_cast<std::size_t>(result.rowBytes) * result.height);
    void* mapped = nullptr;
    D3D12_RANGE readRange{0, static_cast<SIZE_T>(readbackBytes)};
    if (FAILED(hr = readback->Map(0, &readRange, &mapped)))
        return base::Failure<ExternalTexture2DReadback, runtime::RuntimeError>(
            HResultError("domain/map-texture-readback", hr, device));
    const auto* sourceBytes = static_cast<const std::byte*>(mapped) + footprint.Offset;
    for (std::uint32_t row = 0; row < result.height; ++row)
        std::memcpy(result.bytes.data() + static_cast<std::size_t>(row) * result.rowBytes,
                    sourceBytes + static_cast<std::size_t>(row) * footprint.Footprint.RowPitch,
                    result.rowBytes);
    D3D12_RANGE noWrite{0, 0};
    readback->Unmap(0, &noWrite);
    result.availableAfter = std::make_shared<CompletionToken>(
        fence, 1, nativeDomain->DeviceEpoch(), nativeDomain, native->Slot());
    return base::Success<ExternalTexture2DReadback, runtime::RuntimeError>(std::move(result));
}

base::Expected<runtime::DeviceRecoveryReport, runtime::RuntimeError>
Executor::RecoverDeviceDomain(
    runtime::IPackageDeviceDomain& domain,
    runtime::DeviceRecoveryMode mode)
{
    auto* native = dynamic_cast<DeviceDomain*>(&domain);
    if (!native)
        return base::Failure<runtime::DeviceRecoveryReport, runtime::RuntimeError>(
            Error("domain/recovery", "Deviceが検証または実行の契約に違反しています。"));
    return native->Recover(mode);
}

base::Expected<runtime::FrameSubmission, runtime::RuntimeError> Executor::Submit(
    runtime::IPackageInstance& instance,
    const runtime::FrameInvocation& invocation)
{
    auto* d3dInstance = dynamic_cast<Instance*>(&instance);
    if (!d3dInstance) return base::Failure<runtime::FrameSubmission, runtime::RuntimeError>(Error("submit", "Packageが検証または実行の契約に違反しています。"));
    const auto begin = std::chrono::steady_clock::now();
    auto submitted = d3dInstance->Submit(invocation);
    const auto end = std::chrono::steady_clock::now();
    if (submitted && options_.enableTimestampProfiling)
    {
        const double nanoseconds = std::chrono::duration<double, std::nano>(end - begin).count();
        d3dInstance->CompleteTimestampProfile(invocation.frameNumber, nanoseconds);
    }
    return submitted;
}

std::vector<TimestampProfileSample> Executor::ConsumeTimestampProfileSamples()
{
    std::vector<TimestampProfileSample> result;
    if (!options_.enableTimestampProfiling) return result;
    std::scoped_lock lock(timestampProfileCollector_->mutex);
    auto& records = timestampProfileCollector_->records;
    std::erase_if(records, [](const auto& weak) { return weak.expired(); });
    for (const auto& weak : records)
    {
        const auto record = weak.lock();
        if (!record || !record->ready || !record->mappedQueryValues || record->timestampFrequency == 0)
            continue;
        const auto begin = record->mappedQueryValues[0];
        const auto end = record->mappedQueryValues[1];
        TimestampProfileSample sample;
        sample.packageExecutionDigest = record->packageExecutionDigest;
        sample.frameNumber = record->frameNumber;
        sample.instanceOrdinal = record->instanceOrdinal;
        sample.submissionOrdinal = record->submissionOrdinal;
        sample.commandRecordingNanoseconds = record->commandRecordingNanoseconds;
        sample.gpuNanoseconds = end >= begin
            ? static_cast<double>(end - begin) * 1.0e9 / static_cast<double>(record->timestampFrequency)
            : 0.0;
        sample.dispatchCount = record->dispatchCount;
        sample.barrierCount = record->barrierCount;
        result.push_back(sample);
        record->ready = false;
    }
    std::ranges::sort(result, {}, &TimestampProfileSample::submissionOrdinal);
    return result;
}

base::Expected<runtime::DeviceRecoveryReport, runtime::RuntimeError> Executor::RecoverDevice(
    runtime::IPackageInstance& instance,
    runtime::DeviceRecoveryMode mode)
{
    auto* d3dInstance = dynamic_cast<Instance*>(&instance);
    if (!d3dInstance) return base::Failure<runtime::DeviceRecoveryReport, runtime::RuntimeError>(
        Error("recovery", "Packageが検証または実行の契約に違反しています。"));
    return d3dInstance->RecoverDevice(mode);
}

base::Expected<ExternalBufferBinding, runtime::RuntimeError> Executor::CreateExternalBuffer(
    runtime::IPackageInstance& instance,
    std::uint32_t slot,
    std::span<const std::byte> initialBytes)
{
    auto* d3dInstance = dynamic_cast<Instance*>(&instance);
    if (!d3dInstance)
        return base::Failure<ExternalBufferBinding, runtime::RuntimeError>(
            Error("external", "Packageが検証または実行の契約に違反しています。"));
    return d3dInstance->CreateExternalBuffer(slot, initialBytes);
}

base::Expected<ExternalBufferReadback, runtime::RuntimeError> Executor::ReadExternalBuffer(
    runtime::IPackageInstance& instance,
    const std::shared_ptr<runtime::IExternalResource>& resource,
    const std::shared_ptr<runtime::ICompletionToken>& safeAfter)
{
    auto* d3dInstance = dynamic_cast<Instance*>(&instance);
    if (!d3dInstance)
        return base::Failure<ExternalBufferReadback, runtime::RuntimeError>(
            Error("external/readback", "Packageが検証または実行の契約に違反しています。"));
    return d3dInstance->ReadExternalBuffer(resource, safeAfter);
}

base::Expected<ExternalBufferBinding, runtime::RuntimeError> Executor::CreateExternalColorBuffer(
    runtime::IPackageInstance& instance,
    const std::array<float, 4>& color)
{
    return CreateExternalBuffer(instance, 0, std::as_bytes(std::span<const float>(color)));
}

bool Executor::SupportsOperation(pkg::D3D12OperationCode code) noexcept
{
    return pkg::IsKnownOperation(code);
}