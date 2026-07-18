#pragma once

#include "../10_D3D12PackageSchema/D3D12Encoding.h"
#include "../13_PackageRuntime/PackageRuntime.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace sge4::d3d12
{
struct ExecutorOptions final
{
    bool forceWarp = false;
    bool enableDebugLayer = true;
};


struct ExternalBufferBinding final
{
    std::shared_ptr<runtime::IExternalResource> resource;
    std::shared_ptr<runtime::ICompletionToken> availableAfter;
};

struct ExternalBufferReadback final
{
    std::vector<std::byte> bytes;
    // The observation command restores the Package slot's required incoming
    // state. Reuse this token when rebinding the resource on a later frame.
    std::shared_ptr<runtime::ICompletionToken> availableAfter;
};

class D3D12Backend final : public runtime::IPackageExecutor
{
public:
    explicit D3D12Backend(ExecutorOptions options = {}) : options_(options) {}

    [[nodiscard]] base::Result<std::unique_ptr<runtime::IPackageInstance>, runtime::RuntimeError> Load(
        std::shared_ptr<const package::FrozenExecutablePackage> package,
        runtime::ISurfaceHost* surface) override;

    [[nodiscard]] base::Result<runtime::FrameSubmission, runtime::RuntimeError> Submit(
        runtime::IPackageInstance& instance,
        const runtime::FrameInvocation& invocation) override;

    [[nodiscard]] base::Result<runtime::DeviceRecoveryReport, runtime::RuntimeError> RecoverDevice(
        runtime::IPackageInstance& instance,
        runtime::DeviceRecoveryMode mode) override;

    // Canonical Level 4 v1: one native DeviceDomain owns all Leaf devices,
    // shared Buffers, tokens, and one monotonically increasing device epoch.
    [[nodiscard]] base::Result<std::unique_ptr<runtime::IPackageDeviceDomain>, runtime::RuntimeError> CreateDeviceDomain();

    [[nodiscard]] base::Result<std::unique_ptr<runtime::IPackageInstance>, runtime::RuntimeError> LoadIntoDomain(
        runtime::IPackageDeviceDomain& domain,
        std::shared_ptr<const package::FrozenExecutablePackage> package,
        runtime::ISurfaceHost* surface = nullptr);

    [[nodiscard]] base::Result<ExternalBufferBinding, runtime::RuntimeError> CreateSharedBuffer(
        runtime::IPackageDeviceDomain& domain,
        std::uint32_t resourceIdentity,
        std::uint64_t sizeBytes,
        package::d3d12_v13::ResourceState initialState,
        std::span<const std::byte> initialBytes);

    [[nodiscard]] base::Result<std::shared_ptr<runtime::ICompletionToken>, runtime::RuntimeError> TransitionSharedBuffer(
        runtime::IPackageDeviceDomain& domain,
        const std::shared_ptr<runtime::IExternalResource>& resource,
        const std::shared_ptr<runtime::ICompletionToken>& safeAfter,
        package::d3d12_v13::ResourceState beforeState,
        package::d3d12_v13::ResourceState afterState);

    [[nodiscard]] base::Result<ExternalBufferReadback, runtime::RuntimeError> ReadSharedBuffer(
        runtime::IPackageDeviceDomain& domain,
        const std::shared_ptr<runtime::IExternalResource>& resource,
        const std::shared_ptr<runtime::ICompletionToken>& safeAfter,
        package::d3d12_v13::ResourceState restoreState);

    [[nodiscard]] base::Result<runtime::DeviceRecoveryReport, runtime::RuntimeError> RecoverDeviceDomain(
        runtime::IPackageDeviceDomain& domain,
        runtime::DeviceRecoveryMode mode);

    // Creates one executor-owned external Buffer for the exact Package slot.
    // The resource is initialized from the supplied bytes (zero-filled when the
    // span is shorter than the slot minimum) and returned in requiredIncomingState.
    [[nodiscard]] base::Result<ExternalBufferBinding, runtime::RuntimeError> CreateExternalBuffer(
        runtime::IPackageInstance& instance,
        std::uint32_t slot,
        std::span<const std::byte> initialBytes);

    // Observes an executor-owned external Buffer after its Package release
    // token. The helper performs an explicit copy, restores requiredIncomingState,
    // and returns the token that must precede the next Package acquisition.
    [[nodiscard]] base::Result<ExternalBufferReadback, runtime::RuntimeError> ReadExternalBuffer(
        runtime::IPackageInstance& instance,
        const std::shared_ptr<runtime::IExternalResource>& resource,
        const std::shared_ptr<runtime::ICompletionToken>& safeAfter);

    [[nodiscard]] base::Result<ExternalBufferBinding, runtime::RuntimeError> CreateExternalColorBuffer(
        runtime::IPackageInstance& instance,
        const std::array<float, 4>& color);

    [[nodiscard]] static bool SupportsOperation(package::d3d12_v13::D3D12OperationCode code) noexcept;

private:
    ExecutorOptions options_;
};
}
