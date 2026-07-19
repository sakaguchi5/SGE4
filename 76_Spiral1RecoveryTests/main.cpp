#include "../00_Foundation/FileIO.h"
#include "../00_Foundation/Sha256.h"
#include "../14_D3D12Backend/D3D12Backend.h"
#include "../22_CompositionRuntime/CompositionRuntime.h"
#include "../23_CompositionRecovery/CompositionRecovery.h"
#include "../61_Spiral1Contracts/Spiral1Contracts.h"
#include "../62_Spiral1Corpus/Spiral1Corpus.h"
#include "../66_Spiral1LeafCompiler/Spiral1LeafCompiler.h"
#include "../67_Spiral1Observer/Spiral1Observer.h"
#include "../68_Spiral1Scenarios/Spiral1Scenarios.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{
namespace base = sge4_5::base;
namespace runtime = sge4_5::composition::canonical::runtime_v1;
namespace contracts = sge4_5::spiral1::contracts;
namespace corpus = sge4_5::spiral1::corpus;
namespace leaf = sge4_5::spiral1::leaf;
namespace observer = sge4_5::spiral1::observer;
namespace scenarios = sge4_5::spiral1::scenarios;
namespace semantic = sge4_5::spiral1::semantic;

struct LoadedObservation final
{
    contracts::ScenarioReportV1 report;
    std::vector<std::byte> matrixBytes;
    std::vector<std::byte> directBytes;
    std::vector<std::byte> recordBytes;
};

void Require(bool condition, std::string_view message)
{
    if (!condition) throw std::runtime_error(std::string(message));
}

void WriteDigest(base::BinaryWriter& writer, const base::Digest256& value)
{
    writer.WriteBytes(value);
}

const corpus::QualificationCaseV1& FindCase(
    const corpus::QualificationCorpusV1& values,
    std::string_view scenario,
    std::uint32_t subscenario = 0)
{
    const auto found = std::find_if(values.cases.begin(), values.cases.end(), [&](const auto& value) {
        return value.scenarioId == scenario && value.subscenarioIndex == subscenario;
    });
    if (found == values.cases.end()) throw std::runtime_error("qualification case was not found");
    return *found;
}

std::vector<semantic::Float4Point> DecodePoints(std::span<const std::byte> bytes)
{
    if ((bytes.size() % sizeof(semantic::Float4Point)) != 0)
        throw std::runtime_error("point readback byte count is invalid");
    std::vector<semantic::Float4Point> values(bytes.size() / sizeof(semantic::Float4Point));
    if (!bytes.empty()) std::memcpy(values.data(), bytes.data(), bytes.size());
    return values;
}

LoadedObservation ObserveLoaded(
    runtime::LoadedStaticComposition& loaded,
    const scenarios::FrozenComparisonScenarioV1& artifact,
    const corpus::QualificationCaseV1& qualificationCase,
    std::uint64_t frameNumber)
{
    const auto& contract = loaded.Artifact().ValidatedContract().Contract();
    const auto matrixLeaf = scenarios::FindLeafByAuthorKeyV1(contract, scenarios::MatrixLeafKeyV1);
    const auto directLeaf = scenarios::FindLeafByAuthorKeyV1(contract, scenarios::DirectLeafKeyV1);
    const auto matrixFlow = scenarios::FindFlowByAuthorKeyV1(contract, scenarios::MatrixOutputFlowKeyV1);
    const auto directFlow = scenarios::FindFlowByAuthorKeyV1(contract, scenarios::DirectOutputFlowKeyV1);
    const auto recordsFlow = scenarios::FindFlowByAuthorKeyV1(contract, scenarios::ComparisonRecordsFlowKeyV1);
    Require(matrixLeaf.IsValid() && directLeaf.IsValid() && matrixFlow.IsValid() && directFlow.IsValid() && recordsFlow.IsValid(),
            "canonical Spiral 1 role identity was not found");

    runtime::StaticCompositionFrameInvocation invocation;
    invocation.frameNumber = frameNumber;
    invocation.dynamicData = {
        {matrixLeaf, artifact.matrixLeaf.lockedDynamicSlot, artifact.matrixLeaf.lockedConstantPayload},
        {directLeaf, artifact.directPgaLeaf.lockedDynamicSlot, artifact.directPgaLeaf.lockedConstantPayload}};
    auto submitted = runtime::SubmitStaticComposition(loaded, invocation);
    if (!submitted) throw std::runtime_error(submitted.Error().stage + ": " + submitted.Error().message);
    Require(submitted.Value().executionOrder.size() == 4 && submitted.Value().leaves.size() == 4,
            "recovery qualification did not execute exactly four Leaves");

    auto matrixRead = runtime::ReadStaticCompositionBuffer(loaded, matrixFlow);
    auto directRead = runtime::ReadStaticCompositionBuffer(loaded, directFlow);
    auto recordsRead = runtime::ReadStaticCompositionBuffer(loaded, recordsFlow);
    if (!matrixRead || !directRead || !recordsRead)
    {
        const auto* error = !matrixRead ? &matrixRead.Error() : !directRead ? &directRead.Error() : &recordsRead.Error();
        throw std::runtime_error(error->stage + ": " + error->message);
    }
    auto matrixPoints = DecodePoints(matrixRead.Value().bytes);
    auto directPoints = DecodePoints(directRead.Value().bytes);
    auto observed = observer::ObserveCaseV1(qualificationCase, matrixPoints, directPoints);
    if (!observed) throw std::runtime_error(observed.Error());
    const auto expectedRecords = contracts::SerializeComparisonRecordsV1(observed.Value().records);
    Require(recordsRead.Value().bytes == expectedRecords,
            "recovery readback differs from deterministic CPU Observation V2 records");
    Require(observed.Value().report.mismatchCount == 0 && observed.Value().report.nonFiniteCount == 0,
            "recovery observation violates Observation Contract V2");

    LoadedObservation output;
    output.report = observed.Value().report;
    output.matrixBytes = std::move(matrixRead).Value().bytes;
    output.directBytes = std::move(directRead).Value().bytes;
    output.recordBytes = std::move(recordsRead).Value().bytes;
    return output;
}

