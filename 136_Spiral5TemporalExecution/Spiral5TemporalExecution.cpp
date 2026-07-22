#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "Spiral5TemporalExecution.h"

#include "../00_Foundation/BinaryIO.h"

#if defined(_WIN32)
#include <Windows.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#endif

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

#if defined(_WIN32)
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")
#endif

namespace sge4_5::spiral5::execution
{
namespace
{
TemporalExecutionErrorV1 Error(std::string stage, std::string message, long hr = 0)
{
    return {std::move(stage), std::move(message), hr};
}

void WriteString(base::BinaryWriter& writer, std::string_view value)
{
    writer.WriteU32(static_cast<std::uint32_t>(value.size()));
    writer.WriteBytes(std::as_bytes(std::span(value.data(), value.size())));
}

void WriteDigest(base::BinaryWriter& writer, const base::Digest256& value)
{
    writer.WriteBytes(value);
}

void WriteFloat(base::BinaryWriter& writer, float value)
{
    writer.WriteU32(std::bit_cast<std::uint32_t>(value));
}

#if defined(_WIN32)
using Microsoft::WRL::ComPtr;

std::string HResultText(HRESULT hr)
{
    std::ostringstream stream;
    stream << "HRESULT 0x" << std::hex << std::uppercase << static_cast<unsigned long>(hr);
    return stream.str();
}

void Check(HRESULT hr, std::string_view stage)
{
    if (FAILED(hr))
        throw std::runtime_error(std::string(stage) + ": " + HResultText(hr));
}

std::string WideToUtf8(const wchar_t* text)
{
    if (!text) return {};
    const int count = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (count <= 1) return {};
    std::string value(static_cast<std::size_t>(count), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, value.data(), count, nullptr, nullptr);
    value.pop_back();
    return value;
}

struct EventHandle final
{
    HANDLE value = nullptr;
    EventHandle() : value(CreateEventW(nullptr, FALSE, FALSE, nullptr))
    {
        if (!value) throw std::runtime_error("CreateEventW failed.");
    }
    ~EventHandle() { if (value) CloseHandle(value); }
    EventHandle(const EventHandle&) = delete;
    EventHandle& operator=(const EventHandle&) = delete;
};

ComPtr<ID3DBlob> CompileShader(std::string_view source, std::string_view entry)
{
    ComPtr<ID3DBlob> shader;
    ComPtr<ID3DBlob> errors;
    const UINT flags = D3DCOMPILE_ENABLE_STRICTNESS |
                       D3DCOMPILE_WARNINGS_ARE_ERRORS |
                       D3DCOMPILE_OPTIMIZATION_LEVEL3;
    const HRESULT hr = D3DCompile(
        source.data(), source.size(), "Spiral5TemporalV1", nullptr, nullptr,
        std::string(entry).c_str(), "cs_5_1", flags, 0, &shader, &errors);
    if (FAILED(hr))
    {
        std::string message = errors
            ? std::string(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize())
            : HResultText(hr);
        throw std::runtime_error("D3DCompile: " + message);
    }
    return shader;
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
    const HRESULT serializedResult = D3D12SerializeRootSignature(
        &desc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors);
    if (FAILED(serializedResult))
    {
        const std::string message = errors
            ? std::string(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize())
            : HResultText(serializedResult);
        throw std::runtime_error("D3D12SerializeRootSignature: " + message);
    }
    ComPtr<ID3D12RootSignature> root;
    Check(device->CreateRootSignature(
        0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(&root)),
        "CreateRootSignature");
    return root;
}

ComPtr<ID3D12PipelineState> CreateComputePipeline(
    ID3D12Device* device,
    ID3D12RootSignature* root,
    ID3DBlob* shader)
{
    D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = root;
    desc.CS = {shader->GetBufferPointer(), shader->GetBufferSize()};
    ComPtr<ID3D12PipelineState> pipeline;
    Check(device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&pipeline)),
          "CreateComputePipelineState");
    return pipeline;
}

ComPtr<ID3D12Resource> CreateBuffer(
    ID3D12Device* device,
    D3D12_HEAP_TYPE heapType,
    std::uint64_t size,
    D3D12_RESOURCE_STATES state,
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE)
{
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = heapType;
    heap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heap.CreationNodeMask = 1;
    heap.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Alignment = 0;
    desc.Width = size;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = flags;

    ComPtr<ID3D12Resource> resource;
    Check(device->CreateCommittedResource(
        &heap, D3D12_HEAP_FLAG_NONE, &desc, state, nullptr, IID_PPV_ARGS(&resource)),
        "CreateCommittedResource");
    return resource;
}

void UploadBytes(ID3D12Resource* resource, std::span<const std::byte> bytes)
{
    void* mapped = nullptr;
    D3D12_RANGE noRead{0, 0};
    Check(resource->Map(0, &noRead, &mapped), "Map(upload)");
    std::memcpy(mapped, bytes.data(), bytes.size());
    resource->Unmap(0, nullptr);
}

D3D12_RESOURCE_BARRIER Transition(
    ID3D12Resource* resource,
    D3D12_RESOURCE_STATES before,
    D3D12_RESOURCE_STATES after)
{
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
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
    barrier.UAV.pResource = resource;
    return barrier;
}

std::vector<std::byte> ReadbackBytes(ID3D12Resource* resource, std::size_t size)
{
    void* mapped = nullptr;
    D3D12_RANGE readRange{0, size};
    Check(resource->Map(0, &readRange, &mapped), "Map(readback)");
    const auto* begin = static_cast<const std::byte*>(mapped);
    std::vector<std::byte> bytes(begin, begin + size);
    D3D12_RANGE noWrite{0, 0};
    resource->Unmap(0, &noWrite);
    return bytes;
}

std::string ArgumentProducerShader()
{
    return R"(
cbuffer Invocation : register(b0) { uint updateFlag; };
RWByteAddressBuffer Arguments : register(u0);
[numthreads(1,1,1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (any(dispatchThreadId != 0)) return;
    Arguments.Store(0, updateFlag);
    Arguments.Store(4, 1);
    Arguments.Store(8, 1);
}
)";
}

std::string HierarchyShader()
{
    return R"(
struct Motor { float4 qr; float4 qd; };
cbuffer Invocation : register(b0) { uint sourceGeneration; };
StructuredBuffer<Motor> LocalMotors : register(t0);
RWStructuredBuffer<Motor> GlobalHistory : register(u0);
float4 qmul(float4 a, float4 b)
{
    return float4(
        a.w*b.x + b.w*a.x + a.y*b.z - a.z*b.y,
        a.w*b.y + b.w*a.y + a.z*b.x - a.x*b.z,
        a.w*b.z + b.w*a.z + a.x*b.y - a.y*b.x,
        a.w*b.w - dot(a.xyz,b.xyz));
}
Motor composeMotor(Motor parent, Motor local)
{
    Motor output;
    output.qr = qmul(parent.qr, local.qr);
    output.qd = qmul(parent.qr, local.qd) + qmul(parent.qd, local.qr);
    return output;
}
[numthreads(1,1,1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (any(dispatchThreadId != 0)) return;
    Motor parent;
    parent.qr = float4(0,0,0,1);
    parent.qd = float4(0,0,0,0);
    uint baseIndex = sourceGeneration * 8;
    [unroll] for (uint bone = 0; bone < 8; ++bone)
    {
        Motor global = composeMotor(parent, LocalMotors[baseIndex + bone]);
        GlobalHistory[bone] = global;
        parent = global;
    }
}
)";
}

std::string ConsumerShader()
{
    return R"(
struct Motor { float4 qr; float4 qd; };
StructuredBuffer<Motor> GlobalHistory : register(t0);
StructuredBuffer<float4> Points : register(t1);
RWStructuredBuffer<float4> Output : register(u0);
float4 qmul(float4 a, float4 b)
{
    return float4(
        a.w*b.x + b.w*a.x + a.y*b.z - a.z*b.y,
        a.w*b.y + b.w*a.y + a.z*b.x - a.x*b.z,
        a.w*b.z + b.w*a.z + a.x*b.y - a.y*b.x,
        a.w*b.w - dot(a.xyz,b.xyz));
}
float4 qconj(float4 q) { return float4(-q.xyz, q.w); }
[numthreads(64,1,1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint index = dispatchThreadId.x;
    if (index >= 4096) return;
    uint bone = index / 512;
    Motor motor = GlobalHistory[bone];
    float4 inputPoint = Points[index];
    float4 conjugate = qconj(motor.qr);
    float4 rotated = qmul(qmul(motor.qr, float4(inputPoint.xyz,0)), conjugate);
    float3 translation = 2.0f * qmul(motor.qd, conjugate).xyz;
    Output[index] = float4(rotated.xyz + translation, 1.0f);
}
)";
}

struct GpuContext final
{
    ComPtr<IDXGIFactory6> factory;
    ComPtr<IDXGIAdapter> adapter;
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> commandList;
    ComPtr<ID3D12Fence> fence;
    EventHandle fenceEvent;
    std::uint64_t fenceValue = 0;
    std::string adapterDescription;
    LUID adapterLuid{};

    ComPtr<ID3DBlob> argumentShader;
    ComPtr<ID3DBlob> hierarchyShader;
    ComPtr<ID3DBlob> consumerShader;
    ComPtr<ID3D12RootSignature> argumentRoot;
    ComPtr<ID3D12RootSignature> hierarchyRoot;
    ComPtr<ID3D12RootSignature> consumerRoot;
    ComPtr<ID3D12PipelineState> argumentPipeline;
    ComPtr<ID3D12PipelineState> hierarchyPipeline;
    ComPtr<ID3D12PipelineState> consumerPipeline;
    ComPtr<ID3D12CommandSignature> dispatchSignature;

    GpuContext()
    {
        Check(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)), "CreateDXGIFactory2");
        Check(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)), "EnumWarpAdapter");
        DXGI_ADAPTER_DESC adapterDesc{};
        Check(adapter->GetDesc(&adapterDesc), "GetDesc");
        adapterDescription = WideToUtf8(adapterDesc.Description);
        adapterLuid = adapterDesc.AdapterLuid;
        Check(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)),
              "D3D12CreateDevice(WARP)");

        D3D12_COMMAND_QUEUE_DESC queueDesc{};
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
        Check(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue)), "CreateCommandQueue");
        Check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&allocator)),
              "CreateCommandAllocator");
        Check(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, allocator.Get(), nullptr,
                                        IID_PPV_ARGS(&commandList)),
              "CreateCommandList");
        Check(commandList->Close(), "Close(initial command list)");
        Check(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)), "CreateFence");

        argumentShader = CompileShader(ArgumentProducerShader(), "main");
        hierarchyShader = CompileShader(HierarchyShader(), "main");
        consumerShader = CompileShader(ConsumerShader(), "main");

        std::array<D3D12_ROOT_PARAMETER, 2> argumentParameters{};
        argumentParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        argumentParameters[0].Constants.ShaderRegister = 0;
        argumentParameters[0].Constants.RegisterSpace = 0;
        argumentParameters[0].Constants.Num32BitValues = 1;
        argumentParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        argumentParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
        argumentParameters[1].Descriptor.ShaderRegister = 0;
        argumentParameters[1].Descriptor.RegisterSpace = 0;
        argumentParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        argumentRoot = CreateRootSignature(device.Get(), argumentParameters);

        std::array<D3D12_ROOT_PARAMETER, 3> hierarchyParameters{};
        hierarchyParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        hierarchyParameters[0].Constants.ShaderRegister = 0;
        hierarchyParameters[0].Constants.RegisterSpace = 0;
        hierarchyParameters[0].Constants.Num32BitValues = 1;
        hierarchyParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        hierarchyParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        hierarchyParameters[1].Descriptor.ShaderRegister = 0;
        hierarchyParameters[1].Descriptor.RegisterSpace = 0;
        hierarchyParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        hierarchyParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
        hierarchyParameters[2].Descriptor.ShaderRegister = 0;
        hierarchyParameters[2].Descriptor.RegisterSpace = 0;
        hierarchyParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        hierarchyRoot = CreateRootSignature(device.Get(), hierarchyParameters);

        std::array<D3D12_ROOT_PARAMETER, 3> consumerParameters{};
        consumerParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        consumerParameters[0].Descriptor.ShaderRegister = 0;
        consumerParameters[0].Descriptor.RegisterSpace = 0;
        consumerParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        consumerParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        consumerParameters[1].Descriptor.ShaderRegister = 1;
        consumerParameters[1].Descriptor.RegisterSpace = 0;
        consumerParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        consumerParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
        consumerParameters[2].Descriptor.ShaderRegister = 0;
        consumerParameters[2].Descriptor.RegisterSpace = 0;
        consumerParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        consumerRoot = CreateRootSignature(device.Get(), consumerParameters);

        argumentPipeline = CreateComputePipeline(device.Get(), argumentRoot.Get(), argumentShader.Get());
        hierarchyPipeline = CreateComputePipeline(device.Get(), hierarchyRoot.Get(), hierarchyShader.Get());
        consumerPipeline = CreateComputePipeline(device.Get(), consumerRoot.Get(), consumerShader.Get());

        D3D12_INDIRECT_ARGUMENT_DESC indirectArgument{};
        indirectArgument.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
        D3D12_COMMAND_SIGNATURE_DESC signatureDesc{};
        signatureDesc.ByteStride = 12;
        signatureDesc.NumArgumentDescs = 1;
        signatureDesc.pArgumentDescs = &indirectArgument;
        signatureDesc.NodeMask = 0;
        Check(device->CreateCommandSignature(&signatureDesc, nullptr, IID_PPV_ARGS(&dispatchSignature)),
              "CreateCommandSignature");
    }

    void Begin()
    {
        Check(allocator->Reset(), "CommandAllocator::Reset");
        Check(commandList->Reset(allocator.Get(), nullptr), "CommandList::Reset");
    }

    std::uint64_t SubmitAndWait()
    {
        Check(commandList->Close(), "CommandList::Close");
        ID3D12CommandList* lists[] = {commandList.Get()};
        queue->ExecuteCommandLists(1, lists);
        ++fenceValue;
        Check(queue->Signal(fence.Get(), fenceValue), "CommandQueue::Signal");
        if (fence->GetCompletedValue() < fenceValue)
        {
            Check(fence->SetEventOnCompletion(fenceValue, fenceEvent.value), "Fence::SetEventOnCompletion");
            WaitForSingleObject(fenceEvent.value, INFINITE);
        }
        return fenceValue;
    }
};

