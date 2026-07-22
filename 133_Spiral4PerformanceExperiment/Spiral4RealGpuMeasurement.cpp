#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "Spiral4PerformanceExperiment.h"

#include "../00_Foundation/BinaryIO.h"
#include "../00_Foundation/Sha256.h"
#include "../128_ActiveWorkCandidateFamily/ActiveWorkCandidateFamily.h"
#include "../129_ActiveWorkCandidateFamilyPlanner/ActiveWorkCandidateFamilyPlanner.h"

#if defined(_WIN32)
#include <Windows.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")
#endif

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>
#include <iterator>
#include <memory>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace sge4_5::spiral4::performance
{
#if defined(_WIN32)
namespace
{
using Microsoft::WRL::ComPtr;
using Clock = std::chrono::steady_clock;

constexpr std::string_view ArchitectureEvidenceHex =
    "60cf81fb8bac565cd35d696612b7050f464576dede5cef7e2fe61b0cdfa7c32e";
constexpr std::string_view ControlledRecoveryEvidenceHex =
    "4ccaa3f2bb250a0af16037c74da8cff8f65a0834f912803adc3c8c7cf607b4d9";
constexpr std::string_view FreshRematerializationEvidenceHex =
    "e1794f15d6808b4c220ee0d2527564551ee7fa1d2daa70b4f360531819f13dc3";

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
    std::uint32_t x;
    std::uint32_t y;
    std::uint32_t z;
};

struct BatchCommand final
{
    std::uint32_t firstWork;
    std::uint32_t x;
    std::uint32_t y;
    std::uint32_t z;
};

static_assert(sizeof(Float4) == 16);
static_assert(sizeof(Motor) == 32);
static_assert(sizeof(DispatchArguments) == 12);
static_assert(sizeof(BatchCommand) == 16);

constexpr Float4 InactiveSentinel{
    123456.25f,
    -654321.5f,
    777.0f,
    -999.0f
};

base::Digest256 LiteralIdentity(std::string_view value)
{
    return base::Sha256(
        std::as_bytes(std::span(value.data(), value.size())));
}

base::Digest256 DigestFromHex(std::string_view value)
{
    if (value.size() != 64)
    {
        throw std::runtime_error("Digest hex length must be 64.");
    }
    auto nibble = [](char c) -> std::uint8_t
    {
        if (c >= '0' && c <= '9') return static_cast<std::uint8_t>(c - '0');
        if (c >= 'a' && c <= 'f') return static_cast<std::uint8_t>(10 + c - 'a');
        if (c >= 'A' && c <= 'F') return static_cast<std::uint8_t>(10 + c - 'A');
        throw std::runtime_error("Digest contains a non-hex character.");
    };
    base::Digest256 result{};
    for (std::size_t index = 0; index < result.size(); ++index)
    {
        result[index] = static_cast<std::byte>(
            (nibble(value[index * 2]) << 4u) |
            nibble(value[index * 2 + 1]));
    }
    return result;
}

void WriteString(base::BinaryWriter& writer, std::string_view value)
{
    writer.WriteU32(static_cast<std::uint32_t>(value.size()));
    writer.WriteBytes(
        std::as_bytes(std::span(value.data(), value.size())));
}

void WriteDouble(base::BinaryWriter& writer, double value)
{
    writer.WriteU64(std::bit_cast<std::uint64_t>(value));
}

base::Digest256 BuildMeasurementProfileIdentity(
    const MeasurementConfigV1& config)
{
    base::BinaryWriter writer;
    WriteString(writer, "SGE4-5.Spiral4.MeasurementProfile.V1");
    writer.WriteU32(config.warmupCyclesPerOrder);
    writer.WriteU32(config.measurementCyclesPerOrder);
    writer.WriteU32(config.runCount);
    writer.WriteU32(config.adapterIndex);
    WriteString(writer, config.clockPolicy);
    WriteString(writer, config.primaryMetric);
    WriteDouble(writer, config.absoluteNoiseFloorNanoseconds);
    WriteDouble(writer, config.relativeNoiseFloor);
    return base::Sha256(std::move(writer).Take());
}

base::Digest256 BuildOrderBalanceIdentity()
{
    base::BinaryWriter writer;
    WriteString(writer, "SGE4-5.Spiral4.BalancedOrders.V1");
    const auto orders = CanonicalBalancedOrdersV1();
    writer.WriteU32(static_cast<std::uint32_t>(orders.size()));
    for (const auto& order : orders)
    {
        for (const auto candidate : order)
        {
            writer.WriteU32(candidate);
        }
    }
    return base::Sha256(std::move(writer).Take());
}

base::Digest256 BuildVerificationContextIdentity(
    const family_verification::CandidateFamilyVerificationContextV1& context)
{
    base::BinaryWriter writer;
    WriteString(
        writer,
        "SGE4-5.Spiral4.CandidateFamilyVerificationContext.V1");
    writer.WriteBytes(context.targetProfileIdentity);
    writer.WriteBytes(context.activeCountContractIdentity);
    writer.WriteBytes(context.observationContractIdentity);
    return base::Sha256(std::move(writer).Take());
}

void ValidateConfigLocal(const MeasurementConfigV1& config)
{
    if (config.measurementCyclesPerOrder == 0 ||
        config.measurementCyclesPerOrder > 1000 ||
        config.runCount == 0 || config.runCount > 100 ||
        config.warmupCyclesPerOrder > 1000)
    {
        throw std::runtime_error("CU6 measurement cycle configuration is invalid.");
    }
    if (config.primaryMetric != "GpuCandidatePathNanoseconds")
    {
        throw std::runtime_error("CU6 primary metric is not canonical.");
    }
    if (!std::isfinite(config.absoluteNoiseFloorNanoseconds) ||
        config.absoluteNoiseFloorNanoseconds < 0.0 ||
        !std::isfinite(config.relativeNoiseFloor) ||
        config.relativeNoiseFloor < 0.0 ||
        config.relativeNoiseFloor > 1.0)
    {
        throw std::runtime_error("CU6 noise policy is invalid.");
    }
}

std::uint64_t CaptureUnixNanoseconds()
{
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
}

double Nanoseconds(Clock::time_point begin, Clock::time_point end)
{
    return static_cast<double>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            end - begin).count());
}

