#include "../00_Foundation/FileIO.h"
#include "../00_Foundation/Sha256.h"
#include "../170_Spiral6PerformanceExperiment/Spiral6PerformanceExperiment.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace perf = sge4_5::spiral6::performance;
namespace base = sge4_5::base;

namespace
{
std::string RequireValue(const std::vector<std::string>& arguments, std::size_t& index)
{
    if (index + 1 >= arguments.size())
        throw std::runtime_error("Command-line option requires a value.");
    return arguments[++index];
}

std::uint32_t ParseU32(std::string_view value)
{
    std::size_t consumed = 0;
    const unsigned long long parsed = std::stoull(std::string(value), &consumed, 10);
    if (consumed != value.size() || parsed > 0xffffffffull)
        throw std::runtime_error("Invalid unsigned integer option.");
    return static_cast<std::uint32_t>(parsed);
}

double ParseDouble(std::string_view value)
{
    std::size_t consumed = 0;
    const double parsed = std::stod(std::string(value), &consumed);
    if (consumed != value.size())
        throw std::runtime_error("Invalid floating-point option.");
    return parsed;
}

std::string TransitionName(perf::TransitionClassificationV2 value)
{
    switch (value)
    {
    case perf::TransitionClassificationV2::NoObservedWinnerTransition:
        return "NoObservedWinnerTransition";
    case perf::TransitionClassificationV2::SingleStableWinnerTransition:
        return "SingleStableWinnerTransition";
    case perf::TransitionClassificationV2::MultipleWinnerTransitions:
        return "MultipleWinnerTransitions";
    case perf::TransitionClassificationV2::NoiseEquivalentOrUnresolved:
        return "NoiseEquivalentOrUnresolved";
    }
    return "Unknown";
}

std::string WinnerText(const std::vector<perf::CandidateKindV2>& winners)
{
    std::string result;
    for (std::size_t index = 0; index < winners.size(); ++index)
    {
        if (index != 0) result += ',';
        result += perf::CandidateLetterV2(winners[index]);
    }
    return result;
}

std::string FileDigest(const std::filesystem::path& path)
{
    auto bytes = base::ReadAllBytes(path);
    if (!bytes) throw std::runtime_error(bytes.Error());
    return base::ToHex(base::Sha256(bytes.Value()));
}
}

