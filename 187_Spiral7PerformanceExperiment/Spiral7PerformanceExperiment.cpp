#include "Spiral7PerformanceExperiment.h"

#include "../00_Foundation/BinaryIO.h"
#include "../180_SparseTemporalDeltaCandidateFamilyVerifier/DeltaCandidateFamilyVerifier.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <utility>

namespace sge4_5::spiral7::performance
{
namespace
{
constexpr std::uint32_t EvidenceMagic = 0x324D3753u; // "S7M2"
constexpr std::string_view ProfileDomain = "SGE4-5.Spiral7.RelativePerformanceProfile.V2";
constexpr std::string_view ScheduleDomain = "SGE4-5.Spiral7.BalancedMeasurementCaseSchedule.V1";
constexpr std::string_view OrderDomain = "SGE4-5.Spiral7.BalancedCandidateOrders.V1";
constexpr std::string_view ContextDomain = "SGE4-5.Spiral7.DeltaFamilyVerificationContext.V1";

constexpr std::string_view AcceptedArchitectureDigest =
    "1f1d09b4e52dbc35e961e0d3751b32292b2c30eedcfa473800c5bb8e8cb5ab73";
constexpr std::string_view AcceptedControlledDigest =
    "7f0247b193f9bc20f4eda5feed590c1da5722ef36b133640292b1ed616cff62b";
constexpr std::string_view AcceptedFreshDigest =
    "091eaec0d27287fe897f6813043fd75c090d5da17054461df314f99b2a6f5a92";

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

void WriteDigest(base::BinaryWriter& writer, const base::Digest256& value)
{
    writer.WriteBytes(value);
}

base::Digest256 ReadDigest(base::BinaryReader& reader)
{
    auto bytes = reader.ReadBytes(32);
    if (!bytes) throw std::runtime_error(bytes.Error());
    base::Digest256 value{};
    std::memcpy(value.data(), bytes.Value().data(), value.size());
    return value;
}

std::uint8_t HexDigit(char value)
{
    if (value >= '0' && value <= '9') return static_cast<std::uint8_t>(value - '0');
    if (value >= 'a' && value <= 'f') return static_cast<std::uint8_t>(10 + value - 'a');
    if (value >= 'A' && value <= 'F') return static_cast<std::uint8_t>(10 + value - 'A');
    throw std::runtime_error("Invalid hexadecimal digest digit.");
}

base::Digest256 DigestFromHex(std::string_view text)
{
    Require(text.size() == 64, "SHA-256 hexadecimal text must contain 64 digits.");
    base::Digest256 digest{};
    for (std::size_t index = 0; index < digest.size(); ++index)
    {
        const auto high = HexDigit(text[index * 2]);
        const auto low = HexDigit(text[index * 2 + 1]);
        digest[index] = static_cast<std::byte>((high << 4) | low);
    }
    return digest;
}

base::Digest256 LabelDigest(std::string_view label)
{
    return base::Sha256(std::as_bytes(std::span(label.data(), label.size())));
}

CandidateKindV1 CandidateFromU32(std::uint32_t value)
{
    switch (value)
    {
    case 1: return CandidateKindV1::FullActiveDenseRecompute;
    case 2: return CandidateKindV1::CompactDeltaIndexHistoryReuse;
    case 3: return CandidateKindV1::AffectedBlockDeltaHistoryReuse;
    default: throw std::runtime_error("Unknown Candidate kind in evidence.");
    }
}

PatternKindV1 PatternFromU32(std::uint32_t value)
{
    switch (value)
    {
    case 1: return PatternKindV1::PrefixControl;
    case 2: return PatternKindV1::UniformStride;
    case 3: return PatternKindV1::BlockClusteredPermutation;
    case 4: return PatternKindV1::HashScatterPermutation;
    default: throw std::runtime_error("Unknown Pattern kind in evidence.");
    }
}

TransitionShapeV1 ShapeFromU32(std::uint32_t value)
{
    switch (value)
    {
    case 1: return TransitionShapeV1::Hold;
    case 2: return TransitionShapeV1::DirtyOnly;
    case 3: return TransitionShapeV1::ReplaceAndClear;
    default: throw std::runtime_error("Unknown transition shape in evidence.");
    }
}

family_candidate::DeltaFamilyRepresentationRoleV1 RoleFromU32(std::uint32_t value)
{
    switch (value)
    {
    case 1: return family_candidate::DeltaFamilyRepresentationRoleV1::DenseActiveMask4096;
    case 2: return family_candidate::DeltaFamilyRepresentationRoleV1::CanonicalSortedIndexActionList;
    case 3: return family_candidate::DeltaFamilyRepresentationRoleV1::AffectedBlockUpdateClearMasks;
    default: throw std::runtime_error("Unknown representation role in evidence.");
    }
}

family_candidate::DeltaFamilyDispatchPolicyV1 DispatchFromU32(std::uint32_t value)
{
    switch (value)
    {
    case 1: return family_candidate::DeltaFamilyDispatchPolicyV1::FixedUniverseGroups;
    case 2: return family_candidate::DeltaFamilyDispatchPolicyV1::CeilTransitionGroups;
    case 3: return family_candidate::DeltaFamilyDispatchPolicyV1::OneGroupPerAffectedBlock;
    default: throw std::runtime_error("Unknown dispatch policy in evidence.");
    }
}

[[maybe_unused]] PairWinnerV1 PairWinnerFromU32(std::uint32_t value)
{
    switch (value)
    {
    case 1: return PairWinnerV1::Left;
    case 2: return PairWinnerV1::Right;
    case 3: return PairWinnerV1::NoiseEquivalent;
    case 4: return PairWinnerV1::Unresolved;
    default: throw std::runtime_error("Unknown pair winner in evidence.");
    }
}

void ValidateConfig(const MeasurementConfigV1& config)
{
    Require(config.runCount > 0 && config.runCount <= 8, "runCount outside qualified range.");
    Require(config.measurementCyclesPerOrder > 0 && config.measurementCyclesPerOrder <= 8,
            "measurementCyclesPerOrder outside qualified range.");
    Require(config.iterationsPerBatch >= 16 && config.iterationsPerBatch <= 65536,
            "iterationsPerBatch outside qualified range.");
    Require(config.warmupIterations > 0 && config.warmupIterations <= 262144,
            "warmupIterations outside qualified range.");
    Require(config.maximumBlockAttempts > 0 && config.maximumBlockAttempts <= 8,
            "maximumBlockAttempts outside qualified range.");
    Require(config.maximumControlRatio >= 1.0 && config.maximumControlRatio <= 2.0,
            "maximumControlRatio invalid.");
    Require(config.relativeNoiseFloor >= 0.0 && config.relativeNoiseFloor <= 0.25,
            "relativeNoiseFloor invalid.");
    Require(config.minimumPairAgreement >= 0.5 && config.minimumPairAgreement <= 1.0,
            "minimumPairAgreement invalid.");
}

void WriteConfig(base::BinaryWriter& writer, const MeasurementConfigV1& value)
{
    writer.WriteU32(value.runCount);
    writer.WriteU32(value.measurementCyclesPerOrder);
    writer.WriteU32(value.iterationsPerBatch);
    writer.WriteU32(value.warmupIterations);
    writer.WriteU32(value.maximumBlockAttempts);
    writer.WriteU32(value.adapterIndex);
    WriteBool(writer, value.requestStablePowerStateDiagnostic);
    WriteDouble(writer, value.maximumControlRatio);
    WriteDouble(writer, value.relativeNoiseFloor);
    WriteDouble(writer, value.minimumPairAgreement);
}

MeasurementConfigV1 ReadConfig(base::BinaryReader& reader)
{
    MeasurementConfigV1 value;
    value.runCount = ReadU32(reader);
    value.measurementCyclesPerOrder = ReadU32(reader);
    value.iterationsPerBatch = ReadU32(reader);
    value.warmupIterations = ReadU32(reader);
    value.maximumBlockAttempts = ReadU32(reader);
    value.adapterIndex = ReadU32(reader);
    value.requestStablePowerStateDiagnostic = ReadBool(reader);
    value.maximumControlRatio = ReadDouble(reader);
    value.relativeNoiseFloor = ReadDouble(reader);
    value.minimumPairAgreement = ReadDouble(reader);
    ValidateConfig(value);
    return value;
}

void WriteAdapter(base::BinaryWriter& writer, const AdapterProfileV1& value)
{
    WriteString(writer, value.description);
    writer.WriteU32(value.vendorId); writer.WriteU32(value.deviceId);
    writer.WriteU32(value.subsystemId); writer.WriteU32(value.revision);
    writer.WriteU32(value.luidLow); writer.WriteI32(value.luidHigh);
    writer.WriteU64(value.driverVersion); writer.WriteU64(value.timestampFrequency);
    WriteBool(writer, value.uma); WriteBool(writer, value.cacheCoherentUma);
    WriteBool(writer, value.stablePowerStateApplied);
    WriteDigest(writer, value.fingerprint);
}

AdapterProfileV1 ReadAdapter(base::BinaryReader& reader)
{
    AdapterProfileV1 value;
    value.description = ReadString(reader);
    value.vendorId = ReadU32(reader); value.deviceId = ReadU32(reader);
    value.subsystemId = ReadU32(reader); value.revision = ReadU32(reader);
    value.luidLow = ReadU32(reader); value.luidHigh = ReadI32(reader);
    value.driverVersion = ReadU64(reader); value.timestampFrequency = ReadU64(reader);
    value.uma = ReadBool(reader); value.cacheCoherentUma = ReadBool(reader);
    value.stablePowerStateApplied = ReadBool(reader);
    value.fingerprint = ReadDigest(reader);
    return value;
}

void WriteCandidateProfile(base::BinaryWriter& writer, const CandidateProfileV1& value)
{
    writer.WriteU32(static_cast<std::uint32_t>(value.kind));
    writer.WriteU32(static_cast<std::uint32_t>(value.representationRole));
    writer.WriteU32(static_cast<std::uint32_t>(value.dispatchPolicy));
    writer.WriteU32(value.payloadStrideBytes);
    writer.WriteU32(value.fixedCapacityBytes);
    WriteDigest(writer, value.targetProfileIdentity);
    WriteDigest(writer, value.observationContractIdentity);
    WriteDigest(writer, value.completionContractIdentity);
    WriteDigest(writer, value.consumerProgramIdentity);
    WriteDigest(writer, value.representationResourceContractIdentity);
    WriteDigest(writer, value.validationShaderBytecodeDigest);
    WriteDigest(writer, value.timingShaderBytecodeDigest);
}

CandidateProfileV1 ReadCandidateProfile(base::BinaryReader& reader)
{
    CandidateProfileV1 value;
    value.kind = CandidateFromU32(ReadU32(reader));
    value.representationRole = RoleFromU32(ReadU32(reader));
    value.dispatchPolicy = DispatchFromU32(ReadU32(reader));
    value.payloadStrideBytes = ReadU32(reader);
    value.fixedCapacityBytes = ReadU32(reader);
    value.targetProfileIdentity = ReadDigest(reader);
    value.observationContractIdentity = ReadDigest(reader);
    value.completionContractIdentity = ReadDigest(reader);
    value.consumerProgramIdentity = ReadDigest(reader);
    value.representationResourceContractIdentity = ReadDigest(reader);
    value.validationShaderBytecodeDigest = ReadDigest(reader);
    value.timingShaderBytecodeDigest = ReadDigest(reader);
    return value;
}

void WriteAuthority(base::BinaryWriter& writer, const CandidateCaseAuthorityV1& value)
{
    writer.WriteU32(static_cast<std::uint32_t>(value.kind));
    WriteDigest(writer, value.rawCandidateIdentity);
    WriteDigest(writer, value.verifiedPlanIdentity);
    WriteDigest(writer, value.verifierCertificateIdentity);
    WriteDigest(writer, value.artifactIdentity);
    WriteDigest(writer, value.representationResourceIdentity);
    WriteDigest(writer, value.outputDigest);
}

CandidateCaseAuthorityV1 ReadAuthority(base::BinaryReader& reader)
{
    CandidateCaseAuthorityV1 value;
    value.kind = CandidateFromU32(ReadU32(reader));
    value.rawCandidateIdentity = ReadDigest(reader);
    value.verifiedPlanIdentity = ReadDigest(reader);
    value.verifierCertificateIdentity = ReadDigest(reader);
    value.artifactIdentity = ReadDigest(reader);
    value.representationResourceIdentity = ReadDigest(reader);
    value.outputDigest = ReadDigest(reader);
    return value;
}

void WriteKey(base::BinaryWriter& writer, const MeasurementCaseKeyV1& value)
{
    writer.WriteU32(static_cast<std::uint32_t>(value.pattern));
    writer.WriteU32(value.activeCount);
    writer.WriteU32(value.transitionCount);
    writer.WriteU32(value.updateCount);
    writer.WriteU32(value.clearCount);
    writer.WriteU32(value.affectedBlockCount);
    writer.WriteU32(static_cast<std::uint32_t>(value.transitionShape));
    WriteDigest(writer, value.previousActiveSetIdentity);
    WriteDigest(writer, value.activeSetIdentity);
    WriteDigest(writer, value.modifiedSetIdentity);
    WriteDigest(writer, value.transitionSetIdentity);
    WriteDigest(writer, value.invocationIdentity);
}

MeasurementCaseKeyV1 ReadKey(base::BinaryReader& reader)
{
    MeasurementCaseKeyV1 value;
    value.pattern = PatternFromU32(ReadU32(reader));
    value.activeCount = ReadU32(reader);
    value.transitionCount = ReadU32(reader);
    value.updateCount = ReadU32(reader);
    value.clearCount = ReadU32(reader);
    value.affectedBlockCount = ReadU32(reader);
    value.transitionShape = ShapeFromU32(ReadU32(reader));
    value.previousActiveSetIdentity = ReadDigest(reader);
    value.activeSetIdentity = ReadDigest(reader);
    value.modifiedSetIdentity = ReadDigest(reader);
    value.transitionSetIdentity = ReadDigest(reader);
    value.invocationIdentity = ReadDigest(reader);
    return value;
}

void WriteSample(base::BinaryWriter& writer, const TimingSampleV1& value)
{
    writer.WriteU32(value.run); writer.WriteU32(value.caseOrder);
    writer.WriteU32(value.acceptedAttempt); writer.WriteU32(value.orderIndex);
    writer.WriteU32(value.cycle); writer.WriteU32(value.orderPosition);
    writer.WriteU32(static_cast<std::uint32_t>(value.candidate));
    writer.WriteU64(value.timestampTickCount);
    writer.WriteU32(value.effectiveIterations);
    WriteBool(writer, value.timestampResolutionCensored);
    WriteDouble(writer, value.gpuBatchNanoseconds);
    WriteDouble(writer, value.gpuNanosecondsPerIteration);
    WriteDouble(writer, value.blockControlNanosecondsPerIteration);
    WriteDouble(writer, value.controlNormalizedTime);
    writer.WriteU32(value.expectedDispatchGroupsX);
}

TimingSampleV1 ReadSample(base::BinaryReader& reader)
{
    TimingSampleV1 value;
    value.run = ReadU32(reader); value.caseOrder = ReadU32(reader);
    value.acceptedAttempt = ReadU32(reader); value.orderIndex = ReadU32(reader);
    value.cycle = ReadU32(reader); value.orderPosition = ReadU32(reader);
    value.candidate = CandidateFromU32(ReadU32(reader));
    value.timestampTickCount = ReadU64(reader);
    value.effectiveIterations = ReadU32(reader);
    value.timestampResolutionCensored = ReadBool(reader);
    value.gpuBatchNanoseconds = ReadDouble(reader);
    value.gpuNanosecondsPerIteration = ReadDouble(reader);
    value.blockControlNanosecondsPerIteration = ReadDouble(reader);
    value.controlNormalizedTime = ReadDouble(reader);
    value.expectedDispatchGroupsX = ReadU32(reader);
    return value;
}

void WriteAttempt(base::BinaryWriter& writer, const BlockAttemptV1& value)
{
    writer.WriteU32(value.run); writer.WriteU32(value.caseOrder); writer.WriteU32(value.attempt);
    WriteKey(writer, value.key);
    for (const auto& authority : value.authorities) WriteAuthority(writer, authority);
    WriteBool(writer, value.accepted);
    WriteString(writer, value.rejectionReason);
    WriteDouble(writer, value.controlBeforeNanosecondsPerIteration);
    WriteDouble(writer, value.controlAfterNanosecondsPerIteration);
    WriteDouble(writer, value.controlReferenceNanosecondsPerIteration);
    WriteDouble(writer, value.controlRatio);
    writer.WriteU32(static_cast<std::uint32_t>(value.samples.size()));
    for (const auto& sample : value.samples) WriteSample(writer, sample);
}

BlockAttemptV1 ReadAttempt(base::BinaryReader& reader)
{
    BlockAttemptV1 value;
    value.run = ReadU32(reader); value.caseOrder = ReadU32(reader); value.attempt = ReadU32(reader);
    value.key = ReadKey(reader);
    for (auto& authority : value.authorities) authority = ReadAuthority(reader);
    value.accepted = ReadBool(reader);
    value.rejectionReason = ReadString(reader);
    value.controlBeforeNanosecondsPerIteration = ReadDouble(reader);
    value.controlAfterNanosecondsPerIteration = ReadDouble(reader);
    value.controlReferenceNanosecondsPerIteration = ReadDouble(reader);
    value.controlRatio = ReadDouble(reader);
    const auto count = ReadU32(reader);
    Require(count <= 4096, "Attempt sample count exceeds limit.");
    value.samples.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index) value.samples.push_back(ReadSample(reader));
    return value;
}

double Median(std::vector<double> values)
{
    Require(!values.empty(), "Median requires at least one value.");
    std::sort(values.begin(), values.end());
    const auto middle = values.size() / 2;
    if ((values.size() & 1u) != 0u) return values[middle];
    return (values[middle - 1] + values[middle]) * 0.5;
}

std::size_t CandidateIndex(CandidateKindV1 value)
{
    switch (value)
    {
    case CandidateKindV1::FullActiveDenseRecompute: return 0;
    case CandidateKindV1::CompactDeltaIndexHistoryReuse: return 1;
    case CandidateKindV1::AffectedBlockDeltaHistoryReuse: return 2;
    }
    throw std::runtime_error("Unknown Candidate kind.");
}

std::uint32_t ExpectedDispatchGroups(
    CandidateKindV1 candidate,
    const MeasurementCaseKeyV1& key)
{
    switch (candidate)
    {
    case CandidateKindV1::FullActiveDenseRecompute:
        return 64u;
    case CandidateKindV1::CompactDeltaIndexHistoryReuse:
        return key.transitionCount == 0 ? 0u : (key.transitionCount + 63u) / 64u;
    case CandidateKindV1::AffectedBlockDeltaHistoryReuse:
        return key.affectedBlockCount;
    }
    throw std::runtime_error("Unknown Candidate kind.");
}

double TimestampTicksToNanoseconds(
    std::uint64_t ticks,
    std::uint64_t timestampFrequency)
{
    Require(timestampFrequency > 0, "Timestamp frequency is zero.");
    return static_cast<double>(ticks) * 1.0e9 /
           static_cast<double>(timestampFrequency);
}

bool IsPowerOfTwo(std::uint32_t value)
{
    return value != 0 && (value & (value - 1u)) == 0;
}

bool WinnerSetsEqual(const std::vector<CandidateKindV1>& a, const std::vector<CandidateKindV1>& b)
{
    return a == b;
}

std::string WinnerSetText(const std::vector<CandidateKindV1>& values)
{
    if (values.empty()) return "Unresolved";
    std::ostringstream stream;
    for (std::size_t index = 0; index < values.size(); ++index)
    {
        if (index != 0) stream << '/';
        stream << CandidateLetterV1(values[index]);
    }
    return stream.str();
}

using PairKey = std::tuple<std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t>;

PairDecisionV1 BuildPairDecision(
    const std::vector<const TimingSampleV1*>& samples,
    CandidateKindV1 left,
    CandidateKindV1 right,
    const MeasurementConfigV1& config)
{
    std::map<PairKey, std::array<double, CanonicalCandidateCountV1>> values;
    std::map<PairKey, std::array<bool, CanonicalCandidateCountV1>> present;
    for (const auto* sample : samples)
    {
        const PairKey key{sample->run, sample->caseOrder, sample->acceptedAttempt,
                          sample->orderIndex, sample->cycle};
        const auto index = CandidateIndex(sample->candidate);
        values[key][index] = sample->gpuNanosecondsPerIteration;
        present[key][index] = true;
    }

    PairDecisionV1 result;
    result.left = left;
    result.right = right;
    const auto leftIndex = CandidateIndex(left);
    const auto rightIndex = CandidateIndex(right);
    std::vector<double> ratios;
    std::uint32_t leftWins = 0;
    std::uint32_t rightWins = 0;
    for (const auto& [key, row] : values)
    {
        const auto found = present.find(key);
        if (found == present.end() || !found->second[leftIndex] || !found->second[rightIndex]) continue;
        const double ratio = row[leftIndex] / row[rightIndex];
        ratios.push_back(ratio);
        if (ratio < 1.0 - config.relativeNoiseFloor) ++leftWins;
        else if (ratio > 1.0 + config.relativeNoiseFloor) ++rightWins;
    }
    result.pairedObservationCount = static_cast<std::uint32_t>(ratios.size());
    if (ratios.empty()) return result;
    result.medianLeftOverRight = Median(ratios);
    result.leftWinFraction = static_cast<double>(leftWins) / static_cast<double>(ratios.size());
    result.rightWinFraction = static_cast<double>(rightWins) / static_cast<double>(ratios.size());
    if (result.leftWinFraction >= config.minimumPairAgreement) result.winner = PairWinnerV1::Left;
    else if (result.rightWinFraction >= config.minimumPairAgreement) result.winner = PairWinnerV1::Right;
    else if (std::abs(result.medianLeftOverRight - 1.0) <= config.relativeNoiseFloor)
        result.winner = PairWinnerV1::NoiseEquivalent;
    else result.winner = PairWinnerV1::Unresolved;
    return result;
}

std::tuple<std::uint32_t, std::uint32_t, std::uint32_t> CaseTuple(const MeasurementCaseKeyV1& key)
{
    return {static_cast<std::uint32_t>(key.pattern), key.activeCount, key.transitionCount};
}

std::uint32_t SyntheticAffectedBlockCount(PatternKindV1 pattern, std::uint32_t transitionCount)
{
    if (transitionCount == 0) return 0;
    switch (pattern)
    {
    case PatternKindV1::PrefixControl:
    case PatternKindV1::BlockClusteredPermutation:
        return std::min(64u, (transitionCount + 63u) / 64u);
    case PatternKindV1::UniformStride:
    case PatternKindV1::HashScatterPermutation:
        return std::min(64u, transitionCount);
    }
    return 64;
}

MeasurementCaseKeyV1 SyntheticKey(PatternKindV1 pattern, std::uint32_t active, std::uint32_t transition)
{
    MeasurementCaseKeyV1 key;
    key.pattern = pattern;
    key.activeCount = active;
    key.transitionCount = transition;
    key.updateCount = transition <= active ? transition : active;
    key.clearCount = transition - key.updateCount;
    key.affectedBlockCount = SyntheticAffectedBlockCount(pattern, transition);
    key.transitionShape = transition == 0 ? TransitionShapeV1::Hold
        : (transition <= active ? TransitionShapeV1::DirtyOnly : TransitionShapeV1::ReplaceAndClear);
    const auto label = PatternNameV1(pattern) + ":" + std::to_string(active) + ":" + std::to_string(transition);
    key.previousActiveSetIdentity = LabelDigest("previous:" + label);
    key.activeSetIdentity = LabelDigest("active:" + label);
    key.modifiedSetIdentity = LabelDigest("modified:" + label);
    key.transitionSetIdentity = LabelDigest("transition:" + label);
    key.invocationIdentity = LabelDigest("invocation:" + label);
    return key;
}

std::array<double, CanonicalCandidateCountV1> SyntheticTimes(const MeasurementCaseKeyV1& key)
{
    const double patternFactor = key.pattern == PatternKindV1::BlockClusteredPermutation ? 0.72
        : (key.pattern == PatternKindV1::PrefixControl ? 0.88
        : (key.pattern == PatternKindV1::UniformStride ? 1.08 : 1.18));
    const double full = 100.0 + static_cast<double>(key.activeCount) * 0.002;
    const double compact = 7.0 + static_cast<double>(key.transitionCount) * 0.036;
    const double block = 9.0 + static_cast<double>(key.affectedBlockCount) * 1.30 * patternFactor;
    return {full, compact, block};
}

void AppendDigestText(std::ostringstream& stream, const base::Digest256& digest)
{
    stream << base::ToHex(digest);
}
} // namespace

