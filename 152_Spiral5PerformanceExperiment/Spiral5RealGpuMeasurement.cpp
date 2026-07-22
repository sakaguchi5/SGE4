#include "Spiral5PerformanceExperiment.h"

#include "../00_Foundation/BinaryIO.h"
#include "../148_TemporalCandidateFamilyPlanner/TemporalCandidateFamilyPlanner.h"
#include "../149_TemporalCandidateFamilyVerifier/TemporalCandidateFamilyVerifier.h"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#endif

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstring>
#include <memory>
#include <iostream>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

#if defined(_WIN32)
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")
#endif

namespace sge4_5::spiral5::performance
{
namespace
{
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
    if (FAILED(hr)) throw std::runtime_error(std::string(stage) + ": " + HResultText(hr));
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
        source.data(), source.size(), "Spiral5PerformanceV1", nullptr, nullptr,
        std::string(entry).c_str(), "cs_5_1", flags, 0, &shader, &errors);
    if (FAILED(hr))
    {
        const std::string message = errors
            ? std::string(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize())
            : HResultText(hr);
        throw std::runtime_error("D3DCompile: " + message);
    }
    return shader;
}

base::Digest256 BlobDigest(ID3DBlob* blob)
{
    return base::Sha256(std::span(
        static_cast<const std::byte*>(blob->GetBufferPointer()), blob->GetBufferSize()));
}

