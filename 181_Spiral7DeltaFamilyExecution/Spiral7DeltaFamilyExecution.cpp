#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "Spiral7DeltaFamilyExecution.h"

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

namespace sge4_5::spiral7::family_execution
{
namespace fc = family_candidate;
namespace fv = family_verification;
namespace
{
DeltaFamilyExecutionErrorV1 Error(std::string stage, std::string message, long nativeCode = 0)
{
    return {std::move(stage), std::move(message), nativeCode};
}

void WriteString(base::BinaryWriter& writer, std::string_view value)
{
    writer.WriteU32(static_cast<std::uint32_t>(value.size()));
    writer.WriteBytes(std::as_bytes(std::span(value.data(), value.size())));
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

ComPtr<ID3DBlob> CompileShader(std::string_view source, std::string_view sourceName)
{
    ComPtr<ID3DBlob> shader;
    ComPtr<ID3DBlob> errors;
    const UINT flags = D3DCOMPILE_ENABLE_STRICTNESS |
                       D3DCOMPILE_WARNINGS_ARE_ERRORS |
                       D3DCOMPILE_OPTIMIZATION_LEVEL3;
    const HRESULT hr = D3DCompile(
        source.data(), source.size(), sourceName.data(), nullptr, nullptr,
        "main", "cs_5_1", flags, 0, &shader, &errors);
    if (FAILED(hr))
    {
        const std::string message = errors
            ? std::string(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize())
            : HResultText(hr);
        throw std::runtime_error("D3DCompile(" + std::string(sourceName) + "): " + message);
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

std::string ShaderCommon()
{
    return R"(
struct Motor { float4 qr; float4 qd; };
ByteAddressBuffer FamilyPayload : register(t0);
StructuredBuffer<Motor> GlobalMotors : register(t1);
StructuredBuffer<float4> Points : register(t2);
RWStructuredBuffer<float4> Output : register(u0);
RWStructuredBuffer<uint> WriteAudit : register(u1);
float4 qmul(float4 a, float4 b)
{
    return float4(
        a.w*b.x + b.w*a.x + a.y*b.z - a.z*b.y,
        a.w*b.y + b.w*a.y + a.z*b.x - a.x*b.z,
        a.w*b.z + b.w*a.z + a.x*b.y - a.y*b.x,
        a.w*b.w - dot(a.xyz,b.xyz));
}
float4 qconj(float4 q) { return float4(-q.xyz, q.w); }
float4 transformPoint(uint universeIndex)
{
    uint bone = universeIndex / 512;
    Motor motor = GlobalMotors[bone];
    float4 inputPoint = Points[universeIndex];
    float4 conjugate = qconj(motor.qr);
    float4 rotated = qmul(qmul(motor.qr, float4(inputPoint.xyz,0)), conjugate);
    float3 translation = 2.0f * qmul(motor.qd, conjugate).xyz;
    return float4(rotated.xyz + translation, 1.0f);
}
)";
}

std::string DenseShader()
{
    return ShaderCommon() + R"(
cbuffer FamilyParameters : register(b0) { uint workCount; };
[numthreads(64,1,1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint universeIndex = dispatchThreadId.x;
    if (universeIndex >= workCount) return;
    uint word = FamilyPayload.Load((universeIndex / 32) * 4);
    uint bit = 1u << (universeIndex % 32);
    WriteAudit[universeIndex] = 3;
    Output[universeIndex] = (word & bit) != 0
        ? transformPoint(universeIndex)
        : float4(0,0,0,0);
}
)";
}

std::string CompactShader()
{
    return ShaderCommon() + R"(
cbuffer FamilyParameters : register(b0) { uint transitionCount; };
[numthreads(64,1,1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint ordinal = dispatchThreadId.x;
    if (ordinal >= transitionCount) return;
    uint universeIndex = FamilyPayload.Load(ordinal * 8);
    uint action = FamilyPayload.Load(ordinal * 8 + 4);
    if (universeIndex >= 4096) return;
    if (action == 1)
    {
        WriteAudit[universeIndex] = 1;
        Output[universeIndex] = transformPoint(universeIndex);
    }
    else if (action == 2)
    {
        WriteAudit[universeIndex] = 2;
        Output[universeIndex] = float4(0,0,0,0);
    }
}
)";
}

std::string BlockShader()
{
    return ShaderCommon() + R"(
cbuffer FamilyParameters : register(b0) { uint affectedBlockCount; };
[numthreads(64,1,1)]
void main(uint3 groupId : SV_GroupID, uint3 groupThreadId : SV_GroupThreadID)
{
    uint ordinal = groupId.x;
    if (ordinal >= affectedBlockCount) return;
    uint baseOffset = ordinal * 24;
    uint blockIndex = FamilyPayload.Load(baseOffset);
    if (blockIndex >= 64) return;
    uint updateLow = FamilyPayload.Load(baseOffset + 8);
    uint updateHigh = FamilyPayload.Load(baseOffset + 12);
    uint clearLow = FamilyPayload.Load(baseOffset + 16);
    uint clearHigh = FamilyPayload.Load(baseOffset + 20);
    uint lane = groupThreadId.x;
    uint bit = 1u << (lane & 31);
    bool update = lane < 32 ? ((updateLow & bit) != 0) : ((updateHigh & bit) != 0);
    bool clear = lane < 32 ? ((clearLow & bit) != 0) : ((clearHigh & bit) != 0);
    uint universeIndex = blockIndex * 64 + lane;
    if (update)
    {
        WriteAudit[universeIndex] = 1;
        Output[universeIndex] = transformPoint(universeIndex);
    }
    else if (clear)
    {
        WriteAudit[universeIndex] = 2;
        Output[universeIndex] = float4(0,0,0,0);
    }
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
    ComPtr<ID3D12PipelineState> densePipeline;
    ComPtr<ID3D12PipelineState> compactPipeline;
    ComPtr<ID3D12PipelineState> blockPipeline;
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

        const auto createPipeline = [&](std::string_view source, std::string_view name,
                                        ComPtr<ID3D12PipelineState>& destination)
        {
            auto shader = CompileShader(source, name);
            D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
            desc.pRootSignature = rootSignature.Get();
            desc.CS = {shader->GetBufferPointer(), shader->GetBufferSize()};
            Check(device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&destination)),
                  "CreateComputePipelineState");
        };
        createPipeline(DenseShader(), "Spiral7FullActiveDenseRecomputeV1", densePipeline);
        createPipeline(CompactShader(), "Spiral7CompactDeltaIndexHistoryReuseV1", compactPipeline);
        createPipeline(BlockShader(), "Spiral7AffectedBlockDeltaHistoryReuseV1", blockPipeline);

        D3D12_INDIRECT_ARGUMENT_DESC indirectArgument{};
        indirectArgument.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
        D3D12_COMMAND_SIGNATURE_DESC signatureDesc{};
        signatureDesc.ByteStride = 12;
        signatureDesc.NumArgumentDescs = 1;
        signatureDesc.pArgumentDescs = &indirectArgument;
        Check(device->CreateCommandSignature(&signatureDesc, nullptr, IID_PPV_ARGS(&dispatchSignature)),
              "CreateCommandSignature(Dispatch)");
    }

    ID3D12PipelineState* Pipeline(fc::DeltaFamilyCandidateKindV1 kind) const
    {
        switch (kind)
        {
        case fc::DeltaFamilyCandidateKindV1::FullActiveDenseRecompute: return densePipeline.Get();
        case fc::DeltaFamilyCandidateKindV1::CompactDeltaIndexHistoryReuse: return compactPipeline.Get();
        case fc::DeltaFamilyCandidateKindV1::AffectedBlockDeltaHistoryReuse: return blockPipeline.Get();
        }
        throw std::runtime_error("Unknown Delta Family pipeline kind.");
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

struct CandidateRun final
{
    DeltaFamilyCandidateObservationV1 observation;
    std::vector<std::byte> outputBytes;
};

CandidateRun ExecuteCandidate(
    GpuContext& gpu,
    const semantic::SparseTemporalDeltaSemanticV1& semanticValue,
    const semantic::SparseDeltaInvocationV1& invocation,
    std::span<const std::byte> previousHistoryBytes,
    const fc::FrozenDeltaFamilyArtifactV1& artifact,
    const fv::VerifiedDeltaFamilyCandidateV1& verified)
{
    const auto context = fv::BuildCanonicalDeltaFamilyVerificationContextV1();
    const auto verifiedValid = fv::ValidateVerifiedDeltaFamilyCandidateV1(
        verified, semanticValue, invocation, context);
    if (!verifiedValid) throw std::runtime_error(verifiedValid.Error());
    const auto artifactValid = fc::ValidateDeltaFamilyArtifactV1(
        artifact, semanticValue, invocation);
    if (!artifactValid) throw std::runtime_error(artifactValid.Error());
    const auto& operation = verified.Plan().operation;
    if (operation.kind != artifact.Kind() ||
        operation.artifactIdentity != artifact.ArtifactIdentity() ||
        operation.dispatchGroupsX != artifact.DispatchGroupsX() ||
        operation.fixedCapacityBytes != artifact.FixedCapacityBytes())
        throw std::runtime_error("Verified Delta Family operation does not match artifact.");

    const auto historySize = semantic::BuildCanonicalInactiveHistoryBytesV1().size();
    if (previousHistoryBytes.size() != historySize)
        throw std::runtime_error("Previous Family history byte size is not canonical.");

    const auto motors = spiral6::semantic::BuildCanonicalGlobalMotorPaletteV1();
    const auto motorBytes = spiral5::semantic::SerializeMotorRecordsV1(motors);
    const auto points = semantic::BuildPointCorpusForInvocationV1(invocation);
    const auto pointBytes = spiral5::semantic::SerializePointRecordsV1(points);
    const auto expected = semantic::BuildCpuReferenceOutputV1(invocation);
    const auto zeroBytes = semantic::BuildCanonicalInactiveHistoryBytesV1();
    const std::vector<std::uint32_t> zeroAudit(semantic::WorkUniverseCountV1, 0u);
    const auto zeroAuditBytes = std::as_bytes(std::span(zeroAudit));

    const auto& payloadBytes = artifact.PayloadBytes();
    auto payloadUpload = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_UPLOAD,
                                      payloadBytes.size(), D3D12_RESOURCE_STATE_GENERIC_READ);
    auto motorUpload = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_UPLOAD,
                                    motorBytes.size(), D3D12_RESOURCE_STATE_GENERIC_READ);
    auto pointUpload = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_UPLOAD,
                                    pointBytes.size(), D3D12_RESOURCE_STATE_GENERIC_READ);
    auto historyUpload = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_UPLOAD,
                                      previousHistoryBytes.size(), D3D12_RESOURCE_STATE_GENERIC_READ);
    const std::array<std::uint32_t, 3> dispatchArguments{
        artifact.DispatchGroupsX(), 1u, 1u};
    const auto dispatchArgumentBytes = std::as_bytes(std::span(dispatchArguments));
    auto argumentUpload = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_UPLOAD,
                                       dispatchArgumentBytes.size(), D3D12_RESOURCE_STATE_GENERIC_READ);
    auto auditUpload = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_UPLOAD,
                                    zeroAuditBytes.size(), D3D12_RESOURCE_STATE_GENERIC_READ);
    auto output = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_DEFAULT, historySize,
                               D3D12_RESOURCE_STATE_COPY_DEST,
                               D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    auto audit = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_DEFAULT, zeroAuditBytes.size(),
                              D3D12_RESOURCE_STATE_COPY_DEST,
                              D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    auto outputReadback = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_READBACK, historySize,
                                       D3D12_RESOURCE_STATE_COPY_DEST);
    auto auditReadback = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_READBACK, zeroAuditBytes.size(),
                                      D3D12_RESOURCE_STATE_COPY_DEST);

    UploadBytes(payloadUpload.Get(), payloadBytes);
    UploadBytes(motorUpload.Get(), motorBytes);
    UploadBytes(pointUpload.Get(), pointBytes);
    UploadBytes(historyUpload.Get(), previousHistoryBytes);
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
    gpu.commandList->SetPipelineState(gpu.Pipeline(artifact.Kind()));
    gpu.commandList->SetComputeRootSignature(gpu.rootSignature.Get());
    const std::uint32_t rootCount = artifact.Kind() == fc::DeltaFamilyCandidateKindV1::FullActiveDenseRecompute
        ? semantic::WorkUniverseCountV1
        : (artifact.Kind() == fc::DeltaFamilyCandidateKindV1::CompactDeltaIndexHistoryReuse
            ? artifact.TransitionCount() : artifact.AffectedBlockCount());
    gpu.commandList->SetComputeRoot32BitConstant(0, rootCount, 0);
    gpu.commandList->SetComputeRootShaderResourceView(1, payloadUpload->GetGPUVirtualAddress());
    gpu.commandList->SetComputeRootShaderResourceView(2, motorUpload->GetGPUVirtualAddress());
    gpu.commandList->SetComputeRootShaderResourceView(3, pointUpload->GetGPUVirtualAddress());
    gpu.commandList->SetComputeRootUnorderedAccessView(4, output->GetGPUVirtualAddress());
    gpu.commandList->SetComputeRootUnorderedAccessView(5, audit->GetGPUVirtualAddress());
    gpu.commandList->ExecuteIndirect(
        gpu.dispatchSignature.Get(), 1, argumentUpload.Get(), 0, nullptr, 0);
    const std::array<D3D12_RESOURCE_BARRIER, 2> uav{
        UavBarrier(output.Get()), UavBarrier(audit.Get())};
    gpu.commandList->ResourceBarrier(static_cast<UINT>(uav.size()), uav.data());
    const std::array<D3D12_RESOURCE_BARRIER, 2> toCopy{
        Transition(output.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                   D3D12_RESOURCE_STATE_COPY_SOURCE),
        Transition(audit.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                   D3D12_RESOURCE_STATE_COPY_SOURCE)};
    gpu.commandList->ResourceBarrier(static_cast<UINT>(toCopy.size()), toCopy.data());
    gpu.commandList->CopyBufferRegion(outputReadback.Get(), 0, output.Get(), 0, historySize);
    gpu.commandList->CopyBufferRegion(auditReadback.Get(), 0, audit.Get(), 0, zeroAuditBytes.size());
    gpu.SubmitAndWait();

    auto observedBytes = ReadbackBytes(outputReadback.Get(), historySize);
    const auto auditBytes = ReadbackBytes(auditReadback.Get(), zeroAuditBytes.size());
    std::vector<spiral5::semantic::PointRecordV1> observed(semantic::WorkUniverseCountV1);
    std::memcpy(observed.data(), observedBytes.data(), observedBytes.size());
    std::vector<std::uint32_t> observedAudit(semantic::WorkUniverseCountV1);
    std::memcpy(observedAudit.data(), auditBytes.data(), auditBytes.size());

    CandidateRun result;
    auto& o = result.observation;
    o.kind = artifact.Kind();
    o.dispatchGroupsX = artifact.DispatchGroupsX();
    o.legalWriteCount = artifact.LegalWriteSet() == fc::DeltaFamilyLegalWriteSetV1::EntireUniverse
        ? semantic::WorkUniverseCountV1 : artifact.TransitionCount();
    o.activeReferenceQualified = true;
    o.inactiveSentinelByteStable = true;
    o.retainedHistoryByteStable = true;
    o.exactWriteSetQualified = true;
    o.homogeneousWQualified = true;
    o.finiteQualified = true;
    o.artifactIdentity = artifact.ArtifactIdentity();
    o.certificateIdentity = verified.CertificateIdentity();
    o.outputDigest = base::Sha256(observedBytes);

    for (std::uint32_t index = 0; index < semantic::WorkUniverseCountV1; ++index)
    {
        const auto offset = static_cast<std::size_t>(index) *
            sizeof(spiral5::semantic::PointRecordV1);
        const bool active = invocation.ActiveSet().Contains(index);
        const bool retained = invocation.RetainSet().Contains(index);
        const bool updated = invocation.UpdateSet().Contains(index);
        const bool cleared = invocation.DeactivationSet().Contains(index);
        const std::uint32_t expectedAudit =
            artifact.Kind() == fc::DeltaFamilyCandidateKindV1::FullActiveDenseRecompute
                ? 3u : (updated ? 1u : (cleared ? 2u : 0u));
        if (observedAudit[index] != expectedAudit) o.exactWriteSetQualified = false;
        if (observedAudit[index] != 0) ++o.observedWriteCount;

        if (retained && std::memcmp(observedBytes.data() + offset,
                                    previousHistoryBytes.data() + offset,
                                    sizeof(spiral5::semantic::PointRecordV1)) != 0)
            o.retainedHistoryByteStable = false;
        if (!active && std::memcmp(observedBytes.data() + offset,
                                   zeroBytes.data() + offset,
                                   sizeof(spiral5::semantic::PointRecordV1)) != 0)
            o.inactiveSentinelByteStable = false;
        if (!active) continue;

        const auto& actual = observed[index];
        const auto& reference = expected[index];
        const std::array<float, 4> a{actual.x, actual.y, actual.z, actual.w};
        const std::array<float, 4> e{reference.x, reference.y, reference.z, reference.w};
        for (std::size_t component = 0; component < a.size(); ++component)
        {
            if (!std::isfinite(a[component])) o.finiteQualified = false;
            const double error = std::abs(static_cast<double>(a[component]) - e[component]);
            o.maxAbsoluteError = std::max(o.maxAbsoluteError, error);
            if (error > 1.0e-6) o.activeReferenceQualified = false;
        }
        if (std::bit_cast<std::uint32_t>(actual.w) != std::bit_cast<std::uint32_t>(1.0f))
            o.homogeneousWQualified = false;
    }

    if (o.observedWriteCount != o.legalWriteCount ||
        !o.activeReferenceQualified || !o.inactiveSentinelByteStable ||
        !o.retainedHistoryByteStable || !o.exactWriteSetQualified ||
        !o.homogeneousWQualified || !o.finiteQualified)
    {
        std::ostringstream failure;
        failure << "Delta Family candidate observation failed kind="
                << fc::DeltaFamilyCandidateNameV1(artifact.Kind())
                << " invocation=" << invocation.InvocationIndex()
                << " dispatch=" << artifact.DispatchGroupsX()
                << " write=" << o.observedWriteCount << "/" << o.legalWriteCount
                << " activeReference=" << o.activeReferenceQualified
                << " inactive=" << o.inactiveSentinelByteStable
                << " retained=" << o.retainedHistoryByteStable
                << " exactWriteSet=" << o.exactWriteSetQualified
                << " homogeneousW=" << o.homogeneousWQualified
                << " finite=" << o.finiteQualified
                << " maxAbsError=" << o.maxAbsoluteError;
        throw std::runtime_error(failure.str());
    }
    result.outputBytes = std::move(observedBytes);
    return result;
}
#endif
} // namespace