std::string WideToUtf8(const wchar_t* value)
{
    if (!value || *value == L'\0')
    {
        return {};
    }
    const int required = WideCharToMultiByte(
        CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
    if (required <= 1)
    {
        return {};
    }
    std::string result(static_cast<std::size_t>(required), '\0');
    if (WideCharToMultiByte(
            CP_UTF8, 0, value, -1, result.data(), required,
            nullptr, nullptr) != required)
    {
        throw std::runtime_error("WideCharToMultiByte failed.");
    }
    result.resize(static_cast<std::size_t>(required - 1));
    return result;
}

void Check(HRESULT result, std::string_view stage)
{
    if (FAILED(result))
    {
        std::ostringstream stream;
        stream << stage << " HRESULT=0x"
               << std::hex << static_cast<unsigned long>(result);
        throw std::runtime_error(stream.str());
    }
}

D3D12_HEAP_PROPERTIES HeapProperties(D3D12_HEAP_TYPE type)
{
    D3D12_HEAP_PROPERTIES value{};
    value.Type = type;
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
    value.Width = size;
    value.Height = 1;
    value.DepthOrArraySize = 1;
    value.MipLevels = 1;
    value.Format = DXGI_FORMAT_UNKNOWN;
    value.SampleDesc.Count = 1;
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
    const auto heap = HeapProperties(heapType);
    const auto description = BufferDescription(size, flags);
    ComPtr<ID3D12Resource> resource;
    Check(
        device->CreateCommittedResource(
            &heap,
            D3D12_HEAP_FLAG_NONE,
            &description,
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
    D3D12_RESOURCE_BARRIER value{};
    value.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    value.Transition.pResource = resource;
    value.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    value.Transition.StateBefore = before;
    value.Transition.StateAfter = after;
    return value;
}

D3D12_RESOURCE_BARRIER UavBarrier(ID3D12Resource* resource)
{
    D3D12_RESOURCE_BARRIER value{};
    value.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    value.UAV.pResource = resource;
    return value;
}

ComPtr<ID3DBlob> CompileShader(
    const std::string& source,
    const char* stage)
{
    ComPtr<ID3DBlob> bytecode;
    ComPtr<ID3DBlob> errors;
    const UINT flags =
        D3DCOMPILE_ENABLE_STRICTNESS |
        D3DCOMPILE_WARNINGS_ARE_ERRORS |
        D3DCOMPILE_OPTIMIZATION_LEVEL3;
    const HRESULT result = D3DCompile(
        source.data(),
        source.size(),
        "Spiral4CU6Generated.hlsl",
        nullptr,
        nullptr,
        "CSMain",
        "cs_5_1",
        flags,
        0,
        &bytecode,
        &errors);
    if (FAILED(result))
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
    D3D12_ROOT_SIGNATURE_DESC description{};
    description.NumParameters = static_cast<UINT>(parameters.size());
    description.pParameters = parameters.data();
    description.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    ComPtr<ID3DBlob> bytes;
    ComPtr<ID3DBlob> errors;
    const HRESULT serialized = D3D12SerializeRootSignature(
        &description,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &bytes,
        &errors);
    if (FAILED(serialized))
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
    Check(
        device->CreateRootSignature(
            0,
            bytes->GetBufferPointer(),
            bytes->GetBufferSize(),
            IID_PPV_ARGS(&result)),
        "CreateRootSignature");
    return result;
}

ComPtr<ID3D12PipelineState> CreateComputePipeline(
    ID3D12Device* device,
    ID3D12RootSignature* root,
    ID3DBlob* shader)
{
    D3D12_COMPUTE_PIPELINE_STATE_DESC description{};
    description.pRootSignature = root;
    description.CS.pShaderBytecode = shader->GetBufferPointer();
    description.CS.BytecodeLength = shader->GetBufferSize();
    ComPtr<ID3D12PipelineState> result;
    Check(
        device->CreateComputePipelineState(
            &description,
            IID_PPV_ARGS(&result)),
        "CreateComputePipelineState");
    return result;
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

std::string SingleProducerSource()
{
    return R"hlsl(
static const uint MaxWorkCount = 4096u;
static const uint ThreadsPerGroup = 64u;
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
}

std::string BatchProducerSource()
{
    return R"hlsl(
static const uint MaxWorkCount = 4096u;
static const uint ThreadsPerGroup = 64u;
cbuffer Config : register(b0) { uint BatchSize; uint MaxSlots; };
struct BatchCommand { uint firstWork; uint3 dispatch; };
StructuredBuffer<uint> ActiveCount : register(t0);
RWStructuredBuffer<BatchCommand> Commands : register(u0);
[numthreads(64,1,1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    uint slot = id.x;
    if (slot >= MaxSlots) return;
    uint active = min(ActiveCount[0], MaxWorkCount);
    uint first = slot * BatchSize;
    uint remaining = first < active ? active - first : 0;
    uint workCount = min(BatchSize, remaining);
    uint groups = workCount == 0 ? 0 : 1 + ((workCount - 1) / ThreadsPerGroup);
    BatchCommand command;
    command.firstWork = first;
    command.dispatch = uint3(groups, 1, 1);
    Commands[slot] = command;
}
)hlsl";
}

std::string ConsumerSource()
{
    return R"hlsl(
static const uint MaxWorkCount = 4096u;
static const uint ReusePerBone = 512u;
cbuffer BatchBase : register(b0) { uint FirstWork; };
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
    uint n = FirstWork + id.x;
    uint active = min(ActiveCount[0], MaxWorkCount);
    if (n >= active) return;
    uint bone = n / ReusePerBone;
    Motor m = Transforms[bone];
    float3 p = Points[n].xyz;
    float3 q = QMul(QMul(m.qr, float4(0,p)), QConj(m.qr)).yzw;
    float3 t = 2.0f * QMul(m.qd, QConj(m.qr)).yzw;
    Output[n] = float4(q+t, 1.0f);
}
)hlsl";
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
        values[bone].qd = Float4{
            0.0f,
            tx * 0.5f,
            ty * 0.5f,
            tz * 0.5f};
    }
    return values;
}

std::vector<Float4> BuildPoints(std::uint32_t count)
{
    std::vector<Float4> values(count);
    for (std::uint32_t index = 0; index < count; ++index)
    {
        values[index] = Float4{
            static_cast<float>(index % 17u) * 0.25f,
            -static_cast<float>((index / 17u) % 13u) * 0.125f,
            static_cast<float>(index % 7u) * 0.5f,
            1.0f};
    }
    return values;
}

Float4 ExpectedPoint(const Float4& point, std::uint32_t index)
{
    const std::uint32_t bone = index / 512u;
    return Float4{
        point.x + static_cast<float>(bone) * 0.5f,
        point.y - static_cast<float>(bone) * 0.25f,
        point.z + static_cast<float>(bone) * 0.125f,
        1.0f};
}

bool IsSentinel(const Float4& value)
{
    return std::memcmp(&value, &InactiveSentinel, sizeof(Float4)) == 0;
}

std::uint32_t OrderedFloatBits(float value) noexcept
{
    const std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
    if ((bits & 0x8000'0000u) != 0)
    {
        return 0x8000'0000u - (bits & 0x7FFF'FFFFu);
    }
    return 0x8000'0000u + bits;
}

std::uint32_t UlpDistance(float a, float b) noexcept
{
    const std::uint32_t x = OrderedFloatBits(a);
    const std::uint32_t y = OrderedFloatBits(b);
    return x > y ? x - y : y - x;
}

float RelativeError(float actual, float expected) noexcept
{
    const float denominator = std::max(std::abs(expected), 1.0e-20f);
    return std::abs(actual - expected) / denominator;
}

struct HardwareContext final
{
    ComPtr<IDXGIAdapter1> adapter;
    ComPtr<ID3D12Device> device;
    AdapterProfileV1 profile;
};

HardwareContext CreateHardwareContext(std::uint32_t requestedIndex)
{
    ComPtr<IDXGIFactory6> factory;
    Check(
        CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)),
        "CreateDXGIFactory2");

    HardwareContext result;
    std::uint32_t hardwareIndex = 0;
    for (UINT index = 0;; ++index)
    {
        ComPtr<IDXGIAdapter1> adapter;
        const HRESULT found = factory->EnumAdapterByGpuPreference(
            index,
            DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
            IID_PPV_ARGS(&adapter));
        if (found == DXGI_ERROR_NOT_FOUND)
        {
            break;
        }
        if (FAILED(found))
        {
            continue;
        }

        DXGI_ADAPTER_DESC1 description{};
        Check(adapter->GetDesc1(&description), "GetDesc1");
        if ((description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0)
        {
            continue;
        }

        ComPtr<ID3D12Device> device;
        if (FAILED(D3D12CreateDevice(
                adapter.Get(),
                D3D_FEATURE_LEVEL_11_0,
                IID_PPV_ARGS(&device))))
        {
            continue;
        }
        if (hardwareIndex++ != requestedIndex)
        {
            continue;
        }
        result.adapter = adapter;
        result.device = device;
        result.profile.description = WideToUtf8(description.Description);
        result.profile.vendorId = description.VendorId;
        result.profile.deviceId = description.DeviceId;
        result.profile.subsystemId = description.SubSysId;
        result.profile.revision = description.Revision;
        result.profile.luidLow = description.AdapterLuid.LowPart;
        result.profile.luidHigh = description.AdapterLuid.HighPart;
        LARGE_INTEGER driverVersion{};
        if (SUCCEEDED(adapter->CheckInterfaceSupport(
                __uuidof(IDXGIDevice),
                &driverVersion)))
        {
            result.profile.driverVersion =
                static_cast<std::uint64_t>(driverVersion.QuadPart);
        }
        D3D12_FEATURE_DATA_ARCHITECTURE1 architecture{};
        if (SUCCEEDED(device->CheckFeatureSupport(
                D3D12_FEATURE_ARCHITECTURE1,
                &architecture,
                sizeof(architecture))))
        {
            result.profile.uma = architecture.UMA != FALSE;
            result.profile.cacheCoherentUma =
                architecture.CacheCoherentUMA != FALSE;
        }
        break;
    }
    if (!result.device)
    {
        throw std::runtime_error(
            "No compatible non-software D3D12 hardware adapter was found.");
    }
    return result;
}

class EventHandle final
{
public:
    EventHandle()
    {
        value_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!value_)
        {
            throw std::runtime_error("CreateEventW failed.");
        }
    }
    ~EventHandle()
    {
        if (value_)
        {
            CloseHandle(value_);
        }
    }
    [[nodiscard]] HANDLE Get() const noexcept { return value_; }
private:
    HANDLE value_ = nullptr;
};