ComPtr<ID3D12RootSignature> CreateRootSignature(
    ID3D12Device* device,
    std::span<const D3D12_ROOT_PARAMETER> parameters)
{
    D3D12_ROOT_SIGNATURE_DESC description{};
    description.NumParameters = static_cast<UINT>(parameters.size());
    description.pParameters = parameters.data();
    description.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
    ComPtr<ID3DBlob> serialized;
    ComPtr<ID3DBlob> errors;
    const HRESULT result = D3D12SerializeRootSignature(
        &description, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors);
    if (FAILED(result))
    {
        const std::string message = errors
            ? std::string(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize())
            : HResultText(result);
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
    D3D12_COMPUTE_PIPELINE_STATE_DESC description{};
    description.pRootSignature = root;
    description.CS = {shader->GetBufferPointer(), shader->GetBufferSize()};
    ComPtr<ID3D12PipelineState> pipeline;
    Check(device->CreateComputePipelineState(&description, IID_PPV_ARGS(&pipeline)),
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
    D3D12_RESOURCE_DESC description{};
    description.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    description.Width = size;
    description.Height = 1;
    description.DepthOrArraySize = 1;
    description.MipLevels = 1;
    description.SampleDesc.Count = 1;
    description.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    description.Flags = flags;
    ComPtr<ID3D12Resource> resource;
    Check(device->CreateCommittedResource(
        &heap, D3D12_HEAP_FLAG_NONE, &description, state, nullptr, IID_PPV_ARGS(&resource)),
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

std::string ArgumentProducerShader()
{
    return R"(
cbuffer Invocation : register(b0) { uint dispatchX; };
RWByteAddressBuffer Arguments : register(u0);
[numthreads(1,1,1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (any(dispatchThreadId != 0)) return;
    Arguments.Store(0, dispatchX);
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

std::uint32_t OrderedFloatBits(float value)
{
    const auto bits = std::bit_cast<std::uint32_t>(value);
    if ((bits & 0x7fffffffu) == 0u) return 0x80000000u;
    return (bits & 0x80000000u) != 0u ? ~bits : (bits | 0x80000000u);
}

std::uint32_t UlpDistance(float a, float b)
{
    const auto aa = OrderedFloatBits(a);
    const auto bb = OrderedFloatBits(b);
    return aa > bb ? aa - bb : bb - aa;
}

struct DeviceContext final
{
    ComPtr<IDXGIFactory6> factory;
    ComPtr<IDXGIAdapter1> adapter;
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> commandList;
    ComPtr<ID3D12Fence> fence;
    EventHandle fenceEvent;
    std::uint64_t fenceValue = 0;
    std::uint64_t timestampFrequency = 0;
    AdapterProfileV1 profile{};

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

    explicit DeviceContext(std::uint32_t adapterIndex)
    {
        Check(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)), "CreateDXGIFactory2");
        std::vector<ComPtr<IDXGIAdapter1>> eligible;
        for (UINT index = 0;; ++index)
        {
            ComPtr<IDXGIAdapter1> candidate;
            const HRESULT result = factory->EnumAdapterByGpuPreference(
                index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&candidate));
            if (result == DXGI_ERROR_NOT_FOUND) break;
            if (FAILED(result)) continue;
            DXGI_ADAPTER_DESC1 description{};
            if (FAILED(candidate->GetDesc1(&description))) continue;
            if ((description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0) continue;
            ComPtr<ID3D12Device> probe;
            if (FAILED(D3D12CreateDevice(candidate.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&probe)))) continue;
            eligible.push_back(std::move(candidate));
        }
        if (adapterIndex >= eligible.size())
        {
            throw std::runtime_error("Requested eligible hardware adapter index is unavailable.");
        }
        adapter = eligible[adapterIndex];
        Check(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)),
            "D3D12CreateDevice");

        D3D12_COMMAND_QUEUE_DESC queueDescription{};
        queueDescription.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
        Check(device->CreateCommandQueue(&queueDescription, IID_PPV_ARGS(&queue)), "CreateCommandQueue");
        Check(queue->GetTimestampFrequency(&timestampFrequency), "GetTimestampFrequency");
        Check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&allocator)),
            "CreateCommandAllocator");
        Check(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, allocator.Get(), nullptr,
            IID_PPV_ARGS(&commandList)), "CreateCommandList");
        Check(commandList->Close(), "Close(initial command list)");
        Check(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)), "CreateFence");

        argumentShader = CompileShader(ArgumentProducerShader(), "main");
        hierarchyShader = CompileShader(HierarchyShader(), "main");
        consumerShader = CompileShader(ConsumerShader(), "main");

        D3D12_ROOT_PARAMETER argumentParameters[2]{};
        argumentParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        argumentParameters[0].Constants.ShaderRegister = 0;
        argumentParameters[0].Constants.Num32BitValues = 1;
        argumentParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
        argumentParameters[1].Descriptor.ShaderRegister = 0;
        argumentRoot = CreateRootSignature(device.Get(), argumentParameters);

        D3D12_ROOT_PARAMETER hierarchyParameters[3]{};
        hierarchyParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        hierarchyParameters[0].Constants.ShaderRegister = 0;
        hierarchyParameters[0].Constants.Num32BitValues = 1;
        hierarchyParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        hierarchyParameters[1].Descriptor.ShaderRegister = 0;
        hierarchyParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
        hierarchyParameters[2].Descriptor.ShaderRegister = 0;
        hierarchyRoot = CreateRootSignature(device.Get(), hierarchyParameters);

        D3D12_ROOT_PARAMETER consumerParameters[3]{};
        consumerParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        consumerParameters[0].Descriptor.ShaderRegister = 0;
        consumerParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        consumerParameters[1].Descriptor.ShaderRegister = 1;
        consumerParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
        consumerParameters[2].Descriptor.ShaderRegister = 0;
        consumerRoot = CreateRootSignature(device.Get(), consumerParameters);

        argumentPipeline = CreateComputePipeline(device.Get(), argumentRoot.Get(), argumentShader.Get());
        hierarchyPipeline = CreateComputePipeline(device.Get(), hierarchyRoot.Get(), hierarchyShader.Get());
        consumerPipeline = CreateComputePipeline(device.Get(), consumerRoot.Get(), consumerShader.Get());

        D3D12_INDIRECT_ARGUMENT_DESC argumentDescription{};
        argumentDescription.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
        D3D12_COMMAND_SIGNATURE_DESC signatureDescription{};
        signatureDescription.ByteStride = 12;
        signatureDescription.NumArgumentDescs = 1;
        signatureDescription.pArgumentDescs = &argumentDescription;
        Check(device->CreateCommandSignature(&signatureDescription, nullptr, IID_PPV_ARGS(&dispatchSignature)),
            "CreateCommandSignature");

        DXGI_ADAPTER_DESC1 adapterDescription{};
        Check(adapter->GetDesc1(&adapterDescription), "GetDesc1");
        profile.description = WideToUtf8(adapterDescription.Description);
        profile.vendorId = adapterDescription.VendorId;
        profile.deviceId = adapterDescription.DeviceId;
        profile.subsystemId = adapterDescription.SubSysId;
        profile.revision = adapterDescription.Revision;
        profile.luidLow = adapterDescription.AdapterLuid.LowPart;
        profile.luidHigh = adapterDescription.AdapterLuid.HighPart;
        profile.timestampFrequency = timestampFrequency;
        LARGE_INTEGER driverVersion{};
        if (SUCCEEDED(adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &driverVersion)))
        {
            profile.driverVersion = static_cast<std::uint64_t>(driverVersion.QuadPart);
        }
        D3D12_FEATURE_DATA_ARCHITECTURE1 architecture{};
        architecture.NodeIndex = 0;
        if (SUCCEEDED(device->CheckFeatureSupport(
            D3D12_FEATURE_ARCHITECTURE1, &architecture, sizeof(architecture))))
        {
            profile.uma = architecture.UMA != FALSE;
            profile.cacheCoherentUma = architecture.CacheCoherentUMA != FALSE;
        }
        base::BinaryWriter fingerprint;
        fingerprint.WriteU32(profile.vendorId); fingerprint.WriteU32(profile.deviceId);
        fingerprint.WriteU32(profile.subsystemId); fingerprint.WriteU32(profile.revision);
        fingerprint.WriteU32(profile.luidLow); fingerprint.WriteI32(profile.luidHigh);
        fingerprint.WriteU64(profile.driverVersion); fingerprint.WriteU64(profile.timestampFrequency);
        fingerprint.WriteU8(profile.uma ? 1u : 0u); fingerprint.WriteU8(profile.cacheCoherentUma ? 1u : 0u);
        fingerprint.WriteBytes(std::as_bytes(std::span(profile.description.data(), profile.description.size())));
        profile.fingerprint = base::Sha256(fingerprint.Bytes());
    }

    void Begin()
    {
        Check(allocator->Reset(), "Reset allocator");
        Check(commandList->Reset(allocator.Get(), nullptr), "Reset command list");
    }

    void SubmitAndWait()
    {
        Check(commandList->Close(), "Close command list");
        ID3D12CommandList* lists[]{commandList.Get()};
        queue->ExecuteCommandLists(1, lists);
        const std::uint64_t value = ++fenceValue;
        Check(queue->Signal(fence.Get(), value), "Signal fence");
        if (fence->GetCompletedValue() < value)
        {
            Check(fence->SetEventOnCompletion(value, fenceEvent.value), "SetEventOnCompletion");
            WaitForSingleObject(fenceEvent.value, INFINITE);
        }
    }
};

