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
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
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
// Primary metric: batched GPU consumer nanoseconds per iteration.
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

std::string ControlShader()
{
    return R"(
RWStructuredBuffer<uint> ControlOutput : register(u0);
[numthreads(64,1,1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint index = dispatchThreadId.x;
    if (index >= 4096) return;
    uint value = index * 1664525u + 1013904223u;
    value ^= value >> 13;
    value *= 2246822519u;
    ControlOutput[index] = value;
}
)";
}

base::Digest256 DigestFromHex(std::string_view value)
{
    if (value.size() != 64) throw std::runtime_error("Digest hex length must be 64.");
    const auto nibble = [](char c) -> std::uint8_t
    {
        if (c >= '0' && c <= '9') return static_cast<std::uint8_t>(c - '0');
        if (c >= 'a' && c <= 'f') return static_cast<std::uint8_t>(10 + c - 'a');
        if (c >= 'A' && c <= 'F') return static_cast<std::uint8_t>(10 + c - 'A');
        throw std::runtime_error("Digest contains non-hex character.");
    };
    base::Digest256 result{};
    for (std::size_t index = 0; index < result.size(); ++index)
        result[index] = static_cast<std::byte>((nibble(value[index * 2]) << 4u) | nibble(value[index * 2 + 1]));
    return result;
}

NvmlSnapshotV2 ToPublicSnapshot(const nvml_dynamic::Snapshot& value, bool available)
{
    NvmlSnapshotV2 result;
    result.available = available;
    result.graphicsClockMHz = value.graphicsClockMHz;
    result.smClockMHz = value.smClockMHz;
    result.memoryClockMHz = value.memoryClockMHz;
    result.videoClockMHz = value.videoClockMHz;
    result.performanceState = value.performanceState;
    result.temperatureC = value.temperatureC;
    result.powerUsageMilliwatts = value.powerUsageMilliwatts;
    result.powerLimitMilliwatts = value.powerLimitMilliwatts;
    result.gpuUtilizationPercent = value.gpuUtilizationPercent;
    result.memoryUtilizationPercent = value.memoryUtilizationPercent;
    result.clockEventReasons = value.clockEventReasons;
    result.graphicsClockResult = value.graphicsClockResult;
    result.smClockResult = value.smClockResult;
    result.memoryClockResult = value.memoryClockResult;
    result.videoClockResult = value.videoClockResult;
    result.performanceStateResult = value.performanceStateResult;
    result.temperatureResult = value.temperatureResult;
    result.powerUsageResult = value.powerUsageResult;
    result.powerLimitResult = value.powerLimitResult;
    result.utilizationResult = value.utilizationResult;
    result.clockEventReasonsResult = value.clockEventReasonsResult;
    return result;
}

BlockTelemetrySummaryV2 SummarizeTelemetry(
    std::span<const nvml_dynamic::Snapshot> values,
    const NvmlSnapshotV2& pre,
    const NvmlSnapshotV2& post)
{
    BlockTelemetrySummaryV2 result;
    std::vector<NvmlSnapshotV2> snapshots;
    if (pre.available) snapshots.push_back(pre);
    for (const auto& value : values) snapshots.push_back(ToPublicSnapshot(value, true));
    if (post.available) snapshots.push_back(post);
    if (snapshots.empty()) return result;
    result.available = true;
    result.recordCount = static_cast<std::uint32_t>(snapshots.size());
    result.minimumGraphicsClockMHz = std::numeric_limits<std::uint32_t>::max();
    result.minimumMemoryClockMHz = std::numeric_limits<std::uint32_t>::max();
    result.minimumPerformanceState = std::numeric_limits<std::uint32_t>::max();
    for (const auto& value : snapshots)
    {
        if (value.graphicsClockResult == nvml_dynamic::Success)
        {
            result.minimumGraphicsClockMHz = std::min(result.minimumGraphicsClockMHz, value.graphicsClockMHz);
            result.maximumGraphicsClockMHz = std::max(result.maximumGraphicsClockMHz, value.graphicsClockMHz);
        }
        if (value.memoryClockResult == nvml_dynamic::Success)
        {
            result.minimumMemoryClockMHz = std::min(result.minimumMemoryClockMHz, value.memoryClockMHz);
            result.maximumMemoryClockMHz = std::max(result.maximumMemoryClockMHz, value.memoryClockMHz);
        }
        if (value.performanceStateResult == nvml_dynamic::Success)
        {
            result.minimumPerformanceState = std::min(result.minimumPerformanceState, value.performanceState);
            result.maximumPerformanceState = std::max(result.maximumPerformanceState, value.performanceState);
        }
        if (value.temperatureResult == nvml_dynamic::Success)
            result.maximumTemperatureC = std::max(result.maximumTemperatureC, value.temperatureC);
        if (value.clockEventReasonsResult == nvml_dynamic::Success)
            result.clockEventReasonsOr |= value.clockEventReasons;
    }
    if (result.minimumGraphicsClockMHz == std::numeric_limits<std::uint32_t>::max())
        result.minimumGraphicsClockMHz = 0;
    if (result.minimumMemoryClockMHz == std::numeric_limits<std::uint32_t>::max())
        result.minimumMemoryClockMHz = 0;
    if (result.minimumPerformanceState == std::numeric_limits<std::uint32_t>::max())
        result.minimumPerformanceState = 0;
    result.gpuIdleObserved = (result.clockEventReasonsOr & nvml_dynamic::ReasonGpuIdle) != 0;
    result.hardwareThermalObserved =
        (result.clockEventReasonsOr & nvml_dynamic::ReasonHardwareThermal) != 0;
    result.hardwarePowerBrakeObserved =
        (result.clockEventReasonsOr & nvml_dynamic::ReasonHardwarePowerBrake) != 0;
    return result;
}