std::string CandidateNameV1(CandidateKindV1 value)
{
    switch (value)
    {
    case CandidateKindV1::FullActiveDenseRecompute: return "FullActiveDenseRecompute";
    case CandidateKindV1::CompactDeltaIndexHistoryReuse: return "CompactDeltaIndexHistoryReuse";
    case CandidateKindV1::AffectedBlockDeltaHistoryReuse: return "AffectedBlockDeltaHistoryReuse";
    }
    return "Unknown";
}

char CandidateLetterV1(CandidateKindV1 value)
{
    switch (value)
    {
    case CandidateKindV1::FullActiveDenseRecompute: return 'A';
    case CandidateKindV1::CompactDeltaIndexHistoryReuse: return 'B';
    case CandidateKindV1::AffectedBlockDeltaHistoryReuse: return 'C';
    }
    return '?';
}

std::string PatternNameV1(PatternKindV1 value)
{
    switch (value)
    {
    case PatternKindV1::PrefixControl: return "PrefixControl";
    case PatternKindV1::UniformStride: return "UniformStride";
    case PatternKindV1::BlockClusteredPermutation: return "BlockClusteredPermutation";
    case PatternKindV1::HashScatterPermutation: return "HashScatterPermutation";
    }
    return "Unknown";
}

std::string TransitionShapeNameV1(TransitionShapeV1 value)
{
    switch (value)
    {
    case TransitionShapeV1::Hold: return "Hold";
    case TransitionShapeV1::DirtyOnly: return "DirtyOnly";
    case TransitionShapeV1::ReplaceAndClear: return "ReplaceAndClear";
    }
    return "Unknown";
}

