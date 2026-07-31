runtime::RuntimeError Error(std::string stage, std::string message)
{
    return {std::move(stage), std::move(message)};
}

struct SignalPointRuntime final
{
    pkg::QueueId queue;
    std::uint64_t fenceValue = 0;
};

struct QueueRuntimeState final
{
    pkg::QueueId id;
    D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ComPtr<ID3D12CommandQueue> nativeQueue;
    ComPtr<ID3D12Fence> fence;
    HANDLE fenceEvent = nullptr;
    std::uint64_t nextFenceValue = 1;
    std::uint64_t lastFenceValue = 0;
    std::uint64_t frameFenceValue = 0;
    std::vector<std::uint64_t> frameSlotFenceValues;
    std::vector<std::vector<ComPtr<ID3D12CommandAllocator>>> frameAllocators;
    std::vector<std::vector<ComPtr<ID3D12GraphicsCommandList>>> frameCommandLists;
    std::uint32_t frameBatchCursor = 0;
    bool commandOpen = false;
    bool frameSubmitted = false;
};

std::string HResultText(HRESULT hr)
{
    std::ostringstream stream;
    stream << "HRESULT 0x" << std::hex << std::uppercase << static_cast<std::uint32_t>(hr);
    return stream.str();
}

runtime::RuntimeError HResultError(std::string stage, HRESULT hr, ID3D12Device* device = nullptr)
{
    std::string message = HResultText(hr);
    if (device && (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET))
        message += ", Device削除理由 " + HResultText(device->GetDeviceRemovedReason());
    return Error(std::move(stage), std::move(message));
}

struct ProcessDiagnosticsState final
{
    bool debugLayerEnabled = false;
};

const ProcessDiagnosticsState& ConfigureD3D12DiagnosticsOnce(bool requestDebugLayer)
{
    static std::once_flag configureOnce;
    static ProcessDiagnosticsState state;

    std::call_once(configureOnce, [requestDebugLayer]()
    {
        // DRED and the D3D12 debug layer are process-wide runtime settings.
        // They must be configured before the first device is created and must
        // not be repeated while an old device may still be alive during recovery.
        ComPtr<ID3D12DeviceRemovedExtendedDataSettings> dredSettings;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dredSettings))))
        {
            dredSettings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
            dredSettings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
        }

#if defined(_DEBUG)
        if (requestDebugLayer)
        {
            ComPtr<ID3D12Debug> debug;
            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
            {
                debug->EnableDebugLayer();
                state.debugLayerEnabled = true;
            }
        }
#else
        (void)requestDebugLayer;
#endif
    });

    return state;
}


bool SameLuid(const LUID& left, const LUID& right) noexcept
{
    return left.LowPart == right.LowPart && left.HighPart == right.HighPart;
}

DXGI_FORMAT ToDxgi(pkg::Format format)
{
    switch (format)
    {
    case pkg::Format::R32G32Float: return DXGI_FORMAT_R32G32_FLOAT;
    case pkg::Format::R32G32B32Float: return DXGI_FORMAT_R32G32B32_FLOAT;
    case pkg::Format::R32G32B32A32Float: return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case pkg::Format::B8G8R8A8Unorm: return DXGI_FORMAT_B8G8R8A8_UNORM;
    case pkg::Format::D32Float: return DXGI_FORMAT_D32_FLOAT;
    case pkg::Format::Unknown: return DXGI_FORMAT_UNKNOWN;
    default: return DXGI_FORMAT_UNKNOWN;
    }
}

bool IsCopyQueueState(const pkg::ResourceState& state) noexcept
{
    if (state.reserved != 0) return false;
    if (state.stateClass == pkg::StateClass::Common) return state.explicitBits == 0;
    if (state.stateClass != pkg::StateClass::Explicit) return false;
    const auto copySource = static_cast<std::uint32_t>(pkg::ExplicitStateBits::CopySource);
    const auto copyDestination = static_cast<std::uint32_t>(pkg::ExplicitStateBits::CopyDestination);
    return state.explicitBits == copySource || state.explicitBits == copyDestination;
}

D3D12_RESOURCE_STATES ToNativeState(
    const pkg::ResourceState& state,
    D3D12_COMMAND_LIST_TYPE queueType = D3D12_COMMAND_LIST_TYPE_DIRECT)
{
    if (state.stateClass == pkg::StateClass::Common) return D3D12_RESOURCE_STATE_COMMON;
    if (state.stateClass == pkg::StateClass::Present) return D3D12_RESOURCE_STATE_PRESENT;
    D3D12_RESOURCE_STATES result = static_cast<D3D12_RESOURCE_STATES>(0);
    const auto bits = state.explicitBits;
    if (bits & static_cast<std::uint32_t>(pkg::ExplicitStateBits::VertexBuffer)) result |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    if (bits & static_cast<std::uint32_t>(pkg::ExplicitStateBits::ConstantBuffer)) result |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    if (bits & static_cast<std::uint32_t>(pkg::ExplicitStateBits::IndexBuffer)) result |= D3D12_RESOURCE_STATE_INDEX_BUFFER;
    if (bits & static_cast<std::uint32_t>(pkg::ExplicitStateBits::RenderTarget)) result |= D3D12_RESOURCE_STATE_RENDER_TARGET;
    if (bits & static_cast<std::uint32_t>(pkg::ExplicitStateBits::DepthWrite)) result |= D3D12_RESOURCE_STATE_DEPTH_WRITE;
    if (bits & static_cast<std::uint32_t>(pkg::ExplicitStateBits::DepthRead)) result |= D3D12_RESOURCE_STATE_DEPTH_READ;
    if (bits & static_cast<std::uint32_t>(pkg::ExplicitStateBits::ShaderRead))
    {
        // Legacy v13 Package state. v14+ Packages use explicit stage scope; v15 also carries explicit signal identities.
        result |= queueType == D3D12_COMMAND_LIST_TYPE_COMPUTE
            ? D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
            : D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    }
    if (bits & static_cast<std::uint32_t>(pkg::ExplicitStateBits::PixelShaderRead))
        result |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    if (bits & static_cast<std::uint32_t>(pkg::ExplicitStateBits::NonPixelShaderRead))
        result |= D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    if (bits & static_cast<std::uint32_t>(pkg::ExplicitStateBits::UnorderedWrite)) result |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    if (bits & static_cast<std::uint32_t>(pkg::ExplicitStateBits::CopySource)) result |= D3D12_RESOURCE_STATE_COPY_SOURCE;
    if (bits & static_cast<std::uint32_t>(pkg::ExplicitStateBits::CopyDestination)) result |= D3D12_RESOURCE_STATE_COPY_DEST;
    if (bits & static_cast<std::uint32_t>(pkg::ExplicitStateBits::IndirectArgument)) result |= D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
    return result;
}

D3D12_RESOURCE_BARRIER TransitionBarrier(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
{
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    return barrier;
}

const char* SemanticName(pkg::VertexMeaning meaning)
{
    switch (meaning)
    {
    case pkg::VertexMeaning::Position: return "POSITION";
    case pkg::VertexMeaning::Color: return "COLOR";
    case pkg::VertexMeaning::TexCoord: return "TEXCOORD";
    default: return nullptr;
    }
}

// Canonical Level 4 v1 DeviceDomain.  It owns only the native adapter/device
// boundary and its epoch.  Leaf instances and Composition resources are
// destroyed by the Composition Runtime before this object changes epoch.
