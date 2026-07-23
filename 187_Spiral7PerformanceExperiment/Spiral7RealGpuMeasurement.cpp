#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "Spiral7PerformanceExperiment.h"

#include "../00_Foundation/BinaryIO.h"
#include "../154_SparseWorkSetSemantic/SparseWorkSetSemantic.h"
#include "../179_SparseTemporalDeltaCandidateFamilyPlanner/DeltaCandidateFamilyPlanner.h"
#include "../180_SparseTemporalDeltaCandidateFamilyVerifier/DeltaCandidateFamilyVerifier.h"

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
#include <iostream>
#include <limits>
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

namespace sge4_5::spiral7::performance
{
namespace fc = family_candidate;
namespace fp = family_planner;
namespace fv = family_verification;
namespace s6 = spiral6;

namespace
{
[[maybe_unused]] constexpr std::string_view AcceptedArchitectureDigest =
    "1f1d09b4e52dbc35e961e0d3751b32292b2c30eedcfa473800c5bb8e8cb5ab73";
[[maybe_unused]] constexpr std::string_view AcceptedControlledDigest =
    "7f0247b193f9bc20f4eda5feed590c1da5722ef36b133640292b1ed616cff62b";
[[maybe_unused]] constexpr std::string_view AcceptedFreshDigest =
    "091eaec0d27287fe897f6813043fd75c090d5da17054461df314f99b2a6f5a92";

void Require(bool condition, std::string_view message)
{
    if (!condition) throw std::runtime_error(std::string(message));
}

std::uint8_t HexDigit(char value)
{
    if (value >= '0' && value <= '9') return static_cast<std::uint8_t>(value - '0');
    if (value >= 'a' && value <= 'f') return static_cast<std::uint8_t>(10 + value - 'a');
    if (value >= 'A' && value <= 'F') return static_cast<std::uint8_t>(10 + value - 'A');
    throw std::runtime_error("Invalid hexadecimal digest digit.");
}

[[maybe_unused]] base::Digest256 DigestFromHex(std::string_view text)
{
    Require(text.size() == 64, "SHA-256 text must contain 64 hexadecimal digits.");
    base::Digest256 digest{};
    for (std::size_t index = 0; index < digest.size(); ++index)
    {
        const auto high = HexDigit(text[index * 2]);
        const auto low = HexDigit(text[index * 2 + 1]);
        digest[index] = static_cast<std::byte>((high << 4) | low);
    }
    return digest;
}

[[maybe_unused]] std::uint64_t UnixNanosecondsNow()
{
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}

semantic::ExactSparseWorkSetV1 BuildExact(std::span<const std::uint32_t> indices)
{
    auto result = s6::semantic::BuildExactSparseWorkSetV1(indices);
    if (!result) throw std::runtime_error(result.Error().stage + ": " + result.Error().message);
    return std::move(result).Value();
}

semantic::ExactSparseWorkSetV1 BuildExact(std::initializer_list<std::uint32_t> indices)
{
    return BuildExact(std::span<const std::uint32_t>(indices.begin(), indices.size()));
}

semantic::ExactSparseWorkSetV1 BuildSet(PatternKindV1 pattern, std::uint32_t count)
{
    auto result = s6::semantic::BuildCanonicalSparseWorkSetV1(
        pattern, s6::semantic::SparseCardinalityV1(count));
    if (!result) throw std::runtime_error(result.Error().stage + ": " + result.Error().message);
    return std::move(result).Value();
}

std::vector<std::uint32_t> FirstIndices(
    const semantic::ExactSparseWorkSetV1& set,
    std::uint32_t count)
{
    Require(count <= set.Indices().size(), "Requested prefix exceeds exact set cardinality.");
    return std::vector<std::uint32_t>(set.Indices().begin(),
        set.Indices().begin() + static_cast<std::ptrdiff_t>(count));
}

std::vector<std::uint32_t> ComplementPrefix(
    const semantic::ExactSparseWorkSetV1& set,
    std::uint32_t count)
{
    std::vector<std::uint32_t> result;
    result.reserve(count);
    for (std::uint32_t index = 0;
         index < semantic::WorkUniverseCountV1 && result.size() < count;
         ++index)
    {
        if (!set.Contains(index)) result.push_back(index);
    }
    Require(result.size() == count, "Unable to construct requested disjoint previous Active Set.");
    return result;
}

struct BuiltMeasurementCase final
{
    MeasurementCaseKeyV1 key;
    semantic::SparseDeltaInvocationV1 invocation;
    std::vector<std::byte> previousHistoryBytes;
    std::vector<fc::RawDeltaFamilyCandidateV1> proposals;
    std::vector<fc::FrozenDeltaFamilyArtifactV1> artifacts;
    std::vector<fv::VerifiedDeltaFamilyCandidateV1> verified;