int main(int argc, char** argv)
{
    try
    {
        std::vector<std::string> arguments;
        for (int index = 1; index < argc; ++index) arguments.emplace_back(argv[index]);
        enum class Mode { None, SelfTest, Measure, Report } mode = Mode::None;
        std::filesystem::path output;
        std::filesystem::path input;
        perf::MeasurementConfigV2 config;

        for (std::size_t index = 0; index < arguments.size(); ++index)
        {
            const std::string& argument = arguments[index];
            if (argument == "--self-test") mode = Mode::SelfTest;
            else if (argument == "--measure") mode = Mode::Measure;
            else if (argument == "--report") mode = Mode::Report;
            else if (argument == "--output") output = RequireValue(arguments, index);
            else if (argument == "--input") input = RequireValue(arguments, index);
            else if (argument == "--runs")
                config.runCount = ParseU32(RequireValue(arguments, index));
            else if (argument == "--cycles-per-order")
                config.measurementCyclesPerOrder = ParseU32(RequireValue(arguments, index));
            else if (argument == "--iterations-per-batch")
                config.iterationsPerBatch = ParseU32(RequireValue(arguments, index));
            else if (argument == "--warmup-iterations")
                config.warmupIterations = ParseU32(RequireValue(arguments, index));
            else if (argument == "--maximum-block-attempts")
                config.maximumBlockAttempts = ParseU32(RequireValue(arguments, index));
            else if (argument == "--adapter-index")
                config.adapterIndex = ParseU32(RequireValue(arguments, index));
            else if (argument == "--nvml-device-index")
                config.nvmlDeviceIndex = ParseU32(RequireValue(arguments, index));
            else if (argument == "--nvml-poll-ms")
                config.nvmlPollingIntervalMilliseconds = ParseU32(RequireValue(arguments, index));
            else if (argument == "--maximum-control-ratio")
                config.maximumControlRatio = ParseDouble(RequireValue(arguments, index));
            else if (argument == "--relative-noise-floor")
                config.relativeNoiseFloor = ParseDouble(RequireValue(arguments, index));
            else if (argument == "--minimum-pair-agreement")
                config.minimumPairAgreement = ParseDouble(RequireValue(arguments, index));
            else if (argument == "--disable-nvml-diagnostic")
                config.enableNvmlDiagnostic = false;
            else if (argument == "--stable-power-state-diagnostic")
                config.requestStablePowerStateDiagnostic = true;
            else
                throw std::runtime_error("Unknown command-line option: " + argument);
        }

        if (mode == Mode::None)
            throw std::runtime_error("Select --self-test, --measure, or --report.");
        if (output.empty()) throw std::runtime_error("--output is required.");

        if (mode == Mode::SelfTest)
        {
            auto result = perf::RunFormatScheduleAndStabilitySelfTestV2(output);
            if (!result) throw std::runtime_error(result.Error());
            std::cout << "Spiral 6 CU6 V2 relative-map format, corruption rejection, "
                         "balanced Case/Order, scale invariance, and rejected-block exclusion "
                         "self-test passed.\n";
            return 0;
        }

        if (mode == Mode::Measure)
        {
            auto evidence = perf::RunRealGpuMeasurementV2(config);
            if (!evidence) throw std::runtime_error(evidence.Error());
            auto written = perf::WriteMeasurementArtifactsV2(evidence.Value(), output);
            if (!written) throw std::runtime_error(written.Error());
            std::uint32_t accepted = 0;
            std::uint32_t rejected = 0;
            for (const auto& attempt : evidence.Value().attempts)
                attempt.accepted ? ++accepted : ++rejected;
            std::cout << "Spiral 6 CU6 V2 block-local paired real GPU measurement passed.\n";
            std::cout << "Adapter: " << evidence.Value().adapter.description << '\n';
            std::cout << "Accepted Pattern/K blocks: " << accepted << '\n';
            std::cout << "Rejected attempts retained: " << rejected << '\n';
            std::cout << "Primary evidence: BlockLocalPairedCandidateRatio\n";
            std::cout << "Absolute nanoseconds: DescriptiveOnly\n";
            std::cout << "NVML/P-state: DiagnosticOnly\n";
            std::cout << "Evidence: "
                      << (output / "spiral6_measurement_evidence_v2.bin").string() << '\n';
            return 0;
        }

        if (input.empty())
            throw std::runtime_error("--input is required for --report.");
        auto evidence = perf::ReadMeasurementEvidenceV2(input);
        if (!evidence) throw std::runtime_error(evidence.Error());
        auto report = perf::WriteDecisionEvidenceReportV2(evidence.Value(), output);
        if (!report) throw std::runtime_error(report.Error());
        auto analysis = perf::AnalyzeDecisionEvidenceV2(evidence.Value());
        if (!analysis) throw std::runtime_error(analysis.Error());

        for (const auto& value : analysis.Value().cases)
        {
            std::cout << "[RELATIVE] Pattern=" << perf::PatternNameV2(value.pattern)
                      << " K=" << value.activeCount
                      << " blocks=" << value.activeBlockCount
                      << " winners=" << WinnerText(value.globalWinnerSet)
                      << " A/B=" << value.denseVersusCompact.medianLeftOverRight
                      << " B/C=" << value.compactVersusActiveBlock.medianLeftOverRight
                      << " pairs=" << value.pairedObservationCount << '\n';
        }
        for (const auto& value : analysis.Value().patternTransitions)
        {
            std::cout << "[CROSSOVER] Pattern=" << perf::PatternNameV2(value.pattern)
                      << " A/B=" << TransitionName(value.denseCompactClassification)
                      << " B/C=" << TransitionName(value.compactBlockClassification) << '\n';
        }
        std::cout << "[BLOCKS] Accepted=" << analysis.Value().acceptedBlockCount
                  << " RejectedAttempts=" << analysis.Value().rejectedBlockCount << '\n';
        std::cout << "[PATTERN-SENSITIVITY] SameKWinnerDependsOnPattern="
                  << (analysis.Value().sameCardinalityWinnerDependsOnPattern ? "true" : "false")
                  << '\n';
        std::cout << "RuntimePolicyAuthorization: None\n";
        std::cout << "NextCapabilitySelection: DeferredByOwner\n";
        std::cout << "SelectionStatus: OWNER_DECISION_REQUIRED\n";
        std::cout << "Decision report SHA-256: "
                  << FileDigest(output / "SPIRAL6_RELATIVE_DECISION_MAP_V2.md") << '\n';
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Spiral 6 CU6 V2 failure: " << error.what() << '\n';
        return 1;
    }
}