void RequireSameObservation(const LoadedObservation& before, const LoadedObservation& after)
{
    Require(before.matrixBytes == after.matrixBytes, "Matrix output changed across recovery");
    Require(before.directBytes == after.directBytes, "Direct PGA output changed across recovery");
    Require(before.recordBytes == after.recordBytes, "Comparison records changed across recovery");
    Require(before.report.readbackDigest == after.report.readbackDigest,
            "Observation readback digest changed across recovery");
}

std::vector<std::byte> RunControlledRecovery(
    const corpus::QualificationCorpusV1& corpusValue,
    const leaf::NativeProgramSetV1& nativePrograms)
{
    const std::array<std::pair<std::string_view, std::uint32_t>, 4> selected{{
        {"S00",0}, {"S07",0}, {"S12",0}, {"S15",15}}};
    base::BinaryWriter evidence;
    constexpr std::string_view domain = "SGE4-5.Spiral1.CU5.ControlledRecoveryEvidence.V1";
    evidence.WriteU32(static_cast<std::uint32_t>(domain.size()));
    evidence.WriteBytes(std::as_bytes(std::span(domain.data(), domain.size())));
    WriteDigest(evidence, corpusValue.contractIdentity);
    WriteDigest(evidence, nativePrograms.representationTargetProfile.identity);
    evidence.WriteU32(static_cast<std::uint32_t>(selected.size()));

    for (const auto& [scenarioId, subscenario] : selected)
    {
        const auto& value = FindCase(corpusValue, scenarioId, subscenario);
        auto artifact = scenarios::BuildFrozenComparisonScenarioV1(value, nativePrograms);
        if (!artifact) throw std::runtime_error(artifact.Error().stage + ": " + artifact.Error().message);
        auto valid = scenarios::ValidateFrozenComparisonScenarioV1(artifact.Value(), value, nativePrograms);
        if (!valid) throw std::runtime_error(valid.Error().stage + ": " + valid.Error().message);

        sge4_5::d3d12::D3D12Backend backend({true, true});
        auto loaded = runtime::LoadStaticComposition(artifact.Value().frozenCompositionBytes, backend);
        if (!loaded) throw std::runtime_error(loaded.Error().stage + ": " + loaded.Error().message);
        const auto& contract = loaded.Value().Artifact().ValidatedContract().Contract();
        const auto recordsFlow = scenarios::FindFlowByAuthorKeyV1(contract, scenarios::ComparisonRecordsFlowKeyV1);
        Require(recordsFlow.IsValid(), "comparison records flow is missing");

        const auto beforeArtifactDigest = base::Sha256(loaded.Value().Artifact().Artifact().FileBytes());
        auto before = ObserveLoaded(loaded.Value(), artifact.Value(), value, 0);
        const auto oldEpoch = loaded.Value().DeviceEpoch();
        const auto staleResource = loaded.Value().SharedResource(recordsFlow);
        const auto staleToken = loaded.Value().AvailableAfter(recordsFlow);
        Require(staleResource && staleToken, "pre-recovery handle capture failed");

        auto recovered = runtime::RecoverStaticComposition(
            loaded.Value(), sge4_5::runtime::DeviceRecoveryMode::ControlledRebuild);
        if (!recovered) throw std::runtime_error(recovered.Error().stage + ": " + recovered.Error().message);
        const auto& report = recovered.Value();
        Require(loaded.Value().State() == sge4_5::runtime::DeviceRuntimeState::Active,
                "controlled recovery did not return Active state");
        Require(loaded.Value().DeviceEpoch() == oldEpoch + 1 &&
                report.device.previousDeviceEpoch == oldEpoch && report.device.newDeviceEpoch == oldEpoch + 1,
                "controlled recovery epoch transition is invalid");
        Require(report.device.adapterReacquired && report.device.packageObjectsRebuilt &&
                report.frozenArtifactRevalidated && report.allRuntimeObjectsReleased &&
                report.allRuntimeObjectsRematerialized && report.leafCountBefore == 4 && report.leafCountAfter == 4 &&
                report.resourceCountBefore == 5 && report.resourceCountAfter == 5,
                "controlled whole-composition rematerialization report is incomplete");
        const auto afterArtifactDigest = base::Sha256(loaded.Value().Artifact().Artifact().FileBytes());
        Require(beforeArtifactDigest == afterArtifactDigest && beforeArtifactDigest == base::Sha256(artifact.Value().frozenCompositionBytes),
                "Frozen Composition identity changed across recovery");

        auto stale = runtime::ValidateStaticCompositionHandleEpoch(loaded.Value(), staleResource, staleToken);
        Require(!stale && stale.Error().stage == "static-runtime/stale-epoch",
                "old resource/token escaped stale-epoch rejection");
        auto after = ObserveLoaded(loaded.Value(), artifact.Value(), value, 1);
        RequireSameObservation(before, after);
        auto current = runtime::ValidateStaticCompositionHandleEpoch(
            loaded.Value(), loaded.Value().SharedResource(recordsFlow), loaded.Value().AvailableAfter(recordsFlow));
        Require(static_cast<bool>(current), "current-epoch resource/token was rejected");

        evidence.WriteU32(static_cast<std::uint32_t>(scenarioId.size()));
        evidence.WriteBytes(std::as_bytes(std::span(scenarioId.data(), scenarioId.size())));
        evidence.WriteU32(subscenario);
        evidence.WriteU32(static_cast<std::uint32_t>(value.inputPoints.size()));
        WriteDigest(evidence, value.caseIdentity);
        WriteDigest(evidence, artifact.Value().artifactIdentity);
        WriteDigest(evidence, beforeArtifactDigest);
        WriteDigest(evidence, before.report.readbackDigest);
        WriteDigest(evidence, after.report.readbackDigest);
        evidence.WriteU64(oldEpoch);
        evidence.WriteU64(loaded.Value().DeviceEpoch());
        evidence.WriteU32(static_cast<std::uint32_t>(report.leafCountAfter));
        evidence.WriteU32(static_cast<std::uint32_t>(report.resourceCountAfter));
        std::cout << "[RECOVERED] " << scenarioId << '.' << subscenario
                  << " points=" << value.inputPoints.size()
                  << " epoch=" << oldEpoch << "->" << loaded.Value().DeviceEpoch()
                  << " readback=" << base::ToHex(after.report.readbackDigest) << '\n';
    }
    return std::move(evidence).Take();
}

