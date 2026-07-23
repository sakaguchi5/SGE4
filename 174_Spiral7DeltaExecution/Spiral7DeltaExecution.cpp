#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "Spiral7DeltaExecution.h"

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
#include <span>
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

namespace sge4_5::spiral7::execution
{
namespace
{
DeltaExecutionErrorV1 Error(std::string stage, std::string message, long nativeCode = 0)
{
    return {std::move(stage), std::move(message), nativeCode};
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

ComPtr<ID3DBlob> CompileShader(std::string_view source)
{
    ComPtr<ID3DBlob> shader;
    ComPtr<ID3DBlob> errors;
    const UINT flags = D3DCOMPILE_ENABLE_STRICTNESS |
                       D3DCOMPILE_WARNINGS_ARE_ERRORS |
                       D3DCOMPILE_OPTIMIZATION_LEVEL3;
    const HRESULT hr = D3DCompile(
        source.data(), source.size(), "Spiral7CompactDeltaHistoryV1", nullptr, nullptr,
        "main", "cs_5_1", flags, 0, &shader, &errors);
    if (FAILED(hr))
    {
        const std::string message = errors
            ? std::string(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize())
            : HResultText(hr);
        throw std::runtime_error("D3DCompile: " + message);
    }
    return shader;
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
    heap.CreationNodeMask = 1;
    heap.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = size;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
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

std::vector<std::byte> ReadbackBytes(ID3D12Resource* resource, std::size_t size)
{
    void* mapped = nullptr;
    D3D12_RANGE readRange{0, size};
    Check(resource->Map(0, &readRange, &mapped), "Map(readback)");
    const auto* first = static_cast<const std::byte*>(mapped);
    std::vector<std::byte> bytes(first, first + size);
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

std::string CompactDeltaShader()
{
    return R"(
struct Motor { float4 qr; float4 qd; };
cbuffer DeltaParameters : register(b0) { uint transitionCount; };
StructuredBuffer<uint2> DeltaRecords : register(t0);
StructuredBuffer<Motor> GlobalMotors : register(t1);
StructuredBuffer<float4> Points : register(t2);
RWStructuredBuffer<float4> Output : register(u0);
RWStructuredBuffer<uint> TransitionAudit : register(u1);
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
    uint ordinal = dispatchThreadId.x;
    if (ordinal >= transitionCount) return;
    uint2 record = DeltaRecords[ordinal];
    uint universeIndex = record.x;
    uint action = record.y;
    if (universeIndex >= 4096) return;
    if (action == 2)
    {
        TransitionAudit[universeIndex] = 2;
        Output[universeIndex] = float4(0,0,0,0);
        return;
    }
    if (action != 1) return;
    TransitionAudit[universeIndex] = 1;
    uint bone = universeIndex / 512;
    Motor motor = GlobalMotors[bone];
    float4 inputPoint = Points[universeIndex];
    float4 conjugate = qconj(motor.qr);
    float4 rotated = qmul(qmul(motor.qr, float4(inputPoint.xyz,0)), conjugate);
    float3 translation = 2.0f * qmul(motor.qd, conjugate).xyz;
    Output[universeIndex] = float4(rotated.xyz + translation, 1.0f);
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
    ComPtr<ID3D12RootSignature> rootSignature;
    ComPtr<ID3D12PipelineState> pipeline;
    ComPtr<ID3D12CommandSignature> dispatchSignature;

    GpuContext()
    {
        Check(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)), "CreateDXGIFactory2");
        ComPtr<IDXGIAdapter1> warp;
        Check(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp)), "EnumWarpAdapter");
        adapter = warp;
        DXGI_ADAPTER_DESC1 description{};
        Check(warp->GetDesc1(&description), "GetDesc1(WARP)");
        adapterDescription = WideToUtf8(description.Description);
        Check(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)),
              "D3D12CreateDevice(WARP)");

