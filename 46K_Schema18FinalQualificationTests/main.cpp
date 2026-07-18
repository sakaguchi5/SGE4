#include "../46G_Schema18SharedDeviceDomainTests/RuntimeFixture.h"
#include "../00_Foundation/FileIO.h"
#include "../00_Foundation/Sha256.h"
#include "../15_PlatformWin32/Win32Window.h"
#include "../29A_Schema18StaticDagRuntime/Schema18StaticDagRuntime.h"
#include "../29B_Schema18CompositionRecovery/Schema18CompositionRecovery.h"

#include <array>
#include <filesystem>
#include <iostream>
#include <span>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
namespace fixture = sge4::qualification::schema18_runtime_fixture;
namespace runtime18 = sge4::composition::schema18::runtime_v1;

bool WriteText(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    return static_cast<bool>(sge4::base::WriteAllBytes(
        path, std::as_bytes(std::span<const char>(text.data(), text.size()))));
}

struct QualificationEvidence final
{
    std::string frozenDigest;
    std::string outputDigest;
    std::uint64_t initialEpoch = 0;
    std::uint64_t controlledEpoch = 0;
    std::uint64_t actualLossEpoch = 0;
    std::uint64_t retryEpoch = 0;
    std::size_t leavesWhileAwaiting = 0;
    std::size_t resourcesWhileAwaiting = 0;
    std::size_t presenterLeaves = 0;
};

int RunPresenter(QualificationEvidence& evidence)
{
    auto artifact = fixture::BuildPresenterArtifact();
    if (!artifact) return 20;
    const auto inputId = fixture::FindResourceFlow(artifact.Value(), "f9/present/input");
    if (!inputId.IsValid()) return 21;
    auto window = sge4::platform::Win32Window::Create(
        L"SGE4 L4V1 F11 Recovery Qualification", 64, 64);
    if (!window) return 22;
    sge4::d3d12::D3D12Backend backend({true, false});
    runtime18::StaticCompositionLoadInput input;
    input.surface = window.Value().get();
    input.initialResources = {{inputId, fixture::Bytes({0.2f,0.4f,0.8f,1.0f})}};
    auto loaded = runtime18::LoadStaticComposition(
        artifact.Value().fileBytes, backend, std::move(input));
    if (!loaded) return 23;
    auto first = runtime18::SubmitStaticComposition(loaded.Value(), {0,{}});
    if (!first || first.Value().leaves.size() != 2) return 24;
    auto recovered = runtime18::RecoverStaticComposition(
        loaded.Value(), sge4::runtime::DeviceRecoveryMode::ControlledRebuild);
    if (!recovered || !recovered.Value().resourcesRematerialized ||
        recovered.Value().leafCountAfter != 2) return 25;
    auto second = runtime18::SubmitStaticComposition(loaded.Value(), {1,{}});
    if (!second || second.Value().leaves.size() != 2) return 26;
    evidence.presenterLeaves = second.Value().leaves.size();
    return 0;
}

