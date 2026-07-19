#include "../00_Foundation/FileIO.h"
#include "../00_Foundation/Sha256.h"
#include "../16_FrozenCompositionArtifact/FrozenCompositionReader.h"
#include "../17_CompositionContract/CompositionContract.h"
#include "../18_CompositionPlan/CompositionPlan.h"
#include "../19_CompositionVerifier/CompositionVerifier.h"
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
namespace corpus = sge4_5::spiral1::corpus;
namespace leaf = sge4_5::spiral1::leaf;
namespace scenarios = sge4_5::spiral1::scenarios;
namespace canonical = sge4_5::composition::canonical;
namespace planning = sge4_5::composition::canonical::planning;
namespace verification = sge4_5::composition::canonical::verification;

void Require(bool condition, std::string_view message)
{
    if (!condition) throw std::runtime_error(std::string(message));
}

const corpus::QualificationCaseV1& FindCase(
    const corpus::QualificationCorpusV1& values,
    std::string_view scenario,
    std::uint32_t subscenario = 0)
{
    const auto found = std::find_if(values.cases.begin(), values.cases.end(), [&](const auto& value) {
        return value.scenarioId == scenario && value.subscenarioIndex == subscenario;
    });
    if (found == values.cases.end()) throw std::runtime_error("qualification case was not found");
    return *found;
}

canonical::EndpointReferenceDeclaration Ref(std::string leafKey, std::string endpointKey)
{
    return {std::move(leafKey), std::move(endpointKey)};
}

canonical::LeafPackageDeclaration Leaf(
    std::string key,
    const std::vector<std::byte>& bytes,
    std::initializer_list<canonical::LeafEndpointDeclaration> endpoints)
{
    canonical::LeafPackageDeclaration value;
    value.stableKey = std::move(key);
    value.packageBytes = bytes;
    value.endpoints.assign(endpoints.begin(), endpoints.end());
    return value;
}

base::Result<std::vector<std::byte>, std::string> FreezeMutated(
    const scenarios::FrozenComparisonScenarioV1& artifact,
    bool swapComparisonInputs,
    bool duplicateWriter)
{
    canonical::ContractBuildInput input;
    input.leaves = {
        Leaf(std::string(scenarios::SourceLeafKeyV1), artifact.corpusSourcePackageBytes,
            {{0,"input-points"},{1,"reference-points"}}),
        Leaf(std::string(scenarios::MatrixLeafKeyV1), artifact.matrixLeaf.packageBytes,
            {{0,"input"},{1,"output"}}),
        Leaf(std::string(scenarios::DirectLeafKeyV1), artifact.directPgaLeaf.packageBytes,
            {{0,"input"},{1,"output"}}),
        Leaf(std::string(scenarios::ComparisonLeafKeyV1), artifact.comparisonPackageBytes,
            {{0,"reference"},{1,"matrix"},{2,"direct"},{3,"records"}})};

    canonical::ResourceFlowDeclaration inputPoints;
    inputPoints.stableKey = std::string(scenarios::InputPointsFlowKeyV1);
    inputPoints.boundary = canonical::ResourceBoundary::Internal;
    inputPoints.producer = Ref(std::string(scenarios::SourceLeafKeyV1), "input-points");
    inputPoints.consumers = {
        Ref(std::string(scenarios::MatrixLeafKeyV1), "input"),
        Ref(std::string(scenarios::DirectLeafKeyV1), "input")};

    canonical::ResourceFlowDeclaration reference;
    reference.stableKey = std::string(scenarios::ReferencePointsFlowKeyV1);
    reference.boundary = canonical::ResourceBoundary::Internal;
    reference.producer = Ref(std::string(scenarios::SourceLeafKeyV1), "reference-points");
    reference.consumers = {Ref(std::string(scenarios::ComparisonLeafKeyV1), "reference")};

    canonical::ResourceFlowDeclaration matrix;
    matrix.stableKey = std::string(scenarios::MatrixOutputFlowKeyV1);
    matrix.boundary = canonical::ResourceBoundary::Internal;
    matrix.producer = Ref(std::string(scenarios::MatrixLeafKeyV1), "output");
    matrix.consumers = {Ref(std::string(scenarios::ComparisonLeafKeyV1), swapComparisonInputs ? "direct" : "matrix")};

    canonical::ResourceFlowDeclaration direct;
    direct.stableKey = std::string(scenarios::DirectOutputFlowKeyV1);
    direct.boundary = canonical::ResourceBoundary::Internal;
    direct.producer = Ref(std::string(scenarios::DirectLeafKeyV1), "output");
    direct.consumers = {Ref(std::string(scenarios::ComparisonLeafKeyV1), swapComparisonInputs ? "matrix" : "direct")};

    canonical::ResourceFlowDeclaration records;
    records.stableKey = std::string(scenarios::ComparisonRecordsFlowKeyV1);
    records.boundary = canonical::ResourceBoundary::CompositionOutput;
    records.producer = Ref(std::string(scenarios::ComparisonLeafKeyV1), "records");
    input.resources = {inputPoints, reference, matrix, direct, records};

    if (duplicateWriter)
    {
        canonical::ResourceFlowDeclaration duplicate;
        duplicate.stableKey = "spiral1/flow/illegal-second-writer";
        duplicate.boundary = canonical::ResourceBoundary::CompositionOutput;
        duplicate.producer = Ref(std::string(scenarios::MatrixLeafKeyV1), "output");
        input.resources.push_back(std::move(duplicate));
    }

    auto built = canonical::BuildCompositionContract(std::move(input));
    if (!built) return base::Result<std::vector<std::byte>, std::string>::Failure(built.Error().stage + ": " + built.Error().message);
    auto raw = planning::ProposeCompositionPlan(built.Value());
    if (!raw) return base::Result<std::vector<std::byte>, std::string>::Failure(raw.Error().stage + ": " + raw.Error().message);
    auto verified = verification::VerifyAndSeal(built.Value(), raw.Value());
    if (!verified) return base::Result<std::vector<std::byte>, std::string>::Failure(verified.Error().stage + ": " + verified.Error().message);
    auto frozen = verification::FreezeVerifiedComposition(built.Value(), verified.Value());
    if (!frozen) return base::Result<std::vector<std::byte>, std::string>::Failure(frozen.Error().stage + ": " + frozen.Error().message);
    return base::Result<std::vector<std::byte>, std::string>::Success(std::move(frozen).Value());
}