        D3D12_COMMAND_QUEUE_DESC queueDesc{};
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        Check(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue)), "CreateCommandQueue");
        Check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)),
              "CreateCommandAllocator");
        Check(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr,
                                        IID_PPV_ARGS(&commandList)), "CreateCommandList");
        Check(commandList->Close(), "Initial Close");
        Check(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)), "CreateFence");

        std::array<D3D12_ROOT_PARAMETER, 6> parameters{};
        parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        parameters[0].Constants.ShaderRegister = 0;
        parameters[0].Constants.RegisterSpace = 0;
        parameters[0].Constants.Num32BitValues = 1;
        parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        for (std::uint32_t i = 0; i < 3; ++i)
        {
            parameters[i + 1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
            parameters[i + 1].Descriptor.ShaderRegister = i;
            parameters[i + 1].Descriptor.RegisterSpace = 0;
            parameters[i + 1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        }
        parameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
        parameters[4].Descriptor.ShaderRegister = 0;
        parameters[4].Descriptor.RegisterSpace = 0;
        parameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        parameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
        parameters[5].Descriptor.ShaderRegister = 1;
        parameters[5].Descriptor.RegisterSpace = 0;
        parameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        D3D12_ROOT_SIGNATURE_DESC rootDesc{};
        rootDesc.NumParameters = static_cast<UINT>(parameters.size());
        rootDesc.pParameters = parameters.data();
        rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
        ComPtr<ID3DBlob> serialized;
        ComPtr<ID3DBlob> errors;
        const HRESULT rootResult = D3D12SerializeRootSignature(
            &rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors);
        if (FAILED(rootResult))
        {
            const std::string message = errors
                ? std::string(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize())
                : HResultText(rootResult);
            throw std::runtime_error("D3D12SerializeRootSignature: " + message);
        }
        Check(device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(),
                                          IID_PPV_ARGS(&rootSignature)), "CreateRootSignature");
        auto shader = CompileShader(CompactDeltaShader());
        D3D12_COMPUTE_PIPELINE_STATE_DESC pipelineDesc{};
        pipelineDesc.pRootSignature = rootSignature.Get();
        pipelineDesc.CS = {shader->GetBufferPointer(), shader->GetBufferSize()};
        Check(device->CreateComputePipelineState(&pipelineDesc, IID_PPV_ARGS(&pipeline)),
              "CreateComputePipelineState");

        D3D12_INDIRECT_ARGUMENT_DESC indirectArgument{};
        indirectArgument.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
        D3D12_COMMAND_SIGNATURE_DESC signatureDesc{};
        signatureDesc.ByteStride = 12;
        signatureDesc.NumArgumentDescs = 1;
        signatureDesc.pArgumentDescs = &indirectArgument;
        Check(device->CreateCommandSignature(&signatureDesc, nullptr, IID_PPV_ARGS(&dispatchSignature)),
              "CreateCommandSignature(Dispatch)");
    }

    void Begin()
    {
        Check(allocator->Reset(), "CommandAllocator::Reset");
        Check(commandList->Reset(allocator.Get(), nullptr), "CommandList::Reset");
    }

    void SubmitAndWait()
    {
        Check(commandList->Close(), "CommandList::Close");
        ID3D12CommandList* lists[]{commandList.Get()};
        queue->ExecuteCommandLists(1, lists);
        ++fenceValue;
        Check(queue->Signal(fence.Get(), fenceValue), "CommandQueue::Signal");
        if (fence->GetCompletedValue() < fenceValue)
        {
            Check(fence->SetEventOnCompletion(fenceValue, fenceEvent.value), "Fence::SetEventOnCompletion");
            WaitForSingleObject(fenceEvent.value, INFINITE);
        }
    }
};

