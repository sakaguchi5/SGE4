#pragma once

#include "../00_Foundation/Base.h"
#include "../103_ReuseRepresentationCandidate/ReuseRepresentationCandidate.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace sge4_5::spiral3::performance
{
inline constexpr std::uint32_t MeasurementEvidenceSchemaVersionV1 = 1;
inline constexpr std::uint32_t CanonicalWarmupCyclesPerOrderV1 = 2;
inline constexpr std::uint32_t CanonicalMeasurementCyclesPerOrderV1 = 8;
inline constexpr std::uint32_t CanonicalRunCountV1 = 2;
inline constexpr std::uint32_t CanonicalFrameIndexV1 = 10;
inline constexpr std::uint32_t CanonicalOrderCountV1 = 6;
inline constexpr std::uint32_t CanonicalReuseCaseCountV1 = 6;

struct MeasurementConfigV1 final
{
    std::uint32_t warmupCyclesPerOrder = CanonicalWarmupCyclesPerOrderV1;
    std::uint32_t measurementCyclesPerOrder = CanonicalMeasurementCyclesPerOrderV1;
    std::uint32_t runCount = CanonicalRunCountV1;
    std::uint32_t adapterIndex = 0;
    std::uint32_t frameIndex = CanonicalFrameIndexV1;
    std::string clockPolicy = "uncontrolled";
};

struct AdapterProfileV1 final
{
    std::string description;
    std::uint32_t vendorId = 0;
    std::uint32_t deviceId = 0;
    std::uint32_t subsystemId = 0;
    std::uint32_t revision = 0;
    std::uint32_t luidLow = 0;
    std::int32_t luidHigh = 0;
    std::uint64_t driverVersion = 0;
    std::uint64_t timestampFrequency = 0;
    bool uma = false;
    bool cacheCoherentUma = false;
    base::Digest256 fingerprint{};
};

struct CandidatePreparationV1 final
{
    candidate::CandidateKindV1 kind{};
    std::uint64_t generationNanoseconds = 0;
    std::uint64_t verifierNanoseconds = 0;
    std::uint64_t leafFreezeNanoseconds = 0;
    std::uint64_t compositionFreezeNanoseconds = 0;
    std::uint64_t materializationNanoseconds = 0;
    std::uint64_t candidatePackageBytes = 0;
    std::uint64_t sourcePackageBytes = 0;
    std::uint64_t frozenCompositionBytes = 0;
    std::uint64_t dynamicInvocationBytes = 0;
    std::uint64_t pointSourceBytes = 0;
    std::uint64_t logicalIntermediateBytes = 0;
    std::uint32_t plannedDispatchCount = 0;
    std::uint32_t packageDispatchCount = 0;
    std::uint32_t candidateLeafCount = 0;
    std::uint32_t compositionLeafCount = 0;
    base::Digest256 verifiedCertificate{};
    base::Digest256 verifiedPlanIdentity{};
    base::Digest256 targetProfileIdentity{};
    base::Digest256 candidateCompositionDigest{};
};

struct LeafTimingSampleV1 final
{
    std::string role;
    candidate::CandidateKindV1 kind{};
    double commandRecordingNanoseconds = 0.0;
    double gpuNanoseconds = 0.0;
    std::uint32_t dispatchCount = 0;
    std::uint32_t barrierCount = 0;
};

struct CandidateTimingSampleV1 final
{
    candidate::CandidateKindV1 kind{};
    std::uint32_t orderPosition = 0;
    double endToEndNanoseconds = 0.0;
    double commandRecordingNanoseconds = 0.0;
    double gpuTotalNanoseconds = 0.0;
    double numericMaxAbsoluteError = 0.0;
    double numericMaxRelativeError = 0.0;
    std::uint64_t numericMaxUlpDistance = 0;
    std::uint32_t dispatchCount = 0;
    std::uint32_t barrierCount = 0;
};

struct MeasurementSampleV1 final
{
    std::uint32_t run = 0;
    std::uint32_t cycle = 0;
    std::string candidateOrder;
    std::vector<LeafTimingSampleV1> leaves;
    std::vector<CandidateTimingSampleV1> candidates;
};

struct MeasurementCaseV1 final
{
    std::string id;
    std::uint32_t boneCount = 0;
    std::uint32_t hierarchyDepthCount = 0;
    std::uint32_t reuseCount = 0;
    std::uint32_t pointCount = 0;
    base::Digest256 hierarchyIdentity{};
    base::Digest256 reuseSemanticIdentity{};
    base::Digest256 localPointCorpusIdentity{};
    base::Digest256 invocationIdentity{};
    base::Digest256 scenarioIdentity{};
    std::vector<CandidatePreparationV1> preparation;
    std::vector<MeasurementSampleV1> samples;
};

struct MeasurementEvidenceV1 final
{
    std::uint32_t schemaVersion = MeasurementEvidenceSchemaVersionV1;
    std::uint64_t captureUnixNanoseconds = 0;
    MeasurementConfigV1 config{};
    AdapterProfileV1 adapter{};
    base::Digest256 targetProfileIdentity{};
    base::Digest256 observationContractIdentity{};
    base::Digest256 measurementProfileIdentity{};
    std::vector<MeasurementCaseV1> cases;
};

enum class CrossoverClassificationV1 : std::uint32_t
{
    NoObservedWinnerTransition = 1,
    SingleStableWinnerTransition = 2,
    MultipleWinnerTransitions = 3
};

struct WinnerTransitionV1 final
{
    std::uint32_t lowerReuseCount = 0;
    std::uint32_t upperReuseCount = 0;
    candidate::CandidateKindV1 from{};
    candidate::CandidateKindV1 to{};
};

struct CrossoverAnalysisV1 final
{
    CrossoverClassificationV1 classification = CrossoverClassificationV1::NoObservedWinnerTransition;
    std::vector<candidate::CandidateKindV1> winners;
    std::vector<WinnerTransitionV1> transitions;
};

[[nodiscard]] base::Result<MeasurementEvidenceV1,std::string> RunRealGpuMeasurementV1(
    const MeasurementConfigV1& config);
[[nodiscard]] base::Result<void,std::string> WriteMeasurementArtifactsV1(
    const MeasurementEvidenceV1& evidence,
    const std::filesystem::path& outputDirectory);
[[nodiscard]] base::Result<MeasurementEvidenceV1,std::string> ReadMeasurementEvidenceV1(
    const std::filesystem::path& evidencePath);
[[nodiscard]] base::Result<void,std::string> WriteDecisionEvidenceReportV1(
    const MeasurementEvidenceV1& evidence,
    const std::filesystem::path& outputDirectory);
[[nodiscard]] base::Result<void,std::string> RunFormatSelfTestV1(
    const std::filesystem::path& outputDirectory);
[[nodiscard]] base::Result<CrossoverAnalysisV1,std::string> AnalyzeCrossoverV1(
    const MeasurementEvidenceV1& evidence);
[[nodiscard]] std::vector<std::byte> SerializeMeasurementEvidenceV1(
    const MeasurementEvidenceV1& evidence);
}
