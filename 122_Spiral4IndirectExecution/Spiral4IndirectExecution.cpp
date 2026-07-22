#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "Spiral4IndirectExecution.h"

#include <Windows.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iterator>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace sge4_5::spiral4::execution
{
namespace
{
using Microsoft::WRL::ComPtr;

struct Float4 final
{
    float x;
    float y;
    float z;
    float w;
};

struct Motor final
{
    Float4 qr;
    Float4 qd;
};

struct DispatchArguments final
{
    std::uint32_t threadGroupCountX;
    std::uint32_t threadGroupCountY;
    std::uint32_t threadGroupCountZ;
};

static_assert(sizeof(Float4) == 16);
static_assert(sizeof(Motor) == 32);
static_assert(sizeof(DispatchArguments) == 12);

constexpr Float4 InactiveSentinel{
    123456.25f,
    -654321.5f,
    777.0f,
    -999.0f
};


std::string Utf8FromWide(const wchar_t* value)
{
    if (!value || *value == L'\0') return {};
    const int required = WideCharToMultiByte(
        CP_UTF8,
        0,
        value,
        -1,
        nullptr,
        0,
        nullptr,
        nullptr);
    if (required <= 1) return {};
    std::string result(static_cast<std::size_t>(required), '\0');
    const int written = WideCharToMultiByte(
        CP_UTF8,
        0,
        value,
        -1,
        result.data(),
        required,
        nullptr,
        nullptr);
    if (written != required)
    {
        throw std::runtime_error("WideCharToMultiByte failed.");
    }
    result.resize(static_cast<std::size_t>(required - 1));
    return result;
}

IndirectExecutionErrorV1 Error(std::string stage, std::string message, HRESULT hr = S_OK)
{
    return IndirectExecutionErrorV1{
        std::move(stage),
        std::move(message),
        static_cast<long>(hr)
    };
}

void ThrowIfFailed(HRESULT hr, const char* stage)
{
    if (FAILED(hr))
    {
        std::ostringstream stream;
        stream << stage << " failed with HRESULT 0x"
               << std::hex << static_cast<unsigned long>(hr);
        throw std::runtime_error(stream.str());
    }
}

D3D12_HEAP_PROPERTIES HeapProperties(D3D12_HEAP_TYPE type)
{
    D3D12_HEAP_PROPERTIES value{};
    value.Type = type;
    value.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    value.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    value.CreationNodeMask = 1;
    value.VisibleNodeMask = 1;
    return value;
}

D3D12_RESOURCE_DESC BufferDescription(
    std::uint64_t size,
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE)
{
    D3D12_RESOURCE_DESC value{};
    value.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    value.Alignment = 0;
    value.Width = size;
    value.Height = 1;
    value.DepthOrArraySize = 1;
    value.MipLevels = 1;
    value.Format = DXGI_FORMAT_UNKNOWN;
    value.SampleDesc.Count = 1;
    value.SampleDesc.Quality = 0;
    value.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    value.Flags = flags;
    return value;
}

ComPtr<ID3D12Resource> CreateBuffer(
    ID3D12Device* device,
    D3D12_HEAP_TYPE heapType,
    std::uint64_t size,
    D3D12_RESOURCE_STATES initialState,
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE)
{
    ComPtr<ID3D12Resource> resource;
    const auto heap = HeapProperties(heapType);
    const auto desc = BufferDescription(size, flags);
    ThrowIfFailed(
        device->CreateCommittedResource(
            &heap,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            initialState,
            nullptr,
            IID_PPV_ARGS(&resource)),
        "CreateCommittedResource");
    return resource;
}

D3D12_RESOURCE_BARRIER TransitionBarrier(
    ID3D12Resource* resource,
    D3D12_RESOURCE_STATES before,
    D3D12_RESOURCE_STATES after)
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

D3D12_RESOURCE_BARRIER UavBarrier(ID3D12Resource* resource)
{
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.UAV.pResource = resource;
    return barrier;
}

ComPtr<ID3DBlob> CompileShader(
    const std::string& source,
    const char* target,
    const char* stage)
{
    ComPtr<ID3DBlob> bytecode;
    ComPtr<ID3DBlob> errors;
    const UINT flags =
#if defined(_DEBUG)
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION |
#endif
        D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS;

    const HRESULT hr = D3DCompile(
        source.data(),
        source.size(),
        "Spiral4CU2Generated.hlsl",
        nullptr,
        nullptr,
        "CSMain",
        target,
        flags,
        0,
        &bytecode,
        &errors);

    if (FAILED(hr))
    {
        std::string message = stage;
        message += " HLSL compilation failed";
        if (errors && errors->GetBufferPointer())
        {
            message += ": ";
            message.append(
                static_cast<const char*>(errors->GetBufferPointer()),
                errors->GetBufferSize());
        }
        throw std::runtime_error(message);
    }
    return bytecode;
}

ComPtr<ID3D12RootSignature> CreateRootSignature(
    ID3D12Device* device,
    std::span<const D3D12_ROOT_PARAMETER> parameters)
{
    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.NumParameters = static_cast<UINT>(parameters.size());
    desc.pParameters = parameters.data();
    desc.NumStaticSamplers = 0;
    desc.pStaticSamplers = nullptr;
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    ComPtr<ID3DBlob> serialized;
    ComPtr<ID3DBlob> errors;
    const HRESULT serializedHr = D3D12SerializeRootSignature(
        &desc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &serialized,
        &errors);
    if (FAILED(serializedHr))
    {
        std::string message = "D3D12SerializeRootSignature failed";
        if (errors && errors->GetBufferPointer())
        {
            message += ": ";
            message.append(
                static_cast<const char*>(errors->GetBufferPointer()),
                errors->GetBufferSize());
        }
        throw std::runtime_error(message);
    }

    ComPtr<ID3D12RootSignature> result;
    ThrowIfFailed(
        device->CreateRootSignature(
            0,
            serialized->GetBufferPointer(),
            serialized->GetBufferSize(),
            IID_PPV_ARGS(&result)),
        "CreateRootSignature");
    return result;
}

ComPtr<ID3D12PipelineState> CreateComputePipeline(
    ID3D12Device* device,
    ID3D12RootSignature* rootSignature,
    ID3DBlob* bytecode)
{
    D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = rootSignature;
    desc.CS.pShaderBytecode = bytecode->GetBufferPointer();
    desc.CS.BytecodeLength = bytecode->GetBufferSize();

    ComPtr<ID3D12PipelineState> result;
    ThrowIfFailed(
        device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&result)),
        "CreateComputePipelineState");
    return result;
}