    BuiltMeasurementCase(
        MeasurementCaseKeyV1 keyValue,
        semantic::SparseDeltaInvocationV1 invocationValue,
        std::vector<std::byte> historyValue,
        std::vector<fc::RawDeltaFamilyCandidateV1> proposalsValue,
        std::vector<fc::FrozenDeltaFamilyArtifactV1> artifactsValue,
        std::vector<fv::VerifiedDeltaFamilyCandidateV1> verifiedValue)
        : key(std::move(keyValue)),
          invocation(std::move(invocationValue)),
          previousHistoryBytes(std::move(historyValue)),
          proposals(std::move(proposalsValue)),
          artifacts(std::move(artifactsValue)),
          verified(std::move(verifiedValue)) {}
};

[[maybe_unused]] BuiltMeasurementCase BuildMeasurementCase(
    const semantic::SparseTemporalDeltaSemanticV1& semanticValue,
    const fv::DeltaFamilyVerificationContextV1& context,
    PatternKindV1 pattern,
    std::uint32_t activeCount,
    std::uint32_t transitionCount)
{
    auto active = BuildSet(pattern, activeCount);
    semantic::ExactSparseWorkSetV1 previous = active;
    semantic::ExactSparseWorkSetV1 modified = BuildExact({});
    TransitionShapeV1 shape = TransitionShapeV1::Hold;

    if (transitionCount == 0)
    {
        shape = TransitionShapeV1::Hold;
    }
    else if (transitionCount <= activeCount)
    {
        const auto modifiedIndices = FirstIndices(active, transitionCount);
        modified = BuildExact(modifiedIndices);
        shape = TransitionShapeV1::DirtyOnly;
    }
    else
    {
        const auto previousIndices = ComplementPrefix(active, transitionCount - activeCount);
        previous = BuildExact(previousIndices);
        shape = TransitionShapeV1::ReplaceAndClear;
    }

    auto seedResult = semantic::BuildSparseDeltaInvocationV1(
        0, BuildExact({}), previous, BuildExact({}),
        semantic::BuildInitialInvalidGenerationsV1(), true);
    if (!seedResult)
        throw std::runtime_error(seedResult.Error().stage + ": " + seedResult.Error().message);
    auto seed = std::move(seedResult).Value();
    const auto previousPoints = semantic::BuildCpuReferenceOutputV1(seed);
    auto previousHistory = spiral5::semantic::SerializePointRecordsV1(previousPoints);

    auto invocationResult = semantic::BuildSparseDeltaInvocationV1(
        1, previous, active, modified, seed.ItemGenerations(), false);
    if (!invocationResult)
        throw std::runtime_error(invocationResult.Error().stage + ": " + invocationResult.Error().message);
    auto invocation = std::move(invocationResult).Value();
    Require(invocation.ActiveSet().Cardinality().Value() == activeCount,
            "Measurement Active count construction mismatch.");
    Require(invocation.TransitionSet().Cardinality().Value() == transitionCount,
            "Measurement Transition count construction mismatch.");

    auto plannedResult = fp::PlanCanonicalDeltaCandidateFamilyV1(semanticValue, invocation);
    if (!plannedResult) throw std::runtime_error(plannedResult.Error());
    auto proposals = std::move(plannedResult).Value();
    Require(proposals.size() == CanonicalCandidateCountV1,
            "Measurement Planner did not produce exactly A/B/C.");

    std::vector<fc::FrozenDeltaFamilyArtifactV1> artifacts;
    std::vector<fv::VerifiedDeltaFamilyCandidateV1> verified;
    artifacts.reserve(CanonicalCandidateCountV1);
    verified.reserve(CanonicalCandidateCountV1);
    for (const auto& proposal : proposals)
    {
        auto verifiedResult = fv::VerifyDeltaFamilyCandidateV1(
            semanticValue, invocation, context, proposal);
        if (!verifiedResult) throw std::runtime_error(verifiedResult.Error());
        verified.push_back(std::move(verifiedResult).Value());
        artifacts.push_back(fc::BuildDeltaFamilyArtifactV1(
            semanticValue, invocation, proposal.kind));
    }

    MeasurementCaseKeyV1 key;
    key.pattern = pattern;
    key.activeCount = activeCount;
    key.transitionCount = transitionCount;
    key.updateCount = invocation.UpdateSet().Cardinality().Value();
    key.clearCount = invocation.DeactivationSet().Cardinality().Value();
    key.affectedBlockCount = artifacts[2].AffectedBlockCount();
    key.transitionShape = shape;
    key.previousActiveSetIdentity = invocation.PreviousActiveSet().Identity();
    key.activeSetIdentity = invocation.ActiveSet().Identity();
    key.modifiedSetIdentity = invocation.ModifiedSurvivorSet().Identity();
    key.transitionSetIdentity = invocation.TransitionSet().Identity();
    key.invocationIdentity = invocation.Identity();
    return BuiltMeasurementCase(
        std::move(key), std::move(invocation), std::move(previousHistory),
        std::move(proposals), std::move(artifacts), std::move(verified));
}

#if defined(_WIN32)
using Microsoft::WRL::ComPtr;

std::string HResultText(HRESULT value)
{
    std::ostringstream stream;
    stream << "HRESULT 0x" << std::hex << std::uppercase
           << static_cast<unsigned long>(value);
    return stream.str();
}

void Check(HRESULT value, std::string_view stage)
{
    if (FAILED(value)) throw std::runtime_error(std::string(stage) + ": " + HResultText(value));
}

std::string WideToUtf8(const wchar_t* text)
{
    if (!text) return {};
    const int count = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (count <= 1) return {};
    std::string result(static_cast<std::size_t>(count), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, result.data(), count, nullptr, nullptr);
    result.pop_back();
    return result;
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

ComPtr<ID3DBlob> CompileShader(
    std::string_view source,
    std::string_view sourceName,
    bool writeAudit)
{
    const std::string prefix = writeAudit ? "#define WRITE_AUDIT 1\n" : "#define WRITE_AUDIT 0\n";
    const std::string complete = prefix + std::string(source);
    ComPtr<ID3DBlob> shader;
    ComPtr<ID3DBlob> errors;
    const UINT flags = D3DCOMPILE_ENABLE_STRICTNESS |
                       D3DCOMPILE_WARNINGS_ARE_ERRORS |
                       D3DCOMPILE_OPTIMIZATION_LEVEL3;
    const HRESULT result = D3DCompile(
        complete.data(), complete.size(), sourceName.data(), nullptr, nullptr,
        "main", "cs_5_1", flags, 0, &shader, &errors);
    if (FAILED(result))
    {
        const std::string message = errors
            ? std::string(static_cast<const char*>(errors->GetBufferPointer()),
                          errors->GetBufferSize())
            : HResultText(result);
        throw std::runtime_error("D3DCompile(" + std::string(sourceName) + "): " + message);
    }
    return shader;
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
void audit(uint index, uint value)
{
#if WRITE_AUDIT
    WriteAudit[index] = value;
#endif
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
    audit(universeIndex, 3);
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
        audit(universeIndex, 1);
        Output[universeIndex] = transformPoint(universeIndex);
    }
    else if (action == 2)
    {
        audit(universeIndex, 2);
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
        audit(universeIndex, 1);
        Output[universeIndex] = transformPoint(universeIndex);
    }
    else if (clear)
    {
        audit(universeIndex, 2);
        Output[universeIndex] = float4(0,0,0,0);
    }
}
)";
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
    description.Format = DXGI_FORMAT_UNKNOWN;
    description.SampleDesc.Count = 1;
    description.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    description.Flags = flags;
    ComPtr<ID3D12Resource> resource;
    Check(device->CreateCommittedResource(
        &heap, D3D12_HEAP_FLAG_NONE, &description, state, nullptr,
        IID_PPV_ARGS(&resource)), "CreateCommittedResource");
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
    D3D12_RANGE range{0, size};
    Check(resource->Map(0, &range, &mapped), "Map(readback)");
    const auto* first = static_cast<const std::byte*>(mapped);
    std::vector<std::byte> result(first, first + size);
    D3D12_RANGE noWrite{0, 0};
    resource->Unmap(0, &noWrite);
    return result;
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

enum class OutputState
{
    CopyDestination,
    UnorderedAccess,
    CopySource
};

struct GpuContext final
{
    ComPtr<IDXGIFactory6> factory;
    ComPtr<IDXGIAdapter4> adapter;
    ComPtr<ID3D12Device5> device;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> commandList;
    ComPtr<ID3D12Fence> fence;
    EventHandle fenceEvent;
    std::uint64_t fenceValue = 0;
    ComPtr<ID3D12RootSignature> rootSignature;
    std::array<ComPtr<ID3D12PipelineState>, CanonicalCandidateCountV1> validationPipelines;
    std::array<ComPtr<ID3D12PipelineState>, CanonicalCandidateCountV1> timingPipelines;
    std::array<base::Digest256, CanonicalCandidateCountV1> validationShaderDigests{};
    std::array<base::Digest256, CanonicalCandidateCountV1> timingShaderDigests{};
    ComPtr<ID3D12CommandSignature> dispatchSignature;
    ComPtr<ID3D12QueryHeap> timestampHeap;
    ComPtr<ID3D12Resource> timestampReadback;
    AdapterProfileV1 profile;