std::string NormalizeAdapterName(std::string_view value)
{
    std::string result;
    for (unsigned char c : value)
        if (std::isalnum(c) != 0) result += static_cast<char>(std::tolower(c));
    const std::string prefix = "nvidia";
    if (result.starts_with(prefix)) result.erase(0, prefix.size());
    return result;
}

struct DeviceContext final
{
    ComPtr<IDXGIFactory6> factory;
    ComPtr<IDXGIAdapter1> adapter;
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12CommandAllocator> mainAllocator;
    ComPtr<ID3D12GraphicsCommandList> mainList;
    ComPtr<ID3D12CommandAllocator> warmupAllocator;
    ComPtr<ID3D12GraphicsCommandList> warmupList;
    ComPtr<ID3D12Fence> fence;
    EventHandle fenceEvent;
    std::uint64_t fenceValue = 0;
    std::uint64_t timestampFrequency = 0;
    bool stablePowerStateApplied = false;
    AdapterProfileV2 profile{};

    std::map<CandidateKindV2, ComPtr<ID3DBlob>> candidateShaders;
    ComPtr<ID3DBlob> controlShader;
    ComPtr<ID3D12RootSignature> candidateRoot;
    ComPtr<ID3D12RootSignature> controlRoot;
    std::map<CandidateKindV2, ComPtr<ID3D12PipelineState>> candidatePipelines;
    ComPtr<ID3D12PipelineState> controlPipeline;
    ComPtr<ID3D12CommandSignature> dispatchSignature;
    ComPtr<ID3D12QueryHeap> queryHeap;
    ComPtr<ID3D12Resource> queryReadback;
    std::uint32_t queryCapacity = 0;

    ComPtr<ID3D12Resource> motorUpload;
    ComPtr<ID3D12Resource> pointUpload;
    ComPtr<ID3D12Resource> zeroUpload;
    ComPtr<ID3D12Resource> controlOutput;
    std::vector<std::byte> zeroBytes;
    std::vector<spiral5::semantic::PointRecordV1> points;
    std::array<spiral5::semantic::PgaMotorRecordV1, semantic::BoneCountV1> motors{};