std::string ProducerShaderSource(
    const contract::FrozenSingleIndirectArtifactV1& artifact)
{
    std::ostringstream source;
    source
        << "static const uint MaxWorkCount=" << artifact.MaxWorkCount() << "u;\n"
        << "static const uint ThreadsPerGroup=" << artifact.ThreadsPerGroup() << "u;\n"
        << R"hlsl(
StructuredBuffer<uint> ActiveCount : register(t0);
RWStructuredBuffer<uint3> DispatchArgs : register(u0);
[numthreads(1,1,1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    if (id.x != 0) return;
    uint n = min(ActiveCount[0], MaxWorkCount);
    uint groups = n == 0 ? 0 : 1 + ((n - 1) / ThreadsPerGroup);
    DispatchArgs[0] = uint3(groups, 1, 1);
}
)hlsl";
    return source.str();
}

std::string ConsumerShaderSource(
    const contract::FrozenSingleIndirectArtifactV1& artifact)
{
    std::ostringstream source;
    source
        << "static const uint MaxWorkCount=" << artifact.MaxWorkCount() << "u;\n"
        << "static const uint ReusePerBone=512u;\n"
        << R"hlsl(
struct Motor { float4 qr; float4 qd; };
float4 QMul(float4 a, float4 b)
{
    return float4(
        a.x*b.x-dot(a.yzw,b.yzw),
        a.x*b.yzw+b.x*a.yzw+cross(a.yzw,b.yzw));
}
float4 QConj(float4 q) { return float4(q.x,-q.y,-q.z,-q.w); }

StructuredBuffer<Motor> Transforms : register(t0);
StructuredBuffer<float4> Points : register(t1);
StructuredBuffer<uint> ActiveCount : register(t2);
RWStructuredBuffer<float4> Output : register(u0);

[numthreads(64,1,1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    uint n = id.x;
    uint active = min(ActiveCount[0], MaxWorkCount);
    if (n >= active) return;

    uint bone = n / ReusePerBone;
    Motor m = Transforms[bone];
    float3 p = Points[n].xyz;
    float3 q = QMul(QMul(m.qr, float4(0.0f,p)), QConj(m.qr)).yzw;
    float3 t = 2.0f * QMul(m.qd, QConj(m.qr)).yzw;
    Output[n] = float4(q+t, 1.0f);
}
)hlsl";
    return source.str();
}

