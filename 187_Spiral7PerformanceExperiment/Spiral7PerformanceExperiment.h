#pragma once

#include "../00_Foundation/Result.h"
#include "../00_Foundation/Sha256.h"
#include "../172_SparseTemporalDeltaSemantic/SparseTemporalDeltaSemantic.h"
#include "../178_SparseTemporalDeltaCandidateFamily/DeltaCandidateFamily.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace sge4_5::spiral7::performance
{
inline constexpr std::uint32_t MeasurementEvidenceSchemaVersionV3 = 3;
inline constexpr std::uint32_t MaximumAdaptiveIterationsV2 = 65536;
inline constexpr std::uint32_t CanonicalCandidateCountV1 = 3;
inline constexpr std::uint32_t CanonicalPatternCountV1 = 4;
inline constexpr std::uint32_t CanonicalActiveCountCountV1 = 5;
inline constexpr std::uint32_t CanonicalTransitionCountCountV1 = 8;
inline constexpr std::uint32_t RefinementTransitionCountCountV2 = 5;
inline constexpr std::uint32_t CanonicalOrderCountV1 = 6;
inline constexpr std::uint32_t CanonicalCaseCountV1 =
    CanonicalPatternCountV1 * CanonicalActiveCountCountV1 * CanonicalTransitionCountCountV1;
inline constexpr std::uint32_t RefinementCaseCountV2 =
    CanonicalPatternCountV1 * CanonicalActiveCountCountV1 * RefinementTransitionCountCountV2;

using CandidateKindV1 = family_candidate::DeltaFamilyCandidateKindV1;
using PatternKindV1 = semantic::SparsePatternKindV1;

// The canonical case constructor uses DirtyOnly while T <= A and a disjoint
// ReplaceAndClear transition while T > A. This gives every frozen (A,T) pair
// an exact legal realization without silently clipping it to another identity.
enum class TransitionShapeV1 : std::uint32_t
{
    Hold = 1,
    DirtyOnly = 2,
    ReplaceAndClear = 3
};

enum class PairWinnerV1 : std::uint32_t
{
    Left = 1,
    Right = 2,
    NoiseEquivalent = 3,
    Unresolved = 4
};

enum class MeasurementPassV2 : std::uint32_t
{
    CanonicalSurface = 1,
    HighTransitionRefinement = 2
};

enum class CaseDecisionClassV2 : std::uint32_t
{
    ZeroDispatchEquivalent = 1,
    StableWinner = 2,
    StableEquivalentSet = 3,
    Unresolved = 4
};


struct MeasurementConfigV1 final
{
    MeasurementPassV2 measurementPass = MeasurementPassV2::CanonicalSurface;
    std::uint32_t runCount = 2;
    std::uint32_t measurementCyclesPerOrder = 1;
    std::uint32_t iterationsPerBatch = 128;
    std::uint32_t warmupIterations = 1024;
    std::uint32_t maximumBlockAttempts = 3;
    std::uint32_t adapterIndex = 0;
    bool requestStablePowerStateDiagnostic = false;
    double maximumControlRatio = 1.20;
    double relativeNoiseFloor = 0.02;
    double minimumPairAgreement = 0.70;
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
    bool stablePowerStateApplied = false;
    base::Digest256 fingerprint{};
};

struct CandidateProfileV1 final
{
    CandidateKindV1 kind = CandidateKindV1::FullActiveDenseRecompute;
    family_candidate::DeltaFamilyRepresentationRoleV1 representationRole =
        family_candidate::DeltaFamilyRepresentationRoleV1::DenseActiveMask4096;
    family_candidate::DeltaFamilyDispatchPolicyV1 dispatchPolicy =
        family_candidate::DeltaFamilyDispatchPolicyV1::FixedUniverseGroups;
    std::uint32_t payloadStrideBytes = 0;
    std::uint32_t fixedCapacityBytes = 0;
    base::Digest256 targetProfileIdentity{};
    base::Digest256 observationContractIdentity{};
    base::Digest256 completionContractIdentity{};
    base::Digest256 consumerProgramIdentity{};
    base::Digest256 representationResourceContractIdentity{};
    base::Digest256 validationShaderBytecodeDigest{};
    base::Digest256 timingShaderBytecodeDigest{};
};

struct CandidateCaseAuthorityV1 final
{
    CandidateKindV1 kind = CandidateKindV1::FullActiveDenseRecompute;
    base::Digest256 rawCandidateIdentity{};
    base::Digest256 verifiedPlanIdentity{};
    base::Digest256 verifierCertificateIdentity{};
    base::Digest256 artifactIdentity{};
    base::Digest256 representationResourceIdentity{};
    base::Digest256 outputDigest{};
};

struct MeasurementCaseKeyV1 final
{
    PatternKindV1 pattern = PatternKindV1::PrefixControl;
    std::uint32_t activeCount = 0;
    std::uint32_t transitionCount = 0;
    std::uint32_t updateCount = 0;
    std::uint32_t clearCount = 0;
    std::uint32_t affectedBlockCount = 0;
    TransitionShapeV1 transitionShape = TransitionShapeV1::Hold;
    base::Digest256 previousActiveSetIdentity{};
    base::Digest256 activeSetIdentity{};
    base::Digest256 modifiedSetIdentity{};
    base::Digest256 transitionSetIdentity{};
    base::Digest256 invocationIdentity{};
};

struct TimingSampleV1 final
{
    std::uint32_t run = 0;
    std::uint32_t caseOrder = 0;
    std::uint32_t acceptedAttempt = 0;
    std::uint32_t orderIndex = 0;
    std::uint32_t cycle = 0;
    std::uint32_t orderPosition = 0;
    CandidateKindV1 candidate = CandidateKindV1::FullActiveDenseRecompute;
    std::uint64_t timestampTickCount = 0;
    std::uint32_t effectiveIterations = 0;
    bool timestampResolutionCensored = false;
    double gpuBatchNanoseconds = 0.0;
    double gpuNanosecondsPerIteration = 0.0;
    double blockControlNanosecondsPerIteration = 0.0;
    double controlNormalizedTime = 0.0;
    std::uint32_t expectedDispatchGroupsX = 0;
};

struct BlockAttemptV1 final
{
    std::uint32_t run = 0;
    std::uint32_t caseOrder = 0;
    std::uint32_t attempt = 0;
    MeasurementCaseKeyV1 key{};
    std::array<CandidateCaseAuthorityV1, CanonicalCandidateCountV1> authorities{};
    bool accepted = false;
    std::string rejectionReason;
    double controlBeforeNanosecondsPerIteration = 0.0;
    double controlAfterNanosecondsPerIteration = 0.0;
    double controlReferenceNanosecondsPerIteration = 0.0;
    double controlRatio = 0.0;
    std::vector<TimingSampleV1> samples;
};

struct MeasurementEvidenceV1 final
{
    std::uint32_t schemaVersion = MeasurementEvidenceSchemaVersionV3;
    std::uint64_t captureUnixNanoseconds = 0;
    MeasurementConfigV1 config{};
    AdapterProfileV1 adapter{};
    base::Digest256 cu5ArchitectureEvidenceIdentity{};
    base::Digest256 cu5ControlledRecoveryEvidenceIdentity{};
    base::Digest256 cu5FreshRematerializationEvidenceIdentity{};
    base::Digest256 semanticIdentity{};
    base::Digest256 verificationContextIdentity{};
    base::Digest256 measurementProfileIdentity{};
    base::Digest256 caseScheduleIdentity{};
    base::Digest256 candidateOrderIdentity{};
    std::array<CandidateProfileV1, CanonicalCandidateCountV1> candidates{};
    std::vector<BlockAttemptV1> attempts;
};

struct PairDecisionV1 final
{
    CandidateKindV1 left = CandidateKindV1::FullActiveDenseRecompute;
    CandidateKindV1 right = CandidateKindV1::CompactDeltaIndexHistoryReuse;
    PairWinnerV1 winner = PairWinnerV1::Unresolved;
    double medianLeftOverRight = 1.0;
    double leftWinFraction = 0.0;
    double rightWinFraction = 0.0;
    std::uint32_t pairedObservationCount = 0;
};

struct CaseDecisionV1 final
{
    MeasurementPassV2 sourcePass = MeasurementPassV2::CanonicalSurface;
    MeasurementCaseKeyV1 key{};
    std::array<double, CanonicalCandidateCountV1> medianNanosecondsPerIteration{};
    std::array<double, CanonicalCandidateCountV1> medianControlNormalizedTime{};
    std::array<std::uint32_t, CanonicalCandidateCountV1>
        timestampResolutionCensoredSampleCount{};
    std::vector<CandidateKindV1> descriptiveMedianWinnerSet;
    std::vector<CandidateKindV1> pairedAuthoritySet;
    CaseDecisionClassV2 decisionClass = CaseDecisionClassV2::Unresolved;
    bool eligibleForCrossover = false;
    PairDecisionV1 fullVersusCompact{};
    PairDecisionV1 compactVersusBlock{};
    PairDecisionV1 fullVersusBlock{};
    std::uint32_t pairedObservationCount = 0;
};

struct TransitionSurfaceEventV1 final
{
    PatternKindV1 pattern = PatternKindV1::PrefixControl;
    std::uint32_t activeCount = 0;
    std::uint32_t lowerTransitionCount = 0;
    std::uint32_t upperTransitionCount = 0;
    std::vector<CandidateKindV1> fromWinnerSet;
    std::vector<CandidateKindV1> toWinnerSet;
};

struct ActiveSurfaceEventV1 final
{
    PatternKindV1 pattern = PatternKindV1::PrefixControl;
    std::uint32_t transitionCount = 0;
    std::uint32_t lowerActiveCount = 0;
    std::uint32_t upperActiveCount = 0;
    std::vector<CandidateKindV1> fromWinnerSet;
    std::vector<CandidateKindV1> toWinnerSet;
};

struct DecisionAnalysisV1 final
{
    std::vector<CaseDecisionV1> cases;
    std::vector<TransitionSurfaceEventV1> transitionSurfaceEvents;
    std::vector<ActiveSurfaceEventV1> activeSurfaceEvents;
    std::vector<CandidateKindV1> overallPairedAuthoritySet;
    bool combinedProfile = false;
    bool winnerChangesAcrossTransitionCount = false;
    bool winnerChangesAcrossActiveCount = false;
    bool sameActiveAndTransitionWinnerDependsOnPattern = false;
    std::uint32_t acceptedBlockCount = 0;
    std::uint32_t rejectedBlockCount = 0;
    std::uint32_t timestampResolutionCensoredSampleCount = 0;
    std::uint32_t zeroDispatchEquivalentCaseCount = 0;
    std::uint32_t stableWinnerCaseCount = 0;
    std::uint32_t stableEquivalentCaseCount = 0;
    std::uint32_t unresolvedCaseCount = 0;
};

[[nodiscard]] std::string CandidateNameV1(CandidateKindV1 value);
[[nodiscard]] char CandidateLetterV1(CandidateKindV1 value);
[[nodiscard]] std::string PatternNameV1(PatternKindV1 value);
[[nodiscard]] std::string TransitionShapeNameV1(TransitionShapeV1 value);
[[nodiscard]] std::string MeasurementPassNameV2(MeasurementPassV2 value);
[[nodiscard]] std::string DecisionClassNameV2(CaseDecisionClassV2 value);
[[nodiscard]] std::array<CandidateKindV1, CanonicalCandidateCountV1> CanonicalCandidateKindsV1();
[[nodiscard]] std::array<PatternKindV1, CanonicalPatternCountV1> CanonicalPatternsV1();
[[nodiscard]] const std::array<std::uint32_t, CanonicalActiveCountCountV1>&
CanonicalMeasurementActiveCountsV1() noexcept;
[[nodiscard]] const std::array<std::uint32_t, CanonicalTransitionCountCountV1>&
CanonicalMeasurementTransitionCountsV1() noexcept;
[[nodiscard]] const std::array<std::uint32_t, RefinementTransitionCountCountV2>&
HighTransitionRefinementCountsV2() noexcept;
[[nodiscard]] std::vector<std::uint32_t>
MeasurementTransitionCountsV2(MeasurementPassV2 pass);
[[nodiscard]] std::uint32_t
MeasurementCaseCountPerRunV2(MeasurementPassV2 pass);
[[nodiscard]] std::vector<std::array<std::uint32_t, CanonicalCandidateCountV1>>
CanonicalBalancedOrdersV1();
[[nodiscard]] std::vector<std::array<std::uint32_t, 3>>
MeasurementCaseScheduleV2(MeasurementPassV2 pass, std::uint32_t run);

[[nodiscard]] base::Digest256 BuildMeasurementProfileIdentityV1(const MeasurementConfigV1& config);
[[nodiscard]] base::Digest256 BuildCaseScheduleIdentityV1(const MeasurementConfigV1& config);
[[nodiscard]] base::Digest256 BuildCandidateOrderIdentityV1();
[[nodiscard]] base::Digest256 VerificationContextIdentityV1();

[[nodiscard]] base::Result<MeasurementEvidenceV1, std::string>
RunRealGpuMeasurementV1(const MeasurementConfigV1& config);
[[nodiscard]] std::vector<std::byte>
SerializeMeasurementEvidenceV1(const MeasurementEvidenceV1& evidence);
[[nodiscard]] base::Result<MeasurementEvidenceV1, std::string>
DeserializeMeasurementEvidenceV1(const std::vector<std::byte>& bytes);
[[nodiscard]] base::Result<void, std::string>
ValidateMeasurementEvidenceV1(const MeasurementEvidenceV1& evidence);
[[nodiscard]] DecisionAnalysisV1 AnalyzeMeasurementEvidenceV1(const MeasurementEvidenceV1& evidence);
[[nodiscard]] DecisionAnalysisV1 AnalyzeCombinedMeasurementEvidenceV2(
    const MeasurementEvidenceV1& canonical,
    const MeasurementEvidenceV1& refinement);
[[nodiscard]] std::string BuildDecisionReportV1(
    const MeasurementEvidenceV1& evidence,
    const DecisionAnalysisV1& analysis);
[[nodiscard]] std::string BuildDecisionCsvV1(
    const MeasurementEvidenceV1& evidence,
    const DecisionAnalysisV1& analysis);
[[nodiscard]] std::string BuildCombinedDecisionReportV2(
    const MeasurementEvidenceV1& canonical,
    const MeasurementEvidenceV1& refinement,
    const DecisionAnalysisV1& analysis);
[[nodiscard]] std::string BuildCombinedDecisionCsvV2(
    const MeasurementEvidenceV1& canonical,
    const MeasurementEvidenceV1& refinement,
    const DecisionAnalysisV1& analysis);

[[nodiscard]] std::vector<std::byte> BuildSyntheticSelfTestEvidenceV1();
[[nodiscard]] std::vector<std::byte> BuildSyntheticRefinementSelfTestEvidenceV2();
[[nodiscard]] base::Result<void, std::string> RunMeasurementFormatSelfTestV1();
} // namespace sge4_5::spiral7::performance