    DeviceContext(const MeasurementConfigV2& config, std::uint32_t maximumQueries)
        : queryCapacity(maximumQueries)
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
        if (config.adapterIndex >= eligible.size())
            throw std::runtime_error("Requested eligible hardware adapter index is unavailable.");
        adapter = eligible[config.adapterIndex];
        Check(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)),
            "D3D12CreateDevice");
        if (config.requestStablePowerStateDiagnostic)
        {
            Check(device->SetStablePowerState(TRUE), "SetStablePowerState(TRUE)");
            stablePowerStateApplied = true;
        }

        D3D12_COMMAND_QUEUE_DESC queueDescription{};
        queueDescription.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
        Check(device->CreateCommandQueue(&queueDescription, IID_PPV_ARGS(&queue)), "CreateCommandQueue");
        Check(queue->GetTimestampFrequency(&timestampFrequency), "GetTimestampFrequency");
        Check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&mainAllocator)),
            "CreateCommandAllocator(main)");
        Check(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, mainAllocator.Get(), nullptr,
            IID_PPV_ARGS(&mainList)), "CreateCommandList(main)");
        Check(mainList->Close(), "Close(initial main list)");
        Check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&warmupAllocator)),
            "CreateCommandAllocator(warmup)");
        Check(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, warmupAllocator.Get(), nullptr,
            IID_PPV_ARGS(&warmupList)), "CreateCommandList(warmup)");
        Check(warmupList->Close(), "Close(initial warmup list)");
        Check(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)), "CreateFence");

        candidateShaders.emplace(CandidateKindV2::DenseMembershipMaskPredicate,
            CompileShader(DenseShader(), "SparseDenseMaskV2"));
        candidateShaders.emplace(CandidateKindV2::CompactIndexListDispatch,
            CompileShader(CompactShader(), "SparseCompactIndexV2"));
        candidateShaders.emplace(CandidateKindV2::ActiveBlockListLocalMask,
            CompileShader(ActiveBlockShader(), "SparseActiveBlockV2"));
        controlShader = CompileShader(ControlShader(), "Spiral6ControlV2");

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

        D3D12_ROOT_PARAMETER controlParameter{};
        controlParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
        controlParameter.Descriptor.ShaderRegister = 0;
        controlRoot = CreateRootSignature(
            device.Get(), std::span<const D3D12_ROOT_PARAMETER>(&controlParameter, 1));

        for (const auto& [kind, shader] : candidateShaders)
            candidatePipelines.emplace(kind, CreatePipeline(device.Get(), candidateRoot.Get(), shader.Get()));
        controlPipeline = CreatePipeline(device.Get(), controlRoot.Get(), controlShader.Get());

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
        queryDescription.Count = maximumQueries;
        Check(device->CreateQueryHeap(&queryDescription, IID_PPV_ARGS(&queryHeap)), "CreateQueryHeap");
        queryReadback = CreateBuffer(device.Get(), D3D12_HEAP_TYPE_READBACK,
            sizeof(std::uint64_t) * maximumQueries, D3D12_RESOURCE_STATE_COPY_DEST);

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
        controlOutput = CreateBuffer(device.Get(), D3D12_HEAP_TYPE_DEFAULT,
            semantic::WorkUniverseCountV1 * sizeof(std::uint32_t), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
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

    void WaitForSubmitted()
    {
        const std::uint64_t value = ++fenceValue;
        Check(queue->Signal(fence.Get(), value), "Signal fence");
        if (fence->GetCompletedValue() < value)
        {
            Check(fence->SetEventOnCompletion(value, fenceEvent.value), "SetEventOnCompletion");
            WaitForSingleObject(fenceEvent.value, INFINITE);
        }
    }

    void SubmitAndWait(ID3D12GraphicsCommandList* list)
    {
        ID3D12CommandList* lists[]{list};
        queue->ExecuteCommandLists(1, lists);
        WaitForSubmitted();
    }

    void RecordWarmup(std::uint32_t iterations)
    {
        Check(warmupAllocator->Reset(), "Reset warmup allocator");
        Check(warmupList->Reset(warmupAllocator.Get(), nullptr), "Reset warmup list");
        warmupList->SetPipelineState(controlPipeline.Get());
        warmupList->SetComputeRootSignature(controlRoot.Get());
        warmupList->SetComputeRootUnorderedAccessView(0, controlOutput->GetGPUVirtualAddress());
        for (std::uint32_t index = 0; index < iterations; ++index)
            warmupList->Dispatch(64, 1, 1);
        auto barrier = UavBarrier(controlOutput.Get());
        warmupList->ResourceBarrier(1, &barrier);
        Check(warmupList->Close(), "Close warmup list");
    }

    std::array<CandidateProfileV2, CanonicalCandidateCountV2> BuildCandidateProfiles() const
    {
        std::array<CandidateProfileV2, CanonicalCandidateCountV2> values{};
        const auto context = fv::BuildCanonicalSparseFamilyVerificationContextV1();
        const auto kinds = CanonicalCandidateKindsV2();
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
            switch (kinds[index])
            {
            case CandidateKindV2::DenseMembershipMaskPredicate:
                value.representationRole = fc::SparseFamilyRepresentationRoleV1::DenseMembershipMask4096;
                value.dispatchPolicy = fc::SparseFamilyDispatchPolicyV1::FixedUniverseGroups;
                value.payloadStrideBytes = 4;
                value.fixedCapacityBytes = fc::DenseMaskCapacityBytesV1;
                break;
            case CandidateKindV2::CompactIndexListDispatch:
                value.representationRole = fc::SparseFamilyRepresentationRoleV1::CompactSortedUniverseIndexList;
                value.dispatchPolicy = fc::SparseFamilyDispatchPolicyV1::CeilCardinalityGroups;
                value.payloadStrideBytes = 4;
                value.fixedCapacityBytes = fc::CompactIndexCapacityBytesV1;
                break;
            case CandidateKindV2::ActiveBlockListLocalMask:
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
    result.reserve(CanonicalCandidateCountV2);
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
    if (result.size() != CanonicalCandidateCountV2)
        throw std::runtime_error("Sparse Candidate authority family is incomplete.");
    return result;
}

CandidateCaseAuthorityV2 ToEvidenceAuthority(const CandidateAuthorityBundle& value)
{
    CandidateCaseAuthorityV2 result;
    result.kind = value.raw.kind;
    result.rawCandidateIdentity = value.raw.rawCandidateIdentity;
    result.verifiedPlanIdentity = value.verified.Plan().planIdentity;
    result.verifierCertificateIdentity = value.verified.CertificateIdentity();
    result.artifactIdentity = value.artifact.ArtifactIdentity();
    result.representationResourceIdentity = value.verified.Plan().operation.representationResourceIdentity;
    result.frozenBindingIdentity = value.frozen.BindingIdentity();
    return result;
}

struct PreparedCandidate final
{
    CandidateAuthorityBundle authority;
    ComPtr<ID3D12Resource> representationUpload;
    ComPtr<ID3D12Resource> indirectArguments;
    ComPtr<ID3D12Resource> output;
    ComPtr<ID3D12Resource> outputReadback;

    PreparedCandidate(CandidateAuthorityBundle authorityValue)
        : authority(std::move(authorityValue)) {}
};

struct PreparedCase final
{
    PatternKindV2 pattern = PatternKindV2::PrefixControl;
    std::uint32_t activeCount = 0;
    semantic::ExactSparseWorkSetV1 set;
    std::vector<PreparedCandidate> candidates;

    PreparedCase(
        PatternKindV2 patternValue,
        std::uint32_t activeCountValue,
        semantic::ExactSparseWorkSetV1 setValue)
        : pattern(patternValue), activeCount(activeCountValue), set(std::move(setValue)) {}
};

PreparedCase BuildPreparedCase(
    DeviceContext& gpu,
    PatternKindV2 pattern,
    std::uint32_t activeCount,
    const semantic::SparsePointTransformSemanticV1& semanticValue,
    const fv::SparseFamilyVerificationContextV1& context)
{
    auto setResult = semantic::BuildCanonicalSparseWorkSetV1(
        pattern, semantic::SparseCardinalityV1(activeCount));
    if (!setResult) throw std::runtime_error(setResult.Error().message);
    PreparedCase result(pattern, activeCount, setResult.Value());
    auto authorities = BuildAuthorityBundles(semanticValue, result.set, context);
    for (auto& authority : authorities)
    {
        auto valid = fe::ValidateFrozenVerifiedSparseFamilyCandidateV1(
            authority.frozen, semanticValue, result.set, context, 1);
        if (!valid) throw std::runtime_error(valid.Error());
        auto rebuilt = fc::BuildSparseFamilyArtifactV1(semanticValue, result.set, authority.raw.kind);
        if (rebuilt.Bytes() != authority.artifact.Bytes())
            throw std::runtime_error("Sparse representation rematerialization is not deterministic.");

        PreparedCandidate prepared(std::move(authority));
        prepared.representationUpload = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_UPLOAD,
            rebuilt.PayloadBytes().size(), D3D12_RESOURCE_STATE_GENERIC_READ);
        UploadBytes(prepared.representationUpload.Get(), rebuilt.PayloadBytes());
        prepared.indirectArguments = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_UPLOAD,
            sizeof(D3D12_DISPATCH_ARGUMENTS), D3D12_RESOURCE_STATE_GENERIC_READ);
        const D3D12_DISPATCH_ARGUMENTS arguments{rebuilt.DispatchGroupsX(), 1, 1};
        UploadBytes(prepared.indirectArguments.Get(), std::as_bytes(std::span(&arguments, 1)));
        prepared.output = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_DEFAULT,
            gpu.zeroBytes.size(), D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        prepared.outputReadback = CreateBuffer(gpu.device.Get(), D3D12_HEAP_TYPE_READBACK,
            gpu.zeroBytes.size(), D3D12_RESOURCE_STATE_COPY_DEST);
        result.candidates.push_back(std::move(prepared));
    }
    return result;
}

