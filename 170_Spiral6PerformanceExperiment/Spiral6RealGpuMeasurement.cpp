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
#include <atomic>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <map>
#include <mutex>
#include <memory>
#include <set>
#include <span>
#include <thread>
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

// Minimal, dynamically loaded NVML ABI used by the diagnostic probe. Keeping
// this local avoids introducing a CUDA Toolkit or nvml.lib build dependency.
namespace nvml_dynamic
{
using Return = int;
struct DeviceOpaque;
using Device = DeviceOpaque*;

inline constexpr Return Success = 0;
inline constexpr Return FunctionUnavailable = -10000;
inline constexpr unsigned int ClockGraphics = 0;
inline constexpr unsigned int ClockSm = 1;
inline constexpr unsigned int ClockMemory = 2;
inline constexpr unsigned int ClockVideo = 3;
inline constexpr unsigned int TemperatureGpu = 0;

inline constexpr std::uint64_t ReasonGpuIdle = 0x0000'0000'0000'0001ull;
inline constexpr std::uint64_t ReasonApplicationClocks = 0x0000'0000'0000'0002ull;
inline constexpr std::uint64_t ReasonSoftwarePowerCap = 0x0000'0000'0000'0004ull;
inline constexpr std::uint64_t ReasonHardwareSlowdown = 0x0000'0000'0000'0008ull;
inline constexpr std::uint64_t ReasonSyncBoost = 0x0000'0000'0000'0010ull;
inline constexpr std::uint64_t ReasonSoftwareThermal = 0x0000'0000'0000'0020ull;
inline constexpr std::uint64_t ReasonHardwareThermal = 0x0000'0000'0000'0040ull;
inline constexpr std::uint64_t ReasonHardwarePowerBrake = 0x0000'0000'0000'0080ull;
inline constexpr std::uint64_t ReasonDisplayClock = 0x0000'0000'0000'0100ull;

struct Utilization final
{
    unsigned int gpu = 0;
    unsigned int memory = 0;
};

struct Snapshot final
{
    std::uint64_t elapsedNanoseconds = 0;
    std::uint64_t unixNanoseconds = 0;
    std::uint64_t ordinal = 0;

    unsigned int graphicsClockMHz = 0;
    unsigned int smClockMHz = 0;
    unsigned int memoryClockMHz = 0;
    unsigned int videoClockMHz = 0;
    unsigned int performanceState = 0;
    unsigned int temperatureC = 0;
    unsigned int powerUsageMilliwatts = 0;
    unsigned int powerLimitMilliwatts = 0;
    unsigned int gpuUtilizationPercent = 0;
    unsigned int memoryUtilizationPercent = 0;
    std::uint64_t clockEventReasons = 0;

    Return graphicsClockResult = FunctionUnavailable;
    Return smClockResult = FunctionUnavailable;
    Return memoryClockResult = FunctionUnavailable;
    Return videoClockResult = FunctionUnavailable;
    Return performanceStateResult = FunctionUnavailable;
    Return temperatureResult = FunctionUnavailable;
    Return powerUsageResult = FunctionUnavailable;
    Return powerLimitResult = FunctionUnavailable;
    Return utilizationResult = FunctionUnavailable;
    Return clockEventReasonsResult = FunctionUnavailable;
};

std::uint64_t UnixNanosecondsNow()
{
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}

std::string EventReasonNames(std::uint64_t reasons)
{
    if (reasons == 0) return "None";
    std::vector<std::string_view> names;
    const auto add = [&](std::uint64_t bit, std::string_view name)
    {
        if ((reasons & bit) != 0) names.push_back(name);
    };
    add(ReasonGpuIdle, "GpuIdle");
    add(ReasonApplicationClocks, "ApplicationsClocksSetting");
    add(ReasonSoftwarePowerCap, "SwPowerCap");
    add(ReasonHardwareSlowdown, "HwSlowdown");
    add(ReasonSyncBoost, "SyncBoost");
    add(ReasonSoftwareThermal, "SwThermalSlowdown");
    add(ReasonHardwareThermal, "HwThermalSlowdown");
    add(ReasonHardwarePowerBrake, "HwPowerBrakeSlowdown");
    add(ReasonDisplayClock, "DisplayClockSetting");
    std::ostringstream stream;
    for (std::size_t index = 0; index < names.size(); ++index)
    {
        if (index != 0) stream << '|';
        stream << names[index];
    }
    const std::uint64_t known = ReasonGpuIdle | ReasonApplicationClocks |
        ReasonSoftwarePowerCap | ReasonHardwareSlowdown | ReasonSyncBoost |
        ReasonSoftwareThermal | ReasonHardwareThermal |
        ReasonHardwarePowerBrake | ReasonDisplayClock;
    if ((reasons & ~known) != 0)
    {
        if (!names.empty()) stream << '|';
        stream << "Unknown0x" << std::hex << std::uppercase << (reasons & ~known);
    }
    return stream.str();
}

class Runtime final
{
public:
    Runtime(std::uint32_t deviceIndex, bool required)
    {
        try
        {
            LoadModule();
            LoadFunctions();
            CheckReturn(init_(), "nvmlInit_v2");
            initialized_ = true;
            CheckReturn(getHandleByIndex_(deviceIndex, &device_),
                "nvmlDeviceGetHandleByIndex_v2");
            std::array<char, 96> name{};
            if (getName_(device_, name.data(), static_cast<unsigned int>(name.size())) == Success)
                deviceName_ = name.data();
            std::array<char, 96> uuid{};
            if (getUuid_(device_, uuid.data(), static_cast<unsigned int>(uuid.size())) == Success)
                deviceUuid_ = uuid.data();
            available_ = true;
        }
        catch (const std::exception& error)
        {
            error_ = error.what();
            Cleanup();
            if (required) throw;
        }
    }

    ~Runtime() { Cleanup(); }
    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;

    [[nodiscard]] bool Available() const noexcept { return available_; }
    [[nodiscard]] const std::string& Error() const noexcept { return error_; }
    [[nodiscard]] const std::string& DeviceName() const noexcept { return deviceName_; }
    [[nodiscard]] const std::string& DeviceUuid() const noexcept { return deviceUuid_; }

    Snapshot Capture(
        Clock::time_point begin,
        std::uint64_t ordinal) const
    {
        Snapshot value;
        value.elapsedNanoseconds = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - begin).count());
        value.unixNanoseconds = UnixNanosecondsNow();
        value.ordinal = ordinal;
        if (!available_) return value;

        value.graphicsClockResult = getClockInfo_(device_, ClockGraphics, &value.graphicsClockMHz);
        value.smClockResult = getClockInfo_(device_, ClockSm, &value.smClockMHz);
        value.memoryClockResult = getClockInfo_(device_, ClockMemory, &value.memoryClockMHz);
        value.videoClockResult = getClockInfo_(device_, ClockVideo, &value.videoClockMHz);
        value.performanceStateResult = getPerformanceState_
            ? getPerformanceState_(device_, &value.performanceState)
            : (getPowerState_ ? getPowerState_(device_, &value.performanceState) : FunctionUnavailable);
        value.temperatureResult = getTemperature_(device_, TemperatureGpu, &value.temperatureC);
        value.powerUsageResult = getPowerUsage_(device_, &value.powerUsageMilliwatts);
        value.powerLimitResult = getPowerLimit_(device_, &value.powerLimitMilliwatts);
        Utilization utilization{};
        value.utilizationResult = getUtilization_(device_, &utilization);
        if (value.utilizationResult == Success)
        {
            value.gpuUtilizationPercent = utilization.gpu;
            value.memoryUtilizationPercent = utilization.memory;
        }
        value.clockEventReasonsResult = getEventReasons_
            ? getEventReasons_(device_, &value.clockEventReasons)
            : (getThrottleReasons_
                ? getThrottleReasons_(device_, &value.clockEventReasons)
                : FunctionUnavailable);
        return value;
    }

private:
    using InitFn = Return(*)();
    using ShutdownFn = Return(*)();
    using GetHandleByIndexFn = Return(*)(unsigned int, Device*);
    using GetNameFn = Return(*)(Device, char*, unsigned int);
    using GetUuidFn = Return(*)(Device, char*, unsigned int);
    using GetClockInfoFn = Return(*)(Device, unsigned int, unsigned int*);
    using GetPerformanceStateFn = Return(*)(Device, unsigned int*);
    using GetTemperatureFn = Return(*)(Device, unsigned int, unsigned int*);
    using GetUnsignedFn = Return(*)(Device, unsigned int*);
    using GetUtilizationFn = Return(*)(Device, Utilization*);
    using GetReasonsFn = Return(*)(Device, unsigned long long*);
    using ErrorStringFn = const char*(*)(Return);

    HMODULE module_ = nullptr;
    Device device_ = nullptr;
    bool initialized_ = false;
    bool available_ = false;
    std::string error_;
    std::string deviceName_;
    std::string deviceUuid_;

    InitFn init_ = nullptr;
    ShutdownFn shutdown_ = nullptr;
    GetHandleByIndexFn getHandleByIndex_ = nullptr;
    GetNameFn getName_ = nullptr;
    GetUuidFn getUuid_ = nullptr;
    GetClockInfoFn getClockInfo_ = nullptr;
    GetPerformanceStateFn getPerformanceState_ = nullptr;
    GetPerformanceStateFn getPowerState_ = nullptr;
    GetTemperatureFn getTemperature_ = nullptr;
    GetUnsignedFn getPowerUsage_ = nullptr;
    GetUnsignedFn getPowerLimit_ = nullptr;
    GetUtilizationFn getUtilization_ = nullptr;
    GetReasonsFn getEventReasons_ = nullptr;
    GetReasonsFn getThrottleReasons_ = nullptr;
    ErrorStringFn errorString_ = nullptr;

    template<class T>
    T Required(const char* name)
    {
        auto function = reinterpret_cast<T>(GetProcAddress(module_, name));
        if (!function) throw std::runtime_error(std::string("NVML function missing: ") + name);
        return function;
    }

    template<class T>
    T Optional(const char* name)
    {
        return reinterpret_cast<T>(GetProcAddress(module_, name));
    }

    void LoadModule()
    {
        module_ = LoadLibraryExW(L"nvml.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (module_) return;
        std::array<wchar_t, 32768> programW6432{};
        const DWORD length = GetEnvironmentVariableW(
            L"ProgramW6432", programW6432.data(), static_cast<DWORD>(programW6432.size()));
        if (length != 0 && length < static_cast<DWORD>(programW6432.size()))
        {
            std::wstring path(programW6432.data(), length);
            path += L"\\NVIDIA Corporation\\NVSMI\\nvml.dll";
            module_ = LoadLibraryW(path.c_str());
        }
        if (!module_) throw std::runtime_error("NVML DLL was not found in System32 or NVIDIA NVSMI.");
    }

    void LoadFunctions()
    {
        init_ = Required<InitFn>("nvmlInit_v2");
        shutdown_ = Required<ShutdownFn>("nvmlShutdown");
        getHandleByIndex_ = Required<GetHandleByIndexFn>("nvmlDeviceGetHandleByIndex_v2");
        getName_ = Required<GetNameFn>("nvmlDeviceGetName");
        getUuid_ = Required<GetUuidFn>("nvmlDeviceGetUUID");
        getClockInfo_ = Required<GetClockInfoFn>("nvmlDeviceGetClockInfo");
        getPerformanceState_ = Optional<GetPerformanceStateFn>("nvmlDeviceGetPerformanceState");
        getPowerState_ = Optional<GetPerformanceStateFn>("nvmlDeviceGetPowerState");
        getTemperature_ = Required<GetTemperatureFn>("nvmlDeviceGetTemperature");
        getPowerUsage_ = Required<GetUnsignedFn>("nvmlDeviceGetPowerUsage");
        getPowerLimit_ = Required<GetUnsignedFn>("nvmlDeviceGetPowerManagementLimit");
        getUtilization_ = Required<GetUtilizationFn>("nvmlDeviceGetUtilizationRates");
        getEventReasons_ = Optional<GetReasonsFn>("nvmlDeviceGetCurrentClocksEventReasons");
        getThrottleReasons_ = Optional<GetReasonsFn>("nvmlDeviceGetCurrentClocksThrottleReasons");
        errorString_ = Optional<ErrorStringFn>("nvmlErrorString");
        if (!getPerformanceState_ && !getPowerState_)
            throw std::runtime_error("NVML performance-state query is unavailable.");
        if (!getEventReasons_ && !getThrottleReasons_)
            throw std::runtime_error("NVML clock-event-reasons query is unavailable.");
    }

    void CheckReturn(Return result, std::string_view stage) const
    {
        if (result == Success) return;
        std::string message(stage);
        message += " failed with NVML code " + std::to_string(result);
        if (errorString_)
        {
            if (const char* text = errorString_(result))
                message += ": " + std::string(text);
        }
        throw std::runtime_error(message);
    }

    void Cleanup() noexcept
    {
        if (initialized_ && shutdown_) shutdown_();
        initialized_ = false;
        available_ = false;
        device_ = nullptr;
        if (module_) FreeLibrary(module_);
        module_ = nullptr;
    }
};

