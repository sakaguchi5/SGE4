#include "../109_Spiral3PerformanceExperiment/Spiral3PerformanceExperiment.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace perf = sge4_5::spiral3::performance;

namespace
{
std::string Value(int& index, int argc, char** argv)
{
    if (index + 1 >= argc) throw std::runtime_error(std::string("missing value for ") + argv[index]);
    return argv[++index];
}
std::uint32_t U32(std::string_view text)
{
    std::size_t used = 0;
    const auto value = std::stoull(std::string(text), &used, 10);
    if (used != text.size() || value > 0xffffffffull) throw std::runtime_error("invalid unsigned integer argument");
    return static_cast<std::uint32_t>(value);
}
const char* Classification(perf::CrossoverClassificationV1 value)
{
    switch (value)
    {
    case perf::CrossoverClassificationV1::NoObservedWinnerTransition: return "NoObservedWinnerTransition";
    case perf::CrossoverClassificationV1::SingleStableWinnerTransition: return "SingleStableWinnerTransition";
    case perf::CrossoverClassificationV1::MultipleWinnerTransitions: return "MultipleWinnerTransitions";
    default: return "Unknown";
    }
}
}

int main(int argc, char** argv)
{
    try
    {
        bool selfTest = false;
        bool measure = false;
        bool evidence = false;
        bool report = false;
        std::filesystem::path output = "build/tests/spiral3/measurement";
        std::filesystem::path input;
        perf::MeasurementConfigV1 config;
        for (int index = 1; index < argc; ++index)
        {
            const std::string argument = argv[index];
            if (argument == "--self-test") selfTest = true;
            else if (argument == "--measure") measure = true;
            else if (argument == "--evidence") evidence = true;
            else if (argument == "--report") report = true;
            else if (argument == "--output") output = Value(index,argc,argv);
            else if (argument == "--input") input = Value(index,argc,argv);
            else if (argument == "--warmup-cycles") config.warmupCyclesPerOrder = U32(Value(index,argc,argv));
            else if (argument == "--measurement-cycles") config.measurementCyclesPerOrder = U32(Value(index,argc,argv));
            else if (argument == "--runs") config.runCount = U32(Value(index,argc,argv));
            else if (argument == "--adapter-index") config.adapterIndex = U32(Value(index,argc,argv));
            else if (argument == "--frame-index") config.frameIndex = U32(Value(index,argc,argv));
            else if (argument == "--clock-policy") config.clockPolicy = Value(index,argc,argv);
            else throw std::runtime_error("unknown argument: " + argument);
        }
        if (input.empty()) input = output / "spiral3_measurement_evidence_v1.bin";

        if (selfTest)
        {
            auto result = perf::RunFormatSelfTestV1(output);
            if (!result) throw std::runtime_error(result.Error());
            std::cout << "Spiral 3 CU6 format and Owner-deferred decision self-test passed.\n";
        }
        if (measure)
        {
            auto measured = perf::RunRealGpuMeasurementV1(config);
            if (!measured) throw std::runtime_error(measured.Error());
            auto written = perf::WriteMeasurementArtifactsV1(measured.Value(),output);
            if (!written) throw std::runtime_error(written.Error());
            std::cout << "Spiral 3 CU6 real GPU reuse-sweep measurement passed.\n"
                      << "Adapter: " << measured.Value().adapter.description << '\n'
                      << "Cases: " << measured.Value().cases.size() << '\n'
                      << "Evidence: " << (output / "spiral3_measurement_evidence_v1.bin").string() << '\n';
        }
        if (evidence || report)
        {
            auto loaded = perf::ReadMeasurementEvidenceV1(input);
            if (!loaded) throw std::runtime_error(loaded.Error());
            if (evidence) std::cout << "Spiral 3 binary measurement evidence validated. Cases: " << loaded.Value().cases.size() << '\n';
            if (report)
            {
                auto written = perf::WriteDecisionEvidenceReportV1(loaded.Value(),output);
                if (!written) throw std::runtime_error(written.Error());
                auto analysis = perf::AnalyzeCrossoverV1(loaded.Value());
                if (!analysis) throw std::runtime_error(analysis.Error());
                std::cout << "Spiral 3 CU6 Decision Evidence generated.\n"
                          << "CrossoverClassification = " << Classification(analysis.Value().classification) << '\n'
                          << "NextCapabilitySelection = DeferredByOwner\n"
                          << "SelectionStatus = OWNER_DECISION_REQUIRED\n";
            }
        }
        if (!selfTest && !measure && !evidence && !report)
            throw std::runtime_error("select --self-test, --measure, --evidence, or --report");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Spiral 3 launcher failure: " << error.what() << '\n';
        return 1;
    }
}