class MeasurementSession final
{
public:
    MeasurementSession(
        ID3D12Device* device,
        std::string adapterDescription,
        const semantic::ActiveWorkSemanticV1& semantic,
        std::span<const family_execution::FrozenVerifiedCandidateFamilyV1> candidates)
        : device_(device),
          adapterDescription_(std::move(adapterDescription)),
          semantic_(semantic),
          candidates_(candidates.begin(), candidates.end()),
          transformsCpu_(BuildTransforms()),
          pointsCpu_(BuildPoints(semantic.maxWorkCount.Value()))
    {
        if (!device_)
        {
            throw std::runtime_error("MeasurementSession requires a D3D12 device.");
        }
        if (candidates_.size() != CanonicalCandidateCountV1)
        {
            throw std::runtime_error("MeasurementSession requires seven Candidates.");
        }

        CreateQueueObjects();
        CreateRootAndPipelineObjects();
        CreateCommandSignatures();
        CreateResources();
        CreateDescriptors();
        CreateTimestampObjects();
        CreateFenceObjects();
    }

    ~MeasurementSession()
    {
        if (activeCount_ && mappedActiveCount_)
        {
            activeCount_->Unmap(0, nullptr);
        }
        if (outputReadback_ && mappedOutput_)
        {
            outputReadback_->Unmap(0, nullptr);
        }
        if (singleArgumentReadback_ && mappedSingleArguments_)
        {
            singleArgumentReadback_->Unmap(0, nullptr);
        }
        if (batchArgumentReadback_ && mappedBatchArguments_)
        {
            batchArgumentReadback_->Unmap(0, nullptr);
        }
        if (timestampReadback_ && mappedTimestamps_)
        {
            timestampReadback_->Unmap(0, nullptr);
        }
    }

    MeasurementSession(const MeasurementSession&) = delete;
    MeasurementSession& operator=(const MeasurementSession&) = delete;

    [[nodiscard]] std::uint64_t TimestampFrequency() const noexcept
    {
        return timestampFrequency_;
    }

    [[nodiscard]] const base::Digest256& SingleProducerDigest() const noexcept
    {
        return singleProducerDigest_;
    }

    [[nodiscard]] const base::Digest256& BatchProducerDigest() const noexcept
    {
        return batchProducerDigest_;
    }

    [[nodiscard]] const base::Digest256& ConsumerDigest() const noexcept
    {
        return consumerDigest_;
    }

