#pragma once

#include "../00_Foundation/Result.h"
#include "../00_Foundation/Sha256.h"
#include "../120_ActiveWorkSemantic/ActiveWorkSemantic.h"
#include "../127_Spiral4CandidateFamilyContracts/CandidateFamilyContracts.h"
#include "../130_ActiveWorkCandidateFamilyVerifier/ActiveWorkCandidateFamilyVerifier.h"
#include "../131_Spiral4CandidateFamilyExecution/CandidateFamilyExecution.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace sge4_5::spiral4::performance
{
inline constexpr std::uint32_t MeasurementEvidenceSchemaVersionV1 = 1;
inline constexpr std::uint32_t CanonicalCandidateCountV1 = 7;
inline constexpr std::uint32_t CanonicalActiveCountCaseCountV1 = 19;
inline constexpr std::uint32_t CanonicalOrderCountV1 = 14;
inline constexpr std::uint32_t CanonicalWarmupCyclesPerOrderV1 = 1;
inline constexpr std::uint32_t CanonicalMeasurementCyclesPerOrderV1 = 2;
inline constexpr std::uint32_t CanonicalRunCountV1 = 2;
inline constexpr double CanonicalAbsoluteNoiseFloorNanosecondsV1 = 100.0;
inline constexpr double CanonicalRelativeNoiseFloorV1 = 0.02;

struct CandidateIdV1 final
{
    family_contract::CandidateFamilyKindV1 kind =
        family_contract::CandidateFamilyKindV1::FixedMaximumGuarded;
    std::uint32_t batchSize = 0;

    bool operator==(const CandidateIdV1&) const = default;
};

struct MeasurementConfigV1 final
{
    std::uint32_t warmupCyclesPerOrder = CanonicalWarmupCyclesPerOrderV1;
    std::uint32_t measurementCyclesPerOrder = CanonicalMeasurementCyclesPerOrderV1;
    std::uint32_t runCount = CanonicalRunCountV1;
    std::uint32_t adapterIndex = 0;
    std::string clockPolicy = "uncontrolled";
    std::string primaryMetric = "GpuCandidatePathNanoseconds";
    double absoluteNoiseFloorNanoseconds =
        CanonicalAbsoluteNoiseFloorNanosecondsV1;
    double relativeNoiseFloor = CanonicalRelativeNoiseFloorV1;
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

struct CandidateProfileV1 final
{
    CandidateIdV1 candidate{};
    std::uint32_t maxBatchSlots = 0;
    std::uint32_t argumentStrideBytes = 0;
    std::uint32_t argumentBufferBytes = 0;
    std::uint32_t maxCommandCount = 0;
    base::Digest256 rawCandidateIdentity{};
    base::Digest256 verifiedPlanIdentity{};
    base::Digest256 verifierCertificateIdentity{};
    base::Digest256 artifactIdentity{};
    base::Digest256 frozenBindingIdentity{};
    base::Digest256 producerProgramTemplateIdentity{};
    base::Digest256 consumerProgramTemplateIdentity{};
    base::Digest256 producerShaderBytecodeDigest{};
    base::Digest256 consumerShaderBytecodeDigest{};
};

struct TimingSampleV1 final
{
    std::uint32_t run = 0;
    std::uint32_t orderIndex = 0;
    std::uint32_t cycle = 0;
    std::uint32_t orderPosition = 0;
    CandidateIdV1 candidate{};
    std::uint32_t activeWorkCount = 0;

    double activeInputPreparationNanoseconds = 0.0;
    double commandRecordingNanoseconds = 0.0;
    double submitAndWaitNanoseconds = 0.0;
    double numericObservationNanoseconds = 0.0;
    double cpuEndToEndNanoseconds = 0.0;

    double gpuArgumentGenerationNanoseconds = 0.0;
    double gpuStateTransitionNanoseconds = 0.0;
    double gpuExecutionNanoseconds = 0.0;
    double gpuOutputSynchronizationNanoseconds = 0.0;
    double gpuObservationCopyNanoseconds = 0.0;
    double gpuCandidatePathNanoseconds = 0.0;
    double gpuTotalNanoseconds = 0.0;

    std::uint32_t issuedThreadGroupCount = 0;
    std::uint32_t activeMismatchCount = 0;
    std::uint32_t nonFiniteCount = 0;
    std::uint32_t homogeneousCoordinateMismatchCount = 0;
    std::uint32_t duplicateWorkCount = 0;
    std::uint32_t missingWorkCount = 0;
    std::uint32_t reorderedWorkCount = 0;
    std::uint32_t outOfRangeWorkCount = 0;
    bool inactiveTailPreserved = false;
    double maxAbsoluteError = 0.0;
    double maxRelativeError = 0.0;
    std::uint32_t maxUlpError = 0;
    base::Digest256 outputDigest{};
};

struct MeasurementCaseV1 final
{
    std::uint32_t activeWorkCount = 0;
    std::vector<TimingSampleV1> samples;
};

struct MeasurementEvidenceV1 final
{
    std::uint32_t schemaVersion = MeasurementEvidenceSchemaVersionV1;
    std::uint64_t captureUnixNanoseconds = 0;
    MeasurementConfigV1 config{};
    AdapterProfileV1 adapter{};

    base::Digest256 architectureEvidenceIdentity{};
    base::Digest256 controlledRecoveryEvidenceIdentity{};
    base::Digest256 freshRematerializationEvidenceIdentity{};
    base::Digest256 semanticIdentity{};
    base::Digest256 verificationContextIdentity{};
    base::Digest256 measurementProfileIdentity{};
    base::Digest256 orderBalanceIdentity{};

    std::vector<CandidateProfileV1> candidates;
    std::vector<MeasurementCaseV1> cases;
};

enum class PairWinnerV1 : std::uint32_t
{
    Left = 1,
    Right = 2,
    NoiseEquivalent = 3
};

enum class TransitionClassificationV1 : std::uint32_t
{
    NoObservedWinnerTransition = 1,
    SingleStableWinnerTransition = 2,
    MultipleWinnerTransitions = 3,
    NoiseEquivalentOrUnresolved = 4
};

struct WinnerTransitionV1 final
{
    std::uint32_t lowerActiveWorkCount = 0;
    std::uint32_t upperActiveWorkCount = 0;
    CandidateIdV1 from{};
    CandidateIdV1 to{};
};

struct PerActiveCountDecisionV1 final
{
    std::uint32_t activeWorkCount = 0;
    std::vector<CandidateIdV1> globalWinnerSet;
    CandidateIdV1 bestObservedBatch{};
    PairWinnerV1 fixedVersusSingle = PairWinnerV1::NoiseEquivalent;
    PairWinnerV1 singleVersusBestBatch = PairWinnerV1::NoiseEquivalent;
    std::array<double, CanonicalCandidateCountV1> medianGpuCandidatePathNanoseconds{};
};

struct DecisionAnalysisV1 final
{
    std::vector<PerActiveCountDecisionV1> cases;
    TransitionClassificationV1 fixedSingleClassification =
        TransitionClassificationV1::NoObservedWinnerTransition;
    TransitionClassificationV1 singleBatchClassification =
        TransitionClassificationV1::NoObservedWinnerTransition;
    std::vector<WinnerTransitionV1> fixedSingleTransitions;
    std::vector<WinnerTransitionV1> singleBatchTransitions;
    std::vector<std::uint32_t> observedBestBatchSizes;
    bool globalWinnerChanges = false;
};

[[nodiscard]] std::string CandidateNameV1(const CandidateIdV1& value);
[[nodiscard]] char CandidateLetterV1(const CandidateIdV1& value);
[[nodiscard]] std::array<CandidateIdV1, CanonicalCandidateCountV1>
CanonicalCandidateIdsV1();
[[nodiscard]] std::vector<
    std::array<std::uint32_t, CanonicalCandidateCountV1>>
CanonicalBalancedOrdersV1();

[[nodiscard]] base::Result<MeasurementEvidenceV1, std::string>
RunRealGpuMeasurementV1(const MeasurementConfigV1& config);

[[nodiscard]] std::vector<std::byte>
SerializeMeasurementEvidenceV1(const MeasurementEvidenceV1& evidence);

[[nodiscard]] base::Result<MeasurementEvidenceV1, std::string>
ReadMeasurementEvidenceV1(const std::filesystem::path& evidencePath);

[[nodiscard]] base::Result<void, std::string>
WriteMeasurementArtifactsV1(
    const MeasurementEvidenceV1& evidence,
    const std::filesystem::path& outputDirectory);

[[nodiscard]] base::Result<DecisionAnalysisV1, std::string>
AnalyzeDecisionEvidenceV1(const MeasurementEvidenceV1& evidence);

[[nodiscard]] base::Result<void, std::string>
WriteDecisionEvidenceReportV1(
    const MeasurementEvidenceV1& evidence,
    const std::filesystem::path& outputDirectory);

[[nodiscard]] base::Result<void, std::string>
RunFormatAndDecisionSelfTestV1(
    const std::filesystem::path& outputDirectory);
}