std::vector<std::byte> RunFreshProcessRematerialization(
    const corpus::QualificationCorpusV1& corpusValue,
    const leaf::NativeProgramSetV1& nativePrograms)
{
    const auto& value = FindCase(corpusValue, "S12", 0);
    auto artifact = scenarios::BuildFrozenComparisonScenarioV1(value, nativePrograms);
    if (!artifact) throw std::runtime_error(artifact.Error().stage + ": " + artifact.Error().message);
    auto valid = scenarios::ValidateFrozenComparisonScenarioV1(artifact.Value(), value, nativePrograms);
    if (!valid) throw std::runtime_error(valid.Error().stage + ": " + valid.Error().message);

    sge4_5::d3d12::D3D12Backend backend({true, true});
    auto loaded = runtime::LoadStaticComposition(artifact.Value().frozenCompositionBytes, backend);
    if (!loaded) throw std::runtime_error(loaded.Error().stage + ": " + loaded.Error().message);
    auto observed = ObserveLoaded(loaded.Value(), artifact.Value(), value, 0);
    const auto frozenDigest = base::Sha256(loaded.Value().Artifact().Artifact().FileBytes());
    Require(frozenDigest == base::Sha256(artifact.Value().frozenCompositionBytes),
            "fresh process reconstructed a different Frozen Composition");

    base::BinaryWriter evidence;
    constexpr std::string_view domain = "SGE4-5.Spiral1.CU5.FreshProcessRematerializationEvidence.V1";
    evidence.WriteU32(static_cast<std::uint32_t>(domain.size()));
    evidence.WriteBytes(std::as_bytes(std::span(domain.data(), domain.size())));
    WriteDigest(evidence, corpusValue.contractIdentity);
    WriteDigest(evidence, nativePrograms.representationTargetProfile.identity);
    WriteDigest(evidence, value.caseIdentity);
    WriteDigest(evidence, artifact.Value().artifactIdentity);
    WriteDigest(evidence, frozenDigest);
    WriteDigest(evidence, observed.report.readbackDigest);
    WriteDigest(evidence, base::Sha256(observed.matrixBytes));
    WriteDigest(evidence, base::Sha256(observed.directBytes));
    WriteDigest(evidence, base::Sha256(observed.recordBytes));
    evidence.WriteU32(static_cast<std::uint32_t>(value.inputPoints.size()));
    evidence.WriteU32(observed.report.mismatchCount);
    evidence.WriteU32(observed.report.nonFiniteCount);

    std::cout << "Fresh-process rematerialization qualification passed.\n";
    std::cout << "S12 Observation V2 readback: " << base::ToHex(observed.report.readbackDigest) << '\n';
    return std::move(evidence).Take();
}

