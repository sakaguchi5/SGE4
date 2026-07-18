#include "Schema18SharedDeviceDomain.h"

#include "../09_FrozenPackageCore/PackageReader.h"
#include "../10A_D3D12CompositionSchema18/D3D12CompositionEncoding18.h"

#include <algorithm>
#include <utility>

namespace sge4::composition::schema18::runtime_v1
{
namespace
{
DomainError Error(std::string stage, std::string message)
{
    return {std::move(stage), std::move(message)};
}

DomainError Error(const runtime::RuntimeError& value)
{
    return {value.stage, value.message};
}

template<class T>
base::Result<T, DomainError> Failure(std::string stage, std::string message)
{
    return base::Result<T, DomainError>::Failure(
        Error(std::move(stage), std::move(message)));
}
}

std::uint64_t SharedDeviceDomain::DeviceEpoch() const noexcept
{
    return domain_ ? domain_->DeviceEpoch() : 0;
}

runtime::DeviceRuntimeState SharedDeviceDomain::State() const noexcept
{
    return domain_ ? domain_->State() : runtime::DeviceRuntimeState::Lost;
}

runtime::IPackageInstance* SharedDeviceDomain::Instance(LeafPackageId leaf) noexcept
{
    return leaf.value < instances_.size() ? instances_[leaf.value].get() : nullptr;
}

const runtime::IPackageInstance* SharedDeviceDomain::Instance(LeafPackageId leaf) const noexcept
{
    return leaf.value < instances_.size() ? instances_[leaf.value].get() : nullptr;
}

const package::FrozenExecutablePackage* SharedDeviceDomain::BasePackage(
    LeafPackageId leaf) const noexcept
{
    return leaf.value < basePackages_.size() ? basePackages_[leaf.value].get() : nullptr;
}

void SharedDeviceDomain::DiscardLeavesForRecovery() noexcept
{
    // Instance destruction retires its Package queues before native Domain loss.
    instances_.clear();
    basePackages_.clear();
}

base::Result<void, DomainError> SharedDeviceDomain::MaterializeLeaves()
{
    if (!backend_ || !domain_ || domain_->State() != runtime::DeviceRuntimeState::Active ||
        domain_->DeviceEpoch() == 0)
        return base::Result<void, DomainError>::Failure(
            Error("domain/materialize", "DeviceDomain is not Active with a non-zero epoch"));

    const auto& contract = artifact_.contract;
    if (contract.leaves.empty() || contract.leaves.size() != artifact_.plan.schedule.size())
        return base::Result<void, DomainError>::Failure(
            Error("domain/contract", "Frozen Composition Leaf and schedule counts disagree"));
    if (contract.presenterLeaf.IsValid() && surface_ == nullptr)
        return base::Result<void, DomainError>::Failure(
            Error("domain/surface", "the single presenter Leaf requires an ISurfaceHost"));

    basePackages_.resize(contract.leaves.size());
    instances_.resize(contract.leaves.size());
    std::vector<bool> seen(contract.leaves.size(), false);
    const auto epoch = domain_->DeviceEpoch();

    for (const auto& leaf : contract.leaves)
    {
        if (leaf.id.value >= contract.leaves.size() || seen[leaf.id.value])
            return base::Result<void, DomainError>::Failure(
                Error("domain/leaf-id", "Leaf IDs are not a canonical dense identity space"));
        seen[leaf.id.value] = true;

        auto schema18Package = package::PackageReader::Read(leaf.packageBytes);
        if (!schema18Package)
            return base::Result<void, DomainError>::Failure(
                Error("domain/leaf-schema18", schema18Package.Error().message));
        auto compositionView = package::d3d12_v18::CompositionLeafView::Decode(
            schema18Package.Value());
        if (!compositionView)
            return base::Result<void, DomainError>::Failure(
                Error("domain/leaf-interface", compositionView.Error().message));
        auto schema17Package = package::PackageReader::Read(
            compositionView.Value().Schema17BasePackageBytes());
        if (!schema17Package)
            return base::Result<void, DomainError>::Failure(
                Error("domain/leaf-schema17", schema17Package.Error().message));
        if (schema17Package.Value().Header().targetSchemaVersion != 17 ||
            schema17Package.Value().Header().minimumRuntimeVersion != 17)
            return base::Result<void, DomainError>::Failure(
                Error("domain/leaf-schema17", "embedded execution package is not Schema 17 / Runtime 17"));

        auto shared = std::make_shared<const package::FrozenExecutablePackage>(
            std::move(schema17Package).Value());
        runtime::ISurfaceHost* leafSurface =
            contract.presenterLeaf.IsValid() && contract.presenterLeaf == leaf.id
                ? surface_ : nullptr;
        if (leaf.surfaceSlotCount != 0 && leafSurface == nullptr)
            return base::Result<void, DomainError>::Failure(
                Error("domain/surface", "a surface-bearing Leaf is not the authoritative presenter"));
        if (leaf.surfaceSlotCount == 0 && leafSurface != nullptr)
            return base::Result<void, DomainError>::Failure(
                Error("domain/surface", "the authoritative presenter has no Surface slot"));

        auto instance = backend_->LoadIntoDomain(*domain_, shared, leafSurface);
        if (!instance)
            return base::Result<void, DomainError>::Failure(Error(instance.Error()));
        if (domain_->DeviceEpoch() != epoch ||
            domain_->State() != runtime::DeviceRuntimeState::Active)
            return base::Result<void, DomainError>::Failure(
                Error("domain/epoch", "Leaf loading changed the shared DeviceDomain epoch or state"));
        basePackages_[leaf.id.value] = std::move(shared);
        instances_[leaf.id.value] = std::move(instance).Value();
    }

    if (std::ranges::any_of(instances_, [](const auto& value) { return !value; }))
        return base::Result<void, DomainError>::Failure(
            Error("domain/leaves", "one or more verified Leaves were not materialized"));
    return base::Result<void, DomainError>::Success();
}

base::Result<runtime::DeviceRecoveryReport, DomainError>
SharedDeviceDomain::Recover(runtime::DeviceRecoveryMode mode)
{
    if (!backend_ || !domain_)
        return Failure<runtime::DeviceRecoveryReport>(
            "domain/recovery", "Shared DeviceDomain is not materialized");

    const auto oldEpoch = domain_->DeviceEpoch();
    DiscardLeavesForRecovery();
    auto recovered = backend_->RecoverDeviceDomain(*domain_, mode);
    if (!recovered)
        return base::Result<runtime::DeviceRecoveryReport, DomainError>::Failure(
            Error(recovered.Error()));

    auto report = std::move(recovered).Value();
    if (report.previousDeviceEpoch != oldEpoch ||
        report.newDeviceEpoch != domain_->DeviceEpoch() ||
        report.stateAfter != domain_->State())
        return Failure<runtime::DeviceRecoveryReport>(
            "domain/recovery-report", "native DeviceDomain recovery report is inconsistent");

    if (report.stateAfter != runtime::DeviceRuntimeState::Active)
    {
        if (report.packageObjectsRebuilt || !instances_.empty() || !basePackages_.empty())
            return Failure<runtime::DeviceRecoveryReport>(
                "domain/recovery-state", "inactive recovery retained or claimed rebuilt Leaf objects");
        return base::Result<runtime::DeviceRecoveryReport, DomainError>::Success(
            std::move(report));
    }

    if (domain_->DeviceEpoch() <= oldEpoch)
        return Failure<runtime::DeviceRecoveryReport>(
            "domain/recovery-epoch", "Active recovery did not advance the DeviceDomain epoch");
    auto materialized = MaterializeLeaves();
    if (!materialized)
        return base::Result<runtime::DeviceRecoveryReport, DomainError>::Failure(
            materialized.Error());
    report.packageObjectsRebuilt = true;
    report.temporalHistoryReset = true;
    report.externalRebindRequired = true;
    return base::Result<runtime::DeviceRecoveryReport, DomainError>::Success(
        std::move(report));
}

base::Result<SharedDeviceDomain, DomainError> MaterializeSharedDeviceDomain(
    linking::FrozenCompositionArtifact artifact,
    d3d12::D3D12Backend& backend,
    runtime::ISurfaceHost* surface)
{
    // Re-read the bytes so callers cannot manufacture a trusted artifact struct.
    auto authoritative = linking::ReadFrozenComposition(artifact.fileBytes);
    if (!authoritative)
        return Failure<SharedDeviceDomain>(
            "domain/frozen-read",
            authoritative.Error().stage + ": " + authoritative.Error().message);
    artifact = std::move(authoritative).Value();

    SharedDeviceDomain loaded(std::move(artifact), backend, surface);
    auto domain = backend.CreateDeviceDomain();
    if (!domain)
        return base::Result<SharedDeviceDomain, DomainError>::Failure(Error(domain.Error()));
    loaded.domain_ = std::move(domain).Value();
    auto materialized = loaded.MaterializeLeaves();
    if (!materialized)
        return base::Result<SharedDeviceDomain, DomainError>::Failure(
            materialized.Error());
    return base::Result<SharedDeviceDomain, DomainError>::Success(std::move(loaded));
}
}
