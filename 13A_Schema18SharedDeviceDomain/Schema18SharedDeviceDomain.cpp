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

    const auto& preflightContract = artifact.contract;
    if (preflightContract.leaves.empty() ||
        preflightContract.leaves.size() != artifact.plan.schedule.size())
        return Failure<SharedDeviceDomain>(
            "domain/contract", "Frozen Composition Leaf and schedule counts disagree");
    if (preflightContract.presenterLeaf.IsValid() && surface == nullptr)
        return Failure<SharedDeviceDomain>(
            "domain/surface", "the single presenter Leaf requires an ISurfaceHost");

    SharedDeviceDomain loaded(std::move(artifact), backend);
    const auto& contract = loaded.artifact_.contract;
    auto domain = backend.CreateDeviceDomain();
    if (!domain)
        return base::Result<SharedDeviceDomain, DomainError>::Failure(Error(domain.Error()));
    loaded.domain_ = std::move(domain).Value();
    const auto epoch = loaded.domain_->DeviceEpoch();
    if (epoch == 0 || loaded.domain_->State() != runtime::DeviceRuntimeState::Active)
        return Failure<SharedDeviceDomain>(
            "domain/create", "new DeviceDomain is not Active with a non-zero epoch");

    loaded.basePackages_.resize(contract.leaves.size());
    loaded.instances_.resize(contract.leaves.size());
    std::vector<bool> seen(contract.leaves.size(), false);

    for (const auto& leaf : contract.leaves)
    {
        if (leaf.id.value >= contract.leaves.size() || seen[leaf.id.value])
            return Failure<SharedDeviceDomain>(
                "domain/leaf-id", "Leaf IDs are not a canonical dense identity space");
        seen[leaf.id.value] = true;

        auto schema18Package = package::PackageReader::Read(leaf.packageBytes);
        if (!schema18Package)
            return Failure<SharedDeviceDomain>(
                "domain/leaf-schema18", schema18Package.Error().message);
        auto compositionView = package::d3d12_v18::CompositionLeafView::Decode(
            schema18Package.Value());
        if (!compositionView)
            return Failure<SharedDeviceDomain>(
                "domain/leaf-interface", compositionView.Error().message);
        auto schema17Package = package::PackageReader::Read(
            compositionView.Value().Schema17BasePackageBytes());
        if (!schema17Package)
            return Failure<SharedDeviceDomain>(
                "domain/leaf-schema17", schema17Package.Error().message);
        if (schema17Package.Value().Header().targetSchemaVersion != 17 ||
            schema17Package.Value().Header().minimumRuntimeVersion != 17)
            return Failure<SharedDeviceDomain>(
                "domain/leaf-schema17", "embedded execution package is not Schema 17 / Runtime 17");

        auto shared = std::make_shared<const package::FrozenExecutablePackage>(
            std::move(schema17Package).Value());
        runtime::ISurfaceHost* leafSurface =
            contract.presenterLeaf.IsValid() && contract.presenterLeaf == leaf.id
                ? surface : nullptr;
        if (leaf.surfaceSlotCount != 0 && leafSurface == nullptr)
            return Failure<SharedDeviceDomain>(
                "domain/surface", "a surface-bearing Leaf is not the authoritative presenter");
        if (leaf.surfaceSlotCount == 0 && leafSurface != nullptr)
            return Failure<SharedDeviceDomain>(
                "domain/surface", "the authoritative presenter has no Surface slot");

        auto instance = backend.LoadIntoDomain(*loaded.domain_, shared, leafSurface);
        if (!instance)
            return base::Result<SharedDeviceDomain, DomainError>::Failure(Error(instance.Error()));
        if (loaded.domain_->DeviceEpoch() != epoch ||
            loaded.domain_->State() != runtime::DeviceRuntimeState::Active)
            return Failure<SharedDeviceDomain>(
                "domain/epoch", "Leaf loading changed the shared DeviceDomain epoch or state");
        loaded.basePackages_[leaf.id.value] = std::move(shared);
        loaded.instances_[leaf.id.value] = std::move(instance).Value();
    }

    if (std::ranges::any_of(loaded.instances_, [](const auto& value) { return !value; }))
        return Failure<SharedDeviceDomain>(
            "domain/leaves", "one or more verified Leaves were not materialized");
    return base::Result<SharedDeviceDomain, DomainError>::Success(std::move(loaded));
}
}
