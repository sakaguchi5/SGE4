#include "RuntimeFixture.h"

#include "../00_Foundation/FileIO.h"
#include "../00_Foundation/Sha256.h"
#include "../15_PlatformWin32/Win32Window.h"
#include "../22_CompositionRuntime/CompositionRuntime.h"

#include <array>
#include <filesystem>
#include <iostream>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
namespace fixture = sge4::qualification::canonical_runtime_fixture;
namespace runtime18 = sge4::composition::canonical::runtime_v1;
namespace contract = sge4::composition::canonical;

struct Evidence final
{
    std::vector<std::byte> diamondOutput;
    std::vector<std::byte> linearOutput;
    std::vector<std::byte> fanoutLeft;
    std::vector<std::byte> fanoutRight;
    std::vector<std::byte> multiInputOutput;
    std::vector<std::byte> independentA;
    std::vector<std::byte> independentB;
    std::size_t diamondLeaves = 0;
    std::size_t linearLeaves = 0;
    std::size_t fanoutLeaves = 0;
    std::size_t multiInputLeaves = 0;
    std::size_t independentLeaves = 0;
    std::size_t presenterLeaves = 0;
};

bool WriteText(const std::filesystem::path& path, std::string_view text)
{
    std::filesystem::create_directories(path.parent_path());
    return static_cast<bool>(sge4::base::WriteAllBytes(
        path, std::as_bytes(std::span<const char>(text.data(), text.size()))));
}

int RunDiamond(Evidence& evidence)
{
    auto artifact = fixture::BuildDiamondArtifact();
    if (!artifact) { std::cerr << artifact.Error() << '\n'; return 1; }
    const auto inputId = fixture::FindResourceFlow(artifact.Value(), "f9/diamond/input");
    const auto outputId = fixture::FindResourceFlow(artifact.Value(), "f9/diamond/output");
    if (!inputId.IsValid() || !outputId.IsValid()) return 2;

    const std::array<float, 4> input{1.0f, 2.0f, 3.0f, 4.0f};
    const std::array<float, 4> expected{7.0f, 9.0f, 11.0f, 13.0f};
    sge4::d3d12::D3D12Backend backend({true, false});
    runtime18::StaticCompositionLoadInput loadInput;
    loadInput.initialResources.push_back({inputId, fixture::Bytes(input)});
    auto loaded = runtime18::LoadStaticComposition(
        artifact.Value(), backend, std::move(loadInput));
    if (!loaded)
    {
        std::cerr << loaded.Error().stage << ": " << loaded.Error().message << '\n';
        return 3;
    }
    if (loaded.Value().LeafCount() != 4 ||
        loaded.Value().State() != sge4::runtime::DeviceRuntimeState::Active)
        return 4;

    auto submitted = runtime18::SubmitStaticComposition(loaded.Value(), {0, {}});
    if (!submitted)
    {
        std::cerr << submitted.Error().stage << ": " << submitted.Error().message << '\n';
        return 5;
    }
    if (submitted.Value().leaves.size() != 4 ||
        submitted.Value().executionOrder.size() != 4)
        return 6;
    for (std::size_t index = 0; index < submitted.Value().leaves.size(); ++index)
    {
        if (submitted.Value().leaves[index].deviceEpoch != submitted.Value().deviceEpoch)
            return 7;
    }
    auto read = runtime18::ReadStaticCompositionBuffer(loaded.Value(), outputId);
    if (!read || !fixture::Equals(read.Value().bytes, expected)) return 8;
    evidence.diamondOutput = read.Value().bytes;
    evidence.diamondLeaves = submitted.Value().leaves.size();

    // A second frame proves the fan-out retirement chain protects the next writer.
    auto second = runtime18::SubmitStaticComposition(loaded.Value(), {1, {}});
    auto secondRead = runtime18::ReadStaticCompositionBuffer(loaded.Value(), outputId);
    if (!second || !secondRead || !fixture::Equals(secondRead.Value().bytes, expected))
        return 9;

    const auto sourceLeaf = fixture::FindLeaf(
        artifact.Value(), "f9/diamond/source");
    runtime18::StaticCompositionFrameInvocation duplicate;
    duplicate.frameNumber = 2;
    duplicate.dynamicData = {
        {sourceLeaf, 0, {}}, {sourceLeaf, 0, {}}};
    auto duplicateRejected = runtime18::SubmitStaticComposition(
        loaded.Value(), duplicate);
    if (duplicateRejected ||
        duplicateRejected.Error().stage != "static-runtime/dynamic-data")
        return 10;

    auto corrupt = artifact.Value();
    corrupt[corrupt.size() / 2] ^= std::byte{0x01};
    auto corruptRejected = runtime18::LoadStaticComposition(corrupt, backend, {});
    if (corruptRejected ||
        corruptRejected.Error().stage != "static-runtime/frozen-read")
        return 11;
    return 0;
}

