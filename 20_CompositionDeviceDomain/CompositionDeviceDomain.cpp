#include "CompositionDeviceDomain.h"

#include "../09_FrozenPackageCore/PackageReader.h"

#include <algorithm>
#include <utility>

namespace sge4_5::composition::canonical::runtime_v1
{
namespace
{
DomainError Error(std::string stage, std::string message) { return {std::move(stage), std::move(message)}; }
DomainError Error(const ::sge4_5::runtime::RuntimeError& value) { return {value.stage, value.message}; }
template<class T> base::Result<T, DomainError> Failure(std::string stage, std::string message)
{
    return base::Result<T, DomainError>::Failure(Error(std::move(stage), std::move(message)));
}
}

std::uint64_t SharedDeviceDomain::DeviceEpoch() const noexcept { return domain_ ? domain_->DeviceEpoch() : 0; }
::sge4_5::runtime::DeviceRuntimeState SharedDeviceDomain::State() const noexcept
{
    return domain_ ? domain_->State() : ::sge4_5::runtime::DeviceRuntimeState::Lost;
}
::sge4_5::runtime::IPackageInstance* SharedDeviceDomain::Instance(LeafPackageId leaf) noexcept
{
    return leaf.value < instances_.size() ? instances_[leaf.value].get() : nullptr;
}
const ::sge4_5::runtime::IPackageInstance* SharedDeviceDomain::Instance(LeafPackageId leaf) const noexcept
{
    return leaf.value < instances_.size() ? instances_[leaf.value].get() : nullptr;
}
const package::FrozenExecutablePackage* SharedDeviceDomain::BasePackage(LeafPackageId leaf) const noexcept
{
    return leaf.value < basePackages_.size() ? basePackages_[leaf.value].get() : nullptr;
}

void SharedDeviceDomain::ClearLeafInstances() noexcept
{
    instances_.clear();
    basePackages_.clear();
}

base::Result<void, DomainError> SharedDeviceDomain::RematerializeLeaves()
{
    if (!backend_ || !domain_ || State() != ::sge4_5::runtime::DeviceRuntimeState::Active || DeviceEpoch() == 0)
        return base::Result<void, DomainError>::Failure(Error("domain/rematerialize", "DeviceDomain is not Active"));

    auto authoritative = verification::ReadVerifiedFrozenComposition(artifact_.Artifact().FileBytes());
    if (!authoritative)
        return base::Result<void, DomainError>::Failure(Error(
            "domain/frozen-read", authoritative.Error().stage + ": " + authoritative.Error().message));
    artifact_ = std::move(authoritative).Value();

    const auto& validated = artifact_.ValidatedContract();
    const auto& contract = validated.Contract();
    const auto& plan = artifact_.VerifiedPlan().Plan();
    if (contract.leaves.empty() || contract.leaves.size() != plan.schedule.size())
        return base::Result<void, DomainError>::Failure(Error(
            "domain/contract", "Frozen Composition Leaf and schedule counts disagree"));
    if (contract.presenterLeaf.IsValid() && surface_ == nullptr)
        return base::Result<void, DomainError>::Failure(Error(
            "domain/surface", "the presenter Leaf requires an ISurfaceHost"));

    ClearLeafInstances();
    basePackages_.resize(contract.leaves.size());
    instances_.resize(contract.leaves.size());
    std::vector<bool> seen(contract.leaves.size(), false);
    const auto epoch = DeviceEpoch();

    for (const auto& leaf : contract.leaves)
    {
        if (leaf.id.value >= contract.leaves.size() || seen[leaf.id.value])
        {
            ClearLeafInstances();
            return base::Result<void, DomainError>::Failure(Error(
                "domain/leaf-id", "Leaf IDs are not canonical and dense"));
        }
        seen[leaf.id.value] = true;
        const auto& canonicalLeaf = validated.Leaves()[leaf.id.value];
        auto packageResult = package::PackageReader::Read(canonicalLeaf.packageBytes);
        if (!packageResult)
        {
            ClearLeafInstances();
            return base::Result<void, DomainError>::Failure(Error(
                "domain/leaf-package", packageResult.Error().message));
        }
        if (packageResult.Value().Header().targetSchemaVersion != 17 ||
            packageResult.Value().Header().minimumRuntimeVersion != 17)
        {
            ClearLeafInstances();
            return base::Result<void, DomainError>::Failure(Error(
                "domain/leaf-schema", "Leaf is not Schema 17 / Runtime 17"));
        }
        auto shared = std::make_shared<const package::FrozenExecutablePackage>(
            std::move(packageResult).Value());
        ::sge4_5::runtime::ISurfaceHost* leafSurface =
            contract.presenterLeaf.IsValid() && contract.presenterLeaf == leaf.id ? surface_ : nullptr;
        if (leaf.surfaceSlotCount != 0 && leafSurface == nullptr)
        {
            ClearLeafInstances();
            return base::Result<void, DomainError>::Failure(Error(
                "domain/surface", "surface-bearing Leaf is not the presenter"));
        }
        if (leaf.surfaceSlotCount == 0 && leafSurface != nullptr)
        {
            ClearLeafInstances();
            return base::Result<void, DomainError>::Failure(Error(
                "domain/surface", "presenter has no Surface slot"));
        }
        auto instance = backend_->LoadIntoDomain(*domain_, shared, leafSurface);
        if (!instance)
        {
            ClearLeafInstances();
            return base::Result<void, DomainError>::Failure(Error(instance.Error()));
        }
        if (DeviceEpoch() != epoch || State() != ::sge4_5::runtime::DeviceRuntimeState::Active)
        {
            ClearLeafInstances();
            return base::Result<void, DomainError>::Failure(Error(
                "domain/epoch", "Leaf loading changed DeviceDomain epoch or state"));
        }
        basePackages_[leaf.id.value] = std::move(shared);
        instances_[leaf.id.value] = std::move(instance).Value();
    }
    if (std::ranges::any_of(instances_, [](const auto& value) { return !value; }))
    {
        ClearLeafInstances();
        return base::Result<void, DomainError>::Failure(Error(
            "domain/leaves", "one or more verified Leaves were not materialized"));
    }
    return base::Result<void, DomainError>::Success();
}

base::Result<::sge4_5::runtime::DeviceRecoveryReport, DomainError>
SharedDeviceDomain::RecoverNativeDomain(::sge4_5::runtime::DeviceRecoveryMode mode)
{
    if (!backend_ || !domain_)
        return Failure<::sge4_5::runtime::DeviceRecoveryReport>(
            "domain/recovery", "DeviceDomain is unavailable");
    auto recovered = backend_->RecoverDeviceDomain(*domain_, mode);
    if (!recovered)
        return base::Result<::sge4_5::runtime::DeviceRecoveryReport, DomainError>::Failure(
            Error(recovered.Error()));
    return base::Result<::sge4_5::runtime::DeviceRecoveryReport, DomainError>::Success(
        std::move(recovered).Value());
}

base::Result<SharedDeviceDomain, DomainError> MaterializeSharedDeviceDomain(
    verification::VerifiedFrozenComposition artifact,
    d3d12::D3D12Backend& backend,
    ::sge4_5::runtime::ISurfaceHost* surface)
{
    SharedDeviceDomain loaded(std::move(artifact), backend, surface);
    auto domain = backend.CreateDeviceDomain();
    if (!domain) return base::Result<SharedDeviceDomain, DomainError>::Failure(Error(domain.Error()));
    loaded.domain_ = std::move(domain).Value();
    if (loaded.DeviceEpoch() == 0 || loaded.State() != ::sge4_5::runtime::DeviceRuntimeState::Active)
        return Failure<SharedDeviceDomain>("domain/create", "new DeviceDomain is not Active with a non-zero epoch");
    auto leaves = loaded.RematerializeLeaves();
    if (!leaves) return base::Result<SharedDeviceDomain, DomainError>::Failure(leaves.Error());
    return base::Result<SharedDeviceDomain, DomainError>::Success(std::move(loaded));
}
}
