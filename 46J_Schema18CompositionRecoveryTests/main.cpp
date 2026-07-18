#include "../46G_Schema18SharedDeviceDomainTests/RuntimeFixture.h"
#include "../29A_Schema18StaticDagRuntime/Schema18StaticDagRuntime.h"
#include "../29B_Schema18CompositionRecovery/Schema18CompositionRecovery.h"

#include <array>
#include <iostream>
#include <memory>
#include <span>
#include <utility>

namespace
{
namespace fixture = sge4::qualification::schema18_runtime_fixture;
namespace runtime18 = sge4::composition::schema18::runtime_v1;

int RunBackendStaleEpochRejection()
{
    auto artifact = fixture::BuildDiamondArtifact();
    if (!artifact) return 30;
    const auto inputId = fixture::FindResourceFlow(artifact.Value(), "f9/diamond/input");
    if (!inputId.IsValid()) return 31;

    const std::array<float, 4> input{1, 2, 3, 4};
    sge4::d3d12::D3D12Backend backend({true, false});
    auto domain = runtime18::MaterializeSharedDeviceDomain(
        artifact.Value(), backend, nullptr);
    if (!domain) return 32;
    const runtime18::SharedResourceInitialData initial{inputId, fixture::Bytes(input)};
    auto resources = runtime18::MaterializeSharedResources(
        domain.Value(), std::span<const runtime18::SharedResourceInitialData>(&initial, 1));
    if (!resources) return 33;
    auto table = std::make_unique<runtime18::SharedResourceTable>(
        std::move(resources).Value());
    const auto* record = table->Record(inputId);
    if (!record || !record->resource || !record->availableAfter) return 34;
    const auto oldResource = record->resource;
    const auto oldToken = record->availableAfter;
    const auto oldState = record->currentState;
    const auto oldEpoch = domain.Value().DeviceEpoch();

    table.reset();
    auto recovered = domain.Value().Recover(
        sge4::runtime::DeviceRecoveryMode::ControlledRebuild);
    if (!recovered || domain.Value().DeviceEpoch() <= oldEpoch) return 35;
    auto staleRead = backend.ReadSharedBuffer(
        domain.Value().NativeDomain(), oldResource, oldToken, oldState);
    if (staleRead || staleRead.Error().stage != "domain/readback") return 36;
    return 0;
}

int RunControlledAndActualRemoval()
{
    auto artifact = fixture::BuildDiamondArtifact();
    if (!artifact) { std::cerr << artifact.Error() << '\n'; return 1; }
    const auto inputId = fixture::FindResourceFlow(artifact.Value(), "f9/diamond/input");
    const auto outputId = fixture::FindResourceFlow(artifact.Value(), "f9/diamond/output");
    if (!inputId.IsValid() || !outputId.IsValid()) return 2;

    const std::array<float, 4> input{1, 2, 3, 4};
    const std::array<float, 4> expected{7, 9, 11, 13};
    sge4::d3d12::D3D12Backend backend({true, false});
    runtime18::StaticCompositionLoadInput loadInput;
    loadInput.initialResources = {{inputId, fixture::Bytes(input)}};
    auto loaded = runtime18::LoadStaticComposition(
        artifact.Value().fileBytes, backend, std::move(loadInput));
    if (!loaded) { std::cerr << loaded.Error().stage << ": " << loaded.Error().message << '\n'; return 3; }

    auto first = runtime18::SubmitStaticComposition(loaded.Value(), {0, {}});
    auto firstRead = runtime18::ReadStaticCompositionBuffer(loaded.Value(), outputId);
    if (!first || !firstRead || !fixture::Equals(firstRead.Value().bytes, expected)) return 4;

    const auto epoch1 = loaded.Value().DeviceEpoch();
    const auto oldResource1 = loaded.Value().SharedResource(outputId);
    const auto oldToken1 = loaded.Value().AvailableAfter(outputId);
    auto controlled = runtime18::RecoverStaticComposition(
        loaded.Value(), sge4::runtime::DeviceRecoveryMode::ControlledRebuild);
    if (!controlled) { std::cerr << controlled.Error().stage << ": " << controlled.Error().message << '\n'; return 5; }
    if (controlled.Value().device.stateAfter != sge4::runtime::DeviceRuntimeState::Active ||
        controlled.Value().device.previousDeviceEpoch != epoch1 ||
        controlled.Value().device.newDeviceEpoch != loaded.Value().DeviceEpoch() ||
        loaded.Value().DeviceEpoch() <= epoch1 ||
        !controlled.Value().device.packageObjectsRebuilt ||
        !controlled.Value().device.temporalHistoryReset ||
        !controlled.Value().device.externalRebindRequired ||
        !controlled.Value().endpointTokensReset ||
        !controlled.Value().resourcesRematerialized ||
        controlled.Value().leafCountAfter != 4 ||
        controlled.Value().resourceCountAfter != 5) return 6;
    if (!oldResource1 || !oldToken1 ||
        oldResource1->DeviceEpoch() == loaded.Value().DeviceEpoch() ||
        oldToken1->DeviceEpoch() == loaded.Value().DeviceEpoch()) return 7;

    auto second = runtime18::SubmitStaticComposition(loaded.Value(), {1, {}});
    auto secondRead = runtime18::ReadStaticCompositionBuffer(loaded.Value(), outputId);
    if (!second || !secondRead || !fixture::Equals(secondRead.Value().bytes, expected)) return 8;

    auto invalidRetry = runtime18::RecoverStaticComposition(
        loaded.Value(), sge4::runtime::DeviceRecoveryMode::RetryAdapterReacquisition);
    if (invalidRetry || invalidRetry.Error().stage != "composition-recovery/retry") return 9;

    const auto epoch2 = loaded.Value().DeviceEpoch();
    const auto oldResource2 = loaded.Value().SharedResource(outputId);
    const auto oldToken2 = loaded.Value().AvailableAfter(outputId);
    auto removed = runtime18::RecoverStaticComposition(
        loaded.Value(), sge4::runtime::DeviceRecoveryMode::ForceRemovalForTest);
    if (!removed) { std::cerr << removed.Error().stage << ": " << removed.Error().message << '\n'; return 10; }
    if (!removed.Value().device.forcedRemoval ||
        removed.Value().device.stateAfter != sge4::runtime::DeviceRuntimeState::AwaitingAdapter ||
        removed.Value().device.newDeviceEpoch != epoch2 ||
        removed.Value().device.adapterReacquired ||
        removed.Value().device.packageObjectsRebuilt ||
        removed.Value().leafCountAfter != 0 ||
        removed.Value().resourceCountAfter != 0 ||
        removed.Value().resourcesRematerialized ||
        !removed.Value().endpointTokensReset ||
        loaded.Value().State() != sge4::runtime::DeviceRuntimeState::AwaitingAdapter ||
        loaded.Value().SharedResource(outputId) || loaded.Value().AvailableAfter(outputId)) return 11;

    auto inactiveSubmit = runtime18::SubmitStaticComposition(loaded.Value(), {2, {}});
    auto inactiveRead = runtime18::ReadStaticCompositionBuffer(loaded.Value(), outputId);
    if (inactiveSubmit || inactiveSubmit.Error().stage != "static-runtime/device-state" ||
        inactiveRead || inactiveRead.Error().stage != "static-runtime/device-state") return 12;

    auto retried = runtime18::RecoverStaticComposition(
        loaded.Value(), sge4::runtime::DeviceRecoveryMode::RetryAdapterReacquisition);
    if (!retried) { std::cerr << retried.Error().stage << ": " << retried.Error().message << '\n'; return 13; }
    // WARP exposes only the removed adapter.  A Retry checks topology but must not
    // clear the removed-LUID exclusion, so the same-topology retry stays AwaitingAdapter.
    if (retried.Value().device.stateBefore != sge4::runtime::DeviceRuntimeState::AwaitingAdapter ||
        retried.Value().device.stateAfter != sge4::runtime::DeviceRuntimeState::AwaitingAdapter ||
        retried.Value().device.adapterReacquired ||
        retried.Value().device.packageObjectsRebuilt ||
        retried.Value().device.newDeviceEpoch != epoch2 ||
        loaded.Value().DeviceEpoch() != epoch2 ||
        retried.Value().resourcesRematerialized ||
        retried.Value().leafCountAfter != 0 || retried.Value().resourceCountAfter != 0 ||
        !retried.Value().endpointTokensReset ||
        loaded.Value().SharedResource(outputId) || loaded.Value().AvailableAfter(outputId))
    {
        std::cerr << "same-topology retry qualification failed: stateAfter="
                  << static_cast<int>(retried.Value().device.stateAfter)
                  << " adapterReacquired=" << retried.Value().device.adapterReacquired
                  << " packageObjectsRebuilt=" << retried.Value().device.packageObjectsRebuilt
                  << " previousEpoch=" << retried.Value().device.previousDeviceEpoch
                  << " reportNewEpoch=" << retried.Value().device.newDeviceEpoch
                  << " loadedEpoch=" << loaded.Value().DeviceEpoch()
                  << " resourcesRematerialized=" << retried.Value().resourcesRematerialized
                  << " leafCount=" << retried.Value().leafCountAfter
                  << " resourceCount=" << retried.Value().resourceCountAfter << '\n';
        return 14;
    }
    if (!oldResource2 || !oldToken2 ||
        oldResource2->DeviceEpoch() != epoch2 || oldToken2->DeviceEpoch() != epoch2) return 15;

    auto stillInactive = runtime18::SubmitStaticComposition(loaded.Value(), {3, {}});
    if (stillInactive || stillInactive.Error().stage != "static-runtime/device-state") return 16;
    return 0;
}
}

int main()
{
    int result = RunBackendStaleEpochRejection();
    if (result != 0) return result;
    result = RunControlledAndActualRemoval();
    if (result != 0) return result;
    std::cout << "Schema-18 controlled whole-domain rematerialization, actual RemoveDevice, removed-LUID exclusion, same-topology AwaitingAdapter retry, stale epoch rejection, and inactive Runtime rejection passed\n";
    return 0;
}
