#pragma once

#include "../00_Foundation/Base.h"
#include "../83_HierarchyRepresentationCandidate/HierarchyRepresentationCandidate.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace sge4_5::spiral2::performance
{
inline constexpr std::uint32_t MeasurementEvidenceSchemaVersionV1 = 2;
inline constexpr std::uint32_t CanonicalWarmupCyclesPerOrderV1 = 2;
inline constexpr std::uint32_t CanonicalMeasurementCyclesPerOrderV1 = 8;
inline constexpr std::uint32_t CanonicalRunCountV1 = 2;

struct MeasurementConfigV1 final
{
    std::uint32_t warmupCyclesPerOrder = CanonicalWarmupCyclesPerOrderV1;
    std::uint32_t measurementCyclesPerOrder = CanonicalMeasurementCyclesPerOrderV1;
    std::uint32_t runCount = CanonicalRunCountV1;
    std::uint32_t adapterIndex = 0;
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

// One-time, candidate-specific costs and exact frozen resource facts.
struct CandidatePreparationV1 final
{
    candidate::CandidateKindV1 kind{};
    std::uint64_t generationNanoseconds = 0;
    std::uint64_t verifierNanoseconds = 0;
    std::uint64_t freezeNanoseconds = 0;
    std::uint64_t materializationNanoseconds = 0;
    std::uint64_t packageBytes = 0;
    std::uint64_t dynamicInvocationBytes = 0;
    std::uint64_t logicalIntermediateBytes = 0;
    std::uint32_t plannedDispatchCount = 0;
    std::uint32_t packageDispatchCount = 0;
    base::Digest256 verifiedCertificate{};
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

// Per-invocation values are candidate-specific. No composition-wide A+B+C value is stored.
struct CandidateTimingSampleV1 final
{
    candidate::CandidateKindV1 kind{};
    std::uint32_t orderPosition = 0;
    double endToEndNanoseconds = 0.0;
    double commandRecordingNanoseconds = 0.0;
    double gpuTotalNanoseconds = 0.0;
    double numericMaxAbsoluteError = 0.0;
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
    std::string topology;
    std::uint32_t boneCount = 0;
    std::uint32_t hierarchyDepthCount = 0;
    base::Digest256 hierarchyIdentity{};
    base::Digest256 invocationIdentity{};
    std::vector<CandidatePreparationV1> preparation;
    std::vector<MeasurementSampleV1> samples;
};

struct MeasurementEvidenceV1 final
{
    std::uint32_t schemaVersion = MeasurementEvidenceSchemaVersionV1;
    std::uint64_t captureUnixNanoseconds = 0;
    MeasurementConfigV1 config{};
    AdapterProfileV1 adapter{};
    base::Digest256 observationContractIdentity{};
    base::Digest256 measurementProfileIdentity{};
    std::vector<MeasurementCaseV1> cases;
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
}