struct RecordedSample final
{
    std::uint32_t orderIndex = 0;
    std::uint32_t cycle = 0;
    std::uint32_t orderPosition = 0;
    std::uint32_t candidateIndex = 0;
    std::uint32_t queryBegin = 0;
    std::uint32_t queryEnd = 0;
};

struct RecordedBlock final
{
    std::vector<RecordedSample> samples;
    std::uint32_t controlBeforeBegin = 0;
    std::uint32_t controlBeforeEnd = 0;
    std::uint32_t controlAfterBegin = 0;
    std::uint32_t controlAfterEnd = 0;
    std::uint32_t queryCount = 0;
};

RecordedBlock RecordMeasurementBlock(
    DeviceContext& gpu,
    PreparedCase& prepared,
    const MeasurementConfigV2& config,
    std::uint32_t orderRotation)
{
    const auto orders = CanonicalBalancedOrdersV2();
    Check(gpu.mainAllocator->Reset(), "Reset main allocator");
    Check(gpu.mainList->Reset(gpu.mainAllocator.Get(), nullptr), "Reset main list");
    RecordedBlock recorded;
    std::uint32_t query = 0;
    recorded.controlBeforeBegin = query++;
    gpu.mainList->EndQuery(gpu.queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, recorded.controlBeforeBegin);
    gpu.mainList->SetPipelineState(gpu.controlPipeline.Get());
    gpu.mainList->SetComputeRootSignature(gpu.controlRoot.Get());
    gpu.mainList->SetComputeRootUnorderedAccessView(0, gpu.controlOutput->GetGPUVirtualAddress());
    for (std::uint32_t index = 0; index < config.iterationsPerBatch; ++index)
        gpu.mainList->Dispatch(64, 1, 1);
    auto controlBarrier = UavBarrier(gpu.controlOutput.Get());
    gpu.mainList->ResourceBarrier(1, &controlBarrier);
    recorded.controlBeforeEnd = query++;
    gpu.mainList->EndQuery(gpu.queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, recorded.controlBeforeEnd);

    std::array<bool, CanonicalCandidateCountV2> candidateInUav{};
    for (std::uint32_t orderOffset = 0; orderOffset < orders.size(); ++orderOffset)
    {
        const std::uint32_t orderIndex = (orderOffset + orderRotation) % static_cast<std::uint32_t>(orders.size());
        const auto& order = orders[orderIndex];
        for (std::uint32_t cycle = 0; cycle < config.measurementCyclesPerOrder; ++cycle)
        {
            for (std::uint32_t position = 0; position < CanonicalCandidateCountV2; ++position)
            {
                const std::uint32_t candidateIndex = order[position];
                auto& candidate = prepared.candidates[candidateIndex];
                if (candidateInUav[candidateIndex])
                {
                    auto toCopyDest = Transition(candidate.output.Get(),
                        D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST);
                    gpu.mainList->ResourceBarrier(1, &toCopyDest);
                }
                gpu.mainList->CopyBufferRegion(candidate.output.Get(), 0, gpu.zeroUpload.Get(), 0, gpu.zeroBytes.size());
                auto toUav = Transition(candidate.output.Get(),
                    D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                gpu.mainList->ResourceBarrier(1, &toUav);
                candidateInUav[candidateIndex] = true;

                RecordedSample sample;
                sample.orderIndex = orderIndex;
                sample.cycle = cycle;
                sample.orderPosition = position;
                sample.candidateIndex = candidateIndex;
                sample.queryBegin = query++;
                gpu.mainList->EndQuery(gpu.queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, sample.queryBegin);
                gpu.mainList->SetPipelineState(gpu.candidatePipelines.at(candidate.authority.raw.kind).Get());
                gpu.mainList->SetComputeRootSignature(gpu.candidateRoot.Get());
                gpu.mainList->SetComputeRoot32BitConstant(0, candidate.authority.artifact.ActiveCount(), 0);
                gpu.mainList->SetComputeRootShaderResourceView(1,
                    candidate.representationUpload->GetGPUVirtualAddress());
                gpu.mainList->SetComputeRootShaderResourceView(2, gpu.motorUpload->GetGPUVirtualAddress());
                gpu.mainList->SetComputeRootShaderResourceView(3, gpu.pointUpload->GetGPUVirtualAddress());
                gpu.mainList->SetComputeRootUnorderedAccessView(4, candidate.output->GetGPUVirtualAddress());
                for (std::uint32_t iteration = 0; iteration < config.iterationsPerBatch; ++iteration)
                    gpu.mainList->ExecuteIndirect(gpu.dispatchSignature.Get(), 1,
                        candidate.indirectArguments.Get(), 0, nullptr, 0);
                auto outputBarrier = UavBarrier(candidate.output.Get());
                gpu.mainList->ResourceBarrier(1, &outputBarrier);
                sample.queryEnd = query++;
                gpu.mainList->EndQuery(gpu.queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, sample.queryEnd);
                recorded.samples.push_back(sample);
            }
        }
    }

    recorded.controlAfterBegin = query++;
    gpu.mainList->EndQuery(gpu.queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, recorded.controlAfterBegin);
    gpu.mainList->SetPipelineState(gpu.controlPipeline.Get());
    gpu.mainList->SetComputeRootSignature(gpu.controlRoot.Get());
    gpu.mainList->SetComputeRootUnorderedAccessView(0, gpu.controlOutput->GetGPUVirtualAddress());
    for (std::uint32_t index = 0; index < config.iterationsPerBatch; ++index)
        gpu.mainList->Dispatch(64, 1, 1);
    controlBarrier = UavBarrier(gpu.controlOutput.Get());
    gpu.mainList->ResourceBarrier(1, &controlBarrier);
    recorded.controlAfterEnd = query++;
    gpu.mainList->EndQuery(gpu.queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, recorded.controlAfterEnd);

    for (std::size_t candidateIndex = 0; candidateIndex < prepared.candidates.size(); ++candidateIndex)
    {
        auto& candidate = prepared.candidates[candidateIndex];
        auto toCopySource = Transition(candidate.output.Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
        gpu.mainList->ResourceBarrier(1, &toCopySource);
        gpu.mainList->CopyBufferRegion(candidate.outputReadback.Get(), 0,
            candidate.output.Get(), 0, gpu.zeroBytes.size());
        auto toCopyDest = Transition(candidate.output.Get(),
            D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
        gpu.mainList->ResourceBarrier(1, &toCopyDest);
    }
    recorded.queryCount = query;
    if (recorded.queryCount > gpu.queryCapacity)
        throw std::runtime_error("Recorded query count exceeds query heap capacity.");
    gpu.mainList->ResolveQueryData(gpu.queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP,
        0, recorded.queryCount, gpu.queryReadback.Get(), 0);
    Check(gpu.mainList->Close(), "Close main list");
    return recorded;
}

void ValidateOutputs(
    DeviceContext& gpu,
    PreparedCase& prepared,
    std::array<TimingSampleV2, CanonicalCandidateCountV2>& qualifications)
{
    const auto expected = semantic::BuildSparseCpuReferenceOutputV1(prepared.set);
    std::array<base::Digest256, CanonicalCandidateCountV2> digests{};
    for (std::size_t candidateIndex = 0; candidateIndex < prepared.candidates.size(); ++candidateIndex)
    {
        const auto outputBytes = ReadbackBytes(
            prepared.candidates[candidateIndex].outputReadback.Get(), gpu.zeroBytes.size());
        auto& sample = qualifications[candidateIndex];
        sample.outputDigest = base::Sha256(outputBytes);
        digests[candidateIndex] = sample.outputDigest;
        std::vector<spiral5::semantic::PointRecordV1> observed(semantic::WorkUniverseCountV1);
        std::memcpy(observed.data(), outputBytes.data(), outputBytes.size());
        for (std::uint32_t index = 0; index < semantic::WorkUniverseCountV1; ++index)
        {
            const auto offset = static_cast<std::size_t>(index) * sizeof(spiral5::semantic::PointRecordV1);
            if (!prepared.set.Contains(index))
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
            const std::array<float, 4> actualValues{actual.x, actual.y, actual.z, actual.w};
            const std::array<float, 4> expectedValues{reference.x, reference.y, reference.z, reference.w};
            bool mismatch = false;
            for (std::size_t component = 0; component < actualValues.size(); ++component)
            {
                if (!std::isfinite(actualValues[component])) ++sample.nonFiniteCount;
                const double absolute = std::abs(
                    static_cast<double>(actualValues[component]) - expectedValues[component]);
                const double denominator = std::max(
                    std::abs(static_cast<double>(expectedValues[component])), 1.0e-30);
                sample.maxAbsoluteError = std::max(sample.maxAbsoluteError, absolute);
                sample.maxRelativeError = std::max(sample.maxRelativeError, absolute / denominator);
                sample.maxUlpError = std::max(sample.maxUlpError,
                    UlpDistance(actualValues[component], expectedValues[component]));
                if (absolute > 1.0e-6) mismatch = true;
            }
            if (mismatch) ++sample.activeMismatchCount;
            if (std::bit_cast<std::uint32_t>(actual.w) != std::bit_cast<std::uint32_t>(1.0f))
                ++sample.homogeneousCoordinateMismatchCount;
        }
        if (sample.activeWriteCount != prepared.activeCount || sample.missingWriteCount != 0 ||
            sample.unauthorizedWriteCount != 0 || sample.activeMismatchCount != 0 ||
            sample.nonFiniteCount != 0 || sample.homogeneousCoordinateMismatchCount != 0)
        {
            std::ostringstream message;
            message << "Batched real-GPU output failed exact write-set qualification."
                << " Pattern=" << PatternNameV2(prepared.pattern)
                << " K=" << prepared.activeCount
                << " Candidate=" << CandidateLetterV2(
                    prepared.candidates[candidateIndex].authority.raw.kind)
                << " activeObserved=" << sample.activeWriteCount
                << " missing=" << sample.missingWriteCount
                << " unauthorized=" << sample.unauthorizedWriteCount
                << " numericMismatch=" << sample.activeMismatchCount
                << " nonFinite=" << sample.nonFiniteCount
                << " homogeneousMismatch=" << sample.homogeneousCoordinateMismatchCount
                << " maxAbs=" << sample.maxAbsoluteError
                << " maxRel=" << sample.maxRelativeError
                << " maxUlp=" << sample.maxUlpError;
            throw std::runtime_error(message.str());
        }
    }
    if (!(digests[0] == digests[1] && digests[1] == digests[2]))
        throw std::runtime_error("A/B/C output digests are not byte-identical.");
}

std::string EvaluateStability(
    const MeasurementConfigV2& config,
    bool nvidiaAdapter,
    bool nvmlAvailable,
    const NvmlSnapshotV2& pre,
    const NvmlSnapshotV2& post,
    const BlockTelemetrySummaryV2& summary,
    double controlRatio)
{
    (void)nvidiaAdapter;
    (void)nvmlAvailable;
    (void)pre;
    (void)post;
    // CU6 V2 is a block-local paired experiment. Vendor P-state, GpuIdle and
    // absolute clock values are diagnostics, not acceptance conditions.
    if (!std::isfinite(controlRatio) || controlRatio > config.maximumControlRatio)
        return "FixedControlNonStationaryInsidePairedBlock";
    // Hardware thermal or power-brake signals are retained as diagnostics. The
    // fixed control is the portable authority for material within-block drift.
    (void)summary;
    return {};
}

BlockAttemptV2 ExecuteBlockAttempt(
    DeviceContext& gpu,
    nvml_dynamic::Runtime& nvml,
    PreparedCase& prepared,
    const MeasurementConfigV2& config,
    const semantic::SparsePointTransformSemanticV1& semanticValue,
    const fv::SparseFamilyVerificationContextV1& context,
    std::uint32_t run,
    std::uint32_t caseOrder,
    std::uint32_t attemptIndex)
{
    BlockAttemptV2 attempt;
    attempt.run = run;
    attempt.caseOrder = caseOrder;
    attempt.attempt = attemptIndex;
    attempt.pattern = prepared.pattern;
    attempt.activeCount = prepared.activeCount;
    attempt.sparseSetIdentity = prepared.set.Identity();
    attempt.activeBlockCount = prepared.candidates[2].authority.artifact.ActiveBlockCount();
    for (std::size_t index = 0; index < prepared.candidates.size(); ++index)
        attempt.authorities[index] = ToEvidenceAuthority(prepared.candidates[index].authority);

    for (const auto& candidate : prepared.candidates)
    {
        auto valid = fe::ValidateFrozenVerifiedSparseFamilyCandidateV1(
            candidate.authority.frozen, semanticValue, prepared.set, context, 1);
        if (!valid) throw std::runtime_error(valid.Error());
    }

    const std::uint32_t orderRotation =
        (run + caseOrder + attemptIndex) % CanonicalOrderCountV2;
    const auto recorded = RecordMeasurementBlock(gpu, prepared, config, orderRotation);
    gpu.RecordWarmup(config.warmupIterations);
    gpu.SubmitAndWait(gpu.warmupList.Get());

    const auto telemetryOrigin = Clock::now();
    const bool diagnosticTelemetry = config.enableNvmlDiagnostic && nvml.Available();
    const auto preInternal = diagnosticTelemetry
        ? nvml.Capture(telemetryOrigin, caseOrder)
        : nvml_dynamic::Snapshot{};
    attempt.preBlockTelemetry = ToPublicSnapshot(preInternal, diagnosticTelemetry);
    if (gpu.profile.vendorId == 0x10deu && diagnosticTelemetry)
    {
        const auto d3dName = NormalizeAdapterName(gpu.profile.description);
        const auto nvmlName = NormalizeAdapterName(nvml.DeviceName());
        if (!d3dName.empty() && !nvmlName.empty() &&
            d3dName.find(nvmlName) == std::string::npos && nvmlName.find(d3dName) == std::string::npos)
            throw std::runtime_error("NVML device name does not match selected D3D12 adapter.");
    }

    nvml_dynamic::Poller poller(
        diagnosticTelemetry ? &nvml : nullptr,
        telemetryOrigin,
        config.nvmlPollingIntervalMilliseconds);
    poller.SetOrdinal(caseOrder);
    const auto mainBegin = Clock::now();
    gpu.SubmitAndWait(gpu.mainList.Get());
    const auto mainEnd = Clock::now();
    auto timeline = poller.StopAndTake();
    const auto beginNanoseconds = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(mainBegin - telemetryOrigin).count());
    const auto endNanoseconds = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(mainEnd - telemetryOrigin).count());
    timeline.erase(std::remove_if(timeline.begin(), timeline.end(), [&](const auto& value)
    {
        return value.elapsedNanoseconds < beginNanoseconds || value.elapsedNanoseconds > endNanoseconds;
    }), timeline.end());
    const auto postInternal = diagnosticTelemetry
        ? nvml.Capture(telemetryOrigin, caseOrder)
        : nvml_dynamic::Snapshot{};
    attempt.postBlockTelemetry = ToPublicSnapshot(postInternal, diagnosticTelemetry);
    attempt.telemetrySummary = SummarizeTelemetry(
        timeline, attempt.preBlockTelemetry, attempt.postBlockTelemetry);

    const auto timestampBytes = ReadbackBytes(gpu.queryReadback.Get(),
        sizeof(std::uint64_t) * recorded.queryCount);
    std::vector<std::uint64_t> timestamps(recorded.queryCount);
    std::memcpy(timestamps.data(), timestampBytes.data(), timestampBytes.size());
    for (std::size_t index = 1; index < timestamps.size(); ++index)
        if (timestamps[index] < timestamps[index - 1])
            throw std::runtime_error("GPU timestamps are not monotonic.");
    const double tickToNanoseconds = 1.0e9 / static_cast<double>(gpu.timestampFrequency);
    const auto phase = [&](std::uint32_t begin, std::uint32_t end)
    {
        return static_cast<double>(timestamps[end] - timestamps[begin]) * tickToNanoseconds;
    };
    attempt.controlBeforeNanosecondsPerIteration =
        phase(recorded.controlBeforeBegin, recorded.controlBeforeEnd) / config.iterationsPerBatch;
    attempt.controlAfterNanosecondsPerIteration =
        phase(recorded.controlAfterBegin, recorded.controlAfterEnd) / config.iterationsPerBatch;
    const double minimumControl = std::min(
        attempt.controlBeforeNanosecondsPerIteration, attempt.controlAfterNanosecondsPerIteration);
    const double maximumControl = std::max(
        attempt.controlBeforeNanosecondsPerIteration, attempt.controlAfterNanosecondsPerIteration);
    attempt.controlRatio = minimumControl > 0.0 ? maximumControl / minimumControl :
        std::numeric_limits<double>::infinity();
    attempt.controlReferenceNanosecondsPerIteration =
        std::sqrt(attempt.controlBeforeNanosecondsPerIteration *
            attempt.controlAfterNanosecondsPerIteration);

    std::array<TimingSampleV2, CanonicalCandidateCountV2> qualifications{};
    const auto kinds = CanonicalCandidateKindsV2();
    for (std::size_t index = 0; index < kinds.size(); ++index)
    {
        qualifications[index].candidate = kinds[index];
        qualifications[index].pattern = prepared.pattern;
        qualifications[index].activeCount = prepared.activeCount;
        qualifications[index].activeBlockCount = attempt.activeBlockCount;
        qualifications[index].expectedDispatchGroupsX =
            prepared.candidates[index].authority.artifact.DispatchGroupsX();
    }
    ValidateOutputs(gpu, prepared, qualifications);

    const std::string rejection = EvaluateStability(
        config, gpu.profile.vendorId == 0x10deu, nvml.Available(),
        attempt.preBlockTelemetry, attempt.postBlockTelemetry,
        attempt.telemetrySummary, attempt.controlRatio);
    if (!rejection.empty())
    {
        attempt.accepted = false;
        attempt.rejectionReason = rejection;
        return attempt;
    }

    attempt.accepted = true;
    attempt.samples.reserve(recorded.samples.size());
    for (const auto& value : recorded.samples)
    {
        TimingSampleV2 sample = qualifications[value.candidateIndex];
        sample.run = run;
        sample.caseOrder = caseOrder;
        sample.acceptedAttempt = attemptIndex;
        sample.orderIndex = value.orderIndex;
        sample.cycle = value.cycle;
        sample.orderPosition = value.orderPosition;
        sample.iterationCount = config.iterationsPerBatch;
        sample.gpuBatchNanoseconds = phase(value.queryBegin, value.queryEnd);
        sample.gpuNanosecondsPerIteration = sample.gpuBatchNanoseconds / config.iterationsPerBatch;
        sample.blockControlNanosecondsPerIteration =
            attempt.controlReferenceNanosecondsPerIteration;
        sample.controlNormalizedTime =
            sample.gpuNanosecondsPerIteration / sample.blockControlNanosecondsPerIteration;
        attempt.samples.push_back(sample);
    }
    return attempt;
}

std::vector<PreparedCase> BuildAllPreparedCases(
    DeviceContext& gpu,
    const semantic::SparsePointTransformSemanticV1& semanticValue,
    const fv::SparseFamilyVerificationContextV1& context)
{
    std::vector<PreparedCase> result;
    result.reserve(CanonicalPatternCountV2 * CanonicalMeasurementCardinalityCountV2);
    std::map<std::string, std::array<base::Digest256, CanonicalCandidateCountV2>> exactSetAuthorities;
    for (PatternKindV2 pattern : CanonicalPatternsV2())
    {
        for (std::uint32_t activeCount : CanonicalMeasurementCardinalitiesV2())
        {
            auto prepared = BuildPreparedCase(gpu, pattern, activeCount, semanticValue, context);
            std::array<base::Digest256, CanonicalCandidateCountV2> artifactIdentities{};
            for (std::size_t index = 0; index < prepared.candidates.size(); ++index)
                artifactIdentities[index] = prepared.candidates[index].authority.artifact.ArtifactIdentity();
            const std::string setKey = base::ToHex(prepared.set.Identity());
            auto [iterator, inserted] = exactSetAuthorities.emplace(setKey, artifactIdentities);
            if (!inserted && iterator->second != artifactIdentities)
                throw std::runtime_error("Equal Exact Set produced unequal Candidate Artifact identities.");
            result.push_back(std::move(prepared));
        }
    }
    return result;
}

PreparedCase& FindPreparedCase(
    std::vector<PreparedCase>& values,
    PatternKindV2 pattern,
    std::uint32_t activeCount)
{
    const auto found = std::find_if(values.begin(), values.end(), [&](const PreparedCase& value)
    {
        return value.pattern == pattern && value.activeCount == activeCount;
    });
    if (found == values.end()) throw std::runtime_error("Prepared Pattern/K case not found.");
    return *found;
}
#endif
}

