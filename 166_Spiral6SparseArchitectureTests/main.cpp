#include "../00_Foundation/FileIO.h"
#include "../154_SparseWorkSetSemantic/SparseWorkSetSemantic.h"
#include "../155_Spiral6SparseContracts/Spiral6SparseContracts.h"
#include "../156_Spiral6SparseExecution/Spiral6SparseExecution.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{
namespace semantic = sge4_5::spiral6::semantic;
namespace contract = sge4_5::spiral6::contract;
namespace execution = sge4_5::spiral6::execution;
namespace base = sge4_5::base;

void Require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

semantic::ExactSparseWorkSetV1 BuildSet(
    semantic::SparsePatternKindV1 pattern,
    std::uint32_t cardinality)
{
    auto result = semantic::BuildCanonicalSparseWorkSetV1(
        pattern, semantic::SparseCardinalityV1(cardinality));
    if (!result) throw std::runtime_error(result.Error().stage + ": " + result.Error().message);
    return std::move(result).Value();
}

void RunNegativeGates(const semantic::SparsePointTransformSemanticV1& semanticValue)
{
    const std::vector<std::uint32_t> duplicate{1u, 1u};
    const std::vector<std::uint32_t> unsorted{2u, 1u};
    const std::vector<std::uint32_t> outOfRange{semantic::WorkUniverseCountV1};
    Require(!semantic::BuildExactSparseWorkSetV1(duplicate), "Duplicate Sparse index was accepted.");
    Require(!semantic::BuildExactSparseWorkSetV1(unsorted), "Unsorted Sparse indices were accepted.");
    Require(!semantic::BuildExactSparseWorkSetV1(outOfRange), "Out-of-range Sparse index was accepted.");

    auto firstSet = BuildSet(semantic::SparsePatternKindV1::HashScatterPermutation, 65);
    auto secondSet = BuildSet(semantic::SparsePatternKindV1::UniformStride, 65);
    auto artifact = contract::BuildCompactIndexListArtifactV1(semanticValue, firstSet);
    Require(!contract::ValidateCompactIndexListArtifactForExecutionV1(
        artifact, semanticValue, secondSet), "Cross-set Compact Index artifact replay was accepted.");

    auto corruptedBytes = artifact.Bytes();
    corruptedBytes[corruptedBytes.size() / 2] ^= std::byte{0x01};
    Require(!contract::ParseCompactIndexListArtifactV1(corruptedBytes),
            "Corrupted Compact Index artifact was accepted.");

    auto deterministic = contract::BuildCompactIndexListArtifactV1(semanticValue, firstSet);
    Require(artifact.Bytes() == deterministic.Bytes(),
            "Compact Index artifact bytes are not deterministic.");
}

std::vector<execution::CompactIndexArchitectureCaseV1> BuildArchitectureCases(
    const semantic::SparsePointTransformSemanticV1& semanticValue)
{
    std::vector<execution::CompactIndexArchitectureCaseV1> cases;
    const auto add = [&](semantic::SparsePatternKindV1 pattern, std::uint32_t cardinality)
    {
        auto set = BuildSet(pattern, cardinality);
        auto artifact = contract::BuildCompactIndexListArtifactV1(semanticValue, set);
        cases.emplace_back(pattern, std::move(set), std::move(artifact));
    };

    add(semantic::SparsePatternKindV1::PrefixControl, 0);
    add(semantic::SparsePatternKindV1::HashScatterPermutation, 1);
    add(semantic::SparsePatternKindV1::PrefixControl, 65);
    add(semantic::SparsePatternKindV1::UniformStride, 65);
    add(semantic::SparsePatternKindV1::BlockClusteredPermutation, 65);
    add(semantic::SparsePatternKindV1::HashScatterPermutation, 65);
    add(semantic::SparsePatternKindV1::PrefixControl, 1024);
    add(semantic::SparsePatternKindV1::UniformStride, 1024);
    add(semantic::SparsePatternKindV1::BlockClusteredPermutation, 1024);
    add(semantic::SparsePatternKindV1::HashScatterPermutation, 1024);
    add(semantic::SparsePatternKindV1::HashScatterPermutation, 4095);
    add(semantic::SparsePatternKindV1::PrefixControl, 4096);
    return cases;
}
}

int main(int argc, char** argv)
{
    try
    {
        std::filesystem::path output;
        if (argc == 3 && std::string(argv[1]) == "--emit") output = argv[2];
        else throw std::runtime_error("Usage: 166_Spiral6SparseArchitectureTests --emit <evidence.bin>");

        const auto semanticValue = semantic::BuildCanonicalSparsePointTransformSemanticV1();
        RunNegativeGates(semanticValue);
        const auto cases = BuildArchitectureCases(semanticValue);
        auto report = execution::ExecuteCompactIndexListArchitectureV1(semanticValue, cases);
        if (!report)
            throw std::runtime_error(report.Error().stage + ": " + report.Error().message);
        Require(report.Value().cases.size() == cases.size(), "Sparse architecture case count mismatch.");
        for (const auto& observation : report.Value().cases)
        {
            Require(observation.activeReferenceQualified, "Active reference qualification failed.");
            Require(observation.inactiveSentinelByteStable, "Inactive sentinel byte stability failed.");
            Require(observation.activeHomogeneousWQualified, "Active homogeneous w qualification failed.");
            Require(observation.finiteQualified, "Sparse output contains a non-finite value.");
            Require(observation.activeWriteCount == observation.activeCount,
                    "Sparse active write count mismatch.");
        }

        const auto evidence = execution::SerializeCompactIndexArchitectureReportV1(report.Value());
        auto write = base::WriteAllBytes(output, evidence);
        if (!write) throw std::runtime_error(write.Error());

        std::cout << "Spiral 6 CU2 Compact Index List architecture passed.\n";
        std::cout << "Adapter: " << report.Value().adapterDescription << "\n";
        std::cout << "Architecture cases: " << report.Value().cases.size() << "\n";
        std::cout << "Patterns: PrefixControl, UniformStride, BlockClusteredPermutation, HashScatterPermutation\n";
        std::cout << "Compact index capacity: 4096 uint32 indices / 16384 bytes\n";
        std::cout << "Inactive output sentinel byte stability: qualified\n";
        std::cout << "Runtime sparse-policy decision: None\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Spiral 6 CU2 failure: " << error.what() << '\n';
        return 1;
    }
}
