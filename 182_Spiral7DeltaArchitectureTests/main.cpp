#include "../00_Foundation/FileIO.h"
#include "../172_SparseTemporalDeltaSemantic/SparseTemporalDeltaSemantic.h"
#include "../173_Spiral7DeltaContracts/Spiral7DeltaContracts.h"
#include "../174_Spiral7DeltaExecution/Spiral7DeltaExecution.h"

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
namespace semantic = sge4_5::spiral7::semantic;
namespace contract = sge4_5::spiral7::contract;
namespace execution = sge4_5::spiral7::execution;
namespace sparse = sge4_5::spiral6::semantic;
namespace temporal = sge4_5::spiral5::semantic;
namespace base = sge4_5::base;

void Require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

semantic::ExactSparseWorkSetV1 BuildSet(std::span<const std::uint32_t> indices)
{
    auto result = sparse::BuildExactSparseWorkSetV1(indices);
    if (!result) throw std::runtime_error(result.Error().stage + ": " + result.Error().message);
    return std::move(result).Value();
}

semantic::ExactSparseWorkSetV1 BuildPatternSet(
    sparse::SparsePatternKindV1 pattern,
    std::uint32_t cardinality)
{
    auto result = sparse::BuildCanonicalSparseWorkSetV1(
        pattern, sparse::SparseCardinalityV1(cardinality));
    if (!result) throw std::runtime_error(result.Error().stage + ": " + result.Error().message);
    return std::move(result).Value();
}

semantic::ExactSparseWorkSetV1 BuildEmptySet()
{
    const std::vector<std::uint32_t> empty;
    return BuildSet(empty);
}

semantic::ExactSparseWorkSetV1 BuildModifiedPrefixOfSet(
    const semantic::ExactSparseWorkSetV1& set,
    std::uint32_t count)
{
    const std::uint32_t actual = std::min(count, set.Cardinality().Value());
    std::vector<std::uint32_t> indices(set.Indices().begin(), set.Indices().begin() + actual);
    return BuildSet(indices);
}

struct TimelineState final
{
    semantic::ExactSparseWorkSetV1 activeSet;
    std::vector<std::uint32_t> generations;
    std::vector<std::byte> historyBytes;
    std::uint32_t invocationIndex = 0;

    TimelineState()
        : activeSet(BuildEmptySet()),
          generations(semantic::BuildInitialInvalidGenerationsV1()),
          historyBytes(semantic::BuildCanonicalInactiveHistoryBytesV1()) {}
};

execution::CompactDeltaArchitectureCaseV1 AddCase(
    TimelineState& state,
    const semantic::SparseTemporalDeltaSemanticV1& semanticValue,
    std::string id,
    semantic::ExactSparseWorkSetV1 activeSet,
    semantic::ExactSparseWorkSetV1 modifiedSet,
    bool forceFullRebuild = false)
{
    auto invocationResult = semantic::BuildSparseDeltaInvocationV1(
        state.invocationIndex,
        state.activeSet,
        activeSet,
        modifiedSet,
        state.generations,
        forceFullRebuild);
    if (!invocationResult)
        throw std::runtime_error(invocationResult.Error().stage + ": " + invocationResult.Error().message);
    auto invocation = std::move(invocationResult).Value();
    auto artifact = contract::BuildCompactDeltaArtifactV1(semanticValue, invocation);

    execution::CompactDeltaArchitectureCaseV1 testCase(
        std::move(id), invocation, artifact, state.historyBytes);

    const auto expected = semantic::BuildCpuReferenceOutputV1(invocation);
    state.historyBytes = temporal::SerializePointRecordsV1(expected);
    state.generations = invocation.ItemGenerations();
    state.activeSet = invocation.ActiveSet();
    ++state.invocationIndex;
    return testCase;
}

