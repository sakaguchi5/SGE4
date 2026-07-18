#include "RuntimeFixture.h"

#include "../13A_Schema18SharedDeviceDomain/Schema18SharedDeviceDomain.h"
#include "../14_D3D12Backend/D3D12Backend.h"

#include <iostream>

int main()
{
    namespace fixture = sge4::qualification::schema18_runtime_fixture;
    namespace runtime18 = sge4::composition::schema18::runtime_v1;

    auto artifact = fixture::BuildDiamondArtifact();
    if (!artifact)
    {
        std::cerr << artifact.Error() << '\n';
        return 1;
    }
    sge4::d3d12::D3D12Backend backend({true, false});
    auto domain = runtime18::MaterializeSharedDeviceDomain(
        artifact.Value(), backend, nullptr);
    if (!domain)
    {
        std::cerr << domain.Error().stage << ": " << domain.Error().message << '\n';
        return 2;
    }
    if (domain.Value().State() != sge4::runtime::DeviceRuntimeState::Active ||
        domain.Value().DeviceEpoch() == 0 || domain.Value().LeafCount() != 4)
        return 3;
    for (const auto& leaf : domain.Value().Artifact().contract.leaves)
    {
        const auto* instance = domain.Value().Instance(leaf.id);
        const auto* basePackage = domain.Value().BasePackage(leaf.id);
        if (!instance || !basePackage ||
            basePackage->Header().targetSchemaVersion != 17 ||
            basePackage->Header().minimumRuntimeVersion != 17)
            return 4;
    }

    auto presenter = fixture::BuildPresenterArtifact();
    if (!presenter) return 5;
    auto missingSurface = runtime18::MaterializeSharedDeviceDomain(
        presenter.Value(), backend, nullptr);
    if (missingSurface || missingSurface.Error().stage != "domain/surface") return 6;

    auto corrupt = artifact.Value();
    corrupt.fileBytes[corrupt.fileBytes.size() / 2] ^= std::byte{0x01};
    auto corruptDomain = runtime18::MaterializeSharedDeviceDomain(
        std::move(corrupt), backend, nullptr);
    if (corruptDomain || corruptDomain.Error().stage != "domain/frozen-read") return 7;

    std::cout << "L4V1-F7 Shared DeviceDomain tests passed\n";
    std::cout << "Leaves: 4 Schema-18 authorities, 4 embedded Schema-17 execution packages\n";
    std::cout << "DeviceDomain epoch: " << domain.Value().DeviceEpoch() << '\n';
    std::cout << "Authority: one Active D3D12 DeviceDomain owns every Leaf instance\n";
    return 0;
}