class Poller final
{
public:
    Poller(Runtime* runtime, Clock::time_point begin, std::uint32_t intervalMilliseconds)
        : runtime_(runtime), begin_(begin), intervalMilliseconds_(std::max(1u, intervalMilliseconds))
    {
        if (!runtime_ || !runtime_->Available()) return;
        worker_ = std::thread([this]
        {
            while (!stop_.load(std::memory_order_relaxed))
            {
                const auto snapshot = runtime_->Capture(
                    begin_, ordinal_.load(std::memory_order_relaxed));
                {
                    std::scoped_lock lock(mutex_);
                    samples_.push_back(snapshot);
                }
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(intervalMilliseconds_));
            }
        });
    }

    ~Poller() { Stop(); }
    Poller(const Poller&) = delete;
    Poller& operator=(const Poller&) = delete;

    void SetOrdinal(std::uint64_t ordinal) noexcept
    {
        ordinal_.store(ordinal, std::memory_order_relaxed);
    }

    std::vector<Snapshot> StopAndTake()
    {
        Stop();
        std::scoped_lock lock(mutex_);
        return samples_;
    }

private:
    Runtime* runtime_ = nullptr;
    Clock::time_point begin_{};
    std::uint32_t intervalMilliseconds_ = 5;
    std::atomic<bool> stop_{false};
    std::atomic<std::uint64_t> ordinal_{0};
    std::thread worker_;
    std::mutex mutex_;
    std::vector<Snapshot> samples_;

    void Stop()
    {
        stop_.store(true, std::memory_order_relaxed);
        if (worker_.joinable()) worker_.join();
    }
};
}

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
    bool stablePowerStateRequested = false;
    bool stablePowerStateApplied = false;
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

    explicit DeviceContext(
        std::uint32_t adapterIndex,
        bool requestStablePowerState = false)
        : stablePowerStateRequested(requestStablePowerState)
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
        if (stablePowerStateRequested)
        {
            const HRESULT stableResult = device->SetStablePowerState(TRUE);
            if (FAILED(stableResult))
            {
                throw std::runtime_error(
                    "SetStablePowerState(TRUE) failed. Enable Windows Developer Mode and retry: " +
                    HResultText(stableResult));
            }
            stablePowerStateApplied = true;
        }

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


namespace
{
struct EquivalentSetAuditWorkItem final
{
    std::uint64_t ordinal = 0;
    std::uint32_t round = 0;
    std::uint32_t block = 0;
    EquivalentSetAuditGroupV1 group = EquivalentSetAuditGroupV1::K1PrefixUniform;
    PatternKindV1 pattern = PatternKindV1::PrefixControl;
    std::uint32_t patternOrderIndex = 0;
    std::uint32_t patternPosition = 0;
    std::uint32_t candidateOrderIndex = 0;
    std::uint32_t candidatePosition = 0;
    CandidateKindV1 candidate = CandidateKindV1::DenseMembershipMaskPredicate;
};

[[maybe_unused]] std::string EquivalentSetAuditGroupName(EquivalentSetAuditGroupV1 value)
{
    switch (value)
    {
    case EquivalentSetAuditGroupV1::K1PrefixUniform: return "K1PrefixUniform";
    case EquivalentSetAuditGroupV1::K4096AllPatterns: return "K4096AllPatterns";
    }
    throw std::invalid_argument("Unknown Equivalent Set Audit group.");
}

void AppendEquivalentSetAuditLabel(
    std::vector<EquivalentSetAuditWorkItem>& result,
    std::uint32_t round,
    std::uint32_t block,
    EquivalentSetAuditGroupV1 group,
    PatternKindV1 pattern,
    std::uint32_t patternOrderIndex,
    std::uint32_t patternPosition,
    std::uint32_t candidateOrderIndex,
    const std::array<std::uint32_t, CanonicalCandidateCountV1>& candidateOrder,
    const std::array<CandidateKindV1, CanonicalCandidateCountV1>& kinds)
{
    for (std::uint32_t candidatePosition = 0;
         candidatePosition < CanonicalCandidateCountV1;
         ++candidatePosition)
    {
        EquivalentSetAuditWorkItem item;
        item.round = round;
        item.block = block;
        item.group = group;
        item.pattern = pattern;
        item.patternOrderIndex = patternOrderIndex;
        item.patternPosition = patternPosition;
        item.candidateOrderIndex = candidateOrderIndex;
        item.candidatePosition = candidatePosition;
        item.candidate = kinds[candidateOrder[candidatePosition]];
        result.push_back(item);
    }
}

std::vector<EquivalentSetAuditWorkItem> BuildEquivalentSetAuditSchedule(
    const EquivalentSetAuditConfigV1& config)
{
    if (config.roundCount == 0)
        throw std::invalid_argument("Equivalent Set Audit roundCount must be non-zero.");

    const auto kinds = CanonicalCandidateKindsV1();
    const auto candidateOrders = CanonicalBalancedOrdersV1();
    std::vector<EquivalentSetAuditWorkItem> result;

    if (config.schedule == EquivalentSetAuditScheduleV1::BalancedInterleaved)
    {
        static constexpr std::array<std::array<PatternKindV1, 4>, 4> latin{{
            {PatternKindV1::PrefixControl, PatternKindV1::UniformStride,
             PatternKindV1::BlockClusteredPermutation, PatternKindV1::HashScatterPermutation},
            {PatternKindV1::UniformStride, PatternKindV1::HashScatterPermutation,
             PatternKindV1::PrefixControl, PatternKindV1::BlockClusteredPermutation},
            {PatternKindV1::BlockClusteredPermutation, PatternKindV1::PrefixControl,
             PatternKindV1::HashScatterPermutation, PatternKindV1::UniformStride},
            {PatternKindV1::HashScatterPermutation, PatternKindV1::BlockClusteredPermutation,
             PatternKindV1::UniformStride, PatternKindV1::PrefixControl}}};

        for (std::uint32_t round = 0; round < config.roundCount; ++round)
        {
            for (std::uint32_t block = 0; block < 4; ++block)
            {
                const std::uint32_t k1OrderIndex = (round + block) % 2u;
                const std::array<PatternKindV1, 2> k1Order = k1OrderIndex == 0u
                    ? std::array<PatternKindV1, 2>{PatternKindV1::PrefixControl, PatternKindV1::UniformStride}
                    : std::array<PatternKindV1, 2>{PatternKindV1::UniformStride, PatternKindV1::PrefixControl};
                const std::uint32_t k4096OrderIndex = (round + block) % 4u;
                const auto& k4096Order = latin[k4096OrderIndex];

                for (std::uint32_t candidateOrderIndex = 0;
                     candidateOrderIndex < candidateOrders.size();
                     ++candidateOrderIndex)
                {
                    const auto appendK1 = [&]
                    {
                        for (std::uint32_t position = 0; position < k1Order.size(); ++position)
                            AppendEquivalentSetAuditLabel(result, round, block,
                                EquivalentSetAuditGroupV1::K1PrefixUniform,
                                k1Order[position], k1OrderIndex, position,
                                candidateOrderIndex, candidateOrders[candidateOrderIndex], kinds);
                    };
                    const auto appendK4096 = [&]
                    {
                        for (std::uint32_t position = 0; position < k4096Order.size(); ++position)
                            AppendEquivalentSetAuditLabel(result, round, block,
                                EquivalentSetAuditGroupV1::K4096AllPatterns,
                                k4096Order[position], k4096OrderIndex, position,
                                candidateOrderIndex, candidateOrders[candidateOrderIndex], kinds);
                    };

                    if (((round + block + candidateOrderIndex) & 1u) == 0u)
                    {
                        appendK1();
                        appendK4096();
                    }
                    else
                    {
                        appendK4096();
                        appendK1();
                    }
                }
            }
        }
    }
    else if (config.schedule == EquivalentSetAuditScheduleV1::FixedPatternMajor)
    {
        struct Label final
        {
            EquivalentSetAuditGroupV1 group;
            PatternKindV1 pattern;
            std::uint32_t position;
        };
        static constexpr std::array<Label, 6> labels{{
            {EquivalentSetAuditGroupV1::K1PrefixUniform, PatternKindV1::PrefixControl, 0u},
            {EquivalentSetAuditGroupV1::K1PrefixUniform, PatternKindV1::UniformStride, 1u},
            {EquivalentSetAuditGroupV1::K4096AllPatterns, PatternKindV1::PrefixControl, 0u},
            {EquivalentSetAuditGroupV1::K4096AllPatterns, PatternKindV1::UniformStride, 1u},
            {EquivalentSetAuditGroupV1::K4096AllPatterns, PatternKindV1::BlockClusteredPermutation, 2u},
            {EquivalentSetAuditGroupV1::K4096AllPatterns, PatternKindV1::HashScatterPermutation, 3u}}};

        for (std::uint32_t round = 0; round < config.roundCount; ++round)
        {
            for (std::uint32_t labelIndex = 0; labelIndex < labels.size(); ++labelIndex)
            {
                const auto& label = labels[labelIndex];
                for (std::uint32_t candidateOrderIndex = 0;
                     candidateOrderIndex < candidateOrders.size();
                     ++candidateOrderIndex)
                {
                    AppendEquivalentSetAuditLabel(result, round, labelIndex,
                        label.group, label.pattern, labelIndex, label.position,
                        candidateOrderIndex, candidateOrders[candidateOrderIndex], kinds);
                }
            }
        }
    }
    else
    {
        throw std::invalid_argument("Unknown Equivalent Set Audit schedule.");
    }

    for (std::size_t index = 0; index < result.size(); ++index)
        result[index].ordinal = static_cast<std::uint64_t>(index);
    return result;
}

void RequireExactSingletonZero(const semantic::ExactSparseWorkSetV1& set, std::string_view label)
{
    if (set.Indices() != std::vector<std::uint32_t>{0u})
        throw std::runtime_error(std::string(label) + " must be the exact set {0}.");
}

void RequireFullUniverse(const semantic::ExactSparseWorkSetV1& set, std::string_view label)
{
    if (set.Indices().size() != semantic::WorkUniverseCountV1)
        throw std::runtime_error(std::string(label) + " must contain all 4096 indices.");
    for (std::uint32_t index = 0; index < semantic::WorkUniverseCountV1; ++index)
        if (set.Indices()[index] != index)
            throw std::runtime_error(std::string(label) + " is not the canonical full universe.");
}

[[maybe_unused]] base::Digest256 TextIdentity(std::string_view text)
{
    return base::Sha256(std::as_bytes(std::span<const char>(text.data(), text.size())));
}

[[maybe_unused]] void WriteAuditTextFile(const std::filesystem::path& path, std::string_view text)
{
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) throw std::runtime_error("Failed to create audit artifact: " + path.string());
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!stream) throw std::runtime_error("Failed to write audit artifact: " + path.string());
}

#if defined(_WIN32)
struct EquivalentSetAuditTelemetry final
{
    std::uint64_t timestampFrequency = 0;
    std::uint64_t gpuCalibration = 0;
    std::uint64_t cpuCalibration = 0;
    std::uint64_t localBudget = 0;
    std::uint64_t localUsage = 0;
    std::uint64_t nonLocalBudget = 0;
    std::uint64_t nonLocalUsage = 0;
};

EquivalentSetAuditTelemetry CaptureEquivalentSetAuditTelemetry(DeviceContext& gpu)
{
    EquivalentSetAuditTelemetry value;
    UINT64 frequency = 0;
    if (SUCCEEDED(gpu.queue->GetTimestampFrequency(&frequency)))
        value.timestampFrequency = frequency;
    UINT64 gpuTimestamp = 0;
    UINT64 cpuTimestamp = 0;
    if (SUCCEEDED(gpu.queue->GetClockCalibration(&gpuTimestamp, &cpuTimestamp)))
    {
        value.gpuCalibration = gpuTimestamp;
        value.cpuCalibration = cpuTimestamp;
    }
    ComPtr<IDXGIAdapter3> adapter3;
    if (SUCCEEDED(gpu.adapter.As(&adapter3)))
    {
        DXGI_QUERY_VIDEO_MEMORY_INFO info{};
        if (SUCCEEDED(adapter3->QueryVideoMemoryInfo(
            0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info)))
        {
            value.localBudget = info.Budget;
            value.localUsage = info.CurrentUsage;
        }
        info = {};
        if (SUCCEEDED(adapter3->QueryVideoMemoryInfo(
            0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &info)))
        {
            value.nonLocalBudget = info.Budget;
            value.nonLocalUsage = info.CurrentUsage;
        }
    }
    return value;
}

