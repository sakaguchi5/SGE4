#include "../00_Foundation/FileIO.h"
#include "../00_Foundation/Sha256.h"
#include "../152_Spiral5PerformanceExperiment/Spiral5PerformanceExperiment.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace perf = sge4_5::spiral5::performance;

namespace
{
std::string RequireValue(const std::vector<std::string>& arguments, std::size_t& index)
{
    if (index + 1 >= arguments.size()) throw std::runtime_error("Command-line option requires a value.");
    return arguments[++index];
}

std::uint32_t ParseU32(std::string_view value)
{
    std::size_t consumed = 0;
    const unsigned long long parsed = std::stoull(std::string(value), &consumed, 10);
    if (consumed != value.size() || parsed > 0xffffffffull) throw std::runtime_error("Invalid unsigned integer option.");
    return static_cast<std::uint32_t>(parsed);
}

std::string TransitionName(perf::TransitionClassificationV1 value)
{
    switch (value)
    {
    case perf::TransitionClassificationV1::NoObservedWinnerTransition: return "NoObservedWinnerTransition";
    case perf::TransitionClassificationV1::SingleStableWinnerTransition: return "SingleStableWinnerTransition";
    case perf::TransitionClassificationV1::MultipleWinnerTransitions: return "MultipleWinnerTransitions";
    case perf::TransitionClassificationV1::NoiseEquivalentOrUnresolved: return "NoiseEquivalentOrUnresolved";
    }
    return "Unknown";
}

std::string WinnerText(const std::vector<perf::CandidateKindV1>& winners)
{
    std::string result;
    for (std::size_t i = 0; i < winners.size(); ++i)
    {
        if (i != 0) result += ',';
        result += perf::CandidateLetterV1(winners[i]);
    }
    return result;
}

std::string FileDigest(const std::filesystem::path& path)
{
    auto bytes = sge4_5::base::ReadAllBytes(path);
    if (!bytes) throw std::runtime_error(bytes.Error());
    return sge4_5::base::ToHex(sge4_5::base::Sha256(bytes.Value()));
}
}

int main(int argc, char** argv)
{
    try
    {
        std::vector<std::string> arguments;
        for (int i = 1; i < argc; ++i) arguments.emplace_back(argv[i]);
        enum class Mode { None, SelfTest, Measure, Report } mode = Mode::None;
        std::filesystem::path output;
        std::filesystem::path input;
        perf::MeasurementConfigV1 config;

        for (std::size_t i = 0; i < arguments.size(); ++i)
        {
            const std::string& argument = arguments[i];
            if (argument == "--self-test") mode = Mode::SelfTest;
            else if (argument == "--measure") mode = Mode::Measure;
            else if (argument == "--report") mode = Mode::Report;
            else if (argument == "--output") output = RequireValue(arguments, i);
            else if (argument == "--input") input = RequireValue(arguments, i);
            else if (argument == "--warmup-per-order") config.warmupTimelinesPerOrder = ParseU32(RequireValue(arguments, i));
            else if (argument == "--measurement-per-order") config.measurementTimelinesPerOrder = ParseU32(RequireValue(arguments, i));
            else if (argument == "--runs") config.runCount = ParseU32(RequireValue(arguments, i));
            else if (argument == "--adapter-index") config.adapterIndex = ParseU32(RequireValue(arguments, i));
            else if (argument == "--clock-policy") config.clockPolicy = RequireValue(arguments, i);
            else throw std::runtime_error("Unknown command-line option: " + argument);
        }

        if (mode == Mode::None) throw std::runtime_error("Select --self-test, --measure, or --report.");
        if (output.empty()) throw std::runtime_error("--output is required.");

        if (mode == Mode::SelfTest)
        {
            auto result = perf::RunFormatAndDecisionSelfTestV1(output);
            if (!result) throw std::runtime_error(result.Error());
            std::cout << "Spiral 5 CU6 format, corruption rejection, six-order balance, and crossover self-test passed.\n";
            return 0;
        }

        if (mode == Mode::Measure)
        {
            auto evidence = perf::RunRealGpuMeasurementV1(config);
            if (!evidence) throw std::runtime_error(evidence.Error());
            auto written = perf::WriteMeasurementArtifactsV1(evidence.Value(), output);
            if (!written) throw std::runtime_error(written.Error());
            std::cout << "Spiral 5 CU6 real GPU measurement passed.\n";
            std::cout << "Adapter: " << evidence.Value().adapter.description << '\n';
            std::cout << "Update intervals: U=1,2,4,8,16,32,64\n";
            std::cout << "Candidates: A.EveryInvocationRecompute, B.GlobalMotorHistoryReuse, C.FinalOutputHistoryReuse\n";
            std::cout << "Primary metric: GpuCandidateTimelineNanoseconds\n";
            std::cout << "Evidence: " << (output / "spiral5_measurement_evidence_v1.bin").string() << '\n';
            return 0;
        }

        if (input.empty()) throw std::runtime_error("--input is required for --report.");
        auto evidence = perf::ReadMeasurementEvidenceV1(input);
        if (!evidence) throw std::runtime_error(evidence.Error());
        auto report = perf::WriteDecisionEvidenceReportV1(evidence.Value(), output);
        if (!report) throw std::runtime_error(report.Error());
        auto analysis = perf::AnalyzeDecisionEvidenceV1(evidence.Value());
        if (!analysis) throw std::runtime_error(analysis.Error());
        for (const auto& value : analysis.Value().cases)
        {
            std::cout << "[RANKING] U=" << value.updateInterval
                      << " winners=" << WinnerText(value.globalWinnerSet)
                      << " A=" << value.medianGpuCandidateTimelineNanoseconds[0] << " ns"
                      << " B=" << value.medianGpuCandidateTimelineNanoseconds[1] << " ns"
                      << " C=" << value.medianGpuCandidateTimelineNanoseconds[2] << " ns\n";
        }
        std::cout << "[CROSSOVER] A/B " << TransitionName(analysis.Value().recomputeGlobalClassification) << '\n';
        std::cout << "[CROSSOVER] B/C " << TransitionName(analysis.Value().globalFinalClassification) << '\n';
        std::cout << "RuntimePolicyAuthorization: None\n";
        std::cout << "NextCapabilitySelection: DeferredByOwner\n";
        std::cout << "SelectionStatus: OWNER_DECISION_REQUIRED\n";
        std::cout << "Decision report SHA-256: "
                  << FileDigest(output / "SPIRAL5_DECISION_EVIDENCE_REPORT.md") << '\n';
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Spiral 5 CU6 failure: " << error.what() << '\n';
        return 1;
    }
}
