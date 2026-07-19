#include "../00_Foundation/FileIO.h"
#include "../00_Foundation/Sha256.h"
#include "../19_CompositionVerifier/CompositionVerifier.h"
#include "../61_Spiral1Contracts/Spiral1Contracts.h"
#include "../62_Spiral1Corpus/Spiral1Corpus.h"
#include "../66_Spiral1LeafCompiler/Spiral1LeafCompiler.h"
#include "../68_Spiral1Scenarios/Spiral1Scenarios.h"

#include <algorithm>
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
namespace verification = sge4_5::composition::canonical::verification;
namespace contracts = sge4_5::spiral1::contracts;
namespace corpus = sge4_5::spiral1::corpus;
namespace leaf = sge4_5::spiral1::leaf;
namespace scenarios = sge4_5::spiral1::scenarios;

void Require(bool condition, std::string_view message)
{
    if (!condition) throw std::runtime_error(std::string(message));
}
void WriteDigest(base::BinaryWriter& writer, const base::Digest256& value)
{
    writer.WriteBytes(value);
}

void RequireRejected(
    scenarios::FrozenComparisonScenarioV1 mutation,
    const corpus::QualificationCaseV1& value,
    const leaf::NativeProgramSetV1& nativePrograms,
    std::string_view label)
{
    mutation.artifactIdentity = scenarios::ComputeFrozenComparisonScenarioIdentityV1(mutation);
    auto result = scenarios::ValidateFrozenComparisonScenarioV1(mutation, value, nativePrograms);
    if (result) throw std::runtime_error(std::string(label) + " mutation escaped the final authority gate");
}

