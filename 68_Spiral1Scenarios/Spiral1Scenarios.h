#pragma once

#include "../00_Foundation/Base.h"
#include "../14_D3D12Backend/D3D12Backend.h"
#include "../17_CompositionContract/CompositionContract.h"
#include "../22_CompositionRuntime/CompositionRuntime.h"
#include "../62_Spiral1Corpus/Spiral1Corpus.h"
#include "../66_Spiral1LeafCompiler/Spiral1LeafCompiler.h"
#include "../67_Spiral1Observer/Spiral1Observer.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace sge4_5::spiral1::scenarios
{
inline constexpr std::uint32_t FrozenComparisonScenarioSchemaVersionV1 = 1;

inline constexpr std::string_view SourceLeafKeyV1 = "spiral1/source";
inline constexpr std::string_view MatrixLeafKeyV1 = "spiral1/matrix";
inline constexpr std::string_view DirectLeafKeyV1 = "spiral1/direct-pga";
inline constexpr std::string_view ComparisonLeafKeyV1 = "spiral1/comparison";
inline constexpr std::string_view InputPointsFlowKeyV1 = "spiral1/flow/input-points";
inline constexpr std::string_view ReferencePointsFlowKeyV1 = "spiral1/flow/reference-points";
inline constexpr std::string_view MatrixOutputFlowKeyV1 = "spiral1/flow/matrix-output";
inline constexpr std::string_view DirectOutputFlowKeyV1 = "spiral1/flow/direct-output";
inline constexpr std::string_view ComparisonRecordsFlowKeyV1 = "spiral1/flow/comparison-records";

struct ScenarioErrorV1 final
{
    std::string stage;
    std::string message;
};

struct FrozenComparisonScenarioV1 final
{
    std::uint32_t schemaVersion = FrozenComparisonScenarioSchemaVersionV1;
    base::Digest256 caseIdentity{};
    std::uint32_t pointCount = 0;
    base::Digest256 observationContractIdentity{};
    leaf::FrozenRepresentationLeafV1 matrixLeaf;
    leaf::FrozenRepresentationLeafV1 directPgaLeaf;
    base::Digest256 corpusSourcePackageExecutionDigest{};
    base::Digest256 comparisonPackageExecutionDigest{};
    std::vector<std::byte> corpusSourcePackageBytes;
    std::vector<std::byte> comparisonPackageBytes;
    std::vector<std::byte> frozenCompositionBytes;
    base::Digest256 artifactIdentity{};
};

struct ComparisonExecutionResultV1 final
{
    std::vector<semantic::Float4Point> matrixOutput;
    std::vector<semantic::Float4Point> directPgaOutput;
    std::vector<contracts::PointComparisonRecordV1> comparisonRecords;
    contracts::ScenarioReportV1 report;
    base::Digest256 frozenCompositionDigest{};
};

[[nodiscard]] base::Result<FrozenComparisonScenarioV1, ScenarioErrorV1>
BuildFrozenComparisonScenarioV1(
    const corpus::QualificationCaseV1& qualificationCase,
    const leaf::NativeProgramSetV1& nativePrograms);

[[nodiscard]] base::Result<void, ScenarioErrorV1>
ValidateFrozenComparisonScenarioV1(
    const FrozenComparisonScenarioV1& artifact,
    const corpus::QualificationCaseV1& qualificationCase,
    const leaf::NativeProgramSetV1& nativePrograms);

[[nodiscard]] base::Result<ComparisonExecutionResultV1, ScenarioErrorV1>
ExecuteFrozenComparisonScenarioV1(
    const FrozenComparisonScenarioV1& artifact,
    const corpus::QualificationCaseV1& qualificationCase,
    d3d12::D3D12Backend& backend,
    std::uint64_t frameNumber = 0);

[[nodiscard]] base::Digest256 ComputeFrozenComparisonScenarioIdentityV1(
    const FrozenComparisonScenarioV1& artifact);
[[nodiscard]] std::vector<std::byte> SerializeFrozenComparisonScenarioEvidenceV1(
    const FrozenComparisonScenarioV1& artifact);

[[nodiscard]] composition::canonical::LeafPackageId FindLeafByAuthorKeyV1(
    const composition::canonical::PackageCompositionContract& contract,
    std::string_view authorKey) noexcept;
[[nodiscard]] composition::canonical::ResourceFlowId FindFlowByAuthorKeyV1(
    const composition::canonical::PackageCompositionContract& contract,
    std::string_view authorKey) noexcept;
}
