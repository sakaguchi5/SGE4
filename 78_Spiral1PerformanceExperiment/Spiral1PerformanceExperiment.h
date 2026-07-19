#pragma once

#include "../00_Foundation/Base.h"
#include "../61_Spiral1Contracts/Spiral1Contracts.h"
#include "../63_D3D12RepresentationCandidate/D3D12RepresentationCandidate.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace sge4_5::spiral1::performance
{
inline constexpr std::uint32_t BenchmarkEvidenceSchemaVersionV1 = 1;
inline constexpr std::uint32_t CanonicalWarmupSamplesPerAlgorithmV1 = 200;
inline constexpr std::uint32_t CanonicalMeasurementSamplesPerAlgorithmV1 = 2000;
inline constexpr std::uint32_t CanonicalRunCountV1 = 5;

struct BenchmarkConfigV1 final
{
    std::uint32_t warmupSamplesPerAlgorithm = CanonicalWarmupSamplesPerAlgorithmV1;
    std::uint32_t measurementSamplesPerAlgorithm = CanonicalMeasurementSamplesPerAlgorithmV1;
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
    std::uint64_t dedicatedVideoMemory = 0;
    std::uint64_t dedicatedSystemMemory = 0;
    std::uint64_t sharedSystemMemory = 0;
    std::uint64_t driverVersion = 0;
    std::uint64_t timestampFrequency = 0;
    bool uma = false;
    bool cacheCoherentUma = false;
    base::Digest256 fingerprint{};
};

struct TimingSampleV1 final
{
    std::uint32_t run = 0;
    std::uint32_t block = 0;
    std::uint32_t orderPosition = 0;
    representation::LoweringKindV1 loweringKind = representation::LoweringKindV1::MatrixExpandedCompute;
    double commandRecordingNs = 0.0;
    double gpuDispatchNs = 0.0;
    double endToEndNs = 0.0;
};

struct MetricSummaryV1 final
{
    double median = 0.0;
    double p95 = 0.0;
    double minimum = 0.0;
    double maximum = 0.0;
};

struct AlgorithmMeasurementV1 final
{
    representation::LoweringKindV1 loweringKind = representation::LoweringKindV1::MatrixExpandedCompute;
    base::Digest256 candidateIdentity{};
    base::Digest256 representationSeal{};
    base::Digest256 packageExecutionDigest{};
    base::Digest256 packageFileDigest{};
    std::uint64_t candidateGenerationNs = 0;
    std::uint64_t verifierNs = 0;
    std::uint64_t freezeNs = 0;
    std::uint64_t materializationNs = 0;
    std::uint64_t packageBytes = 0;
    std::uint64_t constantBytes = 0;
    std::uint64_t cpuToGpuUploadBytes = 0;
    std::uint64_t allocatedBytes = 0;
    std::uint64_t readbackBytes = 0;
    MetricSummaryV1 commandRecording{};
    MetricSummaryV1 gpuDispatch{};
    MetricSummaryV1 endToEnd{};
    std::vector<TimingSampleV1> samples;
};

struct CaseMeasurementV1 final
{
    std::string scenarioId;
    std::uint32_t subscenarioIndex = 0;
    std::uint32_t pointCount = 0;
    base::Digest256 caseIdentity{};
    std::uint64_t sharedInputAllocatedBytes = 0;
    std::uint64_t sharedInputUploadBytes = 0;
    AlgorithmMeasurementV1 matrix{};
    AlgorithmMeasurementV1 directPga{};
    contracts::ScenarioReportV1 observation{};
};

struct BenchmarkEvidenceV1 final
{
    std::uint32_t schemaVersion = BenchmarkEvidenceSchemaVersionV1;
    std::uint64_t captureUnixNanoseconds = 0;
    BenchmarkConfigV1 config{};
    AdapterProfileV1 adapter{};
    base::Digest256 corpusIdentity{};
    base::Digest256 observationContractIdentity{};
    base::Digest256 representationTargetProfileIdentity{};
    base::Digest256 benchmarkProfileIdentity{};
    std::uint64_t nativeProgramBuildNs = 0;
    std::vector<CaseMeasurementV1> cases;
};

[[nodiscard]] base::Result<BenchmarkEvidenceV1, std::string> RunRealGpuMeasurementV1(
    const BenchmarkConfigV1& config);
[[nodiscard]] base::Result<void, std::string> WriteBenchmarkArtifactsV1(
    const BenchmarkEvidenceV1& evidence,
    const std::filesystem::path& outputDirectory);
[[nodiscard]] base::Result<BenchmarkEvidenceV1, std::string> ReadBenchmarkEvidenceV1(
    const std::filesystem::path& evidencePath);
[[nodiscard]] base::Result<void, std::string> WriteDecisionEvidenceReportV1(
    const BenchmarkEvidenceV1& evidence,
    const std::filesystem::path& outputDirectory);
[[nodiscard]] base::Result<void, std::string> RunFormatSelfTestV1(
    const std::filesystem::path& outputDirectory);
}