struct EquivalentSetAuditRecordedSample final
{
    EquivalentSetAuditWorkItem work;
    TimingSampleV1 timing;
    std::uint64_t elapsedBeforeNanoseconds = 0;
    std::uint64_t elapsedAfterNanoseconds = 0;
    EquivalentSetAuditTelemetry before;
    EquivalentSetAuditTelemetry after;
    base::Digest256 sparseSetIdentity{};
    base::Digest256 artifactIdentity{};
    base::Digest256 representationResourceIdentity{};
    base::Digest256 frozenBindingIdentity{};
};
#endif
}

std::string EquivalentSetAuditScheduleNameV1(EquivalentSetAuditScheduleV1 value)
{
    switch (value)
    {
    case EquivalentSetAuditScheduleV1::BalancedInterleaved: return "BalancedInterleaved";
    case EquivalentSetAuditScheduleV1::FixedPatternMajor: return "FixedPatternMajor";
    }
    throw std::invalid_argument("Unknown Equivalent Set Audit schedule.");
}

base::Result<void, std::string> RunEquivalentSetAuditScheduleSelfTestV1()
{
    try
    {
        const auto semanticValue = semantic::BuildCanonicalSparsePointTransformSemanticV1();
        auto prefix1 = semantic::BuildCanonicalSparseWorkSetV1(
            PatternKindV1::PrefixControl, semantic::SparseCardinalityV1(1));
        auto uniform1 = semantic::BuildCanonicalSparseWorkSetV1(
            PatternKindV1::UniformStride, semantic::SparseCardinalityV1(1));
        if (!prefix1 || !uniform1) throw std::runtime_error("Failed to construct K=1 audit sets.");
        RequireExactSingletonZero(prefix1.Value(), "PrefixControl K=1");
        RequireExactSingletonZero(uniform1.Value(), "UniformStride K=1");
        if (prefix1.Value().Identity() != uniform1.Value().Identity())
            throw std::runtime_error("PrefixControl K=1 and UniformStride K=1 identities differ.");

        std::vector<semantic::ExactSparseWorkSetV1> fullSets;
        for (PatternKindV1 pattern : CanonicalPatternsV1())
        {
            auto set = semantic::BuildCanonicalSparseWorkSetV1(
                pattern, semantic::SparseCardinalityV1(semantic::WorkUniverseCountV1));
            if (!set) throw std::runtime_error("Failed to construct K=4096 audit set.");
            RequireFullUniverse(set.Value(), PatternNameV1(pattern) + " K=4096");
            fullSets.push_back(std::move(set).Value());
        }
        for (const auto& set : fullSets)
            if (set.Identity() != fullSets.front().Identity())
                throw std::runtime_error("K=4096 Pattern identities differ.");

        for (CandidateKindV1 kind : CanonicalCandidateKindsV1())
        {
            const auto a = fc::BuildSparseFamilyArtifactV1(semanticValue, prefix1.Value(), kind);
            const auto b = fc::BuildSparseFamilyArtifactV1(semanticValue, uniform1.Value(), kind);
            if (a.Bytes() != b.Bytes() || a.ArtifactIdentity() != b.ArtifactIdentity())
                throw std::runtime_error("K=1 equivalent Pattern artifacts differ.");
            const auto full = fc::BuildSparseFamilyArtifactV1(semanticValue, fullSets.front(), kind);
            for (std::size_t index = 1; index < fullSets.size(); ++index)
            {
                const auto other = fc::BuildSparseFamilyArtifactV1(semanticValue, fullSets[index], kind);
                if (full.Bytes() != other.Bytes() || full.ArtifactIdentity() != other.ArtifactIdentity())
                    throw std::runtime_error("K=4096 equivalent Pattern artifacts differ.");
            }
        }

        EquivalentSetAuditConfigV1 balanced;
        balanced.schedule = EquivalentSetAuditScheduleV1::BalancedInterleaved;
        balanced.roundCount = 1;
        const auto balancedItems = BuildEquivalentSetAuditSchedule(balanced);
        EquivalentSetAuditConfigV1 fixed;
        fixed.schedule = EquivalentSetAuditScheduleV1::FixedPatternMajor;
        fixed.roundCount = 4;
        const auto fixedItems = BuildEquivalentSetAuditSchedule(fixed);
        if (balancedItems.size() != 432u || fixedItems.size() != 432u)
            throw std::runtime_error("Equivalent Set Audit canonical schedules must contain 432 samples.");

        const auto validateCounts = [](const std::vector<EquivalentSetAuditWorkItem>& items)
        {
            std::map<std::tuple<std::uint32_t, std::uint32_t, std::uint32_t>, std::uint32_t> counts;
            std::map<std::tuple<std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t>, std::uint32_t> positions;
            for (const auto& item : items)
            {
                ++counts[{static_cast<std::uint32_t>(item.group),
                    static_cast<std::uint32_t>(item.pattern),
                    static_cast<std::uint32_t>(item.candidate)}];
                ++positions[{static_cast<std::uint32_t>(item.group),
                    static_cast<std::uint32_t>(item.pattern),
                    static_cast<std::uint32_t>(item.candidate),
                    item.candidatePosition}];
            }
            for (const auto& [key, count] : counts)
                if (count != 24u) throw std::runtime_error("Each equivalent label/Candidate must have 24 samples.");
            for (const auto& [key, count] : positions)
                if (count != 8u) throw std::runtime_error("Each Candidate must appear eight times in each Candidate position.");
        };
        validateCounts(balancedItems);
        validateCounts(fixedItems);
        return base::Result<void, std::string>::Success();
    }
    catch (const std::exception& error)
    {
        return base::Result<void, std::string>::Failure(error.what());
    }
}

base::Result<EquivalentSetAuditRunSummaryV1, std::string>
RunEquivalentSetAuditV1(
    const EquivalentSetAuditConfigV1& config,
    const std::filesystem::path& outputDirectory)
{
#if !defined(_WIN32)
    (void)config;
    (void)outputDirectory;
    return base::Result<EquivalentSetAuditRunSummaryV1, std::string>::Failure(
        "Spiral 6 Equivalent Set Audit requires Windows D3D12.");
#else
    try
    {
        auto selfTest = RunEquivalentSetAuditScheduleSelfTestV1();
        if (!selfTest) throw std::runtime_error(selfTest.Error());
        const auto schedule = BuildEquivalentSetAuditSchedule(config);
        std::filesystem::create_directories(outputDirectory);

        DeviceContext gpu(config.adapterIndex);
        const auto semanticValue = semantic::BuildCanonicalSparsePointTransformSemanticV1();
        const auto context = fv::BuildCanonicalSparseFamilyVerificationContextV1();

        auto prefix1Result = semantic::BuildCanonicalSparseWorkSetV1(
            PatternKindV1::PrefixControl, semantic::SparseCardinalityV1(1));
        auto uniform1Result = semantic::BuildCanonicalSparseWorkSetV1(
            PatternKindV1::UniformStride, semantic::SparseCardinalityV1(1));
        if (!prefix1Result || !uniform1Result)
            throw std::runtime_error("Failed to construct K=1 Equivalent Set Audit inputs.");
        const auto& k1Set = prefix1Result.Value();
        RequireExactSingletonZero(k1Set, "PrefixControl K=1");
        RequireExactSingletonZero(uniform1Result.Value(), "UniformStride K=1");
        if (k1Set.Identity() != uniform1Result.Value().Identity())
            throw std::runtime_error("K=1 equivalent labels do not share one Exact Set identity.");

        std::vector<semantic::ExactSparseWorkSetV1> fullSets;
        for (PatternKindV1 pattern : CanonicalPatternsV1())
        {
            auto set = semantic::BuildCanonicalSparseWorkSetV1(
                pattern, semantic::SparseCardinalityV1(semantic::WorkUniverseCountV1));
            if (!set) throw std::runtime_error("Failed to construct K=4096 Equivalent Set Audit input.");
            RequireFullUniverse(set.Value(), PatternNameV1(pattern) + " K=4096");
            fullSets.push_back(std::move(set).Value());
        }
        const auto& k4096Set = fullSets.front();
        for (const auto& set : fullSets)
            if (set.Identity() != k4096Set.Identity())
                throw std::runtime_error("K=4096 equivalent labels do not share one Exact Set identity.");

        const auto k1Authorities = BuildAuthorityBundles(semanticValue, k1Set, context);
        const auto k4096Authorities = BuildAuthorityBundles(semanticValue, k4096Set, context);
        const auto kinds = CanonicalCandidateKindsV1();
        const auto findAuthority = [&](EquivalentSetAuditGroupV1 group, CandidateKindV1 kind)
            -> const CandidateAuthorityBundle&
        {
            const auto& authorities = group == EquivalentSetAuditGroupV1::K1PrefixUniform
                ? k1Authorities : k4096Authorities;
            const auto found = std::find_if(authorities.begin(), authorities.end(),
                [&](const CandidateAuthorityBundle& value) { return value.raw.kind == kind; });
            if (found == authorities.end()) throw std::runtime_error("Audit Candidate authority is missing.");
            return *found;
        };

        std::vector<EquivalentSetAuditRecordedSample> records;
        records.reserve(schedule.size());
        const auto auditBegin = Clock::now();
        for (const auto& work : schedule)
        {
            const auto& set = work.group == EquivalentSetAuditGroupV1::K1PrefixUniform
                ? k1Set : k4096Set;
            const auto& authority = findAuthority(work.group, work.candidate);
            EquivalentSetAuditRecordedSample record;
            record.work = work;
            record.sparseSetIdentity = set.Identity();
            record.artifactIdentity = authority.artifact.ArtifactIdentity();
            record.representationResourceIdentity =
                authority.verified.Plan().operation.representationResourceIdentity;
            record.frozenBindingIdentity = authority.frozen.BindingIdentity();
            record.elapsedBeforeNanoseconds = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - auditBegin).count());
            record.before = CaptureEquivalentSetAuditTelemetry(gpu);
            record.timing = ExecuteMeasuredCandidate(
                gpu, semanticValue, work.pattern, set, context, authority,
                work.round, work.candidateOrderIndex, work.block, work.candidatePosition);
            record.after = CaptureEquivalentSetAuditTelemetry(gpu);
            record.elapsedAfterNanoseconds = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - auditBegin).count());
            if (record.before.timestampFrequency != 0 &&
                record.after.timestampFrequency != 0 &&
                record.before.timestampFrequency != record.after.timestampFrequency)
                throw std::runtime_error("D3D12 timestamp frequency changed inside one audit sample.");
            records.push_back(std::move(record));
        }

        for (EquivalentSetAuditGroupV1 group : {
            EquivalentSetAuditGroupV1::K1PrefixUniform,
            EquivalentSetAuditGroupV1::K4096AllPatterns})
        {
            for (CandidateKindV1 kind : kinds)
            {
                std::vector<const EquivalentSetAuditRecordedSample*> matching;
                for (const auto& record : records)
                    if (record.work.group == group && record.work.candidate == kind)
                        matching.push_back(&record);
                if (matching.empty()) throw std::runtime_error("Equivalent Set Audit sample group is empty.");
                const auto output = matching.front()->timing.outputDigest;
                const auto setIdentity = matching.front()->sparseSetIdentity;
                const auto artifact = matching.front()->artifactIdentity;
                const auto resource = matching.front()->representationResourceIdentity;
                const auto dispatch = matching.front()->timing.expectedDispatchGroupsX;
                for (const auto* record : matching)
                {
                    if (record->timing.outputDigest != output ||
                        record->sparseSetIdentity != setIdentity ||
                        record->artifactIdentity != artifact ||
                        record->representationResourceIdentity != resource ||
                        record->timing.expectedDispatchGroupsX != dispatch ||
                        record->timing.observedDispatchGroupsX != dispatch)
                        throw std::runtime_error("Equivalent label structural identity changed across audit samples.");
                }
            }
        }

        std::ostringstream csv;
        csv << "schedule,ordinal,round,block,group,pattern,patternOrderIndex,patternPosition,"
            << "candidateOrderIndex,candidatePosition,candidate,K,activeBlockCount,setIdentity,"
            << "artifactIdentity,representationResourceIdentity,frozenBindingIdentity,outputDigest,"
            << "elapsedBeforeNs,elapsedAfterNs,timestampFrequencyBefore,timestampFrequencyAfter,"
            << "gpuCalibrationBefore,gpuCalibrationAfter,cpuCalibrationBefore,cpuCalibrationAfter,"
            << "localBudgetBefore,localBudgetAfter,localUsageBefore,localUsageAfter,"
            << "nonLocalBudgetBefore,nonLocalBudgetAfter,nonLocalUsageBefore,nonLocalUsageAfter,"
            << "gpuSentinelInitializationNs,gpuIndirectArgumentGenerationNs,"
            << "gpuSparseConsumerExecutionNs,gpuCompletionNs,gpuObservationCopyNs,"
            << "gpuCandidatePathNs,gpuTotalNs,submitAndWaitNs,cpuEndToEndNs,"
            << "expectedDispatchGroupsX,observedDispatchGroupsX\n";
        csv << std::setprecision(17);
        for (const auto& record : records)
        {
            const auto& t = record.timing;
            csv << EquivalentSetAuditScheduleNameV1(config.schedule) << ','
                << record.work.ordinal << ',' << record.work.round << ',' << record.work.block << ','
                << EquivalentSetAuditGroupName(record.work.group) << ','
                << PatternNameV1(record.work.pattern) << ','
                << record.work.patternOrderIndex << ',' << record.work.patternPosition << ','
                << record.work.candidateOrderIndex << ',' << record.work.candidatePosition << ','
                << CandidateLetterV1(record.work.candidate) << ',' << t.activeCount << ','
                << t.activeBlockCount << ',' << base::ToHex(record.sparseSetIdentity) << ','
                << base::ToHex(record.artifactIdentity) << ','
                << base::ToHex(record.representationResourceIdentity) << ','
                << base::ToHex(record.frozenBindingIdentity) << ','
                << base::ToHex(t.outputDigest) << ','
                << record.elapsedBeforeNanoseconds << ',' << record.elapsedAfterNanoseconds << ','
                << record.before.timestampFrequency << ',' << record.after.timestampFrequency << ','
                << record.before.gpuCalibration << ',' << record.after.gpuCalibration << ','
                << record.before.cpuCalibration << ',' << record.after.cpuCalibration << ','
                << record.before.localBudget << ',' << record.after.localBudget << ','
                << record.before.localUsage << ',' << record.after.localUsage << ','
                << record.before.nonLocalBudget << ',' << record.after.nonLocalBudget << ','
                << record.before.nonLocalUsage << ',' << record.after.nonLocalUsage << ','
                << t.gpuSentinelInitializationNanoseconds << ','
                << t.gpuIndirectArgumentGenerationNanoseconds << ','
                << t.gpuSparseConsumerExecutionNanoseconds << ','
                << t.gpuCompletionNanoseconds << ','
                << t.gpuObservationCopyNanoseconds << ','
                << t.gpuCandidatePathNanoseconds << ',' << t.gpuTotalNanoseconds << ','
                << t.submitAndWaitNanoseconds << ',' << t.cpuEndToEndNanoseconds << ','
                << t.expectedDispatchGroupsX << ',' << t.observedDispatchGroupsX << '\n';
        }
        const std::string csvText = csv.str();
        const auto csvIdentity = TextIdentity(csvText);
        WriteAuditTextFile(outputDirectory / "spiral6_equivalent_set_audit_samples_v1.csv", csvText);

        std::ostringstream report;
        report << "# Spiral 6 CU6 Equivalent Set Audit Run\n\n";
        report << "- Schedule: `" << EquivalentSetAuditScheduleNameV1(config.schedule) << "`\n";
        report << "- Adapter: `" << gpu.profile.description << "`\n";
        report << "- Samples: " << records.size() << "\n";
        report << "- K=1 Exact Set: `{0}`\n";
        report << "- K=1 Set identity: `" << base::ToHex(k1Set.Identity()) << "`\n";
        report << "- K=4096 Exact Set: full universe `[0,4095]`\n";
        report << "- K=4096 Set identity: `" << base::ToHex(k4096Set.Identity()) << "`\n";
        report << "- CSV SHA-256: `" << base::ToHex(csvIdentity) << "`\n\n";
        report << "Structural gate: for every equivalent label and Candidate, Set identity, Artifact identity, "
               "Representation Resource identity, Dispatch and Output digest remained identical.\n\n";
        report << "Timing interpretation is intentionally deferred to the combined audit analyzer, which compares "
               "two fresh balanced runs, one fresh fixed Pattern-major run, and the original CU6 CSV.\n";
        const std::string reportText = report.str();
        const auto reportIdentity = TextIdentity(reportText);
        WriteAuditTextFile(outputDirectory / "SPIRAL6_EQUIVALENT_SET_AUDIT_RUN.md", reportText);

        EquivalentSetAuditRunSummaryV1 summary;
        summary.schedule = config.schedule;
        summary.adapterDescription = gpu.profile.description;
        summary.sampleCount = static_cast<std::uint32_t>(records.size());
        summary.k1SparseSetIdentity = k1Set.Identity();
        summary.k4096SparseSetIdentity = k4096Set.Identity();
        summary.csvIdentity = csvIdentity;
        summary.runReportIdentity = reportIdentity;
        return base::Result<EquivalentSetAuditRunSummaryV1, std::string>::Success(std::move(summary));
    }
    catch (const std::exception& error)
    {
        return base::Result<EquivalentSetAuditRunSummaryV1, std::string>::Failure(error.what());
    }
