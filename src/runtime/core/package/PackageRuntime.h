#pragma once

#include "../../../canonical/base/Expected.h"
#include "../../../leaf/artifact/package/FrozenExecutablePackage.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace sge4::runtime
{
struct RuntimeError final
{
    std::string stage;
    std::string message;
};

class IExternalResource
{
public:
    virtual ~IExternalResource() = default;
    [[nodiscard]] virtual std::uint64_t DeviceEpoch() const noexcept = 0;
    [[nodiscard]] virtual std::uint64_t SizeBytes() const noexcept = 0;
};

class ICompletionToken
{
public:
    virtual ~ICompletionToken() = default;
    [[nodiscard]] virtual std::uint64_t DeviceEpoch() const noexcept = 0;
    [[nodiscard]] virtual std::uint64_t Value() const noexcept = 0;
};

struct DynamicDataBinding final
{
    std::uint32_t slot = package::InvalidIndex;
    std::span<const std::byte> bytes;
};

struct ExternalResourceBinding final
{
    std::uint32_t slot = package::InvalidIndex;
    std::shared_ptr<IExternalResource> resource;
    std::shared_ptr<ICompletionToken> availableAfter;
};

// A verified indirect dispatch binding is not an arbitrary Runtime dispatch
// request.  It is the already-sealed result of the Dynamic Planner and the
// independent Dynamic Verifier.  The Executor may only map these fixed values
// to D3D12 ExecuteIndirect; it must not derive or clamp them.
struct VerifiedIndirectDispatchBinding final
{
    std::uint32_t computeCommand = package::InvalidIndex;
    std::uint32_t workCount = 0;
    std::uint32_t threadGroupCountX = 0;
    std::uint32_t threadGroupCountY = 1;
    std::uint32_t threadGroupCountZ = 1;
};

struct FrameInvocation final
{
    std::uint64_t frameNumber = 0;
    std::span<const DynamicDataBinding> dynamicData;
    std::span<const ExternalResourceBinding> externalResources;
    std::span<const VerifiedIndirectDispatchBinding> indirectDispatches;
};

struct QueueCompletion final
{
    std::uint32_t queue = package::InvalidIndex;
    std::uint64_t value = 0;
};

struct ExternalRelease final
{
    std::uint32_t slot = package::InvalidIndex;
    std::shared_ptr<ICompletionToken> safeAfter;
};

enum class DeviceRuntimeState : std::uint8_t
{
    Active = 0,
    Lost = 1,
    AwaitingAdapter = 2
};

enum class DeviceRecoveryMode : std::uint8_t
{
    ControlledRebuild = 0,
    RecoverDetectedLoss = 1,
    ForceRemovalForTest = 2,
    RetryAdapterReacquisition = 3
};

struct DeviceRecoveryReport final
{
    std::uint64_t previousDeviceEpoch = 0;
    std::uint64_t newDeviceEpoch = 0;
    std::int64_t removalReason = 0;
    std::uint32_t dredBreadcrumbNodeCount = 0;
    std::uint32_t dredPageFaultAllocationCount = 0;
    std::uint32_t removedAdapterLuidLow = 0;
    std::int32_t removedAdapterLuidHigh = 0;
    DeviceRecoveryMode mode = DeviceRecoveryMode::ControlledRebuild;
    DeviceRuntimeState stateBefore = DeviceRuntimeState::Active;
    DeviceRuntimeState stateAfter = DeviceRuntimeState::Active;
    bool forcedRemoval = false;
    bool adapterReacquired = false;
    bool packageObjectsRebuilt = false;
    bool temporalHistoryReset = false;
    bool externalRebindRequired = false;
};

struct FrameSubmission final
{
    std::uint64_t deviceEpoch = 0;
    std::uint32_t frameSlot = 0;
    std::uint32_t framesInFlight = 0;
    std::uint64_t reusedSlotFenceValue = 0;
    std::uint32_t temporalPreviousInstance = 0;
    std::uint32_t temporalCurrentInstance = 0;
    std::uint64_t temporalDependencyFenceValue = 0;
    std::vector<QueueCompletion> queues;
    std::vector<ExternalRelease> releasedExternalResources;
};

class ISurfaceHost
{
public:
    virtual ~ISurfaceHost() = default;
    [[nodiscard]] virtual void* NativeWindowHandle() const noexcept = 0;
    [[nodiscard]] virtual std::uint32_t ClientWidth() const noexcept = 0;
    [[nodiscard]] virtual std::uint32_t ClientHeight() const noexcept = 0;
};

class IPackageInstance
{
public:
    virtual ~IPackageInstance() = default;
};

class IPackageDeviceDomain
{
public:
    virtual ~IPackageDeviceDomain() = default;
    [[nodiscard]] virtual std::uint64_t DeviceEpoch() const noexcept = 0;
    [[nodiscard]] virtual DeviceRuntimeState State() const noexcept = 0;
};

class IPackageExecutor
{
public:
    virtual ~IPackageExecutor() = default;
    [[nodiscard]] virtual base::Expected<std::unique_ptr<IPackageInstance>, RuntimeError> Load(
        std::shared_ptr<const package::FrozenExecutablePackage> package,
        ISurfaceHost* surface) = 0;
    [[nodiscard]] virtual base::Expected<FrameSubmission, RuntimeError> Submit(
        IPackageInstance& instance,
        const FrameInvocation& invocation) = 0;
    [[nodiscard]] virtual base::Expected<DeviceRecoveryReport, RuntimeError> RecoverDevice(
        IPackageInstance& instance,
        DeviceRecoveryMode mode) = 0;
};

class LoadedPackage final
{
public:
    LoadedPackage(LoadedPackage&&) noexcept = default;
    LoadedPackage& operator=(LoadedPackage&&) noexcept = default;
    LoadedPackage(const LoadedPackage&) = delete;
    LoadedPackage& operator=(const LoadedPackage&) = delete;

    [[nodiscard]] IPackageInstance& Instance() noexcept { return *instance_; }
    [[nodiscard]] const package::FrozenExecutablePackage& Package() const noexcept { return *package_; }

private:
    friend base::Expected<LoadedPackage, RuntimeError> LoadPackage(
        package::FrozenExecutablePackage,
        IPackageExecutor&,
        ISurfaceHost*);
    LoadedPackage(std::shared_ptr<const package::FrozenExecutablePackage> package, std::unique_ptr<IPackageInstance> instance)
        : package_(std::move(package)), instance_(std::move(instance)) {}
    std::shared_ptr<const package::FrozenExecutablePackage> package_;
    std::unique_ptr<IPackageInstance> instance_;
};

[[nodiscard]] base::Expected<LoadedPackage, RuntimeError> LoadPackage(
    package::FrozenExecutablePackage package,
    IPackageExecutor& executor,
    ISurfaceHost* surface);

[[nodiscard]] inline base::Expected<LoadedPackage, RuntimeError> LoadPackage(
    package::FrozenExecutablePackage package,
    IPackageExecutor& executor,
    ISurfaceHost& surface)
{
    return LoadPackage(std::move(package), executor, &surface);
}

[[nodiscard]] inline base::Expected<LoadedPackage, RuntimeError> LoadPackage(
    package::FrozenExecutablePackage package,
    IPackageExecutor& executor)
{
    return LoadPackage(std::move(package), executor, nullptr);
}

[[nodiscard]] base::Expected<FrameSubmission, RuntimeError> Submit(
    LoadedPackage& loaded,
    IPackageExecutor& executor,
    const FrameInvocation& invocation);

[[nodiscard]] base::Expected<DeviceRecoveryReport, RuntimeError> RecoverDevice(
    LoadedPackage& loaded,
    IPackageExecutor& executor,
    DeviceRecoveryMode mode);
}