int RunLinear(Evidence& evidence)
{
    auto artifact = fixture::BuildLinearArtifact();
    if (!artifact) return 50;
    const auto inputId = fixture::FindResourceFlow(artifact.Value(), "f9/linear/input");
    const auto outputId = fixture::FindResourceFlow(artifact.Value(), "f9/linear/output");
    if (!inputId.IsValid() || !outputId.IsValid()) return 51;
    const std::array<float, 4> input{1.0f, 2.0f, 3.0f, 4.0f};
    const std::array<float, 4> expected{3.0f, 4.0f, 5.0f, 6.0f};
    sge4::d3d12::D3D12Backend backend({true, false});
    runtime18::StaticCompositionLoadInput loadInput;
    loadInput.initialResources = {{inputId, fixture::Bytes(input)}};
    auto loaded = runtime18::LoadStaticComposition(
        artifact.Value(), backend, std::move(loadInput));
    if (!loaded) return 52;
    auto submitted = runtime18::SubmitStaticComposition(loaded.Value(), {0, {}});
    auto read = runtime18::ReadStaticCompositionBuffer(loaded.Value(), outputId);
    if (!submitted || submitted.Value().leaves.size() != 2 ||
        !read || !fixture::Equals(read.Value().bytes, expected)) return 53;
    evidence.linearOutput = read.Value().bytes;
    evidence.linearLeaves = submitted.Value().leaves.size();
    return 0;
}

int RunFanout(Evidence& evidence)
{
    auto artifact = fixture::BuildFanoutArtifact();
    if (!artifact) return 60;
    const auto inputId = fixture::FindResourceFlow(artifact.Value(), "f9/fanout/input");
    const auto leftId = fixture::FindResourceFlow(artifact.Value(), "f9/fanout/output/left");
    const auto rightId = fixture::FindResourceFlow(artifact.Value(), "f9/fanout/output/right");
    if (!inputId.IsValid() || !leftId.IsValid() || !rightId.IsValid()) return 61;
    const std::array<float, 4> input{1.0f, 2.0f, 3.0f, 4.0f};
    const std::array<float, 4> expected{3.0f, 4.0f, 5.0f, 6.0f};
    sge4::d3d12::D3D12Backend backend({true, false});
    runtime18::StaticCompositionLoadInput loadInput;
    loadInput.initialResources = {{inputId, fixture::Bytes(input)}};
    auto loaded = runtime18::LoadStaticComposition(
        artifact.Value(), backend, std::move(loadInput));
    if (!loaded) return 62;
    auto submitted = runtime18::SubmitStaticComposition(loaded.Value(), {0, {}});
    auto left = runtime18::ReadStaticCompositionBuffer(loaded.Value(), leftId);
    auto right = runtime18::ReadStaticCompositionBuffer(loaded.Value(), rightId);
    if (!submitted || submitted.Value().leaves.size() != 3 || !left || !right ||
        !fixture::Equals(left.Value().bytes, expected) ||
        !fixture::Equals(right.Value().bytes, expected)) return 63;
    evidence.fanoutLeft = left.Value().bytes;
    evidence.fanoutRight = right.Value().bytes;
    evidence.fanoutLeaves = submitted.Value().leaves.size();
    return 0;
}

