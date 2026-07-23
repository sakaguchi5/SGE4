#include "../187_Spiral7PerformanceExperiment/Spiral7PerformanceExperiment.h"

#include "../00_Foundation/Sha256.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace perf = sge4_5::spiral7::performance;
namespace base = sge4_5::base;

namespace
{
void WriteBytes(const std::filesystem::path& path, std::span<const std::byte> bytes)
{
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) throw std::runtime_error("Unable to open binary output: " + path.string());
    stream.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    if (!stream) throw std::runtime_error("Unable to write binary output: " + path.string());
}

std::vector<std::byte> ReadBytes(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("Unable to open binary input: " + path.string());
    stream.seekg(0, std::ios::end);
    const auto size = stream.tellg();
    if (size < 0) throw std::runtime_error("Unable to determine input size: " + path.string());
    stream.seekg(0, std::ios::beg);
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!stream) throw std::runtime_error("Unable to read binary input: " + path.string());
    return bytes;
}

void WriteText(const std::filesystem::path& path, std::string_view text)
{
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) throw std::runtime_error("Unable to open text output: " + path.string());
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!stream) throw std::runtime_error("Unable to write text output: " + path.string());
}

std::uint32_t ParseU32(std::string_view text, std::string_view name)
{
    std::size_t used = 0;
    const auto value = std::stoull(std::string(text), &used, 10);
    if (used != text.size() || value > 0xFFFF'FFFFull)
        throw std::runtime_error("Invalid " + std::string(name) + ": " + std::string(text));
    return static_cast<std::uint32_t>(value);
}

double ParseDouble(std::string_view text, std::string_view name)
{
    std::size_t used = 0;
    const auto value = std::stod(std::string(text), &used);
    if (used != text.size())
        throw std::runtime_error("Invalid " + std::string(name) + ": " + std::string(text));
    return value;
}

std::string WinnerSetText(const std::vector<perf::CandidateKindV1>& values)
{
    if (values.empty()) return "Unresolved";
    std::string result;
    for (const auto value : values)
    {
        if (!result.empty()) result += '/';
        result += perf::CandidateLetterV1(value);
    }
    return result;
}

void PrintDecisionSummary(
    const perf::MeasurementEvidenceV1& evidence,
    const perf::DecisionAnalysisV1& analysis,
    const base::Digest256& evidenceDigest)
{
    std::cout
        << "Spiral 7 CU6 real-GPU measurement passed.\n"
        << "Adapter: " << evidence.adapter.description << '\n'
        << "Canonical cases per run: " << perf::CanonicalCaseCountV1 << '\n'
        << "Runs: " << evidence.config.runCount << '\n'
        << "Accepted blocks: " << analysis.acceptedBlockCount
        << ", rejected attempts: " << analysis.rejectedBlockCount << '\n'
        << "Timestamp-resolution-censored samples: "
        << analysis.timestampResolutionCensoredSampleCount << '\n'
        << "Overall observed winner set: " << WinnerSetText(analysis.overallObservedWinnerSet) << '\n'
        << "Winner changes across Transition count: "
        << (analysis.winnerChangesAcrossTransitionCount ? "true" : "false") << '\n'
        << "Winner changes across Active count: "
        << (analysis.winnerChangesAcrossActiveCount ? "true" : "false") << '\n'
        << "Pattern-dependent winner at equal (A,T): "
        << (analysis.sameActiveAndTransitionWinnerDependsOnPattern ? "true" : "false") << '\n'
        << "Runtime Candidate-policy decision: None\n"
        << "Spiral 7 closure: Owner required\n"
        << "Measurement evidence SHA-256: " << base::ToHex(evidenceDigest) << '\n';
}
} // namespace

