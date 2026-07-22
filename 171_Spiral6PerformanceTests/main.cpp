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
    if (index + 1 >= arguments.size()) throw std::runtime_error("Command-line option requires a value.");
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
            else if (argument == "--warmup-per-order") config.warmupCyclesPerOrder = ParseU32(RequireValue(arguments, i));
            else if (argument == "--measurement-per-order") config.measurementCyclesPerOrder = ParseU32(RequireValue(arguments, i));
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
            std::cout << "Spiral 6 CU6 format, corruption rejection, six-order balance, crossover, and Pattern-sensitivity self-test passed.\n";
            return 0;
        }

        if (mode == Mode::Measure)
        {
            auto evidence = perf::RunRealGpuMeasurementV1(config);
            if (!evidence) throw std::runtime_error(evidence.Error());
            auto written = perf::WriteMeasurementArtifactsV1(evidence.Value(), output);
            if (!written) throw std::runtime_error(written.Error());
            std::cout << "Spiral 6 CU6 real GPU measurement passed.\n";
            std::cout << "Adapter: " << evidence.Value().adapter.description << '\n';
            std::cout << "Patterns: PrefixControl, UniformStride, BlockClusteredPermutation, HashScatterPermutation\n";
            std::cout << "K: 1,8,32,64,128,256,512,1024,2048,3072,4096\n";
            std::cout << "Candidates: A.DenseMembershipMaskPredicate, B.CompactIndexListDispatch, C.ActiveBlockListLocalMask\n";
            std::cout << "Primary metric: GpuCandidatePathNanoseconds\n";
            std::cout << "Evidence: " << (output / "spiral6_measurement_evidence_v1.bin").string() << '\n';
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
            std::cout << "[RANKING] Pattern=" << perf::PatternNameV1(value.pattern)
                      << " K=" << value.activeCount
                      << " blocks=" << value.activeBlockCount
                      << " winners=" << WinnerText(value.globalWinnerSet)
                      << " A=" << value.medianGpuCandidatePathNanoseconds[0] << " ns"
                      << " B=" << value.medianGpuCandidatePathNanoseconds[1] << " ns"
                      << " C=" << value.medianGpuCandidatePathNanoseconds[2] << " ns\n";
        }
        for (const auto& value : analysis.Value().patternTransitions)
        {
            std::cout << "[CROSSOVER] Pattern=" << perf::PatternNameV1(value.pattern)
                      << " A/B=" << TransitionName(value.denseCompactClassification)
                      << " B/C=" << TransitionName(value.compactBlockClassification) << '\n';
        }
        std::cout << "[PATTERN-SENSITIVITY] SameKWinnerDependsOnPattern="
                  << (analysis.Value().sameCardinalityWinnerDependsOnPattern ? "true" : "false") << '\n';
        std::cout << "RuntimePolicyAuthorization: None\n";
        std::cout << "NextCapabilitySelection: DeferredByOwner\n";
        std::cout << "SelectionStatus: OWNER_DECISION_REQUIRED\n";
        std::cout << "Decision report SHA-256: "
                  << FileDigest(output / "SPIRAL6_DECISION_EVIDENCE_REPORT.md") << '\n';
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Spiral 6 CU6 failure: " << error.what() << '\n';
        return 1;
    }
}