std::array<CandidateKindV1, CanonicalCandidateCountV1> CanonicalCandidateKindsV1()
{
    return {CandidateKindV1::FullActiveDenseRecompute,
            CandidateKindV1::CompactDeltaIndexHistoryReuse,
            CandidateKindV1::AffectedBlockDeltaHistoryReuse};
}

std::array<PatternKindV1, CanonicalPatternCountV1> CanonicalPatternsV1()
{
    return {PatternKindV1::PrefixControl,
            PatternKindV1::UniformStride,
            PatternKindV1::BlockClusteredPermutation,
            PatternKindV1::HashScatterPermutation};
}

const std::array<std::uint32_t, CanonicalActiveCountCountV1>&
CanonicalMeasurementActiveCountsV1() noexcept
{
    static constexpr std::array<std::uint32_t, CanonicalActiveCountCountV1> values{
        64u, 256u, 1024u, 2048u, 4096u};
    return values;
}

const std::array<std::uint32_t, CanonicalTransitionCountCountV1>&
CanonicalMeasurementTransitionCountsV1() noexcept
{
    static constexpr std::array<std::uint32_t, CanonicalTransitionCountCountV1> values{
        0u, 1u, 8u, 32u, 64u, 256u, 1024u, 4096u};
    return values;
}

std::vector<std::array<std::uint32_t, CanonicalCandidateCountV1>> CanonicalBalancedOrdersV1()
{
    return {{0u,1u,2u}, {0u,2u,1u}, {1u,0u,2u},
            {1u,2u,0u}, {2u,0u,1u}, {2u,1u,0u}};
}