void RunActualRemoval(
    const corpus::QualificationCorpusV1& corpusValue,
    const leaf::NativeProgramSetV1& nativePrograms)
{
    const auto& value = FindCase(corpusValue, "S12", 0);
    auto artifact = scenarios::BuildFrozenComparisonScenarioV1(value, nativePrograms);
    if (!artifact) throw std::runtime_error(artifact.Error().stage + ": " + artifact.Error().message);
    sge4_5::d3d12::D3D12Backend removedBackend({true, true});
    auto loaded = runtime::LoadStaticComposition(artifact.Value().frozenCompositionBytes, removedBackend);
    if (!loaded) throw std::runtime_error(loaded.Error().stage + ": " + loaded.Error().message);
    const auto& contract = loaded.Value().Artifact().ValidatedContract().Contract();
    const auto recordsFlow = scenarios::FindFlowByAuthorKeyV1(contract, scenarios::ComparisonRecordsFlowKeyV1);
    Require(recordsFlow.IsValid(), "actual-removal records flow is missing");
    auto before = ObserveLoaded(loaded.Value(), artifact.Value(), value, 0);
    const auto oldEpoch = loaded.Value().DeviceEpoch();
    const auto staleResource = loaded.Value().SharedResource(recordsFlow);
    const auto staleToken = loaded.Value().AvailableAfter(recordsFlow);

    auto removed = runtime::RecoverStaticComposition(
        loaded.Value(), sge4_5::runtime::DeviceRecoveryMode::ForceRemovalForTest);
    if (!removed) throw std::runtime_error(removed.Error().stage + ": " + removed.Error().message);
    const auto& report = removed.Value();
    Require(report.device.forcedRemoval &&
            loaded.Value().State() == sge4_5::runtime::DeviceRuntimeState::AwaitingAdapter &&
            loaded.Value().DeviceEpoch() == oldEpoch && loaded.Value().LeafCount() == 0 && loaded.Value().ResourceCount() == 0 &&
            report.allRuntimeObjectsReleased && !report.allRuntimeObjectsRematerialized,
            "actual RemoveDevice did not produce the canonical AwaitingAdapter boundary");
    auto stale = runtime::ValidateStaticCompositionHandleEpoch(loaded.Value(), staleResource, staleToken);
    Require(!stale && stale.Error().stage == "static-runtime/device-state",
            "removed-domain resource/token was not rejected");
    auto rejected = runtime::SubmitStaticComposition(loaded.Value(), {1, {}});
    Require(!rejected && rejected.Error().stage == "static-runtime/device-state",
            "removed-domain submission was not rejected");
    auto retry = runtime::RecoverStaticComposition(
        loaded.Value(), sge4_5::runtime::DeviceRecoveryMode::RetryAdapterReacquisition);
    Require(static_cast<bool>(retry) && !retry.Value().device.adapterReacquired &&
            loaded.Value().State() == sge4_5::runtime::DeviceRuntimeState::AwaitingAdapter,
            "removed WARP LUID exclusion was not preserved by retry");

    // The old process deliberately keeps the removed LUID excluded. On a
    // single-adapter WARP environment there is therefore no eligible adapter in
    // this process, which is the canonical AwaitingAdapter result. The runner
    // qualifies rematerialization in a separate fresh process after this one exits.
    Require(loaded.Value().State() == sge4_5::runtime::DeviceRuntimeState::AwaitingAdapter,
            "removed domain escaped AwaitingAdapter quarantine");

    std::cout << "Actual RemoveDevice quarantine qualification passed.\n";
    std::cout << "Old process: AwaitingAdapter with removed WARP LUID excluded; stale handles and submit rejected.\n";
    std::cout << "Pre-removal S12 Observation V2 readback "
              << base::ToHex(before.report.readbackDigest) << '\n';
    std::cout << "Removal reason=" << report.device.removalReason
              << " DRED breadcrumbs=" << report.device.dredBreadcrumbNodeCount
              << " page-fault allocations=" << report.device.dredPageFaultAllocationCount << '\n';
}
}