std::vector<Motor> BuildTransforms()
{
    std::vector<Motor> values(8);
    for (std::uint32_t bone = 0; bone < values.size(); ++bone)
    {
        const float tx = static_cast<float>(bone) * 0.5f;
        const float ty = -static_cast<float>(bone) * 0.25f;
        const float tz = static_cast<float>(bone) * 0.125f;
        values[bone].qr = Float4{1.0f, 0.0f, 0.0f, 0.0f};
        values[bone].qd = Float4{0.0f, tx * 0.5f, ty * 0.5f, tz * 0.5f};
    }
    return values;
}

std::vector<Float4> BuildPoints(std::uint32_t count)
{
    std::vector<Float4> values(count);
    for (std::uint32_t i = 0; i < count; ++i)
    {
        values[i] = Float4{
            static_cast<float>(i % 17u) * 0.25f,
            -static_cast<float>((i / 17u) % 13u) * 0.125f,
            static_cast<float>(i % 7u) * 0.5f,
            1.0f
        };
    }
    return values;
}

Float4 ExpectedPoint(const Float4& point, std::uint32_t index)
{
    const std::uint32_t bone = index / 512u;
    const float tx = static_cast<float>(bone) * 0.5f;
    const float ty = -static_cast<float>(bone) * 0.25f;
    const float tz = static_cast<float>(bone) * 0.125f;
    return Float4{point.x + tx, point.y + ty, point.z + tz, 1.0f};
}

bool EqualSentinel(const Float4& value)
{
    return std::memcmp(&value, &InactiveSentinel, sizeof(Float4)) == 0;
}

D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle(
    D3D12_CPU_DESCRIPTOR_HANDLE start,
    UINT increment,
    UINT index)
{
    start.ptr += static_cast<SIZE_T>(increment) * index;
    return start;
}

D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle(
    D3D12_GPU_DESCRIPTOR_HANDLE start,
    UINT increment,
    UINT index)
{
    start.ptr += static_cast<UINT64>(increment) * index;
    return start;
}

struct EventHandle final
{
    HANDLE value = nullptr;
    ~EventHandle()
    {
        if (value) CloseHandle(value);
    }
};
}