int RunMultiInput(Evidence& evidence)
{
    auto artifact = fixture::BuildMultiInputArtifact();
    if (!artifact) return 70;
    const auto inputA = fixture::FindResourceFlow(artifact.Value(), "f9/multi/input/a");
    const auto inputB = fixture::FindResourceFlow(artifact.Value(), "f9/multi/input/b");
    const auto outputId = fixture::FindResourceFlow(artifact.Value(), "f9/multi/output");
    if (!inputA.IsValid() || !inputB.IsValid() || !outputId.IsValid()) return 71;
    const std::array<float, 4> a{1.0f, 2.0f, 3.0f, 4.0f};
    const std::array<float, 4> b{4.0f, 3.0f, 2.0f, 1.0f};
    const std::array<float, 4> expected{8.0f, 8.0f, 8.0f, 8.0f};
    sge4::d3d12::D3D12Backend backend({true, false});
    runtime18::StaticCompositionLoadInput loadInput;
    loadInput.initialResources = {
        {inputA, fixture::Bytes(a)}, {inputB, fixture::Bytes(b)}};
    auto loaded = runtime18::LoadStaticComposition(
        artifact.Value(), backend, std::move(loadInput));
    if (!loaded) return 72;
    auto submitted = runtime18::SubmitStaticComposition(loaded.Value(), {0, {}});
    auto read = runtime18::ReadStaticCompositionBuffer(loaded.Value(), outputId);
    if (!submitted || submitted.Value().leaves.size() != 3 ||
        !read || !fixture::Equals(read.Value().bytes, expected)) return 73;
    evidence.multiInputOutput = read.Value().bytes;
    evidence.multiInputLeaves = submitted.Value().leaves.size();
    return 0;
}

int RunIndependent(Evidence& evidence)
{
    auto artifact = fixture::BuildIndependentArtifact();
    if (!artifact) { std::cerr << artifact.Error() << '\n'; return 20; }
    const auto inputA = fixture::FindResourceFlow(
        artifact.Value(), "f9/independent/input/a");
    const auto inputB = fixture::FindResourceFlow(
        artifact.Value(), "f9/independent/input/b");
    const auto outputA = fixture::FindResourceFlow(
        artifact.Value(), "f9/independent/output/a");
    const auto outputB = fixture::FindResourceFlow(
        artifact.Value(), "f9/independent/output/b");
    if (!inputA.IsValid() || !inputB.IsValid() ||
        !outputA.IsValid() || !outputB.IsValid()) return 21;

    const std::array<float, 4> a{1.0f, 2.0f, 3.0f, 4.0f};
    const std::array<float, 4> b{4.0f, 3.0f, 2.0f, 1.0f};
    sge4::d3d12::D3D12Backend backend({true, false});
    runtime18::StaticCompositionLoadInput input;
    input.initialResources = {
        {inputA, fixture::Bytes(a)}, {inputB, fixture::Bytes(b)}};
    auto loaded = runtime18::LoadStaticComposition(
        artifact.Value(), backend, std::move(input));
    if (!loaded) return 22;
    auto submitted = runtime18::SubmitStaticComposition(loaded.Value(), {0, {}});
    if (!submitted || submitted.Value().leaves.size() != 2) return 23;
    auto readA = runtime18::ReadStaticCompositionBuffer(loaded.Value(), outputA);
    auto readB = runtime18::ReadStaticCompositionBuffer(loaded.Value(), outputB);
    if (!readA || !readB ||
        !fixture::Equals(readA.Value().bytes, {2.0f, 3.0f, 4.0f, 5.0f}) ||
        !fixture::Equals(readB.Value().bytes, {5.0f, 4.0f, 3.0f, 2.0f}))
        return 24;
    evidence.independentA = readA.Value().bytes;
    evidence.independentB = readB.Value().bytes;
    evidence.independentLeaves = submitted.Value().leaves.size();
    return 0;
}

int RunPresenter(Evidence& evidence)
{
    auto artifact = fixture::BuildPresenterArtifact();
    if (!artifact) { std::cerr << artifact.Error() << '\n'; return 30; }
    const auto inputId = fixture::FindResourceFlow(artifact.Value(), "f9/present/input");
    if (!inputId.IsValid() || !fixture::HasPresenter(artifact.Value())) return 31;
    auto window = sge4::platform::Win32Window::Create(
        L"SGE4 Level4v1 Canonical R3 Presenter", 64, 64);
    if (!window) return 32;

    sge4::d3d12::D3D12Backend backend({true, false});
    runtime18::StaticCompositionLoadInput input;
    input.surface = window.Value().get();
    input.initialResources = {
        {inputId, fixture::Bytes({0.2f, 0.4f, 0.8f, 1.0f})}};
    auto loaded = runtime18::LoadStaticComposition(
        artifact.Value(), backend, std::move(input));
    if (!loaded)
    {
        std::cerr << loaded.Error().stage << ": " << loaded.Error().message << '\n';
        return 33;
    }
    auto submitted = runtime18::SubmitStaticComposition(loaded.Value(), {0, {}});
    if (!submitted || submitted.Value().leaves.size() != 2 ||
        submitted.Value().executionOrder.back() != fixture::FindLeaf(artifact.Value(), "f9/present/presenter"))
        return 34;
    evidence.presenterLeaves = submitted.Value().leaves.size();
    return 0;
}

