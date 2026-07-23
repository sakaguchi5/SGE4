#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "Spiral6PerformanceExperiment.h"

#include "../00_Foundation/BinaryIO.h"
#include "../161_SparseCandidateFamilyPlanner/SparseCandidateFamilyPlanner.h"
#include "../162_SparseCandidateFamilyVerifier/SparseCandidateFamilyVerifier.h"
#include "../163_Spiral6SparseFamilyExecution/Spiral6SparseFamilyExecution.h"

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
#include <chrono>
#include <cmath>
#include <cstring>
#include <map>
#include <set>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#if defined(_WIN32)
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")
#endif

namespace sge4_5::spiral6::performance
{
// Primary metric: GpuCandidatePathNanoseconds.
namespace fc = family_candidate;
namespace fp = family_planner;
namespace fv = family_verification;
namespace fe = family_execution;
namespace
{
[[maybe_unused]] base::Digest256 ContextIdentity(const fv::SparseFamilyVerificationContextV1& context)
{
    base::BinaryWriter writer;
    writer.WriteBytes(context.targetProfileIdentity);
    writer.WriteBytes(context.observationContractIdentity);
    writer.WriteBytes(context.completionContractIdentity);
    writer.WriteBytes(context.deviceEpochPolicyIdentity);
    writer.WriteBytes(context.spiral4IndirectArtifactIdentity);
    return base::Sha256(writer.Bytes());
}

#if defined(_WIN32)
using Microsoft::WRL::ComPtr;
using Clock = std::chrono::steady_clock;

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

ComPtr<ID3DBlob> CompileShader(std::string_view source, std::string_view name)
{
    ComPtr<ID3DBlob> shader;
    ComPtr<ID3DBlob> errors;
    const UINT flags = D3DCOMPILE_ENABLE_STRICTNESS |
        D3DCOMPILE_WARNINGS_ARE_ERRORS | D3DCOMPILE_OPTIMIZATION_LEVEL3;
    const HRESULT hr = D3DCompile(source.data(), source.size(), std::string(name).c_str(),
        nullptr, nullptr, "main", "cs_5_1", flags, 0, &shader, &errors);
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
    return base::Sha256(std::span<const std::byte>(
        static_cast<const std::byte*>(blob->GetBufferPointer()), blob->GetBufferSize()));
}

ComPtr<ID3D12RootSignature> CreateRootSignature(
    ID3D12Device* device,
    std::span<const D3D12_ROOT_PARAMETER> parameters)
{
    D3D12_ROOT_SIGNATURE_DESC description{};
    description.NumParameters = static_cast<UINT>(parameters.size());
    description.pParameters = parameters.data();
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
    Check(device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(),
        IID_PPV_ARGS(&root)), "CreateRootSignature");
    return root;
}

ComPtr<ID3D12PipelineState> CreatePipeline(
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
cbuffer Arguments : register(b0) { uint dispatchX; };
RWByteAddressBuffer IndirectArguments : register(u0);
[numthreads(1,1,1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (any(dispatchThreadId != 0)) return;
    IndirectArguments.Store(0, dispatchX);
    IndirectArguments.Store(4, 1);
    IndirectArguments.Store(8, 1);
}
)";
}

std::string CommonTransformBody()
{
    return R"(
struct Motor { float4 qr; float4 qd; };
cbuffer SparseParameters : register(b0) { uint activeCount; };
StructuredBuffer<uint> Representation : register(t0);
StructuredBuffer<Motor> GlobalMotors : register(t1);
StructuredBuffer<float4> Points : register(t2);
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
void writeTransform(uint universeIndex)
{
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

std::string DenseShader()
{
    return CommonTransformBody() + R"(
[numthreads(64,1,1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (activeCount > 4096) return;
    uint universeIndex = dispatchThreadId.x;
    if (universeIndex >= 4096) return;
    uint word = Representation[universeIndex >> 5];
    if ((word & (1u << (universeIndex & 31u))) == 0) return;
    writeTransform(universeIndex);
}
)";
}

std::string CompactShader()
{
    return CommonTransformBody() + R"(
[numthreads(64,1,1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint ordinal = dispatchThreadId.x;
    if (ordinal >= activeCount) return;
    uint universeIndex = Representation[ordinal];
    if (universeIndex >= 4096) return;
    writeTransform(universeIndex);
}
)";
}

std::string ActiveBlockShader()
{
    return CommonTransformBody() + R"(
[numthreads(64,1,1)]
void main(uint3 groupId : SV_GroupID, uint3 groupThreadId : SV_GroupThreadID)
{
    if (activeCount > 4096) return;
    uint base = groupId.x * 4;
    uint blockIndex = Representation[base + 0];
    uint mask = groupThreadId.x < 32 ? Representation[base + 1] : Representation[base + 2];
    uint bit = groupThreadId.x & 31u;
    if ((mask & (1u << bit)) == 0) return;
    uint universeIndex = blockIndex * 64 + groupThreadId.x;
    if (universeIndex >= 4096) return;
    writeTransform(universeIndex);
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
    std::map<CandidateKindV1, ComPtr<ID3DBlob>> candidateShaders;
    ComPtr<ID3D12RootSignature> argumentRoot;
    ComPtr<ID3D12RootSignature> candidateRoot;
    ComPtr<ID3D12PipelineState> argumentPipeline;
    std::map<CandidateKindV1, ComPtr<ID3D12PipelineState>> candidatePipelines;
    ComPtr<ID3D12CommandSignature> dispatchSignature;
    ComPtr<ID3D12QueryHeap> queryHeap;
    ComPtr<ID3D12Resource> queryReadback;

    ComPtr<ID3D12Resource> motorUpload;
    ComPtr<ID3D12Resource> pointUpload;
    ComPtr<ID3D12Resource> zeroUpload;
    std::vector<std::byte> zeroBytes;
    std::vector<spiral5::semantic::PointRecordV1> points;
    std::array<spiral5::semantic::PgaMotorRecordV1, semantic::BoneCountV1> motors{};

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
            throw std::runtime_error("Requested eligible hardware adapter index is unavailable.");
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

        argumentShader = CompileShader(ArgumentProducerShader(), "SparseArgumentProducer");
        candidateShaders.emplace(CandidateKindV1::DenseMembershipMaskPredicate,
            CompileShader(DenseShader(), "SparseDenseMask"));
        candidateShaders.emplace(CandidateKindV1::CompactIndexListDispatch,
            CompileShader(CompactShader(), "SparseCompactIndex"));
        candidateShaders.emplace(CandidateKindV1::ActiveBlockListLocalMask,
            CompileShader(ActiveBlockShader(), "SparseActiveBlock"));

        std::array<D3D12_ROOT_PARAMETER, 2> argumentParameters{};
        argumentParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        argumentParameters[0].Constants.ShaderRegister = 0;
        argumentParameters[0].Constants.Num32BitValues = 1;
        argumentParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
        argumentParameters[1].Descriptor.ShaderRegister = 0;
        argumentRoot = CreateRootSignature(device.Get(), argumentParameters);

        std::array<D3D12_ROOT_PARAMETER, 5> candidateParameters{};
        candidateParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        candidateParameters[0].Constants.ShaderRegister = 0;
        candidateParameters[0].Constants.Num32BitValues = 1;
        for (std::uint32_t index = 0; index < 3; ++index)
        {
            candidateParameters[index + 1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
            candidateParameters[index + 1].Descriptor.ShaderRegister = index;
        }
        candidateParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
        candidateParameters[4].Descriptor.ShaderRegister = 0;
        candidateRoot = CreateRootSignature(device.Get(), candidateParameters);

        argumentPipeline = CreatePipeline(device.Get(), argumentRoot.Get(), argumentShader.Get());
        for (const auto& [kind, shader] : candidateShaders)
            candidatePipelines.emplace(kind, CreatePipeline(device.Get(), candidateRoot.Get(), shader.Get()));

        D3D12_INDIRECT_ARGUMENT_DESC argumentDescription{};
        argumentDescription.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
        D3D12_COMMAND_SIGNATURE_DESC signatureDescription{};
        signatureDescription.ByteStride = sizeof(D3D12_DISPATCH_ARGUMENTS);
        signatureDescription.NumArgumentDescs = 1;
        signatureDescription.pArgumentDescs = &argumentDescription;
        Check(device->CreateCommandSignature(&signatureDescription, nullptr, IID_PPV_ARGS(&dispatchSignature)),
            "CreateCommandSignature");

        D3D12_QUERY_HEAP_DESC queryDescription{};
        queryDescription.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
        queryDescription.Count = 6;
        Check(device->CreateQueryHeap(&queryDescription, IID_PPV_ARGS(&queryHeap)), "CreateQueryHeap");
        queryReadback = CreateBuffer(device.Get(), D3D12_HEAP_TYPE_READBACK,
            sizeof(std::uint64_t) * 6u, D3D12_RESOURCE_STATE_COPY_DEST);

        motors = semantic::BuildCanonicalGlobalMotorPaletteV1();
        const auto motorBytes = spiral5::semantic::SerializeMotorRecordsV1(motors);
        points = spiral5::semantic::BuildCanonicalPointCorpusV1();
        const auto pointBytes = spiral5::semantic::SerializePointRecordsV1(points);
        zeroBytes = semantic::BuildCanonicalInactiveOutputBytesV1();
        motorUpload = CreateBuffer(device.Get(), D3D12_HEAP_TYPE_UPLOAD,
            motorBytes.size(), D3D12_RESOURCE_STATE_GENERIC_READ);
        pointUpload = CreateBuffer(device.Get(), D3D12_HEAP_TYPE_UPLOAD,
            pointBytes.size(), D3D12_RESOURCE_STATE_GENERIC_READ);
        zeroUpload = CreateBuffer(device.Get(), D3D12_HEAP_TYPE_UPLOAD,
            zeroBytes.size(), D3D12_RESOURCE_STATE_GENERIC_READ);
        UploadBytes(motorUpload.Get(), motorBytes);
        UploadBytes(pointUpload.Get(), pointBytes);
        UploadBytes(zeroUpload.Get(), zeroBytes);

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
            profile.driverVersion = static_cast<std::uint64_t>(driverVersion.QuadPart);
        D3D12_FEATURE_DATA_ARCHITECTURE1 architecture{};
        architecture.NodeIndex = 0;
        if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_ARCHITECTURE1,
            &architecture, sizeof(architecture))))
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

    std::array<CandidateProfileV1, CanonicalCandidateCountV1> BuildCandidateProfiles() const
    {
        std::array<CandidateProfileV1, CanonicalCandidateCountV1> values{};
        const auto context = fv::BuildCanonicalSparseFamilyVerificationContextV1();
        const auto kinds = CanonicalCandidateKindsV1();
        for (std::size_t index = 0; index < kinds.size(); ++index)
        {
            auto& value = values[index];
            value.kind = kinds[index];
            value.targetProfileIdentity = context.targetProfileIdentity;
            value.observationContractIdentity = context.observationContractIdentity;
            value.completionContractIdentity = context.completionContractIdentity;
            value.consumerProgramIdentity = fc::SparseFamilyConsumerProgramIdentityV1();
            value.representationResourceContractIdentity =
                fc::SparseFamilyRepresentationResourceContractIdentityV1(kinds[index]);
            value.representationShaderBytecodeDigest = BlobDigest(candidateShaders.at(kinds[index]).Get());
            value.argumentProducerShaderBytecodeDigest = BlobDigest(argumentShader.Get());
            switch (kinds[index])
            {
            case CandidateKindV1::DenseMembershipMaskPredicate:
                value.representationRole = fc::SparseFamilyRepresentationRoleV1::DenseMembershipMask4096;
                value.dispatchPolicy = fc::SparseFamilyDispatchPolicyV1::FixedUniverseGroups;
                value.payloadStrideBytes = 4;
                value.fixedCapacityBytes = fc::DenseMaskCapacityBytesV1;
                break;
            case CandidateKindV1::CompactIndexListDispatch:
                value.representationRole = fc::SparseFamilyRepresentationRoleV1::CompactSortedUniverseIndexList;
                value.dispatchPolicy = fc::SparseFamilyDispatchPolicyV1::CeilCardinalityGroups;
                value.payloadStrideBytes = 4;
                value.fixedCapacityBytes = fc::CompactIndexCapacityBytesV1;
                break;
            case CandidateKindV1::ActiveBlockListLocalMask:
                value.representationRole = fc::SparseFamilyRepresentationRoleV1::ActiveBlockListWithLocal64BitMask;
                value.dispatchPolicy = fc::SparseFamilyDispatchPolicyV1::OneGroupPerActiveBlock;
                value.payloadStrideBytes = fc::ActiveBlockRecordStrideBytesV1;
                value.fixedCapacityBytes = fc::ActiveBlockCapacityBytesV1;
                break;
            }
        }
        return values;
    }
};

struct CandidateAuthorityBundle final
{
    fc::RawSparseFamilyCandidateV1 raw;
    fv::VerifiedSparseFamilyCandidateV1 verified;
    fc::FrozenSparseFamilyArtifactV1 artifact;
    fe::FrozenVerifiedSparseFamilyCandidateV1 frozen;

    CandidateAuthorityBundle(
        fc::RawSparseFamilyCandidateV1 rawValue,
        fv::VerifiedSparseFamilyCandidateV1 verifiedValue,
        fc::FrozenSparseFamilyArtifactV1 artifactValue,
        fe::FrozenVerifiedSparseFamilyCandidateV1 frozenValue)
        : raw(std::move(rawValue)), verified(std::move(verifiedValue)),
          artifact(std::move(artifactValue)), frozen(std::move(frozenValue)) {}
};

std::vector<CandidateAuthorityBundle> BuildAuthorityBundles(
    const semantic::SparsePointTransformSemanticV1& semanticValue,
    const semantic::ExactSparseWorkSetV1& set,
    const fv::SparseFamilyVerificationContextV1& context)
{
    auto planned = fp::PlanCanonicalSparseCandidateFamilyV1(semanticValue, set);
    if (!planned) throw std::runtime_error(planned.Error());
    std::vector<CandidateAuthorityBundle> result;
    result.reserve(CanonicalCandidateCountV1);
    for (auto raw : planned.Value())
    {
        auto verified = fv::VerifySparseFamilyCandidateV1(semanticValue, set, context, raw);
        if (!verified) throw std::runtime_error(verified.Error());
        auto artifact = fc::BuildSparseFamilyArtifactV1(semanticValue, set, raw.kind);
        auto binding = fe::BuildCanonicalSparseFamilyResourceBindingInputV1(verified.Value(), 1);
        auto frozen = fe::FreezeVerifiedSparseFamilyCandidateV1(
            semanticValue, set, context, verified.Value(), artifact, binding);
        if (!frozen) throw std::runtime_error(frozen.Error());
        result.emplace_back(std::move(raw), verified.Value(), std::move(artifact), frozen.Value());
    }
    std::sort(result.begin(), result.end(), [](const auto& left, const auto& right)
    {
        return static_cast<std::uint32_t>(left.raw.kind) < static_cast<std::uint32_t>(right.raw.kind);
    });
    if (result.size() != CanonicalCandidateCountV1)
        throw std::runtime_error("Sparse Candidate authority family is incomplete.");
    return result;
}

CandidateCaseAuthorityV1 ToEvidenceAuthority(const CandidateAuthorityBundle& value)
{
    CandidateCaseAuthorityV1 result;
    result.kind = value.raw.kind;
    result.rawCandidateIdentity = value.raw.rawCandidateIdentity;
    result.verifiedPlanIdentity = value.verified.Plan().planIdentity;
    result.verifierCertificateIdentity = value.verified.CertificateIdentity();
    result.artifactIdentity = value.artifact.ArtifactIdentity();
    result.representationResourceIdentity = value.verified.Plan().operation.representationResourceIdentity;
    result.frozenBindingIdentity = value.frozen.BindingIdentity();
    return result;
}

double Nanoseconds(Clock::duration duration)
{
    return std::chrono::duration<double, std::nano>(duration).count();
}

TimingSampleV1 ExecuteMeasuredCandidate(
    DeviceContext& gpu,
    const semantic::SparsePointTransformSemanticV1& semanticValue,
    PatternKindV1 pattern,
    const semantic::ExactSparseWorkSetV1& set,
    const fv::SparseFamilyVerificationContextV1& context,
    const CandidateAuthorityBundle& authority,
    std::uint32_t run,
    std::uint32_t orderIndex,
    std::uint32_t cycle,
    std::uint32_t orderPosition)
{
    const auto cpuBegin = Clock::now();
    const auto validationBegin = Clock::now();
    auto valid = fe::ValidateFrozenVerifiedSparseFamilyCandidateV1(
        authority.frozen, semanticValue, set, context, 1);
    if (!valid) throw std::runtime_error(valid.Error());
    auto artifactValid = fc::ValidateSparseFamilyArtifactV1(authority.artifact, semanticValue, set);
    if (!artifactValid) throw std::runtime_error(artifactValid.Error());
    const auto validationEnd = Clock::now();

    const auto representationBegin = Clock::now();
    const auto rebuilt = fc::BuildSparseFamilyArtifactV1(semanticValue, set, authority.raw.kind);
    if (rebuilt.Bytes() != authority.artifact.Bytes())
        throw std::runtime_error("Measured Sparse representation rematerialization is not deterministic.");
    auto representationUpload = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_UPLOAD,
        rebuilt.PayloadBytes().size(), D3D12_RESOURCE_STATE_GENERIC_READ);
    UploadBytes(representationUpload.Get(), rebuilt.PayloadBytes());
    const auto representationEnd = Clock::now();

    auto argumentBuffer = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_DEFAULT,
        sizeof(D3D12_DISPATCH_ARGUMENTS), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    auto argumentReadback = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_READBACK,
        sizeof(D3D12_DISPATCH_ARGUMENTS), D3D12_RESOURCE_STATE_COPY_DEST);
    auto output = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_DEFAULT,
        gpu.zeroBytes.size(), D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    auto outputReadback = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_READBACK,
        gpu.zeroBytes.size(), D3D12_RESOURCE_STATE_COPY_DEST);

    const auto recordingBegin = Clock::now();
    gpu.Begin();
    gpu.commandList->EndQuery(gpu.queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0);
    gpu.commandList->CopyBufferRegion(output.Get(), 0, gpu.zeroUpload.Get(), 0, gpu.zeroBytes.size());
    auto outputToUav = Transition(output.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    gpu.commandList->ResourceBarrier(1, &outputToUav);
    gpu.commandList->EndQuery(gpu.queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 1);

    gpu.commandList->SetPipelineState(gpu.argumentPipeline.Get());
    gpu.commandList->SetComputeRootSignature(gpu.argumentRoot.Get());
    gpu.commandList->SetComputeRoot32BitConstant(0, authority.artifact.DispatchGroupsX(), 0);
    gpu.commandList->SetComputeRootUnorderedAccessView(1, argumentBuffer->GetGPUVirtualAddress());
    gpu.commandList->Dispatch(1, 1, 1);
    auto argumentUav = UavBarrier(argumentBuffer.Get());
    gpu.commandList->ResourceBarrier(1, &argumentUav);
    auto argumentToIndirect = Transition(argumentBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
    gpu.commandList->ResourceBarrier(1, &argumentToIndirect);
    gpu.commandList->EndQuery(gpu.queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 2);

    gpu.commandList->SetPipelineState(gpu.candidatePipelines.at(authority.raw.kind).Get());
    gpu.commandList->SetComputeRootSignature(gpu.candidateRoot.Get());
    gpu.commandList->SetComputeRoot32BitConstant(0, authority.artifact.ActiveCount(), 0);
    gpu.commandList->SetComputeRootShaderResourceView(1, representationUpload->GetGPUVirtualAddress());
    gpu.commandList->SetComputeRootShaderResourceView(2, gpu.motorUpload->GetGPUVirtualAddress());
    gpu.commandList->SetComputeRootShaderResourceView(3, gpu.pointUpload->GetGPUVirtualAddress());
    gpu.commandList->SetComputeRootUnorderedAccessView(4, output->GetGPUVirtualAddress());
    gpu.commandList->ExecuteIndirect(gpu.dispatchSignature.Get(), 1, argumentBuffer.Get(), 0, nullptr, 0);
    gpu.commandList->EndQuery(gpu.queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 3);

    auto outputUav = UavBarrier(output.Get());
    gpu.commandList->ResourceBarrier(1, &outputUav);
    auto outputToCopy = Transition(output.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_COPY_SOURCE);
    auto argumentToCopy = Transition(argumentBuffer.Get(), D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT,
        D3D12_RESOURCE_STATE_COPY_SOURCE);
    const std::array<D3D12_RESOURCE_BARRIER, 2> copyBarriers{outputToCopy, argumentToCopy};
    gpu.commandList->ResourceBarrier(static_cast<UINT>(copyBarriers.size()), copyBarriers.data());
    gpu.commandList->EndQuery(gpu.queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 4);

    gpu.commandList->CopyBufferRegion(outputReadback.Get(), 0, output.Get(), 0, gpu.zeroBytes.size());
    gpu.commandList->CopyBufferRegion(argumentReadback.Get(), 0, argumentBuffer.Get(), 0,
        sizeof(D3D12_DISPATCH_ARGUMENTS));
    gpu.commandList->EndQuery(gpu.queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 5);
    gpu.commandList->ResolveQueryData(gpu.queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP,
        0, 6, gpu.queryReadback.Get(), 0);
    const auto recordingEnd = Clock::now();

    const auto submitBegin = Clock::now();
    gpu.SubmitAndWait();
    const auto submitEnd = Clock::now();

    const auto observationBegin = Clock::now();
    const auto outputBytes = ReadbackBytes(outputReadback.Get(), gpu.zeroBytes.size());
    const auto argumentBytes = ReadbackBytes(argumentReadback.Get(), sizeof(D3D12_DISPATCH_ARGUMENTS));
    const auto timestampBytes = ReadbackBytes(gpu.queryReadback.Get(), sizeof(std::uint64_t) * 6u);
    std::array<std::uint64_t, 6> timestamps{};
    std::memcpy(timestamps.data(), timestampBytes.data(), timestampBytes.size());
    for (std::size_t index = 1; index < timestamps.size(); ++index)
        if (timestamps[index] < timestamps[index - 1])
            throw std::runtime_error("GPU timestamps are not monotonic.");
    D3D12_DISPATCH_ARGUMENTS observedArguments{};
    std::memcpy(&observedArguments, argumentBytes.data(), sizeof(observedArguments));

    const auto expected = semantic::BuildSparseCpuReferenceOutputV1(set);
    std::vector<spiral5::semantic::PointRecordV1> observed(semantic::WorkUniverseCountV1);
    std::memcpy(observed.data(), outputBytes.data(), outputBytes.size());

    TimingSampleV1 sample;
    sample.run = run;
    sample.orderIndex = orderIndex;
    sample.cycle = cycle;
    sample.orderPosition = orderPosition;
    sample.candidate = authority.raw.kind;
    sample.pattern = pattern;
    sample.activeCount = set.Cardinality().Value();
    sample.activeBlockCount = authority.artifact.ActiveBlockCount();
    sample.expectedDispatchGroupsX = authority.artifact.DispatchGroupsX();
    sample.observedDispatchGroupsX = observedArguments.ThreadGroupCountX;
    sample.outputDigest = base::Sha256(outputBytes);

    for (std::uint32_t index = 0; index < semantic::WorkUniverseCountV1; ++index)
    {
        const auto offset = static_cast<std::size_t>(index) * sizeof(spiral5::semantic::PointRecordV1);
        if (!set.Contains(index))
        {
            if (std::memcmp(outputBytes.data() + offset, gpu.zeroBytes.data() + offset,
                sizeof(spiral5::semantic::PointRecordV1)) != 0)
                ++sample.unauthorizedWriteCount;
            continue;
        }
        const auto& actual = observed[index];
        const auto& reference = expected[index];
        const bool allZero = std::bit_cast<std::uint32_t>(actual.x) == 0u &&
            std::bit_cast<std::uint32_t>(actual.y) == 0u &&
            std::bit_cast<std::uint32_t>(actual.z) == 0u &&
            std::bit_cast<std::uint32_t>(actual.w) == 0u;
        if (allZero) ++sample.missingWriteCount;
        else ++sample.activeWriteCount;
        const std::array<float, 4> a{actual.x, actual.y, actual.z, actual.w};
        const std::array<float, 4> e{reference.x, reference.y, reference.z, reference.w};
        bool recordMismatch = false;
        for (std::size_t component = 0; component < a.size(); ++component)
        {
            if (!std::isfinite(a[component])) ++sample.nonFiniteCount;
            const double absolute = std::abs(static_cast<double>(a[component]) - e[component]);
            const double denominator = std::max(std::abs(static_cast<double>(e[component])), 1.0e-30);
            const double relative = absolute / denominator;
            sample.maxAbsoluteError = std::max(sample.maxAbsoluteError, absolute);
            sample.maxRelativeError = std::max(sample.maxRelativeError, relative);
            sample.maxUlpError = std::max(sample.maxUlpError, UlpDistance(a[component], e[component]));
            if (absolute > 1.0e-6) recordMismatch = true;
        }
        if (recordMismatch) ++sample.activeMismatchCount;
        if (std::bit_cast<std::uint32_t>(actual.w) != std::bit_cast<std::uint32_t>(1.0f))
            ++sample.homogeneousCoordinateMismatchCount;
    }
    const auto observationEnd = Clock::now();
    const auto cpuEnd = observationEnd;

    const double tickToNanoseconds = 1.0e9 / static_cast<double>(gpu.timestampFrequency);
    auto phase = [&](std::size_t first, std::size_t second)
    {
        return static_cast<double>(timestamps[second] - timestamps[first]) * tickToNanoseconds;
    };
    sample.sparseInputValidationNanoseconds = Nanoseconds(validationEnd - validationBegin);
    sample.derivedRepresentationConstructionUploadNanoseconds = Nanoseconds(representationEnd - representationBegin);
    sample.commandRecordingNanoseconds = Nanoseconds(recordingEnd - recordingBegin);
    sample.submitAndWaitNanoseconds = Nanoseconds(submitEnd - submitBegin);
    sample.numericObservationNanoseconds = Nanoseconds(observationEnd - observationBegin);
    sample.cpuEndToEndNanoseconds = Nanoseconds(cpuEnd - cpuBegin);
    sample.gpuSentinelInitializationNanoseconds = phase(0, 1);
    sample.gpuIndirectArgumentGenerationNanoseconds = phase(1, 2);
    sample.gpuSparseConsumerExecutionNanoseconds = phase(2, 3);
    sample.gpuCompletionNanoseconds = phase(3, 4);
    sample.gpuObservationCopyNanoseconds = phase(4, 5);
    sample.gpuCandidatePathNanoseconds = phase(0, 4);
    sample.gpuTotalNanoseconds = phase(0, 5);

    if (sample.observedDispatchGroupsX != sample.expectedDispatchGroupsX ||
        observedArguments.ThreadGroupCountY != 1u || observedArguments.ThreadGroupCountZ != 1u ||
        sample.activeWriteCount != sample.activeCount || sample.missingWriteCount != 0 ||
        sample.unauthorizedWriteCount != 0 || sample.activeMismatchCount != 0 ||
        sample.nonFiniteCount != 0 || sample.homogeneousCoordinateMismatchCount != 0)
        throw std::runtime_error("Real-GPU Sparse observation failed exact write-set qualification.");
    return sample;
}
#endif
}