std::vector<std::array<std::uint32_t, 3>> CanonicalCaseScheduleV1(std::uint32_t run)
{
    std::vector<std::array<std::uint32_t, 3>> cases;
    cases.reserve(CanonicalCaseCountV1);
    for (const auto pattern : CanonicalPatternsV1())
        for (const auto active : CanonicalMeasurementActiveCountsV1())
            for (const auto transition : CanonicalMeasurementTransitionCountsV1())
                cases.push_back({static_cast<std::uint32_t>(pattern), active, transition});

    if ((run & 1u) != 0u) std::reverse(cases.begin(), cases.end());
    if (!cases.empty())
    {
        const std::size_t rotation = static_cast<std::size_t>((run * 37u) % cases.size());
        std::rotate(cases.begin(), cases.begin() + static_cast<std::ptrdiff_t>(rotation), cases.end());
    }
    return cases;
}

base::Digest256 BuildMeasurementProfileIdentityV1(const MeasurementConfigV1& config)
{
    ValidateConfig(config);
    base::BinaryWriter writer;
    writer.WriteBytes(std::as_bytes(std::span(ProfileDomain.data(), ProfileDomain.size())));
    WriteConfig(writer, config);
    for (const auto value : CanonicalMeasurementActiveCountsV1()) writer.WriteU32(value);
    for (const auto value : CanonicalMeasurementTransitionCountsV1()) writer.WriteU32(value);
    return base::Sha256(writer.Bytes());
}

base::Digest256 BuildCaseScheduleIdentityV1(std::uint32_t runCount)
{
    base::BinaryWriter writer;
    writer.WriteBytes(std::as_bytes(std::span(ScheduleDomain.data(), ScheduleDomain.size())));
    writer.WriteU32(runCount);
    for (std::uint32_t run = 0; run < runCount; ++run)
    {
        const auto schedule = CanonicalCaseScheduleV1(run);
        writer.WriteU32(static_cast<std::uint32_t>(schedule.size()));
        for (const auto& value : schedule)
        {
            writer.WriteU32(value[0]); writer.WriteU32(value[1]); writer.WriteU32(value[2]);
        }
    }
    return base::Sha256(writer.Bytes());
}

base::Digest256 BuildCandidateOrderIdentityV1()
{
    base::BinaryWriter writer;
    writer.WriteBytes(std::as_bytes(std::span(OrderDomain.data(), OrderDomain.size())));
    const auto orders = CanonicalBalancedOrdersV1();
    writer.WriteU32(static_cast<std::uint32_t>(orders.size()));
    for (const auto& order : orders)
        for (const auto value : order) writer.WriteU32(value);
    return base::Sha256(writer.Bytes());
}

base::Digest256 VerificationContextIdentityV1()
{
    const auto context = family_verification::BuildCanonicalDeltaFamilyVerificationContextV1();
    base::BinaryWriter writer;
    writer.WriteBytes(std::as_bytes(std::span(ContextDomain.data(), ContextDomain.size())));
    writer.WriteBytes(context.targetProfileIdentity);
    writer.WriteBytes(context.observationContractIdentity);
    writer.WriteBytes(context.completionContractIdentity);
    writer.WriteBytes(context.deviceEpochPolicyIdentity);
    return base::Sha256(writer.Bytes());
}

std::vector<std::byte> SerializeMeasurementEvidenceV1(const MeasurementEvidenceV1& evidence)
{
    const auto valid = ValidateMeasurementEvidenceV1(evidence);
    if (!valid) throw std::invalid_argument(valid.Error());

    base::BinaryWriter writer;
    writer.WriteU32(EvidenceMagic);
    writer.WriteU32(evidence.schemaVersion);
    writer.WriteU64(evidence.captureUnixNanoseconds);
    WriteConfig(writer, evidence.config);
    WriteAdapter(writer, evidence.adapter);
    WriteDigest(writer, evidence.cu5ArchitectureEvidenceIdentity);
    WriteDigest(writer, evidence.cu5ControlledRecoveryEvidenceIdentity);
    WriteDigest(writer, evidence.cu5FreshRematerializationEvidenceIdentity);
    WriteDigest(writer, evidence.semanticIdentity);
    WriteDigest(writer, evidence.verificationContextIdentity);
    WriteDigest(writer, evidence.measurementProfileIdentity);
    WriteDigest(writer, evidence.caseScheduleIdentity);
    WriteDigest(writer, evidence.candidateOrderIdentity);
    for (const auto& candidate : evidence.candidates) WriteCandidateProfile(writer, candidate);
    writer.WriteU32(static_cast<std::uint32_t>(evidence.attempts.size()));
    for (const auto& attempt : evidence.attempts) WriteAttempt(writer, attempt);
    const auto digest = base::Sha256(writer.Bytes());
    writer.WriteBytes(digest);
    return std::move(writer).Take();
}

