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
}