#endif
}
namespace
{
struct TransitionProbeCheckpoint final
{
    std::uint64_t elapsedNanoseconds = 0;
#if defined(_WIN32)
    EquivalentSetAuditTelemetry telemetry{};
    nvml_dynamic::Snapshot nvml{};
#endif
};

struct TransitionCalibrationTiming final
{
    double uploadToDefaultNanoseconds = 0.0;
    double defaultToReadbackNanoseconds = 0.0;
    double totalNanoseconds = 0.0;
};

struct TransitionProbeRecordedSample final
{
    std::uint64_t ordinal = 0;
    CandidateKindV1 candidate = CandidateKindV1::DenseMembershipMaskPredicate;
    base::Digest256 sparseSetIdentity{};
    base::Digest256 artifactIdentity{};
    base::Digest256 representationResourceIdentity{};
    base::Digest256 frozenBindingIdentity{};
    base::Digest256 outputDigest{};

    TransitionProbeCheckpoint sampleBegin{};
    TransitionProbeCheckpoint afterValidation{};
    TransitionProbeCheckpoint afterArtifactRebuild{};
    TransitionProbeCheckpoint afterResources{};
    TransitionProbeCheckpoint afterCommandRecording{};
    TransitionProbeCheckpoint beforeMainSubmit{};
    TransitionProbeCheckpoint afterExecuteCall{};
    TransitionProbeCheckpoint afterSignal{};
    TransitionProbeCheckpoint afterFence{};
    TransitionProbeCheckpoint afterReadback{};
    TransitionProbeCheckpoint sampleEnd{};

    TransitionCalibrationTiming preResourceCalibration{};
    TransitionCalibrationTiming postResourceCalibration{};
    TransitionCalibrationTiming preSubmitCalibration{};
    TransitionCalibrationTiming postMainCalibration{};

    double validationNanoseconds = 0.0;
    double artifactRebuildNanoseconds = 0.0;
    double resourceCreationUploadNanoseconds = 0.0;
    double commandRecordingNanoseconds = 0.0;
    double closeNanoseconds = 0.0;
    double executeCallNanoseconds = 0.0;
    double signalNanoseconds = 0.0;
    double fenceWaitNanoseconds = 0.0;
    double readbackNanoseconds = 0.0;
    double numericValidationNanoseconds = 0.0;
    double cpuEndToEndNanoseconds = 0.0;

    double gpuSentinelCopyNanoseconds = 0.0;
    double gpuOutputTransitionNanoseconds = 0.0;
    double gpuArgumentDispatchNanoseconds = 0.0;
    double gpuArgumentFinalizeNanoseconds = 0.0;
    double gpuSparseConsumerNanoseconds = 0.0;
    double gpuOutputUavBarrierNanoseconds = 0.0;
    double gpuCopyTransitionNanoseconds = 0.0;
    double gpuOutputReadbackCopyNanoseconds = 0.0;
    double gpuArgumentReadbackCopyNanoseconds = 0.0;
    double gpuSentinelPathNanoseconds = 0.0;
    double gpuMainTotalNanoseconds = 0.0;

    std::uint32_t expectedDispatchGroupsX = 0;
    std::uint32_t observedDispatchGroupsX = 0;
};

std::array<CandidateKindV1, 3> TransitionProbeCandidateSequence()
{
    return {
        CandidateKindV1::DenseMembershipMaskPredicate,
        CandidateKindV1::ActiveBlockListLocalMask,
        CandidateKindV1::CompactIndexListDispatch};
}

[[maybe_unused]] double MedianCopy(std::vector<double> values)
{
    if (values.empty()) return 0.0;
    std::sort(values.begin(), values.end());
    const std::size_t middle = values.size() / 2u;
    if ((values.size() & 1u) != 0u) return values[middle];
    return (values[middle - 1u] + values[middle]) * 0.5;
}

struct SyntheticTransitionPoint final
{
    double previousPost = 1.0;
    double preResource = 1.0;
    double postResource = 1.0;
    double preSubmit = 1.0;
    double mainSentinel = 1.0;
    double postMain = 1.0;
};

TransitionProbeWindowV1 ClassifySyntheticTransition(
    const SyntheticTransitionPoint& point,
    double threshold)
{
    const auto slow = [threshold](double value) { return value >= threshold; };
    if (!slow(point.previousPost) && slow(point.preResource))
        return TransitionProbeWindowV1::BetweenSamples;
    if (!slow(point.preResource) && slow(point.postResource))
        return TransitionProbeWindowV1::ValidationOrResourceCreation;
    if (!slow(point.postResource) && slow(point.preSubmit))
        return TransitionProbeWindowV1::CommandRecording;
    if (!slow(point.preSubmit) && slow(point.mainSentinel))
        return TransitionProbeWindowV1::SubmitOrBeforeMainGpu;
    if (!slow(point.mainSentinel) && slow(point.postMain))
        return TransitionProbeWindowV1::AfterMainGpu;
    if (slow(point.mainSentinel))
        return TransitionProbeWindowV1::DuringMainGpu;
    return TransitionProbeWindowV1::NotObserved;
}

#if defined(_WIN32)
struct TransitionProbeCalibrationContext final
{
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> commandList;
    ComPtr<ID3D12QueryHeap> queryHeap;
    ComPtr<ID3D12Resource> queryReadback;
    ComPtr<ID3D12Resource> defaultBuffer;
    ComPtr<ID3D12Resource> readbackBuffer;

    explicit TransitionProbeCalibrationContext(DeviceContext& gpu)
    {
        Check(gpu.device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&allocator)),
            "CreateCommandAllocator(transition calibration)");
        Check(gpu.device->CreateCommandList(
            0, D3D12_COMMAND_LIST_TYPE_COMPUTE, allocator.Get(), nullptr,
            IID_PPV_ARGS(&commandList)),
            "CreateCommandList(transition calibration)");
        Check(commandList->Close(), "Close(initial transition calibration list)");
        D3D12_QUERY_HEAP_DESC description{};
        description.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
        description.Count = 3;
        Check(gpu.device->CreateQueryHeap(
            &description, IID_PPV_ARGS(&queryHeap)),
            "CreateQueryHeap(transition calibration)");
        queryReadback = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_READBACK,
            sizeof(std::uint64_t) * 3u, D3D12_RESOURCE_STATE_COPY_DEST);
        defaultBuffer = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_DEFAULT,
            gpu.zeroBytes.size(), D3D12_RESOURCE_STATE_COPY_DEST);
        readbackBuffer = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_READBACK,
            gpu.zeroBytes.size(), D3D12_RESOURCE_STATE_COPY_DEST);
    }
};

TransitionProbeCheckpoint CaptureTransitionCheckpoint(
    DeviceContext& gpu,
    Clock::time_point begin,
    bool captureTelemetry,
    nvml_dynamic::Runtime* nvml,
    std::uint64_t ordinal)
{
    TransitionProbeCheckpoint value;
    value.elapsedNanoseconds = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - begin).count());
    if (captureTelemetry)
    {
        value.telemetry = CaptureEquivalentSetAuditTelemetry(gpu);
        if (nvml && nvml->Available()) value.nvml = nvml->Capture(begin, ordinal);
    }
    return value;
}

