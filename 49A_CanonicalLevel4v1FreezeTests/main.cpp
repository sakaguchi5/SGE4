#include "../48_CanonicalCompositionRuntimeTests/RuntimeFixture.h"

#include "../00_Foundation/FileIO.h"
#include "../00_Foundation/Sha256.h"
#include "../15_PlatformWin32/Win32Window.h"
#include "../23_CompositionRecovery/CompositionRecovery.h"

#include <array>
#include <filesystem>
#include <iostream>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace
{
namespace fixture = sge4::qualification::canonical_runtime_fixture;
namespace runtime = sge4::composition::canonical::runtime_v1;

bool WriteText(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    return static_cast<bool>(sge4::base::WriteAllBytes(
        path, std::as_bytes(std::span<const char>(text.data(), text.size()))));
}

int BuildEvidence(std::string& manifest)
{
    auto artifact = fixture::BuildDiamondArtifact();
    if (!artifact) return 1;
    const auto inputId = fixture::FindResourceFlow(artifact.Value(), "f9/diamond/input");
    const auto outputId = fixture::FindResourceFlow(artifact.Value(), "f9/diamond/output");
    if (!inputId.IsValid() || !outputId.IsValid()) return 2;
    sge4::d3d12::D3D12Backend backend({true, false});
    runtime::StaticCompositionLoadInput input;
    input.initialResources = {{inputId, fixture::Bytes({1.0f, 2.0f, 3.0f, 4.0f})}};
    auto loaded = runtime::LoadStaticComposition(artifact.Value(), backend, std::move(input));
    if (!loaded) return 3;
    const auto leafCount = loaded.Value().LeafCount();
    const auto resourceCount = loaded.Value().ResourceCount();
    const auto epoch0 = loaded.Value().DeviceEpoch();
    auto submit0 = runtime::SubmitStaticComposition(loaded.Value(), {0, {}});
    auto read0 = runtime::ReadStaticCompositionBuffer(loaded.Value(), outputId);
    if (!submit0 || !read0) return 4;
    auto recover = runtime::RecoverStaticComposition(
        loaded.Value(), sge4::runtime::DeviceRecoveryMode::ControlledRebuild);
    auto submit1 = runtime::SubmitStaticComposition(loaded.Value(), {1, {}});
    auto read1 = runtime::ReadStaticCompositionBuffer(loaded.Value(), outputId);
    if (!recover || !submit1 || !read1 || read0.Value().bytes != read1.Value().bytes) return 5;

    auto removalArtifact = fixture::BuildLinearArtifact();
    if (!removalArtifact) return 6;
    const auto removalInputId = fixture::FindResourceFlow(removalArtifact.Value(), "f9/linear/input");
    sge4::d3d12::D3D12Backend removalBackend({true, false});
    runtime::StaticCompositionLoadInput removalInput;
    removalInput.initialResources = {{removalInputId, fixture::Bytes({1.0f, 2.0f, 3.0f, 4.0f})}};
    auto removalLoaded = runtime::LoadStaticComposition(
        removalArtifact.Value(), removalBackend, std::move(removalInput));
    if (!removalLoaded || !runtime::SubmitStaticComposition(removalLoaded.Value(), {0, {}})) return 7;
    auto removed = runtime::RecoverStaticComposition(
        removalLoaded.Value(), sge4::runtime::DeviceRecoveryMode::ForceRemovalForTest);
    if (!removed) return 8;
    auto retry = runtime::RecoverStaticComposition(
        removalLoaded.Value(), sge4::runtime::DeviceRecoveryMode::RetryAdapterReacquisition);
    if (!retry) return 9;

    std::ostringstream out;
    out << "identity=SGE4-Level4v1-Canonical-R5-Final-Qualification-v1\n";
    out << "frozen-composition-sha256=" << sge4::base::ToHex(sge4::base::Sha256(artifact.Value())) << '\n';
    out << "pre-recovery-output-sha256=" << sge4::base::ToHex(sge4::base::Sha256(read0.Value().bytes)) << '\n';
    out << "post-recovery-output-sha256=" << sge4::base::ToHex(sge4::base::Sha256(read1.Value().bytes)) << '\n';
    out << "leaf-count=" << leafCount << '\n';
    out << "resource-count=" << resourceCount << '\n';
    out << "controlled-epoch-delta=" << (loaded.Value().DeviceEpoch() - epoch0) << '\n';
    out << "controlled-rematerialized=" << (recover.Value().allRuntimeObjectsRematerialized ? 1 : 0) << '\n';
    out << "actual-removal-forced=" << (removed.Value().device.forcedRemoval ? 1 : 0) << '\n';
    out << "actual-removal-awaiting=" << (removed.Value().device.stateAfter == sge4::runtime::DeviceRuntimeState::AwaitingAdapter ? 1 : 0) << '\n';
    out << "retry-awaiting=" << (retry.Value().device.stateAfter == sge4::runtime::DeviceRuntimeState::AwaitingAdapter ? 1 : 0) << '\n';
    const auto body = out.str();
    out << "evidence-aggregate=" << sge4::base::ToHex(sge4::base::Sha256(
        std::as_bytes(std::span<const char>(body.data(), body.size())))) << '\n';
    manifest = out.str();
    return 0;
}
}

int main(int argc, char** argv)
{
    std::string manifest;
    const int result = BuildEvidence(manifest);
    if (result != 0) { std::cerr << "R5 evidence qualification failed: " << result << '\n'; return result; }
    if (argc == 3 && std::string_view(argv[1]) == "--emit")
        if (!WriteText(argv[2], manifest)) return 20;
    std::cout << "Canonical R5 final qualification evidence passed\n";
    std::cout << manifest;
    return 0;
}
