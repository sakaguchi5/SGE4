#pragma once

#include "../00_Foundation/Result.h"
#include "../00_Foundation/Sha256.h"
#include "../134_TemporalStateSemantic/TemporalStateSemantic.h"
#include "../147_TemporalCandidateFamily/TemporalCandidateFamily.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace sge4_5::spiral5::performance
{
inline constexpr std::uint32_t MeasurementEvidenceSchemaVersionV1 = 1;
inline constexpr std::uint32_t CanonicalCandidateCountV1 = 3;
inline constexpr std::uint32_t CanonicalUpdateIntervalCountV1 = 7;
inline constexpr std::uint32_t CanonicalOrderCountV1 = 6;
inline constexpr std::uint32_t CanonicalWarmupTimelinesPerOrderV1 = 1;
inline constexpr std::uint32_t CanonicalMeasurementTimelinesPerOrderV1 = 2;
inline constexpr std::uint32_t CanonicalRunCountV1 = 2;
inline constexpr double CanonicalAbsoluteNoiseFloorNanosecondsV1 = 100.0;
inline constexpr double CanonicalRelativeNoiseFloorV1 = 0.02;

using CandidateKindV1 = family_candidate::TemporalCandidateKindV1;

struct MeasurementConfigV1 final
{
    std::uint32_t warmupTimelinesPerOrder = CanonicalWarmupTimelinesPerOrderV1;
    std::uint32_t measurementTimelinesPerOrder = CanonicalMeasurementTimelinesPerOrderV1;
    std::uint32_t runCount = CanonicalRunCountV1;
    std::uint32_t adapterIndex = 0;
    std::string clockPolicy = "uncontrolled";
    std::string primaryMetric = "GpuCandidateTimelineNanoseconds";
    double absoluteNoiseFloorNanoseconds = CanonicalAbsoluteNoiseFloorNanosecondsV1;
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
    CandidateKindV1 kind = CandidateKindV1::EveryInvocationRecompute;
    family_candidate::TemporalFamilyHistoryRoleV1 historyRole =
        family_candidate::TemporalFamilyHistoryRoleV1::None;
    std::uint32_t historyDepth = 0;
    std::uint32_t historyBytes = 0;
    base::Digest256 targetProfileIdentity{};
    base::Digest256 observationContractIdentity{};
    base::Digest256 argumentProducerProgramIdentity{};
    base::Digest256 hierarchyProgramIdentity{};
    base::Digest256 consumerProgramIdentity{};
    base::Digest256 argumentProducerShaderBytecodeDigest{};
    base::Digest256 hierarchyShaderBytecodeDigest{};
    base::Digest256 consumerShaderBytecodeDigest{};
};

struct CandidateCaseAuthorityV1 final
{
    CandidateKindV1 kind = CandidateKindV1::EveryInvocationRecompute;
    base::Digest256 rawCandidateIdentity{};
    base::Digest256 verifiedPlanIdentity{};
    base::Digest256 verifierCertificateIdentity{};
    base::Digest256 artifactIdentity{};
    base::Digest256 historyResourceIdentity{};
    base::Digest256 invocationAuthorityIdentity{};
};

struct TimingSampleV1 final
{
    std::uint32_t run = 0;
    std::uint32_t orderIndex = 0;
    std::uint32_t cycle = 0;
    std::uint32_t orderPosition = 0;
    CandidateKindV1 candidate = CandidateKindV1::EveryInvocationRecompute;
    std::uint32_t updateInterval = 1;

    double temporalInputPreparationNanoseconds = 0.0;
    double commandRecordingNanoseconds = 0.0;
    double submitAndWaitNanoseconds = 0.0;
    double numericObservationNanoseconds = 0.0;
    double cpuEndToEndNanoseconds = 0.0;

    double gpuArgumentGenerationNanoseconds = 0.0;
    double gpuHierarchyExecutionNanoseconds = 0.0;
    double gpuConsumerExecutionNanoseconds = 0.0;
    double gpuRetainedCompletionNanoseconds = 0.0;
    double gpuObservationCopyNanoseconds = 0.0;
    double gpuCandidateTimelineNanoseconds = 0.0;
    double gpuTotalNanoseconds = 0.0;
    double gpuUpdatePathNanoseconds = 0.0;
    double gpuHoldPathNanoseconds = 0.0;

    std::uint32_t updateInvocationCount = 0;
    std::uint32_t holdInvocationCount = 0;
    std::uint32_t mismatchCount = 0;
    std::uint32_t nonFiniteCount = 0;
    std::uint32_t homogeneousCoordinateMismatchCount = 0;
    double maxAbsoluteError = 0.0;
    double maxRelativeError = 0.0;
    std::uint32_t maxUlpError = 0;
    base::Digest256 outputDigest{};
};

struct MeasurementCaseV1 final
{
    std::uint32_t updateInterval = 1;
    base::Digest256 scheduleIdentity{};
    std::array<CandidateCaseAuthorityV1, CanonicalCandidateCountV1> authorities{};
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
    base::Digest256 measurementProfileIdentity{};
    base::Digest256 orderBalanceIdentity{};

    std::array<CandidateProfileV1, CanonicalCandidateCountV1> candidates{};
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
    std::uint32_t lowerUpdateInterval = 0;
    std::uint32_t upperUpdateInterval = 0;
    CandidateKindV1 from = CandidateKindV1::EveryInvocationRecompute;
    CandidateKindV1 to = CandidateKindV1::EveryInvocationRecompute;
};

struct PerUpdateIntervalDecisionV1 final
{
    std::uint32_t updateInterval = 1;
    std::vector<CandidateKindV1> globalWinnerSet;
    PairWinnerV1 recomputeVersusGlobalMotor = PairWinnerV1::NoiseEquivalent;
    PairWinnerV1 globalMotorVersusFinalOutput = PairWinnerV1::NoiseEquivalent;
    std::array<double, CanonicalCandidateCountV1> medianGpuCandidateTimelineNanoseconds{};
    std::array<double, CanonicalCandidateCountV1> medianGpuUpdateInvocationNanoseconds{};
    std::array<double, CanonicalCandidateCountV1> medianGpuHoldInvocationNanoseconds{};
};

struct DecisionAnalysisV1 final
{
    std::vector<PerUpdateIntervalDecisionV1> cases;
    TransitionClassificationV1 recomputeGlobalClassification =
        TransitionClassificationV1::NoObservedWinnerTransition;
    TransitionClassificationV1 globalFinalClassification =
        TransitionClassificationV1::NoObservedWinnerTransition;
    std::vector<WinnerTransitionV1> recomputeGlobalTransitions;
    std::vector<WinnerTransitionV1> globalFinalTransitions;
    std::vector<CandidateKindV1> overallObservedWinnerSet;
    bool globalWinnerSetChanges = false;
};

[[nodiscard]] std::string CandidateNameV1(CandidateKindV1 value);
[[nodiscard]] char CandidateLetterV1(CandidateKindV1 value);
[[nodiscard]] std::array<CandidateKindV1, CanonicalCandidateCountV1>
CanonicalCandidateKindsV1();
[[nodiscard]] const std::array<std::uint32_t, CanonicalUpdateIntervalCountV1>&
CanonicalUpdateIntervalsV1() noexcept;
[[nodiscard]] std::vector<std::array<std::uint32_t, CanonicalCandidateCountV1>>
CanonicalBalancedOrdersV1();

[[nodiscard]] base::Digest256 BuildMeasurementProfileIdentityV1(const MeasurementConfigV1& config);
[[nodiscard]] base::Digest256 BuildOrderBalanceIdentityV1();

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