base::Result<MeasurementEvidenceV1, std::string>
DeserializeMeasurementEvidenceV1(const std::vector<std::byte>& bytes)
{
    try
    {
        Require(bytes.size() >= 40, "Measurement evidence is truncated.");
        const auto payloadSize = bytes.size() - 32;
        base::Digest256 stored{};
        std::memcpy(stored.data(), bytes.data() + payloadSize, stored.size());
        const auto actual = base::Sha256(std::span<const std::byte>(bytes.data(), payloadSize));
        Require(stored == actual, "Measurement evidence digest mismatch.");

        base::BinaryReader reader(std::span<const std::byte>(bytes.data(), payloadSize));
        Require(ReadU32(reader) == EvidenceMagic, "Measurement evidence magic mismatch.");
        MeasurementEvidenceV1 evidence;
        evidence.schemaVersion = ReadU32(reader);
        evidence.captureUnixNanoseconds = ReadU64(reader);
        evidence.config = ReadConfig(reader);
        evidence.adapter = ReadAdapter(reader);
        evidence.cu5ArchitectureEvidenceIdentity = ReadDigest(reader);
        evidence.cu5ControlledRecoveryEvidenceIdentity = ReadDigest(reader);
        evidence.cu5FreshRematerializationEvidenceIdentity = ReadDigest(reader);
        evidence.semanticIdentity = ReadDigest(reader);
        evidence.verificationContextIdentity = ReadDigest(reader);
        evidence.measurementProfileIdentity = ReadDigest(reader);
        evidence.caseScheduleIdentity = ReadDigest(reader);
        evidence.candidateOrderIdentity = ReadDigest(reader);
        for (auto& candidate : evidence.candidates) candidate = ReadCandidateProfile(reader);
        const auto attemptCount = ReadU32(reader);
        Require(attemptCount <= 10000, "Measurement attempt count exceeds limit.");
        evidence.attempts.reserve(attemptCount);
        for (std::uint32_t index = 0; index < attemptCount; ++index)
            evidence.attempts.push_back(ReadAttempt(reader));
        Require(reader.Remaining() == 0, "Measurement evidence contains trailing payload bytes.");
        const auto valid = ValidateMeasurementEvidenceV1(evidence);
        if (!valid) return base::Result<MeasurementEvidenceV1, std::string>::Failure(valid.Error());
        return base::Result<MeasurementEvidenceV1, std::string>::Success(std::move(evidence));
    }
    catch (const std::exception& error)
    {
        return base::Result<MeasurementEvidenceV1, std::string>::Failure(error.what());
    }
}

base::Result<void, std::string> ValidateMeasurementEvidenceV1(const MeasurementEvidenceV1& evidence)
{
    try
    {
        Require(evidence.schemaVersion == MeasurementEvidenceSchemaVersionV2,
                "Measurement evidence schema mismatch.");
        ValidateConfig(evidence.config);
        Require(!evidence.adapter.description.empty(), "Adapter description is empty.");
        Require(evidence.adapter.timestampFrequency > 0, "Adapter timestamp frequency is zero.");
        Require(evidence.adapter.fingerprint != base::Digest256{}, "Adapter fingerprint is empty.");
        Require(evidence.cu5ArchitectureEvidenceIdentity == DigestFromHex(AcceptedArchitectureDigest),
                "CU5 Architecture evidence binding mismatch.");
        Require(evidence.cu5ControlledRecoveryEvidenceIdentity == DigestFromHex(AcceptedControlledDigest),
                "CU5 Controlled Recovery evidence binding mismatch.");
        Require(evidence.cu5FreshRematerializationEvidenceIdentity == DigestFromHex(AcceptedFreshDigest),
                "CU5 Fresh evidence binding mismatch.");
        Require(evidence.measurementProfileIdentity == BuildMeasurementProfileIdentityV1(evidence.config),
                "Measurement profile identity mismatch.");
        Require(evidence.caseScheduleIdentity == BuildCaseScheduleIdentityV1(evidence.config.runCount),
                "Case schedule identity mismatch.");
        Require(evidence.candidateOrderIdentity == BuildCandidateOrderIdentityV1(),
                "Candidate order identity mismatch.");
        Require(evidence.verificationContextIdentity == VerificationContextIdentityV1(),
                "Verification context identity mismatch.");

        const auto kinds = CanonicalCandidateKindsV1();
        for (std::size_t index = 0; index < kinds.size(); ++index)
        {
            const auto& profile = evidence.candidates[index];
            Require(profile.kind == kinds[index], "Candidate profile order mismatch.");
            Require(profile.fixedCapacityBytes > 0, "Candidate fixed capacity is zero.");
            Require(profile.validationShaderBytecodeDigest != base::Digest256{},
                    "Validation shader digest is empty.");
            Require(profile.timingShaderBytecodeDigest != base::Digest256{},
                    "Timing shader digest is empty.");
        }

        const auto& activeValues = CanonicalMeasurementActiveCountsV1();
        const auto& transitionValues = CanonicalMeasurementTransitionCountsV1();
        std::map<std::tuple<std::uint32_t,std::uint32_t,std::uint32_t>, std::uint32_t> acceptedCounts;
        std::set<std::tuple<std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t>> acceptedRunCases;
        for (const auto& attempt : evidence.attempts)
        {
            Require(attempt.run < evidence.config.runCount, "Attempt run is outside config.");
            Require(attempt.caseOrder < CanonicalCaseCountV1, "Attempt case order is outside schedule.");
            Require(attempt.attempt < evidence.config.maximumBlockAttempts, "Attempt ordinal outside config.");
            Require(std::find(activeValues.begin(), activeValues.end(), attempt.key.activeCount) != activeValues.end(),
                    "Attempt Active count is outside canonical grid.");
            Require(std::find(transitionValues.begin(), transitionValues.end(), attempt.key.transitionCount) != transitionValues.end(),
                    "Attempt Transition count is outside canonical grid.");
            Require(attempt.key.updateCount + attempt.key.clearCount == attempt.key.transitionCount,
                    "Attempt update/clear count does not equal transition count.");
            Require(attempt.key.affectedBlockCount <= 64, "Attempt affected block count exceeds 64.");
            for (std::size_t index = 0; index < kinds.size(); ++index)
                Require(attempt.authorities[index].kind == kinds[index], "Attempt authority order mismatch.");

            if (attempt.accepted)
            {
                Require(attempt.rejectionReason.empty(), "Accepted attempt has a rejection reason.");
                Require(std::isfinite(attempt.controlRatio) && attempt.controlRatio >= 1.0,
                        "Accepted control ratio invalid.");
                Require(attempt.controlRatio <= evidence.config.maximumControlRatio,
                        "Accepted attempt exceeds control drift bound.");
                const auto expectedSamples = CanonicalOrderCountV1 *
                    evidence.config.measurementCyclesPerOrder * CanonicalCandidateCountV1;
                Require(attempt.samples.size() == expectedSamples,
                        "Accepted attempt sample count mismatch.");
                const auto key = CaseTuple(attempt.key);
                ++acceptedCounts[key];
                Require(acceptedRunCases.insert({attempt.run, std::get<0>(key), std::get<1>(key), std::get<2>(key)}).second,
                        "More than one accepted attempt exists for the same run/case.");
                for (const auto& sample : attempt.samples)
                {
                    Require(sample.run == attempt.run && sample.caseOrder == attempt.caseOrder,
                            "Sample run/case does not match attempt.");
                    Require(sample.acceptedAttempt == attempt.attempt,
                            "Sample accepted attempt does not match attempt.");
                    Require(sample.orderIndex < CanonicalOrderCountV1,
                            "Sample order index outside canonical orders.");
                    Require(sample.cycle < evidence.config.measurementCyclesPerOrder,
                            "Sample cycle outside config.");
                    Require(sample.orderPosition < CanonicalCandidateCountV1,
                            "Sample order position outside A/B/C.");
                    const auto expectedGroups = ExpectedDispatchGroups(
                        sample.candidate, attempt.key);
                    Require(sample.expectedDispatchGroupsX == expectedGroups,
                            "Sample expected dispatch group count differs from case authority.");
                    Require(sample.effectiveIterations >= evidence.config.iterationsPerBatch &&
                            sample.effectiveIterations <= MaximumAdaptiveIterationsV2 &&
                            sample.effectiveIterations % evidence.config.iterationsPerBatch == 0 &&
                            IsPowerOfTwo(sample.effectiveIterations /
                                         evidence.config.iterationsPerBatch),
                            "Sample adaptive iteration count is invalid.");
                    if (sample.timestampResolutionCensored)
                    {
                        Require(sample.timestampTickCount == 0,
                                "Resolution-censored sample retained nonzero ticks.");
                        Require(sample.expectedDispatchGroupsX == 0,
                                "Only a zero-dispatch Candidate may be resolution-censored.");
                    }
                    else
                    {
                        Require(sample.timestampTickCount > 0,
                                "Uncensored sample has a non-positive timestamp interval.");
                    }
                    const auto effectiveTicks = std::max<std::uint64_t>(
                        1u, sample.timestampTickCount);
                    const auto expectedBatchNanoseconds = TimestampTicksToNanoseconds(
                        effectiveTicks, evidence.adapter.timestampFrequency);
                    const auto expectedPerIteration = expectedBatchNanoseconds /
                        static_cast<double>(sample.effectiveIterations);
                    Require(std::isfinite(sample.gpuBatchNanoseconds) &&
                            sample.gpuBatchNanoseconds == expectedBatchNanoseconds,
                            "Sample batch time is not derived from recorded timestamp ticks.");
                    Require(std::isfinite(sample.gpuNanosecondsPerIteration) &&
                            sample.gpuNanosecondsPerIteration == expectedPerIteration &&
                            sample.gpuNanosecondsPerIteration > 0.0,
                            "Sample GPU time invalid.");
                    Require(std::isfinite(sample.controlNormalizedTime) &&
                            sample.controlNormalizedTime > 0.0,
                            "Sample normalized time invalid.");
                }
            }
            else
            {
                Require(!attempt.rejectionReason.empty(), "Rejected attempt lacks a reason.");
                Require(attempt.samples.empty(), "Rejected attempt retains timing samples.");
            }
        }

        Require(acceptedRunCases.size() ==
                    static_cast<std::size_t>(evidence.config.runCount) * CanonicalCaseCountV1,
                "Evidence does not contain one accepted block for every run/canonical case.");
        for (const auto pattern : CanonicalPatternsV1())
            for (const auto active : activeValues)
                for (const auto transition : transitionValues)
                    Require(acceptedCounts[{static_cast<std::uint32_t>(pattern), active, transition}] ==
                                evidence.config.runCount,
                            "A canonical case is missing an accepted block in one or more runs.");
        return base::Result<void, std::string>::Success();
    }
    catch (const std::exception& error)
    {
        return base::Result<void, std::string>::Failure(error.what());
    }
}