CompactDeltaCaseObservationV1 ExecuteCase(
    GpuContext& gpu,
    const semantic::SparseTemporalDeltaSemanticV1& semanticValue,
    const CompactDeltaArchitectureCaseV1& testCase)
{
    auto valid = contract::ValidateCompactDeltaArtifactForExecutionV1(
        testCase.artifact, semanticValue, testCase.invocation);
    if (!valid) throw std::runtime_error(valid.Error().stage + ": " + valid.Error().message);

    const auto historySize = semantic::BuildCanonicalInactiveHistoryBytesV1().size();
    if (testCase.previousHistoryBytes.size() != historySize)
        throw std::runtime_error("Previous history byte size is not canonical.");

    const auto motors = spiral6::semantic::BuildCanonicalGlobalMotorPaletteV1();
    const auto motorBytes = spiral5::semantic::SerializeMotorRecordsV1(motors);
    const auto points = semantic::BuildPointCorpusForInvocationV1(testCase.invocation);
    const auto pointBytes = spiral5::semantic::SerializePointRecordsV1(points);
    const auto expected = semantic::BuildCpuReferenceOutputV1(testCase.invocation);
    const auto expectedBytes = spiral5::semantic::SerializePointRecordsV1(expected);
    const auto zeroBytes = semantic::BuildCanonicalInactiveHistoryBytesV1();
    const std::vector<std::uint32_t> zeroAudit(semantic::WorkUniverseCountV1, 0u);
    const auto zeroAuditBytes = std::as_bytes(std::span(zeroAudit));

    std::vector<std::byte> recordBytes(semantic::DeltaRecordCapacityBytesV1);
    std::memcpy(recordBytes.data(), testCase.artifact.FixedCapacityRecords().data(), recordBytes.size());

    auto recordUpload = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_UPLOAD, recordBytes.size(),
                                     D3D12_RESOURCE_STATE_GENERIC_READ);
    auto motorUpload = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_UPLOAD, motorBytes.size(),
                                    D3D12_RESOURCE_STATE_GENERIC_READ);
    auto pointUpload = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_UPLOAD, pointBytes.size(),
                                    D3D12_RESOURCE_STATE_GENERIC_READ);
    auto historyUpload = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_UPLOAD,
                                      testCase.previousHistoryBytes.size(),
                                      D3D12_RESOURCE_STATE_GENERIC_READ);
    const std::array<std::uint32_t, 3> dispatchArguments{
        testCase.artifact.DispatchGroupsX(), 1u, 1u};
    const auto dispatchArgumentBytes = std::as_bytes(std::span(dispatchArguments));
    auto argumentUpload = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_UPLOAD,
                                       dispatchArgumentBytes.size(), D3D12_RESOURCE_STATE_GENERIC_READ);
    auto output = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_DEFAULT, historySize,
                               D3D12_RESOURCE_STATE_COPY_DEST,
                               D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    auto readback = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_READBACK, historySize,
                                 D3D12_RESOURCE_STATE_COPY_DEST);
    auto auditUpload = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_UPLOAD, zeroAuditBytes.size(),
                                    D3D12_RESOURCE_STATE_GENERIC_READ);
    auto audit = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_DEFAULT, zeroAuditBytes.size(),
                              D3D12_RESOURCE_STATE_COPY_DEST,
                              D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    auto auditReadback = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_READBACK, zeroAuditBytes.size(),
                                      D3D12_RESOURCE_STATE_COPY_DEST);

    UploadBytes(recordUpload.Get(), recordBytes);
    UploadBytes(motorUpload.Get(), motorBytes);
    UploadBytes(pointUpload.Get(), pointBytes);
    UploadBytes(historyUpload.Get(), testCase.previousHistoryBytes);
    UploadBytes(argumentUpload.Get(), dispatchArgumentBytes);
    UploadBytes(auditUpload.Get(), zeroAuditBytes);

    gpu.Begin();
    gpu.commandList->CopyBufferRegion(output.Get(), 0, historyUpload.Get(), 0, historySize);
    gpu.commandList->CopyBufferRegion(audit.Get(), 0, auditUpload.Get(), 0, zeroAuditBytes.size());
    const std::array<D3D12_RESOURCE_BARRIER, 2> toUav{
        Transition(output.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
                   D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
        Transition(audit.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
                   D3D12_RESOURCE_STATE_UNORDERED_ACCESS)};
    gpu.commandList->ResourceBarrier(static_cast<UINT>(toUav.size()), toUav.data());
    gpu.commandList->SetPipelineState(gpu.pipeline.Get());
    gpu.commandList->SetComputeRootSignature(gpu.rootSignature.Get());
    gpu.commandList->SetComputeRoot32BitConstant(0, testCase.artifact.TransitionCount(), 0);
    gpu.commandList->SetComputeRootShaderResourceView(1, recordUpload->GetGPUVirtualAddress());
    gpu.commandList->SetComputeRootShaderResourceView(2, motorUpload->GetGPUVirtualAddress());
    gpu.commandList->SetComputeRootShaderResourceView(3, pointUpload->GetGPUVirtualAddress());
    gpu.commandList->SetComputeRootUnorderedAccessView(4, output->GetGPUVirtualAddress());
    gpu.commandList->SetComputeRootUnorderedAccessView(5, audit->GetGPUVirtualAddress());
    gpu.commandList->ExecuteIndirect(
        gpu.dispatchSignature.Get(), 1, argumentUpload.Get(), 0, nullptr, 0);
    const std::array<D3D12_RESOURCE_BARRIER, 2> uavBarriers{
        UavBarrier(output.Get()), UavBarrier(audit.Get())};
    gpu.commandList->ResourceBarrier(static_cast<UINT>(uavBarriers.size()), uavBarriers.data());
    const std::array<D3D12_RESOURCE_BARRIER, 2> toCopy{
        Transition(output.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                   D3D12_RESOURCE_STATE_COPY_SOURCE),
        Transition(audit.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                   D3D12_RESOURCE_STATE_COPY_SOURCE)};
    gpu.commandList->ResourceBarrier(static_cast<UINT>(toCopy.size()), toCopy.data());
    gpu.commandList->CopyBufferRegion(readback.Get(), 0, output.Get(), 0, historySize);
    gpu.commandList->CopyBufferRegion(auditReadback.Get(), 0, audit.Get(), 0, zeroAuditBytes.size());
    gpu.SubmitAndWait();

    const auto observedBytes = ReadbackBytes(readback.Get(), historySize);
    const auto observedAuditBytes = ReadbackBytes(auditReadback.Get(), zeroAuditBytes.size());
    std::vector<spiral5::semantic::PointRecordV1> observed(semantic::WorkUniverseCountV1);
    std::memcpy(observed.data(), observedBytes.data(), observedBytes.size());
    std::vector<std::uint32_t> observedAudit(semantic::WorkUniverseCountV1, 0u);
    std::memcpy(observedAudit.data(), observedAuditBytes.data(), observedAuditBytes.size());

    CompactDeltaCaseObservationV1 result;
    result.id = testCase.id;
    result.invocationIndex = testCase.invocation.InvocationIndex();
    result.previousActiveCount = testCase.invocation.PreviousActiveSet().Cardinality().Value();
    result.activeCount = testCase.invocation.ActiveSet().Cardinality().Value();
    result.updateCount = testCase.artifact.UpdateCount();
    result.clearCount = testCase.artifact.ClearCount();
    result.retainCount = testCase.invocation.RetainSet().Cardinality().Value();
    result.transitionCount = testCase.artifact.TransitionCount();
    result.expectedDispatchGroupsX = result.transitionCount == 0 ? 0u :
        1u + ((result.transitionCount - 1u) / semantic::ThreadsPerGroupV1);
    result.observedDispatchGroupsX = testCase.artifact.DispatchGroupsX();
    result.activeReferenceQualified = true;
    result.retainedHistoryByteStable = true;
    result.inactiveSentinelByteStable = true;
    result.updateAndClearQualified = true;
    result.activeHomogeneousWQualified = true;
    result.finiteQualified = true;
    result.invocationIdentity = testCase.invocation.Identity();
    result.artifactIdentity = testCase.artifact.ArtifactIdentity();
    result.previousHistoryDigest = base::Sha256(testCase.previousHistoryBytes);
    result.outputDigest = base::Sha256(observedBytes);

    for (std::uint32_t index = 0; index < semantic::WorkUniverseCountV1; ++index)
    {
        const std::size_t offset = static_cast<std::size_t>(index) *
            sizeof(spiral5::semantic::PointRecordV1);
        const bool active = testCase.invocation.ActiveSet().Contains(index);
        const bool retained = testCase.invocation.RetainSet().Contains(index);
        const bool updated = testCase.invocation.UpdateSet().Contains(index);
        const bool cleared = testCase.invocation.DeactivationSet().Contains(index);

        if (retained && std::memcmp(observedBytes.data() + offset,
                                    testCase.previousHistoryBytes.data() + offset,
                                    sizeof(spiral5::semantic::PointRecordV1)) != 0)
            result.retainedHistoryByteStable = false;

        if (!active && std::memcmp(observedBytes.data() + offset, zeroBytes.data() + offset,
                                   sizeof(spiral5::semantic::PointRecordV1)) != 0)
            result.inactiveSentinelByteStable = false;

        const std::uint32_t expectedAudit = updated ? 1u : (cleared ? 2u : 0u);
        if (observedAudit[index] != expectedAudit)
            result.updateAndClearQualified = false;

        if (cleared && std::memcmp(observedBytes.data() + offset, zeroBytes.data() + offset,
                                   sizeof(spiral5::semantic::PointRecordV1)) != 0)
            result.updateAndClearQualified = false;

        if (!active) continue;
        const auto& actual = observed[index];
        const auto& reference = expected[index];
        const std::array<float, 4> a{actual.x, actual.y, actual.z, actual.w};
        const std::array<float, 4> e{reference.x, reference.y, reference.z, reference.w};
        for (std::size_t component = 0; component < a.size(); ++component)
        {
            if (!std::isfinite(a[component])) result.finiteQualified = false;
            const double error = std::abs(static_cast<double>(a[component]) - e[component]);
            result.maxAbsoluteError = std::max(result.maxAbsoluteError, error);
            if (error > 1.0e-6) result.activeReferenceQualified = false;
        }
        if (std::bit_cast<std::uint32_t>(actual.w) != std::bit_cast<std::uint32_t>(1.0f))
            result.activeHomogeneousWQualified = false;
        // Update authority is proven by TransitionAudit, not by requiring output bytes to differ.
        // A semantically legal update may write the same value as the previous history.
    }

    if (result.observedDispatchGroupsX != result.expectedDispatchGroupsX ||
        result.updateCount + result.clearCount != result.transitionCount ||
        !result.activeReferenceQualified || !result.retainedHistoryByteStable ||
        !result.inactiveSentinelByteStable || !result.updateAndClearQualified ||
        !result.activeHomogeneousWQualified || !result.finiteQualified ||
        observedBytes.size() != expectedBytes.size())
    {
        std::ostringstream failure;
        failure << "Compact Delta WARP observation failed for case=" << testCase.id
                << " invocation=" << result.invocationIndex
                << " transition=" << result.transitionCount
                << " update=" << result.updateCount
                << " clear=" << result.clearCount
                << " dispatch=" << result.observedDispatchGroupsX
                << "/" << result.expectedDispatchGroupsX
                << " activeReference=" << result.activeReferenceQualified
                << " retainedStable=" << result.retainedHistoryByteStable
                << " inactiveStable=" << result.inactiveSentinelByteStable
                << " exactTransitionAudit=" << result.updateAndClearQualified
                << " homogeneousW=" << result.activeHomogeneousWQualified
                << " finite=" << result.finiteQualified
                << " maxAbsError=" << result.maxAbsoluteError;
        throw std::runtime_error(failure.str());
    }
    return result;
}
#endif
} // namespace