std::vector<std::byte> BuildFreezeEvidence(
    const corpus::QualificationCorpusV1& corpusValue,
    const leaf::NativeProgramSetV1& nativePrograms)
{
    base::BinaryWriter writer;
    constexpr std::string_view domain = "SGE4-5.Spiral1.CU5.ArchitectureFinalFreeze.V1";
    writer.WriteU32(static_cast<std::uint32_t>(domain.size()));
    writer.WriteBytes(std::as_bytes(std::span(domain.data(), domain.size())));
    WriteDigest(writer, corpusValue.contractIdentity);
    WriteDigest(writer, contracts::ObservationContractIdentityV2());
    WriteDigest(writer, nativePrograms.representationTargetProfile.identity);
    writer.WriteU32(static_cast<std::uint32_t>(corpusValue.cases.size()));
    writer.WriteU32(static_cast<std::uint32_t>(corpus::TotalPointCount(corpusValue)));

    bool ranMutations = false;
    for (const auto& value : corpusValue.cases)
    {
        auto artifact = scenarios::BuildFrozenComparisonScenarioV1(value, nativePrograms);
        if (!artifact) throw std::runtime_error(artifact.Error().stage + ": " + artifact.Error().message);
        auto valid = scenarios::ValidateFrozenComparisonScenarioV1(artifact.Value(), value, nativePrograms);
        if (!valid) throw std::runtime_error(valid.Error().stage + ": " + valid.Error().message);
        auto frozen = verification::ReadVerifiedFrozenComposition(artifact.Value().frozenCompositionBytes);
        if (!frozen) throw std::runtime_error(frozen.Error().stage + ": " + frozen.Error().message);
        const auto& contract = frozen.Value().ValidatedContract().Contract();
        const auto& plan = frozen.Value().VerifiedPlan().Plan();
        Require(contract.leaves.size() == 4 && contract.resources.size() == 5,
                "final freeze found a non-canonical Four-Leaf contract");
        Require(plan.schedule.size() == 4 && plan.allocations.size() == 5,
                "final freeze found an incomplete verified Composition Plan");
        Require(artifact.Value().matrixLeaf.semanticIdentity == artifact.Value().directPgaLeaf.semanticIdentity &&
                artifact.Value().matrixLeaf.candidateIdentity != artifact.Value().directPgaLeaf.candidateIdentity,
                "dual Representation identity relation changed");

        writer.WriteU32(static_cast<std::uint32_t>(value.scenarioId.size()));
        writer.WriteBytes(std::as_bytes(std::span(value.scenarioId.data(), value.scenarioId.size())));
        writer.WriteU32(value.subscenarioIndex);
        writer.WriteU32(static_cast<std::uint32_t>(value.inputPoints.size()));
        WriteDigest(writer, value.caseIdentity);
        WriteDigest(writer, artifact.Value().artifactIdentity);
        WriteDigest(writer, base::Sha256(artifact.Value().frozenCompositionBytes));
        WriteDigest(writer, artifact.Value().matrixLeaf.semanticIdentity);
        WriteDigest(writer, artifact.Value().matrixLeaf.candidateIdentity);
        WriteDigest(writer, artifact.Value().directPgaLeaf.candidateIdentity);
        WriteDigest(writer, artifact.Value().matrixLeaf.packageExecutionDigest);
        WriteDigest(writer, artifact.Value().directPgaLeaf.packageExecutionDigest);
        WriteDigest(writer, artifact.Value().corpusSourcePackageExecutionDigest);
        WriteDigest(writer, artifact.Value().comparisonPackageExecutionDigest);
        writer.WriteU32(static_cast<std::uint32_t>(contract.leaves.size()));
        writer.WriteU32(static_cast<std::uint32_t>(contract.resources.size()));
        writer.WriteU32(static_cast<std::uint32_t>(plan.schedule.size()));
        writer.WriteU32(static_cast<std::uint32_t>(plan.allocations.size()));

        if (!ranMutations && value.scenarioId == "S07" && value.subscenarioIndex == 0)
        {
            auto matrixPayload = artifact.Value();
            matrixPayload.matrixLeaf.lockedConstantPayload.at(0) ^= std::byte{1};
            RequireRejected(std::move(matrixPayload), value, nativePrograms, "Matrix locked payload");

            auto directPayload = artifact.Value();
            directPayload.directPgaLeaf.lockedConstantPayload.at(0) ^= std::byte{1};
            RequireRejected(std::move(directPayload), value, nativePrograms, "Direct PGA locked payload");

            auto matrixSeal = artifact.Value();
            matrixSeal.matrixLeaf.representationSeal.at(0) ^= std::byte{1};
            RequireRejected(std::move(matrixSeal), value, nativePrograms, "Matrix verifier seal");

            auto directCandidate = artifact.Value();
            directCandidate.directPgaLeaf.candidateIdentity.at(0) ^= std::byte{1};
            RequireRejected(std::move(directCandidate), value, nativePrograms, "Direct PGA candidate identity");

            auto observation = artifact.Value();
            observation.observationContractIdentity.at(0) ^= std::byte{1};
            RequireRejected(std::move(observation), value, nativePrograms, "Observation Contract identity");

            auto composition = artifact.Value();
            composition.frozenCompositionBytes.back() ^= std::byte{1};
            RequireRejected(std::move(composition), value, nativePrograms, "Frozen Composition bytes");

            auto sourcePackage = artifact.Value();
            sourcePackage.corpusSourcePackageBytes.back() ^= std::byte{1};
            RequireRejected(std::move(sourcePackage), value, nativePrograms, "Corpus Source Package bytes");

            auto comparisonPackage = artifact.Value();
            comparisonPackage.comparisonPackageBytes.back() ^= std::byte{1};
            RequireRejected(std::move(comparisonPackage), value, nativePrograms, "Comparison Package bytes");
            ranMutations = true;
        }
    }
    Require(ranMutations, "final authority mutation suite did not run");
    writer.WriteU32(8); // normalized mutation rejection count
    writer.WriteU32(16); // S1-I01 through S1-I16 qualified by CU1-CU5 gates
    writer.WriteU32(2);  // S1-I17 and S1-I18 remain for CU6
    return std::move(writer).Take();
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
        auto evidence = BuildFreezeEvidence(corpusValue.Value(), nativePrograms.Value());
        if (argc == 3 && std::string_view(argv[1]) == "--emit")
        {
            auto written = base::WriteAllBytes(std::filesystem::path(argv[2]), evidence);
            if (!written) throw std::runtime_error(written.Error());
        }
        else if (argc != 1)
            throw std::runtime_error("usage: 77_Spiral1FreezeTests [--emit <path>]");
        std::cout << "Stage 12 Spiral 1 architecture authority and determinism freeze passed.\n";
        std::cout << "Cases: " << corpusValue.Value().cases.size()
                  << ", points: " << corpus::TotalPointCount(corpusValue.Value()) << '\n';
        std::cout << "Mutation rejections: 8; qualified invariants: S1-I01 through S1-I16.\n";
        std::cout << "Freeze evidence SHA-256: " << base::ToHex(base::Sha256(evidence)) << '\n';
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "CU5 freeze failure: " << error.what() << '\n';
        return 1;
    }
}