DecisionAnalysisV1 AnalyzeMeasurementEvidenceV1(const MeasurementEvidenceV1& evidence)
{
    const auto valid = ValidateMeasurementEvidenceV1(evidence);
    if (!valid) throw std::invalid_argument(valid.Error());

    DecisionAnalysisV1 analysis;
    std::map<std::tuple<std::uint32_t,std::uint32_t,std::uint32_t>,
             std::vector<const BlockAttemptV1*>> groups;
    for (const auto& attempt : evidence.attempts)
    {
        if (attempt.accepted)
        {
            groups[CaseTuple(attempt.key)].push_back(&attempt);
            ++analysis.acceptedBlockCount;
        }
        else ++analysis.rejectedBlockCount;
    }

    for (const auto& [key, attempts] : groups)
    {
        CaseDecisionV1 decision;
        decision.key = attempts.front()->key;
        std::array<std::vector<double>, CanonicalCandidateCountV1> absolute;
        std::array<std::vector<double>, CanonicalCandidateCountV1> normalized;
        std::vector<const TimingSampleV1*> samplePointers;
        for (const auto* attempt : attempts)
        {
            for (const auto& sample : attempt->samples)
            {
                const auto index = CandidateIndex(sample.candidate);
                absolute[index].push_back(sample.gpuNanosecondsPerIteration);
                normalized[index].push_back(sample.controlNormalizedTime);
                if (sample.timestampResolutionCensored)
                {
                    ++decision.timestampResolutionCensoredSampleCount[index];
                    ++analysis.timestampResolutionCensoredSampleCount;
                }
                samplePointers.push_back(&sample);
            }
        }
        for (std::size_t index = 0; index < CanonicalCandidateCountV1; ++index)
        {
            decision.medianNanosecondsPerIteration[index] = Median(absolute[index]);
            decision.medianControlNormalizedTime[index] = Median(normalized[index]);
        }
        const double minimum = *std::min_element(
            decision.medianNanosecondsPerIteration.begin(), decision.medianNanosecondsPerIteration.end());
        const auto kinds = CanonicalCandidateKindsV1();
        for (std::size_t index = 0; index < kinds.size(); ++index)
            if (decision.medianNanosecondsPerIteration[index] <=
                    minimum * (1.0 + evidence.config.relativeNoiseFloor))
                decision.observedWinnerSet.push_back(kinds[index]);

        decision.fullVersusCompact = BuildPairDecision(
            samplePointers, kinds[0], kinds[1], evidence.config);
        decision.compactVersusBlock = BuildPairDecision(
            samplePointers, kinds[1], kinds[2], evidence.config);
        decision.fullVersusBlock = BuildPairDecision(
            samplePointers, kinds[0], kinds[2], evidence.config);
        decision.pairedObservationCount = decision.fullVersusCompact.pairedObservationCount;
        analysis.cases.push_back(std::move(decision));
    }

    std::sort(analysis.cases.begin(), analysis.cases.end(), [](const auto& a, const auto& b)
    {
        return CaseTuple(a.key) < CaseTuple(b.key);
    });

    std::set<CandidateKindV1> overall;
    for (const auto& value : analysis.cases)
        overall.insert(value.observedWinnerSet.begin(), value.observedWinnerSet.end());
    analysis.overallObservedWinnerSet.assign(overall.begin(), overall.end());

    for (const auto pattern : CanonicalPatternsV1())
    {
        for (const auto active : CanonicalMeasurementActiveCountsV1())
        {
            std::vector<const CaseDecisionV1*> row;
            for (const auto& value : analysis.cases)
                if (value.key.pattern == pattern && value.key.activeCount == active) row.push_back(&value);
            std::sort(row.begin(), row.end(), [](const auto* a, const auto* b)
            {
                return a->key.transitionCount < b->key.transitionCount;
            });
            for (std::size_t index = 1; index < row.size(); ++index)
            {
                if (!WinnerSetsEqual(row[index - 1]->observedWinnerSet, row[index]->observedWinnerSet))
                {
                    analysis.winnerChangesAcrossTransitionCount = true;
                    analysis.transitionSurfaceEvents.push_back({
                        pattern, active,
                        row[index - 1]->key.transitionCount, row[index]->key.transitionCount,
                        row[index - 1]->observedWinnerSet, row[index]->observedWinnerSet});
                }
            }
        }
        for (const auto transition : CanonicalMeasurementTransitionCountsV1())
        {
            std::vector<const CaseDecisionV1*> column;
            for (const auto& value : analysis.cases)
                if (value.key.pattern == pattern && value.key.transitionCount == transition)
                    column.push_back(&value);
            std::sort(column.begin(), column.end(), [](const auto* a, const auto* b)
            {
                return a->key.activeCount < b->key.activeCount;
            });
            for (std::size_t index = 1; index < column.size(); ++index)
            {
                if (!WinnerSetsEqual(column[index - 1]->observedWinnerSet,
                                     column[index]->observedWinnerSet))
                {
                    analysis.winnerChangesAcrossActiveCount = true;
                    analysis.activeSurfaceEvents.push_back({
                        pattern, transition,
                        column[index - 1]->key.activeCount, column[index]->key.activeCount,
                        column[index - 1]->observedWinnerSet, column[index]->observedWinnerSet});
                }
            }
        }
    }

    for (const auto active : CanonicalMeasurementActiveCountsV1())
    {
        for (const auto transition : CanonicalMeasurementTransitionCountsV1())
        {
            std::vector<std::vector<CandidateKindV1>> sets;
            for (const auto& value : analysis.cases)
                if (value.key.activeCount == active && value.key.transitionCount == transition)
                    sets.push_back(value.observedWinnerSet);
            for (std::size_t index = 1; index < sets.size(); ++index)
                if (!WinnerSetsEqual(sets.front(), sets[index]))
                    analysis.sameActiveAndTransitionWinnerDependsOnPattern = true;
        }
    }
    return analysis;
}