    GpuContext(const MeasurementConfigV1& config)
    {
        Check(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)), "CreateDXGIFactory2");
        std::uint32_t hardwareOrdinal = 0;
        for (UINT index = 0;; ++index)
        {
            ComPtr<IDXGIAdapter4> candidate;
            if (factory->EnumAdapterByGpuPreference(
                    index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                    IID_PPV_ARGS(&candidate)) == DXGI_ERROR_NOT_FOUND) break;
            DXGI_ADAPTER_DESC3 description{};
            Check(candidate->GetDesc3(&description), "IDXGIAdapter4::GetDesc3");
            if ((description.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE) != 0) continue;
            if (FAILED(D3D12CreateDevice(candidate.Get(), D3D_FEATURE_LEVEL_11_0,
                                         __uuidof(ID3D12Device), nullptr))) continue;
            if (hardwareOrdinal++ == config.adapterIndex)
            {
                adapter = candidate;
                profile.description = WideToUtf8(description.Description);
                profile.vendorId = description.VendorId;
                profile.deviceId = description.DeviceId;
                profile.subsystemId = description.SubSysId;
                profile.revision = description.Revision;
                profile.luidLow = description.AdapterLuid.LowPart;
                profile.luidHigh = description.AdapterLuid.HighPart;
                break;
            }
        }
        Require(adapter.Get() != nullptr, "Requested real hardware D3D12 adapter was not found.");
        Check(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
                                IID_PPV_ARGS(&device)), "D3D12CreateDevice(real GPU)");

        LARGE_INTEGER driver{};
        if (SUCCEEDED(adapter->CheckInterfaceSupport(__uuidof(ID3D12Device), &driver)))
            profile.driverVersion = static_cast<std::uint64_t>(driver.QuadPart);

        D3D12_FEATURE_DATA_ARCHITECTURE1 architecture{};
        architecture.NodeIndex = 0;
        if (SUCCEEDED(device->CheckFeatureSupport(
                D3D12_FEATURE_ARCHITECTURE1, &architecture, sizeof(architecture))))
        {
            profile.uma = architecture.UMA != FALSE;
            profile.cacheCoherentUma = architecture.CacheCoherentUMA != FALSE;
        }
        if (config.requestStablePowerStateDiagnostic)
            profile.stablePowerStateApplied = SUCCEEDED(device->SetStablePowerState(TRUE));

        D3D12_COMMAND_QUEUE_DESC queueDescription{};
        queueDescription.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        Check(device->CreateCommandQueue(&queueDescription, IID_PPV_ARGS(&queue)),
              "CreateCommandQueue");
        Check(queue->GetTimestampFrequency(&profile.timestampFrequency),
              "ID3D12CommandQueue::GetTimestampFrequency");
        Check(device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)),
            "CreateCommandAllocator");
        Check(device->CreateCommandList(
            0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr,
            IID_PPV_ARGS(&commandList)), "CreateCommandList");
        Check(commandList->Close(), "Initial CommandList Close");
        Check(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)),
              "CreateFence");

        std::array<D3D12_ROOT_PARAMETER, 6> parameters{};
        parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        parameters[0].Constants.ShaderRegister = 0;
        parameters[0].Constants.RegisterSpace = 0;
        parameters[0].Constants.Num32BitValues = 1;
        parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        for (std::uint32_t index = 0; index < 3; ++index)
        {
            parameters[index + 1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
            parameters[index + 1].Descriptor.ShaderRegister = index;
            parameters[index + 1].Descriptor.RegisterSpace = 0;
            parameters[index + 1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        }
        parameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
        parameters[4].Descriptor.ShaderRegister = 0;
        parameters[4].Descriptor.RegisterSpace = 0;
        parameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        parameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
        parameters[5].Descriptor.ShaderRegister = 1;
        parameters[5].Descriptor.RegisterSpace = 0;
        parameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        D3D12_ROOT_SIGNATURE_DESC rootDescription{};
        rootDescription.NumParameters = static_cast<UINT>(parameters.size());
        rootDescription.pParameters = parameters.data();
        ComPtr<ID3DBlob> serializedRoot;
        ComPtr<ID3DBlob> rootErrors;
        const HRESULT rootResult = D3D12SerializeRootSignature(
            &rootDescription, D3D_ROOT_SIGNATURE_VERSION_1, &serializedRoot, &rootErrors);
        if (FAILED(rootResult))
        {
            const std::string message = rootErrors
                ? std::string(static_cast<const char*>(rootErrors->GetBufferPointer()),
                              rootErrors->GetBufferSize())
                : HResultText(rootResult);
            throw std::runtime_error("D3D12SerializeRootSignature: " + message);
        }
        Check(device->CreateRootSignature(
            0, serializedRoot->GetBufferPointer(), serializedRoot->GetBufferSize(),
            IID_PPV_ARGS(&rootSignature)), "CreateRootSignature");

        const std::array<std::string, CanonicalCandidateCountV1> sources{
            DenseShader(), CompactShader(), BlockShader()};
        const std::array<std::string, CanonicalCandidateCountV1> names{
            "Spiral7FullActiveDenseRecomputeV1",
            "Spiral7CompactDeltaIndexHistoryReuseV1",
            "Spiral7AffectedBlockDeltaHistoryReuseV1"};
        for (std::size_t index = 0; index < sources.size(); ++index)
        {
            auto validationShader = CompileShader(sources[index], names[index] + ".Validation", true);
            auto timingShader = CompileShader(sources[index], names[index] + ".Timing", false);
            validationShaderDigests[index] = base::Sha256(std::span<const std::byte>(
                static_cast<const std::byte*>(validationShader->GetBufferPointer()),
                validationShader->GetBufferSize()));
            timingShaderDigests[index] = base::Sha256(std::span<const std::byte>(
                static_cast<const std::byte*>(timingShader->GetBufferPointer()),
                timingShader->GetBufferSize()));
            D3D12_COMPUTE_PIPELINE_STATE_DESC description{};
            description.pRootSignature = rootSignature.Get();
            description.CS = {validationShader->GetBufferPointer(),
                              validationShader->GetBufferSize()};
            Check(device->CreateComputePipelineState(
                &description, IID_PPV_ARGS(&validationPipelines[index])),
                "Create validation ComputePipelineState");
            description.CS = {timingShader->GetBufferPointer(), timingShader->GetBufferSize()};
            Check(device->CreateComputePipelineState(
                &description, IID_PPV_ARGS(&timingPipelines[index])),
                "Create timing ComputePipelineState");
        }

        D3D12_INDIRECT_ARGUMENT_DESC indirectArgument{};
        indirectArgument.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
        D3D12_COMMAND_SIGNATURE_DESC signatureDescription{};
        signatureDescription.ByteStride = 12;
        signatureDescription.NumArgumentDescs = 1;
        signatureDescription.pArgumentDescs = &indirectArgument;
        Check(device->CreateCommandSignature(
            &signatureDescription, nullptr, IID_PPV_ARGS(&dispatchSignature)),
            "CreateCommandSignature");

        D3D12_QUERY_HEAP_DESC queryDescription{};
        queryDescription.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
        queryDescription.Count = 6;
        Check(device->CreateQueryHeap(&queryDescription, IID_PPV_ARGS(&timestampHeap)),
              "CreateQueryHeap(timestamp)");
        timestampReadback = CreateBuffer(
            device.Get(), D3D12_HEAP_TYPE_READBACK, 6u * sizeof(std::uint64_t),
            D3D12_RESOURCE_STATE_COPY_DEST);

        base::BinaryWriter fingerprint;
        fingerprint.WriteU32(profile.vendorId); fingerprint.WriteU32(profile.deviceId);
        fingerprint.WriteU32(profile.subsystemId); fingerprint.WriteU32(profile.revision);
        fingerprint.WriteU32(profile.luidLow); fingerprint.WriteI32(profile.luidHigh);
        fingerprint.WriteU64(profile.driverVersion); fingerprint.WriteU64(profile.timestampFrequency);
        fingerprint.WriteU8(profile.uma ? 1u : 0u);
        fingerprint.WriteU8(profile.cacheCoherentUma ? 1u : 0u);
        fingerprint.WriteBytes(std::as_bytes(std::span(
            profile.description.data(), profile.description.size())));
        profile.fingerprint = base::Sha256(fingerprint.Bytes());
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
            Check(fence->SetEventOnCompletion(fenceValue, fenceEvent.value),
                  "Fence::SetEventOnCompletion");
            WaitForSingleObject(fenceEvent.value, INFINITE);
        }
    }
};