int RunDiamond(QualificationEvidence& evidence)
{
    auto artifact = fixture::BuildDiamondArtifact();
    if (!artifact) return 1;
    const auto inputId = fixture::FindResourceFlow(artifact.Value(), "f9/diamond/input");
    const auto outputId = fixture::FindResourceFlow(artifact.Value(), "f9/diamond/output");
    if (!inputId.IsValid() || !outputId.IsValid()) return 2;
    const std::array<float, 4> input{1,2,3,4};
    const std::array<float, 4> expected{7,9,11,13};
    sge4::d3d12::D3D12Backend backend({true, false});
    runtime18::StaticCompositionLoadInput loadInput;
    loadInput.initialResources = {{inputId, fixture::Bytes(input)}};
    auto loaded = runtime18::LoadStaticComposition(
        artifact.Value().fileBytes, backend, std::move(loadInput));
    if (!loaded) return 3;
    evidence.initialEpoch = loaded.Value().DeviceEpoch();
    evidence.frozenDigest = sge4::base::ToHex(
        sge4::base::Sha256(artifact.Value().fileBytes));

    auto first = runtime18::SubmitStaticComposition(loaded.Value(), {0,{}});
    auto firstRead = runtime18::ReadStaticCompositionBuffer(loaded.Value(), outputId);
    if (!first || !firstRead || !fixture::Equals(firstRead.Value().bytes, expected)) return 4;
    evidence.outputDigest = sge4::base::ToHex(
        sge4::base::Sha256(firstRead.Value().bytes));

    auto controlled = runtime18::RecoverStaticComposition(
        loaded.Value(), sge4::runtime::DeviceRecoveryMode::ControlledRebuild);
    if (!controlled || !controlled.Value().resourcesRematerialized ||
        controlled.Value().leafCountAfter != 4 || controlled.Value().resourceCountAfter != 5) return 5;
    evidence.controlledEpoch = loaded.Value().DeviceEpoch();
    auto second = runtime18::SubmitStaticComposition(loaded.Value(), {1,{}});
    auto secondRead = runtime18::ReadStaticCompositionBuffer(loaded.Value(), outputId);
    if (!second || !secondRead || !fixture::Equals(secondRead.Value().bytes, expected)) return 6;

    auto removed = runtime18::RecoverStaticComposition(
        loaded.Value(), sge4::runtime::DeviceRecoveryMode::ForceRemovalForTest);
    if (!removed || removed.Value().device.stateAfter !=
        sge4::runtime::DeviceRuntimeState::AwaitingAdapter ||
        removed.Value().device.adapterReacquired || removed.Value().resourcesRematerialized) return 7;
    evidence.actualLossEpoch = loaded.Value().DeviceEpoch();

    auto retry = runtime18::RecoverStaticComposition(
        loaded.Value(), sge4::runtime::DeviceRecoveryMode::RetryAdapterReacquisition);
    if (!retry || retry.Value().device.stateAfter !=
        sge4::runtime::DeviceRuntimeState::AwaitingAdapter ||
        retry.Value().device.adapterReacquired || retry.Value().device.packageObjectsRebuilt ||
        retry.Value().resourcesRematerialized || retry.Value().leafCountAfter != 0 ||
        retry.Value().resourceCountAfter != 0 ||
        loaded.Value().DeviceEpoch() != evidence.actualLossEpoch) return 8;
    evidence.retryEpoch = loaded.Value().DeviceEpoch();
    evidence.leavesWhileAwaiting = retry.Value().leafCountAfter;
    evidence.resourcesWhileAwaiting = retry.Value().resourceCountAfter;

    auto inactiveSubmit = runtime18::SubmitStaticComposition(loaded.Value(), {2,{}});
    if (inactiveSubmit || inactiveSubmit.Error().stage != "static-runtime/device-state") return 9;
    return 0;
}
}

int wmain(int argc, wchar_t** argv)
{
    if (argc != 3 || std::wstring_view(argv[1]) != L"--emit") return 100;
    QualificationEvidence evidence;
    // The presenter qualification must finish before actual RemoveDevice.  WARP's
    // removed adapter remains excluded for the rest of this process by design.
    int result = RunPresenter(evidence);
    if (result != 0) return result;
    result = RunDiamond(evidence);
    if (result != 0) return result;

    std::ostringstream text;
    text << "SGE4-L4V1-F10-F11-EVIDENCE-V2\n";
    text << "schema=18\nexecution-runtime=17\n";
    text << "frozen-composition-sha256=" << evidence.frozenDigest << '\n';
    text << "post-controlled-recovery-output-sha256=" << evidence.outputDigest << '\n';
    text << "initial-epoch=" << evidence.initialEpoch << '\n';
    text << "controlled-epoch=" << evidence.controlledEpoch << '\n';
    text << "actual-loss-epoch=" << evidence.actualLossEpoch << '\n';
    text << "same-topology-retry-epoch=" << evidence.retryEpoch << '\n';
    text << "leaves-while-awaiting=" << evidence.leavesWhileAwaiting << '\n';
    text << "resources-while-awaiting=" << evidence.resourcesWhileAwaiting << '\n';
    text << "presenter-leaves-after-controlled-recovery=" << evidence.presenterLeaves << '\n';
    text << "controlled-whole-domain-rematerialization=passed\n";
    text << "actual-remove-device=passed\n";
    text << "removed-adapter-exclusion=passed\n";
    text << "same-topology-retry-awaiting-adapter=passed\n";
    text << "awaiting-adapter-submit-rejection=passed\n";
    text << "stale-epoch-invalidation=passed\n";
    text << "post-controlled-recovery-dag=passed\n";
    text << "single-presenter-controlled-recovery=passed\n";
    text << "fresh-process-frozen-rematerialization=passed\n";
    return WriteText(argv[2], text.str()) ? 0 : 101;
}