std::vector<std::byte> BuildEvidence(
    const scenarios::FrozenComparisonScenarioV1& artifact)
{
    return scenarios::SerializeFrozenComparisonScenarioEvidenceV1(artifact);
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
        const auto& value = FindCase(corpusValue.Value(), "S07");
        auto artifact = scenarios::BuildFrozenComparisonScenarioV1(value, nativePrograms.Value());
        if (!artifact) throw std::runtime_error(artifact.Error().stage + ": " + artifact.Error().message);
        auto valid = scenarios::ValidateFrozenComparisonScenarioV1(artifact.Value(), value, nativePrograms.Value());
        if (!valid) throw std::runtime_error(valid.Error().stage + ": " + valid.Error().message);

        auto frozen = verification::ReadVerifiedFrozenComposition(artifact.Value().frozenCompositionBytes);
        Require(static_cast<bool>(frozen), "Frozen Composition did not pass independent read verification");
        const auto& contract = frozen.Value().ValidatedContract().Contract();
        const auto& plan = frozen.Value().VerifiedPlan().Plan();
        Require(contract.leaves.size() == 4, "Four-Leaf Composition does not have four Leaves");
        Require(contract.resources.size() == 5, "Four-Leaf Composition does not have five Resource Flows");
        Require(plan.schedule.size() == 4, "Four-Leaf Composition schedule is not complete");
        Require(plan.allocations.size() == 5, "Four-Leaf Composition allocation plan is not complete");
        Require(plan.signals.size() == 4, "Four-Leaf Composition internal signal count differs");
        Require(plan.waits.size() == 5, "Four-Leaf Composition wait count differs");
        const auto inputFlow = scenarios::FindFlowByAuthorKeyV1(contract, scenarios::InputPointsFlowKeyV1);
        Require(inputFlow.IsValid(), "InputPoints flow is missing");
        Require(contract.resources[inputFlow.value].consumers.size() == 2,
                "InputPoints flow is not an authoritative fan-out");

        auto swapped = FreezeMutated(artifact.Value(), true, false);
        Require(static_cast<bool>(swapped), "role-swapped contract must remain a structurally valid Level 4 v1 composition");
        auto roleMutation = artifact.Value();
        roleMutation.frozenCompositionBytes = std::move(swapped).Value();
        roleMutation.artifactIdentity = scenarios::ComputeFrozenComparisonScenarioIdentityV1(roleMutation);
        Require(!scenarios::ValidateFrozenComparisonScenarioV1(roleMutation, value, nativePrograms.Value()),
                "Matrix/Direct comparison endpoint swap escaped the Spiral 1 role authority gate");

        auto duplicateWriter = FreezeMutated(artifact.Value(), false, true);
        Require(!duplicateWriter, "one producer endpoint was illegally bound as a second writer");

        auto constantMutation = artifact.Value();
        constantMutation.matrixLeaf.lockedConstantPayload[0] ^= std::byte{1};
        constantMutation.artifactIdentity = scenarios::ComputeFrozenComparisonScenarioIdentityV1(constantMutation);
        Require(!scenarios::ValidateFrozenComparisonScenarioV1(constantMutation, value, nativePrograms.Value()),
                "tampered Matrix locked constant escaped the aggregate authority gate");

        auto compositionMutation = artifact.Value();
        compositionMutation.frozenCompositionBytes.back() ^= std::byte{1};
        compositionMutation.artifactIdentity = scenarios::ComputeFrozenComparisonScenarioIdentityV1(compositionMutation);
        Require(!scenarios::ValidateFrozenComparisonScenarioV1(compositionMutation, value, nativePrograms.Value()),
                "corrupt Frozen Composition escaped validation");

        if (argc == 3 && std::string_view(argv[1]) == "--emit")
        {
            const auto evidence = BuildEvidence(artifact.Value());
            auto written = base::WriteAllBytes(std::filesystem::path(argv[2]), evidence);
            if (!written) throw std::runtime_error(written.Error());
        }
        else if (argc != 1)
            throw std::runtime_error("usage: 74_Spiral1CompositionTests [--emit <path>]");

        std::cout << "Stage 10 Four-Leaf Frozen Comparison authority qualification passed.\n";
        std::cout << "Leaves: 4, Resource Flows: 5, InputPoints fan-out consumers: 2\n";
        std::cout << "Frozen Comparison identity: " << base::ToHex(artifact.Value().artifactIdentity) << '\n';
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "CU4 Stage10 failure: " << error.what() << '\n';
        return 1;
    }
}
