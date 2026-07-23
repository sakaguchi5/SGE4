#include "../170_Spiral6PerformanceExperiment/Spiral6PerformanceExperiment.h"

#include "../00_Foundation/Sha256.h"

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace perf = sge4_5::spiral6::performance;
namespace base = sge4_5::base;

namespace
{
std::string RequireValue(int argc, char** argv, int& index, std::string_view option)
{
    if (index + 1 >= argc)
        throw std::invalid_argument(std::string(option) + " requires a value.");
    return argv[++index];
}

std::uint32_t ParseU32(const std::string& text, std::string_view option)
{
    std::size_t consumed = 0;
    const unsigned long value = std::stoul(text, &consumed, 10);
    if (consumed != text.size() || value > 0xffff'fffful)
        throw std::invalid_argument(std::string(option) + " is not a uint32 value.");
    return static_cast<std::uint32_t>(value);
}

double ParseDouble(const std::string& text, std::string_view option)
{
    std::size_t consumed = 0;
    const double value = std::stod(text, &consumed);
    if (consumed != text.size())
        throw std::invalid_argument(std::string(option) + " is not a floating-point value.");
    return value;
}

perf::TransitionProbeModeV1 ParseMode(const std::string& value)
{
    if (value == "checkpoint") return perf::TransitionProbeModeV1::CheckpointOnly;
    if (value == "bracketed") return perf::TransitionProbeModeV1::BracketedCalibration;
    throw std::invalid_argument("--mode must be checkpoint or bracketed.");
}
}

int main(int argc, char** argv)
{
    try
    {
        bool selfTest = false;
        bool probe = false;
        std::filesystem::path output;
        perf::TransitionProbeConfigV1 config;

        for (int index = 1; index < argc; ++index)
        {
            const std::string option = argv[index];
            if (option == "--self-test") selfTest = true;
            else if (option == "--probe") probe = true;
            else if (option == "--output") output = RequireValue(argc, argv, index, option);
            else if (option == "--mode") config.mode = ParseMode(RequireValue(argc, argv, index, option));
            else if (option == "--samples") config.sampleCount = ParseU32(RequireValue(argc, argv, index, option), option);
            else if (option == "--adapter-index") config.adapterIndex = ParseU32(RequireValue(argc, argv, index, option), option);
            else if (option == "--baseline-samples") config.baselineSampleCount = ParseU32(RequireValue(argc, argv, index, option), option);
            else if (option == "--sustained-samples") config.sustainedSampleCount = ParseU32(RequireValue(argc, argv, index, option), option);
            else if (option == "--transition-ratio") config.transitionRatio = ParseDouble(RequireValue(argc, argv, index, option), option);
            else if (option == "--nvml") config.enableNvmlTelemetry = true;
            else if (option == "--require-nvml")
            {
                config.enableNvmlTelemetry = true;
                config.requireNvmlTelemetry = true;
            }
            else if (option == "--nvml-index") config.nvmlDeviceIndex = ParseU32(RequireValue(argc, argv, index, option), option);
            else if (option == "--nvml-poll-ms") config.nvmlPollingIntervalMilliseconds = ParseU32(RequireValue(argc, argv, index, option), option);
            else if (option == "--stable-power-state") config.requestStablePowerState = true;
            else throw std::invalid_argument("Unknown option: " + option);
        }

        if (selfTest)
        {
            auto result = perf::RunTransitionProbeSelfTestV1();
            if (!result) throw std::runtime_error(result.Error());
            std::cout << "Spiral 6 intra-sample Transition Probe sequence, exact-set, and window-classification self-test passed.\n";
        }

        if (probe)
        {
            if (output.empty()) throw std::invalid_argument("--probe requires --output.");
            auto result = perf::RunTransitionProbeV1(config, output);
            if (!result) throw std::runtime_error(result.Error());
            const auto& summary = result.Value();
            std::cout << "Spiral 6 intra-sample Transition Probe completed.\n";
            std::cout << "Mode: " << perf::TransitionProbeModeNameV1(summary.mode) << '\n';
            std::cout << "Adapter: " << summary.adapterDescription << '\n';
            std::cout << "Samples: " << summary.sampleCount << '\n';
            std::cout << "Transition observed: " << (summary.transitionObserved ? "true" : "false") << '\n';
            if (summary.transitionObserved)
            {
                std::cout << "First slow ordinal: " << summary.firstSlowOrdinal << '\n';
                std::cout << "Transition window: "
                    << perf::TransitionProbeWindowNameV1(summary.transitionWindow) << '\n';
                std::cout << "Baseline sentinel path: "
                    << summary.baselineSentinelNanoseconds << " ns\n";
                std::cout << "First slow sentinel path: "
                    << summary.firstSlowSentinelNanoseconds << " ns\n";
            }
            std::cout << "Set identity: " << base::ToHex(summary.sparseSetIdentity) << '\n';
            std::cout << "NVML enabled: " << (summary.nvmlTelemetryEnabled ? "true" : "false") << '\n';
            std::cout << "NVML available: " << (summary.nvmlAvailable ? "true" : "false") << '\n';
            if (summary.nvmlTelemetryEnabled)
            {
                std::cout << "NVML device: " << summary.nvmlDeviceName << '\n';
                std::cout << "NVML UUID: " << summary.nvmlDeviceUuid << '\n';
                std::cout << "NVML timeline records: " << summary.nvmlTimelineRecordCount << '\n';
                std::cout << "NVML diagnosis: " << summary.nvmlTransitionDiagnosis << '\n';
                std::cout << "NVML timeline SHA-256: "
                    << base::ToHex(summary.nvmlTimelineIdentity) << '\n';
            }
            std::cout << "Stable Power State requested/applied: "
                << (summary.stablePowerStateRequested ? "true" : "false") << '/'
                << (summary.stablePowerStateApplied ? "true" : "false") << '\n';
            std::cout << "CSV SHA-256: " << base::ToHex(summary.csvIdentity) << '\n';
            std::cout << "Run report SHA-256: " << base::ToHex(summary.runReportIdentity) << '\n';
        }

        if (!selfTest && !probe)
            throw std::invalid_argument("Specify --self-test and/or --probe.");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Spiral 6 intra-sample Transition Probe failed: " << error.what() << '\n';
        return 1;
    }
}
