#include "../48_CanonicalCompositionRuntimeTests/RuntimeFixture.h"
#include "../23_CompositionRecovery/CompositionRecovery.h"
#include "../209_Level4V2RuntimeRecovery/RuntimeRecovery.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fixture = sge4_5::qualification::canonical_runtime_fixture;
namespace reference_runtime = sge4_5::composition::canonical::runtime_v1;

namespace
{
void Emit(const std::string& path, const std::vector<std::byte>& bytes)
{
    std::ofstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("open evidence");
    stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!stream) throw std::runtime_error("write evidence");
}

std::pair<std::uint64_t, std::uint64_t> ControlledRebuildQualification()
{
    auto artifact = fixture::BuildDiamondArtifact();
    if (!artifact) throw std::runtime_error(artifact.Error());
    const auto inputId = fixture::FindResourceFlow(artifact.Value(), "f9/diamond/input");
    const auto outputId = fixture::FindResourceFlow(artifact.Value(), "f9/diamond/output");
    if (!inputId.IsValid() || !outputId.IsValid()) throw std::runtime_error("flow lookup");
    const std::array<float, 4> input{1.0f, 2.0f, 3.0f, 4.0f};
    const std::array<float, 4> expected{7.0f, 9.0f, 11.0f, 13.0f};
    sge4_5::d3d12::D3D12Backend backend({true, false});
    reference_runtime::StaticCompositionLoadInput loadInput;
    loadInput.initialResources = {{inputId, fixture::Bytes(input)}};
    auto loaded = reference_runtime::LoadStaticComposition(artifact.Value(), backend, std::move(loadInput));
    if (!loaded) throw std::runtime_error(loaded.Error().stage + ": " + loaded.Error().message);
    auto beforeSubmit = reference_runtime::SubmitStaticComposition(loaded.Value(), {0, {}});
    auto beforeRead = reference_runtime::ReadStaticCompositionBuffer(loaded.Value(), outputId);
    if (!beforeSubmit || !beforeRead || !fixture::Equals(beforeRead.Value().bytes, expected))
        throw std::runtime_error("pre-recovery execution");
    const auto oldEpoch = loaded.Value().DeviceEpoch();
    const auto staleResource = loaded.Value().SharedResource(outputId);
    const auto staleToken = loaded.Value().AvailableAfter(outputId);
    auto recovered = reference_runtime::RecoverStaticComposition(
        loaded.Value(), sge4_5::runtime::DeviceRecoveryMode::ControlledRebuild);
    if (!recovered) throw std::runtime_error(recovered.Error().stage + ": " + recovered.Error().message);
    if (loaded.Value().DeviceEpoch() != oldEpoch + 1u ||
        !recovered.Value().frozenArtifactRevalidated ||
        !recovered.Value().allRuntimeObjectsReleased ||
        !recovered.Value().allRuntimeObjectsRematerialized)
        throw std::runtime_error("controlled recovery report");
    auto stale = reference_runtime::ValidateStaticCompositionHandleEpoch(
        loaded.Value(), staleResource, staleToken);
    if (stale || stale.Error().stage != "static-runtime/stale-epoch")
        throw std::runtime_error("stale epoch was not rejected");
    auto afterSubmit = reference_runtime::SubmitStaticComposition(loaded.Value(), {1, {}});
    auto afterRead = reference_runtime::ReadStaticCompositionBuffer(loaded.Value(), outputId);
    if (!afterSubmit || !afterRead || !fixture::Equals(afterRead.Value().bytes, expected))
        throw std::runtime_error("post-recovery execution");
    return {oldEpoch, loaded.Value().DeviceEpoch()};
}

std::uint64_t ActualRemovalQualification()
{
    auto artifact = fixture::BuildLinearArtifact();
    if (!artifact) throw std::runtime_error(artifact.Error());
    const auto inputId = fixture::FindResourceFlow(artifact.Value(), "f9/linear/input");
    const auto outputId = fixture::FindResourceFlow(artifact.Value(), "f9/linear/output");
    if (!inputId.IsValid() || !outputId.IsValid()) throw std::runtime_error("flow lookup");
    sge4_5::d3d12::D3D12Backend backend({true, false});
    reference_runtime::StaticCompositionLoadInput input;
    input.initialResources = {{inputId, fixture::Bytes({1.0f, 2.0f, 3.0f, 4.0f})}};
    auto loaded = reference_runtime::LoadStaticComposition(artifact.Value(), backend, std::move(input));
    if (!loaded) throw std::runtime_error("load");
    if (!reference_runtime::SubmitStaticComposition(loaded.Value(), {0, {}}))
        throw std::runtime_error("submit");
    const auto oldEpoch = loaded.Value().DeviceEpoch();
    auto removed = reference_runtime::RecoverStaticComposition(
        loaded.Value(), sge4_5::runtime::DeviceRecoveryMode::ForceRemovalForTest);
    if (!removed) throw std::runtime_error(removed.Error().stage + ": " + removed.Error().message);
    if (!removed.Value().device.forcedRemoval ||
        loaded.Value().State() != sge4_5::runtime::DeviceRuntimeState::AwaitingAdapter ||
        loaded.Value().DeviceEpoch() != oldEpoch || loaded.Value().LeafCount() != 0 ||
        loaded.Value().ResourceCount() != 0 || !removed.Value().allRuntimeObjectsReleased)
        throw std::runtime_error("actual removal state");
    auto retry = reference_runtime::RecoverStaticComposition(
        loaded.Value(), sge4_5::runtime::DeviceRecoveryMode::RetryAdapterReacquisition);
    if (!retry || loaded.Value().State() != sge4_5::runtime::DeviceRuntimeState::AwaitingAdapter ||
        retry.Value().device.adapterReacquired)
        throw std::runtime_error("removed adapter exclusion");
    return oldEpoch;
}
}

int main(int argc, char** argv)
{
    try
    {
        std::string emitPath;
        for (int index = 1; index < argc; ++index)
            if (std::string(argv[index]) == "--emit" && index + 1 < argc) emitPath = argv[++index];
        if (emitPath.empty()) throw std::runtime_error("--emit is required");
        const auto controlled = ControlledRebuildQualification();
        const auto removalEpoch = ActualRemovalQualification();
        if (controlled.second != controlled.first + 1u || removalEpoch == 0u)
            throw std::runtime_error("normalized qualification evidence precondition");
        std::vector<std::byte> evidence{
            std::byte{'L'}, std::byte{'4'}, std::byte{'V'}, std::byte{'2'},
            std::byte{'R'}, std::byte{'5'}, std::byte{'W'}, std::byte{'1'}};
        evidence.push_back(std::byte{1}); // controlled full rematerialization
        evidence.push_back(std::byte{1}); // stale epoch rejection
        evidence.push_back(std::byte{1}); // actual removal
        evidence.push_back(std::byte{1}); // removed adapter exclusion
        evidence.push_back(std::byte{1}); // awaiting-adapter retry preservation
        Emit(emitPath, evidence);
        std::cout << "Level 4 v2 R5 Windows Runtime/Recovery qualification passed.\n";
        std::cout << "WARP controlled rebuild, stale epoch, actual removal and removed-adapter exclusion passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "R5 Windows qualification failure: " << error.what() << '\n';
        return 1;
    }
}