std::string EvidenceManifest(const Evidence& evidence)
{
    std::ostringstream output;
    output << "identity=SGE4-Level4v1-Canonical-R3-Static-DAG-Runtime-v1\n";
    output << "diamond-leaves=" << evidence.diamondLeaves << '\n';
    output << "linear-leaves=" << evidence.linearLeaves << '\n';
    output << "fanout-leaves=" << evidence.fanoutLeaves << '\n';
    output << "multi-input-leaves=" << evidence.multiInputLeaves << '\n';
    output << "independent-leaves=" << evidence.independentLeaves << '\n';
    output << "presenter-leaves=" << evidence.presenterLeaves << '\n';
    output << "diamond-output-sha256="
           << sge4::base::ToHex(sge4::base::Sha256(evidence.diamondOutput)) << '\n';
    output << "linear-output-sha256="
           << sge4::base::ToHex(sge4::base::Sha256(evidence.linearOutput)) << '\n';
    output << "fanout-left-sha256="
           << sge4::base::ToHex(sge4::base::Sha256(evidence.fanoutLeft)) << '\n';
    output << "fanout-right-sha256="
           << sge4::base::ToHex(sge4::base::Sha256(evidence.fanoutRight)) << '\n';
    output << "multi-input-output-sha256="
           << sge4::base::ToHex(sge4::base::Sha256(evidence.multiInputOutput)) << '\n';
    output << "independent-a-sha256="
           << sge4::base::ToHex(sge4::base::Sha256(evidence.independentA)) << '\n';
    output << "independent-b-sha256="
           << sge4::base::ToHex(sge4::base::Sha256(evidence.independentB)) << '\n';
    const auto body = output.str();
    output << "evidence-aggregate=" << sge4::base::ToHex(sge4::base::Sha256(
        std::as_bytes(std::span<const char>(body.data(), body.size())))) << '\n';
    return output.str();
}
}

int main(int argc, char** argv)
{
    Evidence evidence;
    const int diamond = RunDiamond(evidence);
    if (diamond != 0)
    {
        std::cerr << "diamond runtime failed: " << diamond << '\n';
        return diamond;
    }
    const int linear = RunLinear(evidence);
    if (linear != 0)
    {
        std::cerr << "linear runtime failed: " << linear << '\n';
        return linear;
    }
    const int fanout = RunFanout(evidence);
    if (fanout != 0)
    {
        std::cerr << "fan-out runtime failed: " << fanout << '\n';
        return fanout;
    }
    const int multiInput = RunMultiInput(evidence);
    if (multiInput != 0)
    {
        std::cerr << "multi-input runtime failed: " << multiInput << '\n';
        return multiInput;
    }
    const int independent = RunIndependent(evidence);
    if (independent != 0)
    {
        std::cerr << "independent runtime failed: " << independent << '\n';
        return independent;
    }
    const int presenter = RunPresenter(evidence);
    if (presenter != 0)
    {
        std::cerr << "presenter runtime failed: " << presenter << '\n';
        return presenter;
    }

    const auto manifest = EvidenceManifest(evidence);
    if (argc == 3 && std::string_view(argv[1]) == "--emit")
        if (!WriteText(argv[2], manifest)) return 40;

    std::cout << "Canonical R3 Shared DeviceDomain and Static DAG Runtime tests passed\n";
    std::cout << "DAGs: independent, linear, fan-out, multi-input, diamond, headless, single-presenter\n";
    std::cout << "WARP diamond output: 7, 9, 11, 13\n";
    std::cout << "Frames: repeated fan-out execution preserved retirement authority\n";
    std::cout << manifest;
    return 0;
}