int main(int argc, char** argv)
{
    try
    {
        bool controlled = argc == 1;
        bool actual = argc == 1;
        bool freshRematerialization = false;
        std::filesystem::path emitPath;
        for (int i = 1; i < argc; ++i)
        {
            const std::string_view arg = argv[i];
            if (arg == "--controlled") controlled = true;
            else if (arg == "--actual-removal") actual = true;
            else if (arg == "--fresh-rematerialization") freshRematerialization = true;
            else if (arg == "--emit" && i + 1 < argc) emitPath = std::filesystem::path(argv[++i]);
            else throw std::runtime_error("usage: 76_Spiral1RecoveryTests [--controlled] [--actual-removal] [--fresh-rematerialization] [--emit <path>]");
        }
        if (!controlled && !actual && !freshRematerialization)
            throw std::runtime_error("at least one recovery mode is required");
        if (actual && freshRematerialization)
            throw std::runtime_error("actual removal and fresh rematerialization must run in separate processes");
        const int evidenceModeCount = (controlled ? 1 : 0) + (freshRematerialization ? 1 : 0);
        if (!emitPath.empty() && (evidenceModeCount != 1 || actual))
            throw std::runtime_error("--emit requires exactly one of --controlled or --fresh-rematerialization");

        auto corpusValue = corpus::BuildQualificationCorpusV1();
        if (!corpusValue) throw std::runtime_error(corpusValue.Error());
        auto nativePrograms = leaf::BuildNativeProgramSetV1();
        if (!nativePrograms) throw std::runtime_error(nativePrograms.Error().stage + ": " + nativePrograms.Error().message);
        if (controlled)
        {
            auto evidence = RunControlledRecovery(corpusValue.Value(), nativePrograms.Value());
            if (!emitPath.empty())
            {
                auto written = base::WriteAllBytes(emitPath, evidence);
                if (!written) throw std::runtime_error(written.Error());
            }
            std::cout << "Controlled recovery qualification passed for S00, S07, S12 and S15.15.\n";
            std::cout << "Controlled evidence SHA-256: " << base::ToHex(base::Sha256(evidence)) << '\n';
        }
        if (actual) RunActualRemoval(corpusValue.Value(), nativePrograms.Value());
        if (freshRematerialization)
        {
            auto evidence = RunFreshProcessRematerialization(corpusValue.Value(), nativePrograms.Value());
            if (!emitPath.empty())
            {
                auto written = base::WriteAllBytes(emitPath, evidence);
                if (!written) throw std::runtime_error(written.Error());
            }
            std::cout << "Fresh-process evidence SHA-256: " << base::ToHex(base::Sha256(evidence)) << '\n';
        }
        std::cout << "Stage 12 Spiral 1 recovery qualification passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "CU5 recovery failure: " << error.what() << '\n';
        return 1;
    }
}
