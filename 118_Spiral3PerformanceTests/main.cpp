#include "../109_Spiral3PerformanceExperiment/Spiral3PerformanceExperiment.h"
#include "../00_Foundation/FileIO.h"
#include "../00_Foundation/Sha256.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace perf = sge4_5::spiral3::performance;

namespace
{
std::string Hex(const sge4_5::base::Digest256& value)
{
    static constexpr char digits[] = "0123456789abcdef";
    std::string result(value.size() * 2, '0');
    for (std::size_t i = 0; i < value.size(); ++i)
    {
        const auto byte = std::to_integer<unsigned char>(value[i]);
        result[i * 2] = digits[byte >> 4];
        result[i * 2 + 1] = digits[byte & 15];
    }
    return result;
}
}

int main(int argc, char** argv)
{
    try
    {
        std::filesystem::path output = "build/tests/spiral3-cu6/self-test";
        for (int index = 1; index < argc; ++index)
        {
            const std::string argument = argv[index];
            if (argument == "--output")
            {
                if (index + 1 >= argc) throw std::runtime_error("missing --output value");
                output = argv[++index];
            }
            else throw std::runtime_error("unknown argument: " + argument);
        }

        auto tested = perf::RunFormatSelfTestV1(output);
        if (!tested) throw std::runtime_error(tested.Error());
        auto evidence = perf::ReadMeasurementEvidenceV1(output / "spiral3_measurement_evidence_v1.bin");
        if (!evidence) throw std::runtime_error(evidence.Error());
        auto analysis = perf::AnalyzeCrossoverV1(evidence.Value());
        if (!analysis) throw std::runtime_error(analysis.Error());
        if (analysis.Value().classification != perf::CrossoverClassificationV1::SingleStableWinnerTransition ||
            analysis.Value().transitions.size() != 1 ||
            analysis.Value().transitions[0].lowerReuseCount != 4 ||
            analysis.Value().transitions[0].upperReuseCount != 16)
            throw std::runtime_error("synthetic crossover authority differs from the frozen R4-R16 self-test");
        auto bytes = sge4_5::base::ReadAllBytes(output / "spiral3_measurement_evidence_v1.bin");
        if (!bytes) throw std::runtime_error(bytes.Error());
        std::cout << "Spiral 3 CU6 format, corruption rejection, six-order balance, and crossover self-test passed.\n";
        std::cout << Hex(sge4_5::base::Sha256(bytes.Value())) << '\n';
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Spiral 3 CU6 self-test failure: " << error.what() << '\n';
        return 1;
    }
}
