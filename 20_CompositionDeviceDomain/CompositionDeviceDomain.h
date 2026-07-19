#pragma once

#include "../00_Foundation/Result.h"
#include "../13_PackageRuntime/PackageRuntime.h"
#include "../14_D3D12Backend/D3D12Backend.h"
#include "../19_CompositionVerifier/CompositionVerifier.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace sge4_5::composition::canonical::runtime_v1
{
struct DomainError final { std::string stage; std::string message; };

class SharedDeviceDomain final
{
public:
    SharedDeviceDomain(SharedDeviceDomain&&) noexcept = default;
    SharedDeviceDomain& operator=(SharedDeviceDomain&&) noexcept = default;
    SharedDeviceDomain(const SharedDeviceDomain&) = delete;
    SharedDeviceDomain& operator=(const SharedDeviceDomain&) = delete;

    [[nodiscard]] std::uint64_t DeviceEpoch() const noexcept;
    [[nodiscard]] ::sge4_5::runtime::DeviceRuntimeState State() const noexcept;
    [[nodiscard]] std::size_t LeafCount() const noexcept { return instances_.size(); }
    [[nodiscard]] const verification::VerifiedFrozenComposition& Artifact() const noexcept { return artifact_; }
    [[nodiscard]] ::sge4_5::runtime::IPackageInstance* Instance(LeafPackageId leaf) noexcept;
    [[nodiscard]] const ::sge4_5::runtime::IPackageInstance* Instance(LeafPackageId leaf) const noexcept;
    [[nodiscard]] const package::FrozenExecutablePackage* BasePackage(LeafPackageId leaf) const noexcept;
    [[nodiscard]] d3d12::D3D12Backend& Backend() noexcept { return *backend_; }
    [[nodiscard]] ::sge4_5::runtime::IPackageDeviceDomain& NativeDomain() noexcept { return *domain_; }

    void ClearLeafInstances() noexcept;
    [[nodiscard]] base::Result<void, DomainError> RematerializeLeaves();
    [[nodiscard]] base::Result<::sge4_5::runtime::DeviceRecoveryReport, DomainError> RecoverNativeDomain(
        ::sge4_5::runtime::DeviceRecoveryMode mode);

private:
    friend base::Result<SharedDeviceDomain, DomainError> MaterializeSharedDeviceDomain(
        verification::VerifiedFrozenComposition, d3d12::D3D12Backend&, ::sge4_5::runtime::ISurfaceHost*);

    SharedDeviceDomain(
        verification::VerifiedFrozenComposition artifact,
        d3d12::D3D12Backend& backend,
        ::sge4_5::runtime::ISurfaceHost* surface)
        : artifact_(std::move(artifact)), backend_(&backend), surface_(surface) {}

    verification::VerifiedFrozenComposition artifact_;
    d3d12::D3D12Backend* backend_ = nullptr;
    ::sge4_5::runtime::ISurfaceHost* surface_ = nullptr;
    std::unique_ptr<::sge4_5::runtime::IPackageDeviceDomain> domain_;
    std::vector<std::shared_ptr<const package::FrozenExecutablePackage>> basePackages_;
    std::vector<std::unique_ptr<::sge4_5::runtime::IPackageInstance>> instances_;
};

[[nodiscard]] base::Result<SharedDeviceDomain, DomainError> MaterializeSharedDeviceDomain(
    verification::VerifiedFrozenComposition artifact,
    d3d12::D3D12Backend& backend,
    ::sge4_5::runtime::ISurfaceHost* surface = nullptr);
}