struct InvocationInput final
{
    bool update = false;
    std::uint32_t sourceGeneration = 0;
};

class CandidateSession final
{
public:
    CandidateSession(DeviceContext& device, CandidateKindV1 kind)
        : device_(device), kind_(kind)
    {
        constexpr std::uint64_t argumentBytes = 12;
        constexpr std::uint64_t globalBytes = semantic::BoneCountV1 * sizeof(semantic::PgaMotorRecordV1);
        constexpr std::uint64_t outputBytes = semantic::WorkCountV1 * sizeof(semantic::PointRecordV1);
        std::vector<semantic::PgaMotorRecordV1> allLocalMotors;
        allLocalMotors.reserve(semantic::TimelineLengthV1 * semantic::BoneCountV1);
        for (std::uint32_t generation = 0; generation < semantic::TimelineLengthV1; ++generation)
        {
            const auto motors = semantic::BuildCanonicalLocalMotorGenerationV1(generation);
            allLocalMotors.insert(allLocalMotors.end(), motors.begin(), motors.end());
        }
        const auto points = semantic::BuildCanonicalPointCorpusV1();
        const auto localBytes = std::as_bytes(std::span(allLocalMotors));
        const auto pointBytes = std::as_bytes(std::span(points));
        localUpload_ = CreateBuffer(device_.device.Get(), D3D12_HEAP_TYPE_UPLOAD, localBytes.size(),
            D3D12_RESOURCE_STATE_GENERIC_READ);
        pointUpload_ = CreateBuffer(device_.device.Get(), D3D12_HEAP_TYPE_UPLOAD, pointBytes.size(),
            D3D12_RESOURCE_STATE_GENERIC_READ);
        UploadBytes(localUpload_.Get(), localBytes);
        UploadBytes(pointUpload_.Get(), pointBytes);
        if (kind_ != CandidateKindV1::EveryInvocationRecompute)
        {
            hierarchyArguments_ = CreateBuffer(device_.device.Get(), D3D12_HEAP_TYPE_DEFAULT, argumentBytes,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        }
        if (kind_ == CandidateKindV1::FinalOutputHistoryReuse)
        {
            consumerArguments_ = CreateBuffer(device_.device.Get(), D3D12_HEAP_TYPE_DEFAULT, argumentBytes,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        }
        global_ = CreateBuffer(device_.device.Get(), D3D12_HEAP_TYPE_DEFAULT, globalBytes,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        output_ = CreateBuffer(device_.device.Get(), D3D12_HEAP_TYPE_DEFAULT, outputBytes,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        outputReadback_ = CreateBuffer(device_.device.Get(), D3D12_HEAP_TYPE_READBACK, outputBytes,
            D3D12_RESOURCE_STATE_COPY_DEST);
        D3D12_QUERY_HEAP_DESC queryDescription{};
        queryDescription.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
        queryDescription.Count = QueryCount;
        Check(device_.device->CreateQueryHeap(&queryDescription, IID_PPV_ARGS(&queryHeap_)),
            "CreateQueryHeap");
        queryReadback_ = CreateBuffer(device_.device.Get(), D3D12_HEAP_TYPE_READBACK,
            static_cast<std::uint64_t>(QueryCount) * sizeof(std::uint64_t),
            D3D12_RESOURCE_STATE_COPY_DEST);
    }

    TimingSampleV1 Measure(
        const semantic::UpdateScheduleV1& schedule,
        std::uint32_t run,
        std::uint32_t orderIndex,
        std::uint32_t cycle,
        std::uint32_t orderPosition,
        std::uint32_t updateInterval)
    {
        using Clock = std::chrono::steady_clock;
        const auto cpuBegin = Clock::now();
        const auto preparationBegin = Clock::now();
        std::vector<InvocationInput> invocations;
        invocations.reserve(schedule.invocations.size());
        for (const auto& invocation : schedule.invocations)
        {
            invocations.push_back(InvocationInput{
                invocation.event == semantic::UpdateEventV1::Update,
                invocation.sourceGeneration});
        }
        const auto preparationEnd = Clock::now();

        const auto recordingBegin = Clock::now();
        device_.Begin();
        for (std::uint32_t invocationIndex = 0; invocationIndex < semantic::TimelineLengthV1; ++invocationIndex)
        {
            const auto& invocation = invocations[invocationIndex];
            const std::uint32_t hierarchyDispatch =
                kind_ == CandidateKindV1::EveryInvocationRecompute ? 1u : (invocation.update ? 1u : 0u);
            const std::uint32_t consumerDispatch =
                kind_ == CandidateKindV1::FinalOutputHistoryReuse ? (invocation.update ? 64u : 0u) : 64u;
            const std::uint32_t queryBase = invocationIndex * QueriesPerInvocation;
            device_.commandList->EndQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, queryBase + 0u);

            if (hierarchyArguments_)
            {
                RecordArgument(hierarchyArguments_.Get(), hierarchyDispatch);
            }
            if (consumerArguments_)
            {
                RecordArgument(consumerArguments_.Get(), consumerDispatch);
            }
            device_.commandList->EndQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, queryBase + 1u);

            if (globalState_ != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
            {
                const auto toUav = Transition(global_.Get(), globalState_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                device_.commandList->ResourceBarrier(1, &toUav);
                globalState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            }
            device_.commandList->SetPipelineState(device_.hierarchyPipeline.Get());
            device_.commandList->SetComputeRootSignature(device_.hierarchyRoot.Get());
            device_.commandList->SetComputeRoot32BitConstant(0, invocation.sourceGeneration, 0);
            device_.commandList->SetComputeRootShaderResourceView(1, localUpload_->GetGPUVirtualAddress());
            device_.commandList->SetComputeRootUnorderedAccessView(2, global_->GetGPUVirtualAddress());
            if (hierarchyArguments_)
            {
                device_.commandList->ExecuteIndirect(
                    device_.dispatchSignature.Get(), 1, hierarchyArguments_.Get(), 0, nullptr, 0);
                const auto back = Transition(hierarchyArguments_.Get(),
                    D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                device_.commandList->ResourceBarrier(1, &back);
            }
            else
            {
                device_.commandList->Dispatch(hierarchyDispatch, 1, 1);
            }
            const auto globalUav = UavBarrier(global_.Get());
            device_.commandList->ResourceBarrier(1, &globalUav);
            const auto toRead = Transition(global_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            device_.commandList->ResourceBarrier(1, &toRead);
            globalState_ = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
            device_.commandList->EndQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, queryBase + 2u);

            device_.commandList->SetPipelineState(device_.consumerPipeline.Get());
            device_.commandList->SetComputeRootSignature(device_.consumerRoot.Get());
            device_.commandList->SetComputeRootShaderResourceView(0, global_->GetGPUVirtualAddress());
            device_.commandList->SetComputeRootShaderResourceView(1, pointUpload_->GetGPUVirtualAddress());
            device_.commandList->SetComputeRootUnorderedAccessView(2, output_->GetGPUVirtualAddress());
            if (consumerArguments_)
            {
                device_.commandList->ExecuteIndirect(
                    device_.dispatchSignature.Get(), 1, consumerArguments_.Get(), 0, nullptr, 0);
                const auto back = Transition(consumerArguments_.Get(),
                    D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                device_.commandList->ResourceBarrier(1, &back);
            }
            else
            {
                device_.commandList->Dispatch(consumerDispatch, 1, 1);
            }
            device_.commandList->EndQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, queryBase + 3u);

            if (consumerDispatch != 0)
            {
                const auto completion = UavBarrier(output_.Get());
                device_.commandList->ResourceBarrier(1, &completion);
            }
            device_.commandList->EndQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, queryBase + 4u);
        }

        device_.commandList->EndQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, ObservationQueryStart);
        const auto toCopy = Transition(output_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COPY_SOURCE);
        device_.commandList->ResourceBarrier(1, &toCopy);
        device_.commandList->CopyBufferRegion(outputReadback_.Get(), 0, output_.Get(), 0, OutputBytes);
        const auto back = Transition(output_.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        device_.commandList->ResourceBarrier(1, &back);
        device_.commandList->EndQuery(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, ObservationQueryEnd);
        device_.commandList->ResolveQueryData(queryHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP,
            0, QueryCount, queryReadback_.Get(), 0);
        const auto recordingEnd = Clock::now();

        const auto submitBegin = Clock::now();
        device_.SubmitAndWait();
        const auto submitEnd = Clock::now();

        const auto observationBegin = Clock::now();
        const auto outputBytes = ReadbackBytes(outputReadback_.Get(), OutputBytes);
        const auto queryBytes = ReadbackBytes(queryReadback_.Get(),
            static_cast<std::size_t>(QueryCount) * sizeof(std::uint64_t));
        std::array<std::uint64_t, QueryCount> timestamps{};
        std::memcpy(timestamps.data(), queryBytes.data(), queryBytes.size());

        const std::uint32_t finalGeneration = schedule.invocations.back().sourceGeneration;
        const auto local = semantic::BuildCanonicalLocalMotorGenerationV1(finalGeneration);
        const auto global = semantic::ComposeCanonicalGlobalMotorsCpuV1(local);
        const auto points = semantic::BuildCanonicalPointCorpusV1();
        const auto expected = semantic::ApplyGlobalMotorsCpuV1(global, points);
        std::vector<semantic::PointRecordV1> actual(semantic::WorkCountV1);
        std::memcpy(actual.data(), outputBytes.data(), outputBytes.size());

        TimingSampleV1 sample;
        sample.run = run; sample.orderIndex = orderIndex; sample.cycle = cycle;
        sample.orderPosition = orderPosition; sample.candidate = kind_; sample.updateInterval = updateInterval;
        sample.outputDigest = base::Sha256(outputBytes);
        for (std::size_t i = 0; i < actual.size(); ++i)
        {
            const float* a = reinterpret_cast<const float*>(&actual[i]);
            const float* e = reinterpret_cast<const float*>(&expected[i]);
            bool mismatch = false;
            for (std::size_t component = 0; component < 4; ++component)
            {
                if (!std::isfinite(a[component])) ++sample.nonFiniteCount;
                if (component == 3 && std::bit_cast<std::uint32_t>(a[component]) != 0x3f800000u)
                {
                    ++sample.homogeneousCoordinateMismatchCount;
                }
                const double absolute = std::abs(static_cast<double>(a[component]) - e[component]);
                const double scale = std::max(1.0, std::abs(static_cast<double>(e[component])));
                sample.maxAbsoluteError = std::max(sample.maxAbsoluteError, absolute);
                sample.maxRelativeError = std::max(sample.maxRelativeError, absolute / scale);
                sample.maxUlpError = std::max(sample.maxUlpError, UlpDistance(a[component], e[component]));
                if (!std::isfinite(a[component]) || absolute > 1.0e-6 * scale + 1.0e-7) mismatch = true;
            }
            if (mismatch) ++sample.mismatchCount;
        }
        const auto observationEnd = Clock::now();
        const auto cpuEnd = observationEnd;

        const auto ticksToNs = [&](std::uint64_t begin, std::uint64_t end) -> double
        {
            if (end < begin) throw std::runtime_error("GPU timestamp order is invalid.");
            return static_cast<double>(end - begin) * 1.0e9 /
                static_cast<double>(device_.timestampFrequency);
        };
        for (std::uint32_t invocationIndex = 0; invocationIndex < semantic::TimelineLengthV1; ++invocationIndex)
        {
            const std::uint32_t queryBase = invocationIndex * QueriesPerInvocation;
            const double argument = ticksToNs(timestamps[queryBase], timestamps[queryBase + 1u]);
            const double hierarchy = ticksToNs(timestamps[queryBase + 1u], timestamps[queryBase + 2u]);
            const double consumer = ticksToNs(timestamps[queryBase + 2u], timestamps[queryBase + 3u]);
            const double completion = ticksToNs(timestamps[queryBase + 3u], timestamps[queryBase + 4u]);
            const double path = argument + hierarchy + consumer + completion;
            sample.gpuArgumentGenerationNanoseconds += argument;
            sample.gpuHierarchyExecutionNanoseconds += hierarchy;
            sample.gpuConsumerExecutionNanoseconds += consumer;
            sample.gpuRetainedCompletionNanoseconds += completion;
            if (invocations[invocationIndex].update)
            {
                ++sample.updateInvocationCount;
                sample.gpuUpdatePathNanoseconds += path;
            }
            else
            {
                ++sample.holdInvocationCount;
                sample.gpuHoldPathNanoseconds += path;
            }
        }
        sample.gpuCandidateTimelineNanoseconds =
            sample.gpuArgumentGenerationNanoseconds +
            sample.gpuHierarchyExecutionNanoseconds +
            sample.gpuConsumerExecutionNanoseconds +
            sample.gpuRetainedCompletionNanoseconds;
        sample.gpuObservationCopyNanoseconds = ticksToNs(
            timestamps[ObservationQueryStart], timestamps[ObservationQueryEnd]);
        sample.gpuTotalNanoseconds = ticksToNs(timestamps[0], timestamps[ObservationQueryEnd]);
        sample.temporalInputPreparationNanoseconds =
            std::chrono::duration<double, std::nano>(preparationEnd - preparationBegin).count();
        sample.commandRecordingNanoseconds =
            std::chrono::duration<double, std::nano>(recordingEnd - recordingBegin).count();
        sample.submitAndWaitNanoseconds =
            std::chrono::duration<double, std::nano>(submitEnd - submitBegin).count();
        sample.numericObservationNanoseconds =
            std::chrono::duration<double, std::nano>(observationEnd - observationBegin).count();
        sample.cpuEndToEndNanoseconds =
            std::chrono::duration<double, std::nano>(cpuEnd - cpuBegin).count();
        if (sample.mismatchCount != 0 || sample.nonFiniteCount != 0 ||
            sample.homogeneousCoordinateMismatchCount != 0)
        {
            throw std::runtime_error("Real GPU Temporal Candidate output failed the Observation Contract.");
        }
        return sample;
    }

private:
    void RecordArgument(ID3D12Resource* argument, std::uint32_t dispatchX)
    {
        device_.commandList->SetPipelineState(device_.argumentPipeline.Get());
        device_.commandList->SetComputeRootSignature(device_.argumentRoot.Get());
        device_.commandList->SetComputeRoot32BitConstant(0, dispatchX, 0);
        device_.commandList->SetComputeRootUnorderedAccessView(1, argument->GetGPUVirtualAddress());
        device_.commandList->Dispatch(1, 1, 1);
        const auto uav = UavBarrier(argument);
        device_.commandList->ResourceBarrier(1, &uav);
        const auto toIndirect = Transition(argument, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
        device_.commandList->ResourceBarrier(1, &toIndirect);
    }

    static constexpr std::uint32_t QueriesPerInvocation = 5;
    static constexpr std::uint32_t ObservationQueryStart =
        semantic::TimelineLengthV1 * QueriesPerInvocation;
    static constexpr std::uint32_t ObservationQueryEnd = ObservationQueryStart + 1;
    static constexpr std::uint32_t QueryCount = ObservationQueryEnd + 1;
    static constexpr std::uint64_t OutputBytes =
        semantic::WorkCountV1 * sizeof(semantic::PointRecordV1);

    DeviceContext& device_;
    CandidateKindV1 kind_;
    ComPtr<ID3D12Resource> localUpload_;
    ComPtr<ID3D12Resource> pointUpload_;
    ComPtr<ID3D12Resource> hierarchyArguments_;
    ComPtr<ID3D12Resource> consumerArguments_;
    ComPtr<ID3D12Resource> global_;
    ComPtr<ID3D12Resource> output_;
    ComPtr<ID3D12Resource> outputReadback_;
    ComPtr<ID3D12QueryHeap> queryHeap_;
    ComPtr<ID3D12Resource> queryReadback_;
    D3D12_RESOURCE_STATES globalState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
};

CandidateProfileV1 BuildCandidateProfile(DeviceContext& device, CandidateKindV1 kind)
{
    CandidateProfileV1 profile;
    profile.kind = kind;
    if (kind == CandidateKindV1::EveryInvocationRecompute)
    {
        profile.historyRole = family_candidate::TemporalFamilyHistoryRoleV1::None;
        profile.historyDepth = 0; profile.historyBytes = 0;
    }
    else if (kind == CandidateKindV1::GlobalMotorHistoryReuse)
    {
        profile.historyRole = family_candidate::TemporalFamilyHistoryRoleV1::GlobalMotorHistory;
        profile.historyDepth = 1; profile.historyBytes = 256;
    }
    else
    {
        profile.historyRole = family_candidate::TemporalFamilyHistoryRoleV1::FinalOutputHistory;
        profile.historyDepth = 1; profile.historyBytes = 65536;
    }
    profile.targetProfileIdentity = family_candidate::TemporalFamilyTargetProfileIdentityV1();
    profile.observationContractIdentity = family_candidate::TemporalFamilyObservationContractIdentityV1();
    profile.argumentProducerProgramIdentity = family_candidate::TemporalFamilyArgumentProducerProgramIdentityV1();
    profile.hierarchyProgramIdentity = family_candidate::TemporalFamilyHierarchyProgramIdentityV1();
    profile.consumerProgramIdentity = family_candidate::TemporalFamilyConsumerProgramIdentityV1();
    profile.argumentProducerShaderBytecodeDigest = BlobDigest(device.argumentShader.Get());
    profile.hierarchyShaderBytecodeDigest = BlobDigest(device.hierarchyShader.Get());
    profile.consumerShaderBytecodeDigest = BlobDigest(device.consumerShader.Get());
    return profile;
}

base::Digest256 DigestFromHexLocal(std::string_view value)
{
    auto nibble = [](char c) -> std::uint8_t
    {
        if (c >= '0' && c <= '9') return static_cast<std::uint8_t>(c - '0');
        if (c >= 'a' && c <= 'f') return static_cast<std::uint8_t>(10 + c - 'a');
        if (c >= 'A' && c <= 'F') return static_cast<std::uint8_t>(10 + c - 'A');
        throw std::runtime_error("Invalid digest hex.");
    };
    if (value.size() != 64) throw std::runtime_error("Invalid digest hex length.");
    base::Digest256 result{};
    for (std::size_t i = 0; i < result.size(); ++i)
    {
        result[i] = static_cast<std::byte>((nibble(value[i * 2]) << 4u) | nibble(value[i * 2 + 1]));
    }
    return result;
}
#endif
}

base::Result<MeasurementEvidenceV1, std::string>
RunRealGpuMeasurementV1(const MeasurementConfigV1& config)
{
#if !defined(_WIN32)
    (void)config;
    return base::Result<MeasurementEvidenceV1, std::string>::Failure(
        "Real GPU measurement requires Windows D3D12.");
#else
    try
    {
        if (config.measurementTimelinesPerOrder == 0 || config.runCount == 0 ||
            config.primaryMetric != "GpuCandidateTimelineNanoseconds")
        {
            throw std::runtime_error("Real GPU measurement configuration is invalid.");
        }
        DeviceContext device(config.adapterIndex);
        const auto semanticValue = semantic::BuildCanonicalTemporalStateSemanticV1();
        const auto verificationContext =
            family_verification::BuildCanonicalTemporalCandidateFamilyVerificationContextV1();
        const auto kinds = CanonicalCandidateKindsV1();
        const auto orders = CanonicalBalancedOrdersV1();
        std::array<std::unique_ptr<CandidateSession>, CanonicalCandidateCountV1> sessions;
        for (std::size_t i = 0; i < kinds.size(); ++i)
        {
            sessions[i] = std::make_unique<CandidateSession>(device, kinds[i]);
        }

        MeasurementEvidenceV1 evidence;
        evidence.captureUnixNanoseconds = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        evidence.config = config;
        evidence.adapter = device.profile;
        evidence.architectureEvidenceIdentity = DigestFromHexLocal(
            "EB4937B1181E3377FD5367D02F218C682AA1F402A5C4099D8B5F452C103D84A1");
        evidence.controlledRecoveryEvidenceIdentity = DigestFromHexLocal(
            "6DFBA1F98F1338BE96E7985C7532A20A1B4144B81408BEA0FFD8C03ADCA4DB17");
        evidence.freshRematerializationEvidenceIdentity = DigestFromHexLocal(
            "6B0D6F9A617D257268C84AFF523FBA6D4F9A727A0EA130B956E7DB992123300B");
        evidence.semanticIdentity = semantic::TemporalStateSemanticIdentityV1(semanticValue);
        evidence.measurementProfileIdentity = BuildMeasurementProfileIdentityV1(config);
        evidence.orderBalanceIdentity = BuildOrderBalanceIdentityV1();
        for (std::size_t i = 0; i < kinds.size(); ++i)
        {
            evidence.candidates[i] = BuildCandidateProfile(device, kinds[i]);
        }

        for (std::uint32_t updateInterval : CanonicalUpdateIntervalsV1())
        {
            auto scheduleResult = semantic::BuildPeriodicUpdateScheduleV1(updateInterval);
            if (!scheduleResult)
            {
                throw std::runtime_error(scheduleResult.Error().stage + ": " + scheduleResult.Error().message);
            }
            const auto schedule = std::move(scheduleResult).Value();
            auto proposals = family_planner::PlanCanonicalTemporalCandidateFamilyV1(semanticValue, schedule);
            if (!proposals) throw std::runtime_error(proposals.Error());
            MeasurementCaseV1 measurementCase;
            measurementCase.updateInterval = updateInterval;
            measurementCase.scheduleIdentity = semantic::UpdateScheduleIdentityV1(schedule);
            for (std::size_t i = 0; i < kinds.size(); ++i)
            {
                auto verified = family_verification::VerifyTemporalCandidateV1(
                    semanticValue, schedule, verificationContext, proposals.Value()[i]);
                if (!verified) throw std::runtime_error(verified.Error());
                auto& authority = measurementCase.authorities[i];
                authority.kind = kinds[i];
                authority.rawCandidateIdentity = proposals.Value()[i].rawCandidateIdentity;
                authority.verifiedPlanIdentity = verified.Value().Plan().planIdentity;
                authority.verifierCertificateIdentity = verified.Value().CertificateIdentity();
                authority.artifactIdentity = verified.Value().Plan().operation.artifactIdentity;
                authority.historyResourceIdentity = verified.Value().Plan().operation.historyResourceIdentity;
                authority.invocationAuthorityIdentity = verified.Value().Plan().operation.invocationAuthorityIdentity;
            }

            base::Digest256 expectedOutputDigest{};
            bool hasExpectedOutputDigest = false;
            for (std::uint32_t run = 0; run < config.runCount; ++run)
            {
                for (std::uint32_t orderIndex = 0; orderIndex < orders.size(); ++orderIndex)
                {
                    const auto& order = orders[orderIndex];
                    for (std::uint32_t warmup = 0; warmup < config.warmupTimelinesPerOrder; ++warmup)
                    {
                        for (std::uint32_t position = 0; position < order.size(); ++position)
                        {
                            (void)sessions[order[position]]->Measure(
                                schedule, run, orderIndex, warmup, position, updateInterval);
                        }
                    }
                    for (std::uint32_t cycle = 0; cycle < config.measurementTimelinesPerOrder; ++cycle)
                    {
                        for (std::uint32_t position = 0; position < order.size(); ++position)
                        {
                            TimingSampleV1 sample = sessions[order[position]]->Measure(
                                schedule, run, orderIndex, cycle, position, updateInterval);
                            if (!hasExpectedOutputDigest)
                            {
                                expectedOutputDigest = sample.outputDigest;
                                hasExpectedOutputDigest = true;
                            }
                            else if (sample.outputDigest != expectedOutputDigest)
                            {
                                throw std::runtime_error(
                                    "A/B/C output digest differs within one measured update interval.");
                            }
                            measurementCase.samples.push_back(std::move(sample));
                        }
                    }
                }
            }
            const std::uint32_t expectedSamples = CanonicalOrderCountV1 *
                config.measurementTimelinesPerOrder * config.runCount * CanonicalCandidateCountV1;
            if (measurementCase.samples.size() != expectedSamples)
            {
                throw std::runtime_error("Measured sample count differs from the balanced profile.");
            }
            std::cout << "[MEASURED] U=" << updateInterval
                      << " samples=" << measurementCase.samples.size() << '\n';
            evidence.cases.push_back(std::move(measurementCase));
        }
        return base::Result<MeasurementEvidenceV1, std::string>::Success(std::move(evidence));
    }
    catch (const std::exception& error)
    {
        return base::Result<MeasurementEvidenceV1, std::string>::Failure(error.what());
    }
#endif
}
}
