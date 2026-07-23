#pragma once

#include "../00_Foundation/Result.h"
#include "../00_Foundation/Sha256.h"
#include "../154_SparseWorkSetSemantic/SparseWorkSetSemantic.h"
#include "../160_SparseCandidateFamily/SparseCandidateFamily.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace sge4_5::spiral6::performance
{
inline constexpr std::uint32_t MeasurementEvidenceSchemaVersionV2 = 2;
inline constexpr std::uint32_t CanonicalCandidateCountV2 = 3;
inline constexpr std::uint32_t CanonicalPatternCountV2 = 4;
inline constexpr std::uint32_t CanonicalMeasurementCardinalityCountV2 = 11;
inline constexpr std::uint32_t CanonicalOrderCountV2 = 6;

using CandidateKindV2 = family_candidate::SparseFamilyCandidateKindV1;
using PatternKindV2 = semantic::SparsePatternKindV1;

// CU6 V2 measures a local relative map. Absolute nanoseconds are descriptive only.
// A/B/C decisions are derived from paired observations captured inside one Pattern/K block.
struct MeasurementConfigV2 final
{
    std::uint32_t runCount = 2;
    std::uint32_t measurementCyclesPerOrder = 2;
    std::uint32_t iterationsPerBatch = 512;
    std::uint32_t warmupIterations = 4096;
    std::uint32_t maximumBlockAttempts = 4;
    std::uint32_t adapterIndex = 0;

    // Optional NVIDIA telemetry is diagnostic only. It never authorizes or rejects a block.
    std::uint32_t nvmlDeviceIndex = 0;
    std::uint32_t nvmlPollingIntervalMilliseconds = 2;
    bool enableNvmlDiagnostic = true;
    bool requestStablePowerStateDiagnostic = false;

    // A block is rejected only when its fixed control proves material within-block drift.
    double maximumControlRatio = 1.20;

    // Paired ratio decision contract.
    double relativeNoiseFloor = 0.02;
    double minimumPairAgreement = 0.70;
};

struct AdapterProfileV2 final
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

struct CandidateProfileV2 final
{
    CandidateKindV2 kind = CandidateKindV2::DenseMembershipMaskPredicate;
    family_candidate::SparseFamilyRepresentationRoleV1 representationRole =
        family_candidate::SparseFamilyRepresentationRoleV1::DenseMembershipMask4096;
    family_candidate::SparseFamilyDispatchPolicyV1 dispatchPolicy =
        family_candidate::SparseFamilyDispatchPolicyV1::FixedUniverseGroups;
    std::uint32_t payloadStrideBytes = 0;
    std::uint32_t fixedCapacityBytes = 0;
    base::Digest256 targetProfileIdentity{};
    base::Digest256 observationContractIdentity{};
    base::Digest256 completionContractIdentity{};
    base::Digest256 consumerProgramIdentity{};
    base::Digest256 representationResourceContractIdentity{};
    base::Digest256 representationShaderBytecodeDigest{};
};

struct CandidateCaseAuthorityV2 final
{
    CandidateKindV2 kind = CandidateKindV2::DenseMembershipMaskPredicate;
    base::Digest256 rawCandidateIdentity{};
    base::Digest256 verifiedPlanIdentity{};
    base::Digest256 verifierCertificateIdentity{};
    base::Digest256 artifactIdentity{};
    base::Digest256 representationResourceIdentity{};
    base::Digest256 frozenBindingIdentity{};
};

struct NvmlSnapshotV2 final
{
    bool available = false;
    std::uint32_t graphicsClockMHz = 0;
    std::uint32_t smClockMHz = 0;
    std::uint32_t memoryClockMHz = 0;
    std::uint32_t videoClockMHz = 0;
    std::uint32_t performanceState = 0;
    std::uint32_t temperatureC = 0;
    std::uint32_t powerUsageMilliwatts = 0;
    std::uint32_t powerLimitMilliwatts = 0;
    std::uint32_t gpuUtilizationPercent = 0;
    std::uint32_t memoryUtilizationPercent = 0;
    std::uint64_t clockEventReasons = 0;
    std::int32_t graphicsClockResult = -10000;
    std::int32_t smClockResult = -10000;
    std::int32_t memoryClockResult = -10000;
    std::int32_t videoClockResult = -10000;
    std::int32_t performanceStateResult = -10000;
    std::int32_t temperatureResult = -10000;
    std::int32_t powerUsageResult = -10000;
    std::int32_t powerLimitResult = -10000;
    std::int32_t utilizationResult = -10000;
    std::int32_t clockEventReasonsResult = -10000;
};

struct BlockTelemetrySummaryV2 final
{
    bool available = false;
    std::uint32_t recordCount = 0;
    std::uint32_t minimumGraphicsClockMHz = 0;
    std::uint32_t maximumGraphicsClockMHz = 0;
    std::uint32_t minimumMemoryClockMHz = 0;
    std::uint32_t maximumMemoryClockMHz = 0;
    std::uint32_t minimumPerformanceState = 0;
    std::uint32_t maximumPerformanceState = 0;
    std::uint32_t maximumTemperatureC = 0;
    std::uint64_t clockEventReasonsOr = 0;
    bool gpuIdleObserved = false;
    bool hardwareThermalObserved = false;
    bool hardwarePowerBrakeObserved = false;
};

struct TimingSampleV2 final
{
    std::uint32_t run = 0;
    std::uint32_t caseOrder = 0;
    std::uint32_t acceptedAttempt = 0;
    std::uint32_t orderIndex = 0;
    std::uint32_t cycle = 0;
    std::uint32_t orderPosition = 0;
    CandidateKindV2 candidate = CandidateKindV2::DenseMembershipMaskPredicate;
    PatternKindV2 pattern = PatternKindV2::PrefixControl;
    std::uint32_t activeCount = 0;
    std::uint32_t activeBlockCount = 0;
    std::uint32_t iterationCount = 0;

    double gpuBatchNanoseconds = 0.0;
    double gpuNanosecondsPerIteration = 0.0;
    double blockControlNanosecondsPerIteration = 0.0;
    double controlNormalizedTime = 0.0;

    std::uint32_t expectedDispatchGroupsX = 0;
    std::uint32_t activeWriteCount = 0;
    std::uint32_t missingWriteCount = 0;
    std::uint32_t unauthorizedWriteCount = 0;
    std::uint32_t activeMismatchCount = 0;
    std::uint32_t nonFiniteCount = 0;
    std::uint32_t homogeneousCoordinateMismatchCount = 0;
    double maxAbsoluteError = 0.0;
    double maxRelativeError = 0.0;
    std::uint32_t maxUlpError = 0;
    base::Digest256 outputDigest{};
};

struct BlockAttemptV2 final
{
    std::uint32_t run = 0;
    std::uint32_t caseOrder = 0;
    std::uint32_t attempt = 0;
    PatternKindV2 pattern = PatternKindV2::PrefixControl;
    std::uint32_t activeCount = 0;
    std::uint32_t activeBlockCount = 0;
    base::Digest256 sparseSetIdentity{};
    std::array<CandidateCaseAuthorityV2, CanonicalCandidateCountV2> authorities{};
    bool accepted = false;
    std::string rejectionReason;
    double controlBeforeNanosecondsPerIteration = 0.0;
    double controlAfterNanosecondsPerIteration = 0.0;
    double controlReferenceNanosecondsPerIteration = 0.0;
    double controlRatio = 0.0;
    NvmlSnapshotV2 preBlockTelemetry{};
    NvmlSnapshotV2 postBlockTelemetry{};
    BlockTelemetrySummaryV2 telemetrySummary{};
    std::vector<TimingSampleV2> samples;
};

struct MeasurementEvidenceV2 final
{
    std::uint32_t schemaVersion = MeasurementEvidenceSchemaVersionV2;
    std::uint64_t captureUnixNanoseconds = 0;
    MeasurementConfigV2 config{};
    AdapterProfileV2 adapter{};
    std::string nvmlDeviceName;
    std::string nvmlDeviceUuid;
    bool nvmlAvailable = false;
    bool stablePowerStateApplied = false;
    base::Digest256 architectureEvidenceIdentity{};
    base::Digest256 controlledRecoveryEvidenceIdentity{};
    base::Digest256 freshRematerializationEvidenceIdentity{};
    base::Digest256 semanticIdentity{};
    base::Digest256 verificationContextIdentity{};
    base::Digest256 measurementProfileIdentity{};
    base::Digest256 caseScheduleIdentity{};
    base::Digest256 candidateOrderIdentity{};
    std::array<CandidateProfileV2, CanonicalCandidateCountV2> candidates{};
    std::vector<BlockAttemptV2> attempts;
};

enum class PairWinnerV2 : std::uint32_t
{
    Left = 1,
    Right = 2,
    NoiseEquivalent = 3
};

enum class TransitionClassificationV2 : std::uint32_t
{
    NoObservedWinnerTransition = 1,
    SingleStableWinnerTransition = 2,
    MultipleWinnerTransitions = 3,
    NoiseEquivalentOrUnresolved = 4
};

struct WinnerTransitionV2 final
{
    PatternKindV2 pattern = PatternKindV2::PrefixControl;
    std::uint32_t lowerActiveCount = 0;
    std::uint32_t upperActiveCount = 0;
    CandidateKindV2 from = CandidateKindV2::DenseMembershipMaskPredicate;
    CandidateKindV2 to = CandidateKindV2::DenseMembershipMaskPredicate;
};

struct PairDecisionV2 final
{
    CandidateKindV2 left = CandidateKindV2::DenseMembershipMaskPredicate;
    CandidateKindV2 right = CandidateKindV2::CompactIndexListDispatch;
    PairWinnerV2 winner = PairWinnerV2::NoiseEquivalent;
    double medianLeftOverRight = 1.0;
    double leftWinFraction = 0.0;
    double rightWinFraction = 0.0;
    std::uint32_t pairedObservationCount = 0;
};

struct PerPatternCardinalityDecisionV2 final
{
    PatternKindV2 pattern = PatternKindV2::PrefixControl;
    std::uint32_t activeCount = 0;
    std::uint32_t activeBlockCount = 0;
    std::vector<CandidateKindV2> globalWinnerSet;
    PairDecisionV2 denseVersusCompact{};
    PairDecisionV2 compactVersusActiveBlock{};
    PairDecisionV2 denseVersusActiveBlock{};
    std::array<double, CanonicalCandidateCountV2> medianAbsoluteNanosecondsPerIteration{};
    std::array<double, CanonicalCandidateCountV2> medianControlNormalizedTime{};
    std::uint32_t pairedObservationCount = 0;
};

struct PatternTransitionAnalysisV2 final
{
    PatternKindV2 pattern = PatternKindV2::PrefixControl;
    TransitionClassificationV2 denseCompactClassification =
        TransitionClassificationV2::NoObservedWinnerTransition;
    TransitionClassificationV2 compactBlockClassification =
        TransitionClassificationV2::NoObservedWinnerTransition;
    std::vector<WinnerTransitionV2> denseCompactTransitions;
    std::vector<WinnerTransitionV2> compactBlockTransitions;
};

struct DecisionAnalysisV2 final
{
    std::vector<PerPatternCardinalityDecisionV2> cases;
    std::array<PatternTransitionAnalysisV2, CanonicalPatternCountV2> patternTransitions{};
    std::vector<CandidateKindV2> overallObservedWinnerSet;
    bool winnerChangesAcrossCardinality = false;
    bool sameCardinalityWinnerDependsOnPattern = false;
    std::uint32_t acceptedBlockCount = 0;
    std::uint32_t rejectedBlockCount = 0;
};

[[nodiscard]] std::string CandidateNameV2(CandidateKindV2 value);
[[nodiscard]] char CandidateLetterV2(CandidateKindV2 value);
[[nodiscard]] std::string PatternNameV2(PatternKindV2 value);
[[nodiscard]] std::array<CandidateKindV2, CanonicalCandidateCountV2> CanonicalCandidateKindsV2();
[[nodiscard]] std::array<PatternKindV2, CanonicalPatternCountV2> CanonicalPatternsV2();
[[nodiscard]] const std::array<std::uint32_t, CanonicalMeasurementCardinalityCountV2>&
CanonicalMeasurementCardinalitiesV2() noexcept;
[[nodiscard]] std::vector<std::array<std::uint32_t, CanonicalCandidateCountV2>>
CanonicalBalancedOrdersV2();
[[nodiscard]] std::vector<std::pair<PatternKindV2, std::uint32_t>>
CanonicalBalancedCaseScheduleV2(std::uint32_t run);

[[nodiscard]] base::Digest256 BuildMeasurementProfileIdentityV2(const MeasurementConfigV2& config);
[[nodiscard]] base::Digest256 BuildCaseScheduleIdentityV2(std::uint32_t runCount);
[[nodiscard]] base::Digest256 BuildCandidateOrderIdentityV2();

[[nodiscard]] base::Result<MeasurementEvidenceV2, std::string>
RunRealGpuMeasurementV2(const MeasurementConfigV2& config);

[[nodiscard]] std::vector<std::byte>
SerializeMeasurementEvidenceV2(const MeasurementEvidenceV2& evidence);

[[nodiscard]] base::Result<MeasurementEvidenceV2, std::string>
ReadMeasurementEvidenceV2(const std::filesystem::path& evidencePath);

[[nodiscard]] base::Result<void, std::string>
WriteMeasurementArtifactsV2(
    const MeasurementEvidenceV2& evidence,
    const std::filesystem::path& outputDirectory);

[[nodiscard]] base::Result<DecisionAnalysisV2, std::string>
AnalyzeDecisionEvidenceV2(const MeasurementEvidenceV2& evidence);

[[nodiscard]] base::Result<void, std::string>
WriteDecisionEvidenceReportV2(
    const MeasurementEvidenceV2& evidence,
    const std::filesystem::path& outputDirectory);

[[nodiscard]] base::Result<void, std::string>
RunFormatScheduleAndStabilitySelfTestV2(
    const std::filesystem::path& outputDirectory);
}