base::Result<DeltaFamilyArchitectureReportV1, DeltaFamilyExecutionErrorV1>
ExecuteVerifiedDeltaCandidateFamilyV1(
    const semantic::SparseTemporalDeltaSemanticV1& semanticValue,
    const std::vector<VerifiedDeltaFamilyCaseV1>& cases)
{
#if !defined(_WIN32)
    (void)semanticValue;
    (void)cases;
    return base::Result<DeltaFamilyArchitectureReportV1, DeltaFamilyExecutionErrorV1>::Failure(
        Error("execution/platform", "Spiral 7 Delta Candidate Family requires Windows D3D12."));
#else
    try
    {
        if (cases.empty())
            return base::Result<DeltaFamilyArchitectureReportV1, DeltaFamilyExecutionErrorV1>::Failure(
                Error("execution/corpus", "Delta Candidate Family corpus is empty."));

        GpuContext gpu;
        DeltaFamilyArchitectureReportV1 report;
        report.adapterDescription = gpu.adapterDescription;
        report.semanticIdentity = semantic::SparseTemporalDeltaSemanticIdentityV1(semanticValue);
        report.invocations.reserve(cases.size());
        std::vector<std::byte> previousHistory = semantic::BuildCanonicalInactiveHistoryBytesV1();
        base::Digest256 previousActiveIdentity{};
        bool havePrevious = false;

        const std::array<fc::DeltaFamilyCandidateKindV1, 3> expectedOrder{
            fc::DeltaFamilyCandidateKindV1::FullActiveDenseRecompute,
            fc::DeltaFamilyCandidateKindV1::CompactDeltaIndexHistoryReuse,
            fc::DeltaFamilyCandidateKindV1::AffectedBlockDeltaHistoryReuse};

        for (std::size_t caseIndex = 0; caseIndex < cases.size(); ++caseIndex)
        {
            const auto& testCase = cases[caseIndex];
            if (testCase.artifacts.size() != expectedOrder.size() ||
                testCase.verifiedCandidates.size() != expectedOrder.size())
                throw std::runtime_error("Delta Family case must contain exactly A/B/C.");
            if (testCase.invocation.InvocationIndex() != caseIndex)
                throw std::runtime_error("Delta Family invocation indices must be contiguous from zero.");
            if (havePrevious && testCase.invocation.PreviousActiveSet().Identity() != previousActiveIdentity)
                throw std::runtime_error("Delta Family timeline previous Active Set is not chained.");
            if (!havePrevious && testCase.invocation.PreviousActiveSet().Cardinality().Value() != 0)
                throw std::runtime_error("Delta Family first invocation must begin from empty Active Set.");

            DeltaFamilyInvocationObservationV1 invocationObservation;
            invocationObservation.id = testCase.id;
            invocationObservation.invocationIndex = testCase.invocation.InvocationIndex();
            invocationObservation.activeCount = testCase.invocation.ActiveSet().Cardinality().Value();
            invocationObservation.transitionCount = testCase.invocation.TransitionSet().Cardinality().Value();
            invocationObservation.affectedBlockCount = testCase.artifacts[2].AffectedBlockCount();
            invocationObservation.exactCandidateOrder = true;

            std::array<std::vector<std::byte>, 3> outputs;
            for (std::size_t i = 0; i < expectedOrder.size(); ++i)
            {
                if (testCase.artifacts[i].Kind() != expectedOrder[i] ||
                    testCase.verifiedCandidates[i].Plan().operation.kind != expectedOrder[i])
                    invocationObservation.exactCandidateOrder = false;
                auto run = ExecuteCandidate(gpu, semanticValue, testCase.invocation,
                    previousHistory, testCase.artifacts[i], testCase.verifiedCandidates[i]);
                invocationObservation.candidates.push_back(run.observation);
                outputs[i] = std::move(run.outputBytes);
            }
            invocationObservation.pairwiseOutputByteIdentical =
                outputs[0] == outputs[1] && outputs[0] == outputs[2];
            if (!invocationObservation.exactCandidateOrder ||
                !invocationObservation.pairwiseOutputByteIdentical)
            {
                std::ostringstream failure;
                failure << "Delta Family A/B/C output mismatch at invocation="
                        << invocationObservation.invocationIndex
                        << " case=" << invocationObservation.id
                        << " order=" << invocationObservation.exactCandidateOrder
                        << " pairwise=" << invocationObservation.pairwiseOutputByteIdentical;
                throw std::runtime_error(failure.str());
            }
            previousHistory = std::move(outputs[0]);
            previousActiveIdentity = testCase.invocation.ActiveSet().Identity();
            havePrevious = true;
            report.invocations.push_back(std::move(invocationObservation));
        }
        return base::Result<DeltaFamilyArchitectureReportV1, DeltaFamilyExecutionErrorV1>::Success(
            std::move(report));
    }
    catch (const std::exception& error)
    {
        return base::Result<DeltaFamilyArchitectureReportV1, DeltaFamilyExecutionErrorV1>::Failure(
            Error("execution/warp", error.what()));
    }
#endif
}