TransitionCalibrationTiming RunTransitionCalibration(
    DeviceContext& gpu,
    TransitionProbeCalibrationContext& calibration)
{
    Check(calibration.allocator->Reset(), "Reset transition calibration allocator");
    Check(calibration.commandList->Reset(calibration.allocator.Get(), nullptr),
        "Reset transition calibration command list");
    calibration.commandList->EndQuery(
        calibration.queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0);
    calibration.commandList->CopyBufferRegion(
        calibration.defaultBuffer.Get(), 0, gpu.zeroUpload.Get(), 0, gpu.zeroBytes.size());
    auto toSource = Transition(calibration.defaultBuffer.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);
    calibration.commandList->ResourceBarrier(1, &toSource);
    calibration.commandList->EndQuery(
        calibration.queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 1);
    calibration.commandList->CopyBufferRegion(
        calibration.readbackBuffer.Get(), 0, calibration.defaultBuffer.Get(), 0,
        gpu.zeroBytes.size());
    auto toDestination = Transition(calibration.defaultBuffer.Get(),
        D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
    calibration.commandList->ResourceBarrier(1, &toDestination);
    calibration.commandList->EndQuery(
        calibration.queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 2);
    calibration.commandList->ResolveQueryData(
        calibration.queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP,
        0, 3, calibration.queryReadback.Get(), 0);
    Check(calibration.commandList->Close(), "Close transition calibration command list");
    ID3D12CommandList* lists[]{calibration.commandList.Get()};
    gpu.queue->ExecuteCommandLists(1, lists);
    const std::uint64_t fenceValue = ++gpu.fenceValue;
    Check(gpu.queue->Signal(gpu.fence.Get(), fenceValue),
        "Signal transition calibration fence");
    if (gpu.fence->GetCompletedValue() < fenceValue)
    {
        Check(gpu.fence->SetEventOnCompletion(fenceValue, gpu.fenceEvent.value),
            "SetEventOnCompletion(transition calibration)");
        WaitForSingleObject(gpu.fenceEvent.value, INFINITE);
    }
    const auto bytes = ReadbackBytes(
        calibration.queryReadback.Get(), sizeof(std::uint64_t) * 3u);
    std::array<std::uint64_t, 3> timestamps{};
    std::memcpy(timestamps.data(), bytes.data(), bytes.size());
    const double factor = 1.0e9 / static_cast<double>(gpu.timestampFrequency);
    TransitionCalibrationTiming result;
    result.uploadToDefaultNanoseconds =
        static_cast<double>(timestamps[1] - timestamps[0]) * factor;
    result.defaultToReadbackNanoseconds =
        static_cast<double>(timestamps[2] - timestamps[1]) * factor;
    result.totalNanoseconds =
        static_cast<double>(timestamps[2] - timestamps[0]) * factor;
    return result;
}

struct TransitionProbeMainQuery final
{
    ComPtr<ID3D12QueryHeap> heap;
    ComPtr<ID3D12Resource> readback;

    explicit TransitionProbeMainQuery(DeviceContext& gpu)
    {
        D3D12_QUERY_HEAP_DESC description{};
        description.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
        description.Count = 10;
        Check(gpu.device->CreateQueryHeap(&description, IID_PPV_ARGS(&heap)),
            "CreateQueryHeap(transition main)");
        readback = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_READBACK,
            sizeof(std::uint64_t) * 10u, D3D12_RESOURCE_STATE_COPY_DEST);
    }
};