void RunNegativeGates(const semantic::SparseTemporalDeltaSemanticV1& semanticValue)
{
    const std::vector<std::uint32_t> duplicate{1u, 1u};
    const std::vector<std::uint32_t> unsorted{2u, 1u};
    const std::vector<std::uint32_t> outOfRange{semantic::WorkUniverseCountV1};
    Require(!sparse::BuildExactSparseWorkSetV1(duplicate), "Duplicate Active index was accepted.");
    Require(!sparse::BuildExactSparseWorkSetV1(unsorted), "Unsorted Active indices were accepted.");
    Require(!sparse::BuildExactSparseWorkSetV1(outOfRange), "Out-of-range Active index was accepted.");

    const auto active = BuildPatternSet(sparse::SparsePatternKindV1::PrefixControl, 64);
    const std::vector<std::uint32_t> illegalModifiedIndices{100u};
    const auto illegalModified = BuildSet(illegalModifiedIndices);
    const auto initialGenerations = semantic::BuildInitialInvalidGenerationsV1();
    Require(!semantic::BuildSparseDeltaInvocationV1(
        0, active, active, illegalModified, initialGenerations),
        "Modified Survivor outside A_t intersect A_(t-1) was accepted.");

    TimelineState state;
    auto firstCase = AddCase(
        state, semanticValue, "Negative.Baseline",
        BuildPatternSet(sparse::SparsePatternKindV1::PrefixControl, 64), BuildEmptySet());
    auto deterministic = contract::BuildCompactDeltaArtifactV1(
        semanticValue, firstCase.invocation);
    Require(firstCase.artifact.Bytes() == deterministic.Bytes(),
            "Compact Delta artifact bytes are not deterministic.");

    auto corruptedBytes = firstCase.artifact.Bytes();
    corruptedBytes[corruptedBytes.size() / 2] ^= std::byte{0x01};
    Require(!contract::ParseCompactDeltaArtifactV1(corruptedBytes),
            "Corrupted Compact Delta artifact was accepted.");

    auto secondInvocationResult = semantic::BuildSparseDeltaInvocationV1(
        state.invocationIndex,
        state.activeSet,
        BuildPatternSet(sparse::SparsePatternKindV1::PrefixControl, 65),
        BuildEmptySet(),
        state.generations);
    if (!secondInvocationResult)
        throw std::runtime_error(secondInvocationResult.Error().stage + ": " + secondInvocationResult.Error().message);
    Require(!contract::ValidateCompactDeltaArtifactForExecutionV1(
        firstCase.artifact, semanticValue, secondInvocationResult.Value()),
        "Cross-invocation Compact Delta artifact replay was accepted.");
}

std::vector<execution::CompactDeltaArchitectureCaseV1> BuildArchitectureCases(
    const semantic::SparseTemporalDeltaSemanticV1& semanticValue)
{
    TimelineState state;
    std::vector<execution::CompactDeltaArchitectureCaseV1> cases;

    cases.push_back(AddCase(
        state, semanticValue, "InitialPrefix64",
        BuildPatternSet(sparse::SparsePatternKindV1::PrefixControl, 64), BuildEmptySet()));
    cases.push_back(AddCase(
        state, semanticValue, "HoldPrefix64",
        BuildPatternSet(sparse::SparsePatternKindV1::PrefixControl, 64), BuildEmptySet()));
    {
        const std::vector<std::uint32_t> dirty{17u};
        cases.push_back(AddCase(
            state, semanticValue, "DirtyOne",
            BuildPatternSet(sparse::SparsePatternKindV1::PrefixControl, 64), BuildSet(dirty)));
    }
    cases.push_back(AddCase(
        state, semanticValue, "ActivateOne",
        BuildPatternSet(sparse::SparsePatternKindV1::PrefixControl, 65), BuildEmptySet()));
    cases.push_back(AddCase(
        state, semanticValue, "DeactivateOne",
        BuildPatternSet(sparse::SparsePatternKindV1::PrefixControl, 64), BuildEmptySet()));
    {
        std::vector<std::uint32_t> shifted;
        for (std::uint32_t index = 1; index <= 64; ++index) shifted.push_back(index);
        const std::vector<std::uint32_t> dirty{1u, 2u, 3u};
        cases.push_back(AddCase(
            state, semanticValue, "BalancedChurn",
            BuildSet(shifted), BuildSet(dirty)));
    }
    cases.push_back(AddCase(
        state, semanticValue, "PatternMigration",
        BuildPatternSet(sparse::SparsePatternKindV1::HashScatterPermutation, 64), BuildEmptySet()));
    cases.push_back(AddCase(
        state, semanticValue, "EmptyReset",
        BuildEmptySet(), BuildEmptySet()));
    cases.push_back(AddCase(
        state, semanticValue, "ActivateHash1024",
        BuildPatternSet(sparse::SparsePatternKindV1::HashScatterPermutation, 1024), BuildEmptySet()));
    {
        const auto current = BuildPatternSet(sparse::SparsePatternKindV1::HashScatterPermutation, 1024);
        cases.push_back(AddCase(
            state, semanticValue, "Dirty256Of1024",
            current, BuildModifiedPrefixOfSet(current, 256)));
    }
    cases.push_back(AddCase(
        state, semanticValue, "DeactivateToHash256",
        BuildPatternSet(sparse::SparsePatternKindV1::HashScatterPermutation, 256), BuildEmptySet()));
    cases.push_back(AddCase(
        state, semanticValue, "HoldHash256",
        BuildPatternSet(sparse::SparsePatternKindV1::HashScatterPermutation, 256), BuildEmptySet()));
    {
        const auto current = BuildPatternSet(sparse::SparsePatternKindV1::HashScatterPermutation, 256);
        cases.push_back(AddCase(
            state, semanticValue, "FullInvalidation256",
            current, current));
    }
    cases.push_back(AddCase(
        state, semanticValue, "MigrateToUniform256",
        BuildPatternSet(sparse::SparsePatternKindV1::UniformStride, 256), BuildEmptySet()));

    return cases;
}
} // namespace