base::Result<SingleIndirectArchitectureResultV1, IndirectExecutionErrorV1>
RunSingleIndirectArchitectureOnWarpV1(
    const semantic::ActiveWorkSemanticV1& semantic,
    const contract::FrozenSingleIndirectArtifactV1& artifact,
    std::span<const std::uint32_t> activeCounts)
{
    try
    {
        auto artifactValidation =
            contract::ValidateSingleIndirectArtifactForExecutionV1(artifact, semantic);
        if (!artifactValidation)
        {
            return base::Result<SingleIndirectArchitectureResultV1, IndirectExecutionErrorV1>::Failure(
                Error(
                    artifactValidation.Error().stage,
                    artifactValidation.Error().message));
        }

        for (const std::uint32_t active : activeCounts)
        {
            if (active > artifact.MaxWorkCount())
            {
                return base::Result<SingleIndirectArchitectureResultV1, IndirectExecutionErrorV1>::Failure(
                    Error("input/active-count", "Active Work Count exceeds the Frozen maximum."));
            }
        }

        ComPtr<IDXGIFactory6> factory;
        ThrowIfFailed(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)), "CreateDXGIFactory2");

        ComPtr<IDXGIAdapter> warpAdapter;
        ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)), "EnumWarpAdapter");

        DXGI_ADAPTER_DESC adapterDesc{};
        ThrowIfFailed(warpAdapter->GetDesc(&adapterDesc), "GetDesc");

        ComPtr<ID3D12Device> device;
        ThrowIfFailed(
            D3D12CreateDevice(
                warpAdapter.Get(),
                D3D_FEATURE_LEVEL_11_0,
                IID_PPV_ARGS(&device)),
            "D3D12CreateDevice(WARP)");

        D3D12_COMMAND_QUEUE_DESC queueDesc{};
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
        queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.NodeMask = 0;

        ComPtr<ID3D12CommandQueue> queue;
        ThrowIfFailed(
            device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue)),
            "CreateCommandQueue");

        ComPtr<ID3D12CommandAllocator> allocator;
        ThrowIfFailed(
            device->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_COMPUTE,
                IID_PPV_ARGS(&allocator)),
            "CreateCommandAllocator");

        ComPtr<ID3D12GraphicsCommandList> list;
        ThrowIfFailed(
            device->CreateCommandList(
                0,
                D3D12_COMMAND_LIST_TYPE_COMPUTE,
                allocator.Get(),
                nullptr,
                IID_PPV_ARGS(&list)),
            "CreateCommandList");
        ThrowIfFailed(list->Close(), "Initial command-list close");

        D3D12_DESCRIPTOR_RANGE producerRanges[2]{};
        producerRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        producerRanges[0].NumDescriptors = 1;
        producerRanges[0].BaseShaderRegister = 0;
        producerRanges[0].OffsetInDescriptorsFromTableStart =
            D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        producerRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        producerRanges[1].NumDescriptors = 1;
        producerRanges[1].BaseShaderRegister = 0;
        producerRanges[1].OffsetInDescriptorsFromTableStart =
            D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_ROOT_PARAMETER producerParameters[2]{};
        for (UINT i = 0; i < 2; ++i)
        {
            producerParameters[i].ParameterType =
                D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            producerParameters[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
            producerParameters[i].DescriptorTable.NumDescriptorRanges = 1;
            producerParameters[i].DescriptorTable.pDescriptorRanges =
                &producerRanges[i];
        }

        D3D12_DESCRIPTOR_RANGE consumerRanges[2]{};
        consumerRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        consumerRanges[0].NumDescriptors = 3;
        consumerRanges[0].BaseShaderRegister = 0;
        consumerRanges[0].OffsetInDescriptorsFromTableStart =
            D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        consumerRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        consumerRanges[1].NumDescriptors = 1;
        consumerRanges[1].BaseShaderRegister = 0;
        consumerRanges[1].OffsetInDescriptorsFromTableStart =
            D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_ROOT_PARAMETER consumerParameters[2]{};
        for (UINT i = 0; i < 2; ++i)
        {
            consumerParameters[i].ParameterType =
                D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            consumerParameters[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
            consumerParameters[i].DescriptorTable.NumDescriptorRanges = 1;
            consumerParameters[i].DescriptorTable.pDescriptorRanges =
                &consumerRanges[i];
        }

        auto producerRoot = CreateRootSignature(device.Get(), producerParameters);
        auto consumerRoot = CreateRootSignature(device.Get(), consumerParameters);

        auto producerShader = CompileShader(
            ProducerShaderSource(artifact),
            "cs_5_1",
            "Argument Producer");
        auto consumerShader = CompileShader(
            ConsumerShaderSource(artifact),
            "cs_5_1",
            "Indirect Consumer");

        const auto producerShaderBytecodeDigest = base::Sha256(
            std::span<const std::byte>(
                static_cast<const std::byte*>(
                    producerShader->GetBufferPointer()),
                producerShader->GetBufferSize()));
        const auto consumerShaderBytecodeDigest = base::Sha256(
            std::span<const std::byte>(
                static_cast<const std::byte*>(
                    consumerShader->GetBufferPointer()),
                consumerShader->GetBufferSize()));

        auto producerPipeline = CreateComputePipeline(
            device.Get(),
            producerRoot.Get(),
            producerShader.Get());
        auto consumerPipeline = CreateComputePipeline(
            device.Get(),
            consumerRoot.Get(),
            consumerShader.Get());

        D3D12_INDIRECT_ARGUMENT_DESC indirectArgument{};
        indirectArgument.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;

        D3D12_COMMAND_SIGNATURE_DESC signatureDesc{};
        signatureDesc.ByteStride = artifact.ArgumentStrideBytes();
        signatureDesc.NumArgumentDescs = 1;
        signatureDesc.pArgumentDescs = &indirectArgument;
        signatureDesc.NodeMask = 0;

        ComPtr<ID3D12CommandSignature> commandSignature;
        ThrowIfFailed(
            device->CreateCommandSignature(
                &signatureDesc,
                nullptr,
                IID_PPV_ARGS(&commandSignature)),
            "CreateCommandSignature");

        const std::uint64_t transformBytes = 8ull * sizeof(Motor);
        const std::uint64_t pointBytes =
            static_cast<std::uint64_t>(artifact.MaxWorkCount()) * sizeof(Float4);
        const std::uint64_t activeCountBytes = sizeof(std::uint32_t);
        const std::uint64_t argumentBytes = artifact.ArgumentStrideBytes();
        const std::uint64_t outputBytes = pointBytes;

        auto transforms = CreateBuffer(
            device.Get(),
            D3D12_HEAP_TYPE_UPLOAD,
            transformBytes,
            D3D12_RESOURCE_STATE_GENERIC_READ);
        auto points = CreateBuffer(
            device.Get(),
            D3D12_HEAP_TYPE_UPLOAD,
            pointBytes,
            D3D12_RESOURCE_STATE_GENERIC_READ);
        auto activeCount = CreateBuffer(
            device.Get(),
            D3D12_HEAP_TYPE_UPLOAD,
            activeCountBytes,
            D3D12_RESOURCE_STATE_GENERIC_READ);
        auto sentinelUpload = CreateBuffer(
            device.Get(),
            D3D12_HEAP_TYPE_UPLOAD,
            outputBytes,
            D3D12_RESOURCE_STATE_GENERIC_READ);

        auto arguments = CreateBuffer(
            device.Get(),
            D3D12_HEAP_TYPE_DEFAULT,
            argumentBytes,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        auto output = CreateBuffer(
            device.Get(),
            D3D12_HEAP_TYPE_DEFAULT,
            outputBytes,
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        auto outputReadback = CreateBuffer(
            device.Get(),
            D3D12_HEAP_TYPE_READBACK,
            outputBytes,
            D3D12_RESOURCE_STATE_COPY_DEST);
        auto argumentReadback = CreateBuffer(
            device.Get(),
            D3D12_HEAP_TYPE_READBACK,
            argumentBytes,
            D3D12_RESOURCE_STATE_COPY_DEST);

        const auto transformValues = BuildTransforms();
        const auto pointValues = BuildPoints(artifact.MaxWorkCount());
        std::vector<Float4> sentinelValues(
            artifact.MaxWorkCount(),
            InactiveSentinel);

        void* mapped = nullptr;
        ThrowIfFailed(transforms->Map(0, nullptr, &mapped), "Map transforms");
        std::memcpy(mapped, transformValues.data(), transformBytes);
        transforms->Unmap(0, nullptr);

        ThrowIfFailed(points->Map(0, nullptr, &mapped), "Map points");
        std::memcpy(mapped, pointValues.data(), pointBytes);
        points->Unmap(0, nullptr);

        ThrowIfFailed(
            sentinelUpload->Map(0, nullptr, &mapped),
            "Map sentinel upload");
        std::memcpy(mapped, sentinelValues.data(), outputBytes);
        sentinelUpload->Unmap(0, nullptr);

        std::uint32_t* mappedActiveCount = nullptr;
        ThrowIfFailed(
            activeCount->Map(
                0,
                nullptr,
                reinterpret_cast<void**>(&mappedActiveCount)),
            "Map active count");

        D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.NumDescriptors = 6;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

        ComPtr<ID3D12DescriptorHeap> descriptorHeap;
        ThrowIfFailed(
            device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptorHeap)),
            "CreateDescriptorHeap");

        const UINT descriptorIncrement =
            device->GetDescriptorHandleIncrementSize(
                D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        const auto cpuStart =
            descriptorHeap->GetCPUDescriptorHandleForHeapStart();
        const auto gpuStart =
            descriptorHeap->GetGPUDescriptorHandleForHeapStart();

        auto createStructuredSrv =
            [&](UINT index, ID3D12Resource* resource, UINT elements, UINT stride)
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
            desc.Shader4ComponentMapping =
                D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            desc.Format = DXGI_FORMAT_UNKNOWN;
            desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            desc.Buffer.FirstElement = 0;
            desc.Buffer.NumElements = elements;
            desc.Buffer.StructureByteStride = stride;
            desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
            device->CreateShaderResourceView(
                resource,
                &desc,
                CpuHandle(cpuStart, descriptorIncrement, index));
        };

        auto createStructuredUav =
            [&](UINT index, ID3D12Resource* resource, UINT elements, UINT stride)
        {
            D3D12_UNORDERED_ACCESS_VIEW_DESC desc{};
            desc.Format = DXGI_FORMAT_UNKNOWN;
            desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
            desc.Buffer.FirstElement = 0;
            desc.Buffer.NumElements = elements;
            desc.Buffer.StructureByteStride = stride;
            desc.Buffer.CounterOffsetInBytes = 0;
            desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
            device->CreateUnorderedAccessView(
                resource,
                nullptr,
                &desc,
                CpuHandle(cpuStart, descriptorIncrement, index));
        };

        createStructuredSrv(0, activeCount.Get(), 1, sizeof(std::uint32_t));
        createStructuredUav(1, arguments.Get(), 1, sizeof(DispatchArguments));
        createStructuredSrv(2, transforms.Get(), 8, sizeof(Motor));
        createStructuredSrv(
            3,
            points.Get(),
            artifact.MaxWorkCount(),
            sizeof(Float4));
        createStructuredSrv(4, activeCount.Get(), 1, sizeof(std::uint32_t));
        createStructuredUav(
            5,
            output.Get(),
            artifact.MaxWorkCount(),
            sizeof(Float4));

        ComPtr<ID3D12Fence> fence;
        ThrowIfFailed(
            device->CreateFence(
                0,
                D3D12_FENCE_FLAG_NONE,
                IID_PPV_ARGS(&fence)),
            "CreateFence");

        EventHandle fenceEvent;
        fenceEvent.value = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!fenceEvent.value)
        {
            throw std::runtime_error("CreateEventW failed.");
        }

        UINT64 fenceValue = 0;
        SingleIndirectArchitectureResultV1 result;
        result.adapterDescription = Utf8FromWide(adapterDesc.Description);
        result.producerShaderBytecodeDigest =
            producerShaderBytecodeDigest;
        result.consumerShaderBytecodeDigest =
            consumerShaderBytecodeDigest;

        for (const std::uint32_t active : activeCounts)
        {
            *mappedActiveCount = active;

            ThrowIfFailed(allocator->Reset(), "CommandAllocator Reset");
            ThrowIfFailed(
                list->Reset(allocator.Get(), nullptr),
                "CommandList Reset");

            list->CopyBufferRegion(
                output.Get(),
                0,
                sentinelUpload.Get(),
                0,
                outputBytes);

            auto outputToUav = TransitionBarrier(
                output.Get(),
                D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            list->ResourceBarrier(1, &outputToUav);

            ID3D12DescriptorHeap* heaps[] = {descriptorHeap.Get()};
            list->SetDescriptorHeaps(1, heaps);

            list->SetPipelineState(producerPipeline.Get());
            list->SetComputeRootSignature(producerRoot.Get());
            list->SetComputeRootDescriptorTable(
                0,
                GpuHandle(gpuStart, descriptorIncrement, 0));
            list->SetComputeRootDescriptorTable(
                1,
                GpuHandle(gpuStart, descriptorIncrement, 1));
            list->Dispatch(
                artifact.ProducerDispatchX(),
                artifact.ProducerDispatchY(),
                artifact.ProducerDispatchZ());

            auto argumentUav = UavBarrier(arguments.Get());
            list->ResourceBarrier(1, &argumentUav);

            auto argumentToIndirect = TransitionBarrier(
                arguments.Get(),
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
            list->ResourceBarrier(1, &argumentToIndirect);

            list->SetPipelineState(consumerPipeline.Get());
            list->SetComputeRootSignature(consumerRoot.Get());
            list->SetComputeRootDescriptorTable(
                0,
                GpuHandle(gpuStart, descriptorIncrement, 2));
            list->SetComputeRootDescriptorTable(
                1,
                GpuHandle(gpuStart, descriptorIncrement, 5));

            list->ExecuteIndirect(
                commandSignature.Get(),
                artifact.MaxCommandCount(),
                arguments.Get(),
                artifact.ArgumentOffsetBytes(),
                nullptr,
                0);

            auto outputUav = UavBarrier(output.Get());
            list->ResourceBarrier(1, &outputUav);

            auto argumentToCopy = TransitionBarrier(
                arguments.Get(),
                D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT,
                D3D12_RESOURCE_STATE_COPY_SOURCE);
            auto outputToCopy = TransitionBarrier(
                output.Get(),
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                D3D12_RESOURCE_STATE_COPY_SOURCE);
            const D3D12_RESOURCE_BARRIER toCopy[] = {
                argumentToCopy,
                outputToCopy
            };
            list->ResourceBarrier(2, toCopy);

            list->CopyBufferRegion(
                argumentReadback.Get(),
                0,
                arguments.Get(),
                0,
                argumentBytes);
            list->CopyBufferRegion(
                outputReadback.Get(),
                0,
                output.Get(),
                0,
                outputBytes);

            auto argumentBackToUav = TransitionBarrier(
                arguments.Get(),
                D3D12_RESOURCE_STATE_COPY_SOURCE,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            auto outputBackToCopyDest = TransitionBarrier(
                output.Get(),
                D3D12_RESOURCE_STATE_COPY_SOURCE,
                D3D12_RESOURCE_STATE_COPY_DEST);
            const D3D12_RESOURCE_BARRIER restore[] = {
                argumentBackToUav,
                outputBackToCopyDest
            };
            list->ResourceBarrier(2, restore);

            ThrowIfFailed(list->Close(), "CommandList Close");

            ID3D12CommandList* lists[] = {list.Get()};
            queue->ExecuteCommandLists(1, lists);

            ++fenceValue;
            ThrowIfFailed(queue->Signal(fence.Get(), fenceValue), "Queue Signal");
            if (fence->GetCompletedValue() < fenceValue)
            {
                ThrowIfFailed(
                    fence->SetEventOnCompletion(fenceValue, fenceEvent.value),
                    "SetEventOnCompletion");
                WaitForSingleObject(fenceEvent.value, INFINITE);
            }

            Float4* mappedOutput = nullptr;
            DispatchArguments* mappedArguments = nullptr;
            D3D12_RANGE outputReadRange{0, static_cast<SIZE_T>(outputBytes)};
            D3D12_RANGE argumentReadRange{0, static_cast<SIZE_T>(argumentBytes)};
            ThrowIfFailed(
                outputReadback->Map(
                    0,
                    &outputReadRange,
                    reinterpret_cast<void**>(&mappedOutput)),
                "Map output readback");
            ThrowIfFailed(
                argumentReadback->Map(
                    0,
                    &argumentReadRange,
                    reinterpret_cast<void**>(&mappedArguments)),
                "Map argument readback");

            IndirectExecutionCaseResultV1 caseResult;
            caseResult.activeWorkCount = active;
            caseResult.observedDispatchGroupCountX =
                mappedArguments->threadGroupCountX;
            caseResult.expectedDispatchGroupCountX =
                semantic::DispatchGroupCountX(
                    semantic::ActiveWorkCountV1(
                        active,
                        semantic.maxWorkCount),
                    semantic.threadsPerGroup);
            caseResult.inactiveTailPreserved = true;

            if (mappedArguments->threadGroupCountY != 1 ||
                mappedArguments->threadGroupCountZ != 1)
            {
                caseResult.activeMismatchCount += 1;
                caseResult.firstActiveMismatch = 0;
            }

            for (std::uint32_t i = 0; i < active; ++i)
            {
                const Float4 expected = ExpectedPoint(pointValues[i], i);
                const Float4 actual = mappedOutput[i];
                const float errors[] = {
                    std::abs(actual.x - expected.x),
                    std::abs(actual.y - expected.y),
                    std::abs(actual.z - expected.z),
                    std::abs(actual.w - expected.w)
                };
                const float localMax =
                    *std::max_element(std::begin(errors), std::end(errors));
                caseResult.maxAbsoluteError =
                    std::max(caseResult.maxAbsoluteError, localMax);
                if (!std::isfinite(localMax) || localMax > 1.0e-5f)
                {
                    if (caseResult.firstActiveMismatch == 0xFFFF'FFFFu)
                    {
                        caseResult.firstActiveMismatch = i;
                    }
                    ++caseResult.activeMismatchCount;
                }
            }

            for (std::uint32_t i = active;
                 i < artifact.MaxWorkCount();
                 ++i)
            {
                if (!EqualSentinel(mappedOutput[i]))
                {
                    caseResult.inactiveTailPreserved = false;
                    break;
                }
            }

            caseResult.readbackDigest = base::Sha256(
                std::as_bytes(
                    std::span(
                        mappedOutput,
                        artifact.MaxWorkCount())));

            D3D12_RANGE noCpuWrites{0, 0};
            outputReadback->Unmap(0, &noCpuWrites);
            argumentReadback->Unmap(0, &noCpuWrites);

            if (caseResult.observedDispatchGroupCountX !=
                    caseResult.expectedDispatchGroupCountX ||
                caseResult.activeMismatchCount != 0 ||
                !caseResult.inactiveTailPreserved)
            {
                return base::Result<SingleIndirectArchitectureResultV1, IndirectExecutionErrorV1>::Failure(
                    Error(
                        "observation",
                        "WARP ExecuteIndirect observation contract failed."));
            }

            result.cases.push_back(caseResult);
        }

        activeCount->Unmap(0, nullptr);

        return base::Result<SingleIndirectArchitectureResultV1, IndirectExecutionErrorV1>::Success(
            std::move(result));
    }
    catch (const std::exception& error)
    {
        return base::Result<SingleIndirectArchitectureResultV1, IndirectExecutionErrorV1>::Failure(
            Error("d3d12/exception", error.what()));
    }
}
}
