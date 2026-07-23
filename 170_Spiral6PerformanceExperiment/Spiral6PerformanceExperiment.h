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
#include <vector>

namespace sge4_5::spiral6::performance
{
inline constexpr std::uint32_t MeasurementEvidenceSchemaVersionV1 = 1;
inline constexpr std::uint32_t CanonicalCandidateCountV1 = 3;
inline constexpr std::uint32_t CanonicalPatternCountV1 = 4;
inline constexpr std::uint32_t CanonicalMeasurementCardinalityCountV1 = 11;
inline constexpr std::uint32_t CanonicalOrderCountV1 = 6;
inline constexpr std::uint32_t CanonicalWarmupCyclesPerOrderV1 = 1;
inline constexpr std::uint32_t CanonicalMeasurementCyclesPerOrderV1 = 2;
inline constexpr std::uint32_t CanonicalRunCountV1 = 2;
inline constexpr double CanonicalAbsoluteNoiseFloorNanosecondsV1 = 100.0;
inline constexpr double CanonicalRelativeNoiseFloorV1 = 0.02;

using CandidateKindV1 = family_candidate::SparseFamilyCandidateKindV1;
using PatternKindV1 = semantic::SparsePatternKindV1;

struct MeasurementConfigV1 final
{
    std::uint32_t warmupCyclesPerOrder = CanonicalWarmupCyclesPerOrderV1;
    std::uint32_t measurementCyclesPerOrder = CanonicalMeasurementCyclesPerOrderV1;
    std::uint32_t runCount = CanonicalRunCountV1;
    std::uint32_t adapterIndex = 0;
    std::string clockPolicy = "uncontrolled";
    std::string primaryMetric = "GpuCandidatePathNanoseconds";
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
    CandidateKindV1 kind = CandidateKindV1::DenseMembershipMaskPredicate;
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
    base::Digest256 argumentProducerShaderBytecodeDigest{};
};

struct CandidateCaseAuthorityV1 final
{
    CandidateKindV1 kind = CandidateKindV1::DenseMembershipMaskPredicate;
    base::Digest256 rawCandidateIdentity{};
    base::Digest256 verifiedPlanIdentity{};
    base::Digest256 verifierCertificateIdentity{};
    base::Digest256 artifactIdentity{};
    base::Digest256 representationResourceIdentity{};
    base::Digest256 frozenBindingIdentity{};
};

struct TimingSampleV1 final
{
    std::uint32_t run = 0;
    std::uint32_t orderIndex = 0;
    std::uint32_t cycle = 0;
    std::uint32_t orderPosition = 0;
    CandidateKindV1 candidate = CandidateKindV1::DenseMembershipMaskPredicate;
    PatternKindV1 pattern = PatternKindV1::PrefixControl;
    std::uint32_t activeCount = 0;
    std::uint32_t activeBlockCount = 0;

    double sparseInputValidationNanoseconds = 0.0;
    double derivedRepresentationConstructionUploadNanoseconds = 0.0;
    double commandRecordingNanoseconds = 0.0;
    double submitAndWaitNanoseconds = 0.0;
    double numericObservationNanoseconds = 0.0;
    double cpuEndToEndNanoseconds = 0.0;

    double gpuSentinelInitializationNanoseconds = 0.0;
    double gpuIndirectArgumentGenerationNanoseconds = 0.0;
    double gpuSparseConsumerExecutionNanoseconds = 0.0;
    double gpuCompletionNanoseconds = 0.0;
    double gpuObservationCopyNanoseconds = 0.0;
    double gpuCandidatePathNanoseconds = 0.0;
    double gpuTotalNanoseconds = 0.0;

    std::uint32_t expectedDispatchGroupsX = 0;
    std::uint32_t observedDispatchGroupsX = 0;
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

struct MeasurementCaseV1 final
{
    PatternKindV1 pattern = PatternKindV1::PrefixControl;
    std::uint32_t activeCount = 0;
    std::uint32_t activeBlockCount = 0;
    base::Digest256 sparseSetIdentity{};
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
    base::Digest256 verificationContextIdentity{};
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
    PatternKindV1 pattern = PatternKindV1::PrefixControl;
    std::uint32_t lowerActiveCount = 0;
    std::uint32_t upperActiveCount = 0;
    CandidateKindV1 from = CandidateKindV1::DenseMembershipMaskPredicate;
    CandidateKindV1 to = CandidateKindV1::DenseMembershipMaskPredicate;
};

struct PerPatternCardinalityDecisionV1 final
{
    PatternKindV1 pattern = PatternKindV1::PrefixControl;
    std::uint32_t activeCount = 0;
    std::uint32_t activeBlockCount = 0;
    std::vector<CandidateKindV1> globalWinnerSet;
    PairWinnerV1 denseVersusCompact = PairWinnerV1::NoiseEquivalent;
    PairWinnerV1 compactVersusActiveBlock = PairWinnerV1::NoiseEquivalent;
    std::array<double, CanonicalCandidateCountV1> medianGpuCandidatePathNanoseconds{};
    std::array<double, CanonicalCandidateCountV1> medianGpuRepresentationConstructionUploadNanoseconds{};
};

struct PatternTransitionAnalysisV1 final
{
    PatternKindV1 pattern = PatternKindV1::PrefixControl;
    TransitionClassificationV1 denseCompactClassification =
        TransitionClassificationV1::NoObservedWinnerTransition;
    TransitionClassificationV1 compactBlockClassification =
        TransitionClassificationV1::NoObservedWinnerTransition;
    std::vector<WinnerTransitionV1> denseCompactTransitions;
    std::vector<WinnerTransitionV1> compactBlockTransitions;
};

struct DecisionAnalysisV1 final
{
    std::vector<PerPatternCardinalityDecisionV1> cases;
    std::array<PatternTransitionAnalysisV1, CanonicalPatternCountV1> patternTransitions{};
    std::vector<CandidateKindV1> overallObservedWinnerSet;
    bool winnerChangesAcrossCardinality = false;
    bool sameCardinalityWinnerDependsOnPattern = false;
};

[[nodiscard]] std::string CandidateNameV1(CandidateKindV1 value);
[[nodiscard]] char CandidateLetterV1(CandidateKindV1 value);
[[nodiscard]] std::string PatternNameV1(PatternKindV1 value);
[[nodiscard]] std::array<CandidateKindV1, CanonicalCandidateCountV1> CanonicalCandidateKindsV1();
[[nodiscard]] std::array<PatternKindV1, CanonicalPatternCountV1> CanonicalPatternsV1();
[[nodiscard]] const std::array<std::uint32_t, CanonicalMeasurementCardinalityCountV1>&
CanonicalMeasurementCardinalitiesV1() noexcept;
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