int main(int argc, char** argv)
{
    try
    {
        std::filesystem::path output;
        if (argc == 3 && std::string(argv[1]) == "--emit") output = argv[2];
        else throw std::runtime_error(
            "Usage: 182_Spiral7DeltaArchitectureTests --emit <evidence.bin>");

        const auto semanticValue = semantic::BuildCanonicalSparseTemporalDeltaSemanticV1();
        RunNegativeGates(semanticValue);
        const auto cases = BuildArchitectureCases(semanticValue);
        auto report = execution::ExecuteCompactDeltaArchitectureV1(semanticValue, cases);
        if (!report)
            throw std::runtime_error(report.Error().stage + ": " + report.Error().message);
        Require(report.Value().cases.size() == cases.size(), "Delta architecture case count mismatch.");
        for (const auto& observation : report.Value().cases)
        {
            Require(observation.activeReferenceQualified, "Active reference qualification failed.");
            Require(observation.retainedHistoryByteStable, "Retained history bytes changed.");
            Require(observation.inactiveSentinelByteStable, "Inactive sentinel byte stability failed.");
            Require(observation.updateAndClearQualified, "Update/Clear transition qualification failed.");
            Require(observation.activeHomogeneousWQualified, "Active homogeneous w qualification failed.");
            Require(observation.finiteQualified, "Delta output contains a non-finite value.");
            Require(observation.updateCount + observation.clearCount == observation.transitionCount,
                    "Delta transition action count mismatch.");
        }

        const auto evidence = execution::SerializeCompactDeltaArchitectureReportV1(report.Value());
        auto write = base::WriteAllBytes(output, evidence);
        if (!write) throw std::runtime_error(write.Error());

        std::cout << "Spiral 7 CU2 Compact Delta History architecture passed.\n";
        std::cout << "Adapter: " << report.Value().adapterDescription << "\n";
        std::cout << "Architecture cases: " << report.Value().cases.size() << "\n";
        std::cout << "Transitions: initial, hold, dirty, activate, deactivate, churn, migration, reset\n";
        std::cout << "Compact Delta capacity: 4096 uint2 records / 32768 bytes\n";
        std::cout << "Write set: exactly T_t = W_t union R_t; retained H_t bytes are untouched\n";
        std::cout << "Runtime transition-policy decision: None\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Spiral 7 CU2 failure: " << error.what() << '\n';
        return 1;
    }
}