base::Result<CompactDeltaArchitectureReportV1, DeltaExecutionErrorV1>
ExecuteCompactDeltaArchitectureV1(
    const semantic::SparseTemporalDeltaSemanticV1& semanticValue,
    const std::vector<CompactDeltaArchitectureCaseV1>& cases)
{
#if !defined(_WIN32)
    (void)semanticValue;
    (void)cases;
    return base::Result<CompactDeltaArchitectureReportV1, DeltaExecutionErrorV1>::Failure(
        Error("execution/platform", "Spiral 7 Compact Delta architecture requires Windows D3D12."));
#else
    try
    {
        if (cases.empty())
        {
            return base::Result<CompactDeltaArchitectureReportV1, DeltaExecutionErrorV1>::Failure(
                Error("execution/corpus", "Compact Delta architecture corpus is empty."));
        }
        GpuContext gpu;
        CompactDeltaArchitectureReportV1 report;
        report.adapterDescription = gpu.adapterDescription;
        report.semanticIdentity = semantic::SparseTemporalDeltaSemanticIdentityV1(semanticValue);
        report.cases.reserve(cases.size());
        for (const auto& testCase : cases)
            report.cases.push_back(ExecuteCase(gpu, semanticValue, testCase));
        return base::Result<CompactDeltaArchitectureReportV1, DeltaExecutionErrorV1>::Success(
            std::move(report));
    }
    catch (const std::exception& error)
    {
        return base::Result<CompactDeltaArchitectureReportV1, DeltaExecutionErrorV1>::Failure(
            Error("execution/warp", error.what()));
    }
#endif
}

