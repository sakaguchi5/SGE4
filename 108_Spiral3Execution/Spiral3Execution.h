#pragma once
#include "../00_Foundation/Base.h"
#include "../14_D3D12Backend/D3D12Backend.h"
#include "../22_CompositionRuntime/CompositionRuntime.h"
#include "../80_HierarchySemantic/HierarchySemantic.h"
#include "../81_Spiral2Contracts/Spiral2Contracts.h"
#include "../82_Spiral2Corpus/Spiral2Corpus.h"
#include "../102_Spiral3Corpus/Spiral3Corpus.h"
#include "../107_Spiral3Scenarios/Spiral3Scenarios.h"
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace sge4_5::spiral3::execution
{
struct ErrorMetricSummaryV1 final
{
    double maxAbsoluteError=0;
    double maxRelativeError=0;
    std::uint64_t maxUlpDistance=0;
    double rmsEuclideanError=0;
};

struct ReuseObservationReportV1 final
{
    std::uint64_t pointCount=0;
    std::uint64_t mismatchCount=0;
    std::uint64_t nonFiniteCount=0;
    std::uint64_t firstMismatchIndex=std::numeric_limits<std::uint64_t>::max();
    ErrorMetricSummaryV1 aReference,bReference,cReference;
    ErrorMetricSummaryV1 aB,aC,bC;
    double maxAbsoluteError=0;
    double maxRelativeError=0;
    std::uint64_t maxUlpDistance=0;
    double maxPairwiseError=0;
    double maxPairwiseRelativeError=0;
    std::uint64_t maxPairwiseUlpDistance=0;
    bool rigidityQualified=false;
    double maxAxisLengthError=0;
    double maxOrthogonalityError=0;
    double maxTranslationError=0;
    double minimumDeterminant=1.0;
    bool orientationPreserved=true;
};

struct ReuseComparisonExecutionResultV1 final
{
    std::vector<corpus::ObservedPointV1> reference;
    std::vector<corpus::ObservedPointV1> candidateA;
    std::vector<corpus::ObservedPointV1> candidateB;
    std::vector<corpus::ObservedPointV1> candidateC;
    ReuseObservationReportV1 observation;
    std::vector<std::byte> gpuRecords;
    std::uint64_t deviceEpoch=0;
};

struct ReuseCorpusExecutionResultV1 final
{
    std::vector<ReuseComparisonExecutionResultV1> frames;
    base::Digest256 frozenDigestBefore{};
    base::Digest256 frozenDigestAfter{};
};

struct ExecutionErrorV1 final{std::string stage;std::string message;};

[[nodiscard]] base::Result<ReuseObservationReportV1,std::string>
ObserveReuseComparisonV1(
    std::span<const corpus::ObservedPointV1> reference,
    std::span<const corpus::ObservedPointV1> candidateA,
    std::span<const corpus::ObservedPointV1> candidateB,
    std::span<const corpus::ObservedPointV1> candidateC,
    std::uint32_t boneCount,
    std::uint32_t reuseCount);

[[nodiscard]] base::Result<ReuseComparisonExecutionResultV1,ExecutionErrorV1>
ExecuteLoadedReuseComparisonScenarioV1(
    const scenarios::FrozenReuseComparisonScenarioV1& scenario,
    const spiral2::hierarchy::RigidHierarchySemanticV1& hierarchy,
    const corpus::ReuseCaseV1& reuseCase,
    const spiral2::contracts::DynamicLocalMotorPaletteV1& palette,
    composition::canonical::runtime_v1::LoadedStaticComposition& loaded,
    std::uint64_t frameNumber);

[[nodiscard]] base::Result<ReuseComparisonExecutionResultV1,ExecutionErrorV1>
ExecuteFrozenReuseComparisonScenarioV1(
    const scenarios::FrozenReuseComparisonScenarioV1& scenario,
    const spiral2::hierarchy::RigidHierarchySemanticV1& hierarchy,
    const corpus::ReuseCaseV1& reuseCase,
    const spiral2::contracts::DynamicLocalMotorPaletteV1& palette,
    d3d12::D3D12Backend& backend,
    std::uint64_t frameNumber);

[[nodiscard]] base::Result<ReuseCorpusExecutionResultV1,ExecutionErrorV1>
ExecuteFrozenReuseCorpusScenarioV1(
    const scenarios::FrozenReuseComparisonScenarioV1& scenario,
    const spiral2::hierarchy::RigidHierarchySemanticV1& hierarchy,
    const corpus::ReuseCaseV1& reuseCase,
    std::span<const spiral2::corpus::FrameCaseV1> frames,
    d3d12::D3D12Backend& backend,
    std::uint64_t firstFrameNumber=0);

[[nodiscard]] std::vector<std::byte> SerializeReuseObservationReportV1(const ReuseObservationReportV1& report);
[[nodiscard]] std::vector<std::byte> SerializeReuseExecutionEvidenceV1(
    const scenarios::FrozenReuseComparisonScenarioV1& scenario,
    const ReuseComparisonExecutionResultV1& result);
}
