#include "../00_Foundation/FileIO.h"
#include "../00_Foundation/Sha256.h"
#include "../134_TemporalStateSemantic/TemporalStateSemantic.h"
#include "../135_Spiral5TemporalContracts/Spiral5TemporalContracts.h"
#include "../136_Spiral5TemporalExecution/Spiral5TemporalExecution.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace base = sge4_5::base;
namespace semantic = sge4_5::spiral5::semantic;
namespace contract = sge4_5::spiral5::contract;
namespace execution = sge4_5::spiral5::execution;

namespace
{
void Require(bool condition, std::string_view message)
{
    if (!condition) throw std::runtime_error(std::string(message));
}

std::string Hex(const base::Digest256& value)
{
    return base::ToHex(value);
}

std::uint32_t CountUpdates(const semantic::UpdateScheduleV1& schedule)
{
    return static_cast<std::uint32_t>(std::count_if(
        schedule.invocations.begin(), schedule.invocations.end(),
        [](const auto& invocation) {
            return invocation.event == semantic::UpdateEventV1::Update;
        }));
}

execution::TemporalScheduleExecutionInputV1 BuildInput(
    const semantic::TemporalStateSemanticV1& temporalSemantic,
    semantic::UpdateScheduleV1 schedule)
{
    auto artifact = contract::BuildCu2GlobalMotorHistoryArtifactV1(
        temporalSemantic, schedule);
    auto parsed = contract::ParseGlobalMotorHistoryArtifactV1(artifact.Bytes());
    Require(static_cast<bool>(parsed), "Temporal artifact round-trip parse failed.");
    Require(parsed.Value().ArtifactIdentity() == artifact.ArtifactIdentity(),
            "Temporal artifact round-trip identity changed.");
    auto valid = contract::ValidateGlobalMotorHistoryArtifactForExecutionV1(
        artifact, temporalSemantic, schedule);
    Require(static_cast<bool>(valid), "Temporal artifact execution validation failed.");
    return {std::move(schedule), std::move(artifact)};
}
}

int main(int argc, char** argv)
{
    try
    {
        std::filesystem::path emitPath;
        for (int index = 1; index < argc; ++index)
        {
            const std::string argument = argv[index];
            if (argument == "--emit")
            {
                if (index + 1 >= argc) throw std::runtime_error("missing --emit value");
                emitPath = argv[++index];
            }
            else throw std::runtime_error("unknown argument: " + argument);
        }

        const auto temporalSemantic = semantic::BuildCanonicalTemporalStateSemanticV1();
        const auto semanticBytes = semantic::SerializeTemporalStateSemanticV1(temporalSemantic);
        Require(!semanticBytes.empty(), "Temporal Semantic serialization is empty.");

        auto p1 = semantic::BuildPeriodicUpdateScheduleV1(1);
        auto p4 = semantic::BuildPeriodicUpdateScheduleV1(4);
        auto p64 = semantic::BuildPeriodicUpdateScheduleV1(64);
        auto irregular = semantic::BuildNamedUpdateScheduleV1("Irregular");
        Require(p1 && p4 && p64 && irregular, "Canonical CU2 schedule construction failed.");
        Require(CountUpdates(p1.Value()) == 128, "P1 update count mismatch.");
        Require(CountUpdates(p4.Value()) == 32, "P4 update count mismatch.");
        Require(CountUpdates(p64.Value()) == 2, "P64 update count mismatch.");
        Require(CountUpdates(irregular.Value()) == 9, "Irregular update count mismatch.");

        auto invalidInitial = p4.Value();
        invalidInitial.invocations[0].event = semantic::UpdateEventV1::Hold;
        invalidInitial.identity = {};
        auto invalidInitialResult = semantic::ValidateUpdateScheduleV1(invalidInitial);
        Require(!invalidInitialResult && invalidInitialResult.Error().stage == "schedule/initial",
                "Initial Hold schedule was not rejected.");

        auto invalidGeneration = p4.Value();
        invalidGeneration.invocations[5].sourceGeneration += 1;
        invalidGeneration.identity = {};
        auto invalidGenerationResult = semantic::ValidateUpdateScheduleV1(invalidGeneration);
        Require(!invalidGenerationResult && invalidGenerationResult.Error().stage == "schedule/generation",
                "Generation mutation was not rejected.");

        auto corruptionArtifact = contract::BuildCu2GlobalMotorHistoryArtifactV1(
            temporalSemantic, p4.Value());
        auto corruptedBytes = corruptionArtifact.Bytes();
        corruptedBytes[24] ^= std::byte{0x01};
        auto corrupted = contract::ParseGlobalMotorHistoryArtifactV1(corruptedBytes);
        Require(!corrupted, "Corrupted Temporal artifact was accepted.");

        std::vector<execution::TemporalScheduleExecutionInputV1> inputs;
        inputs.push_back(BuildInput(temporalSemantic, std::move(p1).Value()));
        inputs.push_back(BuildInput(temporalSemantic, std::move(p4).Value()));
        inputs.push_back(BuildInput(temporalSemantic, std::move(p64).Value()));
        inputs.push_back(BuildInput(temporalSemantic, std::move(irregular).Value()));

        auto executed = execution::RunGlobalMotorHistoryArchitecturesOnWarpV1(
            temporalSemantic, inputs);
        if (!executed)
            throw std::runtime_error(executed.Error().stage + ": " + executed.Error().message);

        Require(executed.Value().schedules.size() == 4, "CU2 WARP schedule result count mismatch.");
        std::size_t invocationCount = 0;
        for (const auto& schedule : executed.Value().schedules)
        {
            Require(schedule.invocations.size() == semantic::TimelineLengthV1,
                    "CU2 WARP schedule did not execute 128 invocations.");
            invocationCount += schedule.invocations.size();
            for (const auto& invocation : schedule.invocations)
            {
                Require(invocation.historyValid, "CU2 observation contains invalid history.");
                Require(invocation.holdHistoryByteStable, "CU2 Hold changed Global Motor history bytes.");
                Require(invocation.outputMatchesReference, "CU2 output differs from CPU reference.");
                Require(invocation.outputMatchesLatestUpdate,
                        "CU2 output differs from the last-update-wins result.");
                Require(invocation.expectedHierarchyDispatchX == invocation.observedHierarchyDispatchX,
                        "CU2 GPU Dispatch argument differs from the Frozen schedule.");
            }
        }
        Require(invocationCount == 512, "CU2 WARP invocation total mismatch.");

        const auto evidence = execution::SerializeGlobalMotorHistoryArchitectureEvidenceV1(
            executed.Value());
        if (!emitPath.empty())
        {
            auto written = base::WriteAllBytes(emitPath, evidence);
            if (!written) throw std::runtime_error(written.Error());
        }

        std::cout << "Spiral 5 CU2 Global Motor history architecture passed.\n";
        std::cout << "Adapter: " << executed.Value().adapterDescription << '\n';
        std::cout << "Schedules: P1, P4, P64, Irregular\n";
        std::cout << "Timeline invocations: 4 x 128 = 512\n";
        std::cout << "GPU update/hold Dispatch arguments: qualified\n";
        std::cout << "Global Motor history Hold stability: qualified\n";
        std::cout << "Retained-history completion ordinals: qualified\n";
        std::cout << "Runtime temporal-policy decision: None\n";
        std::cout << "CU2 architecture evidence SHA-256: " << Hex(base::Sha256(evidence)) << '\n';
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Spiral 5 CU2 failure: " << error.what() << '\n';
        return 1;
    }
}