std::vector<std::byte> SerializeCompactDeltaArchitectureReportV1(
    const CompactDeltaArchitectureReportV1& report)
{
    base::BinaryWriter writer;
    writer.WriteU32(0x31523753u); // "S7R1"
    writer.WriteU32(1);
    WriteString(writer, report.adapterDescription);
    WriteDigest(writer, report.semanticIdentity);
    writer.WriteU32(static_cast<std::uint32_t>(report.cases.size()));
    for (const auto& value : report.cases)
    {
        WriteString(writer, value.id);
        writer.WriteU32(value.invocationIndex);
        writer.WriteU32(value.previousActiveCount);
        writer.WriteU32(value.activeCount);
        writer.WriteU32(value.updateCount);
        writer.WriteU32(value.clearCount);
        writer.WriteU32(value.retainCount);
        writer.WriteU32(value.transitionCount);
        writer.WriteU32(value.expectedDispatchGroupsX);
        writer.WriteU32(value.observedDispatchGroupsX);
        writer.WriteU32(value.activeReferenceQualified ? 1u : 0u);
        writer.WriteU32(value.retainedHistoryByteStable ? 1u : 0u);
        writer.WriteU32(value.inactiveSentinelByteStable ? 1u : 0u);
        writer.WriteU32(value.updateAndClearQualified ? 1u : 0u);
        writer.WriteU32(value.activeHomogeneousWQualified ? 1u : 0u);
        writer.WriteU32(value.finiteQualified ? 1u : 0u);
        writer.WriteU64(std::bit_cast<std::uint64_t>(value.maxAbsoluteError));
        WriteDigest(writer, value.invocationIdentity);
        WriteDigest(writer, value.artifactIdentity);
        WriteDigest(writer, value.previousHistoryDigest);
        WriteDigest(writer, value.outputDigest);
    }
    return std::move(writer).Take();
}
} // namespace sge4_5::spiral7::execution