std::string BuildDecisionReportV1(
    const MeasurementEvidenceV1& evidence,
    const DecisionAnalysisV1& analysis)
{
    std::ostringstream stream;
    stream << "SGE4-5 Spiral 7 CU6 Real-GPU Measurement and Decision Evidence V2\n";
    stream << "Adapter: " << evidence.adapter.description << "\n";
    stream << "Adapter fingerprint: "; AppendDigestText(stream, evidence.adapter.fingerprint); stream << "\n";
    stream << "Timestamp frequency: " << evidence.adapter.timestampFrequency << " Hz\n";
    stream << "Stable power state applied: " << (evidence.adapter.stablePowerStateApplied ? "true" : "false") << "\n";
    stream << "Grid: 4 patterns x 5 Active counts x 8 Transition counts = 160 cases per run\n";
    stream << "Runs: " << evidence.config.runCount
           << ", balanced orders: 6, cycles/order: " << evidence.config.measurementCyclesPerOrder
           << ", iterations/batch: " << evidence.config.iterationsPerBatch << "\n";
    stream << "Accepted blocks: " << analysis.acceptedBlockCount
           << ", rejected attempts: " << analysis.rejectedBlockCount << "\n";
    stream << "Timestamp-resolution-censored samples: "
           << analysis.timestampResolutionCensoredSampleCount
           << " (one-tick conservative upper bound; zero-dispatch only)\n";
    stream << "Overall observed winner set: " << WinnerSetText(analysis.overallObservedWinnerSet) << "\n";
    stream << "Winner changes across Transition count: "
           << (analysis.winnerChangesAcrossTransitionCount ? "true" : "false") << "\n";
    stream << "Winner changes across Active count: "
           << (analysis.winnerChangesAcrossActiveCount ? "true" : "false") << "\n";
    stream << "Same (A,T) winner depends on pattern: "
           << (analysis.sameActiveAndTransitionWinnerDependsOnPattern ? "true" : "false") << "\n\n";

    for (const auto& value : analysis.cases)
    {
        std::array<std::pair<double, CandidateKindV1>, CanonicalCandidateCountV1> ranking{{
            {value.medianNanosecondsPerIteration[0], CandidateKindV1::FullActiveDenseRecompute},
            {value.medianNanosecondsPerIteration[1], CandidateKindV1::CompactDeltaIndexHistoryReuse},
            {value.medianNanosecondsPerIteration[2], CandidateKindV1::AffectedBlockDeltaHistoryReuse}}};
        std::sort(ranking.begin(), ranking.end());
        stream << "[RANKING] pattern=" << PatternNameV1(value.key.pattern)
               << " A=" << value.key.activeCount
               << " T=" << value.key.transitionCount
               << " U=" << value.key.updateCount
               << " C=" << value.key.clearCount
               << " blocks=" << value.key.affectedBlockCount
               << " winnerSet=" << WinnerSetText(value.observedWinnerSet)
               << " censored=A:" << value.timestampResolutionCensoredSampleCount[0]
               << "/B:" << value.timestampResolutionCensoredSampleCount[1]
               << "/C:" << value.timestampResolutionCensoredSampleCount[2];
        for (std::size_t index = 0; index < ranking.size(); ++index)
            stream << " " << (index + 1) << "=" << CandidateLetterV1(ranking[index].second)
                   << "(" << std::fixed << std::setprecision(3) << ranking[index].first << " ns)";
        stream << "\n";
    }

    stream << "\n";
    for (const auto& event : analysis.transitionSurfaceEvents)
        stream << "[CROSSOVER-T] pattern=" << PatternNameV1(event.pattern)
               << " A=" << event.activeCount
               << " T=" << event.lowerTransitionCount << "->" << event.upperTransitionCount
               << " " << WinnerSetText(event.fromWinnerSet) << "->" << WinnerSetText(event.toWinnerSet)
               << "\n";
    for (const auto& event : analysis.activeSurfaceEvents)
        stream << "[CROSSOVER-A] pattern=" << PatternNameV1(event.pattern)
               << " T=" << event.transitionCount
               << " A=" << event.lowerActiveCount << "->" << event.upperActiveCount
               << " " << WinnerSetText(event.fromWinnerSet) << "->" << WinnerSetText(event.toWinnerSet)
               << "\n";

    stream << "\nDecision boundary:\n";
    stream << "RuntimeCandidatePolicyAuthorization = None\n";
    stream << "MeasurementResultScope = AdapterAndDriverSpecificObservation\n";
    stream << "UniversalWinnerClaim = Forbidden\n";
    stream << "Spiral7ExperimentStatus = Complete\n";
    stream << "Spiral7Closure = OwnerRequired\n";
    stream << "NextProgramAfterOwnerClosure = Level4V2CanonicalReconstruction\n";
    return stream.str();
}

std::string BuildDecisionCsvV1(
    const MeasurementEvidenceV1&,
    const DecisionAnalysisV1& analysis)
{
    std::ostringstream stream;
    stream << "pattern,active_count,transition_count,update_count,clear_count,affected_block_count,transition_shape,";
    stream << "median_A_ns,median_B_ns,median_C_ns,censored_A,censored_B,censored_C,winner_set,paired_observations,";
    stream << "A_over_B,A_vs_B,B_over_C,B_vs_C,A_over_C,A_vs_C\n";
    const auto pairText = [](PairWinnerV1 value)
    {
        switch (value)
        {
        case PairWinnerV1::Left: return "Left";
        case PairWinnerV1::Right: return "Right";
        case PairWinnerV1::NoiseEquivalent: return "NoiseEquivalent";
        case PairWinnerV1::Unresolved: return "Unresolved";
        }
        return "Unknown";
    };
    for (const auto& value : analysis.cases)
    {
        stream << PatternNameV1(value.key.pattern) << ','
               << value.key.activeCount << ',' << value.key.transitionCount << ','
               << value.key.updateCount << ',' << value.key.clearCount << ','
               << value.key.affectedBlockCount << ','
               << TransitionShapeNameV1(value.key.transitionShape) << ','
               << std::fixed << std::setprecision(6)
               << value.medianNanosecondsPerIteration[0] << ','
               << value.medianNanosecondsPerIteration[1] << ','
               << value.medianNanosecondsPerIteration[2] << ','
               << value.timestampResolutionCensoredSampleCount[0] << ','
               << value.timestampResolutionCensoredSampleCount[1] << ','
               << value.timestampResolutionCensoredSampleCount[2] << ','
               << WinnerSetText(value.observedWinnerSet) << ','
               << value.pairedObservationCount << ','
               << value.fullVersusCompact.medianLeftOverRight << ','
               << pairText(value.fullVersusCompact.winner) << ','
               << value.compactVersusBlock.medianLeftOverRight << ','
               << pairText(value.compactVersusBlock.winner) << ','
               << value.fullVersusBlock.medianLeftOverRight << ','
               << pairText(value.fullVersusBlock.winner) << '\n';
    }
    return stream.str();
}

