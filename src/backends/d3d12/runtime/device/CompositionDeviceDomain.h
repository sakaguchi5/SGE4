#pragma once
#include "../../../../composition/artifact/VerifiedCompositionArtifact.h"
#include "../RuntimeTypes.h"

#include "../../../../canonical/base/Expected.h"
#include "../../../../runtime/core/package/PackageRuntime.h"
#include "../../executor/Executor.h"
#include "../../../../composition/verifier/CompositionVerifier.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace sge4::d3d12::runtime_detail
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
    [[nodiscard]] ::sge4::runtime::DeviceRuntimeState State() const noexcept;
    [[nodiscard]] std::size_t LeafCount() const noexcept { return instances_.size(); }
    [[nodiscard]] const artifact::VerifiedFrozenComposition& Artifact() const noexcept { return artifact_; }
    [[nodiscard]] ::sge4::runtime::IPackageInstance* Instance(LeafPackageId leaf) noexcept;
    [[nodiscard]] const ::sge4::runtime::IPackageInstance* Instance(LeafPackageId leaf) const noexcept;
    [[nodiscard]] const package::FrozenExecutablePackage* BasePackage(LeafPackageId leaf) const noexcept;
    [[nodiscard]] d3d12::Executor& Backend() noexcept { return *backend_; }
    [[nodiscard]] ::sge4::runtime::IPackageDeviceDomain& NativeDomain() noexcept { return *domain_; }

    void ClearLeafInstances() noexcept;
    [[nodiscard]] base::Expected<void, DomainError> RematerializeLeaves();
    [[nodiscard]] base::Expected<::sge4::runtime::DeviceRecoveryReport, DomainError> RecoverNativeDomain(
        ::sge4::runtime::DeviceRecoveryMode mode);

private:
    friend base::Expected<SharedDeviceDomain, DomainError> MaterializeSharedDeviceDomain(
        artifact::VerifiedFrozenComposition, d3d12::Executor&, ::sge4::runtime::ISurfaceHost*);

    SharedDeviceDomain(
        artifact::VerifiedFrozenComposition artifact,
        d3d12::Executor& backend,
        ::sge4::runtime::ISurfaceHost* surface)
        : artifact_(std::move(artifact)), backend_(&backend), surface_(surface) {}

    artifact::VerifiedFrozenComposition artifact_;
    d3d12::Executor* backend_ = nullptr;
    ::sge4::runtime::ISurfaceHost* surface_ = nullptr;
    std::unique_ptr<::sge4::runtime::IPackageDeviceDomain> domain_;
    std::vector<std::shared_ptr<const package::FrozenExecutablePackage>> basePackages_;
    std::vector<std::unique_ptr<::sge4::runtime::IPackageInstance>> instances_;
};

[[nodiscard]] base::Expected<SharedDeviceDomain, DomainError> MaterializeSharedDeviceDomain(
    artifact::VerifiedFrozenComposition artifact,
    d3d12::Executor& backend,
    ::sge4::runtime::ISurfaceHost* surface = nullptr);
}
