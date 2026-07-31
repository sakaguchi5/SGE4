#include "./CompositionDeviceDomain.h"

#include "../../../../leaf/artifact/package/PackageReader.h"

#include <algorithm>
#include <utility>

namespace sge4::d3d12::runtime_detail
{
namespace
{
DomainError Error(std::string stage, std::string message) { return {std::move(stage), std::move(message)}; }
DomainError Error(const ::sge4::runtime::RuntimeError& value) { return {value.stage, value.message}; }
template<class T> base::Expected<T, DomainError> Failure(std::string stage, std::string message)
{
    return base::Failure<T, DomainError>(Error(std::move(stage), std::move(message)));
}
}

std::uint64_t SharedDeviceDomain::DeviceEpoch() const noexcept { return domain_ ? domain_->DeviceEpoch() : 0; }
::sge4::runtime::DeviceRuntimeState SharedDeviceDomain::State() const noexcept
{
    return domain_ ? domain_->State() : ::sge4::runtime::DeviceRuntimeState::Lost;
}
::sge4::runtime::IPackageInstance* SharedDeviceDomain::Instance(LeafPackageId leaf) noexcept
{
    return leaf.value < instances_.size() ? instances_[leaf.value].get() : nullptr;
}
const ::sge4::runtime::IPackageInstance* SharedDeviceDomain::Instance(LeafPackageId leaf) const noexcept
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

base::Expected<void, DomainError> SharedDeviceDomain::RematerializeLeaves()
{
    if (!backend_ || !domain_ || State() != ::sge4::runtime::DeviceRuntimeState::Active || DeviceEpoch() == 0)
        return base::Failure<void, DomainError>(Error("domain/rematerialize", "Deviceが検証または実行の契約に違反しています。"));

    auto authoritative = artifact::ReadVerifiedFrozenComposition(artifact_.FileBytes());
    if (!authoritative)
        return base::Failure<void, DomainError>(Error(
            "domain/frozen-read", authoritative.error().stage + "：" + authoritative.error().message));
    artifact_ = std::move(authoritative).value();

    const auto& validated = artifact_.ValidatedContract();
    const auto& contract = validated.Contract();
    const auto& plan = artifact_.VerifiedPlan().Plan();
    if (contract.leaves.empty() || contract.leaves.size() != plan.schedule.size())
        return base::Failure<void, DomainError>(Error(
            "domain/contract", "検証または実行の契約に違反しています。"));
    if (contract.presenterLeaf.IsValid() && surface_ == nullptr)
        return base::Failure<void, DomainError>(Error(
            "domain/surface", "Leafが検証または実行の契約に違反しています。"));

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
            return base::Failure<void, DomainError>(Error(
                "domain/leaf-id", "LeafがCanonicalな順序または識別子規則に違反しています。"));
        }
        seen[leaf.id.value] = true;
        const auto& canonicalLeaf = validated.Leaves()[leaf.id.value];
        auto packageResult = package::PackageReader::Read(canonicalLeaf.packageBytes);
        if (!packageResult)
        {
            ClearLeafInstances();
            return base::Failure<void, DomainError>(Error(
                "domain/leaf-package", packageResult.error().message));
        }
        if (packageResult.value().Header().targetSchemaVersion != 17 ||
            packageResult.value().Header().minimumRuntimeVersion != 17)
        {
            ClearLeafInstances();
            return base::Failure<void, DomainError>(Error(
                "domain/leaf-schema", "検証または実行の契約に違反しています。"));
        }
        auto shared = std::make_shared<const package::FrozenExecutablePackage>(
            std::move(packageResult).value());
        ::sge4::runtime::ISurfaceHost* leafSurface =
            contract.presenterLeaf.IsValid() && contract.presenterLeaf == leaf.id ? surface_ : nullptr;
        if (leaf.surfaceSlotCount != 0 && leafSurface == nullptr)
        {
            ClearLeafInstances();
            return base::Failure<void, DomainError>(Error(
                "domain/surface", "検証または実行の契約に違反しています。"));
        }
        if (leaf.surfaceSlotCount == 0 && leafSurface != nullptr)
        {
            ClearLeafInstances();
            return base::Failure<void, DomainError>(Error(
                "domain/surface", "検証または実行の契約に違反しています。"));
        }
        auto instance = backend_->LoadIntoDomain(*domain_, shared, leafSurface);
        if (!instance)
        {
            ClearLeafInstances();
            return base::Failure<void, DomainError>(Error(instance.error()));
        }
        if (DeviceEpoch() != epoch || State() != ::sge4::runtime::DeviceRuntimeState::Active)
        {
            ClearLeafInstances();
            return base::Failure<void, DomainError>(Error(
                "domain/epoch", "検証または実行の契約に違反しています。"));
        }
        basePackages_[leaf.id.value] = std::move(shared);
        instances_[leaf.id.value] = std::move(instance).value();
    }
    if (std::ranges::any_of(instances_, [](const auto& value) { return !value; }))
    {
        ClearLeafInstances();
        return base::Failure<void, DomainError>(Error(
            "domain/leaves", "検証または実行の契約に違反しています。"));
    }
    return base::Success<void, DomainError>();
}

base::Expected<::sge4::runtime::DeviceRecoveryReport, DomainError>
SharedDeviceDomain::RecoverNativeDomain(::sge4::runtime::DeviceRecoveryMode mode)
{
    if (!backend_ || !domain_)
        return Failure<::sge4::runtime::DeviceRecoveryReport>(
            "domain/recovery", "Deviceが検証または実行の契約に違反しています。");
    auto recovered = backend_->RecoverDeviceDomain(*domain_, mode);
    if (!recovered)
        return base::Failure<::sge4::runtime::DeviceRecoveryReport, DomainError>(
            Error(recovered.error()));
    return base::Success<::sge4::runtime::DeviceRecoveryReport, DomainError>(
        std::move(recovered).value());
}

base::Expected<SharedDeviceDomain, DomainError> MaterializeSharedDeviceDomain(
    artifact::VerifiedFrozenComposition artifact,
    d3d12::Executor& backend,
    ::sge4::runtime::ISurfaceHost* surface)
{
    SharedDeviceDomain loaded(std::move(artifact), backend, surface);
    auto domain = backend.CreateDeviceDomain();
    if (!domain) return base::Failure<SharedDeviceDomain, DomainError>(Error(domain.error()));
    loaded.domain_ = std::move(domain).value();
    if (loaded.DeviceEpoch() == 0 || loaded.State() != ::sge4::runtime::DeviceRuntimeState::Active)
        return Failure<SharedDeviceDomain>("domain/create", "Deviceが検証または実行の契約に違反しています。");
    auto leaves = loaded.RematerializeLeaves();
    if (!leaves) return base::Failure<SharedDeviceDomain, DomainError>(leaves.error());
    return base::Success<SharedDeviceDomain, DomainError>(std::move(loaded));
}
}