std::vector<std::byte> BuildSyntheticSelfTestEvidenceV1()
{
    MeasurementEvidenceV1 evidence;
    evidence.captureUnixNanoseconds = 1;
    evidence.config.runCount = 1;
    evidence.config.measurementCyclesPerOrder = 1;
    evidence.config.iterationsPerBatch = 64;
    evidence.config.warmupIterations = 64;
    evidence.config.maximumBlockAttempts = 2;
    evidence.adapter.description = "Synthetic Spiral 7 Measurement Adapter";
    evidence.adapter.vendorId = 0xFFFF;
    evidence.adapter.deviceId = 7;
    evidence.adapter.timestampFrequency = 1'000'000'000ull;
    evidence.adapter.fingerprint = LabelDigest("synthetic-adapter");
    evidence.cu5ArchitectureEvidenceIdentity = DigestFromHex(AcceptedArchitectureDigest);
    evidence.cu5ControlledRecoveryEvidenceIdentity = DigestFromHex(AcceptedControlledDigest);
    evidence.cu5FreshRematerializationEvidenceIdentity = DigestFromHex(AcceptedFreshDigest);
    evidence.semanticIdentity = LabelDigest("synthetic-semantic");
    evidence.verificationContextIdentity = VerificationContextIdentityV1();
    evidence.measurementProfileIdentity = BuildMeasurementProfileIdentityV1(evidence.config);
    evidence.caseScheduleIdentity = BuildCaseScheduleIdentityV1(evidence.config.runCount);
    evidence.candidateOrderIdentity = BuildCandidateOrderIdentityV1();

    const auto kinds = CanonicalCandidateKindsV1();
    for (std::size_t index = 0; index < kinds.size(); ++index)
    {
        auto& profile = evidence.candidates[index];
        profile.kind = kinds[index];
        profile.representationRole = index == 0
            ? family_candidate::DeltaFamilyRepresentationRoleV1::DenseActiveMask4096
            : (index == 1
                ? family_candidate::DeltaFamilyRepresentationRoleV1::CanonicalSortedIndexActionList
                : family_candidate::DeltaFamilyRepresentationRoleV1::AffectedBlockUpdateClearMasks);
        profile.dispatchPolicy = index == 0
            ? family_candidate::DeltaFamilyDispatchPolicyV1::FixedUniverseGroups
            : (index == 1
                ? family_candidate::DeltaFamilyDispatchPolicyV1::CeilTransitionGroups
                : family_candidate::DeltaFamilyDispatchPolicyV1::OneGroupPerAffectedBlock);
        profile.payloadStrideBytes = index == 0 ? 1u : (index == 1 ? 8u : 24u);
        profile.fixedCapacityBytes = index == 0 ? 512u : (index == 1 ? 32768u : 1536u);
        profile.targetProfileIdentity = LabelDigest("target" + std::to_string(index));
        profile.observationContractIdentity = LabelDigest("observation" + std::to_string(index));
        profile.completionContractIdentity = LabelDigest("completion" + std::to_string(index));
        profile.consumerProgramIdentity = LabelDigest("consumer" + std::to_string(index));
        profile.representationResourceContractIdentity = LabelDigest("resource" + std::to_string(index));
        profile.validationShaderBytecodeDigest = LabelDigest("validation-shader" + std::to_string(index));
        profile.timingShaderBytecodeDigest = LabelDigest("timing-shader" + std::to_string(index));
    }

    const auto schedule = CanonicalCaseScheduleV1(0);
    const auto orders = CanonicalBalancedOrdersV1();
    for (std::size_t caseOrder = 0; caseOrder < schedule.size(); ++caseOrder)
    {
        const auto pattern = PatternFromU32(schedule[caseOrder][0]);
        const auto active = schedule[caseOrder][1];
        const auto transition = schedule[caseOrder][2];
        BlockAttemptV1 attempt;
        attempt.run = 0;
        attempt.caseOrder = static_cast<std::uint32_t>(caseOrder);
        attempt.attempt = 0;
        attempt.key = SyntheticKey(pattern, active, transition);
        attempt.accepted = true;
        attempt.controlBeforeNanosecondsPerIteration = 100.0;
        attempt.controlAfterNanosecondsPerIteration = 101.0;
        attempt.controlReferenceNanosecondsPerIteration = 100.5;
        attempt.controlRatio = 1.01;
        const auto times = SyntheticTimes(attempt.key);
        for (std::size_t index = 0; index < kinds.size(); ++index)
        {
            auto& authority = attempt.authorities[index];
            authority.kind = kinds[index];
            const auto suffix = std::to_string(caseOrder) + ":" + std::to_string(index);
            authority.rawCandidateIdentity = LabelDigest("raw:" + suffix);
            authority.verifiedPlanIdentity = LabelDigest("plan:" + suffix);
            authority.verifierCertificateIdentity = LabelDigest("cert:" + suffix);
            authority.artifactIdentity = LabelDigest("artifact:" + suffix);
            authority.representationResourceIdentity = LabelDigest("resource:" + suffix);
            authority.outputDigest = LabelDigest("output:" + std::to_string(caseOrder));
        }
        for (std::size_t orderIndex = 0; orderIndex < orders.size(); ++orderIndex)
        {
            for (std::size_t position = 0; position < CanonicalCandidateCountV1; ++position)
            {
                const auto candidateIndex = orders[orderIndex][position];
                TimingSampleV1 sample;
                sample.run = 0;
                sample.caseOrder = static_cast<std::uint32_t>(caseOrder);
                sample.acceptedAttempt = 0;
                sample.orderIndex = static_cast<std::uint32_t>(orderIndex);
                sample.cycle = 0;
                sample.orderPosition = static_cast<std::uint32_t>(position);
                sample.candidate = kinds[candidateIndex];
                sample.expectedDispatchGroupsX = ExpectedDispatchGroups(
                    sample.candidate, attempt.key);
                sample.effectiveIterations = evidence.config.iterationsPerBatch;
                sample.timestampResolutionCensored = sample.expectedDispatchGroupsX == 0;
                if (sample.timestampResolutionCensored)
                {
                    sample.timestampTickCount = 0;
                }
                else
                {
                    const double jitter = 1.0 +
                        (static_cast<double>((orderIndex + position) % 3) - 1.0) * 0.002;
                    const double requestedNanoseconds = times[candidateIndex] * jitter *
                        static_cast<double>(sample.effectiveIterations);
                    sample.timestampTickCount = std::max<std::uint64_t>(
                        1u, static_cast<std::uint64_t>(std::llround(
                            requestedNanoseconds *
                            static_cast<double>(evidence.adapter.timestampFrequency) / 1.0e9)));
                }
                sample.gpuBatchNanoseconds = TimestampTicksToNanoseconds(
                    std::max<std::uint64_t>(1u, sample.timestampTickCount),
                    evidence.adapter.timestampFrequency);
                sample.gpuNanosecondsPerIteration = sample.gpuBatchNanoseconds /
                    static_cast<double>(sample.effectiveIterations);
                sample.blockControlNanosecondsPerIteration =
                    attempt.controlReferenceNanosecondsPerIteration;
                sample.controlNormalizedTime = sample.gpuNanosecondsPerIteration /
                    attempt.controlReferenceNanosecondsPerIteration;
                attempt.samples.push_back(sample);
            }
        }
        evidence.attempts.push_back(std::move(attempt));
    }
    return SerializeMeasurementEvidenceV1(evidence);
}

base::Result<void, std::string> RunMeasurementFormatSelfTestV1()
{
    try
    {
        const auto orders = CanonicalBalancedOrdersV1();
        Require(orders.size() == CanonicalOrderCountV1, "Balanced order count mismatch.");
        std::set<std::array<std::uint32_t, CanonicalCandidateCountV1>> uniqueOrders(
            orders.begin(), orders.end());
        Require(uniqueOrders.size() == CanonicalOrderCountV1, "Balanced orders are not unique.");
        for (const auto& order : orders)
        {
            auto sorted = order;
            std::sort(sorted.begin(), sorted.end());
            Require(sorted == std::array<std::uint32_t,3>{0u,1u,2u},
                    "Balanced order is not a permutation of A/B/C.");
        }

        auto bytes = BuildSyntheticSelfTestEvidenceV1();
        auto parsed = DeserializeMeasurementEvidenceV1(bytes);
        Require(static_cast<bool>(parsed), "Synthetic evidence did not parse.");
        auto serializedAgain = SerializeMeasurementEvidenceV1(parsed.Value());
        Require(bytes == serializedAgain, "Synthetic evidence did not round-trip byte-identically.");
        const auto analysis = AnalyzeMeasurementEvidenceV1(parsed.Value());
        Require(analysis.cases.size() == CanonicalCaseCountV1,
                "Synthetic decision case count mismatch.");
        Require(!analysis.transitionSurfaceEvents.empty(),
                "Synthetic evidence failed to produce a Transition crossover.");
        Require(analysis.sameActiveAndTransitionWinnerDependsOnPattern,
                "Synthetic evidence failed to produce pattern dependence.");
        Require(analysis.timestampResolutionCensoredSampleCount > 0,
                "Synthetic evidence did not exercise zero-dispatch timestamp censoring.");
        for (const auto& attempt : parsed.Value().attempts)
        {
            for (const auto& sample : attempt.samples)
            {
                if (sample.timestampResolutionCensored)
                    Require(sample.expectedDispatchGroupsX == 0 &&
                            sample.timestampTickCount == 0,
                            "Synthetic timestamp censoring escaped the zero-dispatch boundary.");
            }
        }

        auto corrupted = bytes;
        corrupted[corrupted.size() / 2] ^= std::byte{1};
        Require(!DeserializeMeasurementEvidenceV1(corrupted),
                "Corrupted measurement evidence was accepted.");
        corrupted = bytes;
        corrupted.pop_back();
        Require(!DeserializeMeasurementEvidenceV1(corrupted),
                "Truncated measurement evidence was accepted.");
        return base::Result<void, std::string>::Success();
    }
    catch (const std::exception& error)
    {
        return base::Result<void, std::string>::Failure(error.what());
    }
}
} // namespace sge4_5::spiral7::performance