    [[nodiscard]] TimingSampleV1 Measure(
        std::size_t candidateIndex,
        std::uint32_t activeWorkCount,
        std::uint32_t run,
        std::uint32_t orderIndex,
        std::uint32_t cycle,
        std::uint32_t orderPosition,
        bool recordEvidence)
    {
        if (candidateIndex >= candidates_.size())
        {
            throw std::runtime_error("Measurement Candidate index is out of range.");
        }
        if (activeWorkCount > semantic_.maxWorkCount.Value())
        {
            throw std::runtime_error("Measurement Active Count exceeds Nmax.");
        }

        const auto totalBegin = Clock::now();
        const auto inputBegin = Clock::now();
        *mappedActiveCount_ = activeWorkCount;
        const auto inputEnd = Clock::now();

        const auto recordBegin = Clock::now();
        Check(allocator_->Reset(), "CommandAllocator Reset");
        Check(
            commandList_->Reset(allocator_.Get(), nullptr),
            "CommandList Reset");

        commandList_->CopyBufferRegion(
            output_.Get(),
            0,
            sentinelUpload_.Get(),
            0,
            outputBytes_);
        auto outputToUav = TransitionBarrier(
            output_.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        commandList_->ResourceBarrier(1, &outputToUav);

        ID3D12DescriptorHeap* heaps[] = {descriptorHeap_.Get()};
        commandList_->SetDescriptorHeaps(1, heaps);
        commandList_->EndQuery(
            timestampHeap_.Get(),
            D3D12_QUERY_TYPE_TIMESTAMP,
            0);

        const auto& candidate = candidates_[candidateIndex];
        const auto kind = candidate.Verified().Plan().kind;

        if (kind ==
            family_contract::CandidateFamilyKindV1::
                SingleExecuteIndirectDispatch)
        {
            commandList_->SetPipelineState(singleProducerPipeline_.Get());
            commandList_->SetComputeRootSignature(singleProducerRoot_.Get());
            commandList_->SetComputeRootDescriptorTable(
                0,
                GpuHandle(gpuDescriptorStart_, descriptorIncrement_, 0));
            commandList_->SetComputeRootDescriptorTable(
                1,
                GpuHandle(gpuDescriptorStart_, descriptorIncrement_, 1));
            commandList_->Dispatch(1, 1, 1);
        }
        else if (kind ==
            family_contract::CandidateFamilyKindV1::
                BatchedExecuteIndirectDispatch)
        {
            commandList_->SetPipelineState(batchProducerPipeline_.Get());
            commandList_->SetComputeRootSignature(batchProducerRoot_.Get());
            commandList_->SetComputeRoot32BitConstant(
                0,
                candidate.Verified().Plan().batchSize,
                0);
            commandList_->SetComputeRoot32BitConstant(
                0,
                candidate.Verified().Plan().maxBatchSlots,
                1);
            commandList_->SetComputeRootDescriptorTable(
                1,
                GpuHandle(gpuDescriptorStart_, descriptorIncrement_, 2));
            commandList_->SetComputeRootDescriptorTable(
                2,
                GpuHandle(gpuDescriptorStart_, descriptorIncrement_, 3));
            commandList_->Dispatch(
                candidate.Verified().Plan().producerDispatchX,
                1,
                1);
        }

        commandList_->EndQuery(
            timestampHeap_.Get(),
            D3D12_QUERY_TYPE_TIMESTAMP,
            1);

        if (kind ==
            family_contract::CandidateFamilyKindV1::
                SingleExecuteIndirectDispatch)
        {
            auto uav = UavBarrier(singleArguments_.Get());
            commandList_->ResourceBarrier(1, &uav);
            auto transition = TransitionBarrier(
                singleArguments_.Get(),
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
            commandList_->ResourceBarrier(1, &transition);
        }
        else if (kind ==
            family_contract::CandidateFamilyKindV1::
                BatchedExecuteIndirectDispatch)
        {
            auto uav = UavBarrier(batchArguments_.Get());
            commandList_->ResourceBarrier(1, &uav);
            auto transition = TransitionBarrier(
                batchArguments_.Get(),
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
            commandList_->ResourceBarrier(1, &transition);
        }

        commandList_->EndQuery(
            timestampHeap_.Get(),
            D3D12_QUERY_TYPE_TIMESTAMP,
            2);

        commandList_->SetPipelineState(consumerPipeline_.Get());
        commandList_->SetComputeRootSignature(consumerRoot_.Get());
        commandList_->SetComputeRoot32BitConstant(0, 0, 0);
        commandList_->SetComputeRootDescriptorTable(
            1,
            GpuHandle(gpuDescriptorStart_, descriptorIncrement_, 4));
        commandList_->SetComputeRootDescriptorTable(
            2,
            GpuHandle(gpuDescriptorStart_, descriptorIncrement_, 7));

        if (kind ==
            family_contract::CandidateFamilyKindV1::FixedMaximumGuarded)
        {
            commandList_->Dispatch(
                candidate.Verified().Plan().directDispatchX,
                1,
                1);
        }
        else if (kind ==
            family_contract::CandidateFamilyKindV1::
                SingleExecuteIndirectDispatch)
        {
            commandList_->ExecuteIndirect(
                singleCommandSignature_.Get(),
                1,
                singleArguments_.Get(),
                0,
                nullptr,
                0);
        }
        else
        {
            commandList_->ExecuteIndirect(
                batchCommandSignature_.Get(),
                candidate.Verified().Plan().maxCommandCount,
                batchArguments_.Get(),
                0,
                nullptr,
                0);
        }

        commandList_->EndQuery(
            timestampHeap_.Get(),
            D3D12_QUERY_TYPE_TIMESTAMP,
            3);

        auto outputUav = UavBarrier(output_.Get());
        commandList_->ResourceBarrier(1, &outputUav);
        auto outputToCopy = TransitionBarrier(
            output_.Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COPY_SOURCE);
        commandList_->ResourceBarrier(1, &outputToCopy);

        commandList_->EndQuery(
            timestampHeap_.Get(),
            D3D12_QUERY_TYPE_TIMESTAMP,
            4);

        commandList_->CopyBufferRegion(
            outputReadback_.Get(),
            0,
            output_.Get(),
            0,
            outputBytes_);

        if (kind ==
            family_contract::CandidateFamilyKindV1::
                SingleExecuteIndirectDispatch)
        {
            auto toCopy = TransitionBarrier(
                singleArguments_.Get(),
                D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT,
                D3D12_RESOURCE_STATE_COPY_SOURCE);
            commandList_->ResourceBarrier(1, &toCopy);
            commandList_->CopyBufferRegion(
                singleArgumentReadback_.Get(),
                0,
                singleArguments_.Get(),
                0,
                sizeof(DispatchArguments));
            auto back = TransitionBarrier(
                singleArguments_.Get(),
                D3D12_RESOURCE_STATE_COPY_SOURCE,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            commandList_->ResourceBarrier(1, &back);
        }
        else if (kind ==
            family_contract::CandidateFamilyKindV1::
                BatchedExecuteIndirectDispatch)
        {
            auto toCopy = TransitionBarrier(
                batchArguments_.Get(),
                D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT,
                D3D12_RESOURCE_STATE_COPY_SOURCE);
            commandList_->ResourceBarrier(1, &toCopy);
            commandList_->CopyBufferRegion(
                batchArgumentReadback_.Get(),
                0,
                batchArguments_.Get(),
                0,
                candidate.Verified().Plan().argumentBufferBytes);
            auto back = TransitionBarrier(
                batchArguments_.Get(),
                D3D12_RESOURCE_STATE_COPY_SOURCE,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            commandList_->ResourceBarrier(1, &back);
        }

        commandList_->EndQuery(
            timestampHeap_.Get(),
            D3D12_QUERY_TYPE_TIMESTAMP,
            5);
        commandList_->ResolveQueryData(
            timestampHeap_.Get(),
            D3D12_QUERY_TYPE_TIMESTAMP,
            0,
            TimestampCount,
            timestampReadback_.Get(),
            0);

        auto outputBack = TransitionBarrier(
            output_.Get(),
            D3D12_RESOURCE_STATE_COPY_SOURCE,
            D3D12_RESOURCE_STATE_COPY_DEST);
        commandList_->ResourceBarrier(1, &outputBack);
        Check(commandList_->Close(), "CommandList Close");
        const auto recordEnd = Clock::now();

        const auto submitBegin = Clock::now();
        ID3D12CommandList* lists[] = {commandList_.Get()};
        queue_->ExecuteCommandLists(1, lists);
        ++fenceValue_;
        Check(queue_->Signal(fence_.Get(), fenceValue_), "Queue Signal");
        if (fence_->GetCompletedValue() < fenceValue_)
        {
            Check(
                fence_->SetEventOnCompletion(
                    fenceValue_,
                    fenceEvent_.Get()),
                "SetEventOnCompletion");
            WaitForSingleObject(fenceEvent_.Get(), INFINITE);
        }
        const auto submitEnd = Clock::now();

        const auto observationBegin = Clock::now();
        TimingSampleV1 sample;
        sample.run = run;
        sample.orderIndex = orderIndex;
        sample.cycle = cycle;
        sample.orderPosition = orderPosition;
        sample.candidate = CanonicalCandidateIdsV1()[candidateIndex];
        sample.activeWorkCount = activeWorkCount;
        sample.activeInputPreparationNanoseconds =
            Nanoseconds(inputBegin, inputEnd);
        sample.commandRecordingNanoseconds =
            Nanoseconds(recordBegin, recordEnd);
        sample.submitAndWaitNanoseconds =
            Nanoseconds(submitBegin, submitEnd);

        const auto ticksToNanoseconds = [this](std::uint64_t ticks)
        {
            return static_cast<double>(ticks) * 1.0e9 /
                static_cast<double>(timestampFrequency_);
        };
        const auto delta = [&](std::uint32_t begin, std::uint32_t end)
        {
            if (mappedTimestamps_[end] < mappedTimestamps_[begin])
            {
                throw std::runtime_error("GPU timestamp order is invalid.");
            }
            return ticksToNanoseconds(
                mappedTimestamps_[end] - mappedTimestamps_[begin]);
        };

        sample.gpuArgumentGenerationNanoseconds = delta(0, 1);
        sample.gpuStateTransitionNanoseconds = delta(1, 2);
        sample.gpuExecutionNanoseconds = delta(2, 3);
        sample.gpuOutputSynchronizationNanoseconds = delta(3, 4);
        sample.gpuObservationCopyNanoseconds = delta(4, 5);
        sample.gpuCandidatePathNanoseconds = delta(0, 4);
        sample.gpuTotalNanoseconds = delta(0, 5);

        ValidateArguments(candidate, activeWorkCount, sample);
        ValidateOutput(activeWorkCount, sample);
        const auto observationEnd = Clock::now();
        sample.numericObservationNanoseconds =
            Nanoseconds(observationBegin, observationEnd);
        sample.cpuEndToEndNanoseconds =
            Nanoseconds(totalBegin, observationEnd);

        if (sample.activeMismatchCount != 0 ||
            sample.nonFiniteCount != 0 ||
            sample.homogeneousCoordinateMismatchCount != 0 ||
            sample.duplicateWorkCount != 0 ||
            sample.missingWorkCount != 0 ||
            sample.reorderedWorkCount != 0 ||
            sample.outOfRangeWorkCount != 0 ||
            !sample.inactiveTailPreserved)
        {
            throw std::runtime_error(
                "CU6 measurement Observation Contract failed.");
        }
        (void)recordEvidence;
        return sample;
    }

private:
    static constexpr std::uint32_t TimestampCount = 6;

    void CreateQueueObjects()
    {
        D3D12_COMMAND_QUEUE_DESC queueDescription{};
        queueDescription.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
        Check(
            device_->CreateCommandQueue(
                &queueDescription,
                IID_PPV_ARGS(&queue_)),
            "CreateCommandQueue");
        Check(
            queue_->GetTimestampFrequency(&timestampFrequency_),
            "GetTimestampFrequency");
        if (timestampFrequency_ == 0)
        {
            throw std::runtime_error("D3D12 timestamp frequency is zero.");
        }

        Check(
            device_->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_COMPUTE,
                IID_PPV_ARGS(&allocator_)),
            "CreateCommandAllocator");
        Check(
            device_->CreateCommandList(
                0,
                D3D12_COMMAND_LIST_TYPE_COMPUTE,
                allocator_.Get(),
                nullptr,
                IID_PPV_ARGS(&commandList_)),
            "CreateCommandList");
        Check(commandList_->Close(), "Initial CommandList Close");
    }

    void CreateRootAndPipelineObjects()
    {
        D3D12_DESCRIPTOR_RANGE singleRanges[2]{};
        singleRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        singleRanges[0].NumDescriptors = 1;
        singleRanges[0].BaseShaderRegister = 0;
        singleRanges[0].OffsetInDescriptorsFromTableStart =
            D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        singleRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        singleRanges[1].NumDescriptors = 1;
        singleRanges[1].BaseShaderRegister = 0;
        singleRanges[1].OffsetInDescriptorsFromTableStart =
            D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_ROOT_PARAMETER singleParameters[2]{};
        for (UINT index = 0; index < 2; ++index)
        {
            singleParameters[index].ParameterType =
                D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            singleParameters[index].DescriptorTable.NumDescriptorRanges = 1;
            singleParameters[index].DescriptorTable.pDescriptorRanges =
                &singleRanges[index];
        }

        D3D12_DESCRIPTOR_RANGE batchRanges[2]{};
        batchRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        batchRanges[0].NumDescriptors = 1;
        batchRanges[0].BaseShaderRegister = 0;
        batchRanges[0].OffsetInDescriptorsFromTableStart =
            D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        batchRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        batchRanges[1].NumDescriptors = 1;
        batchRanges[1].BaseShaderRegister = 0;
        batchRanges[1].OffsetInDescriptorsFromTableStart =
            D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_ROOT_PARAMETER batchParameters[3]{};
        batchParameters[0].ParameterType =
            D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        batchParameters[0].Constants.ShaderRegister = 0;
        batchParameters[0].Constants.RegisterSpace = 0;
        batchParameters[0].Constants.Num32BitValues = 2;
        for (UINT index = 0; index < 2; ++index)
        {
            batchParameters[index + 1].ParameterType =
                D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            batchParameters[index + 1].DescriptorTable.NumDescriptorRanges = 1;
            batchParameters[index + 1].DescriptorTable.pDescriptorRanges =
                &batchRanges[index];
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

        D3D12_ROOT_PARAMETER consumerParameters[3]{};
        consumerParameters[0].ParameterType =
            D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        consumerParameters[0].Constants.ShaderRegister = 0;
        consumerParameters[0].Constants.RegisterSpace = 0;
        consumerParameters[0].Constants.Num32BitValues = 1;
        for (UINT index = 0; index < 2; ++index)
        {
            consumerParameters[index + 1].ParameterType =
                D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            consumerParameters[index + 1].DescriptorTable.NumDescriptorRanges = 1;
            consumerParameters[index + 1].DescriptorTable.pDescriptorRanges =
                &consumerRanges[index];
        }

        singleProducerRoot_ = CreateRootSignature(
            device_,
            singleParameters);
        batchProducerRoot_ = CreateRootSignature(
            device_,
            batchParameters);
        consumerRoot_ = CreateRootSignature(
            device_,
            consumerParameters);

        singleProducerShader_ = CompileShader(
            SingleProducerSource(),
            "Single Argument Producer");
        batchProducerShader_ = CompileShader(
            BatchProducerSource(),
            "Batch Argument Producer");
        consumerShader_ = CompileShader(
            ConsumerSource(),
            "Candidate Family Consumer");

        singleProducerPipeline_ = CreateComputePipeline(
            device_,
            singleProducerRoot_.Get(),
            singleProducerShader_.Get());
        batchProducerPipeline_ = CreateComputePipeline(
            device_,
            batchProducerRoot_.Get(),
            batchProducerShader_.Get());
        consumerPipeline_ = CreateComputePipeline(
            device_,
            consumerRoot_.Get(),
            consumerShader_.Get());

        singleProducerDigest_ = base::Sha256(
            std::span<const std::byte>(
                static_cast<const std::byte*>(
                    singleProducerShader_->GetBufferPointer()),
                singleProducerShader_->GetBufferSize()));
        batchProducerDigest_ = base::Sha256(
            std::span<const std::byte>(
                static_cast<const std::byte*>(
                    batchProducerShader_->GetBufferPointer()),
                batchProducerShader_->GetBufferSize()));
        consumerDigest_ = base::Sha256(
            std::span<const std::byte>(
                static_cast<const std::byte*>(
                    consumerShader_->GetBufferPointer()),
                consumerShader_->GetBufferSize()));
    }

    void CreateCommandSignatures()
    {
        D3D12_INDIRECT_ARGUMENT_DESC singleArgument{};
        singleArgument.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
        D3D12_COMMAND_SIGNATURE_DESC singleDescription{};
        singleDescription.ByteStride = sizeof(DispatchArguments);
        singleDescription.NumArgumentDescs = 1;
        singleDescription.pArgumentDescs = &singleArgument;
        Check(
            device_->CreateCommandSignature(
                &singleDescription,
                nullptr,
                IID_PPV_ARGS(&singleCommandSignature_)),
            "Create single Command Signature");

        D3D12_INDIRECT_ARGUMENT_DESC batchArguments[2]{};
        batchArguments[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;
        batchArguments[0].Constant.RootParameterIndex = 0;
        batchArguments[0].Constant.DestOffsetIn32BitValues = 0;
        batchArguments[0].Constant.Num32BitValuesToSet = 1;
        batchArguments[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
        D3D12_COMMAND_SIGNATURE_DESC batchDescription{};
        batchDescription.ByteStride = sizeof(BatchCommand);
        batchDescription.NumArgumentDescs = 2;
        batchDescription.pArgumentDescs = batchArguments;
        Check(
            device_->CreateCommandSignature(
                &batchDescription,
                consumerRoot_.Get(),
                IID_PPV_ARGS(&batchCommandSignature_)),
            "Create Batch Command Signature");
    }

    void CreateResources()
    {
        transformBytes_ =
            static_cast<std::uint64_t>(transformsCpu_.size()) *
            sizeof(Motor);
        outputBytes_ =
            static_cast<std::uint64_t>(semantic_.maxWorkCount.Value()) *
            sizeof(Float4);
        batchArgumentBytes_ = 64ull * sizeof(BatchCommand);

        transforms_ = CreateBuffer(
            device_,
            D3D12_HEAP_TYPE_UPLOAD,
            transformBytes_,
            D3D12_RESOURCE_STATE_GENERIC_READ);
        points_ = CreateBuffer(
            device_,
            D3D12_HEAP_TYPE_UPLOAD,
            outputBytes_,
            D3D12_RESOURCE_STATE_GENERIC_READ);
        activeCount_ = CreateBuffer(
            device_,
            D3D12_HEAP_TYPE_UPLOAD,
            sizeof(std::uint32_t),
            D3D12_RESOURCE_STATE_GENERIC_READ);
        sentinelUpload_ = CreateBuffer(
            device_,
            D3D12_HEAP_TYPE_UPLOAD,
            outputBytes_,
            D3D12_RESOURCE_STATE_GENERIC_READ);

        singleArguments_ = CreateBuffer(
            device_,
            D3D12_HEAP_TYPE_DEFAULT,
            sizeof(DispatchArguments),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        batchArguments_ = CreateBuffer(
            device_,
            D3D12_HEAP_TYPE_DEFAULT,
            batchArgumentBytes_,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        output_ = CreateBuffer(
            device_,
            D3D12_HEAP_TYPE_DEFAULT,
            outputBytes_,
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

        outputReadback_ = CreateBuffer(
            device_,
            D3D12_HEAP_TYPE_READBACK,
            outputBytes_,
            D3D12_RESOURCE_STATE_COPY_DEST);
        singleArgumentReadback_ = CreateBuffer(
            device_,
            D3D12_HEAP_TYPE_READBACK,
            sizeof(DispatchArguments),
            D3D12_RESOURCE_STATE_COPY_DEST);
        batchArgumentReadback_ = CreateBuffer(
            device_,
            D3D12_HEAP_TYPE_READBACK,
            batchArgumentBytes_,
            D3D12_RESOURCE_STATE_COPY_DEST);

        void* mapped = nullptr;
        Check(transforms_->Map(0, nullptr, &mapped), "Map transforms");
        std::memcpy(mapped, transformsCpu_.data(), transformBytes_);
        transforms_->Unmap(0, nullptr);

        Check(points_->Map(0, nullptr, &mapped), "Map points");
        std::memcpy(mapped, pointsCpu_.data(), outputBytes_);
        points_->Unmap(0, nullptr);

        std::vector<Float4> sentinels(
            semantic_.maxWorkCount.Value(),
            InactiveSentinel);
        Check(
            sentinelUpload_->Map(0, nullptr, &mapped),
            "Map sentinel upload");
        std::memcpy(mapped, sentinels.data(), outputBytes_);
        sentinelUpload_->Unmap(0, nullptr);

        Check(
            activeCount_->Map(
                0,
                nullptr,
                reinterpret_cast<void**>(&mappedActiveCount_)),
            "Map Active Count");

        D3D12_RANGE noRead{0, 0};
        Check(
            outputReadback_->Map(
                0,
                &noRead,
                reinterpret_cast<void**>(&mappedOutput_)),
            "Map output readback");
        Check(
            singleArgumentReadback_->Map(
                0,
                &noRead,
                reinterpret_cast<void**>(&mappedSingleArguments_)),
            "Map single argument readback");
        Check(
            batchArgumentReadback_->Map(
                0,
                &noRead,
                reinterpret_cast<void**>(&mappedBatchArguments_)),
            "Map Batch argument readback");
    }

    void CreateDescriptors()
    {
        D3D12_DESCRIPTOR_HEAP_DESC description{};
        description.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        description.NumDescriptors = 8;
        description.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        Check(
            device_->CreateDescriptorHeap(
                &description,
                IID_PPV_ARGS(&descriptorHeap_)),
            "CreateDescriptorHeap");
        descriptorIncrement_ =
            device_->GetDescriptorHandleIncrementSize(
                D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        cpuDescriptorStart_ =
            descriptorHeap_->GetCPUDescriptorHandleForHeapStart();
        gpuDescriptorStart_ =
            descriptorHeap_->GetGPUDescriptorHandleForHeapStart();

        auto createSrv = [this](
            UINT index,
            ID3D12Resource* resource,
            UINT count,
            UINT stride)
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC view{};
            view.Shader4ComponentMapping =
                D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            view.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            view.Buffer.NumElements = count;
            view.Buffer.StructureByteStride = stride;
            device_->CreateShaderResourceView(
                resource,
                &view,
                CpuHandle(
                    cpuDescriptorStart_,
                    descriptorIncrement_,
                    index));
        };

        auto createUav = [this](
            UINT index,
            ID3D12Resource* resource,
            UINT count,
            UINT stride)
        {
            D3D12_UNORDERED_ACCESS_VIEW_DESC view{};
            view.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
            view.Buffer.NumElements = count;
            view.Buffer.StructureByteStride = stride;
            device_->CreateUnorderedAccessView(
                resource,
                nullptr,
                &view,
                CpuHandle(
                    cpuDescriptorStart_,
                    descriptorIncrement_,
                    index));
        };

        createSrv(0, activeCount_.Get(), 1, sizeof(std::uint32_t));
        createUav(1, singleArguments_.Get(), 1, sizeof(DispatchArguments));
        createSrv(2, activeCount_.Get(), 1, sizeof(std::uint32_t));
        createUav(3, batchArguments_.Get(), 64, sizeof(BatchCommand));
        createSrv(4, transforms_.Get(), 8, sizeof(Motor));
        createSrv(
            5,
            points_.Get(),
            semantic_.maxWorkCount.Value(),
            sizeof(Float4));
        createSrv(6, activeCount_.Get(), 1, sizeof(std::uint32_t));
        createUav(
            7,
            output_.Get(),
            semantic_.maxWorkCount.Value(),
            sizeof(Float4));
    }

    void CreateTimestampObjects()
    {
        D3D12_QUERY_HEAP_DESC description{};
        description.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
        description.Count = TimestampCount;
        Check(
            device_->CreateQueryHeap(
                &description,
                IID_PPV_ARGS(&timestampHeap_)),
            "Create timestamp query heap");
        timestampReadback_ = CreateBuffer(
            device_,
            D3D12_HEAP_TYPE_READBACK,
            TimestampCount * sizeof(std::uint64_t),
            D3D12_RESOURCE_STATE_COPY_DEST);
        D3D12_RANGE noRead{0, 0};
        Check(
            timestampReadback_->Map(
                0,
                &noRead,
                reinterpret_cast<void**>(&mappedTimestamps_)),
            "Map timestamp readback");
    }

    void CreateFenceObjects()
    {
        Check(
            device_->CreateFence(
                0,
                D3D12_FENCE_FLAG_NONE,
                IID_PPV_ARGS(&fence_)),
            "CreateFence");
    }

    void ValidateArguments(
        const family_execution::FrozenVerifiedCandidateFamilyV1& candidate,
        std::uint32_t activeWorkCount,
        TimingSampleV1& sample)
    {
        const auto kind = candidate.Verified().Plan().kind;
        if (kind ==
            family_contract::CandidateFamilyKindV1::FixedMaximumGuarded)
        {
            sample.issuedThreadGroupCount =
                candidate.Verified().Plan().directDispatchX;
            return;
        }

        const std::uint32_t expectedSingleGroups =
            activeWorkCount == 0
                ? 0u
                : 1u + ((activeWorkCount - 1u) / 64u);
        if (kind ==
            family_contract::CandidateFamilyKindV1::
                SingleExecuteIndirectDispatch)
        {
            sample.issuedThreadGroupCount = mappedSingleArguments_->x;
            if (mappedSingleArguments_->x != expectedSingleGroups ||
                mappedSingleArguments_->y != 1u ||
                mappedSingleArguments_->z != 1u)
            {
                ++sample.missingWorkCount;
            }
            return;
        }

        auto expected =
            family_verification::DeriveVerifiedBatchDispatchRecordsV1(
                candidate.Verified(),
                semantic::ActiveWorkCountV1(
                    activeWorkCount,
                    semantic_.maxWorkCount));
        if (!expected)
        {
            throw std::runtime_error(expected.Error());
        }
        if (expected.Value().size() !=
            candidate.Verified().Plan().maxBatchSlots)
        {
            throw std::runtime_error(
                "Verified Batch record count differs from max slots.");
        }

        for (std::uint32_t slot = 0;
             slot < candidate.Verified().Plan().maxBatchSlots;
             ++slot)
        {
            const auto& actual = mappedBatchArguments_[slot];
            const auto& expectedRecord = expected.Value()[slot];
            sample.issuedThreadGroupCount += actual.x;
            if (actual.firstWork != expectedRecord.firstWork ||
                actual.x != expectedRecord.threadGroupCountX ||
                actual.y != expectedRecord.threadGroupCountY ||
                actual.z != expectedRecord.threadGroupCountZ)
            {
                ++sample.missingWorkCount;
            }
        }
    }

    void ValidateOutput(
        std::uint32_t activeWorkCount,
        TimingSampleV1& sample)
    {
        sample.inactiveTailPreserved = true;
        for (std::uint32_t index = 0;
             index < activeWorkCount;
             ++index)
        {
            const Float4 expected = ExpectedPoint(pointsCpu_[index], index);
            const Float4 actual = mappedOutput_[index];
            const float actualValues[] = {
                actual.x,
                actual.y,
                actual.z,
                actual.w
            };
            const float expectedValues[] = {
                expected.x,
                expected.y,
                expected.z,
                expected.w
            };
            float localMax = 0.0f;
            for (std::size_t component = 0; component < 4; ++component)
            {
                const float absolute = std::abs(
                    actualValues[component] - expectedValues[component]);
                localMax = std::max(localMax, absolute);
                sample.maxRelativeError = std::max(
                    sample.maxRelativeError,
                    static_cast<double>(RelativeError(
                        actualValues[component],
                        expectedValues[component])));
                if (std::isfinite(actualValues[component]) &&
                    std::isfinite(expectedValues[component]))
                {
                    sample.maxUlpError = std::max(
                        sample.maxUlpError,
                        UlpDistance(
                            actualValues[component],
                            expectedValues[component]));
                }
            }
            sample.maxAbsoluteError = std::max(
                sample.maxAbsoluteError,
                static_cast<double>(localMax));
            const bool finite =
                std::isfinite(actual.x) &&
                std::isfinite(actual.y) &&
                std::isfinite(actual.z) &&
                std::isfinite(actual.w);
            if (!finite)
            {
                ++sample.nonFiniteCount;
            }
            if (!std::isfinite(actual.w) ||
                std::abs(actual.w - 1.0f) > 1.0e-5f)
            {
                ++sample.homogeneousCoordinateMismatchCount;
            }
            if (!finite || localMax > 1.0e-5f)
            {
                ++sample.activeMismatchCount;
            }
        }

        for (std::uint32_t index = activeWorkCount;
             index < semantic_.maxWorkCount.Value();
             ++index)
        {
            if (!IsSentinel(mappedOutput_[index]))
            {
                sample.inactiveTailPreserved = false;
                break;
            }
        }

        sample.outputDigest = base::Sha256(
            std::as_bytes(std::span(
                mappedOutput_,
                semantic_.maxWorkCount.Value())));
    }

    ID3D12Device* device_ = nullptr;
    std::string adapterDescription_;
    semantic::ActiveWorkSemanticV1 semantic_;
    std::vector<family_execution::FrozenVerifiedCandidateFamilyV1> candidates_;
    std::vector<Motor> transformsCpu_;
    std::vector<Float4> pointsCpu_;

    ComPtr<ID3D12CommandQueue> queue_;
    ComPtr<ID3D12CommandAllocator> allocator_;
    ComPtr<ID3D12GraphicsCommandList> commandList_;

    ComPtr<ID3D12RootSignature> singleProducerRoot_;
    ComPtr<ID3D12RootSignature> batchProducerRoot_;
    ComPtr<ID3D12RootSignature> consumerRoot_;
    ComPtr<ID3DBlob> singleProducerShader_;
    ComPtr<ID3DBlob> batchProducerShader_;
    ComPtr<ID3DBlob> consumerShader_;
    ComPtr<ID3D12PipelineState> singleProducerPipeline_;
    ComPtr<ID3D12PipelineState> batchProducerPipeline_;
    ComPtr<ID3D12PipelineState> consumerPipeline_;
    ComPtr<ID3D12CommandSignature> singleCommandSignature_;
    ComPtr<ID3D12CommandSignature> batchCommandSignature_;

    ComPtr<ID3D12Resource> transforms_;
    ComPtr<ID3D12Resource> points_;
    ComPtr<ID3D12Resource> activeCount_;
    ComPtr<ID3D12Resource> sentinelUpload_;
    ComPtr<ID3D12Resource> singleArguments_;
    ComPtr<ID3D12Resource> batchArguments_;
    ComPtr<ID3D12Resource> output_;
    ComPtr<ID3D12Resource> outputReadback_;
    ComPtr<ID3D12Resource> singleArgumentReadback_;
    ComPtr<ID3D12Resource> batchArgumentReadback_;

    ComPtr<ID3D12DescriptorHeap> descriptorHeap_;
    UINT descriptorIncrement_ = 0;
    D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorStart_{};
    D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptorStart_{};

    ComPtr<ID3D12QueryHeap> timestampHeap_;
    ComPtr<ID3D12Resource> timestampReadback_;
    std::uint64_t* mappedTimestamps_ = nullptr;
    std::uint64_t timestampFrequency_ = 0;

    ComPtr<ID3D12Fence> fence_;
    EventHandle fenceEvent_;
    std::uint64_t fenceValue_ = 0;

    std::uint32_t* mappedActiveCount_ = nullptr;
    Float4* mappedOutput_ = nullptr;
    DispatchArguments* mappedSingleArguments_ = nullptr;
    BatchCommand* mappedBatchArguments_ = nullptr;

    std::uint64_t transformBytes_ = 0;
    std::uint64_t outputBytes_ = 0;
    std::uint64_t batchArgumentBytes_ = 0;

    base::Digest256 singleProducerDigest_{};
    base::Digest256 batchProducerDigest_{};
    base::Digest256 consumerDigest_{};
};

base::Digest256 BuildAdapterFingerprint(const AdapterProfileV1& profile)
{
    base::BinaryWriter writer;
    WriteString(writer, "SGE4-5.Spiral4.AdapterProfile.V1");
    WriteString(writer, profile.description);
    writer.WriteU32(profile.vendorId);
    writer.WriteU32(profile.deviceId);
    writer.WriteU32(profile.subsystemId);
    writer.WriteU32(profile.revision);
    writer.WriteU32(profile.luidLow);
    writer.WriteI32(profile.luidHigh);
    writer.WriteU64(profile.driverVersion);
    writer.WriteU64(profile.timestampFrequency);
    writer.WriteU8(profile.uma ? 1u : 0u);
    writer.WriteU8(profile.cacheCoherentUma ? 1u : 0u);
    return base::Sha256(std::move(writer).Take());
}
}
#endif

base::Result<MeasurementEvidenceV1, std::string>
RunRealGpuMeasurementV1(const MeasurementConfigV1& config)
{
#if defined(_WIN32)
    try
    {
        ValidateConfigLocal(config);
        const auto semanticValue =
            semantic::BuildCanonicalActiveWorkSemanticV1();
        const auto context =
            family_verification::
                BuildCanonicalCandidateFamilyVerificationContextV1();

        auto rawResult =
            family_planner::PlanCanonicalCandidateFamilyCorpusV1(
                semanticValue);
        if (!rawResult)
        {
            return base::Result<MeasurementEvidenceV1, std::string>::Failure(
                rawResult.Error());
        }
        if (rawResult.Value().size() != CanonicalCandidateCountV1)
        {
            return base::Result<MeasurementEvidenceV1, std::string>::Failure(
                "CU6 Planner did not produce seven Candidates.");
        }

        std::vector<family_verification::VerifiedCandidateFamilyV1> verified;
        std::vector<family_execution::FrozenVerifiedCandidateFamilyV1> frozen;
        verified.reserve(CanonicalCandidateCountV1);
        frozen.reserve(CanonicalCandidateCountV1);
        for (const auto& raw : rawResult.Value())
        {
            auto verifiedResult =
                family_verification::VerifyCandidateFamilyV1(
                    semanticValue,
                    context,
                    raw);
            if (!verifiedResult)
            {
                return base::Result<MeasurementEvidenceV1, std::string>::Failure(
                    verifiedResult.Error());
            }
            verified.push_back(verifiedResult.Value());
            auto frozenResult =
                family_execution::FreezeVerifiedCandidateFamilyV1(
                    semanticValue,
                    context,
                    verified.back());
            if (!frozenResult)
            {
                return base::Result<MeasurementEvidenceV1, std::string>::Failure(
                    frozenResult.Error());
            }
            frozen.push_back(frozenResult.Value());
        }

        auto hardware = CreateHardwareContext(config.adapterIndex);
        MeasurementSession session(
            hardware.device.Get(),
            hardware.profile.description,
            semanticValue,
            frozen);
        hardware.profile.timestampFrequency = session.TimestampFrequency();
        hardware.profile.fingerprint = BuildAdapterFingerprint(hardware.profile);

        MeasurementEvidenceV1 evidence;
        evidence.captureUnixNanoseconds = CaptureUnixNanoseconds();
        evidence.config = config;
        evidence.adapter = hardware.profile;
        evidence.architectureEvidenceIdentity =
            DigestFromHex(ArchitectureEvidenceHex);
        evidence.controlledRecoveryEvidenceIdentity =
            DigestFromHex(ControlledRecoveryEvidenceHex);
        evidence.freshRematerializationEvidenceIdentity =
            DigestFromHex(FreshRematerializationEvidenceHex);
        evidence.semanticIdentity =
            semantic::ActiveWorkSemanticIdentityV1(semanticValue);
        evidence.verificationContextIdentity =
            BuildVerificationContextIdentity(context);
        evidence.measurementProfileIdentity =
            BuildMeasurementProfileIdentity(config);
        evidence.orderBalanceIdentity = BuildOrderBalanceIdentity();

        const auto canonicalCandidates = CanonicalCandidateIdsV1();
        for (std::size_t index = 0;
             index < CanonicalCandidateCountV1;
             ++index)
        {
            CandidateProfileV1 profile;
            profile.candidate = canonicalCandidates[index];
            profile.maxBatchSlots =
                verified[index].Plan().maxBatchSlots;
            profile.argumentStrideBytes =
                verified[index].Plan().argumentStrideBytes;
            profile.argumentBufferBytes =
                verified[index].Plan().argumentBufferBytes;
            profile.maxCommandCount =
                verified[index].Plan().maxCommandCount;
            profile.rawCandidateIdentity =
                rawResult.Value()[index].rawCandidateIdentity;
            profile.verifiedPlanIdentity =
                verified[index].Plan().planIdentity;
            profile.verifierCertificateIdentity =
                verified[index].CertificateIdentity();
            profile.artifactIdentity =
                frozen[index].Artifact().ArtifactIdentity();
            profile.frozenBindingIdentity =
                frozen[index].BindingIdentity();
            profile.producerProgramTemplateIdentity =
                verified[index].Plan().producerProgramTemplateIdentity;
            profile.consumerProgramTemplateIdentity =
                verified[index].Plan().consumerProgramTemplateIdentity;
            if (index == 0)
            {
                profile.producerShaderBytecodeDigest = LiteralIdentity(
                    "SGE4-5.Spiral4.NoArgumentProducerBytecode.V1");
            }
            else if (index == 1)
            {
                profile.producerShaderBytecodeDigest =
                    session.SingleProducerDigest();
            }
            else
            {
                profile.producerShaderBytecodeDigest =
                    session.BatchProducerDigest();
            }
            profile.consumerShaderBytecodeDigest =
                session.ConsumerDigest();
            evidence.candidates.push_back(std::move(profile));
        }

        const auto activeCounts = semantic::ActiveCountCorpusV1();
        const auto orders = CanonicalBalancedOrdersV1();
        for (const std::uint32_t activeWorkCount : activeCounts)
        {
            for (std::uint32_t warmup = 0;
                 warmup < config.warmupCyclesPerOrder;
                 ++warmup)
            {
                for (std::uint32_t orderIndex = 0;
                     orderIndex < orders.size();
                     ++orderIndex)
                {
                    for (std::uint32_t position = 0;
                         position < CanonicalCandidateCountV1;
                         ++position)
                    {
                        (void)session.Measure(
                            orders[orderIndex][position],
                            activeWorkCount,
                            0,
                            orderIndex,
                            warmup,
                            position,
                            false);
                    }
                }
            }

            MeasurementCaseV1 measurementCase;
            measurementCase.activeWorkCount = activeWorkCount;
            const std::size_t expectedSampleCount =
                static_cast<std::size_t>(config.runCount) *
                CanonicalOrderCountV1 *
                config.measurementCyclesPerOrder *
                CanonicalCandidateCountV1;
            measurementCase.samples.reserve(expectedSampleCount);

            for (std::uint32_t run = 0;
                 run < config.runCount;
                 ++run)
            {
                for (std::uint32_t orderIndex = 0;
                     orderIndex < orders.size();
                     ++orderIndex)
                {
                    for (std::uint32_t cycle = 0;
                         cycle < config.measurementCyclesPerOrder;
                         ++cycle)
                    {
                        for (std::uint32_t position = 0;
                             position < CanonicalCandidateCountV1;
                             ++position)
                        {
                            measurementCase.samples.push_back(
                                session.Measure(
                                    orders[orderIndex][position],
                                    activeWorkCount,
                                    run,
                                    orderIndex,
                                    cycle,
                                    position,
                                    true));
                        }
                    }
                }
            }

            std::cout
                << "[MEASURED] Nf=" << activeWorkCount
                << " samples=" << measurementCase.samples.size()
                << '\n';
            evidence.cases.push_back(std::move(measurementCase));
        }

        return base::Result<MeasurementEvidenceV1, std::string>::Success(
            std::move(evidence));
    }
    catch (const std::exception& error)
    {
        return base::Result<MeasurementEvidenceV1, std::string>::Failure(
            error.what());
    }
#else
    (void)config;
    return base::Result<MeasurementEvidenceV1, std::string>::Failure(
        "Real GPU measurement requires Windows and D3D12.");
#endif
}
}