base::Result<MeasurementEvidenceV1, std::string>
RunRealGpuMeasurementV1(const MeasurementConfigV1& config)
{
#if !defined(_WIN32)
    (void)config;
    return base::Result<MeasurementEvidenceV1, std::string>::Failure(
        "Spiral 6 CU6 real-GPU measurement requires Windows D3D12.");
#else
    try
    {
        if (config.measurementCyclesPerOrder == 0 || config.runCount == 0)
            throw std::runtime_error("Measurement cycles and runs must be non-zero.");
        DeviceContext gpu(config.adapterIndex);
        const auto semanticValue = semantic::BuildCanonicalSparsePointTransformSemanticV1();
        const auto context = fv::BuildCanonicalSparseFamilyVerificationContextV1();
        const auto kinds = CanonicalCandidateKindsV1();
        const auto orders = CanonicalBalancedOrdersV1();

        MeasurementEvidenceV1 evidence;
        evidence.captureUnixNanoseconds = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        evidence.config = config;
        evidence.adapter = gpu.profile;
        evidence.architectureEvidenceIdentity = []
        {
            base::Digest256 value{};
            const std::string hex = "4a634317e988e3f3f9639f249a35af0ce25e18e19ea52a0500ce75ab09674b7e";
            for (std::size_t i = 0; i < value.size(); ++i)
            {
                const auto parse = [](char c) -> std::uint8_t
                {
                    return c <= '9' ? static_cast<std::uint8_t>(c - '0') : static_cast<std::uint8_t>(10 + c - 'a');
                };
                value[i] = static_cast<std::byte>((parse(hex[i * 2]) << 4u) | parse(hex[i * 2 + 1]));
            }
            return value;
        }();
        evidence.controlledRecoveryEvidenceIdentity = []
        {
            base::Digest256 value{};
            const std::string hex = "65aec68060dd68ae42a191e79c5cf2fffc8538442b8d6c029fe59927709bd77f";
            for (std::size_t i = 0; i < value.size(); ++i)
            {
                const auto parse = [](char c) -> std::uint8_t
                {
                    return c <= '9' ? static_cast<std::uint8_t>(c - '0') : static_cast<std::uint8_t>(10 + c - 'a');
                };
                value[i] = static_cast<std::byte>((parse(hex[i * 2]) << 4u) | parse(hex[i * 2 + 1]));
            }
            return value;
        }();
        evidence.freshRematerializationEvidenceIdentity = []
        {
            base::Digest256 value{};
            const std::string hex = "a9398025d0c5235e2370bfd5d0c34bdfcdc114836a763f8b6151bd95d80ff032";
            for (std::size_t i = 0; i < value.size(); ++i)
            {
                const auto parse = [](char c) -> std::uint8_t
                {
                    return c <= '9' ? static_cast<std::uint8_t>(c - '0') : static_cast<std::uint8_t>(10 + c - 'a');
                };
                value[i] = static_cast<std::byte>((parse(hex[i * 2]) << 4u) | parse(hex[i * 2 + 1]));
            }
            return value;
        }();
        evidence.semanticIdentity = semantic::SparsePointTransformSemanticIdentityV1(semanticValue);
        evidence.verificationContextIdentity = ContextIdentity(context);
        evidence.measurementProfileIdentity = BuildMeasurementProfileIdentityV1(config);
        evidence.orderBalanceIdentity = BuildOrderBalanceIdentityV1();
        evidence.candidates = gpu.BuildCandidateProfiles();

        for (PatternKindV1 pattern : CanonicalPatternsV1())
        {
            for (std::uint32_t activeCount : CanonicalMeasurementCardinalitiesV1())
            {
                auto setResult = semantic::BuildCanonicalSparseWorkSetV1(
                    pattern, semantic::SparseCardinalityV1(activeCount));
                if (!setResult) throw std::runtime_error(setResult.Error().message);
                const auto& set = setResult.Value();
                auto authorities = BuildAuthorityBundles(semanticValue, set, context);
                MeasurementCaseV1 measurementCase;
                measurementCase.pattern = pattern;
                measurementCase.activeCount = activeCount;
                measurementCase.activeBlockCount = authorities[2].artifact.ActiveBlockCount();
                measurementCase.sparseSetIdentity = set.Identity();
                for (std::size_t candidateIndex = 0; candidateIndex < authorities.size(); ++candidateIndex)
                    measurementCase.authorities[candidateIndex] = ToEvidenceAuthority(authorities[candidateIndex]);

                for (std::uint32_t run = 0; run < config.runCount; ++run)
                {
                    for (std::uint32_t orderIndex = 0; orderIndex < orders.size(); ++orderIndex)
                    {
                        const auto& order = orders[orderIndex];
                        for (std::uint32_t warmup = 0; warmup < config.warmupCyclesPerOrder; ++warmup)
                        {
                            for (std::uint32_t position = 0; position < CanonicalCandidateCountV1; ++position)
                            {
                                (void)ExecuteMeasuredCandidate(gpu, semanticValue, pattern, set, context,
                                    authorities[order[position]], run, orderIndex, warmup, position);
                            }
                        }
                        for (std::uint32_t cycle = 0; cycle < config.measurementCyclesPerOrder; ++cycle)
                        {
                            for (std::uint32_t position = 0; position < CanonicalCandidateCountV1; ++position)
                            {
                                measurementCase.samples.push_back(ExecuteMeasuredCandidate(
                                    gpu, semanticValue, pattern, set, context, authorities[order[position]],
                                    run, orderIndex, cycle, position));
                            }
                        }
                    }
                }
                for (CandidateKindV1 kind : kinds)
                {
                    std::vector<base::Digest256> digests;
                    for (const auto& sample : measurementCase.samples)
                        if (sample.candidate == kind) digests.push_back(sample.outputDigest);
                    if (digests.empty() || !std::all_of(digests.begin(), digests.end(),
                        [&](const auto& value) { return value == digests.front(); }))
                        throw std::runtime_error("Candidate output digest changed across repeated measurements.");
                }
                const auto firstDigest = measurementCase.samples.front().outputDigest;
                if (!std::all_of(measurementCase.samples.begin(), measurementCase.samples.end(),
                    [&](const auto& sample) { return sample.outputDigest == firstDigest; }))
                    throw std::runtime_error("A/B/C output digests are not byte-identical for one Sparse set.");
                evidence.cases.push_back(std::move(measurementCase));
            }
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