struct ReusableResources final
{
    ComPtr<ID3D12Resource> motorUpload;
    ComPtr<ID3D12Resource> pointUpload;
    ComPtr<ID3D12Resource> historyUpload;
    ComPtr<ID3D12Resource> zeroAuditUpload;
    std::array<ComPtr<ID3D12Resource>, CanonicalCandidateCountV1> payloadUploads;
    std::array<ComPtr<ID3D12Resource>, CanonicalCandidateCountV1> argumentUploads;
    std::array<ComPtr<ID3D12Resource>, CanonicalCandidateCountV1> outputs;
    std::array<ComPtr<ID3D12Resource>, CanonicalCandidateCountV1> audits;
    std::array<ComPtr<ID3D12Resource>, CanonicalCandidateCountV1> outputReadbacks;
    std::array<ComPtr<ID3D12Resource>, CanonicalCandidateCountV1> auditReadbacks;
    std::array<OutputState, CanonicalCandidateCountV1> outputStates{};
    std::array<OutputState, CanonicalCandidateCountV1> auditStates{};
    std::size_t historySize = 0;
    std::size_t auditSize = 0;

    explicit ReusableResources(GpuContext& gpu)
    {
        const auto motors = s6::semantic::BuildCanonicalGlobalMotorPaletteV1();
        const auto motorBytes = spiral5::semantic::SerializeMotorRecordsV1(motors);
        const auto history = semantic::BuildCanonicalInactiveHistoryBytesV1();
        historySize = history.size();
        const std::vector<std::uint32_t> zeroAudit(semantic::WorkUniverseCountV1, 0u);
        const auto zeroAuditBytes = std::as_bytes(std::span(zeroAudit));
        auditSize = zeroAuditBytes.size();
        motorUpload = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_UPLOAD,
                                   motorBytes.size(), D3D12_RESOURCE_STATE_GENERIC_READ);
        pointUpload = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_UPLOAD,
                                   historySize, D3D12_RESOURCE_STATE_GENERIC_READ);
        historyUpload = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_UPLOAD,
                                     historySize, D3D12_RESOURCE_STATE_GENERIC_READ);
        zeroAuditUpload = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_UPLOAD,
                                       auditSize, D3D12_RESOURCE_STATE_GENERIC_READ);
        UploadBytes(motorUpload.Get(), motorBytes);
        UploadBytes(zeroAuditUpload.Get(), zeroAuditBytes);
        const std::array<std::uint64_t, CanonicalCandidateCountV1> capacities{
            fc::DenseActiveMaskCapacityBytesV1,
            fc::CompactDeltaCapacityBytesV1,
            fc::AffectedBlockCapacityBytesV1};
        for (std::size_t index = 0; index < CanonicalCandidateCountV1; ++index)
        {
            payloadUploads[index] = CreateBuffer(
                gpu.device.Get(), D3D12_HEAP_TYPE_UPLOAD, capacities[index],
                D3D12_RESOURCE_STATE_GENERIC_READ);
            argumentUploads[index] = CreateBuffer(
                gpu.device.Get(), D3D12_HEAP_TYPE_UPLOAD, 12,
                D3D12_RESOURCE_STATE_GENERIC_READ);
            outputs[index] = CreateBuffer(
                gpu.device.Get(), D3D12_HEAP_TYPE_DEFAULT, historySize,
                D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
            audits[index] = CreateBuffer(
                gpu.device.Get(), D3D12_HEAP_TYPE_DEFAULT, auditSize,
                D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
            outputReadbacks[index] = CreateBuffer(
                gpu.device.Get(), D3D12_HEAP_TYPE_READBACK, historySize,
                D3D12_RESOURCE_STATE_COPY_DEST);
            auditReadbacks[index] = CreateBuffer(
                gpu.device.Get(), D3D12_HEAP_TYPE_READBACK, auditSize,
                D3D12_RESOURCE_STATE_COPY_DEST);
            outputStates[index] = OutputState::CopyDestination;
            auditStates[index] = OutputState::CopyDestination;
        }
    }
};

D3D12_RESOURCE_STATES NativeState(OutputState state)
{
    switch (state)
    {
    case OutputState::CopyDestination: return D3D12_RESOURCE_STATE_COPY_DEST;
    case OutputState::UnorderedAccess: return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    case OutputState::CopySource: return D3D12_RESOURCE_STATE_COPY_SOURCE;
    }
    return D3D12_RESOURCE_STATE_COMMON;
}

void TransitionResource(
    ID3D12GraphicsCommandList* commandList,
    ID3D12Resource* resource,
    OutputState& current,
    OutputState target)
{
    if (current == target) return;
    const auto barrier = Transition(resource, NativeState(current), NativeState(target));
    commandList->ResourceBarrier(1, &barrier);
    current = target;
}

