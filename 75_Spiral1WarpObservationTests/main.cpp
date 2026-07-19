#include "../00_Foundation/FileIO.h"
#include "../00_Foundation/Sha256.h"
#include "../14_D3D12Backend/D3D12Backend.h"
#include "../61_Spiral1Contracts/Spiral1Contracts.h"
#include "../62_Spiral1Corpus/Spiral1Corpus.h"
#include "../66_Spiral1LeafCompiler/Spiral1LeafCompiler.h"
#include "../68_Spiral1Scenarios/Spiral1Scenarios.h"

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
namespace contracts = sge4_5::spiral1::contracts;
namespace corpus = sge4_5::spiral1::corpus;
namespace leaf = sge4_5::spiral1::leaf;
namespace scenarios = sge4_5::spiral1::scenarios;

void WriteDigest(base::BinaryWriter& writer, const base::Digest256& value)
{
    writer.WriteBytes(value);
}

std::vector<std::byte> RunCorpus(
    const corpus::QualificationCorpusV1& corpusValue,
    const leaf::NativeProgramSetV1& nativePrograms)
{
    base::BinaryWriter manifest;
    constexpr std::string_view domain = "SGE4-5.Spiral1.CU4.WarpCorpusEvidence.V1";
    manifest.WriteU32(static_cast<std::uint32_t>(domain.size()));
    manifest.WriteBytes(std::as_bytes(std::span(domain.data(), domain.size())));
    WriteDigest(manifest, corpusValue.contractIdentity);
    WriteDigest(manifest, nativePrograms.representationTargetProfile.identity);
    manifest.WriteU32(static_cast<std::uint32_t>(corpusValue.cases.size()));
    manifest.WriteU32(static_cast<std::uint32_t>(corpus::TotalPointCount(corpusValue)));

    std::uint32_t qualifiedCases = 0;
    std::uint32_t qualifiedPoints = 0;
    for (const auto& value : corpusValue.cases)
    {
        auto artifact = scenarios::BuildFrozenComparisonScenarioV1(value, nativePrograms);
        if (!artifact)
            throw std::runtime_error(artifact.Error().stage + ": " + artifact.Error().message);
        sge4_5::d3d12::D3D12Backend backend({true, true});
        auto result = scenarios::ExecuteFrozenComparisonScenarioV1(
            artifact.Value(), value, backend, qualifiedCases);
        if (!result)
            throw std::runtime_error(result.Error().stage + ": " + result.Error().message);
        if (result.Value().report.mismatchCount != 0 || result.Value().report.nonFiniteCount != 0)
            throw std::runtime_error("WARP corpus report contains mismatches");

        manifest.WriteU32(static_cast<std::uint32_t>(value.scenarioId.size()));
        manifest.WriteBytes(std::as_bytes(std::span(value.scenarioId.data(), value.scenarioId.size())));
        manifest.WriteU32(value.subscenarioIndex);
        manifest.WriteU32(static_cast<std::uint32_t>(value.inputPoints.size()));
        WriteDigest(manifest, value.caseIdentity);
        WriteDigest(manifest, artifact.Value().artifactIdentity);
        WriteDigest(manifest, result.Value().frozenCompositionDigest);
        const auto reportBytes = contracts::SerializeScenarioReportV1(result.Value().report);
        manifest.WriteU32(static_cast<std::uint32_t>(reportBytes.size()));
        manifest.WriteBytes(reportBytes);

        ++qualifiedCases;
        qualifiedPoints += static_cast<std::uint32_t>(value.inputPoints.size());
        std::cout << "[PASS] " << value.scenarioId << '.' << value.subscenarioIndex
                  << " points=" << value.inputPoints.size()
                  << " readback=" << base::ToHex(result.Value().report.readbackDigest) << '\n';
    }
    if (qualifiedCases != corpusValue.cases.size() || qualifiedPoints != corpus::TotalPointCount(corpusValue))
        throw std::runtime_error("WARP corpus accounting differs from Corpus V1");
    return std::move(manifest).Take();
}
}

int main(int argc, char** argv)
{
    try
    {
        auto corpusValue = corpus::BuildQualificationCorpusV1();
        if (!corpusValue) throw std::runtime_error(corpusValue.Error());
        auto nativePrograms = leaf::BuildNativeProgramSetV1();
        if (!nativePrograms) throw std::runtime_error(nativePrograms.Error().stage + ": " + nativePrograms.Error().message);
        auto evidence = RunCorpus(corpusValue.Value(), nativePrograms.Value());
        if (argc == 3 && std::string_view(argv[1]) == "--emit")
        {
            auto written = base::WriteAllBytes(std::filesystem::path(argv[2]), evidence);
            if (!written) throw std::runtime_error(written.Error());
        }
        else if (argc != 1)
            throw std::runtime_error("usage: 75_Spiral1WarpObservationTests [--emit <path>]");
        std::cout << "Stage 11 WARP S00-S15 Frozen Comparison corpus passed.\n";
        std::cout << "Cases: " << corpusValue.Value().cases.size()
                  << ", points: " << corpus::TotalPointCount(corpusValue.Value()) << '\n';
        std::cout << "Evidence SHA-256: " << base::ToHex(base::Sha256(evidence)) << '\n';
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "CU4 Stage11 failure: " << error.what() << '\n';
        return 1;
    }
}