int main(int argc, char** argv)
{
    try
    {
        enum class Mode { SelfTest, Measure, Verify };
        Mode mode = Mode::SelfTest;
        std::filesystem::path evidencePath;
        std::filesystem::path csvPath;
        std::filesystem::path reportPath;
        std::filesystem::path selfTestEmitPath;
        perf::MeasurementConfigV1 config;

        for (int index = 1; index < argc; ++index)
        {
            const std::string_view argument = argv[index];
            const auto requireValue = [&](std::string_view name) -> std::string_view
            {
                if (index + 1 >= argc)
                    throw std::runtime_error("Missing value for " + std::string(name));
                return argv[++index];
            };

            if (argument == "--self-test") mode = Mode::SelfTest;
            else if (argument == "--measure") mode = Mode::Measure;
            else if (argument == "--verify") mode = Mode::Verify;
            else if (argument == "--evidence") evidencePath = requireValue(argument);
            else if (argument == "--csv") csvPath = requireValue(argument);
            else if (argument == "--report") reportPath = requireValue(argument);
            else if (argument == "--emit") selfTestEmitPath = requireValue(argument);
            else if (argument == "--adapter")
                config.adapterIndex = ParseU32(requireValue(argument), "adapter index");
            else if (argument == "--runs")
                config.runCount = ParseU32(requireValue(argument), "run count");
            else if (argument == "--cycles")
                config.measurementCyclesPerOrder = ParseU32(requireValue(argument), "cycle count");
            else if (argument == "--iterations")
                config.iterationsPerBatch = ParseU32(requireValue(argument), "iterations per batch");
            else if (argument == "--warmup")
                config.warmupIterations = ParseU32(requireValue(argument), "warmup iterations");
            else if (argument == "--attempts")
                config.maximumBlockAttempts = ParseU32(requireValue(argument), "maximum attempts");
            else if (argument == "--control-ratio")
                config.maximumControlRatio = ParseDouble(requireValue(argument), "control ratio");
            else if (argument == "--noise-floor")
                config.relativeNoiseFloor = ParseDouble(requireValue(argument), "noise floor");
            else if (argument == "--pair-agreement")
                config.minimumPairAgreement = ParseDouble(requireValue(argument), "pair agreement");
            else if (argument == "--stable-power-state")
                config.requestStablePowerStateDiagnostic = true;
            else throw std::runtime_error("Unknown argument: " + std::string(argument));
        }

        if (mode == Mode::SelfTest)
        {
            const auto result = perf::RunMeasurementFormatSelfTestV1();
            if (!result) throw std::runtime_error(result.Error());
            const auto bytes = perf::BuildSyntheticSelfTestEvidenceV1();
            if (!selfTestEmitPath.empty()) WriteBytes(selfTestEmitPath, bytes);
            const auto parsed = perf::DeserializeMeasurementEvidenceV1(bytes);
            if (!parsed) throw std::runtime_error(parsed.Error());
            const auto analysis = perf::AnalyzeMeasurementEvidenceV1(parsed.Value());
            std::cout
                << "Spiral 7 CU6 S7M2 format, corruption rejection, zero-dispatch censoring, balanced-order and decision-surface self-test passed.\n"
                << "Synthetic cases: " << analysis.cases.size() << '\n'
                << "Transition-surface events: " << analysis.transitionSurfaceEvents.size() << '\n'
                << "Active-surface events: " << analysis.activeSurfaceEvents.size() << '\n'
                << "Synthetic evidence SHA-256: " << base::ToHex(base::Sha256(bytes)) << '\n';
            return 0;
        }

        if (evidencePath.empty())
            throw std::runtime_error("--evidence path is required.");

        if (mode == Mode::Measure)
        {
            if (csvPath.empty() || reportPath.empty())
                throw std::runtime_error("--csv and --report paths are required for --measure.");
            auto measured = perf::RunRealGpuMeasurementV1(config);
            if (!measured) throw std::runtime_error(measured.Error());
            const auto bytes = perf::SerializeMeasurementEvidenceV1(measured.Value());
            const auto analysis = perf::AnalyzeMeasurementEvidenceV1(measured.Value());
            WriteBytes(evidencePath, bytes);
            WriteText(csvPath, perf::BuildDecisionCsvV1(measured.Value(), analysis));
            WriteText(reportPath, perf::BuildDecisionReportV1(measured.Value(), analysis));
            PrintDecisionSummary(measured.Value(), analysis, base::Sha256(bytes));
            return 0;
        }

        const auto bytes = ReadBytes(evidencePath);
        auto parsed = perf::DeserializeMeasurementEvidenceV1(bytes);
        if (!parsed) throw std::runtime_error(parsed.Error());
        const auto analysis = perf::AnalyzeMeasurementEvidenceV1(parsed.Value());
        if (!reportPath.empty())
            WriteText(reportPath, perf::BuildDecisionReportV1(parsed.Value(), analysis));
        PrintDecisionSummary(parsed.Value(), analysis, base::Sha256(bytes));
        std::cout << "Spiral 7 CU6 evidence reload and independent decision analysis passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Spiral 7 CU6 failed: " << error.what() << '\n';
        return 1;
    }
}