std::uint32_t RootCount(const fc::FrozenDeltaFamilyArtifactV1& artifact)
{
    switch (artifact.Kind())
    {
    case CandidateKindV1::FullActiveDenseRecompute: return semantic::WorkUniverseCountV1;
    case CandidateKindV1::CompactDeltaIndexHistoryReuse: return artifact.TransitionCount();
    case CandidateKindV1::AffectedBlockDeltaHistoryReuse: return artifact.AffectedBlockCount();
    }
    throw std::runtime_error("Unknown Candidate artifact kind.");
}

void BindAndDispatch(
    GpuContext& gpu,
    ReusableResources& resources,
    const BuiltMeasurementCase& testCase,
    std::size_t candidateIndex,
    bool validation)
{
    const auto& artifact = testCase.artifacts[candidateIndex];
    gpu.commandList->SetPipelineState(
        validation ? gpu.validationPipelines[candidateIndex].Get()
                   : gpu.timingPipelines[candidateIndex].Get());
    gpu.commandList->SetComputeRootSignature(gpu.rootSignature.Get());
    gpu.commandList->SetComputeRoot32BitConstant(0, RootCount(artifact), 0);
    gpu.commandList->SetComputeRootShaderResourceView(
        1, resources.payloadUploads[candidateIndex]->GetGPUVirtualAddress());
    gpu.commandList->SetComputeRootShaderResourceView(
        2, resources.motorUpload->GetGPUVirtualAddress());
    gpu.commandList->SetComputeRootShaderResourceView(
        3, resources.pointUpload->GetGPUVirtualAddress());
    gpu.commandList->SetComputeRootUnorderedAccessView(
        4, resources.outputs[candidateIndex]->GetGPUVirtualAddress());
    gpu.commandList->SetComputeRootUnorderedAccessView(
        5, resources.audits[candidateIndex]->GetGPUVirtualAddress());
    gpu.commandList->ExecuteIndirect(
        gpu.dispatchSignature.Get(), 1,
        resources.argumentUploads[candidateIndex].Get(), 0, nullptr, 0);
}

void UploadCase(
    ReusableResources& resources,
    const BuiltMeasurementCase& testCase)
{
    Require(testCase.artifacts.size() == CanonicalCandidateCountV1 &&
            testCase.verified.size() == CanonicalCandidateCountV1 &&
            testCase.proposals.size() == CanonicalCandidateCountV1,
            "Measurement case does not contain exactly A/B/C.");
    const auto points = semantic::BuildPointCorpusForInvocationV1(testCase.invocation);
    const auto pointBytes = spiral5::semantic::SerializePointRecordsV1(points);
    Require(pointBytes.size() == resources.historySize,
            "Measurement point corpus byte size differs from output history size.");
    Require(testCase.previousHistoryBytes.size() == resources.historySize,
            "Previous History byte size differs from fixed output history size.");
    UploadBytes(resources.pointUpload.Get(), pointBytes);
    UploadBytes(resources.historyUpload.Get(), testCase.previousHistoryBytes);
    const auto context = fv::BuildCanonicalDeltaFamilyVerificationContextV1();
    for (std::size_t index = 0; index < CanonicalCandidateCountV1; ++index)
    {
        const auto artifactValid = fc::ValidateDeltaFamilyArtifactV1(
            testCase.artifacts[index],
            semantic::BuildCanonicalSparseTemporalDeltaSemanticV1(),
            testCase.invocation);
        if (!artifactValid) throw std::runtime_error(artifactValid.Error());
        const auto verifiedValid = fv::ValidateVerifiedDeltaFamilyCandidateV1(
            testCase.verified[index],
            semantic::BuildCanonicalSparseTemporalDeltaSemanticV1(),
            testCase.invocation, context);
        if (!verifiedValid) throw std::runtime_error(verifiedValid.Error());
        UploadBytes(resources.payloadUploads[index].Get(),
                    testCase.artifacts[index].PayloadBytes());
        const std::array<std::uint32_t, 3> arguments{
            testCase.artifacts[index].DispatchGroupsX(), 1u, 1u};
        UploadBytes(resources.argumentUploads[index].Get(),
                    std::as_bytes(std::span(arguments)));
    }
}

void ResetOutputs(
    GpuContext& gpu,
    ReusableResources& resources,
    bool resetAudits)
{
    for (std::size_t index = 0; index < CanonicalCandidateCountV1; ++index)
    {
        TransitionResource(gpu.commandList.Get(), resources.outputs[index].Get(),
                           resources.outputStates[index], OutputState::CopyDestination);
        gpu.commandList->CopyBufferRegion(
            resources.outputs[index].Get(), 0, resources.historyUpload.Get(), 0,
            resources.historySize);
        if (resetAudits)
        {
            TransitionResource(gpu.commandList.Get(), resources.audits[index].Get(),
                               resources.auditStates[index], OutputState::CopyDestination);
            gpu.commandList->CopyBufferRegion(
                resources.audits[index].Get(), 0, resources.zeroAuditUpload.Get(), 0,
                resources.auditSize);
        }
    }
    for (std::size_t index = 0; index < CanonicalCandidateCountV1; ++index)
    {
        TransitionResource(gpu.commandList.Get(), resources.outputs[index].Get(),
                           resources.outputStates[index], OutputState::UnorderedAccess);
        if (resetAudits)
            TransitionResource(gpu.commandList.Get(), resources.audits[index].Get(),
                               resources.auditStates[index], OutputState::UnorderedAccess);
    }
}