TransitionProbeRecordedSample ExecuteTransitionProbeSample(
    DeviceContext& gpu,
    TransitionProbeCalibrationContext* calibration,
    TransitionProbeMainQuery& mainQuery,
    const semantic::SparsePointTransformSemanticV1& semanticValue,
    const semantic::ExactSparseWorkSetV1& set,
    const fv::SparseFamilyVerificationContextV1& context,
    const CandidateAuthorityBundle& authority,
    std::uint64_t ordinal,
    Clock::time_point probeBegin,
    nvml_dynamic::Runtime* nvml)
{
    TransitionProbeRecordedSample sample;
    sample.ordinal = ordinal;
    sample.candidate = authority.raw.kind;
    sample.sparseSetIdentity = set.Identity();
    sample.artifactIdentity = authority.artifact.ArtifactIdentity();
    sample.representationResourceIdentity =
        authority.verified.Plan().operation.representationResourceIdentity;
    sample.frozenBindingIdentity = authority.frozen.BindingIdentity();
    const auto cpuBegin = Clock::now();
    sample.sampleBegin = CaptureTransitionCheckpoint(gpu, probeBegin, true, nvml, ordinal);
    if (calibration)
        sample.preResourceCalibration = RunTransitionCalibration(gpu, *calibration);

    const auto validationBegin = Clock::now();
    auto valid = fe::ValidateFrozenVerifiedSparseFamilyCandidateV1(
        authority.frozen, semanticValue, set, context, 1);
    if (!valid) throw std::runtime_error(valid.Error());
    auto artifactValid = fc::ValidateSparseFamilyArtifactV1(
        authority.artifact, semanticValue, set);
    if (!artifactValid) throw std::runtime_error(artifactValid.Error());
    const auto validationEnd = Clock::now();
    sample.validationNanoseconds = Nanoseconds(validationEnd - validationBegin);
    sample.afterValidation = CaptureTransitionCheckpoint(gpu, probeBegin, false, nvml, ordinal);

    const auto artifactBegin = Clock::now();
    const auto rebuilt = fc::BuildSparseFamilyArtifactV1(
        semanticValue, set, authority.raw.kind);
    if (rebuilt.Bytes() != authority.artifact.Bytes())
        throw std::runtime_error("Transition Probe representation rematerialization differs.");
    const auto artifactEnd = Clock::now();
    sample.artifactRebuildNanoseconds = Nanoseconds(artifactEnd - artifactBegin);
    sample.afterArtifactRebuild = CaptureTransitionCheckpoint(gpu, probeBegin, false, nvml, ordinal);

    const auto resourceBegin = Clock::now();
    auto representationUpload = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_UPLOAD,
        rebuilt.PayloadBytes().size(), D3D12_RESOURCE_STATE_GENERIC_READ);
    UploadBytes(representationUpload.Get(), rebuilt.PayloadBytes());
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
    const auto resourceEnd = Clock::now();
    sample.resourceCreationUploadNanoseconds = Nanoseconds(resourceEnd - resourceBegin);
    sample.afterResources = CaptureTransitionCheckpoint(gpu, probeBegin, true, nvml, ordinal);
    if (calibration)
        sample.postResourceCalibration = RunTransitionCalibration(gpu, *calibration);

    const auto recordingBegin = Clock::now();
    gpu.Begin();
    gpu.commandList->EndQuery(mainQuery.heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0);
    gpu.commandList->CopyBufferRegion(
        output.Get(), 0, gpu.zeroUpload.Get(), 0, gpu.zeroBytes.size());
    gpu.commandList->EndQuery(mainQuery.heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 1);
    auto outputToUav = Transition(output.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    gpu.commandList->ResourceBarrier(1, &outputToUav);
    gpu.commandList->EndQuery(mainQuery.heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 2);

    gpu.commandList->SetPipelineState(gpu.argumentPipeline.Get());
    gpu.commandList->SetComputeRootSignature(gpu.argumentRoot.Get());
    gpu.commandList->SetComputeRoot32BitConstant(
        0, authority.artifact.DispatchGroupsX(), 0);
    gpu.commandList->SetComputeRootUnorderedAccessView(
        1, argumentBuffer->GetGPUVirtualAddress());
    gpu.commandList->Dispatch(1, 1, 1);
    gpu.commandList->EndQuery(mainQuery.heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 3);
    auto argumentUav = UavBarrier(argumentBuffer.Get());
    gpu.commandList->ResourceBarrier(1, &argumentUav);
    auto argumentToIndirect = Transition(argumentBuffer.Get(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
    gpu.commandList->ResourceBarrier(1, &argumentToIndirect);
    gpu.commandList->EndQuery(mainQuery.heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 4);

    gpu.commandList->SetPipelineState(
        gpu.candidatePipelines.at(authority.raw.kind).Get());
    gpu.commandList->SetComputeRootSignature(gpu.candidateRoot.Get());
    gpu.commandList->SetComputeRoot32BitConstant(
        0, authority.artifact.ActiveCount(), 0);
    gpu.commandList->SetComputeRootShaderResourceView(
        1, representationUpload->GetGPUVirtualAddress());
    gpu.commandList->SetComputeRootShaderResourceView(
        2, gpu.motorUpload->GetGPUVirtualAddress());
    gpu.commandList->SetComputeRootShaderResourceView(
        3, gpu.pointUpload->GetGPUVirtualAddress());
    gpu.commandList->SetComputeRootUnorderedAccessView(
        4, output->GetGPUVirtualAddress());
    gpu.commandList->ExecuteIndirect(
        gpu.dispatchSignature.Get(), 1, argumentBuffer.Get(), 0, nullptr, 0);
    gpu.commandList->EndQuery(mainQuery.heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 5);

    auto outputUav = UavBarrier(output.Get());
    gpu.commandList->ResourceBarrier(1, &outputUav);
    gpu.commandList->EndQuery(mainQuery.heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 6);
    auto outputToCopy = Transition(output.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_COPY_SOURCE);
    auto argumentToCopy = Transition(argumentBuffer.Get(),
        D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT,
        D3D12_RESOURCE_STATE_COPY_SOURCE);
    const std::array<D3D12_RESOURCE_BARRIER, 2> barriers{
        outputToCopy, argumentToCopy};
    gpu.commandList->ResourceBarrier(
        static_cast<UINT>(barriers.size()), barriers.data());
    gpu.commandList->EndQuery(mainQuery.heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 7);
    gpu.commandList->CopyBufferRegion(
        outputReadback.Get(), 0, output.Get(), 0, gpu.zeroBytes.size());
    gpu.commandList->EndQuery(mainQuery.heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 8);
    gpu.commandList->CopyBufferRegion(
        argumentReadback.Get(), 0, argumentBuffer.Get(), 0,
        sizeof(D3D12_DISPATCH_ARGUMENTS));
    gpu.commandList->EndQuery(mainQuery.heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 9);
    gpu.commandList->ResolveQueryData(
        mainQuery.heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP,
        0, 10, mainQuery.readback.Get(), 0);
    const auto recordingEnd = Clock::now();
    sample.commandRecordingNanoseconds = Nanoseconds(recordingEnd - recordingBegin);
    sample.afterCommandRecording = CaptureTransitionCheckpoint(gpu, probeBegin, false, nvml, ordinal);
    if (calibration)
        sample.preSubmitCalibration = RunTransitionCalibration(gpu, *calibration);
    sample.beforeMainSubmit = CaptureTransitionCheckpoint(gpu, probeBegin, true, nvml, ordinal);

    const auto closeBegin = Clock::now();
    Check(gpu.commandList->Close(), "Close transition main command list");
    const auto closeEnd = Clock::now();
    sample.closeNanoseconds = Nanoseconds(closeEnd - closeBegin);

    ID3D12CommandList* lists[]{gpu.commandList.Get()};
    const auto executeBegin = Clock::now();
    gpu.queue->ExecuteCommandLists(1, lists);
    const auto executeEnd = Clock::now();
    sample.executeCallNanoseconds = Nanoseconds(executeEnd - executeBegin);
    sample.afterExecuteCall = CaptureTransitionCheckpoint(gpu, probeBegin, false, nvml, ordinal);

    const auto signalBegin = Clock::now();
    const std::uint64_t fenceValue = ++gpu.fenceValue;
    Check(gpu.queue->Signal(gpu.fence.Get(), fenceValue),
        "Signal transition main fence");
    const auto signalEnd = Clock::now();
    sample.signalNanoseconds = Nanoseconds(signalEnd - signalBegin);
    sample.afterSignal = CaptureTransitionCheckpoint(gpu, probeBegin, false, nvml, ordinal);

    const auto waitBegin = Clock::now();
    if (gpu.fence->GetCompletedValue() < fenceValue)
    {
        Check(gpu.fence->SetEventOnCompletion(fenceValue, gpu.fenceEvent.value),
            "SetEventOnCompletion(transition main)");
        WaitForSingleObject(gpu.fenceEvent.value, INFINITE);
    }
    const auto waitEnd = Clock::now();
    sample.fenceWaitNanoseconds = Nanoseconds(waitEnd - waitBegin);
    sample.afterFence = CaptureTransitionCheckpoint(gpu, probeBegin, true, nvml, ordinal);
    if (calibration)
        sample.postMainCalibration = RunTransitionCalibration(gpu, *calibration);

    const auto readbackBegin = Clock::now();
    const auto outputBytes = ReadbackBytes(outputReadback.Get(), gpu.zeroBytes.size());
    const auto argumentBytes = ReadbackBytes(
        argumentReadback.Get(), sizeof(D3D12_DISPATCH_ARGUMENTS));
    const auto timestampBytes = ReadbackBytes(
        mainQuery.readback.Get(), sizeof(std::uint64_t) * 10u);
    const auto readbackEnd = Clock::now();
    sample.readbackNanoseconds = Nanoseconds(readbackEnd - readbackBegin);
    sample.afterReadback = CaptureTransitionCheckpoint(gpu, probeBegin, false, nvml, ordinal);

    const auto validationOutputBegin = Clock::now();
    std::array<std::uint64_t, 10> timestamps{};
    std::memcpy(timestamps.data(), timestampBytes.data(), timestampBytes.size());
    for (std::size_t index = 1; index < timestamps.size(); ++index)
        if (timestamps[index] < timestamps[index - 1])
            throw std::runtime_error("Transition Probe GPU timestamps are not monotonic.");
    D3D12_DISPATCH_ARGUMENTS observedArguments{};
    std::memcpy(&observedArguments, argumentBytes.data(), sizeof(observedArguments));
    sample.expectedDispatchGroupsX = authority.artifact.DispatchGroupsX();
    sample.observedDispatchGroupsX = observedArguments.ThreadGroupCountX;
    sample.outputDigest = base::Sha256(outputBytes);
    const auto expected = semantic::BuildSparseCpuReferenceOutputV1(set);
    std::vector<spiral5::semantic::PointRecordV1> observed(
        semantic::WorkUniverseCountV1);
    std::memcpy(observed.data(), outputBytes.data(), outputBytes.size());
    std::uint32_t activeWrites = 0;
    std::uint32_t unauthorizedWrites = 0;
    for (std::uint32_t index = 0; index < semantic::WorkUniverseCountV1; ++index)
    {
        const auto offset = static_cast<std::size_t>(index) *
            sizeof(spiral5::semantic::PointRecordV1);
        if (!set.Contains(index))
        {
            if (std::memcmp(outputBytes.data() + offset,
                gpu.zeroBytes.data() + offset,
                sizeof(spiral5::semantic::PointRecordV1)) != 0)
                ++unauthorizedWrites;
            continue;
        }
        const auto& actual = observed[index];
        const auto& reference = expected[index];
        const std::array<float, 4> actualValues{actual.x, actual.y, actual.z, actual.w};
        const std::array<float, 4> referenceValues{reference.x, reference.y, reference.z, reference.w};
        for (std::size_t component = 0; component < actualValues.size(); ++component)
            if (!std::isfinite(actualValues[component]) ||
                std::abs(static_cast<double>(actualValues[component]) -
                    static_cast<double>(referenceValues[component])) > 1.0e-6)
                throw std::runtime_error("Transition Probe active output differs from CPU reference.");
        if (std::bit_cast<std::uint32_t>(actual.w) !=
            std::bit_cast<std::uint32_t>(1.0f))
            throw std::runtime_error("Transition Probe homogeneous coordinate differs from one.");
        ++activeWrites;
    }
    if (activeWrites != set.Cardinality().Value() || unauthorizedWrites != 0u ||
        sample.observedDispatchGroupsX != sample.expectedDispatchGroupsX ||
        observedArguments.ThreadGroupCountY != 1u || observedArguments.ThreadGroupCountZ != 1u)
        throw std::runtime_error("Transition Probe exact write-set or Dispatch qualification failed.");
    const auto validationOutputEnd = Clock::now();
    sample.numericValidationNanoseconds =
        Nanoseconds(validationOutputEnd - validationOutputBegin);
    sample.sampleEnd = CaptureTransitionCheckpoint(gpu, probeBegin, true, nvml, ordinal);
    sample.cpuEndToEndNanoseconds = Nanoseconds(Clock::now() - cpuBegin);

    const double factor = 1.0e9 / static_cast<double>(gpu.timestampFrequency);
    const auto phase = [&](std::size_t first, std::size_t second)
    {
        return static_cast<double>(timestamps[second] - timestamps[first]) * factor;
    };
    sample.gpuSentinelCopyNanoseconds = phase(0, 1);
    sample.gpuOutputTransitionNanoseconds = phase(1, 2);
    sample.gpuArgumentDispatchNanoseconds = phase(2, 3);
    sample.gpuArgumentFinalizeNanoseconds = phase(3, 4);
    sample.gpuSparseConsumerNanoseconds = phase(4, 5);
    sample.gpuOutputUavBarrierNanoseconds = phase(5, 6);
    sample.gpuCopyTransitionNanoseconds = phase(6, 7);
    sample.gpuOutputReadbackCopyNanoseconds = phase(7, 8);
    sample.gpuArgumentReadbackCopyNanoseconds = phase(8, 9);
    sample.gpuSentinelPathNanoseconds = phase(0, 2);
    sample.gpuMainTotalNanoseconds = phase(0, 9);
    return sample;
}
#endif

[[maybe_unused]] TransitionProbeWindowV1 ClassifyObservedTransition(
    const std::vector<TransitionProbeRecordedSample>& samples,
    std::size_t firstSlow,
    const TransitionProbeConfigV1& config,
    double calibrationBaseline,
    double sentinelPathBaseline,
    double sentinelCopyBaseline)
{
    if (config.mode != TransitionProbeModeV1::BracketedCalibration)
        return TransitionProbeWindowV1::UnresolvedInsideSample;
    const auto calibrationSlow = [&](double value)
    {
        return calibrationBaseline > 0.0 &&
            value >= config.transitionRatio * calibrationBaseline;
    };
    const auto sentinelPathSlow = [&](const TransitionProbeRecordedSample& value)
    {
        return value.gpuSentinelPathNanoseconds >=
            config.transitionRatio * sentinelPathBaseline;
    };
    const auto sentinelCopySlow = [&](const TransitionProbeRecordedSample& value)
    {
        return value.gpuSentinelCopyNanoseconds >=
            config.transitionRatio * sentinelCopyBaseline;
    };

    const auto& current = samples[firstSlow];
    if (firstSlow != 0u)
    {
        const auto& previous = samples[firstSlow - 1u];
        if (!sentinelPathSlow(previous) &&
            calibrationSlow(previous.postMainCalibration.totalNanoseconds))
            return TransitionProbeWindowV1::AfterMainGpu;
        if (!calibrationSlow(previous.postMainCalibration.totalNanoseconds) &&
            calibrationSlow(current.preResourceCalibration.totalNanoseconds))
            return TransitionProbeWindowV1::BetweenSamples;
    }
    if (!calibrationSlow(current.preResourceCalibration.totalNanoseconds) &&
        calibrationSlow(current.postResourceCalibration.totalNanoseconds))
        return TransitionProbeWindowV1::ValidationOrResourceCreation;
    if (!calibrationSlow(current.postResourceCalibration.totalNanoseconds) &&
        calibrationSlow(current.preSubmitCalibration.totalNanoseconds))
        return TransitionProbeWindowV1::CommandRecording;
    if (!calibrationSlow(current.preSubmitCalibration.totalNanoseconds) &&
        sentinelPathSlow(current))
    {
        if (!sentinelCopySlow(current))
            return TransitionProbeWindowV1::DuringMainGpu;
        return TransitionProbeWindowV1::SubmitOrBeforeMainGpu;
    }
    if (!sentinelPathSlow(current) &&
        calibrationSlow(current.postMainCalibration.totalNanoseconds))
        return TransitionProbeWindowV1::AfterMainGpu;
    return TransitionProbeWindowV1::UnresolvedInsideSample;
}
}

std::string TransitionProbeModeNameV1(TransitionProbeModeV1 value)
{
    switch (value)
    {
    case TransitionProbeModeV1::CheckpointOnly: return "CheckpointOnly";
    case TransitionProbeModeV1::BracketedCalibration: return "BracketedCalibration";
    }
    throw std::invalid_argument("Unknown Transition Probe mode.");
}

std::string TransitionProbeWindowNameV1(TransitionProbeWindowV1 value)
{
    switch (value)
    {
    case TransitionProbeWindowV1::NotObserved: return "NotObserved";
    case TransitionProbeWindowV1::BetweenSamples: return "BetweenSamples";
    case TransitionProbeWindowV1::ValidationOrResourceCreation: return "ValidationOrResourceCreation";
    case TransitionProbeWindowV1::CommandRecording: return "CommandRecording";
    case TransitionProbeWindowV1::SubmitOrBeforeMainGpu: return "SubmitOrBeforeMainGpu";
    case TransitionProbeWindowV1::DuringMainGpu: return "DuringMainGpu";
    case TransitionProbeWindowV1::AfterMainGpu: return "AfterMainGpu";
    case TransitionProbeWindowV1::UnresolvedInsideSample: return "UnresolvedInsideSample";
    }
    throw std::invalid_argument("Unknown Transition Probe window.");
}

base::Result<void, std::string> RunTransitionProbeSelfTestV1()
{
    try
    {
        const auto sequence = TransitionProbeCandidateSequence();
        if (sequence[0] != CandidateKindV1::DenseMembershipMaskPredicate ||
            sequence[1] != CandidateKindV1::ActiveBlockListLocalMask ||
            sequence[2] != CandidateKindV1::CompactIndexListDispatch)
            throw std::runtime_error("Transition Probe must reproduce the A/C/B boundary.");
        auto set = semantic::BuildCanonicalSparseWorkSetV1(
            PatternKindV1::PrefixControl, semantic::SparseCardinalityV1(1));
        if (!set || set.Value().Indices() != std::vector<std::uint32_t>{0u})
            throw std::runtime_error("Transition Probe must use PrefixControl K=1 exact set {0}.");
        const double threshold = 2.5;
        if (ClassifySyntheticTransition({1,3,3,3,3,3}, threshold) !=
            TransitionProbeWindowV1::BetweenSamples ||
            ClassifySyntheticTransition({1,1,3,3,3,3}, threshold) !=
            TransitionProbeWindowV1::ValidationOrResourceCreation ||
            ClassifySyntheticTransition({1,1,1,3,3,3}, threshold) !=
            TransitionProbeWindowV1::CommandRecording ||
            ClassifySyntheticTransition({1,1,1,1,3,3}, threshold) !=
            TransitionProbeWindowV1::SubmitOrBeforeMainGpu ||
            ClassifySyntheticTransition({1,1,1,1,1,3}, threshold) !=
            TransitionProbeWindowV1::AfterMainGpu ||
            ClassifySyntheticTransition({1,1,1,1,1,1}, threshold) !=
            TransitionProbeWindowV1::NotObserved)
            throw std::runtime_error("Transition Probe synthetic window classifier failed.");
        return base::Result<void, std::string>::Success();
    }
    catch (const std::exception& error)
    {
        return base::Result<void, std::string>::Failure(error.what());
    }
}

base::Result<TransitionProbeRunSummaryV1, std::string>
RunTransitionProbeV1(
    const TransitionProbeConfigV1& config,
    const std::filesystem::path& outputDirectory)
{
#if !defined(_WIN32)
    (void)config;
    (void)outputDirectory;
    return base::Result<TransitionProbeRunSummaryV1, std::string>::Failure(
        "Spiral 6 Transition Probe requires Windows D3D12.");
#else
    try
    {
        if (config.sampleCount < 64u)
            throw std::runtime_error("Transition Probe requires at least 64 samples.");
        if (config.baselineSampleCount < 8u ||
            config.baselineSampleCount >= config.sampleCount)
            throw std::runtime_error("Transition Probe baseline sample count is invalid.");
        if (config.sustainedSampleCount == 0u ||
            config.sustainedSampleCount >= config.sampleCount)
            throw std::runtime_error("Transition Probe sustained sample count is invalid.");
        if (!(config.transitionRatio > 1.0))
            throw std::runtime_error("Transition Probe transition ratio must exceed one.");
        if (config.enableNvmlTelemetry && config.nvmlPollingIntervalMilliseconds == 0u)
            throw std::runtime_error("Transition Probe NVML polling interval must be non-zero.");
        auto selfTest = RunTransitionProbeSelfTestV1();
        if (!selfTest) throw std::runtime_error(selfTest.Error());
        std::filesystem::create_directories(outputDirectory);

        DeviceContext gpu(config.adapterIndex, config.requestStablePowerState);
        std::unique_ptr<nvml_dynamic::Runtime> nvml;
        if (config.enableNvmlTelemetry)
        {
            nvml = std::make_unique<nvml_dynamic::Runtime>(
                config.nvmlDeviceIndex, config.requireNvmlTelemetry);
        }
        const auto semanticValue = semantic::BuildCanonicalSparsePointTransformSemanticV1();
        const auto context = fv::BuildCanonicalSparseFamilyVerificationContextV1();
        auto setResult = semantic::BuildCanonicalSparseWorkSetV1(
            PatternKindV1::PrefixControl, semantic::SparseCardinalityV1(1));
        if (!setResult) throw std::runtime_error(setResult.Error().message);
        const auto& set = setResult.Value();
        auto authorities = BuildAuthorityBundles(semanticValue, set, context);
        const auto findAuthority = [&](CandidateKindV1 kind) -> const CandidateAuthorityBundle&
        {
            const auto found = std::find_if(authorities.begin(), authorities.end(),
                [kind](const CandidateAuthorityBundle& value)
                {
                    return value.raw.kind == kind;
                });
            if (found == authorities.end())
                throw std::runtime_error("Transition Probe Candidate authority is missing.");
            return *found;
        };

        TransitionProbeMainQuery mainQuery(gpu);
        std::unique_ptr<TransitionProbeCalibrationContext> calibration;
        if (config.mode == TransitionProbeModeV1::BracketedCalibration)
            calibration = std::make_unique<TransitionProbeCalibrationContext>(gpu);
        const auto sequence = TransitionProbeCandidateSequence();
        const auto probeBegin = Clock::now();
        nvml_dynamic::Poller nvmlPoller(
            nvml.get(), probeBegin, config.nvmlPollingIntervalMilliseconds);
        std::vector<TransitionProbeRecordedSample> samples;
        samples.reserve(config.sampleCount);
        for (std::uint64_t ordinal = 0; ordinal < config.sampleCount; ++ordinal)
        {
            nvmlPoller.SetOrdinal(ordinal);
            const auto kind = sequence[ordinal % sequence.size()];
            samples.push_back(ExecuteTransitionProbeSample(
                gpu, calibration.get(), mainQuery, semanticValue, set, context,
                findAuthority(kind), ordinal, probeBegin, nvml.get()));
        }
        auto nvmlTimeline = nvmlPoller.StopAndTake();

        const auto outputDigest = samples.front().outputDigest;
        for (const auto& sample : samples)
            if (sample.outputDigest != outputDigest ||
                sample.sparseSetIdentity != set.Identity())
                throw std::runtime_error("Transition Probe structural identity changed across samples.");
        for (CandidateKindV1 kind : TransitionProbeCandidateSequence())
        {
            const auto first = std::find_if(samples.begin(), samples.end(),
                [kind](const TransitionProbeRecordedSample& value)
                {
                    return value.candidate == kind;
                });
            if (first == samples.end())
                throw std::runtime_error("Transition Probe Candidate sample is missing.");
            for (const auto& sample : samples)
                if (sample.candidate == kind &&
                    (sample.artifactIdentity != first->artifactIdentity ||
                     sample.representationResourceIdentity != first->representationResourceIdentity ||
                     sample.frozenBindingIdentity != first->frozenBindingIdentity ||
                     sample.expectedDispatchGroupsX != first->expectedDispatchGroupsX ||
                     sample.observedDispatchGroupsX != first->observedDispatchGroupsX))
                    throw std::runtime_error("Transition Probe Candidate authority or Dispatch changed across samples.");
        }

        std::vector<double> baselineValues;
        std::vector<double> sentinelCopyBaselineValues;
        for (std::size_t index = 0; index < config.baselineSampleCount; ++index)
        {
            baselineValues.push_back(samples[index].gpuSentinelPathNanoseconds);
            sentinelCopyBaselineValues.push_back(samples[index].gpuSentinelCopyNanoseconds);
        }
        const double baseline = MedianCopy(baselineValues);
        const double sentinelCopyBaseline = MedianCopy(sentinelCopyBaselineValues);
        std::vector<double> calibrationBaselineValues;
        if (config.mode == TransitionProbeModeV1::BracketedCalibration)
        {
            for (std::size_t index = 0; index < config.baselineSampleCount; ++index)
            {
                calibrationBaselineValues.push_back(samples[index].preResourceCalibration.totalNanoseconds);
                calibrationBaselineValues.push_back(samples[index].postResourceCalibration.totalNanoseconds);
                calibrationBaselineValues.push_back(samples[index].preSubmitCalibration.totalNanoseconds);
                calibrationBaselineValues.push_back(samples[index].postMainCalibration.totalNanoseconds);
            }
        }
        const double calibrationBaseline = MedianCopy(calibrationBaselineValues);

        bool observed = false;
        std::size_t firstSlow = 0;
        for (std::size_t index = config.baselineSampleCount;
             index + config.sustainedSampleCount <= samples.size(); ++index)
        {
            bool sustained = true;
            for (std::size_t offset = 0; offset < config.sustainedSampleCount; ++offset)
                if (samples[index + offset].gpuSentinelPathNanoseconds <
                    baseline * config.transitionRatio)
                    sustained = false;
            if (sustained)
            {
                observed = true;
                firstSlow = index;
                break;
            }
        }
        const auto window = observed
            ? ClassifyObservedTransition(samples, firstSlow, config, calibrationBaseline,
                baseline, sentinelCopyBaseline)
            : TransitionProbeWindowV1::NotObserved;

        const auto checkpointColumns = [](std::ostringstream& csv, std::string_view name)
        {
            csv << ',' << name << "ElapsedNs," << name << "TimestampFrequency,"
                << name << "GpuCalibration," << name << "CpuCalibration,"
                << name << "LocalUsage," << name << "NonLocalUsage,"
                << name << "NvmlUnixNs," << name << "NvmlGraphicsClockMHz,"
                << name << "NvmlSmClockMHz," << name << "NvmlMemoryClockMHz,"
                << name << "NvmlVideoClockMHz," << name << "NvmlPState,"
                << name << "NvmlTemperatureC," << name << "NvmlPowerUsageMw,"
                << name << "NvmlPowerLimitMw," << name << "NvmlGpuUtilPercent,"
                << name << "NvmlMemoryUtilPercent," << name << "NvmlEventReasons,"
                << name << "NvmlEventReasonNames," << name << "NvmlGraphicsResult,"
                << name << "NvmlSmResult," << name << "NvmlMemoryResult,"
                << name << "NvmlVideoResult," << name << "NvmlPStateResult," << name << "NvmlTemperatureResult,"
                << name << "NvmlPowerUsageResult," << name << "NvmlPowerLimitResult,"
                << name << "NvmlUtilizationResult," << name << "NvmlEventReasonsResult";
        };
        const auto checkpointValues = [](std::ostringstream& csv,
            const TransitionProbeCheckpoint& checkpoint)
        {
            csv << ',' << checkpoint.elapsedNanoseconds;
#if defined(_WIN32)
            const auto& nvmlValue = checkpoint.nvml;
            csv << ',' << checkpoint.telemetry.timestampFrequency
                << ',' << checkpoint.telemetry.gpuCalibration
                << ',' << checkpoint.telemetry.cpuCalibration
                << ',' << checkpoint.telemetry.localUsage
                << ',' << checkpoint.telemetry.nonLocalUsage
                << ',' << nvmlValue.unixNanoseconds
                << ',' << nvmlValue.graphicsClockMHz
                << ',' << nvmlValue.smClockMHz
                << ',' << nvmlValue.memoryClockMHz
                << ',' << nvmlValue.videoClockMHz
                << ',' << nvmlValue.performanceState
                << ',' << nvmlValue.temperatureC
                << ',' << nvmlValue.powerUsageMilliwatts
                << ',' << nvmlValue.powerLimitMilliwatts
                << ',' << nvmlValue.gpuUtilizationPercent
                << ',' << nvmlValue.memoryUtilizationPercent
                << ',' << nvmlValue.clockEventReasons
                << ',' << nvml_dynamic::EventReasonNames(nvmlValue.clockEventReasons)
                << ',' << nvmlValue.graphicsClockResult
                << ',' << nvmlValue.smClockResult
                << ',' << nvmlValue.memoryClockResult
                << ',' << nvmlValue.videoClockResult
                << ',' << nvmlValue.performanceStateResult
                << ',' << nvmlValue.temperatureResult
                << ',' << nvmlValue.powerUsageResult
                << ',' << nvmlValue.powerLimitResult
                << ',' << nvmlValue.utilizationResult
                << ',' << nvmlValue.clockEventReasonsResult;
#endif
        };

        std::ostringstream csv;
        csv << "mode,ordinal,candidate,K,setIdentity,artifactIdentity,representationResourceIdentity,"
            << "frozenBindingIdentity,outputDigest";
        for (std::string_view name : {"sampleBegin", "afterValidation", "afterArtifactRebuild",
            "afterResources", "afterCommandRecording", "beforeMainSubmit", "afterExecuteCall",
            "afterSignal", "afterFence", "afterReadback", "sampleEnd"})
            checkpointColumns(csv, name);
        csv << ",preResourceCalibrationNs,postResourceCalibrationNs,preSubmitCalibrationNs,postMainCalibrationNs"
            << ",validationNs,artifactRebuildNs,resourceCreationUploadNs,commandRecordingNs,closeNs,"
            << "executeCallNs,signalNs,fenceWaitNs,readbackNs,numericValidationNs,cpuEndToEndNs,"
            << "gpuSentinelCopyNs,gpuOutputTransitionNs,gpuArgumentDispatchNs,gpuArgumentFinalizeNs,"
            << "gpuSparseConsumerNs,gpuOutputUavBarrierNs,gpuCopyTransitionNs,gpuOutputReadbackCopyNs,"
            << "gpuArgumentReadbackCopyNs,gpuSentinelPathNs,gpuMainTotalNs,expectedDispatchGroupsX,"
            << "observedDispatchGroupsX\n";
        csv << std::setprecision(17);
        for (const auto& sample : samples)
        {
            csv << TransitionProbeModeNameV1(config.mode) << ',' << sample.ordinal << ','
                << CandidateLetterV1(sample.candidate) << ",1,"
                << base::ToHex(sample.sparseSetIdentity) << ','
                << base::ToHex(sample.artifactIdentity) << ','
                << base::ToHex(sample.representationResourceIdentity) << ','
                << base::ToHex(sample.frozenBindingIdentity) << ','
                << base::ToHex(sample.outputDigest);
            checkpointValues(csv, sample.sampleBegin);
            checkpointValues(csv, sample.afterValidation);
            checkpointValues(csv, sample.afterArtifactRebuild);
            checkpointValues(csv, sample.afterResources);
            checkpointValues(csv, sample.afterCommandRecording);
            checkpointValues(csv, sample.beforeMainSubmit);
            checkpointValues(csv, sample.afterExecuteCall);
            checkpointValues(csv, sample.afterSignal);
            checkpointValues(csv, sample.afterFence);
            checkpointValues(csv, sample.afterReadback);
            checkpointValues(csv, sample.sampleEnd);
            csv << ',' << sample.preResourceCalibration.totalNanoseconds
                << ',' << sample.postResourceCalibration.totalNanoseconds
                << ',' << sample.preSubmitCalibration.totalNanoseconds
                << ',' << sample.postMainCalibration.totalNanoseconds
                << ',' << sample.validationNanoseconds
                << ',' << sample.artifactRebuildNanoseconds
                << ',' << sample.resourceCreationUploadNanoseconds
                << ',' << sample.commandRecordingNanoseconds
                << ',' << sample.closeNanoseconds
                << ',' << sample.executeCallNanoseconds
                << ',' << sample.signalNanoseconds
                << ',' << sample.fenceWaitNanoseconds
                << ',' << sample.readbackNanoseconds
                << ',' << sample.numericValidationNanoseconds
                << ',' << sample.cpuEndToEndNanoseconds
                << ',' << sample.gpuSentinelCopyNanoseconds
                << ',' << sample.gpuOutputTransitionNanoseconds
                << ',' << sample.gpuArgumentDispatchNanoseconds
                << ',' << sample.gpuArgumentFinalizeNanoseconds
                << ',' << sample.gpuSparseConsumerNanoseconds
                << ',' << sample.gpuOutputUavBarrierNanoseconds
                << ',' << sample.gpuCopyTransitionNanoseconds
                << ',' << sample.gpuOutputReadbackCopyNanoseconds
                << ',' << sample.gpuArgumentReadbackCopyNanoseconds
                << ',' << sample.gpuSentinelPathNanoseconds
                << ',' << sample.gpuMainTotalNanoseconds
                << ',' << sample.expectedDispatchGroupsX
                << ',' << sample.observedDispatchGroupsX << '\n';
        }
        const std::string csvText = csv.str();
        const auto csvIdentity = TextIdentity(csvText);
        WriteAuditTextFile(
            outputDirectory / "spiral6_transition_probe_samples_v1.csv", csvText);

        std::ostringstream nvmlCsv;
        nvmlCsv << "elapsedNs,unixNs,ordinal,graphicsClockMHz,smClockMHz,memoryClockMHz,"
            << "videoClockMHz,pState,temperatureC,powerUsageMw,powerLimitMw,gpuUtilPercent,"
            << "memoryUtilPercent,eventReasons,eventReasonNames,graphicsResult,smResult,"
            << "memoryResult,videoResult,pStateResult,temperatureResult,powerUsageResult,"
            << "powerLimitResult,utilizationResult,eventReasonsResult\n";
        for (const auto& value : nvmlTimeline)
        {
            nvmlCsv << value.elapsedNanoseconds << ',' << value.unixNanoseconds << ','
                << value.ordinal << ',' << value.graphicsClockMHz << ',' << value.smClockMHz
                << ',' << value.memoryClockMHz << ',' << value.videoClockMHz << ','
                << value.performanceState << ',' << value.temperatureC << ','
                << value.powerUsageMilliwatts << ',' << value.powerLimitMilliwatts << ','
                << value.gpuUtilizationPercent << ',' << value.memoryUtilizationPercent << ','
                << value.clockEventReasons << ','
                << nvml_dynamic::EventReasonNames(value.clockEventReasons) << ','
                << value.graphicsClockResult << ',' << value.smClockResult << ','
                << value.memoryClockResult << ',' << value.videoClockResult << ','
                << value.performanceStateResult << ',' << value.temperatureResult << ','
                << value.powerUsageResult << ',' << value.powerLimitResult << ','
                << value.utilizationResult << ',' << value.clockEventReasonsResult << '\n';
        }
        const std::string nvmlCsvText = nvmlCsv.str();
        const auto nvmlTimelineIdentity = TextIdentity(nvmlCsvText);
        WriteAuditTextFile(
            outputDirectory / "spiral6_transition_probe_nvml_timeline_v1.csv", nvmlCsvText);

        std::string nvmlDiagnosis = config.enableNvmlTelemetry
            ? "NoNvmlTransitionClassification"
            : "NvmlTelemetryDisabled";
        const nvml_dynamic::Snapshot* diagnosisBefore = nullptr;
        const nvml_dynamic::Snapshot* diagnosisAfter = nullptr;
        if (observed && nvml && nvml->Available())
        {
            diagnosisBefore = firstSlow == 0u
                ? &samples[firstSlow].sampleBegin.nvml
                : &samples[firstSlow - 1u].sampleEnd.nvml;
            diagnosisAfter = &samples[firstSlow].afterFence.nvml;
            const auto& before = *diagnosisBefore;
            const auto& after = *diagnosisAfter;
            if (after.clockEventReasonsResult == nvml_dynamic::Success &&
                (after.clockEventReasons & nvml_dynamic::ReasonHardwarePowerBrake) != 0)
                nvmlDiagnosis = "HwPowerBrakeSlowdown";
            else if (after.clockEventReasonsResult == nvml_dynamic::Success &&
                (after.clockEventReasons & nvml_dynamic::ReasonHardwareThermal) != 0)
                nvmlDiagnosis = "HwThermalSlowdown";
            else if (after.clockEventReasonsResult == nvml_dynamic::Success &&
                (after.clockEventReasons & nvml_dynamic::ReasonSoftwareThermal) != 0)
                nvmlDiagnosis = "SwThermalSlowdown";
            else if (after.clockEventReasonsResult == nvml_dynamic::Success &&
                (after.clockEventReasons & nvml_dynamic::ReasonSoftwarePowerCap) != 0)
                nvmlDiagnosis = "SwPowerCap";
            else if (before.performanceStateResult == nvml_dynamic::Success &&
                after.performanceStateResult == nvml_dynamic::Success &&
                after.performanceState > before.performanceState)
                nvmlDiagnosis = "PerformanceStateDegraded";
            else if (before.graphicsClockResult == nvml_dynamic::Success &&
                after.graphicsClockResult == nvml_dynamic::Success &&
                before.graphicsClockMHz != 0u &&
                static_cast<std::uint64_t>(after.graphicsClockMHz) * 100u <
                    static_cast<std::uint64_t>(before.graphicsClockMHz) * 70u)
                nvmlDiagnosis = "GraphicsClockDropWithoutReportedLimiter";
            else if (before.memoryClockResult == nvml_dynamic::Success &&
                after.memoryClockResult == nvml_dynamic::Success &&
                before.memoryClockMHz != 0u &&
                static_cast<std::uint64_t>(after.memoryClockMHz) * 100u <
                    static_cast<std::uint64_t>(before.memoryClockMHz) * 70u)
                nvmlDiagnosis = "MemoryClockDropWithoutReportedLimiter";
            else if (after.clockEventReasonsResult == nvml_dynamic::Success &&
                (after.clockEventReasons & nvml_dynamic::ReasonGpuIdle) != 0)
                nvmlDiagnosis = "GpuIdleClockEventObserved";
            else
                nvmlDiagnosis = "NoNvmlClockPStateOrLimiterChangeAtCapturedBoundary";
        }
        else if (config.enableNvmlTelemetry && (!nvml || !nvml->Available()))
        {
            nvmlDiagnosis = "NvmlUnavailable";
        }

        std::ostringstream report;
        report << "# Spiral 6 CU6 Intra-Sample Transition Probe\n\n";
        report << "- Mode: `" << TransitionProbeModeNameV1(config.mode) << "`\n";
        report << "- Adapter: `" << gpu.profile.description << "`\n";
        report << "- Exact Set: `PrefixControl K=1 = {0}`\n";
        report << "- Candidate sequence: `A, C, B` repeated\n";
        report << "- Samples: " << samples.size() << "\n";
        report << "- Baseline samples: " << config.baselineSampleCount << "\n";
        report << "- Sustained window: " << config.sustainedSampleCount << "\n";
        report << "- Transition ratio: " << config.transitionRatio << "x\n";
        report << "- Baseline sentinel path: " << baseline << " ns\n";
        report << "- Transition observed: " << (observed ? "true" : "false") << "\n";
        report << "- Stable Power State requested: "
            << (gpu.stablePowerStateRequested ? "true" : "false") << "\n";
        report << "- Stable Power State applied: "
            << (gpu.stablePowerStateApplied ? "true" : "false") << "\n";
        report << "- NVML telemetry enabled: "
            << (config.enableNvmlTelemetry ? "true" : "false") << "\n";
        report << "- NVML available: "
            << ((nvml && nvml->Available()) ? "true" : "false") << "\n";
        if (nvml)
        {
            report << "- NVML device: `" << nvml->DeviceName() << "`\n";
            report << "- NVML UUID: `" << nvml->DeviceUuid() << "`\n";
            if (!nvml->Available()) report << "- NVML error: `" << nvml->Error() << "`\n";
        }
        report << "- NVML polling interval: "
            << config.nvmlPollingIntervalMilliseconds << " ms\n";
        report << "- NVML timeline records: " << nvmlTimeline.size() << "\n";
        report << "- NVML transition diagnosis: `" << nvmlDiagnosis << "`\n";
        if (observed)
        {
            const auto& current = samples[firstSlow];
            report << "- First sustained slow ordinal: " << firstSlow << "\n";
            report << "- Previous Candidate: `" << (firstSlow == 0u ? '-'
                : CandidateLetterV1(samples[firstSlow - 1u].candidate)) << "`\n";
            report << "- Current Candidate: `" << CandidateLetterV1(current.candidate) << "`\n";
            if (firstSlow != 0u)
            {
                const auto& previous = samples[firstSlow - 1u];
                report << "- Previous sentinel path: "
                    << previous.gpuSentinelPathNanoseconds << " ns\n";
                if (config.mode == TransitionProbeModeV1::BracketedCalibration)
                    report << "- Previous post-main calibration: "
                        << previous.postMainCalibration.totalNanoseconds << " ns\n";
            }
            report << "- First slow sentinel path: "
                << current.gpuSentinelPathNanoseconds << " ns\n";
            report << "- Located transition window: `"
                << TransitionProbeWindowNameV1(window) << "`\n";
            report << "- CPU checkpoint elapsed times (ns): sampleBegin="
                << current.sampleBegin.elapsedNanoseconds
                << ", afterValidation=" << current.afterValidation.elapsedNanoseconds
                << ", afterResources=" << current.afterResources.elapsedNanoseconds
                << ", afterCommandRecording=" << current.afterCommandRecording.elapsedNanoseconds
                << ", beforeMainSubmit=" << current.beforeMainSubmit.elapsedNanoseconds
                << ", afterExecuteCall=" << current.afterExecuteCall.elapsedNanoseconds
                << ", afterSignal=" << current.afterSignal.elapsedNanoseconds
                << ", afterFence=" << current.afterFence.elapsedNanoseconds << "\n";
            if (config.mode == TransitionProbeModeV1::BracketedCalibration)
            {
                report << "- Calibration baseline: " << calibrationBaseline << " ns\n";
                report << "- Current calibration totals (ns): preResource="
                    << current.preResourceCalibration.totalNanoseconds
                    << ", postResource=" << current.postResourceCalibration.totalNanoseconds
                    << ", preSubmit=" << current.preSubmitCalibration.totalNanoseconds
                    << ", postMain=" << current.postMainCalibration.totalNanoseconds << "\n";
            }
            if (diagnosisBefore && diagnosisAfter)
            {
                const auto writeNvml = [&](std::string_view label,
                    const nvml_dynamic::Snapshot& value)
                {
                    report << "- " << label << " NVML: graphics="
                        << value.graphicsClockMHz << " MHz, sm=" << value.smClockMHz
                        << " MHz, memory=" << value.memoryClockMHz << " MHz, pState=P"
                        << value.performanceState << ", temperature=" << value.temperatureC
                        << " C, power=" << value.powerUsageMilliwatts << " mW, limit="
                        << value.powerLimitMilliwatts << " mW, gpuUtil="
                        << value.gpuUtilizationPercent << "%, eventReasons=`"
                        << nvml_dynamic::EventReasonNames(value.clockEventReasons) << "`\n";
                };
                writeNvml("Before", *diagnosisBefore);
                writeNvml("After", *diagnosisAfter);
            }
            report << "- Main GPU phases (ns): sentinelCopy="
                << current.gpuSentinelCopyNanoseconds
                << ", outputTransition=" << current.gpuOutputTransitionNanoseconds
                << ", argumentDispatch=" << current.gpuArgumentDispatchNanoseconds
                << ", argumentFinalize=" << current.gpuArgumentFinalizeNanoseconds
                << ", consumer=" << current.gpuSparseConsumerNanoseconds
                << ", outputUavBarrier=" << current.gpuOutputUavBarrierNanoseconds
                << ", copyTransition=" << current.gpuCopyTransitionNanoseconds
                << ", outputReadbackCopy=" << current.gpuOutputReadbackCopyNanoseconds
                << ", argumentReadbackCopy=" << current.gpuArgumentReadbackCopyNanoseconds << "\n";
        }
        report << "- CSV SHA-256: `" << base::ToHex(csvIdentity) << "`\n";
        report << "- NVML timeline SHA-256: `"
            << base::ToHex(nvmlTimelineIdentity) << "`\n\n";
        report << "NVML values are retained with per-field result codes because GeForce support is query-specific. "
               "The in-process timeline is the primary clock/P-state/power evidence; the runner also records an "
               "independent nvidia-smi sidecar when the executable is available.\n";
        const std::string reportText = report.str();
        const auto reportIdentity = TextIdentity(reportText);
        WriteAuditTextFile(
            outputDirectory / "SPIRAL6_TRANSITION_PROBE_REPORT.md", reportText);

        TransitionProbeRunSummaryV1 summary;
        summary.mode = config.mode;
        summary.adapterDescription = gpu.profile.description;
        summary.sampleCount = static_cast<std::uint32_t>(samples.size());
        summary.transitionObserved = observed;
        summary.firstSlowOrdinal = observed ? firstSlow : 0u;
        summary.transitionWindow = window;
        summary.baselineSentinelNanoseconds = baseline;
        summary.firstSlowSentinelNanoseconds =
            observed ? samples[firstSlow].gpuSentinelPathNanoseconds : 0.0;
        summary.sparseSetIdentity = set.Identity();
        summary.csvIdentity = csvIdentity;
        summary.runReportIdentity = reportIdentity;
        summary.nvmlTelemetryEnabled = config.enableNvmlTelemetry;
        summary.nvmlAvailable = nvml && nvml->Available();
        summary.stablePowerStateRequested = gpu.stablePowerStateRequested;
        summary.stablePowerStateApplied = gpu.stablePowerStateApplied;
        summary.nvmlPollingIntervalMilliseconds =
            config.nvmlPollingIntervalMilliseconds;
        summary.nvmlTimelineRecordCount = static_cast<std::uint64_t>(nvmlTimeline.size());
        if (nvml)
        {
            summary.nvmlDeviceName = nvml->DeviceName();
            summary.nvmlDeviceUuid = nvml->DeviceUuid();
        }
        summary.nvmlTransitionDiagnosis = nvmlDiagnosis;
        summary.nvmlTimelineIdentity = nvmlTimelineIdentity;
        return base::Result<TransitionProbeRunSummaryV1, std::string>::Success(
            std::move(summary));
    }
    catch (const std::exception& error)
    {
        return base::Result<TransitionProbeRunSummaryV1, std::string>::Failure(error.what());
    }
#endif
}

}
