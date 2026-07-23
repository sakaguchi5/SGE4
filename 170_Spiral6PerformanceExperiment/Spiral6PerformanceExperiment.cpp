#include "Spiral6PerformanceExperiment.h"

#include "../00_Foundation/BinaryIO.h"
#include "../00_Foundation/FileIO.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <tuple>

namespace sge4_5::spiral6::performance
{
namespace
{
constexpr std::string_view EvidenceMagic = "SGE4-5.Spiral6.RelativePerformanceEvidence.V2";
constexpr std::string_view MeasurementProfileDomain = "SGE4-5.Spiral6.RelativePerformanceProfile.V2";
constexpr std::string_view CaseScheduleDomain = "SGE4-5.Spiral6.RelativeBalancedCaseSchedule.V2";
constexpr std::string_view CandidateOrderDomain = "SGE4-5.Spiral6.RelativeBalancedCandidateOrders.V2";

void Require(bool condition, std::string_view message)
{
    if (!condition) throw std::runtime_error(std::string(message));
}

void WriteString(base::BinaryWriter& writer, std::string_view value)
{
    Require(value.size() <= std::numeric_limits<std::uint32_t>::max(), "String too large.");
    writer.WriteU32(static_cast<std::uint32_t>(value.size()));
    writer.WriteBytes(std::as_bytes(std::span(value.data(), value.size())));
}

std::string ReadString(base::BinaryReader& reader, std::uint32_t maximum = 16u * 1024u * 1024u)
{
    auto size = reader.ReadU32();
    if (!size) throw std::runtime_error(size.Error());
    Require(size.Value() <= maximum, "Evidence string exceeds limit.");
    auto bytes = reader.ReadBytes(size.Value());
    if (!bytes) throw std::runtime_error(bytes.Error());
    return std::string(reinterpret_cast<const char*>(bytes.Value().data()), bytes.Value().size());
}

void WriteBool(base::BinaryWriter& writer, bool value) { writer.WriteU8(value ? 1u : 0u); }

bool ReadBool(base::BinaryReader& reader)
{
    auto value = reader.ReadU8();
    if (!value) throw std::runtime_error(value.Error());
    Require(value.Value() <= 1u, "Evidence boolean is not canonical.");
    return value.Value() != 0;
}

void WriteDouble(base::BinaryWriter& writer, double value)
{
    writer.WriteU64(std::bit_cast<std::uint64_t>(value));
}

double ReadDouble(base::BinaryReader& reader)
{
    auto value = reader.ReadU64();
    if (!value) throw std::runtime_error(value.Error());
    return std::bit_cast<double>(value.Value());
}

void WriteDigest(base::BinaryWriter& writer, const base::Digest256& value) { writer.WriteBytes(value); }

base::Digest256 ReadDigest(base::BinaryReader& reader)
{
    auto bytes = reader.ReadBytes(32);
    if (!bytes) throw std::runtime_error(bytes.Error());
    base::Digest256 value{};
    std::memcpy(value.data(), bytes.Value().data(), value.size());
    return value;
}

std::uint32_t ReadU32(base::BinaryReader& reader)
{
    auto value = reader.ReadU32();
    if (!value) throw std::runtime_error(value.Error());
    return value.Value();
}

std::int32_t ReadI32(base::BinaryReader& reader)
{
    auto value = reader.ReadI32();
    if (!value) throw std::runtime_error(value.Error());
    return value.Value();
}

std::uint64_t ReadU64(base::BinaryReader& reader)
{
    auto value = reader.ReadU64();
    if (!value) throw std::runtime_error(value.Error());
    return value.Value();
}

CandidateKindV2 CandidateFromU32(std::uint32_t value)
{
    switch (value)
    {
    case 1: return CandidateKindV2::DenseMembershipMaskPredicate;
    case 2: return CandidateKindV2::CompactIndexListDispatch;
    case 3: return CandidateKindV2::ActiveBlockListLocalMask;
    default: throw std::runtime_error("Unknown Candidate kind in evidence.");
    }
}

PatternKindV2 PatternFromU32(std::uint32_t value)
{
    switch (value)
    {
    case 1: return PatternKindV2::PrefixControl;
    case 2: return PatternKindV2::UniformStride;
    case 3: return PatternKindV2::BlockClusteredPermutation;
    case 4: return PatternKindV2::HashScatterPermutation;
    default: throw std::runtime_error("Unknown Pattern kind in evidence.");
    }
}

family_candidate::SparseFamilyRepresentationRoleV1 RoleFromU32(std::uint32_t value)
{
    switch (value)
    {
    case 1: return family_candidate::SparseFamilyRepresentationRoleV1::DenseMembershipMask4096;
    case 2: return family_candidate::SparseFamilyRepresentationRoleV1::CompactSortedUniverseIndexList;
    case 3: return family_candidate::SparseFamilyRepresentationRoleV1::ActiveBlockListWithLocal64BitMask;
    default: throw std::runtime_error("Unknown representation role in evidence.");
    }
}

family_candidate::SparseFamilyDispatchPolicyV1 DispatchFromU32(std::uint32_t value)
{
    switch (value)
    {
    case 1: return family_candidate::SparseFamilyDispatchPolicyV1::FixedUniverseGroups;
    case 2: return family_candidate::SparseFamilyDispatchPolicyV1::CeilCardinalityGroups;
    case 3: return family_candidate::SparseFamilyDispatchPolicyV1::OneGroupPerActiveBlock;
    default: throw std::runtime_error("Unknown dispatch policy in evidence.");
    }
}

void ValidateConfig(const MeasurementConfigV2& config)
{
    Require(config.runCount > 0 && config.runCount <= 16, "runCount outside qualified range.");
    Require(config.measurementCyclesPerOrder > 0 && config.measurementCyclesPerOrder <= 16,
        "measurementCyclesPerOrder outside qualified range.");
    Require(config.iterationsPerBatch >= 32 && config.iterationsPerBatch <= 65536,
        "iterationsPerBatch outside qualified range.");
    Require(config.warmupIterations > 0 && config.warmupIterations <= 262144,
        "warmupIterations outside qualified range.");
    Require(config.maximumBlockAttempts > 0 && config.maximumBlockAttempts <= 16,
        "maximumBlockAttempts outside qualified range.");
    Require(config.nvmlPollingIntervalMilliseconds > 0 && config.nvmlPollingIntervalMilliseconds <= 1000,
        "NVML polling interval outside qualified range.");
    Require(config.maximumControlRatio >= 1.0 && config.maximumControlRatio <= 2.0,
        "maximumControlRatio invalid.");
    Require(config.relativeNoiseFloor >= 0.0 && config.relativeNoiseFloor <= 0.25,
        "relativeNoiseFloor invalid.");
    Require(config.minimumPairAgreement >= 0.5 && config.minimumPairAgreement <= 1.0,
        "minimumPairAgreement invalid.");
}

void WriteConfig(base::BinaryWriter& writer, const MeasurementConfigV2& value)
{
    writer.WriteU32(value.runCount);
    writer.WriteU32(value.measurementCyclesPerOrder);
    writer.WriteU32(value.iterationsPerBatch);
    writer.WriteU32(value.warmupIterations);
    writer.WriteU32(value.maximumBlockAttempts);
    writer.WriteU32(value.adapterIndex);
    writer.WriteU32(value.nvmlDeviceIndex);
    writer.WriteU32(value.nvmlPollingIntervalMilliseconds);
    WriteBool(writer, value.enableNvmlDiagnostic);
    WriteBool(writer, value.requestStablePowerStateDiagnostic);
    WriteDouble(writer, value.maximumControlRatio);
    WriteDouble(writer, value.relativeNoiseFloor);
    WriteDouble(writer, value.minimumPairAgreement);
}

MeasurementConfigV2 ReadConfig(base::BinaryReader& reader)
{
    MeasurementConfigV2 value;
    value.runCount = ReadU32(reader);
    value.measurementCyclesPerOrder = ReadU32(reader);
    value.iterationsPerBatch = ReadU32(reader);
    value.warmupIterations = ReadU32(reader);
    value.maximumBlockAttempts = ReadU32(reader);
    value.adapterIndex = ReadU32(reader);
    value.nvmlDeviceIndex = ReadU32(reader);
    value.nvmlPollingIntervalMilliseconds = ReadU32(reader);
    value.enableNvmlDiagnostic = ReadBool(reader);
    value.requestStablePowerStateDiagnostic = ReadBool(reader);
    value.maximumControlRatio = ReadDouble(reader);
    value.relativeNoiseFloor = ReadDouble(reader);
    value.minimumPairAgreement = ReadDouble(reader);
    ValidateConfig(value);
    return value;
}

void WriteAdapter(base::BinaryWriter& writer, const AdapterProfileV2& value)
{
    WriteString(writer, value.description);
    writer.WriteU32(value.vendorId); writer.WriteU32(value.deviceId);
    writer.WriteU32(value.subsystemId); writer.WriteU32(value.revision);
    writer.WriteU32(value.luidLow); writer.WriteI32(value.luidHigh);
    writer.WriteU64(value.driverVersion); writer.WriteU64(value.timestampFrequency);
    WriteBool(writer, value.uma); WriteBool(writer, value.cacheCoherentUma);
    WriteDigest(writer, value.fingerprint);
}

AdapterProfileV2 ReadAdapter(base::BinaryReader& reader)
{
    AdapterProfileV2 value;
    value.description = ReadString(reader);
    value.vendorId = ReadU32(reader); value.deviceId = ReadU32(reader);
    value.subsystemId = ReadU32(reader); value.revision = ReadU32(reader);
    value.luidLow = ReadU32(reader); value.luidHigh = ReadI32(reader);
    value.driverVersion = ReadU64(reader); value.timestampFrequency = ReadU64(reader);
    value.uma = ReadBool(reader); value.cacheCoherentUma = ReadBool(reader);
    value.fingerprint = ReadDigest(reader);
    return value;
}

void WriteCandidateProfile(base::BinaryWriter& writer, const CandidateProfileV2& value)
{
    writer.WriteU32(static_cast<std::uint32_t>(value.kind));
    writer.WriteU32(static_cast<std::uint32_t>(value.representationRole));
    writer.WriteU32(static_cast<std::uint32_t>(value.dispatchPolicy));
    writer.WriteU32(value.payloadStrideBytes); writer.WriteU32(value.fixedCapacityBytes);
    WriteDigest(writer, value.targetProfileIdentity);
    WriteDigest(writer, value.observationContractIdentity);
    WriteDigest(writer, value.completionContractIdentity);
    WriteDigest(writer, value.consumerProgramIdentity);
    WriteDigest(writer, value.representationResourceContractIdentity);
    WriteDigest(writer, value.representationShaderBytecodeDigest);
}

CandidateProfileV2 ReadCandidateProfile(base::BinaryReader& reader)
{
    CandidateProfileV2 value;
    value.kind = CandidateFromU32(ReadU32(reader));
    value.representationRole = RoleFromU32(ReadU32(reader));
    value.dispatchPolicy = DispatchFromU32(ReadU32(reader));
    value.payloadStrideBytes = ReadU32(reader); value.fixedCapacityBytes = ReadU32(reader);
    value.targetProfileIdentity = ReadDigest(reader);
    value.observationContractIdentity = ReadDigest(reader);
    value.completionContractIdentity = ReadDigest(reader);
    value.consumerProgramIdentity = ReadDigest(reader);
    value.representationResourceContractIdentity = ReadDigest(reader);
    value.representationShaderBytecodeDigest = ReadDigest(reader);
    return value;
}

void WriteAuthority(base::BinaryWriter& writer, const CandidateCaseAuthorityV2& value)
{
    writer.WriteU32(static_cast<std::uint32_t>(value.kind));
    WriteDigest(writer, value.rawCandidateIdentity);
    WriteDigest(writer, value.verifiedPlanIdentity);
    WriteDigest(writer, value.verifierCertificateIdentity);
    WriteDigest(writer, value.artifactIdentity);
    WriteDigest(writer, value.representationResourceIdentity);
    WriteDigest(writer, value.frozenBindingIdentity);
}

CandidateCaseAuthorityV2 ReadAuthority(base::BinaryReader& reader)
{
    CandidateCaseAuthorityV2 value;
    value.kind = CandidateFromU32(ReadU32(reader));
    value.rawCandidateIdentity = ReadDigest(reader);
    value.verifiedPlanIdentity = ReadDigest(reader);
    value.verifierCertificateIdentity = ReadDigest(reader);
    value.artifactIdentity = ReadDigest(reader);
    value.representationResourceIdentity = ReadDigest(reader);
    value.frozenBindingIdentity = ReadDigest(reader);
    return value;
}

void WriteSnapshot(base::BinaryWriter& writer, const NvmlSnapshotV2& value)
{
    WriteBool(writer, value.available);
    writer.WriteU32(value.graphicsClockMHz); writer.WriteU32(value.smClockMHz);
    writer.WriteU32(value.memoryClockMHz); writer.WriteU32(value.videoClockMHz);
    writer.WriteU32(value.performanceState); writer.WriteU32(value.temperatureC);
    writer.WriteU32(value.powerUsageMilliwatts); writer.WriteU32(value.powerLimitMilliwatts);
    writer.WriteU32(value.gpuUtilizationPercent); writer.WriteU32(value.memoryUtilizationPercent);
    writer.WriteU64(value.clockEventReasons);
    writer.WriteI32(value.graphicsClockResult); writer.WriteI32(value.smClockResult);
    writer.WriteI32(value.memoryClockResult); writer.WriteI32(value.videoClockResult);
    writer.WriteI32(value.performanceStateResult); writer.WriteI32(value.temperatureResult);
    writer.WriteI32(value.powerUsageResult); writer.WriteI32(value.powerLimitResult);
    writer.WriteI32(value.utilizationResult); writer.WriteI32(value.clockEventReasonsResult);
}

NvmlSnapshotV2 ReadSnapshot(base::BinaryReader& reader)
{
    NvmlSnapshotV2 value;
    value.available = ReadBool(reader);
    value.graphicsClockMHz = ReadU32(reader); value.smClockMHz = ReadU32(reader);
    value.memoryClockMHz = ReadU32(reader); value.videoClockMHz = ReadU32(reader);
    value.performanceState = ReadU32(reader); value.temperatureC = ReadU32(reader);
    value.powerUsageMilliwatts = ReadU32(reader); value.powerLimitMilliwatts = ReadU32(reader);
    value.gpuUtilizationPercent = ReadU32(reader); value.memoryUtilizationPercent = ReadU32(reader);
    value.clockEventReasons = ReadU64(reader);
    value.graphicsClockResult = ReadI32(reader); value.smClockResult = ReadI32(reader);
    value.memoryClockResult = ReadI32(reader); value.videoClockResult = ReadI32(reader);
    value.performanceStateResult = ReadI32(reader); value.temperatureResult = ReadI32(reader);
    value.powerUsageResult = ReadI32(reader); value.powerLimitResult = ReadI32(reader);
    value.utilizationResult = ReadI32(reader); value.clockEventReasonsResult = ReadI32(reader);
    return value;
}

void WriteTelemetrySummary(base::BinaryWriter& writer, const BlockTelemetrySummaryV2& value)
{
    WriteBool(writer, value.available); writer.WriteU32(value.recordCount);
    writer.WriteU32(value.minimumGraphicsClockMHz); writer.WriteU32(value.maximumGraphicsClockMHz);
    writer.WriteU32(value.minimumMemoryClockMHz); writer.WriteU32(value.maximumMemoryClockMHz);
    writer.WriteU32(value.minimumPerformanceState); writer.WriteU32(value.maximumPerformanceState);
    writer.WriteU32(value.maximumTemperatureC); writer.WriteU64(value.clockEventReasonsOr);
    WriteBool(writer, value.gpuIdleObserved); WriteBool(writer, value.hardwareThermalObserved);
    WriteBool(writer, value.hardwarePowerBrakeObserved);
}

BlockTelemetrySummaryV2 ReadTelemetrySummary(base::BinaryReader& reader)
{
    BlockTelemetrySummaryV2 value;
    value.available = ReadBool(reader); value.recordCount = ReadU32(reader);
    value.minimumGraphicsClockMHz = ReadU32(reader); value.maximumGraphicsClockMHz = ReadU32(reader);
    value.minimumMemoryClockMHz = ReadU32(reader); value.maximumMemoryClockMHz = ReadU32(reader);
    value.minimumPerformanceState = ReadU32(reader); value.maximumPerformanceState = ReadU32(reader);
    value.maximumTemperatureC = ReadU32(reader); value.clockEventReasonsOr = ReadU64(reader);
    value.gpuIdleObserved = ReadBool(reader); value.hardwareThermalObserved = ReadBool(reader);
    value.hardwarePowerBrakeObserved = ReadBool(reader);
    return value;
}

void WriteSample(base::BinaryWriter& writer, const TimingSampleV2& value)
{
    writer.WriteU32(value.run); writer.WriteU32(value.caseOrder); writer.WriteU32(value.acceptedAttempt);
    writer.WriteU32(value.orderIndex); writer.WriteU32(value.cycle); writer.WriteU32(value.orderPosition);
    writer.WriteU32(static_cast<std::uint32_t>(value.candidate));
    writer.WriteU32(static_cast<std::uint32_t>(value.pattern));
    writer.WriteU32(value.activeCount); writer.WriteU32(value.activeBlockCount);
    writer.WriteU32(value.iterationCount);
    WriteDouble(writer, value.gpuBatchNanoseconds);
    WriteDouble(writer, value.gpuNanosecondsPerIteration);
    WriteDouble(writer, value.blockControlNanosecondsPerIteration);
    WriteDouble(writer, value.controlNormalizedTime);
    writer.WriteU32(value.expectedDispatchGroupsX); writer.WriteU32(value.activeWriteCount);
    writer.WriteU32(value.missingWriteCount); writer.WriteU32(value.unauthorizedWriteCount);
    writer.WriteU32(value.activeMismatchCount); writer.WriteU32(value.nonFiniteCount);
    writer.WriteU32(value.homogeneousCoordinateMismatchCount);
    WriteDouble(writer, value.maxAbsoluteError); WriteDouble(writer, value.maxRelativeError);
    writer.WriteU32(value.maxUlpError); WriteDigest(writer, value.outputDigest);
}

TimingSampleV2 ReadSample(base::BinaryReader& reader)
{
    TimingSampleV2 value;
    value.run = ReadU32(reader); value.caseOrder = ReadU32(reader); value.acceptedAttempt = ReadU32(reader);
    value.orderIndex = ReadU32(reader); value.cycle = ReadU32(reader); value.orderPosition = ReadU32(reader);
    value.candidate = CandidateFromU32(ReadU32(reader)); value.pattern = PatternFromU32(ReadU32(reader));
    value.activeCount = ReadU32(reader); value.activeBlockCount = ReadU32(reader);
    value.iterationCount = ReadU32(reader);
    value.gpuBatchNanoseconds = ReadDouble(reader);
    value.gpuNanosecondsPerIteration = ReadDouble(reader);
    value.blockControlNanosecondsPerIteration = ReadDouble(reader);
    value.controlNormalizedTime = ReadDouble(reader);
    value.expectedDispatchGroupsX = ReadU32(reader); value.activeWriteCount = ReadU32(reader);
    value.missingWriteCount = ReadU32(reader); value.unauthorizedWriteCount = ReadU32(reader);
    value.activeMismatchCount = ReadU32(reader); value.nonFiniteCount = ReadU32(reader);
    value.homogeneousCoordinateMismatchCount = ReadU32(reader);
    value.maxAbsoluteError = ReadDouble(reader); value.maxRelativeError = ReadDouble(reader);
    value.maxUlpError = ReadU32(reader); value.outputDigest = ReadDigest(reader);
    return value;
}

void WriteAttempt(base::BinaryWriter& writer, const BlockAttemptV2& value)
{
    writer.WriteU32(value.run); writer.WriteU32(value.caseOrder); writer.WriteU32(value.attempt);
    writer.WriteU32(static_cast<std::uint32_t>(value.pattern));
    writer.WriteU32(value.activeCount); writer.WriteU32(value.activeBlockCount);
    WriteDigest(writer, value.sparseSetIdentity);
    for (const auto& authority : value.authorities) WriteAuthority(writer, authority);
    WriteBool(writer, value.accepted); WriteString(writer, value.rejectionReason);
    WriteDouble(writer, value.controlBeforeNanosecondsPerIteration);
    WriteDouble(writer, value.controlAfterNanosecondsPerIteration);
    WriteDouble(writer, value.controlReferenceNanosecondsPerIteration);
    WriteDouble(writer, value.controlRatio);
    WriteSnapshot(writer, value.preBlockTelemetry); WriteSnapshot(writer, value.postBlockTelemetry);
    WriteTelemetrySummary(writer, value.telemetrySummary);
    writer.WriteU32(static_cast<std::uint32_t>(value.samples.size()));
    for (const auto& sample : value.samples) WriteSample(writer, sample);
}

BlockAttemptV2 ReadAttempt(base::BinaryReader& reader)
{
    BlockAttemptV2 value;
    value.run = ReadU32(reader); value.caseOrder = ReadU32(reader); value.attempt = ReadU32(reader);
    value.pattern = PatternFromU32(ReadU32(reader));
    value.activeCount = ReadU32(reader); value.activeBlockCount = ReadU32(reader);
    value.sparseSetIdentity = ReadDigest(reader);
    for (auto& authority : value.authorities) authority = ReadAuthority(reader);
    value.accepted = ReadBool(reader); value.rejectionReason = ReadString(reader, 4096);
    value.controlBeforeNanosecondsPerIteration = ReadDouble(reader);
    value.controlAfterNanosecondsPerIteration = ReadDouble(reader);
    value.controlReferenceNanosecondsPerIteration = ReadDouble(reader);
    value.controlRatio = ReadDouble(reader);
    value.preBlockTelemetry = ReadSnapshot(reader); value.postBlockTelemetry = ReadSnapshot(reader);
    value.telemetrySummary = ReadTelemetrySummary(reader);
    const auto sampleCount = ReadU32(reader);
    Require(sampleCount <= 1000000u, "Evidence sample count exceeds limit.");
    value.samples.reserve(sampleCount);
    for (std::uint32_t index = 0; index < sampleCount; ++index) value.samples.push_back(ReadSample(reader));
    return value;
}

std::size_t CandidateIndex(CandidateKindV2 candidate)
{
    const auto kinds = CanonicalCandidateKindsV2();
    const auto found = std::find(kinds.begin(), kinds.end(), candidate);
    Require(found != kinds.end(), "Candidate index not found.");
    return static_cast<std::size_t>(std::distance(kinds.begin(), found));
}

std::vector<double> SamplesFor(
    const MeasurementEvidenceV2& evidence,
    PatternKindV2 pattern,
    std::uint32_t activeCount,
    CandidateKindV2 candidate,
    bool normalized)
{
    std::vector<double> values;
    for (const auto& attempt : evidence.attempts)
    {
        if (!attempt.accepted || attempt.pattern != pattern || attempt.activeCount != activeCount) continue;
        for (const auto& sample : attempt.samples)
        {
            if (sample.candidate != candidate) continue;
            values.push_back(normalized ? sample.controlNormalizedTime : sample.gpuNanosecondsPerIteration);
        }
    }
    return values;
}

double Median(std::vector<double> values)
{
    Require(!values.empty(), "Median requires at least one value.");
    std::sort(values.begin(), values.end());
    const std::size_t middle = values.size() / 2;
    return values.size() % 2 == 0 ? (values[middle - 1] + values[middle]) * 0.5 : values[middle];
}

using ObservationKey = std::tuple<
    std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t>;

std::map<ObservationKey, std::array<std::optional<double>, CanonicalCandidateCountV2>>
PairedObservationMap(
    const MeasurementEvidenceV2& evidence,
    PatternKindV2 pattern,
    std::uint32_t activeCount)
{
    std::map<ObservationKey, std::array<std::optional<double>, CanonicalCandidateCountV2>> result;
    for (const auto& attempt : evidence.attempts)
    {
        if (!attempt.accepted || attempt.pattern != pattern || attempt.activeCount != activeCount) continue;
        for (const auto& sample : attempt.samples)
        {
            const ObservationKey key{
                sample.run, sample.caseOrder, sample.acceptedAttempt, sample.orderIndex, sample.cycle};
            auto& values = result[key];
            const std::size_t index = CandidateIndex(sample.candidate);
            Require(!values[index].has_value(), "Duplicate Candidate in one paired observation.");
            values[index] = sample.gpuNanosecondsPerIteration;
        }
    }
    for (const auto& [key, values] : result)
    {
        (void)key;
        for (const auto& value : values)
            Require(value.has_value() && value.value() > 0.0,
                "Paired observation is incomplete or invalid.");
    }
    return result;
}

PairDecisionV2 BuildPairDecision(
    const MeasurementEvidenceV2& evidence,
    PatternKindV2 pattern,
    std::uint32_t activeCount,
    CandidateKindV2 left,
    CandidateKindV2 right)
{
    const auto observations = PairedObservationMap(evidence, pattern, activeCount);
    const std::size_t leftIndex = CandidateIndex(left);
    const std::size_t rightIndex = CandidateIndex(right);
    std::vector<double> ratios;
    ratios.reserve(observations.size());
    std::uint32_t leftWins = 0;
    std::uint32_t rightWins = 0;
    for (const auto& [key, values] : observations)
    {
        (void)key;
        const double ratio = values[leftIndex].value() / values[rightIndex].value();
        ratios.push_back(ratio);
        if (ratio < 1.0) ++leftWins;
        else if (ratio > 1.0) ++rightWins;
    }
    Require(!ratios.empty(), "Pair decision has no paired observations.");
    PairDecisionV2 result;
    result.left = left;
    result.right = right;
    result.medianLeftOverRight = Median(ratios);
    result.pairedObservationCount = static_cast<std::uint32_t>(ratios.size());
    result.leftWinFraction = static_cast<double>(leftWins) / ratios.size();
    result.rightWinFraction = static_cast<double>(rightWins) / ratios.size();
    const double lower = 1.0 - evidence.config.relativeNoiseFloor;
    const double upper = 1.0 + evidence.config.relativeNoiseFloor;
    if (result.medianLeftOverRight < lower &&
        result.leftWinFraction >= evidence.config.minimumPairAgreement)
        result.winner = PairWinnerV2::Left;
    else if (result.medianLeftOverRight > upper &&
        result.rightWinFraction >= evidence.config.minimumPairAgreement)
        result.winner = PairWinnerV2::Right;
    else
        result.winner = PairWinnerV2::NoiseEquivalent;
    return result;
}

std::vector<CandidateKindV2> WinnerSetFromPairs(
    const PairDecisionV2& denseCompact,
    const PairDecisionV2& compactBlock,
    const PairDecisionV2& denseBlock)
{
    std::array<bool, CanonicalCandidateCountV2> beaten{};
    if (denseCompact.winner == PairWinnerV2::Left) beaten[1] = true;
    else if (denseCompact.winner == PairWinnerV2::Right) beaten[0] = true;

    if (compactBlock.winner == PairWinnerV2::Left) beaten[2] = true;
    else if (compactBlock.winner == PairWinnerV2::Right) beaten[1] = true;

    if (denseBlock.winner == PairWinnerV2::Left) beaten[2] = true;
    else if (denseBlock.winner == PairWinnerV2::Right) beaten[0] = true;

    const auto kinds = CanonicalCandidateKindsV2();
    std::vector<CandidateKindV2> result;
    for (std::size_t index = 0; index < beaten.size(); ++index)
        if (!beaten[index]) result.push_back(kinds[index]);

    // A non-transitive observed cycle must not be forced into one winner.
    if (result.empty()) result.assign(kinds.begin(), kinds.end());
    return result;
}

TransitionClassificationV2 ClassifyTransitions(std::size_t transitionCount, bool unresolved)
{
    if (unresolved) return TransitionClassificationV2::NoiseEquivalentOrUnresolved;
    if (transitionCount == 0) return TransitionClassificationV2::NoObservedWinnerTransition;
    if (transitionCount == 1) return TransitionClassificationV2::SingleStableWinnerTransition;
    return TransitionClassificationV2::MultipleWinnerTransitions;
}

std::string WinnerText(const std::vector<CandidateKindV2>& winners)
{
    std::string result;
    for (std::size_t index = 0; index < winners.size(); ++index)
    {
        if (index != 0) result += ',';
        result += CandidateLetterV2(winners[index]);
    }
    return result;
}

std::string PairWinnerName(PairWinnerV2 value)
{
    switch (value)
    {
    case PairWinnerV2::Left: return "Left";
    case PairWinnerV2::Right: return "Right";
    case PairWinnerV2::NoiseEquivalent: return "NoiseEquivalent";
    }
    return "Unknown";
}

std::string TransitionName(TransitionClassificationV2 value)
{
    switch (value)
    {
    case TransitionClassificationV2::NoObservedWinnerTransition: return "NoObservedWinnerTransition";
    case TransitionClassificationV2::SingleStableWinnerTransition: return "SingleStableWinnerTransition";
    case TransitionClassificationV2::MultipleWinnerTransitions: return "MultipleWinnerTransitions";
    case TransitionClassificationV2::NoiseEquivalentOrUnresolved: return "NoiseEquivalentOrUnresolved";
    }
    return "Unknown";
}

std::string CsvEscape(std::string_view value)
{
    bool needsQuotes = value.find_first_of(",\"\r\n") != std::string_view::npos;
    if (!needsQuotes) return std::string(value);
    std::string result = "\"";
    for (char c : value)
    {
        if (c == '\"') result += "\"\"";
        else result += c;
    }
    result += '\"';
    return result;
}

std::vector<std::byte> StringBytes(std::string_view value)
{
    return std::vector<std::byte>(
        reinterpret_cast<const std::byte*>(value.data()),
        reinterpret_cast<const std::byte*>(value.data() + value.size()));
}

void WriteTextFile(const std::filesystem::path& path, std::string_view text)
{
    const auto bytes = StringBytes(text);
    auto result = base::WriteAllBytes(path, bytes);
    if (!result) throw std::runtime_error(result.Error());
}

void ValidateEvidence(const MeasurementEvidenceV2& evidence)
{
    Require(evidence.schemaVersion == MeasurementEvidenceSchemaVersionV2, "Unexpected evidence schema.");
    ValidateConfig(evidence.config);
    Require(evidence.measurementProfileIdentity == BuildMeasurementProfileIdentityV2(evidence.config),
        "Measurement profile identity mismatch.");
    Require(evidence.caseScheduleIdentity == BuildCaseScheduleIdentityV2(evidence.config.runCount),
        "Case schedule identity mismatch.");
    Require(evidence.candidateOrderIdentity == BuildCandidateOrderIdentityV2(),
        "Candidate order identity mismatch.");
    std::set<std::tuple<std::uint32_t, std::uint32_t, std::uint32_t>> accepted;
    const std::size_t expectedSamples =
        static_cast<std::size_t>(CanonicalOrderCountV2) *
        evidence.config.measurementCyclesPerOrder * CanonicalCandidateCountV2;
    for (const auto& attempt : evidence.attempts)
    {
        Require(attempt.activeCount <= semantic::WorkUniverseCountV1, "Attempt activeCount invalid.");
        Require(attempt.controlRatio >= 1.0 || attempt.controlRatio == 0.0,
            "Attempt control ratio invalid.");
        if (attempt.accepted)
        {
            Require(attempt.rejectionReason.empty(), "Accepted attempt has rejection reason.");
            Require(attempt.samples.size() == expectedSamples,
                "Accepted attempt does not contain the complete balanced sample set.");
            Require(accepted.emplace(attempt.run, attempt.caseOrder, attempt.activeCount).second,
                "More than one accepted attempt exists for one run/case.");
            Require(std::isfinite(attempt.controlReferenceNanosecondsPerIteration) &&
                attempt.controlReferenceNanosecondsPerIteration > 0.0,
                "Accepted attempt control reference is invalid.");
            std::map<std::pair<std::uint32_t, std::uint32_t>, std::array<std::uint32_t, CanonicalCandidateCountV2>>
                groupCounts;
            for (const auto& sample : attempt.samples)
            {
                Require(sample.run == attempt.run && sample.caseOrder == attempt.caseOrder,
                    "Sample run/case does not match attempt.");
                Require(sample.pattern == attempt.pattern && sample.activeCount == attempt.activeCount,
                    "Sample Pattern/K does not match attempt.");
                Require(sample.iterationCount == evidence.config.iterationsPerBatch,
                    "Sample iteration count does not match profile.");
                Require(std::isfinite(sample.gpuNanosecondsPerIteration) &&
                    sample.gpuNanosecondsPerIteration > 0.0,
                    "Sample timing is invalid.");
                Require(std::isfinite(sample.blockControlNanosecondsPerIteration) &&
                    sample.blockControlNanosecondsPerIteration > 0.0,
                    "Sample block control is invalid.");
                Require(std::isfinite(sample.controlNormalizedTime) &&
                    sample.controlNormalizedTime > 0.0,
                    "Sample normalized timing is invalid.");
                Require(sample.missingWriteCount == 0 && sample.unauthorizedWriteCount == 0 &&
                    sample.activeMismatchCount == 0 && sample.nonFiniteCount == 0 &&
                    sample.homogeneousCoordinateMismatchCount == 0,
                    "Accepted sample failed numeric qualification.");
                auto& counts = groupCounts[{sample.orderIndex, sample.cycle}];
                ++counts[CandidateIndex(sample.candidate)];
            }
            Require(groupCounts.size() ==
                static_cast<std::size_t>(CanonicalOrderCountV2) *
                evidence.config.measurementCyclesPerOrder,
                "Paired observation group count mismatch.");
            for (const auto& [key, counts] : groupCounts)
            {
                (void)key;
                for (std::uint32_t count : counts)
                    Require(count == 1, "Paired observation does not contain exactly one A/B/C sample.");
            }
        }
        else
        {
            Require(!attempt.rejectionReason.empty(), "Rejected attempt lacks reason.");
            Require(attempt.samples.empty(), "Rejected attempt must not expose ranking samples.");
        }
    }
}
}

std::string CandidateNameV2(CandidateKindV2 value)
{
    switch (value)
    {
    case CandidateKindV2::DenseMembershipMaskPredicate: return "DenseMembershipMaskPredicate";
    case CandidateKindV2::CompactIndexListDispatch: return "CompactIndexListDispatch";
    case CandidateKindV2::ActiveBlockListLocalMask: return "ActiveBlockListLocalMask";
    }
    return "Unknown";
}

char CandidateLetterV2(CandidateKindV2 value)
{
    switch (value)
    {
    case CandidateKindV2::DenseMembershipMaskPredicate: return 'A';
    case CandidateKindV2::CompactIndexListDispatch: return 'B';
    case CandidateKindV2::ActiveBlockListLocalMask: return 'C';
    }
    return '?';
}

std::string PatternNameV2(PatternKindV2 value)
{
    switch (value)
    {
    case PatternKindV2::PrefixControl: return "PrefixControl";
    case PatternKindV2::UniformStride: return "UniformStride";
    case PatternKindV2::BlockClusteredPermutation: return "BlockClusteredPermutation";
    case PatternKindV2::HashScatterPermutation: return "HashScatterPermutation";
    }
    return "Unknown";
}

std::array<CandidateKindV2, CanonicalCandidateCountV2> CanonicalCandidateKindsV2()
{
    return {
        CandidateKindV2::DenseMembershipMaskPredicate,
        CandidateKindV2::CompactIndexListDispatch,
        CandidateKindV2::ActiveBlockListLocalMask};
}

std::array<PatternKindV2, CanonicalPatternCountV2> CanonicalPatternsV2()
{
    return {
        PatternKindV2::PrefixControl,
        PatternKindV2::UniformStride,
        PatternKindV2::BlockClusteredPermutation,
        PatternKindV2::HashScatterPermutation};
}

const std::array<std::uint32_t, CanonicalMeasurementCardinalityCountV2>&
CanonicalMeasurementCardinalitiesV2() noexcept
{
    static constexpr std::array<std::uint32_t, CanonicalMeasurementCardinalityCountV2> values{
        1, 8, 32, 64, 128, 256, 512, 1024, 2048, 3072, 4096};
    return values;
}

std::vector<std::array<std::uint32_t, CanonicalCandidateCountV2>> CanonicalBalancedOrdersV2()
{
    return {{0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0}};
}

std::vector<std::pair<PatternKindV2, std::uint32_t>> CanonicalBalancedCaseScheduleV2(std::uint32_t run)
{
    std::vector<std::pair<PatternKindV2, std::uint32_t>> result;
    const auto patterns = CanonicalPatternsV2();
    const auto& cardinalities = CanonicalMeasurementCardinalitiesV2();
    result.reserve(patterns.size() * cardinalities.size());
    if ((run & 1u) == 0u)
    {
        for (std::size_t kIndex = 0; kIndex < cardinalities.size(); ++kIndex)
            for (std::size_t position = 0; position < patterns.size(); ++position)
                result.emplace_back(patterns[(position + kIndex + run) % patterns.size()], cardinalities[kIndex]);
    }
    else
    {
        for (std::size_t reverse = 0; reverse < cardinalities.size(); ++reverse)
        {
            const std::size_t kIndex = cardinalities.size() - 1u - reverse;
            for (std::size_t position = 0; position < patterns.size(); ++position)
            {
                const std::size_t patternIndex =
                    (patterns.size() - 1u - position + kIndex + run) % patterns.size();
                result.emplace_back(patterns[patternIndex], cardinalities[kIndex]);
            }
        }
    }
    return result;
}

base::Digest256 BuildMeasurementProfileIdentityV2(const MeasurementConfigV2& config)
{
    ValidateConfig(config);
    base::BinaryWriter writer;
    WriteString(writer, MeasurementProfileDomain);
    WriteConfig(writer, config);
    return base::Sha256(writer.Bytes());
}

base::Digest256 BuildCaseScheduleIdentityV2(std::uint32_t runCount)
{
    base::BinaryWriter writer;
    WriteString(writer, CaseScheduleDomain);
    writer.WriteU32(runCount);
    for (std::uint32_t run = 0; run < runCount; ++run)
    {
        const auto schedule = CanonicalBalancedCaseScheduleV2(run);
        writer.WriteU32(static_cast<std::uint32_t>(schedule.size()));
        for (const auto& [pattern, activeCount] : schedule)
        {
            writer.WriteU32(static_cast<std::uint32_t>(pattern));
            writer.WriteU32(activeCount);
        }
    }
    return base::Sha256(writer.Bytes());
}

base::Digest256 BuildCandidateOrderIdentityV2()
{
    base::BinaryWriter writer;
    WriteString(writer, CandidateOrderDomain);
    const auto orders = CanonicalBalancedOrdersV2();
    writer.WriteU32(static_cast<std::uint32_t>(orders.size()));
    for (const auto& order : orders)
        for (std::uint32_t value : order) writer.WriteU32(value);
    return base::Sha256(writer.Bytes());
}

std::vector<std::byte> SerializeMeasurementEvidenceV2(const MeasurementEvidenceV2& evidence)
{
    ValidateEvidence(evidence);
    base::BinaryWriter writer;
    WriteString(writer, EvidenceMagic);
    writer.WriteU32(evidence.schemaVersion);
    writer.WriteU64(evidence.captureUnixNanoseconds);
    WriteConfig(writer, evidence.config);
    WriteAdapter(writer, evidence.adapter);
    WriteString(writer, evidence.nvmlDeviceName);
    WriteString(writer, evidence.nvmlDeviceUuid);
    WriteBool(writer, evidence.nvmlAvailable);
    WriteBool(writer, evidence.stablePowerStateApplied);
    WriteDigest(writer, evidence.architectureEvidenceIdentity);
    WriteDigest(writer, evidence.controlledRecoveryEvidenceIdentity);
    WriteDigest(writer, evidence.freshRematerializationEvidenceIdentity);
    WriteDigest(writer, evidence.semanticIdentity);
    WriteDigest(writer, evidence.verificationContextIdentity);
    WriteDigest(writer, evidence.measurementProfileIdentity);
    WriteDigest(writer, evidence.caseScheduleIdentity);
    WriteDigest(writer, evidence.candidateOrderIdentity);
    for (const auto& candidate : evidence.candidates) WriteCandidateProfile(writer, candidate);
    writer.WriteU32(static_cast<std::uint32_t>(evidence.attempts.size()));
    for (const auto& attempt : evidence.attempts) WriteAttempt(writer, attempt);
    const auto digest = base::Sha256(writer.Bytes());
    WriteDigest(writer, digest);
    return std::move(writer).Take();
}

base::Result<MeasurementEvidenceV2, std::string>
ReadMeasurementEvidenceV2(const std::filesystem::path& evidencePath)
{
    try
    {
        auto bytesResult = base::ReadAllBytes(evidencePath);
        if (!bytesResult) throw std::runtime_error(bytesResult.Error());
        const auto& bytes = bytesResult.Value();
        Require(bytes.size() >= 32, "Evidence is too small.");
        const auto payload = std::span(bytes).first(bytes.size() - 32);
        base::Digest256 stored{};
        std::memcpy(stored.data(), bytes.data() + payload.size(), stored.size());
        Require(base::Sha256(payload) == stored, "Evidence digest mismatch.");
        base::BinaryReader reader(payload);
        Require(ReadString(reader, 256) == EvidenceMagic, "Evidence magic mismatch.");
        MeasurementEvidenceV2 evidence;
        evidence.schemaVersion = ReadU32(reader);
        evidence.captureUnixNanoseconds = ReadU64(reader);
        evidence.config = ReadConfig(reader);
        evidence.adapter = ReadAdapter(reader);
        evidence.nvmlDeviceName = ReadString(reader, 512);
        evidence.nvmlDeviceUuid = ReadString(reader, 512);
        evidence.nvmlAvailable = ReadBool(reader);
        evidence.stablePowerStateApplied = ReadBool(reader);
        evidence.architectureEvidenceIdentity = ReadDigest(reader);
        evidence.controlledRecoveryEvidenceIdentity = ReadDigest(reader);
        evidence.freshRematerializationEvidenceIdentity = ReadDigest(reader);
        evidence.semanticIdentity = ReadDigest(reader);
        evidence.verificationContextIdentity = ReadDigest(reader);
        evidence.measurementProfileIdentity = ReadDigest(reader);
        evidence.caseScheduleIdentity = ReadDigest(reader);
        evidence.candidateOrderIdentity = ReadDigest(reader);
        for (auto& candidate : evidence.candidates) candidate = ReadCandidateProfile(reader);
        const auto attemptCount = ReadU32(reader);
        Require(attemptCount <= 100000u, "Evidence attempt count exceeds limit.");
        evidence.attempts.reserve(attemptCount);
        for (std::uint32_t index = 0; index < attemptCount; ++index)
            evidence.attempts.push_back(ReadAttempt(reader));
        Require(reader.Remaining() == 0, "Evidence contains trailing payload bytes.");
        ValidateEvidence(evidence);
        return base::Result<MeasurementEvidenceV2, std::string>::Success(std::move(evidence));
    }
    catch (const std::exception& error)
    {
        return base::Result<MeasurementEvidenceV2, std::string>::Failure(error.what());
    }
}

base::Result<void, std::string>
WriteMeasurementArtifactsV2(
    const MeasurementEvidenceV2& evidence,
    const std::filesystem::path& outputDirectory)
{
    try
    {
        std::filesystem::create_directories(outputDirectory);
        const auto binary = SerializeMeasurementEvidenceV2(evidence);
        auto written = base::WriteAllBytes(outputDirectory / "spiral6_measurement_evidence_v2.bin", binary);
        if (!written) throw std::runtime_error(written.Error());

        std::ostringstream samples;
        samples << "accepted,run,caseOrder,attempt,pattern,K,activeBlocks,orderIndex,cycle,orderPosition,candidate,iterations,gpuBatchNs,gpuNsPerIteration,blockControlNsPerIteration,controlNormalizedTime,dispatchGroups,outputDigest,rejectionReason\n";
        samples << std::setprecision(17);
        for (const auto& attempt : evidence.attempts)
        {
            if (attempt.accepted)
            {
                for (const auto& sample : attempt.samples)
                {
                    samples << "true," << sample.run << ',' << sample.caseOrder << ',' << attempt.attempt << ','
                        << PatternNameV2(sample.pattern) << ',' << sample.activeCount << ',' << sample.activeBlockCount << ','
                        << sample.orderIndex << ',' << sample.cycle << ',' << sample.orderPosition << ','
                        << CandidateLetterV2(sample.candidate) << ',' << sample.iterationCount << ','
                        << sample.gpuBatchNanoseconds << ',' << sample.gpuNanosecondsPerIteration << ','
                        << sample.blockControlNanosecondsPerIteration << ','
                        << sample.controlNormalizedTime << ','
                        << sample.expectedDispatchGroupsX << ',' << base::ToHex(sample.outputDigest) << ",\n";
                }
            }
            else
            {
                samples << "false," << attempt.run << ',' << attempt.caseOrder << ',' << attempt.attempt << ','
                    << PatternNameV2(attempt.pattern) << ',' << attempt.activeCount << ',' << attempt.activeBlockCount
                    << ",,,,,,,,,,,,," << CsvEscape(attempt.rejectionReason) << '\n';
            }
        }
        WriteTextFile(outputDirectory / "spiral6_measurement_samples_v2.csv", samples.str());

        std::ostringstream blocks;
        blocks << "run,caseOrder,attempt,accepted,pattern,K,setIdentity,rejectionReason,controlBeforeNsPerIter,controlAfterNsPerIter,controlReferenceNsPerIter,controlRatio,nvmlAvailable,prePState,postPState,minPState,maxPState,preGraphicsMHz,postGraphicsMHz,minGraphicsMHz,maxGraphicsMHz,preMemoryMHz,postMemoryMHz,minMemoryMHz,maxMemoryMHz,eventReasonsOr,gpuIdleObserved,hwThermalObserved,hwPowerBrakeObserved,telemetryRecords\n";
        blocks << std::setprecision(17);
        for (const auto& value : evidence.attempts)
        {
            blocks << value.run << ',' << value.caseOrder << ',' << value.attempt << ','
                << (value.accepted ? "true" : "false") << ',' << PatternNameV2(value.pattern) << ','
                << value.activeCount << ',' << base::ToHex(value.sparseSetIdentity) << ','
                << CsvEscape(value.rejectionReason) << ',' << value.controlBeforeNanosecondsPerIteration << ','
                << value.controlAfterNanosecondsPerIteration << ','
                << value.controlReferenceNanosecondsPerIteration << ','
                << value.controlRatio << ','
                << (value.telemetrySummary.available ? "true" : "false") << ','
                << value.preBlockTelemetry.performanceState << ',' << value.postBlockTelemetry.performanceState << ','
                << value.telemetrySummary.minimumPerformanceState << ',' << value.telemetrySummary.maximumPerformanceState << ','
                << value.preBlockTelemetry.graphicsClockMHz << ',' << value.postBlockTelemetry.graphicsClockMHz << ','
                << value.telemetrySummary.minimumGraphicsClockMHz << ',' << value.telemetrySummary.maximumGraphicsClockMHz << ','
                << value.preBlockTelemetry.memoryClockMHz << ',' << value.postBlockTelemetry.memoryClockMHz << ','
                << value.telemetrySummary.minimumMemoryClockMHz << ',' << value.telemetrySummary.maximumMemoryClockMHz << ','
                << value.telemetrySummary.clockEventReasonsOr << ','
                << (value.telemetrySummary.gpuIdleObserved ? "true" : "false") << ','
                << (value.telemetrySummary.hardwareThermalObserved ? "true" : "false") << ','
                << (value.telemetrySummary.hardwarePowerBrakeObserved ? "true" : "false") << ','
                << value.telemetrySummary.recordCount << '\n';
        }
        WriteTextFile(outputDirectory / "spiral6_block_stability_v2.csv", blocks.str());

        std::uint32_t acceptedCount = 0;
        std::uint32_t rejectedCount = 0;
        for (const auto& value : evidence.attempts) value.accepted ? ++acceptedCount : ++rejectedCount;
        std::ostringstream manifest;
        manifest << "{\n"
            << "  \"schema\": \"SGE4-5.Spiral6.RelativePerformanceManifest.V2\",\n"
            << "  \"adapter\": \"" << evidence.adapter.description << "\",\n"
            << "  \"adapterFingerprint\": \"" << base::ToHex(evidence.adapter.fingerprint) << "\",\n"
            << "  \"nvmlAvailable\": " << (evidence.nvmlAvailable ? "true" : "false") << ",\n"
            << "  \"nvmlDeviceName\": \"" << evidence.nvmlDeviceName << "\",\n"
            << "  \"measurementProfileIdentity\": \"" << base::ToHex(evidence.measurementProfileIdentity) << "\",\n"
            << "  \"caseScheduleIdentity\": \"" << base::ToHex(evidence.caseScheduleIdentity) << "\",\n"
            << "  \"candidateOrderIdentity\": \"" << base::ToHex(evidence.candidateOrderIdentity) << "\",\n"
            << "  \"decisionMetric\": \"BlockLocalPairedCandidateRatio\",\n"
            << "  \"absoluteNanosecondsRole\": \"DescriptiveOnly\",\n"
            << "  \"nvmlRole\": \"DiagnosticOnly\",\n"
            << "  \"acceptedBlocks\": " << acceptedCount << ",\n"
            << "  \"rejectedAttempts\": " << rejectedCount << ",\n"
            << "  \"evidenceSha256\": \"" << base::ToHex(base::Sha256(binary)) << "\"\n"
            << "}\n";
        WriteTextFile(outputDirectory / "spiral6_measurement_manifest_v2.json", manifest.str());
        return base::Result<void, std::string>::Success();
    }
    catch (const std::exception& error)
    {
        return base::Result<void, std::string>::Failure(error.what());
    }
}

base::Result<DecisionAnalysisV2, std::string>
AnalyzeDecisionEvidenceV2(const MeasurementEvidenceV2& evidence)
{
    try
    {
        ValidateEvidence(evidence);
        DecisionAnalysisV2 analysis;
        for (const auto& attempt : evidence.attempts)
            attempt.accepted ? ++analysis.acceptedBlockCount : ++analysis.rejectedBlockCount;

        const auto kinds = CanonicalCandidateKindsV2();
        for (PatternKindV2 pattern : CanonicalPatternsV2())
        {
            for (std::uint32_t activeCount : CanonicalMeasurementCardinalitiesV2())
            {
                PerPatternCardinalityDecisionV2 value;
                value.pattern = pattern;
                value.activeCount = activeCount;
                bool foundBlockCount = false;
                for (const auto& attempt : evidence.attempts)
                {
                    if (attempt.accepted && attempt.pattern == pattern &&
                        attempt.activeCount == activeCount)
                    {
                        value.activeBlockCount = attempt.activeBlockCount;
                        foundBlockCount = true;
                        break;
                    }
                }
                Require(foundBlockCount, "Missing accepted block for canonical Pattern/K.");

                for (std::size_t index = 0; index < kinds.size(); ++index)
                {
                    value.medianAbsoluteNanosecondsPerIteration[index] =
                        Median(SamplesFor(evidence, pattern, activeCount, kinds[index], false));
                    value.medianControlNormalizedTime[index] =
                        Median(SamplesFor(evidence, pattern, activeCount, kinds[index], true));
                }
                value.denseVersusCompact = BuildPairDecision(
                    evidence, pattern, activeCount, kinds[0], kinds[1]);
                value.compactVersusActiveBlock = BuildPairDecision(
                    evidence, pattern, activeCount, kinds[1], kinds[2]);
                value.denseVersusActiveBlock = BuildPairDecision(
                    evidence, pattern, activeCount, kinds[0], kinds[2]);
                value.globalWinnerSet = WinnerSetFromPairs(
                    value.denseVersusCompact,
                    value.compactVersusActiveBlock,
                    value.denseVersusActiveBlock);
                value.pairedObservationCount =
                    value.denseVersusCompact.pairedObservationCount;
                Require(value.pairedObservationCount ==
                    value.compactVersusActiveBlock.pairedObservationCount &&
                    value.pairedObservationCount ==
                    value.denseVersusActiveBlock.pairedObservationCount,
                    "Pairwise observation counts differ.");
                analysis.cases.push_back(std::move(value));
            }
        }

        const auto patterns = CanonicalPatternsV2();
        for (std::size_t patternIndex = 0; patternIndex < patterns.size(); ++patternIndex)
        {
            auto& transition = analysis.patternTransitions[patternIndex];
            transition.pattern = patterns[patternIndex];
            std::vector<const PerPatternCardinalityDecisionV2*> values;
            for (const auto& value : analysis.cases)
                if (value.pattern == transition.pattern) values.push_back(&value);
            bool abUnresolved = false;
            bool bcUnresolved = false;
            for (std::size_t index = 1; index < values.size(); ++index)
            {
                const auto* previous = values[index - 1];
                const auto* current = values[index];
                const auto previousAb = previous->denseVersusCompact.winner;
                const auto currentAb = current->denseVersusCompact.winner;
                if (previousAb == PairWinnerV2::NoiseEquivalent ||
                    currentAb == PairWinnerV2::NoiseEquivalent)
                    abUnresolved = true;
                else if (previousAb != currentAb)
                {
                    transition.denseCompactTransitions.push_back({
                        transition.pattern,
                        previous->activeCount,
                        current->activeCount,
                        previousAb == PairWinnerV2::Left ? kinds[0] : kinds[1],
                        currentAb == PairWinnerV2::Left ? kinds[0] : kinds[1]});
                }

                const auto previousBc = previous->compactVersusActiveBlock.winner;
                const auto currentBc = current->compactVersusActiveBlock.winner;
                if (previousBc == PairWinnerV2::NoiseEquivalent ||
                    currentBc == PairWinnerV2::NoiseEquivalent)
                    bcUnresolved = true;
                else if (previousBc != currentBc)
                {
                    transition.compactBlockTransitions.push_back({
                        transition.pattern,
                        previous->activeCount,
                        current->activeCount,
                        previousBc == PairWinnerV2::Left ? kinds[1] : kinds[2],
                        currentBc == PairWinnerV2::Left ? kinds[1] : kinds[2]});
                }
            }
            transition.denseCompactClassification =
                ClassifyTransitions(transition.denseCompactTransitions.size(), abUnresolved);
            transition.compactBlockClassification =
                ClassifyTransitions(transition.compactBlockTransitions.size(), bcUnresolved);
        }

        std::set<CandidateKindV2> allWinners;
        std::set<std::string> allWinnerSetSignatures;
        for (const auto& value : analysis.cases)
        {
            allWinners.insert(value.globalWinnerSet.begin(), value.globalWinnerSet.end());
            allWinnerSetSignatures.insert(WinnerText(value.globalWinnerSet));
        }
        analysis.overallObservedWinnerSet.assign(allWinners.begin(), allWinners.end());
        analysis.winnerChangesAcrossCardinality = allWinnerSetSignatures.size() > 1;

        for (std::uint32_t activeCount : CanonicalMeasurementCardinalitiesV2())
        {
            std::set<std::string> winnerSetSignatures;
            for (const auto& value : analysis.cases)
                if (value.activeCount == activeCount)
                    winnerSetSignatures.insert(WinnerText(value.globalWinnerSet));
            if (winnerSetSignatures.size() > 1)
                analysis.sameCardinalityWinnerDependsOnPattern = true;
        }
        return base::Result<DecisionAnalysisV2, std::string>::Success(std::move(analysis));
    }
    catch (const std::exception& error)
    {
        return base::Result<DecisionAnalysisV2, std::string>::Failure(error.what());
    }
}

base::Result<void, std::string>
WriteDecisionEvidenceReportV2(
    const MeasurementEvidenceV2& evidence,
    const std::filesystem::path& outputDirectory)
{
    try
    {
        auto analysisResult = AnalyzeDecisionEvidenceV2(evidence);
        if (!analysisResult) throw std::runtime_error(analysisResult.Error());
        const auto& analysis = analysisResult.Value();
        std::filesystem::create_directories(outputDirectory);
        std::ostringstream report;
        report << "# Spiral 6 CU6 Relative Sparse Representation Map V2\n\n";
        report << "## Measurement contract\n\n";
        report << "- Primary evidence: A/B/C paired ratios captured inside one Pattern/K block.\n";
        report << "- Candidate order: all six permutations, position-balanced.\n";
        report << "- Resource construction, upload, CPU validation and readback are outside Candidate timestamps.\n";
        report << "- Absolute nanoseconds are descriptive only and are never compared across Pattern/K blocks.\n";
        report << "- Fixed control authority: reject only material drift inside one paired block.\n";
        report << "- NVML/P-state/clock telemetry: diagnostic only; no vendor state is an acceptance condition.\n";
        report << "- Accepted blocks: " << analysis.acceptedBlockCount << "\n";
        report << "- Rejected attempts retained as evidence: " << analysis.rejectedBlockCount << "\n";
        report << "- RuntimePolicyAuthorization: None\n";
        report << "- NextCapabilitySelection: DeferredByOwner\n";
        report << "- SelectionStatus: OWNER_DECISION_REQUIRED\n\n";

        report << "## Relative map\n\n";
        report << "| Pattern | K | Active blocks | Winners | A/B ratio | A/B result | B/C ratio | B/C result | A norm | B norm | C norm | Pairs |\n";
        report << "|---|---:|---:|---|---:|---|---:|---|---:|---:|---:|---:|\n";
        report << std::setprecision(9);
        for (const auto& value : analysis.cases)
        {
            report << '|' << PatternNameV2(value.pattern) << '|' << value.activeCount << '|'
                << value.activeBlockCount << '|' << WinnerText(value.globalWinnerSet) << '|'
                << value.denseVersusCompact.medianLeftOverRight << '|'
                << PairWinnerName(value.denseVersusCompact.winner) << '|'
                << value.compactVersusActiveBlock.medianLeftOverRight << '|'
                << PairWinnerName(value.compactVersusActiveBlock.winner) << '|'
                << value.medianControlNormalizedTime[0] << '|'
                << value.medianControlNormalizedTime[1] << '|'
                << value.medianControlNormalizedTime[2] << '|'
                << value.pairedObservationCount << "|\n";
        }

        report << "\n## Descriptive absolute timings\n\n";
        report << "These values help inspect one adapter capture but do not participate in cross-block decisions.\n\n";
        report << "| Pattern | K | A ns/iter | B ns/iter | C ns/iter |\n";
        report << "|---|---:|---:|---:|---:|\n";
        for (const auto& value : analysis.cases)
        {
            report << '|' << PatternNameV2(value.pattern) << '|' << value.activeCount << '|'
                << value.medianAbsoluteNanosecondsPerIteration[0] << '|'
                << value.medianAbsoluteNanosecondsPerIteration[1] << '|'
                << value.medianAbsoluteNanosecondsPerIteration[2] << "|\n";
        }

        report << "\n## Pair agreement\n\n";
        for (const auto& value : analysis.cases)
        {
            report << "- " << PatternNameV2(value.pattern) << " K=" << value.activeCount
                << ": A/B left=" << value.denseVersusCompact.leftWinFraction
                << " right=" << value.denseVersusCompact.rightWinFraction
                << "; B/C left=" << value.compactVersusActiveBlock.leftWinFraction
                << " right=" << value.compactVersusActiveBlock.rightWinFraction << "\n";
        }

        report << "\n## Crossover classification\n\n";
        for (const auto& value : analysis.patternTransitions)
            report << "- " << PatternNameV2(value.pattern) << ": A/B="
                << TransitionName(value.denseCompactClassification) << ", B/C="
                << TransitionName(value.compactBlockClassification) << "\n";

        report << "\n## Pattern sensitivity\n\n";
        report << "- SameKWinnerDependsOnPattern: "
            << (analysis.sameCardinalityWinnerDependsOnPattern ? "true" : "false") << "\n";
        report << "- WinnerChangesAcrossCardinality: "
            << (analysis.winnerChangesAcrossCardinality ? "true" : "false") << "\n";

        report << "\n## Interpretation boundary\n\n";
        report << "CU6 maps observed relative advantages on one adapter. It does not prove a universal winner, does not require a crossover, and does not authorize Runtime adaptive selection.\n";
        WriteTextFile(outputDirectory / "SPIRAL6_RELATIVE_DECISION_MAP_V2.md", report.str());
        return base::Result<void, std::string>::Success();
    }
    catch (const std::exception& error)
    {
        return base::Result<void, std::string>::Failure(error.what());
    }
}

base::Result<void, std::string>
RunFormatScheduleAndStabilitySelfTestV2(const std::filesystem::path& outputDirectory)
{
    try
    {
        MeasurementConfigV2 config;
        ValidateConfig(config);
        const auto orders = CanonicalBalancedOrdersV2();
        Require(orders.size() == CanonicalOrderCountV2, "Six Candidate orders missing.");
        std::array<std::array<std::uint32_t, CanonicalCandidateCountV2>,
            CanonicalCandidateCountV2> positions{};
        for (const auto& order : orders)
        {
            std::set<std::uint32_t> unique(order.begin(), order.end());
            Require(unique.size() == CanonicalCandidateCountV2,
                "Candidate order is not a permutation.");
            for (std::size_t position = 0; position < order.size(); ++position)
                ++positions[order[position]][position];
        }
        for (const auto& candidate : positions)
            for (std::uint32_t count : candidate)
                Require(count == 2, "Candidate position balance failed.");

        for (std::uint32_t run = 0; run < 2; ++run)
        {
            const auto schedule = CanonicalBalancedCaseScheduleV2(run);
            Require(schedule.size() ==
                CanonicalPatternCountV2 * CanonicalMeasurementCardinalityCountV2,
                "Case schedule size mismatch.");
            std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint32_t> counts;
            for (const auto& [pattern, k] : schedule)
                ++counts[{static_cast<std::uint32_t>(pattern), k}];
            for (PatternKindV2 pattern : CanonicalPatternsV2())
                for (std::uint32_t k : CanonicalMeasurementCardinalitiesV2())
                    Require(counts[{static_cast<std::uint32_t>(pattern), k}] == 1,
                        "Case schedule does not cover each Pattern/K exactly once.");
        }

        MeasurementEvidenceV2 evidence;
        evidence.captureUnixNanoseconds = 1;
        evidence.config = config;
        evidence.adapter.description = "Synthetic Adapter";
        evidence.measurementProfileIdentity = BuildMeasurementProfileIdentityV2(config);
        evidence.caseScheduleIdentity = BuildCaseScheduleIdentityV2(config.runCount);
        evidence.candidateOrderIdentity = BuildCandidateOrderIdentityV2();
        const auto kinds = CanonicalCandidateKindsV2();
        for (std::size_t index = 0; index < kinds.size(); ++index)
        {
            evidence.candidates[index].kind = kinds[index];
            evidence.candidates[index].representationRole =
                static_cast<family_candidate::SparseFamilyRepresentationRoleV1>(index + 1);
            evidence.candidates[index].dispatchPolicy =
                static_cast<family_candidate::SparseFamilyDispatchPolicyV1>(index + 1);
        }

        std::uint32_t caseOrder = 0;
        for (PatternKindV2 pattern : CanonicalPatternsV2())
        {
            for (std::uint32_t k : CanonicalMeasurementCardinalitiesV2())
            {
                for (std::uint32_t run = 0; run < config.runCount; ++run)
                {
                    BlockAttemptV2 rejected;
                    rejected.run = run;
                    rejected.caseOrder = caseOrder;
                    rejected.attempt = 0;
                    rejected.pattern = pattern;
                    rejected.activeCount = k;
                    rejected.rejectionReason = "SyntheticControlDrift";
                    rejected.controlRatio = 2.0;
                    evidence.attempts.push_back(rejected);

                    BlockAttemptV2 accepted;
                    accepted.run = run;
                    accepted.caseOrder = caseOrder;
                    accepted.attempt = 1;
                    accepted.pattern = pattern;
                    accepted.activeCount = k;
                    accepted.activeBlockCount = 1;
                    accepted.accepted = true;
                    const double regimeScale =
                        1.0 + 0.75 * static_cast<double>((caseOrder + run) % 4);
                    accepted.controlBeforeNanosecondsPerIteration = 100.0 * regimeScale;
                    accepted.controlAfterNanosecondsPerIteration = 101.0 * regimeScale;
                    accepted.controlReferenceNanosecondsPerIteration =
                        std::sqrt(accepted.controlBeforeNanosecondsPerIteration *
                            accepted.controlAfterNanosecondsPerIteration);
                    accepted.controlRatio =
                        accepted.controlAfterNanosecondsPerIteration /
                        accepted.controlBeforeNanosecondsPerIteration;

                    for (std::uint32_t orderIndex = 0;
                        orderIndex < orders.size(); ++orderIndex)
                    {
                        for (std::uint32_t cycle = 0;
                            cycle < config.measurementCyclesPerOrder; ++cycle)
                        {
                            for (std::uint32_t position = 0;
                                position < CanonicalCandidateCountV2; ++position)
                            {
                                TimingSampleV2 sample;
                                sample.run = run;
                                sample.caseOrder = caseOrder;
                                sample.acceptedAttempt = 1;
                                sample.orderIndex = orderIndex;
                                sample.cycle = cycle;
                                sample.orderPosition = position;
                                sample.candidate = kinds[orders[orderIndex][position]];
                                sample.pattern = pattern;
                                sample.activeCount = k;
                                sample.activeBlockCount = 1;
                                sample.iterationCount = config.iterationsPerBatch;
                                sample.blockControlNanosecondsPerIteration =
                                    accepted.controlReferenceNanosecondsPerIteration;

                                // B is deliberately fastest. Absolute time changes by up to
                                // 3.25x across blocks, but paired ratios remain invariant.
                                const double normalized =
                                    sample.candidate == kinds[0] ? 1.30 :
                                    (sample.candidate == kinds[1] ? 1.00 : 1.15);
                                sample.controlNormalizedTime = normalized;
                                sample.gpuNanosecondsPerIteration =
                                    normalized * sample.blockControlNanosecondsPerIteration;
                                sample.gpuBatchNanoseconds =
                                    sample.gpuNanosecondsPerIteration *
                                    sample.iterationCount;
                                sample.activeWriteCount = k;
                                accepted.samples.push_back(sample);
                            }
                        }
                    }
                    evidence.attempts.push_back(std::move(accepted));
                }
                ++caseOrder;
            }
        }

        const auto bytes = SerializeMeasurementEvidenceV2(evidence);
        std::filesystem::create_directories(outputDirectory);
        const auto path = outputDirectory / "self_test_relative_evidence_v2.bin";
        auto write = base::WriteAllBytes(path, bytes);
        if (!write) throw std::runtime_error(write.Error());
        auto roundTrip = ReadMeasurementEvidenceV2(path);
        if (!roundTrip) throw std::runtime_error(roundTrip.Error());

        auto corrupted = bytes;
        corrupted[corrupted.size() / 2] ^= std::byte{0x01};
        const auto corruptPath = outputDirectory / "self_test_relative_corrupt_v2.bin";
        write = base::WriteAllBytes(corruptPath, corrupted);
        if (!write) throw std::runtime_error(write.Error());
        Require(!ReadMeasurementEvidenceV2(corruptPath),
            "Corrupted evidence was accepted.");

        auto analysis = AnalyzeDecisionEvidenceV2(roundTrip.Value());
        if (!analysis) throw std::runtime_error(analysis.Error());
        Require(analysis.Value().acceptedBlockCount == 88,
            "Synthetic accepted block count mismatch.");
        Require(analysis.Value().rejectedBlockCount == 88,
            "Synthetic rejected block count mismatch.");
        for (const auto& value : analysis.Value().cases)
        {
            Require(value.globalWinnerSet.size() == 1 &&
                value.globalWinnerSet.front() == kinds[1],
                "Block-local scale invariance failed to preserve the synthetic B winner.");
            Require(value.denseVersusCompact.winner == PairWinnerV2::Right,
                "Synthetic A/B paired result mismatch.");
            Require(value.compactVersusActiveBlock.winner == PairWinnerV2::Left,
                "Synthetic B/C paired result mismatch.");
            Require(std::abs(value.denseVersusCompact.medianLeftOverRight - 1.30) < 1.0e-12,
                "Synthetic A/B ratio changed across absolute regimes.");
        }
        auto report = WriteDecisionEvidenceReportV2(roundTrip.Value(), outputDirectory);
        if (!report) throw std::runtime_error(report.Error());
        auto reportBytes = base::ReadAllBytes(
            outputDirectory / "SPIRAL6_RELATIVE_DECISION_MAP_V2.md");
        if (!reportBytes) throw std::runtime_error(reportBytes.Error());
        const std::string reportText(
            reinterpret_cast<const char*>(reportBytes.Value().data()),
            reportBytes.Value().size());
        Require(reportText.find("Absolute nanoseconds are descriptive only") != std::string::npos,
            "Relative report absolute-time boundary is missing.");
        Require(reportText.find("Primary evidence: A/B/C paired ratios") != std::string::npos,
            "Relative report paired-evidence boundary is missing.");
        std::filesystem::remove(path);
        std::filesystem::remove(corruptPath);
        std::filesystem::remove(outputDirectory / "SPIRAL6_RELATIVE_DECISION_MAP_V2.md");
        return base::Result<void, std::string>::Success();
    }
    catch (const std::exception& error)
    {
        return base::Result<void, std::string>::Failure(error.what());
    }
}

}