std::array<CandidateCaseAuthorityV1, CanonicalCandidateCountV1>
ValidateCaseOnGpu(
    GpuContext& gpu,
    ReusableResources& resources,
    const semantic::SparseTemporalDeltaSemanticV1& semanticValue,
    const BuiltMeasurementCase& testCase)
{
    UploadCase(resources, testCase);
    gpu.Begin();
    ResetOutputs(gpu, resources, true);
    for (std::size_t index = 0; index < CanonicalCandidateCountV1; ++index)
    {
        BindAndDispatch(gpu, resources, testCase, index, true);
        const std::array<D3D12_RESOURCE_BARRIER, 2> barriers{
            UavBarrier(resources.outputs[index].Get()),
            UavBarrier(resources.audits[index].Get())};
        gpu.commandList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
    }
    for (std::size_t index = 0; index < CanonicalCandidateCountV1; ++index)
    {
        TransitionResource(gpu.commandList.Get(), resources.outputs[index].Get(),
                           resources.outputStates[index], OutputState::CopySource);
        TransitionResource(gpu.commandList.Get(), resources.audits[index].Get(),
                           resources.auditStates[index], OutputState::CopySource);
        gpu.commandList->CopyBufferRegion(
            resources.outputReadbacks[index].Get(), 0, resources.outputs[index].Get(), 0,
            resources.historySize);
        gpu.commandList->CopyBufferRegion(
            resources.auditReadbacks[index].Get(), 0, resources.audits[index].Get(), 0,
            resources.auditSize);
    }
    gpu.SubmitAndWait();

    const auto expected = semantic::BuildCpuReferenceOutputV1(testCase.invocation);
    const auto zeroHistory = semantic::BuildCanonicalInactiveHistoryBytesV1();
    std::array<std::vector<std::byte>, CanonicalCandidateCountV1> outputs;
    std::array<CandidateCaseAuthorityV1, CanonicalCandidateCountV1> authorities{};
    const auto kinds = CanonicalCandidateKindsV1();
    for (std::size_t candidateIndex = 0;
         candidateIndex < CanonicalCandidateCountV1;
         ++candidateIndex)
    {
        outputs[candidateIndex] = ReadbackBytes(
            resources.outputReadbacks[candidateIndex].Get(), resources.historySize);
        const auto auditBytes = ReadbackBytes(
            resources.auditReadbacks[candidateIndex].Get(), resources.auditSize);
        std::vector<spiral5::semantic::PointRecordV1> observed(semantic::WorkUniverseCountV1);
        std::vector<std::uint32_t> audit(semantic::WorkUniverseCountV1);
        std::memcpy(observed.data(), outputs[candidateIndex].data(), resources.historySize);
        std::memcpy(audit.data(), auditBytes.data(), resources.auditSize);
        const auto kind = kinds[candidateIndex];
        for (std::uint32_t index = 0; index < semantic::WorkUniverseCountV1; ++index)
        {
            const bool active = testCase.invocation.ActiveSet().Contains(index);
            const bool retained = testCase.invocation.RetainSet().Contains(index);
            const bool updated = testCase.invocation.UpdateSet().Contains(index);
            const bool cleared = testCase.invocation.DeactivationSet().Contains(index);
            const std::uint32_t expectedAudit = kind == CandidateKindV1::FullActiveDenseRecompute
                ? 3u : (updated ? 1u : (cleared ? 2u : 0u));
            Require(audit[index] == expectedAudit,
                    "Real-GPU measurement validation write audit mismatch.");
            const auto offset = static_cast<std::size_t>(index) *
                sizeof(spiral5::semantic::PointRecordV1);
            if (!active)
            {
                Require(std::memcmp(outputs[candidateIndex].data() + offset,
                                    zeroHistory.data() + offset,
                                    sizeof(spiral5::semantic::PointRecordV1)) == 0,
                        "Real-GPU measurement inactive sentinel mismatch.");
                continue;
            }
            if (retained && kind != CandidateKindV1::FullActiveDenseRecompute)
            {
                Require(std::memcmp(outputs[candidateIndex].data() + offset,
                                    testCase.previousHistoryBytes.data() + offset,
                                    sizeof(spiral5::semantic::PointRecordV1)) == 0,
                        "Real-GPU measurement retained History bytes changed.");
            }
            const auto& actual = observed[index];
            const auto& reference = expected[index];
            const std::array<float, 4> actualValues{actual.x, actual.y, actual.z, actual.w};
            const std::array<float, 4> expectedValues{reference.x, reference.y, reference.z, reference.w};
            for (std::size_t component = 0; component < actualValues.size(); ++component)
            {
                Require(std::isfinite(actualValues[component]),
                        "Real-GPU measurement output contains a non-finite component.");
                Require(std::abs(static_cast<double>(actualValues[component]) -
                                 static_cast<double>(expectedValues[component])) <= 1.0e-6,
                        "Real-GPU measurement output differs from CPU reference.");
            }
            Require(std::bit_cast<std::uint32_t>(actual.w) ==
                        std::bit_cast<std::uint32_t>(1.0f),
                    "Real-GPU measurement homogeneous coordinate differs.");
        }

        auto& authority = authorities[candidateIndex];
        authority.kind = kind;
        authority.rawCandidateIdentity = testCase.proposals[candidateIndex].rawCandidateIdentity;
        authority.verifiedPlanIdentity = testCase.verified[candidateIndex].Plan().planIdentity;
        authority.verifierCertificateIdentity =
            testCase.verified[candidateIndex].CertificateIdentity();
        authority.artifactIdentity = testCase.artifacts[candidateIndex].ArtifactIdentity();
        authority.representationResourceIdentity =
            fc::DeltaFamilyRepresentationResourceIdentityV1(testCase.artifacts[candidateIndex]);
        authority.outputDigest = base::Sha256(outputs[candidateIndex]);
    }
    Require(outputs[0] == outputs[1] && outputs[0] == outputs[2],
            "A/B/C Real-GPU validation outputs are not byte-identical.");
    (void)semanticValue;
    return authorities;
}

void ResetAndWarmup(
    GpuContext& gpu,
    ReusableResources& resources,
    const BuiltMeasurementCase& testCase,
    std::uint32_t iterations)
{
    gpu.Begin();
    ResetOutputs(gpu, resources, false);
    for (std::size_t candidateIndex = 0;
         candidateIndex < CanonicalCandidateCountV1;
         ++candidateIndex)
    {
        for (std::uint32_t iteration = 0; iteration < iterations; ++iteration)
            BindAndDispatch(gpu, resources, testCase, candidateIndex, false);
    }
    gpu.SubmitAndWait();
}

std::array<std::uint64_t, 6> ReadTimestamps(GpuContext& gpu)
{
    std::array<std::uint64_t, 6> values{};
    void* mapped = nullptr;
    D3D12_RANGE range{0, sizeof(values)};
    Check(gpu.timestampReadback->Map(0, &range, &mapped), "Map(timestamp readback)");
    std::memcpy(values.data(), mapped, sizeof(values));
    D3D12_RANGE noWrite{0, 0};
    gpu.timestampReadback->Unmap(0, &noWrite);
    return values;
}

double TicksToNanoseconds(std::uint64_t ticks, std::uint64_t frequency)
{
    return static_cast<double>(ticks) * 1.0e9 / static_cast<double>(frequency);
}

struct TimestampIntervalMeasurement final
{
    std::uint64_t ticks = 0;
    std::uint32_t effectiveIterations = 0;
    bool resolutionCensored = false;
    double batchNanoseconds = 0.0;
};

struct OrderedTimestampMeasurement final
{
    std::array<TimestampIntervalMeasurement, CanonicalCandidateCountV1> candidates{};
};

std::uint32_t NextAdaptiveIterationCount(std::uint32_t current)
{
    Require(current > 0 && current <= MaximumAdaptiveIterationsV2,
            "Adaptive timestamp iteration count is outside the qualified range.");
    Require(current <= MaximumAdaptiveIterationsV2 / 2u,
            "Nonzero-dispatch timestamp remained unresolved at the adaptive iteration ceiling.");
    return current * 2u;
}