base::Digest256 BlobDigest(ID3DBlob* blob)
{
    const auto* data = static_cast<const std::byte*>(blob->GetBufferPointer());
    return base::Sha256(std::span(data, blob->GetBufferSize()));
}

TemporalScheduleObservationV1 ExecuteSchedule(
    GpuContext& gpu,
    const semantic::TemporalStateSemanticV1& semanticValue,
    const TemporalScheduleExecutionInputV1& input)
{
    auto artifactValid = contract::ValidateGlobalMotorHistoryArtifactForExecutionV1(
        input.artifact, semanticValue, input.schedule);
    if (!artifactValid)
        throw std::runtime_error(artifactValid.Error().stage + ": " + artifactValid.Error().message);

    const auto invocationArtifacts = contract::BuildTemporalInvocationArtifactsV1(
        input.artifact, input.schedule);
    const std::uint32_t maxGeneration = input.schedule.invocations.back().sourceGeneration;

    std::vector<semantic::PgaMotorRecordV1> allLocalMotors;
    allLocalMotors.reserve(static_cast<std::size_t>(maxGeneration + 1u) * semantic::BoneCountV1);
    for (std::uint32_t generation = 0; generation <= maxGeneration; ++generation)
    {
        const auto motors = semantic::BuildCanonicalLocalMotorGenerationV1(generation);
        allLocalMotors.insert(allLocalMotors.end(), motors.begin(), motors.end());
    }
    const auto localBytes = std::as_bytes(std::span(allLocalMotors));
    const auto points = semantic::BuildCanonicalPointCorpusV1();
    const auto pointBytes = std::as_bytes(std::span(points));

    constexpr std::uint64_t argumentBytes = 12;
    constexpr std::uint64_t historyBytes = semantic::BoneCountV1 * sizeof(semantic::PgaMotorRecordV1);
    constexpr std::uint64_t outputBytes = semantic::WorkCountV1 * sizeof(semantic::PointRecordV1);

    auto localUpload = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_UPLOAD, localBytes.size(),
                                    D3D12_RESOURCE_STATE_GENERIC_READ);
    auto pointUpload = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_UPLOAD, pointBytes.size(),
                                    D3D12_RESOURCE_STATE_GENERIC_READ);
    UploadBytes(localUpload.Get(), localBytes);
    UploadBytes(pointUpload.Get(), pointBytes);

    auto arguments = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_DEFAULT, argumentBytes,
                                  D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                  D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    auto history = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_DEFAULT, historyBytes,
                                D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    auto output = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_DEFAULT, outputBytes,
                               D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                               D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    auto argumentReadback = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_READBACK, argumentBytes,
                                         D3D12_RESOURCE_STATE_COPY_DEST);
    auto historyReadback = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_READBACK, historyBytes,
                                        D3D12_RESOURCE_STATE_COPY_DEST);
    auto outputReadback = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_READBACK, outputBytes,
                                       D3D12_RESOURCE_STATE_COPY_DEST);

    TemporalScheduleObservationV1 scheduleResult;
    scheduleResult.scheduleId = input.schedule.id;
    scheduleResult.scheduleIdentity = semantic::UpdateScheduleIdentityV1(input.schedule);
    scheduleResult.artifactIdentity = input.artifact.ArtifactIdentity();
    scheduleResult.invocations.reserve(invocationArtifacts.size());

    const std::uint64_t scheduleFenceBase = gpu.fenceValue;
    base::Digest256 previousHistoryDigest{};
    base::Digest256 latestUpdateOutputDigest{};
    bool historyValid = false;
    D3D12_RESOURCE_STATES historyState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

    std::map<std::uint32_t, std::array<semantic::PgaMotorRecordV1, semantic::BoneCountV1>> cpuGlobalCache;
    std::map<std::uint32_t, std::vector<semantic::PointRecordV1>> cpuOutputCache;

    for (std::size_t invocationIndex = 0; invocationIndex < invocationArtifacts.size(); ++invocationIndex)
    {
        const auto& invocation = invocationArtifacts[invocationIndex];
        const bool update = invocation.event == semantic::UpdateEventV1::Update;

        gpu.Begin();
        if (historyState != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
        {
            const auto barrier = Transition(history.Get(), historyState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            gpu.commandList->ResourceBarrier(1, &barrier);
            historyState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        }

        gpu.commandList->SetPipelineState(gpu.argumentPipeline.Get());
        gpu.commandList->SetComputeRootSignature(gpu.argumentRoot.Get());
        gpu.commandList->SetComputeRoot32BitConstant(0, update ? 1u : 0u, 0);
        gpu.commandList->SetComputeRootUnorderedAccessView(1, arguments->GetGPUVirtualAddress());
        gpu.commandList->Dispatch(1, 1, 1);
        auto argumentUav = UavBarrier(arguments.Get());
        gpu.commandList->ResourceBarrier(1, &argumentUav);
        auto argumentToIndirect = Transition(arguments.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                             D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
        gpu.commandList->ResourceBarrier(1, &argumentToIndirect);

        gpu.commandList->SetPipelineState(gpu.hierarchyPipeline.Get());
        gpu.commandList->SetComputeRootSignature(gpu.hierarchyRoot.Get());
        gpu.commandList->SetComputeRoot32BitConstant(0, invocation.sourceGeneration, 0);
        gpu.commandList->SetComputeRootShaderResourceView(1, localUpload->GetGPUVirtualAddress());
        gpu.commandList->SetComputeRootUnorderedAccessView(2, history->GetGPUVirtualAddress());
        gpu.commandList->ExecuteIndirect(gpu.dispatchSignature.Get(), 1, arguments.Get(), 0, nullptr, 0);
        auto historyUav = UavBarrier(history.Get());
        gpu.commandList->ResourceBarrier(1, &historyUav);
        auto historyToRead = Transition(history.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        gpu.commandList->ResourceBarrier(1, &historyToRead);
        historyState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

        gpu.commandList->SetPipelineState(gpu.consumerPipeline.Get());
        gpu.commandList->SetComputeRootSignature(gpu.consumerRoot.Get());
        gpu.commandList->SetComputeRootShaderResourceView(0, history->GetGPUVirtualAddress());
        gpu.commandList->SetComputeRootShaderResourceView(1, pointUpload->GetGPUVirtualAddress());
        gpu.commandList->SetComputeRootUnorderedAccessView(2, output->GetGPUVirtualAddress());
        gpu.commandList->Dispatch(invocation.consumerDispatchX, 1, 1);
        auto outputUav = UavBarrier(output.Get());
        gpu.commandList->ResourceBarrier(1, &outputUav);

        auto outputToCopy = Transition(output.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                       D3D12_RESOURCE_STATE_COPY_SOURCE);
        gpu.commandList->ResourceBarrier(1, &outputToCopy);
        gpu.commandList->CopyBufferRegion(outputReadback.Get(), 0, output.Get(), 0, outputBytes);
        auto outputToUav = Transition(output.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE,
                                      D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        gpu.commandList->ResourceBarrier(1, &outputToUav);

        auto historyToCopy = Transition(history.Get(), historyState, D3D12_RESOURCE_STATE_COPY_SOURCE);
        gpu.commandList->ResourceBarrier(1, &historyToCopy);
        gpu.commandList->CopyBufferRegion(historyReadback.Get(), 0, history.Get(), 0, historyBytes);
        auto historyBack = Transition(history.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE,
                                      D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        gpu.commandList->ResourceBarrier(1, &historyBack);
        historyState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

        auto argumentToCopy = Transition(arguments.Get(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT,
                                         D3D12_RESOURCE_STATE_COPY_SOURCE);
        gpu.commandList->ResourceBarrier(1, &argumentToCopy);
        gpu.commandList->CopyBufferRegion(argumentReadback.Get(), 0, arguments.Get(), 0, argumentBytes);
        auto argumentBack = Transition(arguments.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE,
                                       D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        gpu.commandList->ResourceBarrier(1, &argumentBack);

        const auto fenceValue = gpu.SubmitAndWait();
        const auto argumentObserved = ReadbackBytes(argumentReadback.Get(), argumentBytes);
        const auto historyObserved = ReadbackBytes(historyReadback.Get(), historyBytes);
        const auto outputObserved = ReadbackBytes(outputReadback.Get(), outputBytes);

        std::uint32_t observedDispatchX = 0;
        std::memcpy(&observedDispatchX, argumentObserved.data(), sizeof(observedDispatchX));
        if (observedDispatchX != invocation.hierarchyDispatchX)
            throw std::runtime_error("GPU-generated hierarchy Dispatch argument differs from the schedule artifact.");

        if (!cpuGlobalCache.contains(invocation.sourceGeneration))
        {
            const auto local = semantic::BuildCanonicalLocalMotorGenerationV1(invocation.sourceGeneration);
            cpuGlobalCache.emplace(
                invocation.sourceGeneration,
                semantic::ComposeCanonicalGlobalMotorsCpuV1(local));
            cpuOutputCache.emplace(
                invocation.sourceGeneration,
                semantic::ApplyGlobalMotorsCpuV1(cpuGlobalCache.at(invocation.sourceGeneration), points));
        }
        const auto& expectedGlobal = cpuGlobalCache.at(invocation.sourceGeneration);
        const auto& expectedOutput = cpuOutputCache.at(invocation.sourceGeneration);

        const auto historyDigest = base::Sha256(historyObserved);
        const auto outputDigest = base::Sha256(outputObserved);
        const bool holdHistoryStable = update || (historyValid && historyDigest == previousHistoryDigest);

        TemporalInvocationObservationV1 observation;
        observation.invocationIndex = invocation.invocationIndex;
        observation.event = invocation.event;
        observation.sourceGeneration = invocation.sourceGeneration;
        observation.expectedHierarchyDispatchX = invocation.hierarchyDispatchX;
        observation.observedHierarchyDispatchX = observedDispatchX;
        observation.consumerDispatchX = invocation.consumerDispatchX;
        observation.historyReadGeneration = invocation.historyReadGeneration;
        observation.historyWriteGeneration = invocation.historyWriteGeneration;
        observation.retainedCompletionOrdinal = invocation.retainedCompletionOrdinal;
        observation.nativeFenceValue = fenceValue;
        observation.historyValid = update || historyValid;
        observation.holdHistoryByteStable = holdHistoryStable;
        observation.historyDigest = historyDigest;
        observation.outputDigest = outputDigest;

        if (historyObserved.size() != sizeof(expectedGlobal))
            throw std::runtime_error("Global Motor history readback has an invalid byte size.");
        std::array<semantic::PgaMotorRecordV1, semantic::BoneCountV1> actualHistory{};
        std::memcpy(actualHistory.data(), historyObserved.data(), historyObserved.size());
        for (std::uint32_t bone = 0; bone < semantic::BoneCountV1; ++bone)
        {
            const float* actual = reinterpret_cast<const float*>(&actualHistory[bone]);
            const float* expected = reinterpret_cast<const float*>(&expectedGlobal[bone]);
            for (std::size_t component = 0; component < 8; ++component)
            {
                const float error = std::abs(actual[component] - expected[component]);
                if (!std::isfinite(actual[component]) || error > 1.0e-6f)
                    throw std::runtime_error("Global Motor history differs from the CPU hierarchy reference.");
            }
        }

        std::vector<semantic::PointRecordV1> actualOutput(semantic::WorkCountV1);
        std::memcpy(actualOutput.data(), outputObserved.data(), outputObserved.size());
        for (std::uint32_t index = 0; index < semantic::WorkCountV1; ++index)
        {
            const auto& actual = actualOutput[index];
            const auto& expected = expectedOutput[index];
            const std::array<float, 4> a{actual.x, actual.y, actual.z, actual.w};
            const std::array<float, 4> e{expected.x, expected.y, expected.z, expected.w};
            float localMax = 0.0f;
            bool finite = true;
            for (std::size_t component = 0; component < 4; ++component)
            {
                finite = finite && std::isfinite(a[component]);
                localMax = std::max(localMax, std::abs(a[component] - e[component]));
            }
            observation.maxAbsoluteError = std::max(observation.maxAbsoluteError, localMax);
            if (!finite) ++observation.nonFiniteCount;
            if (!std::isfinite(actual.w) || std::abs(actual.w - 1.0f) > 1.0e-6f)
                ++observation.homogeneousCoordinateMismatchCount;
            if (!finite || localMax > 1.0e-5f)
                ++observation.mismatchCount;
        }
        observation.outputMatchesReference =
            observation.mismatchCount == 0 && observation.nonFiniteCount == 0 &&
            observation.homogeneousCoordinateMismatchCount == 0;
        if (update)
            latestUpdateOutputDigest = outputDigest;
        observation.outputMatchesLatestUpdate = outputDigest == latestUpdateOutputDigest;

        if (!observation.historyValid || !observation.holdHistoryByteStable ||
            !observation.outputMatchesReference || !observation.outputMatchesLatestUpdate ||
            observation.nativeFenceValue != scheduleFenceBase + observation.retainedCompletionOrdinal)
        {
            throw std::runtime_error("Temporal history observation violates the CU2 contract.");
        }

        historyValid = true;
        previousHistoryDigest = historyDigest;
        scheduleResult.invocations.push_back(observation);
    }
    return scheduleResult;
}
#endif
}

base::Result<GlobalMotorHistoryArchitectureResultV1, TemporalExecutionErrorV1>
RunGlobalMotorHistoryArchitecturesOnWarpV1(
    const semantic::TemporalStateSemanticV1& semanticValue,
    std::span<const TemporalScheduleExecutionInputV1> inputs)
{
#if defined(_WIN32)
    try
    {
        if (inputs.empty())
            return base::Result<GlobalMotorHistoryArchitectureResultV1, TemporalExecutionErrorV1>::Failure(
                Error("execution/input", "At least one schedule is required."));
        GpuContext gpu;
        GlobalMotorHistoryArchitectureResultV1 result;
        result.adapterDescription = gpu.adapterDescription;
        result.semanticIdentity = semantic::TemporalStateSemanticIdentityV1(semanticValue);
        result.argumentProducerShaderDigest = BlobDigest(gpu.argumentShader.Get());
        result.hierarchyShaderDigest = BlobDigest(gpu.hierarchyShader.Get());
        result.consumerShaderDigest = BlobDigest(gpu.consumerShader.Get());
        result.schedules.reserve(inputs.size());
        for (const auto& input : inputs)
            result.schedules.push_back(ExecuteSchedule(gpu, semanticValue, input));
        return base::Result<GlobalMotorHistoryArchitectureResultV1, TemporalExecutionErrorV1>::Success(
            std::move(result));
    }
    catch (const std::exception& error)
    {
        return base::Result<GlobalMotorHistoryArchitectureResultV1, TemporalExecutionErrorV1>::Failure(
            Error("execution/d3d12", error.what()));
    }
#else
    (void)semanticValue;
    (void)inputs;
    return base::Result<GlobalMotorHistoryArchitectureResultV1, TemporalExecutionErrorV1>::Failure(
        Error("execution/platform", "Spiral 5 CU2 D3D12 execution requires Windows."));
#endif
}

std::vector<std::byte> SerializeGlobalMotorHistoryArchitectureEvidenceV1(
    const GlobalMotorHistoryArchitectureResultV1& result)
{
    base::BinaryWriter writer;
    writer.WriteU32(0x31453553u); // "S5E1"
    writer.WriteU32(1);
    WriteString(writer, result.adapterDescription);
    WriteDigest(writer, result.semanticIdentity);
    WriteDigest(writer, result.argumentProducerShaderDigest);
    WriteDigest(writer, result.hierarchyShaderDigest);
    WriteDigest(writer, result.consumerShaderDigest);
    writer.WriteU32(static_cast<std::uint32_t>(result.schedules.size()));
    for (const auto& schedule : result.schedules)
    {
        WriteString(writer, schedule.scheduleId);
        WriteDigest(writer, schedule.scheduleIdentity);
        WriteDigest(writer, schedule.artifactIdentity);
        writer.WriteU32(static_cast<std::uint32_t>(schedule.invocations.size()));
        for (const auto& invocation : schedule.invocations)
        {
            writer.WriteU32(invocation.invocationIndex);
            writer.WriteU32(static_cast<std::uint32_t>(invocation.event));
            writer.WriteU32(invocation.sourceGeneration);
            writer.WriteU32(invocation.expectedHierarchyDispatchX);
            writer.WriteU32(invocation.observedHierarchyDispatchX);
            writer.WriteU32(invocation.consumerDispatchX);
            writer.WriteU32(invocation.historyReadGeneration);
            writer.WriteU32(invocation.historyWriteGeneration);
            writer.WriteU64(invocation.retainedCompletionOrdinal);
            writer.WriteU64(invocation.nativeFenceValue);
            writer.WriteU32(invocation.historyValid ? 1u : 0u);
            writer.WriteU32(invocation.holdHistoryByteStable ? 1u : 0u);
            writer.WriteU32(invocation.outputMatchesReference ? 1u : 0u);
            writer.WriteU32(invocation.outputMatchesLatestUpdate ? 1u : 0u);
            writer.WriteU32(invocation.mismatchCount);
            writer.WriteU32(invocation.nonFiniteCount);
            writer.WriteU32(invocation.homogeneousCoordinateMismatchCount);
            WriteFloat(writer, invocation.maxAbsoluteError);
            WriteDigest(writer, invocation.historyDigest);
            WriteDigest(writer, invocation.outputDigest);
        }
    }
    return std::move(writer).Take();
}

namespace
{
base::Digest256 VerifiedLiteralIdentity(std::string_view value)
{
    return base::Sha256(std::as_bytes(std::span(value.data(), value.size())));
}

base::Digest256 ExpectedVerifiedHistoryResourceIdentity(
    const contract::FrozenGlobalMotorHistoryArtifactV1& artifact)
{
    base::BinaryWriter writer;
    writer.WriteBytes(VerifiedLiteralIdentity(
        "SGE4-5.Spiral5.ActualHistoryResource.GlobalMotor.V1"));
    writer.WriteBytes(artifact.ArtifactIdentity());
    writer.WriteBytes(artifact.HierarchyProgramIdentity());
    writer.WriteBytes(artifact.ConsumerProgramIdentity());
    writer.WriteU32(static_cast<std::uint32_t>(artifact.HistoryRole()));
    writer.WriteU32(artifact.HistoryDepth());
    writer.WriteU32(artifact.HistoryBytes());
    return base::Sha256(writer.Bytes());
}

base::Digest256 BuildVerifiedTemporalBindingIdentity(
    const verification::VerifiedTemporalLoweringV1& verified,
    const contract::FrozenGlobalMotorHistoryArtifactV1& artifact,
    const TemporalHistoryResourceBindingInputV1& binding)
{
    base::BinaryWriter writer;
    writer.WriteBytes(VerifiedLiteralIdentity(
        "SGE4-5.Spiral5.FrozenVerifiedGlobalMotorHistoryBinding.V1"));
    writer.WriteBytes(verified.Plan().planIdentity);
    writer.WriteBytes(verified.CertificateIdentity());
    writer.WriteBytes(artifact.ArtifactIdentity());
    writer.WriteU64(binding.deviceEpoch);
    writer.WriteBytes(binding.actualHistoryResourceIdentity);
    writer.WriteU32(static_cast<std::uint32_t>(binding.historyRole));
    writer.WriteU32(binding.historyDepth);
    writer.WriteU32(binding.historyBytes);
    return base::Sha256(writer.Bytes());
}
}

FrozenVerifiedGlobalMotorHistoryExecutionV1::
FrozenVerifiedGlobalMotorHistoryExecutionV1(
    verification::VerifiedTemporalLoweringV1 verified,
    contract::FrozenGlobalMotorHistoryArtifactV1 artifact,
    TemporalHistoryResourceBindingInputV1 resourceBinding,
    base::Digest256 bindingIdentity)
    : verified_(std::move(verified)),
      artifact_(std::move(artifact)),
      resourceBinding_(resourceBinding),
      bindingIdentity_(bindingIdentity)
{
}

TemporalHistoryResourceBindingInputV1
BuildCanonicalGlobalMotorHistoryResourceBindingInputV1(
    const contract::FrozenGlobalMotorHistoryArtifactV1& artifact,
    std::uint64_t deviceEpoch)
{
    TemporalHistoryResourceBindingInputV1 binding;
    binding.deviceEpoch = deviceEpoch;
    binding.actualHistoryResourceIdentity =
        ExpectedVerifiedHistoryResourceIdentity(artifact);
    binding.historyRole = artifact.HistoryRole();
    binding.historyDepth = artifact.HistoryDepth();
    binding.historyBytes = artifact.HistoryBytes();
    return binding;
}

base::Result<void, std::string>
ValidateFrozenVerifiedGlobalMotorHistoryExecutionV1(
    const FrozenVerifiedGlobalMotorHistoryExecutionV1& frozen,
    const semantic::TemporalStateSemanticV1& semanticValue,
    const semantic::UpdateScheduleV1& schedule,
    const verification::TemporalVerificationContextV1& context,
    std::uint64_t expectedDeviceEpoch)
{
    auto verified = verification::ValidateVerifiedTemporalLoweringContextV1(
        frozen.Verified(), semanticValue, schedule, context);
    if (!verified) return verified;

    auto artifact = contract::ValidateGlobalMotorHistoryArtifactForExecutionV1(
        frozen.Artifact(), semanticValue, schedule);
    if (!artifact)
        return base::Result<void, std::string>::Failure(
            artifact.Error().stage + ": " + artifact.Error().message);

    const auto& operation = frozen.Verified().Plan().operation;
    const auto& binding = frozen.ResourceBinding();
    if (operation.artifactIdentity != frozen.Artifact().ArtifactIdentity() ||
        operation.historyResourceIdentity != binding.actualHistoryResourceIdentity ||
        operation.historyRole != binding.historyRole ||
        operation.historyDepth != binding.historyDepth ||
        operation.historyBytes != binding.historyBytes ||
        operation.spiral4IndirectArtifactIdentity !=
            frozen.Artifact().Spiral4IndirectArtifactIdentity() ||
        operation.argumentProducerProgramIdentity !=
            frozen.Artifact().ArgumentProducerProgramIdentity() ||
        operation.hierarchyProgramIdentity !=
            frozen.Artifact().HierarchyProgramIdentity() ||
        operation.consumerProgramIdentity !=
            frozen.Artifact().ConsumerProgramIdentity())
    {
        return base::Result<void, std::string>::Failure(
            "verified temporal plan is not bound to the actual artifact and History Resource");
    }

    if (expectedDeviceEpoch == 0 || binding.deviceEpoch != expectedDeviceEpoch)
        return base::Result<void, std::string>::Failure(
            "temporal History binding belongs to another device epoch");
    if (binding.actualHistoryResourceIdentity !=
            ExpectedVerifiedHistoryResourceIdentity(frozen.Artifact()) ||
        binding.historyRole != contract::TemporalHistoryRoleV1::GlobalMotorHistory ||
        binding.historyDepth != 1 ||
        binding.historyBytes != semantic::BoneCountV1 * sizeof(semantic::PgaMotorRecordV1))
    {
        return base::Result<void, std::string>::Failure(
            "actual Temporal History Resource binding is not canonical");
    }

    if (frozen.BindingIdentity() != BuildVerifiedTemporalBindingIdentity(
            frozen.Verified(), frozen.Artifact(), binding))
        return base::Result<void, std::string>::Failure(
            "frozen verified Temporal binding identity mismatch");
    return base::Result<void, std::string>::Success();
}

base::Result<FrozenVerifiedGlobalMotorHistoryExecutionV1, std::string>
FreezeVerifiedGlobalMotorHistoryExecutionV1(
    const semantic::TemporalStateSemanticV1& semanticValue,
    const semantic::UpdateScheduleV1& schedule,
    const verification::TemporalVerificationContextV1& context,
    const verification::VerifiedTemporalLoweringV1& verified,
    TemporalHistoryResourceBindingInputV1 resourceBinding)
{
    auto verifiedContext = verification::ValidateVerifiedTemporalLoweringContextV1(
        verified, semanticValue, schedule, context);
    if (!verifiedContext)
        return base::Result<FrozenVerifiedGlobalMotorHistoryExecutionV1, std::string>::Failure(
            verifiedContext.Error());

    auto artifact = contract::BuildCu2GlobalMotorHistoryArtifactV1(
        semanticValue, schedule);
    const auto bindingIdentity = BuildVerifiedTemporalBindingIdentity(
        verified, artifact, resourceBinding);
    FrozenVerifiedGlobalMotorHistoryExecutionV1 frozen(
        verified, std::move(artifact), resourceBinding, bindingIdentity);
    auto validation = ValidateFrozenVerifiedGlobalMotorHistoryExecutionV1(
        frozen, semanticValue, schedule, context, resourceBinding.deviceEpoch);
    if (!validation)
        return base::Result<FrozenVerifiedGlobalMotorHistoryExecutionV1, std::string>::Failure(
            validation.Error());
    return base::Result<FrozenVerifiedGlobalMotorHistoryExecutionV1, std::string>::Success(
        std::move(frozen));
}

base::Result<VerifiedGlobalMotorHistoryRunResultV1, TemporalExecutionErrorV1>
RunVerifiedGlobalMotorHistoryOnWarpV1(
    const FrozenVerifiedGlobalMotorHistoryExecutionV1& frozen,
    const semantic::TemporalStateSemanticV1& semanticValue,
    const semantic::UpdateScheduleV1& schedule,
    const verification::TemporalVerificationContextV1& context,
    std::uint64_t expectedDeviceEpoch)
{
    auto validation = ValidateFrozenVerifiedGlobalMotorHistoryExecutionV1(
        frozen, semanticValue, schedule, context, expectedDeviceEpoch);
    if (!validation)
        return base::Result<VerifiedGlobalMotorHistoryRunResultV1, TemporalExecutionErrorV1>::Failure(
            Error("authority/context", validation.Error()));

    std::vector<TemporalScheduleExecutionInputV1> inputs;
    inputs.push_back({schedule, frozen.Artifact()});
    auto run = RunGlobalMotorHistoryArchitecturesOnWarpV1(semanticValue, inputs);
    if (!run)
        return base::Result<VerifiedGlobalMotorHistoryRunResultV1, TemporalExecutionErrorV1>::Failure(
            run.Error());

    VerifiedGlobalMotorHistoryRunResultV1 result;
    result.architecture = std::move(run).Value();
    result.verifiedBindingIdentity = frozen.BindingIdentity();
    base::BinaryWriter writer;
    writer.WriteBytes(VerifiedLiteralIdentity(
        "SGE4-5.Spiral5.ActualVerifiedGlobalMotorHistoryExecution.V1"));
    writer.WriteBytes(result.verifiedBindingIdentity);
    writer.WriteBytes(result.architecture.argumentProducerShaderDigest);
    writer.WriteBytes(result.architecture.hierarchyShaderDigest);
    writer.WriteBytes(result.architecture.consumerShaderDigest);
    const auto evidence = SerializeGlobalMotorHistoryArchitectureEvidenceV1(
        result.architecture);
    writer.WriteBytes(base::Sha256(evidence));
    result.actualExecutionIdentity = base::Sha256(writer.Bytes());
    return base::Result<VerifiedGlobalMotorHistoryRunResultV1, TemporalExecutionErrorV1>::Success(
        std::move(result));
}

std::vector<std::byte>
SerializeFrozenVerifiedGlobalMotorHistoryExecutionV1(
    const FrozenVerifiedGlobalMotorHistoryExecutionV1& value)
{
    base::BinaryWriter writer;
    writer.WriteBytes(VerifiedLiteralIdentity(
        "SGE4-5.Spiral5.FrozenVerifiedGlobalMotorHistoryExecution.V1"));
    const auto verified = verification::SerializeVerifiedTemporalLoweringV1(
        value.Verified());
    writer.WriteU32(static_cast<std::uint32_t>(verified.size()));
    writer.WriteBytes(verified);
    writer.WriteU32(static_cast<std::uint32_t>(value.Artifact().Bytes().size()));
    writer.WriteBytes(value.Artifact().Bytes());
    writer.WriteU64(value.ResourceBinding().deviceEpoch);
    writer.WriteBytes(value.ResourceBinding().actualHistoryResourceIdentity);
    writer.WriteU32(static_cast<std::uint32_t>(value.ResourceBinding().historyRole));
    writer.WriteU32(value.ResourceBinding().historyDepth);
    writer.WriteU32(value.ResourceBinding().historyBytes);
    writer.WriteBytes(value.BindingIdentity());
    return std::move(writer).Take();
}

namespace
{
base::Digest256 BuildFamilyBindingIdentity(
    const family_verification::VerifiedTemporalCandidateV1& verified,
    const TemporalCandidateResourceBindingInputV1& binding)
{
    base::BinaryWriter writer;
    writer.WriteBytes(VerifiedLiteralIdentity(
        "SGE4-5.Spiral5.FrozenVerifiedTemporalCandidateBinding.V1"));
    writer.WriteBytes(verified.Plan().planIdentity);
    writer.WriteBytes(verified.CertificateIdentity());
    writer.WriteU64(binding.deviceEpoch);
    writer.WriteBytes(binding.actualHistoryResourceIdentity);
    writer.WriteU32(static_cast<std::uint32_t>(binding.historyRole));
    writer.WriteU32(binding.historyDepth);
    writer.WriteU32(binding.historyBytes);
    return base::Sha256(writer.Bytes());
}

#if defined(_WIN32)
TemporalCandidateObservationV1 ExecuteFamilyCandidate(
    GpuContext& gpu,
    const semantic::UpdateScheduleV1& schedule,
    const FrozenVerifiedTemporalCandidateV1& frozen)
{
    namespace fc = family_candidate;
    const auto& operation = frozen.Verified().Plan().operation;
    const std::uint32_t maxGeneration = schedule.invocations.back().sourceGeneration;

    std::vector<semantic::PgaMotorRecordV1> allLocalMotors;
    allLocalMotors.reserve(static_cast<std::size_t>(maxGeneration + 1u) * semantic::BoneCountV1);
    for (std::uint32_t generation = 0; generation <= maxGeneration; ++generation)
    {
        const auto motors = semantic::BuildCanonicalLocalMotorGenerationV1(generation);
        allLocalMotors.insert(allLocalMotors.end(), motors.begin(), motors.end());
    }
    const auto localBytes = std::as_bytes(std::span(allLocalMotors));
    const auto points = semantic::BuildCanonicalPointCorpusV1();
    const auto pointBytes = std::as_bytes(std::span(points));

    constexpr std::uint64_t argumentBytes = 12;
    constexpr std::uint64_t globalBytes = semantic::BoneCountV1 * sizeof(semantic::PgaMotorRecordV1);
    constexpr std::uint64_t outputBytes = semantic::WorkCountV1 * sizeof(semantic::PointRecordV1);

    auto localUpload = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_UPLOAD, localBytes.size(), D3D12_RESOURCE_STATE_GENERIC_READ);
    auto pointUpload = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_UPLOAD, pointBytes.size(), D3D12_RESOURCE_STATE_GENERIC_READ);
    UploadBytes(localUpload.Get(), localBytes);
    UploadBytes(pointUpload.Get(), pointBytes);

    ComPtr<ID3D12Resource> hierarchyArguments;
    ComPtr<ID3D12Resource> hierarchyArgumentReadback;
    ComPtr<ID3D12Resource> consumerArguments;
    ComPtr<ID3D12Resource> consumerArgumentReadback;
    if (operation.issuancePolicy != fc::TemporalIssuancePolicyV1::DirectEveryInvocation)
    {
        hierarchyArguments = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_DEFAULT, argumentBytes,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        hierarchyArgumentReadback = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_READBACK, argumentBytes,
            D3D12_RESOURCE_STATE_COPY_DEST);
    }
    if (operation.issuancePolicy == fc::TemporalIssuancePolicyV1::UpdateIndirectHierarchyAndConsumer)
    {
        consumerArguments = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_DEFAULT, argumentBytes,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        consumerArgumentReadback = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_READBACK, argumentBytes,
            D3D12_RESOURCE_STATE_COPY_DEST);
    }

    auto global = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_DEFAULT, globalBytes,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    auto output = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_DEFAULT, outputBytes,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    auto globalReadback = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_READBACK, globalBytes,
        D3D12_RESOURCE_STATE_COPY_DEST);
    auto outputReadback = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_READBACK, outputBytes,
        D3D12_RESOURCE_STATE_COPY_DEST);

    TemporalCandidateObservationV1 result;
    result.kind = operation.kind;
    result.verifiedBindingIdentity = frozen.BindingIdentity();
    result.invocations.reserve(schedule.invocations.size());

    D3D12_RESOURCE_STATES globalState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    base::Digest256 previousRetainedDigest{};
    base::Digest256 latestUpdateOutputDigest{};
    bool retainedValid = false;
    const std::uint64_t fenceBase = gpu.fenceValue;
    std::map<std::uint32_t, std::array<semantic::PgaMotorRecordV1, semantic::BoneCountV1>> cpuGlobalCache;
    std::map<std::uint32_t, std::vector<semantic::PointRecordV1>> cpuOutputCache;

    for (const auto& invocation : schedule.invocations)
    {
        const bool update = invocation.event == semantic::UpdateEventV1::Update;
        const std::uint32_t hierarchyDispatch = update
            ? operation.hierarchyUpdateDispatchX : operation.hierarchyHoldDispatchX;
        const std::uint32_t consumerDispatch = update
            ? operation.consumerUpdateDispatchX : operation.consumerHoldDispatchX;

        gpu.Begin();
        if (globalState != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
        {
            const auto barrier = Transition(global.Get(), globalState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            gpu.commandList->ResourceBarrier(1, &barrier);
            globalState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        }

        if (hierarchyArguments)
        {
            gpu.commandList->SetPipelineState(gpu.argumentPipeline.Get());
            gpu.commandList->SetComputeRootSignature(gpu.argumentRoot.Get());
            gpu.commandList->SetComputeRoot32BitConstant(0, hierarchyDispatch, 0);
            gpu.commandList->SetComputeRootUnorderedAccessView(1, hierarchyArguments->GetGPUVirtualAddress());
            gpu.commandList->Dispatch(1, 1, 1);
            auto uav = UavBarrier(hierarchyArguments.Get()); gpu.commandList->ResourceBarrier(1, &uav);
            auto toIndirect = Transition(hierarchyArguments.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
            gpu.commandList->ResourceBarrier(1, &toIndirect);
        }
        if (consumerArguments)
        {
            gpu.commandList->SetPipelineState(gpu.argumentPipeline.Get());
            gpu.commandList->SetComputeRootSignature(gpu.argumentRoot.Get());
            gpu.commandList->SetComputeRoot32BitConstant(0, consumerDispatch, 0);
            gpu.commandList->SetComputeRootUnorderedAccessView(1, consumerArguments->GetGPUVirtualAddress());
            gpu.commandList->Dispatch(1, 1, 1);
            auto uav = UavBarrier(consumerArguments.Get()); gpu.commandList->ResourceBarrier(1, &uav);
            auto toIndirect = Transition(consumerArguments.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
            gpu.commandList->ResourceBarrier(1, &toIndirect);
        }

        gpu.commandList->SetPipelineState(gpu.hierarchyPipeline.Get());
        gpu.commandList->SetComputeRootSignature(gpu.hierarchyRoot.Get());
        gpu.commandList->SetComputeRoot32BitConstant(0, invocation.sourceGeneration, 0);
        gpu.commandList->SetComputeRootShaderResourceView(1, localUpload->GetGPUVirtualAddress());
        gpu.commandList->SetComputeRootUnorderedAccessView(2, global->GetGPUVirtualAddress());
        if (hierarchyArguments)
            gpu.commandList->ExecuteIndirect(gpu.dispatchSignature.Get(), 1, hierarchyArguments.Get(), 0, nullptr, 0);
        else
            gpu.commandList->Dispatch(hierarchyDispatch, 1, 1);
        auto globalUav = UavBarrier(global.Get()); gpu.commandList->ResourceBarrier(1, &globalUav);
        auto globalToRead = Transition(global.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        gpu.commandList->ResourceBarrier(1, &globalToRead); globalState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

        gpu.commandList->SetPipelineState(gpu.consumerPipeline.Get());
        gpu.commandList->SetComputeRootSignature(gpu.consumerRoot.Get());
        gpu.commandList->SetComputeRootShaderResourceView(0, global->GetGPUVirtualAddress());
        gpu.commandList->SetComputeRootShaderResourceView(1, pointUpload->GetGPUVirtualAddress());
        gpu.commandList->SetComputeRootUnorderedAccessView(2, output->GetGPUVirtualAddress());
        if (consumerArguments)
            gpu.commandList->ExecuteIndirect(gpu.dispatchSignature.Get(), 1, consumerArguments.Get(), 0, nullptr, 0);
        else
            gpu.commandList->Dispatch(consumerDispatch, 1, 1);
        auto outputUav = UavBarrier(output.Get()); gpu.commandList->ResourceBarrier(1, &outputUav);

        auto outputToCopy = Transition(output.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
        gpu.commandList->ResourceBarrier(1, &outputToCopy);
        gpu.commandList->CopyBufferRegion(outputReadback.Get(), 0, output.Get(), 0, outputBytes);
        auto outputBack = Transition(output.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        gpu.commandList->ResourceBarrier(1, &outputBack);

        auto globalToCopy = Transition(global.Get(), globalState, D3D12_RESOURCE_STATE_COPY_SOURCE);
        gpu.commandList->ResourceBarrier(1, &globalToCopy);
        gpu.commandList->CopyBufferRegion(globalReadback.Get(), 0, global.Get(), 0, globalBytes);
        auto globalBack = Transition(global.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        gpu.commandList->ResourceBarrier(1, &globalBack); globalState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

        if (hierarchyArguments)
        {
            auto toCopy=Transition(hierarchyArguments.Get(),D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT,D3D12_RESOURCE_STATE_COPY_SOURCE);
            gpu.commandList->ResourceBarrier(1,&toCopy);
            gpu.commandList->CopyBufferRegion(hierarchyArgumentReadback.Get(),0,hierarchyArguments.Get(),0,argumentBytes);
            auto back=Transition(hierarchyArguments.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            gpu.commandList->ResourceBarrier(1,&back);
        }
        if (consumerArguments)
        {
            auto toCopy=Transition(consumerArguments.Get(),D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT,D3D12_RESOURCE_STATE_COPY_SOURCE);
            gpu.commandList->ResourceBarrier(1,&toCopy);
            gpu.commandList->CopyBufferRegion(consumerArgumentReadback.Get(),0,consumerArguments.Get(),0,argumentBytes);
            auto back=Transition(consumerArguments.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            gpu.commandList->ResourceBarrier(1,&back);
        }

        const auto fence = gpu.SubmitAndWait();
        const auto globalObserved = ReadbackBytes(globalReadback.Get(), globalBytes);
        const auto outputObserved = ReadbackBytes(outputReadback.Get(), outputBytes);
        std::uint32_t observedHierarchy = hierarchyDispatch;
        std::uint32_t observedConsumer = consumerDispatch;
        if (hierarchyArguments)
        {
            const auto bytes=ReadbackBytes(hierarchyArgumentReadback.Get(),argumentBytes);
            std::memcpy(&observedHierarchy,bytes.data(),sizeof(observedHierarchy));
        }
        if (consumerArguments)
        {
            const auto bytes=ReadbackBytes(consumerArgumentReadback.Get(),argumentBytes);
            std::memcpy(&observedConsumer,bytes.data(),sizeof(observedConsumer));
        }
        if(observedHierarchy!=hierarchyDispatch || observedConsumer!=consumerDispatch)
            throw std::runtime_error("Temporal Candidate GPU Dispatch arguments differ from Verified Plan.");

        if(!cpuGlobalCache.contains(invocation.sourceGeneration))
        {
            const auto local=semantic::BuildCanonicalLocalMotorGenerationV1(invocation.sourceGeneration);
            cpuGlobalCache.emplace(invocation.sourceGeneration,semantic::ComposeCanonicalGlobalMotorsCpuV1(local));
            cpuOutputCache.emplace(invocation.sourceGeneration,
                semantic::ApplyGlobalMotorsCpuV1(cpuGlobalCache.at(invocation.sourceGeneration),points));
        }
        const auto& expectedGlobal=cpuGlobalCache.at(invocation.sourceGeneration);
        const auto& expectedOutput=cpuOutputCache.at(invocation.sourceGeneration);

        std::array<semantic::PgaMotorRecordV1,semantic::BoneCountV1> actualGlobal{};
        std::memcpy(actualGlobal.data(),globalObserved.data(),globalObserved.size());
        for(std::uint32_t bone=0;bone<semantic::BoneCountV1;++bone)
        {
            const float* a=reinterpret_cast<const float*>(&actualGlobal[bone]);
            const float* e=reinterpret_cast<const float*>(&expectedGlobal[bone]);
            for(std::size_t c=0;c<8;++c)
                if(!std::isfinite(a[c]) || std::abs(a[c]-e[c])>1.0e-6f)
                    throw std::runtime_error("Temporal Candidate Global Motor differs from CPU reference.");
        }

        TemporalFamilyInvocationObservationV1 observation;
        observation.invocationIndex=invocation.invocationIndex; observation.event=invocation.event;
        observation.sourceGeneration=invocation.sourceGeneration;
        observation.expectedHierarchyDispatchX=hierarchyDispatch; observation.observedHierarchyDispatchX=observedHierarchy;
        observation.expectedConsumerDispatchX=consumerDispatch; observation.observedConsumerDispatchX=observedConsumer;
        observation.retainedCompletionOrdinal=static_cast<std::uint64_t>(invocation.invocationIndex)+1u;
        observation.nativeFenceValue=fence;
        observation.outputDigest=base::Sha256(outputObserved);
        if(operation.historyRole==fc::TemporalFamilyHistoryRoleV1::GlobalMotorHistory)
            observation.retainedHistoryDigest=base::Sha256(globalObserved);
        else if(operation.historyRole==fc::TemporalFamilyHistoryRoleV1::FinalOutputHistory)
            observation.retainedHistoryDigest=observation.outputDigest;
        else
            observation.retainedHistoryDigest={};
        observation.retainedHistoryByteStable=update || operation.historyRole==fc::TemporalFamilyHistoryRoleV1::None ||
            (retainedValid && observation.retainedHistoryDigest==previousRetainedDigest);

        std::vector<semantic::PointRecordV1> actualOutput(semantic::WorkCountV1);
        std::memcpy(actualOutput.data(),outputObserved.data(),outputObserved.size());
        for(std::uint32_t index=0;index<semantic::WorkCountV1;++index)
        {
            const auto& a=actualOutput[index]; const auto& e=expectedOutput[index];
            const std::array<float,4> av{a.x,a.y,a.z,a.w}, ev{e.x,e.y,e.z,e.w};
            float localMax=0.0f; bool finite=true;
            for(std::size_t c=0;c<4;++c){finite=finite&&std::isfinite(av[c]);localMax=std::max(localMax,std::abs(av[c]-ev[c]));}
            observation.maxAbsoluteError=std::max(observation.maxAbsoluteError,localMax);
            if(!finite)++observation.nonFiniteCount;
            if(!std::isfinite(a.w)||std::abs(a.w-1.0f)>1.0e-6f)++observation.homogeneousCoordinateMismatchCount;
            if(!finite||localMax>1.0e-5f)++observation.mismatchCount;
        }
        observation.outputMatchesReference=observation.mismatchCount==0&&observation.nonFiniteCount==0&&observation.homogeneousCoordinateMismatchCount==0;
        if(update)latestUpdateOutputDigest=observation.outputDigest;
        observation.outputMatchesLatestUpdate=observation.outputDigest==latestUpdateOutputDigest;
        if(!observation.retainedHistoryByteStable||!observation.outputMatchesReference||!observation.outputMatchesLatestUpdate||
           observation.nativeFenceValue!=fenceBase+observation.retainedCompletionOrdinal)
            throw std::runtime_error("Temporal Candidate observation violates the Verified family contract.");
        previousRetainedDigest=observation.retainedHistoryDigest; retainedValid=true;
        result.invocations.push_back(observation);
    }

    base::BinaryWriter identity;
    identity.WriteBytes(VerifiedLiteralIdentity("SGE4-5.Spiral5.ActualTemporalCandidateExecution.V1"));
    identity.WriteBytes(result.verifiedBindingIdentity);
    identity.WriteU32(static_cast<std::uint32_t>(result.kind));
    for(const auto& invocation:result.invocations)identity.WriteBytes(invocation.outputDigest);
    result.actualExecutionIdentity=base::Sha256(identity.Bytes());
    return result;
}
#endif
}

FrozenVerifiedTemporalCandidateV1::FrozenVerifiedTemporalCandidateV1(
    family_verification::VerifiedTemporalCandidateV1 verified,
    TemporalCandidateResourceBindingInputV1 resourceBinding,
    base::Digest256 bindingIdentity)
    : verified_(std::move(verified)), resourceBinding_(resourceBinding), bindingIdentity_(bindingIdentity) {}

TemporalCandidateResourceBindingInputV1 BuildCanonicalTemporalCandidateResourceBindingInputV1(
    const family_verification::VerifiedTemporalCandidateV1& verified,
    std::uint64_t deviceEpoch)
{
    TemporalCandidateResourceBindingInputV1 binding;
    binding.deviceEpoch=deviceEpoch;
    binding.actualHistoryResourceIdentity=verified.Plan().operation.historyResourceIdentity;
    binding.historyRole=verified.Plan().operation.historyRole;
    binding.historyDepth=verified.Plan().operation.historyDepth;
    binding.historyBytes=verified.Plan().operation.historyBytes;
    return binding;
}

base::Result<void,std::string> ValidateFrozenVerifiedTemporalCandidateV1(
    const FrozenVerifiedTemporalCandidateV1& frozen,
    const semantic::TemporalStateSemanticV1& semanticValue,
    const semantic::UpdateScheduleV1& schedule,
    const family_verification::TemporalCandidateFamilyVerificationContextV1& context,
    std::uint64_t expectedDeviceEpoch)
{
    auto verified=family_verification::ValidateVerifiedTemporalCandidateContextV1(
        frozen.Verified(),semanticValue,schedule,context);
    if(!verified)return verified;
    const auto& operation=frozen.Verified().Plan().operation;
    const auto& binding=frozen.ResourceBinding();
    if(expectedDeviceEpoch==0||binding.deviceEpoch!=expectedDeviceEpoch)
        return base::Result<void,std::string>::Failure("Temporal Candidate binding belongs to another device epoch.");
    if(binding.actualHistoryResourceIdentity!=operation.historyResourceIdentity||binding.historyRole!=operation.historyRole||
       binding.historyDepth!=operation.historyDepth||binding.historyBytes!=operation.historyBytes)
        return base::Result<void,std::string>::Failure("Temporal Candidate History Resource binding mismatch.");
    if(frozen.BindingIdentity()!=BuildFamilyBindingIdentity(frozen.Verified(),binding))
        return base::Result<void,std::string>::Failure("Temporal Candidate frozen binding identity mismatch.");
    return base::Result<void,std::string>::Success();
}

base::Result<FrozenVerifiedTemporalCandidateV1,std::string> FreezeVerifiedTemporalCandidateV1(
    const semantic::TemporalStateSemanticV1& semanticValue,
    const semantic::UpdateScheduleV1& schedule,
    const family_verification::TemporalCandidateFamilyVerificationContextV1& context,
    const family_verification::VerifiedTemporalCandidateV1& verified,
    TemporalCandidateResourceBindingInputV1 binding)
{
    auto valid=family_verification::ValidateVerifiedTemporalCandidateContextV1(verified,semanticValue,schedule,context);
    if(!valid)return base::Result<FrozenVerifiedTemporalCandidateV1,std::string>::Failure(valid.Error());
    FrozenVerifiedTemporalCandidateV1 frozen(verified,binding,BuildFamilyBindingIdentity(verified,binding));
    auto frozenValid=ValidateFrozenVerifiedTemporalCandidateV1(frozen,semanticValue,schedule,context,binding.deviceEpoch);
    if(!frozenValid)return base::Result<FrozenVerifiedTemporalCandidateV1,std::string>::Failure(frozenValid.Error());
    return base::Result<FrozenVerifiedTemporalCandidateV1,std::string>::Success(std::move(frozen));
}

#if defined(_WIN32)
namespace
{
TemporalCandidateFamilyArchitectureResultV1
RunVerifiedTemporalCandidateFamilyWithGpuV1(
    GpuContext& gpu,
    const semantic::TemporalStateSemanticV1& semanticValue,
    const semantic::UpdateScheduleV1& schedule,
    const family_verification::TemporalCandidateFamilyVerificationContextV1& context,
    std::span<const FrozenVerifiedTemporalCandidateV1> candidates,
    std::uint64_t expectedDeviceEpoch)
{
    if (candidates.size() != 3)
        throw std::runtime_error("Temporal Candidate family must contain exactly three candidates.");
    constexpr std::array<family_candidate::TemporalCandidateKindV1,3> expectedKinds{
        family_candidate::TemporalCandidateKindV1::EveryInvocationRecompute,
        family_candidate::TemporalCandidateKindV1::GlobalMotorHistoryReuse,
        family_candidate::TemporalCandidateKindV1::FinalOutputHistoryReuse};
    for (std::size_t index = 0; index < candidates.size(); ++index)
    {
        const auto& candidate = candidates[index];
        auto valid = ValidateFrozenVerifiedTemporalCandidateV1(
            candidate, semanticValue, schedule, context, expectedDeviceEpoch);
        if (!valid)
            throw std::runtime_error("Temporal Candidate family authority: " + valid.Error());
        if (candidate.Verified().Plan().operation.kind != expectedKinds[index])
            throw std::runtime_error(
                "Temporal Candidate family order or uniqueness differs from A/B/C.");
    }

    TemporalCandidateFamilyArchitectureResultV1 result;
    result.adapterDescription = gpu.adapterDescription;
    result.scheduleId = schedule.id;
    result.semanticIdentity = semantic::TemporalStateSemanticIdentityV1(semanticValue);
    result.scheduleIdentity = semantic::UpdateScheduleIdentityV1(schedule);
    result.argumentProducerShaderDigest = BlobDigest(gpu.argumentShader.Get());
    result.hierarchyShaderDigest = BlobDigest(gpu.hierarchyShader.Get());
    result.consumerShaderDigest = BlobDigest(gpu.consumerShader.Get());
    for (const auto& candidate : candidates)
        result.candidates.push_back(ExecuteFamilyCandidate(gpu, schedule, candidate));

    for (std::size_t invocation = 0; invocation < schedule.invocations.size(); ++invocation)
    {
        const auto digest = result.candidates.front().invocations[invocation].outputDigest;
        for (std::size_t candidate = 1; candidate < result.candidates.size(); ++candidate)
        {
            if (result.candidates[candidate].invocations[invocation].outputDigest != digest)
                throw std::runtime_error(
                    "Temporal Candidate outputs differ at one Invocation.");
        }
    }
    result.pairwiseOutputEquivalent = true;
    return result;
}
}
#endif

base::Result<TemporalCandidateFamilyArchitectureResultV1,TemporalExecutionErrorV1>
RunVerifiedTemporalCandidateFamilyOnWarpV1(
    const semantic::TemporalStateSemanticV1& semanticValue,
    const semantic::UpdateScheduleV1& schedule,
    const family_verification::TemporalCandidateFamilyVerificationContextV1& context,
    std::span<const FrozenVerifiedTemporalCandidateV1> candidates,
    std::uint64_t expectedDeviceEpoch)
{
#if defined(_WIN32)
    try
    {
        GpuContext gpu;
        return base::Result<TemporalCandidateFamilyArchitectureResultV1,TemporalExecutionErrorV1>::Success(
            RunVerifiedTemporalCandidateFamilyWithGpuV1(
                gpu, semanticValue, schedule, context, candidates, expectedDeviceEpoch));
    }
    catch (const std::exception& error)
    {
        return base::Result<TemporalCandidateFamilyArchitectureResultV1,TemporalExecutionErrorV1>::Failure(
            Error("family/d3d12", error.what()));
    }
#else
    (void)semanticValue;(void)schedule;(void)context;(void)candidates;(void)expectedDeviceEpoch;
    return base::Result<TemporalCandidateFamilyArchitectureResultV1,TemporalExecutionErrorV1>::Failure(
        Error("family/platform","Spiral 5 CU4 D3D12 execution requires Windows."));
#endif
}

std::vector<std::byte> SerializeFrozenVerifiedTemporalCandidateV1(const FrozenVerifiedTemporalCandidateV1& value)
{
    base::BinaryWriter writer;
    writer.WriteBytes(VerifiedLiteralIdentity("SGE4-5.Spiral5.FrozenVerifiedTemporalCandidate.V1"));
    const auto verified=family_verification::SerializeVerifiedTemporalCandidateV1(value.Verified());
    writer.WriteU32(static_cast<std::uint32_t>(verified.size()));writer.WriteBytes(verified);
    writer.WriteU64(value.ResourceBinding().deviceEpoch);writer.WriteBytes(value.ResourceBinding().actualHistoryResourceIdentity);
    writer.WriteU32(static_cast<std::uint32_t>(value.ResourceBinding().historyRole));
    writer.WriteU32(value.ResourceBinding().historyDepth);writer.WriteU32(value.ResourceBinding().historyBytes);
    writer.WriteBytes(value.BindingIdentity());
    return std::move(writer).Take();
}

std::vector<std::byte> SerializeTemporalCandidateFamilyArchitectureEvidenceV1(
    const TemporalCandidateFamilyArchitectureResultV1& value)
{
    base::BinaryWriter writer;
    writer.WriteU32(0x34463553u);writer.WriteU32(1);WriteString(writer,value.adapterDescription);WriteString(writer,value.scheduleId);
    WriteDigest(writer,value.semanticIdentity);WriteDigest(writer,value.scheduleIdentity);WriteDigest(writer,value.argumentProducerShaderDigest);
    WriteDigest(writer,value.hierarchyShaderDigest);WriteDigest(writer,value.consumerShaderDigest);
    writer.WriteU32(value.pairwiseOutputEquivalent?1u:0u);writer.WriteU32(static_cast<std::uint32_t>(value.candidates.size()));
    for(const auto& candidate:value.candidates)
    {
        writer.WriteU32(static_cast<std::uint32_t>(candidate.kind));WriteDigest(writer,candidate.verifiedBindingIdentity);
        WriteDigest(writer,candidate.actualExecutionIdentity);writer.WriteU32(static_cast<std::uint32_t>(candidate.invocations.size()));
        for(const auto& invocation:candidate.invocations)
        {
            writer.WriteU32(invocation.invocationIndex);writer.WriteU32(static_cast<std::uint32_t>(invocation.event));
            writer.WriteU32(invocation.sourceGeneration);writer.WriteU32(invocation.expectedHierarchyDispatchX);
            writer.WriteU32(invocation.observedHierarchyDispatchX);writer.WriteU32(invocation.expectedConsumerDispatchX);
            writer.WriteU32(invocation.observedConsumerDispatchX);writer.WriteU64(invocation.retainedCompletionOrdinal);
            writer.WriteU64(invocation.nativeFenceValue);writer.WriteU32(invocation.retainedHistoryByteStable?1u:0u);
            writer.WriteU32(invocation.outputMatchesReference?1u:0u);writer.WriteU32(invocation.outputMatchesLatestUpdate?1u:0u);
            writer.WriteU32(invocation.mismatchCount);writer.WriteU32(invocation.nonFiniteCount);
            writer.WriteU32(invocation.homogeneousCoordinateMismatchCount);WriteFloat(writer,invocation.maxAbsoluteError);
            WriteDigest(writer,invocation.retainedHistoryDigest);WriteDigest(writer,invocation.outputDigest);
        }
    }
    return std::move(writer).Take();
}

namespace
{
TemporalCandidateFamilyRuntimeErrorV1 RuntimeError(
    std::string stage,
    std::string message,
    long hresult = 0)
{
    return {std::move(stage), std::move(message), hresult};
}

[[maybe_unused]] base::Digest256 BuildTemporalCandidateBindingSetIdentityV1(
    std::span<const TemporalScheduleCandidateAuthorityV1> authorities)
{
    base::BinaryWriter writer;
    writer.WriteBytes(VerifiedLiteralIdentity(
        "SGE4-5.Spiral5.TemporalCandidateBindingSet.V1"));
    writer.WriteU32(static_cast<std::uint32_t>(authorities.size()));
    for (const auto& authority : authorities)
    {
        writer.WriteBytes(semantic::UpdateScheduleIdentityV1(authority.schedule));
        writer.WriteU32(static_cast<std::uint32_t>(authority.candidates.size()));
        for (const auto& candidate : authority.candidates)
        {
            writer.WriteBytes(candidate.Plan().planIdentity);
            writer.WriteBytes(candidate.CertificateIdentity());
        }
    }
    return base::Sha256(writer.Bytes());
}

[[maybe_unused]] base::Digest256 BuildTemporalRuntimeDomainIdentityV1(
    const semantic::TemporalStateSemanticV1& semanticValue,
    const family_verification::TemporalCandidateFamilyVerificationContextV1& context,
    const base::Digest256& bindingSetIdentity)
{
    base::BinaryWriter writer;
    writer.WriteBytes(VerifiedLiteralIdentity(
        "SGE4-5.Spiral5.TemporalCandidateFamilyRuntimeDomain.V1"));
    writer.WriteBytes(semantic::TemporalStateSemanticIdentityV1(semanticValue));
    writer.WriteBytes(context.targetProfileIdentity);
    writer.WriteBytes(context.observationContractIdentity);
    writer.WriteBytes(context.deviceEpochPolicyIdentity);
    writer.WriteBytes(bindingSetIdentity);
    return base::Sha256(writer.Bytes());
}

base::Digest256 BuildTemporalDynamicBindingIdentityV1(
    const base::Digest256& domainIdentity,
    const base::Digest256& bindingSetIdentity,
    std::uint64_t deviceEpoch)
{
    base::BinaryWriter writer;
    writer.WriteBytes(VerifiedLiteralIdentity(
        "SGE4-5.Spiral5.TemporalRuntimeDynamicBinding.V1"));
    writer.WriteBytes(domainIdentity);
    writer.WriteBytes(bindingSetIdentity);
    writer.WriteU64(deviceEpoch);
    return base::Sha256(writer.Bytes());
}

base::Digest256 BuildTemporalSeedIdentityV1(
    const TemporalRuntimeDynamicBindingV1& binding)
{
    base::BinaryWriter writer;
    writer.WriteBytes(VerifiedLiteralIdentity(
        "SGE4-5.Spiral5.TemporalHistoryExplicitReseed.V1"));
    writer.WriteU64(binding.deviceEpoch);
    writer.WriteBytes(binding.bindingIdentity);
    writer.WriteU32(0);
    return base::Sha256(writer.Bytes());
}

#if defined(_WIN32)
bool SameLuidV1(const LUID& left, const LUID& right) noexcept
{
    return left.LowPart == right.LowPart && left.HighPart == right.HighPart;
}
#endif
}

struct LoadedTemporalCandidateFamilyRuntimeV1::Impl final
{
    semantic::TemporalStateSemanticV1 semantic;
    family_verification::TemporalCandidateFamilyVerificationContextV1 context;
    std::vector<TemporalScheduleCandidateAuthorityV1> authorities;
    TemporalCandidateFamilyRuntimeStateV1 state =
        TemporalCandidateFamilyRuntimeStateV1::Active;
    std::uint64_t deviceEpoch = 1;
    std::uint64_t submissionOrdinal = 0;
    base::Digest256 domainIdentity{};
    base::Digest256 candidateBindingSetIdentity{};
    std::string adapterDescription;
    bool dynamicInputsBound = false;
    bool historySeeded = false;
    TemporalRuntimeDynamicBindingV1 currentBinding{};
    TemporalHistorySeedV1 currentSeed{};
    std::vector<TemporalRuntimeSubmissionTokenV1> issuedTokens;
    std::vector<TemporalHistoryHandleV1> issuedHistories;
#if defined(_WIN32)
    std::unique_ptr<GpuContext> gpu;
    bool hasExcludedAdapterLuid = false;
    LUID excludedAdapterLuid{};
#endif
};

LoadedTemporalCandidateFamilyRuntimeV1::LoadedTemporalCandidateFamilyRuntimeV1(
    std::unique_ptr<Impl> impl)
    : impl_(std::move(impl))
{
}

LoadedTemporalCandidateFamilyRuntimeV1::LoadedTemporalCandidateFamilyRuntimeV1(
    LoadedTemporalCandidateFamilyRuntimeV1&&) noexcept = default;
LoadedTemporalCandidateFamilyRuntimeV1&
LoadedTemporalCandidateFamilyRuntimeV1::operator=(
    LoadedTemporalCandidateFamilyRuntimeV1&&) noexcept = default;
LoadedTemporalCandidateFamilyRuntimeV1::~LoadedTemporalCandidateFamilyRuntimeV1() = default;

TemporalCandidateFamilyRuntimeStateV1
LoadedTemporalCandidateFamilyRuntimeV1::State() const noexcept
{
    return impl_->state;
}

std::uint64_t LoadedTemporalCandidateFamilyRuntimeV1::DeviceEpoch() const noexcept
{
    return impl_->deviceEpoch;
}

const base::Digest256&
LoadedTemporalCandidateFamilyRuntimeV1::DomainIdentity() const noexcept
{
    return impl_->domainIdentity;
}

const base::Digest256&
LoadedTemporalCandidateFamilyRuntimeV1::CandidateBindingSetIdentity() const noexcept
{
    return impl_->candidateBindingSetIdentity;
}

const std::string&
LoadedTemporalCandidateFamilyRuntimeV1::AdapterDescription() const noexcept
{
    return impl_->adapterDescription;
}

std::size_t LoadedTemporalCandidateFamilyRuntimeV1::ScheduleCount() const noexcept
{
    return impl_->authorities.size();
}

bool LoadedTemporalCandidateFamilyRuntimeV1::DynamicInputsBound() const noexcept
{
    return impl_->dynamicInputsBound;
}

bool LoadedTemporalCandidateFamilyRuntimeV1::HistorySeeded() const noexcept
{
    return impl_->historySeeded;
}

base::Result<LoadedTemporalCandidateFamilyRuntimeV1, TemporalCandidateFamilyRuntimeErrorV1>
LoadTemporalCandidateFamilyRuntimeOnWarpV1(
    const semantic::TemporalStateSemanticV1& semanticValue,
    const family_verification::TemporalCandidateFamilyVerificationContextV1& context,
    std::span<const TemporalScheduleCandidateAuthorityV1> authorities)
{
#if defined(_WIN32)
    try
    {
        constexpr std::array<std::string_view, 9> expectedSchedules{
            "P1", "P2", "P4", "P8", "P16", "P32", "P64",
            "BurstThenHold", "Irregular"};
        if (authorities.size() != expectedSchedules.size())
        {
            return base::Result<LoadedTemporalCandidateFamilyRuntimeV1, TemporalCandidateFamilyRuntimeErrorV1>::Failure(
                RuntimeError("temporal-runtime/authority-count",
                    "Temporal Runtime requires the complete nine-schedule authority corpus."));
        }

        constexpr std::array<family_candidate::TemporalCandidateKindV1, 3> expectedKinds{
            family_candidate::TemporalCandidateKindV1::EveryInvocationRecompute,
            family_candidate::TemporalCandidateKindV1::GlobalMotorHistoryReuse,
            family_candidate::TemporalCandidateKindV1::FinalOutputHistoryReuse};

        auto impl = std::make_unique<LoadedTemporalCandidateFamilyRuntimeV1::Impl>();
        impl->semantic = semanticValue;
        impl->context = context;
        impl->authorities.assign(authorities.begin(), authorities.end());

        for (std::size_t scheduleIndex = 0;
             scheduleIndex < impl->authorities.size();
             ++scheduleIndex)
        {
            auto& authority = impl->authorities[scheduleIndex];
            auto scheduleValid = semantic::ValidateUpdateScheduleV1(authority.schedule);
            if (!scheduleValid)
            {
                return base::Result<LoadedTemporalCandidateFamilyRuntimeV1, TemporalCandidateFamilyRuntimeErrorV1>::Failure(
                    RuntimeError("temporal-runtime/schedule", scheduleValid.Error().message));
            }
            if (authority.schedule.id != expectedSchedules[scheduleIndex])
            {
                return base::Result<LoadedTemporalCandidateFamilyRuntimeV1, TemporalCandidateFamilyRuntimeErrorV1>::Failure(
                    RuntimeError("temporal-runtime/schedule-order",
                        "Temporal Runtime schedule corpus order is not canonical."));
            }
            if (authority.candidates.size() != expectedKinds.size())
            {
                return base::Result<LoadedTemporalCandidateFamilyRuntimeV1, TemporalCandidateFamilyRuntimeErrorV1>::Failure(
                    RuntimeError("temporal-runtime/candidate-count",
                        "Each Temporal schedule must contain exactly A/B/C."));
            }
            for (std::size_t candidateIndex = 0;
                 candidateIndex < authority.candidates.size();
                 ++candidateIndex)
            {
                const auto& candidate = authority.candidates[candidateIndex];
                auto candidateValid =
                    family_verification::ValidateVerifiedTemporalCandidateContextV1(
                        candidate, semanticValue, authority.schedule, context);
                if (!candidateValid)
                {
                    return base::Result<LoadedTemporalCandidateFamilyRuntimeV1, TemporalCandidateFamilyRuntimeErrorV1>::Failure(
                        RuntimeError("temporal-runtime/candidate-authority", candidateValid.Error()));
                }
                if (candidate.Plan().operation.kind != expectedKinds[candidateIndex])
                {
                    return base::Result<LoadedTemporalCandidateFamilyRuntimeV1, TemporalCandidateFamilyRuntimeErrorV1>::Failure(
                        RuntimeError("temporal-runtime/candidate-order",
                            "Temporal Candidate order differs from A/B/C."));
                }
            }
        }

        impl->candidateBindingSetIdentity =
            BuildTemporalCandidateBindingSetIdentityV1(impl->authorities);
        impl->domainIdentity = BuildTemporalRuntimeDomainIdentityV1(
            impl->semantic, impl->context, impl->candidateBindingSetIdentity);
        impl->gpu = std::make_unique<GpuContext>();
        impl->adapterDescription = impl->gpu->adapterDescription;

        return base::Result<LoadedTemporalCandidateFamilyRuntimeV1, TemporalCandidateFamilyRuntimeErrorV1>::Success(
            LoadedTemporalCandidateFamilyRuntimeV1(std::move(impl)));
    }
    catch (const std::exception& error)
    {
        return base::Result<LoadedTemporalCandidateFamilyRuntimeV1, TemporalCandidateFamilyRuntimeErrorV1>::Failure(
            RuntimeError("temporal-runtime/load", error.what()));
    }
#else
    (void)semanticValue;(void)context;(void)authorities;
    return base::Result<LoadedTemporalCandidateFamilyRuntimeV1, TemporalCandidateFamilyRuntimeErrorV1>::Failure(
        RuntimeError("temporal-runtime/platform",
            "Spiral 5 CU5 Temporal Runtime requires Windows D3D12."));
#endif
}

base::Result<void, TemporalCandidateFamilyRuntimeErrorV1>
ValidateTemporalRuntimeEpochHandleV1(
    const LoadedTemporalCandidateFamilyRuntimeV1& runtime,
    const TemporalRuntimeEpochHandleV1& handle)
{
    const auto& impl = *runtime.impl_;
    if (impl.state != TemporalCandidateFamilyRuntimeStateV1::Active)
    {
        return base::Result<void, TemporalCandidateFamilyRuntimeErrorV1>::Failure(
            RuntimeError("temporal-runtime/device-state",
                "Temporal Runtime is not active."));
    }
    if (handle.deviceEpoch != impl.deviceEpoch)
    {
        return base::Result<void, TemporalCandidateFamilyRuntimeErrorV1>::Failure(
            RuntimeError("temporal-runtime/stale-epoch",
                "Temporal Runtime handle belongs to another device epoch."));
    }
    if (handle.domainIdentity != impl.domainIdentity ||
        handle.candidateBindingSetIdentity != impl.candidateBindingSetIdentity)
    {
        return base::Result<void, TemporalCandidateFamilyRuntimeErrorV1>::Failure(
            RuntimeError("temporal-runtime/handle-identity",
                "Temporal Runtime handle identity mismatch."));
    }
    return base::Result<void, TemporalCandidateFamilyRuntimeErrorV1>::Success();
}

TemporalRuntimeEpochHandleV1 CaptureTemporalRuntimeEpochHandleV1(
    const LoadedTemporalCandidateFamilyRuntimeV1& runtime)
{
    return {
        runtime.impl_->deviceEpoch,
        runtime.impl_->domainIdentity,
        runtime.impl_->candidateBindingSetIdentity};
}

base::Result<TemporalRuntimeDynamicBindingV1, TemporalCandidateFamilyRuntimeErrorV1>
BindCanonicalTemporalRuntimeInputsV1(LoadedTemporalCandidateFamilyRuntimeV1& runtime)
{
    auto& impl = *runtime.impl_;
    if (impl.state != TemporalCandidateFamilyRuntimeStateV1::Active)
    {
        return base::Result<TemporalRuntimeDynamicBindingV1, TemporalCandidateFamilyRuntimeErrorV1>::Failure(
            RuntimeError("temporal-runtime/device-state",
                "Temporal Runtime cannot bind inputs while inactive."));
    }
    impl.currentBinding.deviceEpoch = impl.deviceEpoch;
    impl.currentBinding.bindingIdentity = BuildTemporalDynamicBindingIdentityV1(
        impl.domainIdentity, impl.candidateBindingSetIdentity, impl.deviceEpoch);
    impl.dynamicInputsBound = true;
    impl.historySeeded = false;
    impl.currentSeed = {};
    return base::Result<TemporalRuntimeDynamicBindingV1, TemporalCandidateFamilyRuntimeErrorV1>::Success(
        impl.currentBinding);
}

base::Result<TemporalHistorySeedV1, TemporalCandidateFamilyRuntimeErrorV1>
ReseedTemporalHistoriesV1(
    LoadedTemporalCandidateFamilyRuntimeV1& runtime,
    const TemporalRuntimeEpochHandleV1& handle,
    const TemporalRuntimeDynamicBindingV1& binding)
{
    auto handleValid = ValidateTemporalRuntimeEpochHandleV1(runtime, handle);
    if (!handleValid)
        return base::Result<TemporalHistorySeedV1, TemporalCandidateFamilyRuntimeErrorV1>::Failure(
            handleValid.Error());
    auto& impl = *runtime.impl_;
    if (!impl.dynamicInputsBound ||
        binding.deviceEpoch != impl.deviceEpoch ||
        binding.bindingIdentity != impl.currentBinding.bindingIdentity)
    {
        return base::Result<TemporalHistorySeedV1, TemporalCandidateFamilyRuntimeErrorV1>::Failure(
            RuntimeError("temporal-runtime/rebind-required",
                "Current-epoch Temporal dynamic inputs must be rebound before reseed."));
    }
    impl.currentSeed.deviceEpoch = impl.deviceEpoch;
    impl.currentSeed.sourceGeneration = 0;
    impl.currentSeed.seedIdentity = BuildTemporalSeedIdentityV1(binding);
    impl.historySeeded = true;
    return base::Result<TemporalHistorySeedV1, TemporalCandidateFamilyRuntimeErrorV1>::Success(
        impl.currentSeed);
}

base::Result<TemporalRuntimeSubmissionV1, TemporalCandidateFamilyRuntimeErrorV1>
SubmitTemporalScheduleV1(
    LoadedTemporalCandidateFamilyRuntimeV1& runtime,
    const TemporalRuntimeEpochHandleV1& handle,
    const TemporalRuntimeDynamicBindingV1& binding,
    const TemporalHistorySeedV1& seed,
    const base::Digest256& scheduleIdentity)
{
#if defined(_WIN32)
    auto handleValid = ValidateTemporalRuntimeEpochHandleV1(runtime, handle);
    if (!handleValid)
        return base::Result<TemporalRuntimeSubmissionV1, TemporalCandidateFamilyRuntimeErrorV1>::Failure(
            handleValid.Error());
    auto& impl = *runtime.impl_;
    if (!impl.dynamicInputsBound ||
        binding.deviceEpoch != impl.deviceEpoch ||
        binding.bindingIdentity != impl.currentBinding.bindingIdentity)
    {
        return base::Result<TemporalRuntimeSubmissionV1, TemporalCandidateFamilyRuntimeErrorV1>::Failure(
            RuntimeError("temporal-runtime/rebind-required",
                "Temporal submission requires a current-epoch dynamic binding."));
    }
    if (!impl.historySeeded ||
        seed.deviceEpoch != impl.deviceEpoch ||
        seed.sourceGeneration != 0 ||
        seed.seedIdentity != impl.currentSeed.seedIdentity)
    {
        return base::Result<TemporalRuntimeSubmissionV1, TemporalCandidateFamilyRuntimeErrorV1>::Failure(
            RuntimeError("temporal-runtime/reseed-required",
                "Temporal history is invalid until an explicit generation-zero reseed is authorized."));
    }

    auto authority = std::find_if(
        impl.authorities.begin(), impl.authorities.end(),
        [&](const TemporalScheduleCandidateAuthorityV1& value)
        {
            return semantic::UpdateScheduleIdentityV1(value.schedule) == scheduleIdentity;
        });
    if (authority == impl.authorities.end())
    {
        return base::Result<TemporalRuntimeSubmissionV1, TemporalCandidateFamilyRuntimeErrorV1>::Failure(
            RuntimeError("temporal-runtime/schedule-authority",
                "Requested schedule is not in the Frozen CU5 authority corpus."));
    }

    try
    {
        std::vector<FrozenVerifiedTemporalCandidateV1> frozen;
        frozen.reserve(authority->candidates.size());
        for (const auto& verified : authority->candidates)
        {
            auto resourceBinding =
                BuildCanonicalTemporalCandidateResourceBindingInputV1(
                    verified, impl.deviceEpoch);
            auto value = FreezeVerifiedTemporalCandidateV1(
                impl.semantic, authority->schedule, impl.context,
                verified, resourceBinding);
            if (!value)
            {
                return base::Result<TemporalRuntimeSubmissionV1, TemporalCandidateFamilyRuntimeErrorV1>::Failure(
                    RuntimeError("temporal-runtime/freeze", value.Error()));
            }
            frozen.push_back(std::move(value).Value());
        }

        TemporalRuntimeSubmissionV1 submission;
        submission.architecture = RunVerifiedTemporalCandidateFamilyWithGpuV1(
            *impl.gpu, impl.semantic, authority->schedule,
            impl.context, frozen, impl.deviceEpoch);
        submission.historyResourcesMaterialized = true;
        submission.temporalArgumentsRegenerated = true;
        submission.completionStateMaterialized = true;
        submission.readbackStateMaterialized = true;

        ++impl.submissionOrdinal;
        base::BinaryWriter token;
        token.WriteBytes(VerifiedLiteralIdentity(
            "SGE4-5.Spiral5.TemporalRuntimeSubmissionToken.V1"));
        token.WriteBytes(impl.domainIdentity);
        token.WriteU64(impl.deviceEpoch);
        token.WriteU64(impl.submissionOrdinal);
        token.WriteBytes(scheduleIdentity);
        const auto architectureBytes =
            SerializeTemporalCandidateFamilyArchitectureEvidenceV1(
                submission.architecture);
        token.WriteBytes(base::Sha256(architectureBytes));
        submission.completion.deviceEpoch = impl.deviceEpoch;
        submission.completion.submissionOrdinal = impl.submissionOrdinal;
        submission.completion.tokenIdentity = base::Sha256(token.Bytes());

        const auto lastGeneration = authority->schedule.invocations.back().sourceGeneration;
        base::BinaryWriter history;
        history.WriteBytes(VerifiedLiteralIdentity(
            "SGE4-5.Spiral5.TemporalRetainedHistoryHandle.V1"));
        history.WriteBytes(impl.domainIdentity);
        history.WriteU64(impl.deviceEpoch);
        history.WriteBytes(scheduleIdentity);
        history.WriteU32(lastGeneration);
        for (const auto& candidate : submission.architecture.candidates)
            history.WriteBytes(candidate.invocations.back().retainedHistoryDigest);
        submission.retainedHistory.deviceEpoch = impl.deviceEpoch;
        submission.retainedHistory.scheduleIdentity = scheduleIdentity;
        submission.retainedHistory.sourceGeneration = lastGeneration;
        submission.retainedHistory.historyIdentity = base::Sha256(history.Bytes());
        impl.issuedTokens.push_back(submission.completion);
        impl.issuedHistories.push_back(submission.retainedHistory);

        return base::Result<TemporalRuntimeSubmissionV1, TemporalCandidateFamilyRuntimeErrorV1>::Success(
            std::move(submission));
    }
    catch (const std::exception& error)
    {
        return base::Result<TemporalRuntimeSubmissionV1, TemporalCandidateFamilyRuntimeErrorV1>::Failure(
            RuntimeError("temporal-runtime/submit", error.what()));
    }
#else
    (void)runtime;(void)handle;(void)binding;(void)seed;(void)scheduleIdentity;
    return base::Result<TemporalRuntimeSubmissionV1, TemporalCandidateFamilyRuntimeErrorV1>::Failure(
        RuntimeError("temporal-runtime/platform",
            "Spiral 5 CU5 Temporal submission requires Windows D3D12."));
#endif
}

base::Result<void, TemporalCandidateFamilyRuntimeErrorV1>
ValidateTemporalRuntimeSubmissionTokenV1(
    const LoadedTemporalCandidateFamilyRuntimeV1& runtime,
    const TemporalRuntimeSubmissionTokenV1& token)
{
    const auto& impl = *runtime.impl_;
    if (impl.state != TemporalCandidateFamilyRuntimeStateV1::Active)
    {
        return base::Result<void, TemporalCandidateFamilyRuntimeErrorV1>::Failure(
            RuntimeError("temporal-runtime/device-state",
                "Temporal submission token belongs to an inactive domain."));
    }
    if (token.deviceEpoch != impl.deviceEpoch)
    {
        return base::Result<void, TemporalCandidateFamilyRuntimeErrorV1>::Failure(
            RuntimeError("temporal-runtime/stale-epoch",
                "Temporal submission token belongs to another device epoch."));
    }
    const bool issued = std::any_of(
        impl.issuedTokens.begin(), impl.issuedTokens.end(),
        [&](const TemporalRuntimeSubmissionTokenV1& value)
        {
            return value.deviceEpoch == token.deviceEpoch &&
                value.submissionOrdinal == token.submissionOrdinal &&
                value.tokenIdentity == token.tokenIdentity;
        });
    if (!issued)
    {
        return base::Result<void, TemporalCandidateFamilyRuntimeErrorV1>::Failure(
            RuntimeError("temporal-runtime/token-identity",
                "Temporal submission token is not recognized by this Runtime."));
    }
    return base::Result<void, TemporalCandidateFamilyRuntimeErrorV1>::Success();
}

base::Result<void, TemporalCandidateFamilyRuntimeErrorV1>
ValidateTemporalHistoryHandleV1(
    const LoadedTemporalCandidateFamilyRuntimeV1& runtime,
    const TemporalHistoryHandleV1& history)
{
    const auto& impl = *runtime.impl_;
    if (impl.state != TemporalCandidateFamilyRuntimeStateV1::Active)
    {
        return base::Result<void, TemporalCandidateFamilyRuntimeErrorV1>::Failure(
            RuntimeError("temporal-runtime/device-state",
                "Temporal history belongs to an inactive domain."));
    }
    if (history.deviceEpoch != impl.deviceEpoch)
    {
        return base::Result<void, TemporalCandidateFamilyRuntimeErrorV1>::Failure(
            RuntimeError("temporal-runtime/stale-epoch",
                "Temporal history belongs to another device epoch."));
    }
    const bool issued = std::any_of(
        impl.issuedHistories.begin(), impl.issuedHistories.end(),
        [&](const TemporalHistoryHandleV1& value)
        {
            return value.deviceEpoch == history.deviceEpoch &&
                value.scheduleIdentity == history.scheduleIdentity &&
                value.sourceGeneration == history.sourceGeneration &&
                value.historyIdentity == history.historyIdentity;
        });
    if (!issued)
    {
        return base::Result<void, TemporalCandidateFamilyRuntimeErrorV1>::Failure(
            RuntimeError("temporal-runtime/history-identity",
                "Temporal retained-history handle is not recognized by this Runtime."));
    }
    return base::Result<void, TemporalCandidateFamilyRuntimeErrorV1>::Success();
}

base::Result<TemporalCandidateFamilyRecoveryReportV1, TemporalCandidateFamilyRuntimeErrorV1>
RecoverTemporalCandidateFamilyRuntimeV1(
    LoadedTemporalCandidateFamilyRuntimeV1& runtime,
    TemporalCandidateFamilyRecoveryModeV1 mode)
{
#if defined(_WIN32)
    auto& impl = *runtime.impl_;
    TemporalCandidateFamilyRecoveryReportV1 report;
    report.mode = mode;
    report.stateBefore = impl.state;
    report.stateAfter = impl.state;
    report.previousDeviceEpoch = impl.deviceEpoch;
    report.newDeviceEpoch = impl.deviceEpoch;
    const auto frozenBefore = impl.candidateBindingSetIdentity;

    if (mode == TemporalCandidateFamilyRecoveryModeV1::RetryAdapterReacquisition)
    {
        if (impl.state != TemporalCandidateFamilyRuntimeStateV1::AwaitingAdapter ||
            !impl.hasExcludedAdapterLuid)
        {
            return base::Result<TemporalCandidateFamilyRecoveryReportV1, TemporalCandidateFamilyRuntimeErrorV1>::Failure(
                RuntimeError("temporal-runtime/retry-state",
                    "Adapter retry requires an excluded removed adapter."));
        }
        try
        {
            ComPtr<IDXGIFactory6> factory;
            Check(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)), "CreateDXGIFactory2(retry)");
            ComPtr<IDXGIAdapter> adapter;
            Check(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)), "EnumWarpAdapter(retry)");
            DXGI_ADAPTER_DESC desc{};
            Check(adapter->GetDesc(&desc), "GetDesc(retry)");
            if (SameLuidV1(desc.AdapterLuid, impl.excludedAdapterLuid))
            {
                report.adapterReacquired = false;
                report.stateAfter = impl.state;
                report.dynamicRebindRequired = true;
                report.explicitReseedRequired = true;
                report.frozenAuthorityPreserved =
                    frozenBefore == impl.candidateBindingSetIdentity;
                return base::Result<TemporalCandidateFamilyRecoveryReportV1, TemporalCandidateFamilyRuntimeErrorV1>::Success(report);
            }
            impl.gpu = std::make_unique<GpuContext>();
            ++impl.deviceEpoch;
            impl.state = TemporalCandidateFamilyRuntimeStateV1::Active;
            impl.adapterDescription = impl.gpu->adapterDescription;
            impl.dynamicInputsBound = false;
            impl.historySeeded = false;
            report.adapterReacquired = true;
            report.nativeDeviceRematerialized = true;
            report.executionContextRematerialized = true;
            report.newDeviceEpoch = impl.deviceEpoch;
            report.stateAfter = impl.state;
            report.dynamicRebindRequired = true;
            report.explicitReseedRequired = true;
            report.frozenAuthorityPreserved =
                frozenBefore == impl.candidateBindingSetIdentity;
            return base::Result<TemporalCandidateFamilyRecoveryReportV1, TemporalCandidateFamilyRuntimeErrorV1>::Success(report);
        }
        catch (const std::exception& error)
        {
            return base::Result<TemporalCandidateFamilyRecoveryReportV1, TemporalCandidateFamilyRuntimeErrorV1>::Failure(
                RuntimeError("temporal-runtime/retry", error.what()));
        }
    }

    if (impl.state != TemporalCandidateFamilyRuntimeStateV1::Active || !impl.gpu)
    {
        return base::Result<TemporalCandidateFamilyRecoveryReportV1, TemporalCandidateFamilyRuntimeErrorV1>::Failure(
            RuntimeError("temporal-runtime/device-state",
                "Recovery requires an active Temporal Runtime."));
    }

    report.historyResourcesInvalidated = true;
    report.temporalArgumentsInvalidated = true;
    report.completionStateInvalidated = true;
    report.readbackStateInvalidated = true;
    report.dynamicRebindRequired = true;
    report.explicitReseedRequired = true;

    if (mode == TemporalCandidateFamilyRecoveryModeV1::ControlledRebuild)
    {
        try
        {
            impl.gpu.reset();
            report.allRuntimeObjectsReleased = true;
            impl.gpu = std::make_unique<GpuContext>();
            impl.adapterDescription = impl.gpu->adapterDescription;
            ++impl.deviceEpoch;
            impl.submissionOrdinal = 0;
            impl.dynamicInputsBound = false;
            impl.historySeeded = false;
            impl.currentBinding = {};
            impl.currentSeed = {};
            impl.issuedTokens.clear();
            impl.issuedHistories.clear();
            report.adapterReacquired = true;
            report.nativeDeviceRematerialized = true;
            report.executionContextRematerialized = true;
            report.newDeviceEpoch = impl.deviceEpoch;
            report.stateAfter = impl.state;
            report.frozenAuthorityPreserved =
                frozenBefore == impl.candidateBindingSetIdentity;
            return base::Result<TemporalCandidateFamilyRecoveryReportV1, TemporalCandidateFamilyRuntimeErrorV1>::Success(report);
        }
        catch (const std::exception& error)
        {
            return base::Result<TemporalCandidateFamilyRecoveryReportV1, TemporalCandidateFamilyRuntimeErrorV1>::Failure(
                RuntimeError("temporal-runtime/controlled", error.what()));
        }
    }

    if (mode != TemporalCandidateFamilyRecoveryModeV1::ForceRemovalForTest)
    {
        return base::Result<TemporalCandidateFamilyRecoveryReportV1, TemporalCandidateFamilyRuntimeErrorV1>::Failure(
            RuntimeError("temporal-runtime/recovery-mode", "Unknown Temporal Recovery mode."));
    }

    ComPtr<ID3D12Device5> removable;
    HRESULT hr = impl.gpu->device.As(&removable);
    if (FAILED(hr))
    {
        return base::Result<TemporalCandidateFamilyRecoveryReportV1, TemporalCandidateFamilyRuntimeErrorV1>::Failure(
            RuntimeError("temporal-runtime/query-device5", HResultText(hr), hr));
    }
    removable->RemoveDevice();
    const HRESULT reason = impl.gpu->device->GetDeviceRemovedReason();
    report.removalReason = static_cast<long>(reason);
    if (SUCCEEDED(reason))
    {
        return base::Result<TemporalCandidateFamilyRecoveryReportV1, TemporalCandidateFamilyRuntimeErrorV1>::Failure(
            RuntimeError("temporal-runtime/remove-device",
                "ID3D12Device5::RemoveDevice did not remove the Temporal device."));
    }

    report.forcedRemoval = true;
    impl.excludedAdapterLuid = impl.gpu->adapterLuid;
    impl.hasExcludedAdapterLuid = true;
    report.removedAdapterLuidLow = impl.excludedAdapterLuid.LowPart;
    report.removedAdapterLuidHigh = impl.excludedAdapterLuid.HighPart;
    impl.gpu.reset();
    impl.state = TemporalCandidateFamilyRuntimeStateV1::AwaitingAdapter;
    impl.dynamicInputsBound = false;
    impl.historySeeded = false;
    impl.currentBinding = {};
    impl.currentSeed = {};
    impl.issuedTokens.clear();
    impl.issuedHistories.clear();
    report.allRuntimeObjectsReleased = true;
    report.stateAfter = impl.state;
    report.frozenAuthorityPreserved =
        frozenBefore == impl.candidateBindingSetIdentity;
    return base::Result<TemporalCandidateFamilyRecoveryReportV1, TemporalCandidateFamilyRuntimeErrorV1>::Success(report);
#else
    (void)runtime;(void)mode;
    return base::Result<TemporalCandidateFamilyRecoveryReportV1, TemporalCandidateFamilyRuntimeErrorV1>::Failure(
        RuntimeError("temporal-runtime/platform",
            "Spiral 5 CU5 Recovery requires Windows D3D12."));
#endif
}

std::vector<std::byte> SerializeTemporalCandidateFamilyRuntimeAuthorityV1(
    const LoadedTemporalCandidateFamilyRuntimeV1& runtime)
{
    const auto& impl = *runtime.impl_;
    base::BinaryWriter writer;
    writer.WriteBytes(VerifiedLiteralIdentity(
        "SGE4-5.Spiral5.TemporalCandidateFamilyRuntimeAuthority.V1"));
    writer.WriteBytes(impl.domainIdentity);
    writer.WriteBytes(impl.candidateBindingSetIdentity);
    const auto semanticBytes = semantic::SerializeTemporalStateSemanticV1(impl.semantic);
    writer.WriteU32(static_cast<std::uint32_t>(semanticBytes.size()));
    writer.WriteBytes(semanticBytes);
    writer.WriteBytes(impl.context.targetProfileIdentity);
    writer.WriteBytes(impl.context.observationContractIdentity);
    writer.WriteBytes(impl.context.deviceEpochPolicyIdentity);
    writer.WriteU32(static_cast<std::uint32_t>(impl.authorities.size()));
    for (const auto& authority : impl.authorities)
    {
        const auto scheduleBytes = semantic::SerializeUpdateScheduleV1(authority.schedule);
        writer.WriteU32(static_cast<std::uint32_t>(scheduleBytes.size()));
        writer.WriteBytes(scheduleBytes);
        writer.WriteU32(static_cast<std::uint32_t>(authority.candidates.size()));
        for (const auto& candidate : authority.candidates)
        {
            const auto candidateBytes =
                family_verification::SerializeVerifiedTemporalCandidateV1(candidate);
            writer.WriteU32(static_cast<std::uint32_t>(candidateBytes.size()));
            writer.WriteBytes(candidateBytes);
        }
    }
    return std::move(writer).Take();
}

}
