#include "../00_Foundation/FileIO.h"
#include "../00_Foundation/Sha256.h"
#include "../133_Spiral4PerformanceExperiment/Spiral4PerformanceExperiment.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace perf = sge4_5::spiral4::performance;
namespace base = sge4_5::base;

namespace
{
enum class Mode
{
    None,
    SelfTest,
    Measure,
    Report
};

std::uint32_t ParseU32(const std::string& value, const char* name)
{
    std::size_t consumed = 0;
    const unsigned long parsed = std::stoul(value, &consumed, 10);
    if (consumed != value.size() || parsed > 0xFFFF'FFFFul)
    {
        throw std::runtime_error(std::string("invalid ") + name);
    }
    return static_cast<std::uint32_t>(parsed);
}

std::string ClassificationName(
    perf::TransitionClassificationV1 value)
{
    switch (value)
    {
    case perf::TransitionClassificationV1::NoObservedWinnerTransition:
        return "NoObservedWinnerTransition";
    case perf::TransitionClassificationV1::SingleStableWinnerTransition:
        return "SingleStableWinnerTransition";
    case perf::TransitionClassificationV1::MultipleWinnerTransitions:
        return "MultipleWinnerTransitions";
    case perf::TransitionClassificationV1::NoiseEquivalentOrUnresolved:
        return "NoiseEquivalentOrUnresolved";
    default:
        return "Unknown";
    }
}
}

int main(int argc, char** argv)
{
    try
    {
        Mode mode = Mode::None;
        std::filesystem::path output =
            "build/tests/spiral4-cu6/self-test";
        std::filesystem::path input;
        perf::MeasurementConfigV1 config;

        for (int index = 1; index < argc; ++index)
        {
            const std::string argument = argv[index];
            if (argument == "--self-test")
            {
                mode = Mode::SelfTest;
            }
            else if (argument == "--measure")
            {
                mode = Mode::Measure;
            }
            else if (argument == "--report")
            {
                mode = Mode::Report;
            }
            else if (argument == "--output")
            {
                if (index + 1 >= argc)
                {
                    throw std::runtime_error("missing --output value");
                }
                output = argv[++index];
            }
            else if (argument == "--input")
            {
                if (index + 1 >= argc)
                {
                    throw std::runtime_error("missing --input value");
                }
                input = argv[++index];
            }
            else if (argument == "--warmup-per-order")
            {
                if (index + 1 >= argc)
                {
                    throw std::runtime_error(
                        "missing --warmup-per-order value");
                }
                config.warmupCyclesPerOrder =
                    ParseU32(argv[++index], "warmup count");
            }
            else if (argument == "--measurement-per-order")
            {
                if (index + 1 >= argc)
                {
                    throw std::runtime_error(
                        "missing --measurement-per-order value");
                }
                config.measurementCyclesPerOrder =
                    ParseU32(argv[++index], "measurement count");
            }
            else if (argument == "--runs")
            {
                if (index + 1 >= argc)
                {
                    throw std::runtime_error("missing --runs value");
                }
                config.runCount = ParseU32(argv[++index], "run count");
            }
            else if (argument == "--adapter-index")
            {
                if (index + 1 >= argc)
                {
                    throw std::runtime_error(
                        "missing --adapter-index value");
                }
                config.adapterIndex =
                    ParseU32(argv[++index], "adapter index");
            }
            else if (argument == "--clock-policy")
            {
                if (index + 1 >= argc)
                {
                    throw std::runtime_error(
                        "missing --clock-policy value");
                }
                config.clockPolicy = argv[++index];
            }
            else
            {
                throw std::runtime_error("unknown argument: " + argument);
            }
        }

        if (mode == Mode::None)
        {
            throw std::runtime_error(
                "one of --self-test, --measure, or --report is required");
        }

        if (mode == Mode::SelfTest)
        {
            auto tested = perf::RunFormatAndDecisionSelfTestV1(output);
            if (!tested)
            {
                throw std::runtime_error(tested.Error());
            }
            auto bytes = base::ReadAllBytes(
                output / "spiral4_measurement_evidence_v1.bin");
            if (!bytes)
            {
                throw std::runtime_error(bytes.Error());
            }
            std::cout
                << "Spiral 4 CU6 format, corruption rejection, "
                   "14-order balance, and decision self-test passed.\n"
                << "Synthetic evidence SHA-256: "
                << base::ToHex(base::Sha256(bytes.Value()))
                << '\n';
            return 0;
        }

        if (mode == Mode::Measure)
        {
            std::filesystem::remove_all(output);
            std::filesystem::create_directories(output);
            auto evidence = perf::RunRealGpuMeasurementV1(config);
            if (!evidence)
            {
                throw std::runtime_error(evidence.Error());
            }
            auto written = perf::WriteMeasurementArtifactsV1(
                evidence.Value(),
                output);
            if (!written)
            {
                throw std::runtime_error(written.Error());
            }
            auto bytes = base::ReadAllBytes(
                output / "spiral4_measurement_evidence_v1.bin");
            if (!bytes)
            {
                throw std::runtime_error(bytes.Error());
            }
            std::cout
                << "Spiral 4 CU6 real GPU measurement passed.\n"
                << "Adapter: " << evidence.Value().adapter.description << '\n'
                << "Candidates: 7\n"
                << "Active Counts: 19\n"
                << "Balanced orders: 14\n"
                << "Samples per Candidate and Nf: "
                << perf::CanonicalOrderCountV1 *
                    config.measurementCyclesPerOrder *
                    config.runCount
                << '\n'
                << "Evidence SHA-256: "
                << base::ToHex(base::Sha256(bytes.Value()))
                << '\n'
                << "Runtime dispatch-dimension decision: None\n";
            return 0;
        }

        if (input.empty())
        {
            input = output / "spiral4_measurement_evidence_v1.bin";
        }
        auto evidence = perf::ReadMeasurementEvidenceV1(input);
        if (!evidence)
        {
            throw std::runtime_error(evidence.Error());
        }
        auto report = perf::WriteDecisionEvidenceReportV1(
            evidence.Value(),
            output);
        if (!report)
        {
            throw std::runtime_error(report.Error());
        }
        auto analysis = perf::AnalyzeDecisionEvidenceV1(evidence.Value());
        if (!analysis)
        {
            throw std::runtime_error(analysis.Error());
        }
        std::cout
            << "Spiral 4 CU6 Decision Evidence Report generated.\n"
            << "Fixed/Single: "
            << ClassificationName(
                analysis.Value().fixedSingleClassification)
            << '\n'
            << "Single/BestBatch: "
            << ClassificationName(
                analysis.Value().singleBatchClassification)
            << '\n'
            << "RuntimePolicyAuthorization: None\n"
            << "NextCapabilitySelection: DeferredByOwner\n"
            << "SelectionStatus: OWNER_DECISION_REQUIRED\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "Spiral 4 CU6 failure: "
            << error.what()
            << '\n';
        return 1;
    }
}