TimestampIntervalMeasurement MeasureSingleCandidate(
    GpuContext& gpu,
    ReusableResources& resources,
    const BuiltMeasurementCase& testCase,
    std::size_t candidateIndex,
    std::uint32_t requestedIterations)
{
    auto effectiveIterations = requestedIterations;
    for (;;)
    {
        gpu.Begin();
        gpu.commandList->EndQuery(gpu.timestampHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0);
        for (std::uint32_t iteration = 0; iteration < effectiveIterations; ++iteration)
            BindAndDispatch(gpu, resources, testCase, candidateIndex, false);
        gpu.commandList->EndQuery(gpu.timestampHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 1);
        gpu.commandList->ResolveQueryData(
            gpu.timestampHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0, 2,
            gpu.timestampReadback.Get(), 0);
        gpu.SubmitAndWait();
        const auto timestamps = ReadTimestamps(gpu);
        Require(timestamps[1] >= timestamps[0],
                "Single-Candidate timestamp order is non-monotonic.");
        const auto ticks = timestamps[1] - timestamps[0];
        if (ticks != 0)
        {
            return {ticks, effectiveIterations, false,
                    TicksToNanoseconds(ticks, gpu.profile.timestampFrequency)};
        }

        const auto dispatchGroups = testCase.artifacts[candidateIndex].DispatchGroupsX();
        if (dispatchGroups == 0)
        {
            return {0, effectiveIterations, true,
                    TicksToNanoseconds(1, gpu.profile.timestampFrequency)};
        }
        effectiveIterations = NextAdaptiveIterationCount(effectiveIterations);
    }
}

OrderedTimestampMeasurement MeasureOrder(
    GpuContext& gpu,
    ReusableResources& resources,
    const BuiltMeasurementCase& testCase,
    const std::array<std::uint32_t, CanonicalCandidateCountV1>& order,
    std::uint32_t requestedIterations)
{
    auto effectiveIterations = requestedIterations;
    for (;;)
    {
        gpu.Begin();
        for (std::size_t position = 0; position < order.size(); ++position)
        {
            const auto candidateIndex = order[position];
            const auto query = static_cast<UINT>(position * 2);
            gpu.commandList->EndQuery(
                gpu.timestampHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, query);
            for (std::uint32_t iteration = 0; iteration < effectiveIterations; ++iteration)
                BindAndDispatch(gpu, resources, testCase, candidateIndex, false);
            gpu.commandList->EndQuery(
                gpu.timestampHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, query + 1);
        }
        gpu.commandList->ResolveQueryData(
            gpu.timestampHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0, 6,
            gpu.timestampReadback.Get(), 0);
        gpu.SubmitAndWait();
        const auto timestamps = ReadTimestamps(gpu);

        bool retryForNonzeroDispatch = false;
        OrderedTimestampMeasurement result;
        for (std::size_t position = 0; position < order.size(); ++position)
        {
            const auto candidateIndex = order[position];
            const auto begin = timestamps[position * 2];
            const auto end = timestamps[position * 2 + 1];
            Require(end >= begin, "Balanced-order timestamp order is non-monotonic.");
            const auto ticks = end - begin;
            const auto dispatchGroups = testCase.artifacts[candidateIndex].DispatchGroupsX();
            if (ticks == 0 && dispatchGroups != 0)
            {
                retryForNonzeroDispatch = true;
                continue;
            }
            auto& measured = result.candidates[position];
            measured.ticks = ticks;
            measured.effectiveIterations = effectiveIterations;
            measured.resolutionCensored = ticks == 0;
            measured.batchNanoseconds = TicksToNanoseconds(
                std::max<std::uint64_t>(1u, ticks), gpu.profile.timestampFrequency);
            if (measured.resolutionCensored)
                Require(dispatchGroups == 0,
                        "Only a zero-dispatch Candidate may be timestamp-resolution censored.");
        }
        if (!retryForNonzeroDispatch) return result;
        effectiveIterations = NextAdaptiveIterationCount(effectiveIterations);
    }
}

CandidateProfileV1 BuildCandidateProfile(
    const GpuContext& gpu,
    const BuiltMeasurementCase& testCase,
    std::size_t index,
    const fv::DeltaFamilyVerificationContextV1& context)
{
    const auto& artifact = testCase.artifacts[index];
    const auto& operation = testCase.verified[index].Plan().operation;
    CandidateProfileV1 profile;
    profile.kind = artifact.Kind();
    profile.representationRole = artifact.Role();
    profile.dispatchPolicy = artifact.DispatchPolicy();
    profile.payloadStrideBytes = artifact.PayloadStrideBytes();
    profile.fixedCapacityBytes = artifact.FixedCapacityBytes();
    profile.targetProfileIdentity = context.targetProfileIdentity;
    profile.observationContractIdentity = context.observationContractIdentity;
    profile.completionContractIdentity = context.completionContractIdentity;
    profile.consumerProgramIdentity = operation.consumerProgramIdentity;
    profile.representationResourceContractIdentity =
        operation.representationResourceContractIdentity;
    profile.validationShaderBytecodeDigest = gpu.validationShaderDigests[index];
    profile.timingShaderBytecodeDigest = gpu.timingShaderDigests[index];
    return profile;
}
#endif // _WIN32
} // namespace

