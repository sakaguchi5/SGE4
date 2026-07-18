#pragma once

#include "../00_Foundation/Result.h"
#include "../10A_D3D12CompositionSchema18/D3D12CompositionSchema18.h"
#include "../13_PackageRuntime/PackageRuntime.h"
#include "../14_D3D12Backend/D3D12Backend.h"
#include "../19A_Schema18CompositionLinker/Schema18CompositionLinker.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace sge4::composition::schema18::runtime_v1
{
struct DomainError final
{
    std::string stage;
    std::string message;
};

class SharedDeviceDomain final
{
public:
    SharedDeviceDomain(SharedDeviceDomain&&) noexcept = default;
    SharedDeviceDomain& operator=(SharedDeviceDomain&&) noexcept = default;
    SharedDeviceDomain(const SharedDeviceDomain&) = delete;
    SharedDeviceDomain& operator=(const SharedDeviceDomain&) = delete;

    [[nodiscard]] std::uint64_t DeviceEpoch() const noexcept;
    [[nodiscard]] runtime::DeviceRuntimeState State() const noexcept;
    [[nodiscard]] std::size_t LeafCount() const noexcept { return instances_.size(); }
    [[nodiscard]] const linking::FrozenCompositionArtifact& Artifact() const noexcept
    {
        return artifact_;
    }
    [[nodiscard]] runtime::IPackageInstance* Instance(LeafPackageId leaf) noexcept;
    [[nodiscard]] const runtime::IPackageInstance* Instance(LeafPackageId leaf) const noexcept;
    [[nodiscard]] const package::FrozenExecutablePackage* BasePackage(LeafPackageId leaf) const noexcept;

    // Internal execution boundary used by F8/F9. Ownership remains here.
    [[nodiscard]] d3d12::D3D12Backend& Backend() noexcept { return *backend_; }
    [[nodiscard]] runtime::IPackageDeviceDomain& NativeDomain() noexcept { return *domain_; }

private:
    friend base::Result<SharedDeviceDomain, DomainError> MaterializeSharedDeviceDomain(
        linking::FrozenCompositionArtifact,
        d3d12::D3D12Backend&,
        runtime::ISurfaceHost*);

    SharedDeviceDomain(linking::FrozenCompositionArtifact artifact,
                       d3d12::D3D12Backend& backend)
        : artifact_(std::move(artifact)), backend_(&backend) {}

    linking::FrozenCompositionArtifact artifact_;
    d3d12::D3D12Backend* backend_ = nullptr;
    std::unique_ptr<runtime::IPackageDeviceDomain> domain_;
    std::vector<std::shared_ptr<const package::FrozenExecutablePackage>> basePackages_;
    std::vector<std::unique_ptr<runtime::IPackageInstance>> instances_;
};

[[nodiscard]] base::Result<SharedDeviceDomain, DomainError>
MaterializeSharedDeviceDomain(
    linking::FrozenCompositionArtifact artifact,
    d3d12::D3D12Backend& backend,
    runtime::ISurfaceHost* surface = nullptr);
}