base::Result<MeasurementEvidenceV2, std::string>
RunRealGpuMeasurementV2(const MeasurementConfigV2& config)
{
#if !defined(_WIN32)
    (void)config;
    return base::Result<MeasurementEvidenceV2, std::string>::Failure(
        "Spiral 6 CU6 V2 real-GPU measurement requires Windows D3D12.");
#else
    try
    {
        (void)BuildMeasurementProfileIdentityV2(config);
        const std::uint32_t sampleCount =
            CanonicalOrderCountV2 * config.measurementCyclesPerOrder * CanonicalCandidateCountV2;
        const std::uint32_t maximumQueries = 4u + sampleCount * 2u;
        DeviceContext gpu(config, maximumQueries);
        nvml_dynamic::Runtime nvml(config.nvmlDeviceIndex, false);

        const auto semanticValue = semantic::BuildCanonicalSparsePointTransformSemanticV1();
        const auto context = fv::BuildCanonicalSparseFamilyVerificationContextV1();
        auto preparedCases = BuildAllPreparedCases(gpu, semanticValue, context);

        MeasurementEvidenceV2 evidence;
        evidence.captureUnixNanoseconds = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        evidence.config = config;
        evidence.adapter = gpu.profile;
        evidence.nvmlAvailable = config.enableNvmlDiagnostic && nvml.Available();
        evidence.nvmlDeviceName = evidence.nvmlAvailable ? nvml.DeviceName() : std::string{};
        evidence.nvmlDeviceUuid = evidence.nvmlAvailable ? nvml.DeviceUuid() : std::string{};
        evidence.stablePowerStateApplied = gpu.stablePowerStateApplied;
        evidence.architectureEvidenceIdentity = DigestFromHex(
            "4a634317e988e3f3f9639f249a35af0ce25e18e19ea52a0500ce75ab09674b7e");
        evidence.controlledRecoveryEvidenceIdentity = DigestFromHex(
            "65aec68060dd68ae42a191e79c5cf2fffc8538442b8d6c029fe59927709bd77f");
        evidence.freshRematerializationEvidenceIdentity = DigestFromHex(
            "a9398025d0c5235e2370bfd5d0c34bdfcdc114836a763f8b6151bd95d80ff032");
        evidence.semanticIdentity = semantic::SparsePointTransformSemanticIdentityV1(semanticValue);
        evidence.verificationContextIdentity = ContextIdentity(context);
        evidence.measurementProfileIdentity = BuildMeasurementProfileIdentityV2(config);
        evidence.caseScheduleIdentity = BuildCaseScheduleIdentityV2(config.runCount);
        evidence.candidateOrderIdentity = BuildCandidateOrderIdentityV2();
        evidence.candidates = gpu.BuildCandidateProfiles();

        for (std::uint32_t run = 0; run < config.runCount; ++run)
        {
            const auto schedule = CanonicalBalancedCaseScheduleV2(run);
            for (std::uint32_t caseOrder = 0; caseOrder < schedule.size(); ++caseOrder)
            {
                const auto [pattern, activeCount] = schedule[caseOrder];
                auto& prepared = FindPreparedCase(preparedCases, pattern, activeCount);
                bool accepted = false;
                for (std::uint32_t attemptIndex = 0;
                    attemptIndex < config.maximumBlockAttempts; ++attemptIndex)
                {
                    auto attempt = ExecuteBlockAttempt(
                        gpu, nvml, prepared, config, semanticValue, context,
                        run, caseOrder, attemptIndex);
                    accepted = attempt.accepted;
                    evidence.attempts.push_back(std::move(attempt));
                    if (accepted) break;
                }
                if (!accepted)
                {
                    std::ostringstream message;
                    message << "No stable measurement block was accepted for Pattern="
                        << PatternNameV2(pattern) << " K=" << activeCount
                        << " run=" << run << ".";
                    throw std::runtime_error(message.str());
                }
            }
        }
        return base::Result<MeasurementEvidenceV2, std::string>::Success(std::move(evidence));
    }
    catch (const std::exception& error)
    {
        return base::Result<MeasurementEvidenceV2, std::string>::Failure(error.what());
    }
#endif
}
}
