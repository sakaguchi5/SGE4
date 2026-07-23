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

perf::EquivalentSetAuditScheduleV1 ParseSchedule(const std::string& value)
{
    if (value == "balanced")
        return perf::EquivalentSetAuditScheduleV1::BalancedInterleaved;
    if (value == "fixed")
        return perf::EquivalentSetAuditScheduleV1::FixedPatternMajor;
    throw std::invalid_argument("--schedule must be balanced or fixed.");
}
}

int main(int argc, char** argv)
{
    try
    {
        bool selfTest = false;
        bool audit = false;
        std::filesystem::path output;
        perf::EquivalentSetAuditConfigV1 config;

        for (int index = 1; index < argc; ++index)
        {
            const std::string option = argv[index];
            if (option == "--self-test") selfTest = true;
            else if (option == "--audit") audit = true;
            else if (option == "--output") output = RequireValue(argc, argv, index, option);
            else if (option == "--schedule") config.schedule = ParseSchedule(RequireValue(argc, argv, index, option));
            else if (option == "--rounds") config.roundCount = ParseU32(RequireValue(argc, argv, index, option), option);
            else if (option == "--adapter-index") config.adapterIndex = ParseU32(RequireValue(argc, argv, index, option), option);
            else throw std::invalid_argument("Unknown option: " + option);
        }

        if (selfTest)
        {
            auto result = perf::RunEquivalentSetAuditScheduleSelfTestV1();
            if (!result) throw std::runtime_error(result.Error());
            std::cout << "Spiral 6 Equivalent Set Audit schedule, exact-set, Artifact, and position-balance self-test passed.\n";
        }

        if (audit)
        {
            if (output.empty()) throw std::invalid_argument("--audit requires --output.");
            auto result = perf::RunEquivalentSetAuditV1(config, output);
            if (!result) throw std::runtime_error(result.Error());
            const auto& summary = result.Value();
            std::cout << "Spiral 6 Equivalent Set Audit run completed.\n";
            std::cout << "Schedule: " << perf::EquivalentSetAuditScheduleNameV1(summary.schedule) << '\n';
            std::cout << "Adapter: " << summary.adapterDescription << '\n';
            std::cout << "Samples: " << summary.sampleCount << '\n';
            std::cout << "K=1 Set identity: " << base::ToHex(summary.k1SparseSetIdentity) << '\n';
            std::cout << "K=4096 Set identity: " << base::ToHex(summary.k4096SparseSetIdentity) << '\n';
            std::cout << "CSV SHA-256: " << base::ToHex(summary.csvIdentity) << '\n';
            std::cout << "Run report SHA-256: " << base::ToHex(summary.runReportIdentity) << '\n';
        }

        if (!selfTest && !audit)
            throw std::invalid_argument("Specify --self-test and/or --audit.");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Spiral 6 Equivalent Set Audit failed: " << error.what() << '\n';
        return 1;
    }
}
