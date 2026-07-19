#include "../00_Foundation/Base.h"
#include "../14_D3D12Backend/D3D12Backend.h"
#include "../22_CompositionRuntime/CompositionRuntime.h"
#include "../23_CompositionRecovery/CompositionRecovery.h"
#include "../82_Spiral2Corpus/Spiral2Corpus.h"
#include "../88_Spiral2Scenarios/Spiral2Scenarios.h"
#include <filesystem>
#include <iostream>

namespace b = sge4_5::base;
namespace c = sge4_5::spiral2::corpus;
namespace s = sge4_5::spiral2::scenarios;
namespace can = sge4_5::composition::canonical;
namespace runtime = sge4_5::composition::canonical::runtime_v1;

namespace {
can::ResourceFlowId FindFlow(const can::PackageCompositionContract &contract,
                             std::string_view key) {
  const auto stable = can::ComputeStableResourceKey(key);
  for (const auto &flow : contract.resources)
    if (flow.stableKey == stable)
      return flow.id;
  return {};
}
std::vector<std::byte>
OutputEvidence(const s::FrozenHierarchyComparisonScenarioV1 &scenario,
               const s::HierarchyComparisonExecutionResultV1 &result) {
  b::BinaryWriter writer;
  constexpr std::string_view domain =
      "SGE4-5.Spiral2.CU5.RematerializationEvidence.V1";
  writer.WriteU32(static_cast<std::uint32_t>(domain.size()));
  writer.WriteBytes(std::as_bytes(std::span(domain.data(), domain.size())));
  writer.WriteBytes(scenario.frozenCompositionDigest);
  for (const auto *points : {&result.matrix, &result.direct, &result.hybrid}) {
    auto bytes = c::SerializeReferencePointsV1(*points);
    writer.WriteU64(bytes.size());
    writer.WriteBytes(bytes);
  }
  writer.WriteU64(result.gpuRecords.size());
  writer.WriteBytes(result.gpuRecords);
  return std::move(writer).Take();
}
bool WriteEvidence(const std::filesystem::path &path,
                   const std::vector<std::byte> &evidence) {
  auto wrote = b::WriteAllBytes(path, evidence);
  if (!wrote)
    return false;
  std::cout << b::ToHex(b::Sha256(evidence)) << '\n';
  return true;
}
int Controlled(const std::filesystem::path *output) {
  auto topologies = c::BuildTopologyCorpusV1();
  if (!topologies)
    return 1;
  const auto &topology = topologies.Value()[4];
  auto frames = c::BuildFrameCorpusV1(topology.hierarchy);
  if (!frames)
    return 2;
  auto scenario =
      s::BuildFrozenHierarchyComparisonScenarioV1(topology.hierarchy);
  if (!scenario)
    return 3;
  sge4_5::d3d12::D3D12Backend backend({true, true});
  auto loaded = runtime::LoadStaticComposition(
      scenario.Value().frozenCompositionBytes, backend);
  if (!loaded)
    return 4;
  const auto recordsFlow = FindFlow(
      loaded.Value().Artifact().ValidatedContract().Contract(), "s2/records");
  if (!recordsFlow.IsValid())
    return 5;
  auto before = s::ExecuteLoadedHierarchyComparisonScenarioV1(
      scenario.Value(), topology.hierarchy, frames.Value()[4].palette,
      loaded.Value(), 0);
  if (!before)
    return 6;
  const auto staleResource = loaded.Value().SharedResource(recordsFlow);
  const auto staleToken = loaded.Value().AvailableAfter(recordsFlow);
  const auto oldEpoch = loaded.Value().DeviceEpoch();
  auto recovered = runtime::RecoverStaticComposition(
      loaded.Value(), sge4_5::runtime::DeviceRecoveryMode::ControlledRebuild);
  if (!recovered)
    return 7;
  const auto &report = recovered.Value();
  if (loaded.Value().State() != sge4_5::runtime::DeviceRuntimeState::Active ||
      loaded.Value().DeviceEpoch() != oldEpoch + 1 ||
      report.leafCountBefore != 10 || report.leafCountAfter != 10 ||
      report.resourceCountBefore != 11 || report.resourceCountAfter != 11 ||
      !report.frozenArtifactRevalidated || !report.allRuntimeObjectsReleased ||
      !report.allRuntimeObjectsRematerialized)
    return 8;
  auto stale = runtime::ValidateStaticCompositionHandleEpoch(
      loaded.Value(), staleResource, staleToken);
  if (stale || stale.Error().stage != "static-runtime/stale-epoch")
    return 9;
  runtime::StaticCompositionFrameInvocation missingRebind;
  missingRebind.frameNumber = 1;
  if (runtime::SubmitStaticComposition(loaded.Value(), missingRebind))
    return 10;
  auto after = s::ExecuteLoadedHierarchyComparisonScenarioV1(
      scenario.Value(), topology.hierarchy, frames.Value()[4].palette,
      loaded.Value(), 2);
  if (!after)
    return 11;
  if (OutputEvidence(scenario.Value(), before.Value()) !=
      OutputEvidence(scenario.Value(), after.Value()))
    return 12;
  auto current = runtime::ValidateStaticCompositionHandleEpoch(
      loaded.Value(), loaded.Value().SharedResource(recordsFlow),
      loaded.Value().AvailableAfter(recordsFlow));
  if (!current)
    return 13;
  if (output &&
      !WriteEvidence(*output, OutputEvidence(scenario.Value(), after.Value())))
    return 14;
  std::cout << "Spiral 2 controlled whole-composition recovery passed, epoch "
            << oldEpoch << "->" << loaded.Value().DeviceEpoch()
            << "; explicit palette rebind required.\n";
  return 0;
}
int ActualRemoval(const std::filesystem::path &output) {
  auto topologies = c::BuildTopologyCorpusV1();
  if (!topologies)
    return 20;
  const auto &topology = topologies.Value()[7];
  auto frames = c::BuildFrameCorpusV1(topology.hierarchy);
  if (!frames)
    return 21;
  auto scenario =
      s::BuildFrozenHierarchyComparisonScenarioV1(topology.hierarchy);
  if (!scenario)
    return 22;
  sge4_5::d3d12::D3D12Backend backend({true, true});
  auto loaded = runtime::LoadStaticComposition(
      scenario.Value().frozenCompositionBytes, backend);
  if (!loaded)
    return 23;
  const auto recordsFlow = FindFlow(
      loaded.Value().Artifact().ValidatedContract().Contract(), "s2/records");
  auto before = s::ExecuteLoadedHierarchyComparisonScenarioV1(
      scenario.Value(), topology.hierarchy, frames.Value()[7].palette,
      loaded.Value(), 0);
  if (!before || !recordsFlow.IsValid())
    return 24;
  const auto evidence = OutputEvidence(scenario.Value(), before.Value());
  const auto staleResource = loaded.Value().SharedResource(recordsFlow);
  const auto staleToken = loaded.Value().AvailableAfter(recordsFlow);
  const auto oldEpoch = loaded.Value().DeviceEpoch();
  auto removed = runtime::RecoverStaticComposition(
      loaded.Value(), sge4_5::runtime::DeviceRecoveryMode::ForceRemovalForTest);
  if (!removed)
    return 25;
  if (!removed.Value().device.forcedRemoval ||
      loaded.Value().State() !=
          sge4_5::runtime::DeviceRuntimeState::AwaitingAdapter ||
      loaded.Value().DeviceEpoch() != oldEpoch ||
      loaded.Value().LeafCount() != 0 || loaded.Value().ResourceCount() != 0 ||
      !removed.Value().allRuntimeObjectsReleased ||
      removed.Value().allRuntimeObjectsRematerialized)
    return 26;
  auto stale = runtime::ValidateStaticCompositionHandleEpoch(
      loaded.Value(), staleResource, staleToken);
  if (stale || stale.Error().stage != "static-runtime/device-state")
    return 27;
  if (runtime::SubmitStaticComposition(loaded.Value(), {}))
    return 28;
  auto retry = runtime::RecoverStaticComposition(
      loaded.Value(),
      sge4_5::runtime::DeviceRecoveryMode::RetryAdapterReacquisition);
  if (!retry || retry.Value().device.adapterReacquired ||
      loaded.Value().State() !=
          sge4_5::runtime::DeviceRuntimeState::AwaitingAdapter)
    return 29;
  if (!WriteEvidence(output, evidence))
    return 30;
  std::cout << "Spiral 2 actual RemoveDevice quarantine passed; removed WARP "
               "LUID excluded and stale submission rejected.\n";
  return 0;
}
int FreshRematerialization(const std::filesystem::path &output) {
  auto topologies = c::BuildTopologyCorpusV1();
  if (!topologies)
    return 40;
  const auto &topology = topologies.Value()[7];
  auto frames = c::BuildFrameCorpusV1(topology.hierarchy);
  if (!frames)
    return 41;
  auto scenario =
      s::BuildFrozenHierarchyComparisonScenarioV1(topology.hierarchy);
  if (!scenario)
    return 42;
  sge4_5::d3d12::D3D12Backend backend({true, true});
  auto loaded = runtime::LoadStaticComposition(
      scenario.Value().frozenCompositionBytes, backend);
  if (!loaded)
    return 43;
  auto observed = s::ExecuteLoadedHierarchyComparisonScenarioV1(
      scenario.Value(), topology.hierarchy, frames.Value()[7].palette,
      loaded.Value(), 0);
  if (!observed)
    return 44;
  if (!WriteEvidence(output,
                     OutputEvidence(scenario.Value(), observed.Value())))
    return 45;
  std::cout << "Spiral 2 fresh-process F07 rematerialization passed.\n";
  return 0;
}
} // namespace

int main(int argc, char **argv) {
  if (argc == 4 && std::string(argv[2]) == "--emit") {
    const std::filesystem::path output = argv[3];
    if (std::string(argv[1]) == "--controlled")
      return Controlled(&output);
    if (std::string(argv[1]) == "--actual-removal")
      return ActualRemoval(output);
    if (std::string(argv[1]) == "--fresh-rematerialize")
      return FreshRematerialization(output);
  }
  if (argc == 1 || std::string(argv[1]) == "--controlled")
    return Controlled(nullptr);
  return 64;
}
