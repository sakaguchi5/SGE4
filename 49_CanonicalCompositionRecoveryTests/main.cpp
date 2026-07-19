#include "../48_CanonicalCompositionRuntimeTests/RuntimeFixture.h"

#include "../15_PlatformWin32/Win32Window.h"
#include "../23_CompositionRecovery/CompositionRecovery.h"

#include <array>
#include <iostream>
#include <utility>

namespace
{
namespace fixture = sge4_5::qualification::canonical_runtime_fixture;
namespace runtime = sge4_5::composition::canonical::runtime_v1;

int ControlledDiamond()
{
    auto artifact = fixture::BuildDiamondArtifact();
    if (!artifact) return 1;
    const auto inputId = fixture::FindResourceFlow(artifact.Value(), "f9/diamond/input");
    const auto outputId = fixture::FindResourceFlow(artifact.Value(), "f9/diamond/output");
    if (!inputId.IsValid() || !outputId.IsValid()) return 2;
    const std::array<float, 4> input{1.0f, 2.0f, 3.0f, 4.0f};
    const std::array<float, 4> expected{7.0f, 9.0f, 11.0f, 13.0f};

    sge4_5::d3d12::D3D12Backend backend({true, false});
    runtime::StaticCompositionLoadInput loadInput;
    loadInput.initialResources = {{inputId, fixture::Bytes(input)}};
    auto loaded = runtime::LoadStaticComposition(artifact.Value(), backend, std::move(loadInput));
    if (!loaded) return 3;
    auto submitted = runtime::SubmitStaticComposition(loaded.Value(), {0, {}});
    auto before = runtime::ReadStaticCompositionBuffer(loaded.Value(), outputId);
    if (!submitted || !before || !fixture::Equals(before.Value().bytes, expected)) return 4;

    const auto oldEpoch = loaded.Value().DeviceEpoch();
    const auto staleResource = loaded.Value().SharedResource(outputId);
    const auto staleToken = loaded.Value().AvailableAfter(outputId);
    if (!staleResource || !staleToken) return 5;

    auto recovered = runtime::RecoverStaticComposition(
        loaded.Value(), sge4_5::runtime::DeviceRecoveryMode::ControlledRebuild);
    if (!recovered) {
        std::cerr << recovered.Error().stage << ": " << recovered.Error().message << '\n';
        return 6;
    }
    const auto& report = recovered.Value();
    if (loaded.Value().State() != sge4_5::runtime::DeviceRuntimeState::Active ||
        loaded.Value().DeviceEpoch() != oldEpoch + 1 ||
        report.device.previousDeviceEpoch != oldEpoch ||
        report.device.newDeviceEpoch != oldEpoch + 1 ||
        !report.device.adapterReacquired || !report.device.packageObjectsRebuilt ||
        !report.frozenArtifactRevalidated || !report.allRuntimeObjectsReleased ||
        !report.allRuntimeObjectsRematerialized ||
        report.leafCountAfter != 4 || report.resourceCountAfter == 0)
        return 7;

    auto stale = runtime::ValidateStaticCompositionHandleEpoch(
        loaded.Value(), staleResource, staleToken);
    if (stale || stale.Error().stage != "static-runtime/stale-epoch") return 8;

    auto afterSubmit = runtime::SubmitStaticComposition(loaded.Value(), {1, {}});
    auto after = runtime::ReadStaticCompositionBuffer(loaded.Value(), outputId);
    if (!afterSubmit || !after || !fixture::Equals(after.Value().bytes, expected)) return 9;
    auto current = runtime::ValidateStaticCompositionHandleEpoch(
        loaded.Value(), loaded.Value().SharedResource(outputId), loaded.Value().AvailableAfter(outputId));
    if (!current) return 10;
    return 0;
}

int ControlledPresenter()
{
    auto artifact = fixture::BuildPresenterArtifact();
    if (!artifact) return 20;
    const auto inputId = fixture::FindResourceFlow(artifact.Value(), "f9/present/input");
    auto window = sge4_5::platform::Win32Window::Create(
        L"SGE4 Level4v1 Canonical R4 Presenter Recovery", 64, 64);
    if (!inputId.IsValid() || !window) return 21;
    sge4_5::d3d12::D3D12Backend backend({true, false});
    runtime::StaticCompositionLoadInput input;
    input.surface = window.Value().get();
    input.initialResources = {{inputId, fixture::Bytes({0.2f, 0.4f, 0.8f, 1.0f})}};
    auto loaded = runtime::LoadStaticComposition(artifact.Value(), backend, std::move(input));
    if (!loaded) return 22;
    auto before = runtime::SubmitStaticComposition(loaded.Value(), {0, {}});
    auto recovery = runtime::RecoverStaticComposition(
        loaded.Value(), sge4_5::runtime::DeviceRecoveryMode::ControlledRebuild);
    auto after = runtime::SubmitStaticComposition(loaded.Value(), {1, {}});
    if (!before || !recovery || !after || after.Value().leaves.size() != 2) return 23;
    return 0;
}

int ActualRemovalAndAwaitingAdapter()
{
    auto artifact = fixture::BuildLinearArtifact();
    if (!artifact) return 30;
    const auto inputId = fixture::FindResourceFlow(artifact.Value(), "f9/linear/input");
    const auto outputId = fixture::FindResourceFlow(artifact.Value(), "f9/linear/output");
    if (!inputId.IsValid() || !outputId.IsValid()) return 31;
    sge4_5::d3d12::D3D12Backend backend({true, false});
    runtime::StaticCompositionLoadInput input;
    input.initialResources = {{inputId, fixture::Bytes({1.0f, 2.0f, 3.0f, 4.0f})}};
    auto loaded = runtime::LoadStaticComposition(artifact.Value(), backend, std::move(input));
    if (!loaded) return 32;
    auto submitted = runtime::SubmitStaticComposition(loaded.Value(), {0, {}});
    if (!submitted) return 33;
    const auto oldEpoch = loaded.Value().DeviceEpoch();
    const auto staleResource = loaded.Value().SharedResource(outputId);
    const auto staleToken = loaded.Value().AvailableAfter(outputId);

    auto removed = runtime::RecoverStaticComposition(
        loaded.Value(), sge4_5::runtime::DeviceRecoveryMode::ForceRemovalForTest);
    if (!removed) {
        std::cerr << removed.Error().stage << ": " << removed.Error().message << '\n';
        return 34;
    }
    if (!removed.Value().device.forcedRemoval ||
        removed.Value().device.previousDeviceEpoch != oldEpoch ||
        loaded.Value().State() != sge4_5::runtime::DeviceRuntimeState::AwaitingAdapter ||
        loaded.Value().DeviceEpoch() != oldEpoch ||
        loaded.Value().LeafCount() != 0 || loaded.Value().ResourceCount() != 0 ||
        !removed.Value().allRuntimeObjectsReleased ||
        removed.Value().allRuntimeObjectsRematerialized)
        return 35;

    auto stale = runtime::ValidateStaticCompositionHandleEpoch(
        loaded.Value(), staleResource, staleToken);
    if (stale || stale.Error().stage != "static-runtime/device-state") return 36;
    auto submitRejected = runtime::SubmitStaticComposition(loaded.Value(), {1, {}});
    if (submitRejected || submitRejected.Error().stage != "static-runtime/device-state") return 37;

    auto retry = runtime::RecoverStaticComposition(
        loaded.Value(), sge4_5::runtime::DeviceRecoveryMode::RetryAdapterReacquisition);
    if (!retry || loaded.Value().State() != sge4_5::runtime::DeviceRuntimeState::AwaitingAdapter ||
        loaded.Value().DeviceEpoch() != oldEpoch || loaded.Value().LeafCount() != 0 ||
        loaded.Value().ResourceCount() != 0 || retry.Value().device.adapterReacquired)
        return 38;
    return 0;
}
}

int main()
{
    const int controlled = ControlledDiamond();
    if (controlled != 0) { std::cerr << "controlled recovery failed: " << controlled << '\n'; return controlled; }
    const int presenter = ControlledPresenter();
    if (presenter != 0) { std::cerr << "presenter recovery failed: " << presenter << '\n'; return presenter; }
    const int removal = ActualRemovalAndAwaitingAdapter();
    if (removal != 0) { std::cerr << "actual removal recovery failed: " << removal << '\n'; return removal; }
    std::cout << "Canonical R4 whole-composition recovery tests passed\n";
    std::cout << "Controlled rebuild: full Frozen Composition rematerialization and stale-epoch rejection\n";
    std::cout << "Actual RemoveDevice: removed WARP LUID exclusion, AwaitingAdapter, and retry preservation\n";
    return 0;
}