std::vector<std::byte> SerializeDeltaFamilyArchitectureReportV1(
    const DeltaFamilyArchitectureReportV1& report)
{
    base::BinaryWriter writer;
    writer.WriteU32(0x31463753u); // "S7F1"
    writer.WriteU32(1);
    WriteString(writer, report.adapterDescription);
    writer.WriteBytes(report.semanticIdentity);
    writer.WriteU32(static_cast<std::uint32_t>(report.invocations.size()));
    for (const auto& invocation : report.invocations)
    {
        WriteString(writer, invocation.id);
        writer.WriteU32(invocation.invocationIndex);
        writer.WriteU32(invocation.activeCount);
        writer.WriteU32(invocation.transitionCount);
        writer.WriteU32(invocation.affectedBlockCount);
        writer.WriteU32(invocation.exactCandidateOrder ? 1u : 0u);
        writer.WriteU32(invocation.pairwiseOutputByteIdentical ? 1u : 0u);
        writer.WriteU32(static_cast<std::uint32_t>(invocation.candidates.size()));
        for (const auto& candidate : invocation.candidates)
        {
            writer.WriteU32(static_cast<std::uint32_t>(candidate.kind));
            writer.WriteU32(candidate.dispatchGroupsX);
            writer.WriteU32(candidate.legalWriteCount);
            writer.WriteU32(candidate.observedWriteCount);
            writer.WriteU32(candidate.activeReferenceQualified ? 1u : 0u);
            writer.WriteU32(candidate.inactiveSentinelByteStable ? 1u : 0u);
            writer.WriteU32(candidate.retainedHistoryByteStable ? 1u : 0u);
            writer.WriteU32(candidate.exactWriteSetQualified ? 1u : 0u);
            writer.WriteU32(candidate.homogeneousWQualified ? 1u : 0u);
            writer.WriteU32(candidate.finiteQualified ? 1u : 0u);
            writer.WriteBytes(candidate.artifactIdentity);
            writer.WriteBytes(candidate.certificateIdentity);
            writer.WriteBytes(candidate.outputDigest);
        }
    }
    const auto digest = base::Sha256(writer.Bytes());
    writer.WriteBytes(digest);
    return std::move(writer).Take();
}
} // namespace sge4_5::spiral7::family_execution