base::Result<MeasurementEvidenceV1, std::string>
RunRealGpuMeasurementV1(const MeasurementConfigV1& config)
{
#if !defined(_WIN32)
    (void)config;
    return base::Result<MeasurementEvidenceV1, std::string>::Failure(
        "Spiral 7 CU6 real-GPU measurement requires Windows D3D12.");
#else
    try
    {
        // Validation is performed by the profile identity builder and by the evidence validator.
        (void)BuildMeasurementProfileIdentityV1(config);
        const auto semanticValue = semantic::BuildCanonicalSparseTemporalDeltaSemanticV1();
        const auto context = fv::BuildCanonicalDeltaFamilyVerificationContextV1();
        GpuContext gpu(config);
        ReusableResources resources(gpu);

        MeasurementEvidenceV1 evidence;
        evidence.captureUnixNanoseconds = UnixNanosecondsNow();
        evidence.config = config;
        evidence.adapter = gpu.profile;
        evidence.cu5ArchitectureEvidenceIdentity = DigestFromHex(AcceptedArchitectureDigest);
        evidence.cu5ControlledRecoveryEvidenceIdentity = DigestFromHex(AcceptedControlledDigest);
        evidence.cu5FreshRematerializationEvidenceIdentity = DigestFromHex(AcceptedFreshDigest);
        evidence.semanticIdentity = semantic::SparseTemporalDeltaSemanticIdentityV1(semanticValue);
        evidence.verificationContextIdentity = VerificationContextIdentityV1();
        evidence.measurementProfileIdentity = BuildMeasurementProfileIdentityV1(config);
        evidence.caseScheduleIdentity = BuildCaseScheduleIdentityV1(config.runCount);
        evidence.candidateOrderIdentity = BuildCandidateOrderIdentityV1();

        bool profilesInitialized = false;
        const auto balancedOrders = CanonicalBalancedOrdersV1();
        std::uint32_t completedAcceptedBlocks = 0;
        const std::uint32_t totalAcceptedBlocks = config.runCount * CanonicalCaseCountV1;
        for (std::uint32_t run = 0; run < config.runCount; ++run)
        {
            const auto schedule = CanonicalCaseScheduleV1(run);
            for (std::size_t caseOrder = 0; caseOrder < schedule.size(); ++caseOrder)
            {
                const auto pattern = static_cast<PatternKindV1>(schedule[caseOrder][0]);
                const auto activeCount = schedule[caseOrder][1];
                const auto transitionCount = schedule[caseOrder][2];
                auto testCase = BuildMeasurementCase(
                    semanticValue, context, pattern, activeCount, transitionCount);
                const auto authorities = ValidateCaseOnGpu(
                    gpu, resources, semanticValue, testCase);
                if (!profilesInitialized)
                {
                    for (std::size_t index = 0; index < CanonicalCandidateCountV1; ++index)
                        evidence.candidates[index] = BuildCandidateProfile(
                            gpu, testCase, index, context);
                    profilesInitialized = true;
                }

                bool accepted = false;
                for (std::uint32_t attemptIndex = 0;
                     attemptIndex < config.maximumBlockAttempts;
                     ++attemptIndex)
                {
                    BlockAttemptV1 attempt;
                    attempt.run = run;
                    attempt.caseOrder = static_cast<std::uint32_t>(caseOrder);
                    attempt.attempt = attemptIndex;
                    attempt.key = testCase.key;
                    attempt.authorities = authorities;

                    ResetAndWarmup(gpu, resources, testCase, config.warmupIterations);
                    const auto controlBefore = MeasureSingleCandidate(
                        gpu, resources, testCase, 0, config.iterationsPerBatch);
                    attempt.controlBeforeNanosecondsPerIteration =
                        controlBefore.batchNanoseconds /
                        static_cast<double>(controlBefore.effectiveIterations);
                    for (std::uint32_t cycle = 0;
                         cycle < config.measurementCyclesPerOrder;
                         ++cycle)
                    {
                        for (std::size_t orderIndex = 0;
                             orderIndex < balancedOrders.size();
                             ++orderIndex)
                        {
                            const auto batchMeasurement = MeasureOrder(
                                gpu, resources, testCase, balancedOrders[orderIndex],
                                config.iterationsPerBatch);
                            for (std::size_t position = 0;
                                 position < CanonicalCandidateCountV1;
                                 ++position)
                            {
                                const auto candidateIndex = balancedOrders[orderIndex][position];
                                TimingSampleV1 sample;
                                sample.run = run;
                                sample.caseOrder = static_cast<std::uint32_t>(caseOrder);
                                sample.acceptedAttempt = attemptIndex;
                                sample.orderIndex = static_cast<std::uint32_t>(orderIndex);
                                sample.cycle = cycle;
                                sample.orderPosition = static_cast<std::uint32_t>(position);
                                sample.candidate = CanonicalCandidateKindsV1()[candidateIndex];
                                const auto& interval =
                                    batchMeasurement.candidates[position];
                                sample.timestampTickCount = interval.ticks;
                                sample.effectiveIterations = interval.effectiveIterations;
                                sample.timestampResolutionCensored =
                                    interval.resolutionCensored;
                                sample.gpuBatchNanoseconds = interval.batchNanoseconds;
                                sample.gpuNanosecondsPerIteration =
                                    interval.batchNanoseconds /
                                    static_cast<double>(interval.effectiveIterations);
                                sample.expectedDispatchGroupsX =
                                    testCase.artifacts[candidateIndex].DispatchGroupsX();
                                attempt.samples.push_back(sample);
                            }
                        }
                    }
                    const auto controlAfter = MeasureSingleCandidate(
                        gpu, resources, testCase, 0, config.iterationsPerBatch);
                    attempt.controlAfterNanosecondsPerIteration =
                        controlAfter.batchNanoseconds /
                        static_cast<double>(controlAfter.effectiveIterations);
                    const double minimumControl = std::min(
                        attempt.controlBeforeNanosecondsPerIteration,
                        attempt.controlAfterNanosecondsPerIteration);
                    const double maximumControl = std::max(
                        attempt.controlBeforeNanosecondsPerIteration,
                        attempt.controlAfterNanosecondsPerIteration);
                    Require(minimumControl > 0.0, "Control measurement is non-positive.");
                    attempt.controlReferenceNanosecondsPerIteration =
                        (attempt.controlBeforeNanosecondsPerIteration +
                         attempt.controlAfterNanosecondsPerIteration) * 0.5;
                    attempt.controlRatio = maximumControl / minimumControl;
                    if (attempt.controlRatio <= config.maximumControlRatio)
                    {
                        attempt.accepted = true;
                        for (auto& sample : attempt.samples)
                        {
                            sample.blockControlNanosecondsPerIteration =
                                attempt.controlReferenceNanosecondsPerIteration;
                            sample.controlNormalizedTime =
                                sample.gpuNanosecondsPerIteration /
                                attempt.controlReferenceNanosecondsPerIteration;
                        }
                        evidence.attempts.push_back(std::move(attempt));
                        accepted = true;
                        break;
                    }
                    attempt.accepted = false;
                    attempt.rejectionReason = "Within-block fixed control drift exceeded maximumControlRatio.";
                    attempt.samples.clear();
                    evidence.attempts.push_back(std::move(attempt));
                }
                Require(accepted, "No accepted measurement block remained after maximum attempts.");
                ++completedAcceptedBlocks;
                std::cout << "[MEASURED] " << completedAcceptedBlocks << '/'
                          << totalAcceptedBlocks
                          << " run=" << run
                          << " pattern=" << PatternNameV1(pattern)
                          << " A=" << activeCount
                          << " T=" << transitionCount
                          << " U=" << testCase.key.updateCount
                          << " C=" << testCase.key.clearCount
                          << " blocks=" << testCase.key.affectedBlockCount
                          << '\n';
            }
        }
        const auto valid = ValidateMeasurementEvidenceV1(evidence);
        if (!valid)
            return base::Result<MeasurementEvidenceV1, std::string>::Failure(valid.Error());
        return base::Result<MeasurementEvidenceV1, std::string>::Success(std::move(evidence));
    }
    catch (const std::exception& error)
    {
        return base::Result<MeasurementEvidenceV1, std::string>::Failure(error.what());
    }
#endif
}
} // namespace sge4_5::spiral7::performance
