#include "Spiral6PerformanceExperiment.h"

#include "../00_Foundation/BinaryIO.h"
#include "../00_Foundation/FileIO.h"
#include "../161_SparseCandidateFamilyPlanner/SparseCandidateFamilyPlanner.h"
#include "../162_SparseCandidateFamilyVerifier/SparseCandidateFamilyVerifier.h"

#include <algorithm>
#include <array>
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
#include <utility>

namespace sge4_5::spiral6::performance
{
namespace fc = family_candidate;
namespace fp = family_planner;
namespace fv = family_verification;
namespace
{
constexpr std::string_view EvidenceMagic = "SGE4-5.Spiral6.MeasurementEvidence.V1";
constexpr std::string_view MeasurementProfileDomain = "SGE4-5.Spiral6.MeasurementProfile.V1";
constexpr std::string_view OrderBalanceDomain = "SGE4-5.Spiral6.BalancedOrders.V1";
constexpr std::string_view ArchitectureEvidenceHex =
    "4a634317e988e3f3f9639f249a35af0ce25e18e19ea52a0500ce75ab09674b7e";
constexpr std::string_view ControlledRecoveryEvidenceHex =
    "65aec68060dd68ae42a191e79c5cf2fffc8538442b8d6c029fe59927709bd77f";
constexpr std::string_view FreshRematerializationEvidenceHex =
    "a9398025d0c5235e2370bfd5d0c34bdfcdc114836a763f8b6151bd95d80ff032";

void Require(bool condition, std::string_view message)
{
    if (!condition) throw std::runtime_error(std::string(message));
}

base::Digest256 LiteralIdentity(std::string_view value)
{
    return base::Sha256(std::as_bytes(std::span(value.data(), value.size())));
}

base::Digest256 DigestFromHex(std::string_view value)
{
    Require(value.size() == 64, "Digest hex length must be 64.");
    auto nibble = [](char c) -> std::uint8_t
    {
        if (c >= '0' && c <= '9') return static_cast<std::uint8_t>(c - '0');
        if (c >= 'a' && c <= 'f') return static_cast<std::uint8_t>(10 + c - 'a');
        if (c >= 'A' && c <= 'F') return static_cast<std::uint8_t>(10 + c - 'A');
        throw std::runtime_error("Digest contains a non-hex character.");
    };
    base::Digest256 result{};
    for (std::size_t i = 0; i < result.size(); ++i)
    {
        result[i] = static_cast<std::byte>((nibble(value[i * 2]) << 4u) | nibble(value[i * 2 + 1]));
    }
    return result;
}

void WriteString(base::BinaryWriter& writer, std::string_view value)
{
    Require(value.size() <= std::numeric_limits<std::uint32_t>::max(), "String is too large.");
    writer.WriteU32(static_cast<std::uint32_t>(value.size()));
    writer.WriteBytes(std::as_bytes(std::span(value.data(), value.size())));
}

std::string ReadStringOrThrow(base::BinaryReader& reader, std::uint32_t maximum = 16u * 1024u * 1024u)
{
    auto size = reader.ReadU32();
    if (!size) throw std::runtime_error(size.Error());
    Require(size.Value() <= maximum, "String exceeds evidence limit.");
    auto bytes = reader.ReadBytes(size.Value());
    if (!bytes) throw std::runtime_error(bytes.Error());
    return std::string(reinterpret_cast<const char*>(bytes.Value().data()), bytes.Value().size());
}

void WriteDigest(base::BinaryWriter& writer, const base::Digest256& value)
{
    writer.WriteBytes(value);
}

base::Digest256 ReadDigestOrThrow(base::BinaryReader& reader)
{
    auto bytes = reader.ReadBytes(32);
    if (!bytes) throw std::runtime_error(bytes.Error());
    base::Digest256 value{};
    std::memcpy(value.data(), bytes.Value().data(), value.size());
    return value;
}

void WriteDouble(base::BinaryWriter& writer, double value)
{
    writer.WriteU64(std::bit_cast<std::uint64_t>(value));
}

double ReadDoubleOrThrow(base::BinaryReader& reader)
{
    auto value = reader.ReadU64();
    if (!value) throw std::runtime_error(value.Error());
    return std::bit_cast<double>(value.Value());
}

void WriteBool(base::BinaryWriter& writer, bool value)
{
    writer.WriteU8(value ? 1u : 0u);
}

bool ReadBoolOrThrow(base::BinaryReader& reader)
{
    auto value = reader.ReadU8();
    if (!value) throw std::runtime_error(value.Error());
    Require(value.Value() <= 1u, "Boolean evidence byte is not canonical.");
    return value.Value() != 0;
}

std::uint32_t ReadU32OrThrow(base::BinaryReader& reader)
{
    auto value = reader.ReadU32();
    if (!value) throw std::runtime_error(value.Error());
    return value.Value();
}

std::int32_t ReadI32OrThrow(base::BinaryReader& reader)
{
    auto value = reader.ReadI32();
    if (!value) throw std::runtime_error(value.Error());
    return value.Value();
}

std::uint64_t ReadU64OrThrow(base::BinaryReader& reader)
{
    auto value = reader.ReadU64();
    if (!value) throw std::runtime_error(value.Error());
    return value.Value();
}

std::string JsonEscape(std::string_view value)
{
    std::string result;
    result.reserve(value.size());
    for (char c : value)
    {
        switch (c)
        {
        case '\\': result += "\\\\"; break;
        case '"': result += "\\\""; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default: result += c; break;
        }
    }
    return result;
}

std::string PairWinnerName(PairWinnerV1 value)
{
    switch (value)
    {
    case PairWinnerV1::Left: return "Left";
    case PairWinnerV1::Right: return "Right";
    case PairWinnerV1::NoiseEquivalent: return "NoiseEquivalent";
    }
    return "Unknown";
}

std::string TransitionName(TransitionClassificationV1 value)
{
    switch (value)
    {
    case TransitionClassificationV1::NoObservedWinnerTransition: return "NoObservedWinnerTransition";
    case TransitionClassificationV1::SingleStableWinnerTransition: return "SingleStableWinnerTransition";
    case TransitionClassificationV1::MultipleWinnerTransitions: return "MultipleWinnerTransitions";
    case TransitionClassificationV1::NoiseEquivalentOrUnresolved: return "NoiseEquivalentOrUnresolved";
    }
    return "Unknown";
}

CandidateKindV1 CandidateFromU32(std::uint32_t value)
{
    switch (value)
    {
    case 1: return CandidateKindV1::DenseMembershipMaskPredicate;
    case 2: return CandidateKindV1::CompactIndexListDispatch;
    case 3: return CandidateKindV1::ActiveBlockListLocalMask;
    default: throw std::runtime_error("Evidence contains an unknown Candidate kind.");
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
    default: throw std::runtime_error("Evidence contains an unknown Sparse pattern.");
    }
}

fc::SparseFamilyRepresentationRoleV1 RoleFromU32(std::uint32_t value)
{
    switch (value)
    {
    case 1: return fc::SparseFamilyRepresentationRoleV1::DenseMembershipMask4096;
    case 2: return fc::SparseFamilyRepresentationRoleV1::CompactSortedUniverseIndexList;
    case 3: return fc::SparseFamilyRepresentationRoleV1::ActiveBlockListWithLocal64BitMask;
    default: throw std::runtime_error("Evidence contains an unknown representation role.");
    }
}

fc::SparseFamilyDispatchPolicyV1 DispatchFromU32(std::uint32_t value)
{
    switch (value)
    {
    case 1: return fc::SparseFamilyDispatchPolicyV1::FixedUniverseGroups;
    case 2: return fc::SparseFamilyDispatchPolicyV1::CeilCardinalityGroups;
    case 3: return fc::SparseFamilyDispatchPolicyV1::OneGroupPerActiveBlock;
    default: throw std::runtime_error("Evidence contains an unknown dispatch policy.");
    }
}

void ValidateConfig(const MeasurementConfigV1& config)
{
    Require(config.warmupCyclesPerOrder <= 1000, "Warm-up cycle count is outside the qualified range.");
    Require(config.measurementCyclesPerOrder > 0 && config.measurementCyclesPerOrder <= 1000,
        "Measurement cycle count is outside the qualified range.");
    Require(config.runCount > 0 && config.runCount <= 100, "Run count is outside the qualified range.");
    Require(!config.clockPolicy.empty() && config.clockPolicy.size() <= 256, "Clock policy is invalid.");
    Require(config.primaryMetric == "GpuCandidatePathNanoseconds", "Primary metric is not canonical.");
    Require(std::isfinite(config.absoluteNoiseFloorNanoseconds) && config.absoluteNoiseFloorNanoseconds >= 0.0,
        "Absolute noise floor is invalid.");
    Require(std::isfinite(config.relativeNoiseFloor) && config.relativeNoiseFloor >= 0.0 && config.relativeNoiseFloor <= 1.0,
        "Relative noise floor is invalid.");
}

void ValidateOrders()
{
    const auto orders = CanonicalBalancedOrdersV1();
    Require(orders.size() == CanonicalOrderCountV1, "Balanced Candidate order count is invalid.");
    std::array<std::array<std::uint32_t, CanonicalCandidateCountV1>, CanonicalCandidateCountV1> counts{};
    std::set<std::array<std::uint32_t, CanonicalCandidateCountV1>> unique;
    for (const auto& order : orders)
    {
        Require(unique.insert(order).second, "Balanced Candidate order is duplicated.");
        std::array<bool, CanonicalCandidateCountV1> seen{};
        for (std::size_t position = 0; position < order.size(); ++position)
        {
            Require(order[position] < CanonicalCandidateCountV1, "Candidate order index is out of range.");
            Require(!seen[order[position]], "Candidate order is not a permutation.");
            seen[order[position]] = true;
            ++counts[order[position]][position];
        }
    }
    for (const auto& candidate : counts)
        for (std::uint32_t count : candidate)
            Require(count == 2, "Every Candidate must appear twice at every order position.");
}

void WriteConfig(base::BinaryWriter& writer, const MeasurementConfigV1& value)
{
    writer.WriteU32(value.warmupCyclesPerOrder);
    writer.WriteU32(value.measurementCyclesPerOrder);
    writer.WriteU32(value.runCount);
    writer.WriteU32(value.adapterIndex);
    WriteString(writer, value.clockPolicy);
    WriteString(writer, value.primaryMetric);
    WriteDouble(writer, value.absoluteNoiseFloorNanoseconds);
    WriteDouble(writer, value.relativeNoiseFloor);
}

MeasurementConfigV1 ReadConfigOrThrow(base::BinaryReader& reader)
{
    MeasurementConfigV1 value;
    value.warmupCyclesPerOrder = ReadU32OrThrow(reader);
    value.measurementCyclesPerOrder = ReadU32OrThrow(reader);
    value.runCount = ReadU32OrThrow(reader);
    value.adapterIndex = ReadU32OrThrow(reader);
    value.clockPolicy = ReadStringOrThrow(reader, 256);
    value.primaryMetric = ReadStringOrThrow(reader, 256);
    value.absoluteNoiseFloorNanoseconds = ReadDoubleOrThrow(reader);
    value.relativeNoiseFloor = ReadDoubleOrThrow(reader);
    ValidateConfig(value);
    return value;
}

void WriteAdapter(base::BinaryWriter& writer, const AdapterProfileV1& value)
{
    WriteString(writer, value.description);
    writer.WriteU32(value.vendorId);
    writer.WriteU32(value.deviceId);
    writer.WriteU32(value.subsystemId);
    writer.WriteU32(value.revision);
    writer.WriteU32(value.luidLow);
    writer.WriteI32(value.luidHigh);
    writer.WriteU64(value.driverVersion);
    writer.WriteU64(value.timestampFrequency);
    WriteBool(writer, value.uma);
    WriteBool(writer, value.cacheCoherentUma);
    WriteDigest(writer, value.fingerprint);
}

AdapterProfileV1 ReadAdapterOrThrow(base::BinaryReader& reader)
{
    AdapterProfileV1 value;
    value.description = ReadStringOrThrow(reader, 4096);
    value.vendorId = ReadU32OrThrow(reader);
    value.deviceId = ReadU32OrThrow(reader);
    value.subsystemId = ReadU32OrThrow(reader);
    value.revision = ReadU32OrThrow(reader);
    value.luidLow = ReadU32OrThrow(reader);
    value.luidHigh = ReadI32OrThrow(reader);
    value.driverVersion = ReadU64OrThrow(reader);
    value.timestampFrequency = ReadU64OrThrow(reader);
    value.uma = ReadBoolOrThrow(reader);
    value.cacheCoherentUma = ReadBoolOrThrow(reader);
    value.fingerprint = ReadDigestOrThrow(reader);
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
    WriteDigest(writer, value.representationShaderBytecodeDigest);
    WriteDigest(writer, value.argumentProducerShaderBytecodeDigest);
}

CandidateProfileV1 ReadCandidateProfileOrThrow(base::BinaryReader& reader)
{
    CandidateProfileV1 value;
    value.kind = CandidateFromU32(ReadU32OrThrow(reader));
    value.representationRole = RoleFromU32(ReadU32OrThrow(reader));
    value.dispatchPolicy = DispatchFromU32(ReadU32OrThrow(reader));
    value.payloadStrideBytes = ReadU32OrThrow(reader);
    value.fixedCapacityBytes = ReadU32OrThrow(reader);
    value.targetProfileIdentity = ReadDigestOrThrow(reader);
    value.observationContractIdentity = ReadDigestOrThrow(reader);
    value.completionContractIdentity = ReadDigestOrThrow(reader);
    value.consumerProgramIdentity = ReadDigestOrThrow(reader);
    value.representationResourceContractIdentity = ReadDigestOrThrow(reader);
    value.representationShaderBytecodeDigest = ReadDigestOrThrow(reader);
    value.argumentProducerShaderBytecodeDigest = ReadDigestOrThrow(reader);
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
    WriteDigest(writer, value.frozenBindingIdentity);
}

CandidateCaseAuthorityV1 ReadAuthorityOrThrow(base::BinaryReader& reader)
{
    CandidateCaseAuthorityV1 value;
    value.kind = CandidateFromU32(ReadU32OrThrow(reader));
    value.rawCandidateIdentity = ReadDigestOrThrow(reader);
    value.verifiedPlanIdentity = ReadDigestOrThrow(reader);
    value.verifierCertificateIdentity = ReadDigestOrThrow(reader);
    value.artifactIdentity = ReadDigestOrThrow(reader);
    value.representationResourceIdentity = ReadDigestOrThrow(reader);
    value.frozenBindingIdentity = ReadDigestOrThrow(reader);
    return value;
}

void WriteSample(base::BinaryWriter& writer, const TimingSampleV1& value)
{
    writer.WriteU32(value.run);
    writer.WriteU32(value.orderIndex);
    writer.WriteU32(value.cycle);
    writer.WriteU32(value.orderPosition);
    writer.WriteU32(static_cast<std::uint32_t>(value.candidate));
    writer.WriteU32(static_cast<std::uint32_t>(value.pattern));
    writer.WriteU32(value.activeCount);
    writer.WriteU32(value.activeBlockCount);

    WriteDouble(writer, value.sparseInputValidationNanoseconds);
    WriteDouble(writer, value.derivedRepresentationConstructionUploadNanoseconds);
    WriteDouble(writer, value.commandRecordingNanoseconds);
    WriteDouble(writer, value.submitAndWaitNanoseconds);
    WriteDouble(writer, value.numericObservationNanoseconds);
    WriteDouble(writer, value.cpuEndToEndNanoseconds);

    WriteDouble(writer, value.gpuSentinelInitializationNanoseconds);
    WriteDouble(writer, value.gpuIndirectArgumentGenerationNanoseconds);
    WriteDouble(writer, value.gpuSparseConsumerExecutionNanoseconds);
    WriteDouble(writer, value.gpuCompletionNanoseconds);
    WriteDouble(writer, value.gpuObservationCopyNanoseconds);
    WriteDouble(writer, value.gpuCandidatePathNanoseconds);
    WriteDouble(writer, value.gpuTotalNanoseconds);

    writer.WriteU32(value.expectedDispatchGroupsX);
    writer.WriteU32(value.observedDispatchGroupsX);
    writer.WriteU32(value.activeWriteCount);
    writer.WriteU32(value.missingWriteCount);
    writer.WriteU32(value.unauthorizedWriteCount);
    writer.WriteU32(value.activeMismatchCount);
    writer.WriteU32(value.nonFiniteCount);
    writer.WriteU32(value.homogeneousCoordinateMismatchCount);
    WriteDouble(writer, value.maxAbsoluteError);
    WriteDouble(writer, value.maxRelativeError);
    writer.WriteU32(value.maxUlpError);
    WriteDigest(writer, value.outputDigest);
}

TimingSampleV1 ReadSampleOrThrow(base::BinaryReader& reader)
{
    TimingSampleV1 value;
    value.run = ReadU32OrThrow(reader);
    value.orderIndex = ReadU32OrThrow(reader);
    value.cycle = ReadU32OrThrow(reader);
    value.orderPosition = ReadU32OrThrow(reader);
    value.candidate = CandidateFromU32(ReadU32OrThrow(reader));
    value.pattern = PatternFromU32(ReadU32OrThrow(reader));
    value.activeCount = ReadU32OrThrow(reader);
    value.activeBlockCount = ReadU32OrThrow(reader);

    value.sparseInputValidationNanoseconds = ReadDoubleOrThrow(reader);
    value.derivedRepresentationConstructionUploadNanoseconds = ReadDoubleOrThrow(reader);
    value.commandRecordingNanoseconds = ReadDoubleOrThrow(reader);
    value.submitAndWaitNanoseconds = ReadDoubleOrThrow(reader);
    value.numericObservationNanoseconds = ReadDoubleOrThrow(reader);
    value.cpuEndToEndNanoseconds = ReadDoubleOrThrow(reader);

    value.gpuSentinelInitializationNanoseconds = ReadDoubleOrThrow(reader);
    value.gpuIndirectArgumentGenerationNanoseconds = ReadDoubleOrThrow(reader);
    value.gpuSparseConsumerExecutionNanoseconds = ReadDoubleOrThrow(reader);
    value.gpuCompletionNanoseconds = ReadDoubleOrThrow(reader);
    value.gpuObservationCopyNanoseconds = ReadDoubleOrThrow(reader);
    value.gpuCandidatePathNanoseconds = ReadDoubleOrThrow(reader);
    value.gpuTotalNanoseconds = ReadDoubleOrThrow(reader);

    value.expectedDispatchGroupsX = ReadU32OrThrow(reader);
    value.observedDispatchGroupsX = ReadU32OrThrow(reader);
    value.activeWriteCount = ReadU32OrThrow(reader);
    value.missingWriteCount = ReadU32OrThrow(reader);
    value.unauthorizedWriteCount = ReadU32OrThrow(reader);
    value.activeMismatchCount = ReadU32OrThrow(reader);
    value.nonFiniteCount = ReadU32OrThrow(reader);
    value.homogeneousCoordinateMismatchCount = ReadU32OrThrow(reader);
    value.maxAbsoluteError = ReadDoubleOrThrow(reader);
    value.maxRelativeError = ReadDoubleOrThrow(reader);
    value.maxUlpError = ReadU32OrThrow(reader);
    value.outputDigest = ReadDigestOrThrow(reader);
    return value;
}

std::vector<std::byte> SerializePayload(const MeasurementEvidenceV1& evidence)
{
    ValidateConfig(evidence.config);
    ValidateOrders();
    Require(evidence.schemaVersion == MeasurementEvidenceSchemaVersionV1, "Evidence schema version is invalid.");
    Require(evidence.cases.size() == CanonicalPatternCountV1 * CanonicalMeasurementCardinalityCountV1,
        "Evidence case count is not canonical.");

    base::BinaryWriter writer;
    WriteString(writer, EvidenceMagic);
    writer.WriteU32(evidence.schemaVersion);
    writer.WriteU64(evidence.captureUnixNanoseconds);
    WriteConfig(writer, evidence.config);
    WriteAdapter(writer, evidence.adapter);
    WriteDigest(writer, evidence.architectureEvidenceIdentity);
    WriteDigest(writer, evidence.controlledRecoveryEvidenceIdentity);
    WriteDigest(writer, evidence.freshRematerializationEvidenceIdentity);
    WriteDigest(writer, evidence.semanticIdentity);
    WriteDigest(writer, evidence.verificationContextIdentity);
    WriteDigest(writer, evidence.measurementProfileIdentity);
    WriteDigest(writer, evidence.orderBalanceIdentity);
    writer.WriteU32(CanonicalCandidateCountV1);
    for (const auto& candidate : evidence.candidates) WriteCandidateProfile(writer, candidate);
    writer.WriteU32(static_cast<std::uint32_t>(evidence.cases.size()));
    for (const auto& measurementCase : evidence.cases)
    {
        writer.WriteU32(static_cast<std::uint32_t>(measurementCase.pattern));
        writer.WriteU32(measurementCase.activeCount);
        writer.WriteU32(measurementCase.activeBlockCount);
        WriteDigest(writer, measurementCase.sparseSetIdentity);
        for (const auto& authority : measurementCase.authorities) WriteAuthority(writer, authority);
        writer.WriteU32(static_cast<std::uint32_t>(measurementCase.samples.size()));
        for (const auto& sample : measurementCase.samples) WriteSample(writer, sample);
    }
    return std::move(writer).Take();
}

MeasurementEvidenceV1 ParsePayloadOrThrow(std::span<const std::byte> bytes)
{
    base::BinaryReader reader(bytes);
    Require(ReadStringOrThrow(reader, 256) == EvidenceMagic, "Measurement evidence magic mismatch.");
    MeasurementEvidenceV1 evidence;
    evidence.schemaVersion = ReadU32OrThrow(reader);
    Require(evidence.schemaVersion == MeasurementEvidenceSchemaVersionV1, "Measurement evidence schema mismatch.");
    evidence.captureUnixNanoseconds = ReadU64OrThrow(reader);
    evidence.config = ReadConfigOrThrow(reader);
    evidence.adapter = ReadAdapterOrThrow(reader);
    evidence.architectureEvidenceIdentity = ReadDigestOrThrow(reader);
    evidence.controlledRecoveryEvidenceIdentity = ReadDigestOrThrow(reader);
    evidence.freshRematerializationEvidenceIdentity = ReadDigestOrThrow(reader);
    evidence.semanticIdentity = ReadDigestOrThrow(reader);
    evidence.verificationContextIdentity = ReadDigestOrThrow(reader);
    evidence.measurementProfileIdentity = ReadDigestOrThrow(reader);
    evidence.orderBalanceIdentity = ReadDigestOrThrow(reader);
    Require(ReadU32OrThrow(reader) == CanonicalCandidateCountV1, "Candidate profile count mismatch.");
    for (auto& candidate : evidence.candidates) candidate = ReadCandidateProfileOrThrow(reader);
    const std::uint32_t caseCount = ReadU32OrThrow(reader);
    Require(caseCount == CanonicalPatternCountV1 * CanonicalMeasurementCardinalityCountV1,
        "Measurement case count mismatch.");
    evidence.cases.reserve(caseCount);
    for (std::uint32_t caseIndex = 0; caseIndex < caseCount; ++caseIndex)
    {
        MeasurementCaseV1 measurementCase;
        measurementCase.pattern = PatternFromU32(ReadU32OrThrow(reader));
        measurementCase.activeCount = ReadU32OrThrow(reader);
        measurementCase.activeBlockCount = ReadU32OrThrow(reader);
        measurementCase.sparseSetIdentity = ReadDigestOrThrow(reader);
        for (auto& authority : measurementCase.authorities) authority = ReadAuthorityOrThrow(reader);
        const std::uint32_t sampleCount = ReadU32OrThrow(reader);
        Require(sampleCount <= 1'000'000u, "Sample count exceeds evidence limit.");
        measurementCase.samples.reserve(sampleCount);
        for (std::uint32_t sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
            measurementCase.samples.push_back(ReadSampleOrThrow(reader));
        evidence.cases.push_back(std::move(measurementCase));
    }
    Require(reader.Remaining() == 0, "Measurement evidence contains trailing payload bytes.");
    Require(evidence.architectureEvidenceIdentity == DigestFromHex(ArchitectureEvidenceHex),
        "Measurement evidence Architecture predecessor mismatch.");
    Require(evidence.controlledRecoveryEvidenceIdentity == DigestFromHex(ControlledRecoveryEvidenceHex),
        "Measurement evidence Controlled Recovery predecessor mismatch.");
    Require(evidence.freshRematerializationEvidenceIdentity == DigestFromHex(FreshRematerializationEvidenceHex),
        "Measurement evidence fresh-rematerialization predecessor mismatch.");
    Require(evidence.measurementProfileIdentity == BuildMeasurementProfileIdentityV1(evidence.config),
        "Measurement profile identity mismatch.");
    Require(evidence.orderBalanceIdentity == BuildOrderBalanceIdentityV1(), "Order-balance identity mismatch.");
    return evidence;
}

double Median(std::vector<double> values)
{
    Require(!values.empty(), "Median requires at least one value.");
    std::sort(values.begin(), values.end());
    const std::size_t middle = values.size() / 2;
    return values.size() % 2 == 0 ? (values[middle - 1] + values[middle]) * 0.5 : values[middle];
}

double NoiseThreshold(double a, double b, const MeasurementConfigV1& config)
{
    return std::max(config.absoluteNoiseFloorNanoseconds, config.relativeNoiseFloor * std::min(a, b));
}

PairWinnerV1 Compare(double left, double right, const MeasurementConfigV1& config)
{
    if (std::abs(left - right) <= NoiseThreshold(left, right, config)) return PairWinnerV1::NoiseEquivalent;
    return left < right ? PairWinnerV1::Left : PairWinnerV1::Right;
}

std::pair<TransitionClassificationV1, std::vector<WinnerTransitionV1>> ClassifyTransitions(
    PatternKindV1 pattern,
    const std::vector<PerPatternCardinalityDecisionV1>& cases,
    bool denseCompact)
{
    std::optional<PairWinnerV1> lastNonNoise;
    std::uint32_t lastCount = 0;
    bool sawNoise = false;
    std::vector<WinnerTransitionV1> transitions;
    for (const auto& value : cases)
    {
        const PairWinnerV1 winner = denseCompact ? value.denseVersusCompact : value.compactVersusActiveBlock;
        if (winner == PairWinnerV1::NoiseEquivalent)
        {
            sawNoise = true;
            continue;
        }
        if (lastNonNoise && *lastNonNoise != winner)
        {
            WinnerTransitionV1 transition;
            transition.pattern = pattern;
            transition.lowerActiveCount = lastCount;
            transition.upperActiveCount = value.activeCount;
            if (denseCompact)
            {
                transition.from = *lastNonNoise == PairWinnerV1::Left
                    ? CandidateKindV1::DenseMembershipMaskPredicate
                    : CandidateKindV1::CompactIndexListDispatch;
                transition.to = winner == PairWinnerV1::Left
                    ? CandidateKindV1::DenseMembershipMaskPredicate
                    : CandidateKindV1::CompactIndexListDispatch;
            }
            else
            {
                transition.from = *lastNonNoise == PairWinnerV1::Left
                    ? CandidateKindV1::CompactIndexListDispatch
                    : CandidateKindV1::ActiveBlockListLocalMask;
                transition.to = winner == PairWinnerV1::Left
                    ? CandidateKindV1::CompactIndexListDispatch
                    : CandidateKindV1::ActiveBlockListLocalMask;
            }
            transitions.push_back(transition);
        }
        lastNonNoise = winner;
        lastCount = value.activeCount;
    }
    if (!lastNonNoise || (sawNoise && transitions.empty()))
        return {TransitionClassificationV1::NoiseEquivalentOrUnresolved, transitions};
    if (transitions.empty())
        return {TransitionClassificationV1::NoObservedWinnerTransition, transitions};
    if (transitions.size() == 1)
        return {TransitionClassificationV1::SingleStableWinnerTransition, transitions};
    return {TransitionClassificationV1::MultipleWinnerTransitions, transitions};
}

std::string WinnerSetText(const std::vector<CandidateKindV1>& values)
{
    std::string result;
    for (std::size_t i = 0; i < values.size(); ++i)
    {
        if (i != 0) result += ',';
        result += CandidateLetterV1(values[i]);
    }
    return result;
}

CandidateProfileV1 SyntheticCandidateProfile(CandidateKindV1 kind)
{
    CandidateProfileV1 value;
    value.kind = kind;
    switch (kind)
    {
    case CandidateKindV1::DenseMembershipMaskPredicate:
        value.representationRole = fc::SparseFamilyRepresentationRoleV1::DenseMembershipMask4096;
        value.dispatchPolicy = fc::SparseFamilyDispatchPolicyV1::FixedUniverseGroups;
        value.payloadStrideBytes = 4;
        value.fixedCapacityBytes = fc::DenseMaskCapacityBytesV1;
        break;
    case CandidateKindV1::CompactIndexListDispatch:
        value.representationRole = fc::SparseFamilyRepresentationRoleV1::CompactSortedUniverseIndexList;
        value.dispatchPolicy = fc::SparseFamilyDispatchPolicyV1::CeilCardinalityGroups;
        value.payloadStrideBytes = 4;
        value.fixedCapacityBytes = fc::CompactIndexCapacityBytesV1;
        break;
    case CandidateKindV1::ActiveBlockListLocalMask:
        value.representationRole = fc::SparseFamilyRepresentationRoleV1::ActiveBlockListWithLocal64BitMask;
        value.dispatchPolicy = fc::SparseFamilyDispatchPolicyV1::OneGroupPerActiveBlock;
        value.payloadStrideBytes = fc::ActiveBlockRecordStrideBytesV1;
        value.fixedCapacityBytes = fc::ActiveBlockCapacityBytesV1;
        break;
    }
    const std::string name = CandidateNameV1(kind);
    value.targetProfileIdentity = LiteralIdentity("Synthetic.Target." + name);
    value.observationContractIdentity = LiteralIdentity("Synthetic.Observation." + name);
    value.completionContractIdentity = LiteralIdentity("Synthetic.Completion." + name);
    value.consumerProgramIdentity = LiteralIdentity("Synthetic.Consumer." + name);
    value.representationResourceContractIdentity = LiteralIdentity("Synthetic.Resource." + name);
    value.representationShaderBytecodeDigest = LiteralIdentity("Synthetic.Shader." + name);
    value.argumentProducerShaderBytecodeDigest = LiteralIdentity("Synthetic.Arguments." + name);
    return value;
}

std::uint32_t SyntheticActiveBlockCount(PatternKindV1 pattern, std::uint32_t activeCount)
{
    if (activeCount == 0) return 0;
    switch (pattern)
    {
    case PatternKindV1::PrefixControl:
    case PatternKindV1::BlockClusteredPermutation:
        return 1u + ((activeCount - 1u) / semantic::ThreadsPerGroupV1);
    case PatternKindV1::UniformStride:
    case PatternKindV1::HashScatterPermutation:
        return std::min<std::uint32_t>(64u, activeCount);
    }
    return 0;
}

MeasurementEvidenceV1 BuildSyntheticEvidence()
{
    MeasurementEvidenceV1 evidence;
    evidence.captureUnixNanoseconds = 0;
    evidence.config = {};
    evidence.adapter.description = "Synthetic deterministic adapter";
    evidence.adapter.timestampFrequency = 1'000'000'000ull;
    evidence.adapter.fingerprint = LiteralIdentity("Synthetic.Spiral6.Adapter.V1");
    evidence.architectureEvidenceIdentity = DigestFromHex(ArchitectureEvidenceHex);
    evidence.controlledRecoveryEvidenceIdentity = DigestFromHex(ControlledRecoveryEvidenceHex);
    evidence.freshRematerializationEvidenceIdentity = DigestFromHex(FreshRematerializationEvidenceHex);
    const auto semanticValue = semantic::BuildCanonicalSparsePointTransformSemanticV1();
    evidence.semanticIdentity = semantic::SparsePointTransformSemanticIdentityV1(semanticValue);
    const auto context = fv::BuildCanonicalSparseFamilyVerificationContextV1();
    base::BinaryWriter contextWriter;
    WriteDigest(contextWriter, context.targetProfileIdentity);
    WriteDigest(contextWriter, context.observationContractIdentity);
    WriteDigest(contextWriter, context.completionContractIdentity);
    WriteDigest(contextWriter, context.deviceEpochPolicyIdentity);
    WriteDigest(contextWriter, context.spiral4IndirectArtifactIdentity);
    evidence.verificationContextIdentity = base::Sha256(contextWriter.Bytes());
    evidence.measurementProfileIdentity = BuildMeasurementProfileIdentityV1(evidence.config);
    evidence.orderBalanceIdentity = BuildOrderBalanceIdentityV1();
    const auto kinds = CanonicalCandidateKindsV1();
    for (std::size_t i = 0; i < kinds.size(); ++i) evidence.candidates[i] = SyntheticCandidateProfile(kinds[i]);

    for (PatternKindV1 pattern : CanonicalPatternsV1())
    {
        for (std::uint32_t activeCount : CanonicalMeasurementCardinalitiesV1())
        {
            MeasurementCaseV1 measurementCase;
            measurementCase.pattern = pattern;
            measurementCase.activeCount = activeCount;
            measurementCase.activeBlockCount = SyntheticActiveBlockCount(pattern, activeCount);
            measurementCase.sparseSetIdentity = LiteralIdentity(
                "Synthetic.Set." + PatternNameV1(pattern) + "." + std::to_string(activeCount));
            for (std::size_t i = 0; i < kinds.size(); ++i)
            {
                auto& authority = measurementCase.authorities[i];
                authority.kind = kinds[i];
                const std::string prefix = "Synthetic.Authority." + PatternNameV1(pattern) + "." +
                    std::to_string(activeCount) + "." + CandidateNameV1(kinds[i]);
                authority.rawCandidateIdentity = LiteralIdentity(prefix + ".Raw");
                authority.verifiedPlanIdentity = LiteralIdentity(prefix + ".Plan");
                authority.verifierCertificateIdentity = LiteralIdentity(prefix + ".Certificate");
                authority.artifactIdentity = LiteralIdentity(prefix + ".Artifact");
                authority.representationResourceIdentity = LiteralIdentity(prefix + ".Resource");
                authority.frozenBindingIdentity = LiteralIdentity(prefix + ".Binding");
            }
            for (std::uint32_t run = 0; run < evidence.config.runCount; ++run)
            {
                for (std::uint32_t orderIndex = 0; orderIndex < CanonicalOrderCountV1; ++orderIndex)
                {
                    const auto order = CanonicalBalancedOrdersV1()[orderIndex];
                    for (std::uint32_t cycle = 0; cycle < evidence.config.measurementCyclesPerOrder; ++cycle)
                    {
                        for (std::uint32_t position = 0; position < CanonicalCandidateCountV1; ++position)
                        {
                            const CandidateKindV1 kind = kinds[order[position]];
                            const double k = static_cast<double>(activeCount);
                            const bool clustered = pattern == PatternKindV1::PrefixControl ||
                                pattern == PatternKindV1::BlockClusteredPermutation;
                            double candidatePath = 0.0;
                            if (kind == CandidateKindV1::DenseMembershipMaskPredicate)
                                candidatePath = 900.0 + 0.20 * k;
                            else if (kind == CandidateKindV1::CompactIndexListDispatch)
                                candidatePath = 120.0 + 0.62 * k;
                            else
                                candidatePath = clustered ? 150.0 + 0.43 * k : 320.0 + 0.70 * k;
                            candidatePath += static_cast<double>(run * 7 + orderIndex * 3 + cycle * 2 + position);
                            TimingSampleV1 sample;
                            sample.run = run;
                            sample.orderIndex = orderIndex;
                            sample.cycle = cycle;
                            sample.orderPosition = position;
                            sample.candidate = kind;
                            sample.pattern = pattern;
                            sample.activeCount = activeCount;
                            sample.activeBlockCount = measurementCase.activeBlockCount;
                            sample.sparseInputValidationNanoseconds = 10.0;
                            sample.derivedRepresentationConstructionUploadNanoseconds =
                                kind == CandidateKindV1::DenseMembershipMaskPredicate ? 50.0 :
                                kind == CandidateKindV1::CompactIndexListDispatch ? 100.0 : 75.0;
                            sample.commandRecordingNanoseconds = 20.0;
                            sample.submitAndWaitNanoseconds = 30.0;
                            sample.numericObservationNanoseconds = 40.0;
                            sample.cpuEndToEndNanoseconds = candidatePath + 250.0;
                            sample.gpuSentinelInitializationNanoseconds = 80.0;
                            sample.gpuIndirectArgumentGenerationNanoseconds = 40.0;
                            sample.gpuSparseConsumerExecutionNanoseconds = candidatePath - 160.0;
                            sample.gpuCompletionNanoseconds = 40.0;
                            sample.gpuObservationCopyNanoseconds = 30.0;
                            sample.gpuCandidatePathNanoseconds = candidatePath;
                            sample.gpuTotalNanoseconds = candidatePath + 30.0;
                            sample.expectedDispatchGroupsX = kind == CandidateKindV1::DenseMembershipMaskPredicate ? 64u :
                                kind == CandidateKindV1::CompactIndexListDispatch ? 1u + ((activeCount - 1u) / 64u) :
                                measurementCase.activeBlockCount;
                            sample.observedDispatchGroupsX = sample.expectedDispatchGroupsX;
                            sample.activeWriteCount = activeCount;
                            sample.outputDigest = measurementCase.sparseSetIdentity;
                            measurementCase.samples.push_back(sample);
                        }
                    }
                }
            }
            evidence.cases.push_back(std::move(measurementCase));
        }
    }
    return evidence;
}
}

std::string CandidateNameV1(CandidateKindV1 value)
{
    switch (value)
    {
    case CandidateKindV1::DenseMembershipMaskPredicate: return "DenseMembershipMaskPredicate";
    case CandidateKindV1::CompactIndexListDispatch: return "CompactIndexListDispatch";
    case CandidateKindV1::ActiveBlockListLocalMask: return "ActiveBlockListLocalMask";
    }
    return "Unknown";
}

char CandidateLetterV1(CandidateKindV1 value)
{
    switch (value)
    {
    case CandidateKindV1::DenseMembershipMaskPredicate: return 'A';
    case CandidateKindV1::CompactIndexListDispatch: return 'B';
    case CandidateKindV1::ActiveBlockListLocalMask: return 'C';
    }
    return '?';
}

std::string PatternNameV1(PatternKindV1 value)
{
    return semantic::SparsePatternNameV1(value);
}

std::array<CandidateKindV1, CanonicalCandidateCountV1> CanonicalCandidateKindsV1()
{
    return {
        CandidateKindV1::DenseMembershipMaskPredicate,
        CandidateKindV1::CompactIndexListDispatch,
        CandidateKindV1::ActiveBlockListLocalMask};
}

std::array<PatternKindV1, CanonicalPatternCountV1> CanonicalPatternsV1()
{
    return {
        PatternKindV1::PrefixControl,
        PatternKindV1::UniformStride,
        PatternKindV1::BlockClusteredPermutation,
        PatternKindV1::HashScatterPermutation};
}

const std::array<std::uint32_t, CanonicalMeasurementCardinalityCountV1>&
CanonicalMeasurementCardinalitiesV1() noexcept
{
    static constexpr std::array<std::uint32_t, CanonicalMeasurementCardinalityCountV1> values{
        1u, 8u, 32u, 64u, 128u, 256u, 512u, 1024u, 2048u, 3072u, 4096u};
    return values;
}

std::vector<std::array<std::uint32_t, CanonicalCandidateCountV1>>
CanonicalBalancedOrdersV1()
{
    return {
        {0u,1u,2u}, {0u,2u,1u}, {1u,0u,2u},
        {1u,2u,0u}, {2u,0u,1u}, {2u,1u,0u}};
}

base::Digest256 BuildMeasurementProfileIdentityV1(const MeasurementConfigV1& config)
{
    ValidateConfig(config);
    base::BinaryWriter writer;
    WriteDigest(writer, LiteralIdentity(MeasurementProfileDomain));
    WriteConfig(writer, config);
    writer.WriteU32(CanonicalCandidateCountV1);
    writer.WriteU32(CanonicalPatternCountV1);
    writer.WriteU32(CanonicalMeasurementCardinalityCountV1);
    for (std::uint32_t value : CanonicalMeasurementCardinalitiesV1()) writer.WriteU32(value);
    return base::Sha256(writer.Bytes());
}

base::Digest256 BuildOrderBalanceIdentityV1()
{
    ValidateOrders();
    base::BinaryWriter writer;
    WriteDigest(writer, LiteralIdentity(OrderBalanceDomain));
    const auto orders = CanonicalBalancedOrdersV1();
    writer.WriteU32(static_cast<std::uint32_t>(orders.size()));
    for (const auto& order : orders)
        for (std::uint32_t index : order) writer.WriteU32(index);
    return base::Sha256(writer.Bytes());
}

std::vector<std::byte> SerializeMeasurementEvidenceV1(const MeasurementEvidenceV1& evidence)
{
    auto payload = SerializePayload(evidence);
    const auto digest = base::Sha256(payload);
    payload.insert(payload.end(), digest.begin(), digest.end());
    return payload;
}

base::Result<MeasurementEvidenceV1, std::string>
ReadMeasurementEvidenceV1(const std::filesystem::path& evidencePath)
{
    try
    {
        auto bytesResult = base::ReadAllBytes(evidencePath);
        if (!bytesResult) return base::Result<MeasurementEvidenceV1, std::string>::Failure(bytesResult.Error());
        const auto& bytes = bytesResult.Value();
        Require(bytes.size() >= 32, "Measurement evidence is shorter than its checksum.");
        const std::span<const std::byte> payload(bytes.data(), bytes.size() - 32);
        base::Digest256 stored{};
        std::memcpy(stored.data(), bytes.data() + bytes.size() - 32, 32);
        Require(base::Sha256(payload) == stored, "Measurement evidence checksum mismatch.");
        return base::Result<MeasurementEvidenceV1, std::string>::Success(ParsePayloadOrThrow(payload));
    }
    catch (const std::exception& error)
    {
        return base::Result<MeasurementEvidenceV1, std::string>::Failure(error.what());
    }
}

base::Result<void, std::string> WriteMeasurementArtifactsV1(
    const MeasurementEvidenceV1& evidence,
    const std::filesystem::path& outputDirectory)
{
    try
    {
        std::filesystem::create_directories(outputDirectory);
        const auto bytes = SerializeMeasurementEvidenceV1(evidence);
        auto written = base::WriteAllBytes(outputDirectory / "spiral6_measurement_evidence_v1.bin", bytes);
        if (!written) return written;

        std::ofstream csv(outputDirectory / "spiral6_measurement_samples_v1.csv", std::ios::binary);
        Require(static_cast<bool>(csv), "Could not create measurement CSV.");
        csv << "pattern,K,activeBlockCount,run,orderIndex,cycle,orderPosition,candidate,"
               "sparseInputValidationNs,derivedRepresentationConstructionUploadNs,commandRecordingNs,submitAndWaitNs,"
               "numericObservationNs,cpuEndToEndNs,gpuSentinelInitializationNs,gpuIndirectArgumentGenerationNs,"
               "gpuSparseConsumerExecutionNs,gpuCompletionNs,gpuObservationCopyNs,gpuCandidatePathNs,gpuTotalNs,"
               "expectedDispatchGroupsX,observedDispatchGroupsX,activeWriteCount,missingWriteCount,unauthorizedWriteCount,"
               "activeMismatchCount,nonFiniteCount,homogeneousMismatchCount,maxAbsoluteError,maxRelativeError,maxUlpError,outputDigest\n";
        csv << std::setprecision(17);
        for (const auto& measurementCase : evidence.cases)
        {
            for (const auto& sample : measurementCase.samples)
            {
                csv << PatternNameV1(sample.pattern) << ',' << sample.activeCount << ',' << sample.activeBlockCount << ','
                    << sample.run << ',' << sample.orderIndex << ',' << sample.cycle << ',' << sample.orderPosition << ','
                    << CandidateLetterV1(sample.candidate) << ','
                    << sample.sparseInputValidationNanoseconds << ','
                    << sample.derivedRepresentationConstructionUploadNanoseconds << ','
                    << sample.commandRecordingNanoseconds << ',' << sample.submitAndWaitNanoseconds << ','
                    << sample.numericObservationNanoseconds << ',' << sample.cpuEndToEndNanoseconds << ','
                    << sample.gpuSentinelInitializationNanoseconds << ','
                    << sample.gpuIndirectArgumentGenerationNanoseconds << ','
                    << sample.gpuSparseConsumerExecutionNanoseconds << ',' << sample.gpuCompletionNanoseconds << ','
                    << sample.gpuObservationCopyNanoseconds << ',' << sample.gpuCandidatePathNanoseconds << ','
                    << sample.gpuTotalNanoseconds << ',' << sample.expectedDispatchGroupsX << ','
                    << sample.observedDispatchGroupsX << ',' << sample.activeWriteCount << ',' << sample.missingWriteCount << ','
                    << sample.unauthorizedWriteCount << ',' << sample.activeMismatchCount << ',' << sample.nonFiniteCount << ','
                    << sample.homogeneousCoordinateMismatchCount << ',' << sample.maxAbsoluteError << ','
                    << sample.maxRelativeError << ',' << sample.maxUlpError << ',' << base::ToHex(sample.outputDigest) << '\n';
            }
        }
        csv.close();
        Require(static_cast<bool>(csv), "Could not finish measurement CSV.");

        auto analysisResult = AnalyzeDecisionEvidenceV1(evidence);
        if (!analysisResult) return base::Result<void, std::string>::Failure(analysisResult.Error());
        const auto& analysis = analysisResult.Value();
        std::ofstream json(outputDirectory / "spiral6_measurement_summary_v1.json", std::ios::binary);
        Require(static_cast<bool>(json), "Could not create measurement summary JSON.");
        json << "{\n"
             << "  \"schema\": \"SGE4-5.Spiral6.MeasurementSummary.V1\",\n"
             << "  \"adapter\": \"" << JsonEscape(evidence.adapter.description) << "\",\n"
             << "  \"primaryMetric\": \"GpuCandidatePathNanoseconds\",\n"
             << "  \"sameCardinalityWinnerDependsOnPattern\": "
             << (analysis.sameCardinalityWinnerDependsOnPattern ? "true" : "false") << ",\n"
             << "  \"cases\": [\n";
        for (std::size_t i = 0; i < analysis.cases.size(); ++i)
        {
            const auto& value = analysis.cases[i];
            json << "    {\"pattern\":\"" << PatternNameV1(value.pattern) << "\",\"K\":" << value.activeCount
                 << ",\"activeBlockCount\":" << value.activeBlockCount
                 << ",\"winners\":\"" << WinnerSetText(value.globalWinnerSet) << "\",\"medianNs\":["
                 << std::setprecision(17)
                 << value.medianGpuCandidatePathNanoseconds[0] << ','
                 << value.medianGpuCandidatePathNanoseconds[1] << ','
                 << value.medianGpuCandidatePathNanoseconds[2] << "]}";
            if (i + 1 != analysis.cases.size()) json << ',';
            json << '\n';
        }
        json << "  ]\n}\n";
        json.close();
        Require(static_cast<bool>(json), "Could not finish measurement summary JSON.");
        return base::Result<void, std::string>::Success();
    }
    catch (const std::exception& error)
    {
        return base::Result<void, std::string>::Failure(error.what());
    }
}

base::Result<DecisionAnalysisV1, std::string>
AnalyzeDecisionEvidenceV1(const MeasurementEvidenceV1& evidence)
{
    try
    {
        ValidateConfig(evidence.config);
        Require(evidence.cases.size() == CanonicalPatternCountV1 * CanonicalMeasurementCardinalityCountV1,
            "Decision analysis requires all canonical cases.");
        DecisionAnalysisV1 analysis;
        std::set<CandidateKindV1> overallWinners;
        std::optional<std::vector<CandidateKindV1>> previousWinnerSet;

        for (const auto& measurementCase : evidence.cases)
        {
            PerPatternCardinalityDecisionV1 decision;
            decision.pattern = measurementCase.pattern;
            decision.activeCount = measurementCase.activeCount;
            decision.activeBlockCount = measurementCase.activeBlockCount;
            const auto kinds = CanonicalCandidateKindsV1();
            for (std::size_t candidateIndex = 0; candidateIndex < kinds.size(); ++candidateIndex)
            {
                std::vector<double> paths;
                std::vector<double> constructions;
                for (const auto& sample : measurementCase.samples)
                {
                    if (sample.candidate == kinds[candidateIndex])
                    {
                        Require(sample.pattern == measurementCase.pattern && sample.activeCount == measurementCase.activeCount,
                            "Sample does not belong to its measurement case.");
                        Require(sample.activeMismatchCount == 0 && sample.missingWriteCount == 0 &&
                            sample.unauthorizedWriteCount == 0 && sample.nonFiniteCount == 0 &&
                            sample.homogeneousCoordinateMismatchCount == 0,
                            "Decision evidence contains a failed numeric or write-set observation.");
                        paths.push_back(sample.gpuCandidatePathNanoseconds);
                        constructions.push_back(sample.derivedRepresentationConstructionUploadNanoseconds);
                    }
                }
                const std::size_t expectedSamples = static_cast<std::size_t>(CanonicalOrderCountV1) *
                    evidence.config.measurementCyclesPerOrder * evidence.config.runCount;
                Require(paths.size() == expectedSamples, "Candidate sample count is not canonical.");
                decision.medianGpuCandidatePathNanoseconds[candidateIndex] = Median(paths);
                decision.medianGpuRepresentationConstructionUploadNanoseconds[candidateIndex] = Median(constructions);
            }
            const double fastest = *std::min_element(
                decision.medianGpuCandidatePathNanoseconds.begin(), decision.medianGpuCandidatePathNanoseconds.end());
            for (std::size_t i = 0; i < kinds.size(); ++i)
            {
                const double value = decision.medianGpuCandidatePathNanoseconds[i];
                if (value - fastest <= NoiseThreshold(value, fastest, evidence.config))
                {
                    decision.globalWinnerSet.push_back(kinds[i]);
                    overallWinners.insert(kinds[i]);
                }
            }
            decision.denseVersusCompact = Compare(
                decision.medianGpuCandidatePathNanoseconds[0],
                decision.medianGpuCandidatePathNanoseconds[1], evidence.config);
            decision.compactVersusActiveBlock = Compare(
                decision.medianGpuCandidatePathNanoseconds[1],
                decision.medianGpuCandidatePathNanoseconds[2], evidence.config);
            if (previousWinnerSet && *previousWinnerSet != decision.globalWinnerSet)
                analysis.winnerChangesAcrossCardinality = true;
            previousWinnerSet = decision.globalWinnerSet;
            analysis.cases.push_back(std::move(decision));
        }

        const auto patterns = CanonicalPatternsV1();
        for (std::size_t patternIndex = 0; patternIndex < patterns.size(); ++patternIndex)
        {
            std::vector<PerPatternCardinalityDecisionV1> patternCases;
            for (const auto& value : analysis.cases)
                if (value.pattern == patterns[patternIndex]) patternCases.push_back(value);
            std::sort(patternCases.begin(), patternCases.end(),
                [](const auto& a, const auto& b) { return a.activeCount < b.activeCount; });
            Require(patternCases.size() == CanonicalMeasurementCardinalityCountV1,
                "Pattern decision corpus is incomplete.");
            auto denseCompact = ClassifyTransitions(patterns[patternIndex], patternCases, true);
            auto compactBlock = ClassifyTransitions(patterns[patternIndex], patternCases, false);
            auto& result = analysis.patternTransitions[patternIndex];
            result.pattern = patterns[patternIndex];
            result.denseCompactClassification = denseCompact.first;
            result.denseCompactTransitions = std::move(denseCompact.second);
            result.compactBlockClassification = compactBlock.first;
            result.compactBlockTransitions = std::move(compactBlock.second);
        }

        for (std::uint32_t k : CanonicalMeasurementCardinalitiesV1())
        {
            std::optional<std::vector<CandidateKindV1>> winnerSet;
            for (const auto& value : analysis.cases)
            {
                if (value.activeCount != k) continue;
                if (!winnerSet) winnerSet = value.globalWinnerSet;
                else if (*winnerSet != value.globalWinnerSet) analysis.sameCardinalityWinnerDependsOnPattern = true;
            }
        }
        analysis.overallObservedWinnerSet.assign(overallWinners.begin(), overallWinners.end());
        return base::Result<DecisionAnalysisV1, std::string>::Success(std::move(analysis));
    }
    catch (const std::exception& error)
    {
        return base::Result<DecisionAnalysisV1, std::string>::Failure(error.what());
    }
}

base::Result<void, std::string> WriteDecisionEvidenceReportV1(
    const MeasurementEvidenceV1& evidence,
    const std::filesystem::path& outputDirectory)
{
    try
    {
        auto analysisResult = AnalyzeDecisionEvidenceV1(evidence);
        if (!analysisResult) return base::Result<void, std::string>::Failure(analysisResult.Error());
        const auto& analysis = analysisResult.Value();
        std::filesystem::create_directories(outputDirectory);
        std::ofstream report(outputDirectory / "SPIRAL6_DECISION_EVIDENCE_REPORT.md", std::ios::binary);
        Require(static_cast<bool>(report), "Could not create Decision Evidence Report.");
        report << "# Spiral 6 CU6 Decision Evidence Report\n\n"
               << "- Adapter: `" << evidence.adapter.description << "`\n"
               << "- Primary metric: `GpuCandidatePathNanoseconds`\n"
               << "- Samples per Candidate and `(Pattern,K)`: `"
               << CanonicalOrderCountV1 * evidence.config.measurementCyclesPerOrder * evidence.config.runCount << "`\n"
               << "- RuntimePolicyAuthorization = None\n"
               << "- NextCapabilitySelection = DeferredByOwner\n"
               << "- SelectionStatus = OWNER_DECISION_REQUIRED\n\n"
               << "Performance superiority and crossover are not completion gates. No observed crossover is a valid result.\n\n"
               << "## Per-(Pattern,K) ranking\n\n"
               << "| Pattern | K | Active blocks | Winner set | A Dense ns | B Compact ns | C Active-block ns | A/B | B/C |\n"
               << "|---|---:|---:|---|---:|---:|---:|---|---|\n";
        report << std::setprecision(17);
        for (const auto& value : analysis.cases)
        {
            report << "| " << PatternNameV1(value.pattern) << " | " << value.activeCount << " | "
                   << value.activeBlockCount << " | " << WinnerSetText(value.globalWinnerSet) << " | "
                   << value.medianGpuCandidatePathNanoseconds[0] << " | "
                   << value.medianGpuCandidatePathNanoseconds[1] << " | "
                   << value.medianGpuCandidatePathNanoseconds[2] << " | "
                   << PairWinnerName(value.denseVersusCompact) << " | "
                   << PairWinnerName(value.compactVersusActiveBlock) << " |\n";
        }
        report << "\n## Pattern-specific transition classification\n\n";
        for (const auto& value : analysis.patternTransitions)
        {
            report << "### " << PatternNameV1(value.pattern) << "\n\n"
                   << "- A.Dense versus B.Compact: `" << TransitionName(value.denseCompactClassification) << "`\n"
                   << "- B.Compact versus C.ActiveBlock: `" << TransitionName(value.compactBlockClassification) << "`\n";
            for (const auto& transition : value.denseCompactTransitions)
                report << "- A/B transition K=" << transition.lowerActiveCount << ".." << transition.upperActiveCount
                       << ": " << CandidateLetterV1(transition.from) << " -> " << CandidateLetterV1(transition.to) << "\n";
            for (const auto& transition : value.compactBlockTransitions)
                report << "- B/C transition K=" << transition.lowerActiveCount << ".." << transition.upperActiveCount
                       << ": " << CandidateLetterV1(transition.from) << " -> " << CandidateLetterV1(transition.to) << "\n";
            report << '\n';
        }
        report << "## Sparse granularity interpretation\n\n"
               << "- Same-cardinality winner depends on Pattern: `"
               << (analysis.sameCardinalityWinnerDependsOnPattern ? "true" : "false") << "`\n"
               << "- Winner changes somewhere in the `(Pattern,K)` corpus: `"
               << (analysis.winnerChangesAcrossCardinality ? "true" : "false") << "`\n"
               << "- Overall observed winner set: `" << WinnerSetText(analysis.overallObservedWinnerSet) << "`\n\n"
               << "The report is descriptive evidence for one adapter and one Frozen Sparse experiment. It does not authorize Runtime to infer Pattern, inspect K, or select A/B/C.\n\n"
               << "CU6 pass declares `SGE4-5 SPIRAL 6 EXPERIMENT COMPLETE`. `SPIRAL 6 CLOSED` remains withheld until the Owner selects the next capability or explicitly closes the research line.\n";
        report.close();
        Require(static_cast<bool>(report), "Could not finish Decision Evidence Report.");
        return base::Result<void, std::string>::Success();
    }
    catch (const std::exception& error)
    {
        return base::Result<void, std::string>::Failure(error.what());
    }
}

base::Result<void, std::string> RunFormatAndDecisionSelfTestV1(
    const std::filesystem::path& outputDirectory)
{
    try
    {
        ValidateOrders();
        auto evidence = BuildSyntheticEvidence();
        auto written = WriteMeasurementArtifactsV1(evidence, outputDirectory);
        if (!written) return written;
        auto report = WriteDecisionEvidenceReportV1(evidence, outputDirectory);
        if (!report) return report;
        auto read = ReadMeasurementEvidenceV1(outputDirectory / "spiral6_measurement_evidence_v1.bin");
        if (!read) return base::Result<void, std::string>::Failure(read.Error());
        Require(SerializeMeasurementEvidenceV1(read.Value()) == SerializeMeasurementEvidenceV1(evidence),
            "Measurement evidence round-trip is not byte-identical.");
        auto analysis = AnalyzeDecisionEvidenceV1(read.Value());
        if (!analysis) return base::Result<void, std::string>::Failure(analysis.Error());
        Require(analysis.Value().sameCardinalityWinnerDependsOnPattern,
            "Synthetic decision evidence did not prove Pattern-sensitive winner selection.");
        bool sawDenseCompactTransition = false;
        bool sawCompactBlockDistinction = false;
        for (const auto& value : analysis.Value().patternTransitions)
        {
            if (value.denseCompactClassification == TransitionClassificationV1::SingleStableWinnerTransition)
                sawDenseCompactTransition = true;
            if (value.compactBlockClassification == TransitionClassificationV1::NoObservedWinnerTransition ||
                value.compactBlockClassification == TransitionClassificationV1::SingleStableWinnerTransition)
                sawCompactBlockDistinction = true;
        }
        Require(sawDenseCompactTransition, "Synthetic evidence did not contain an A/B crossover.");
        Require(sawCompactBlockDistinction, "Synthetic evidence did not exercise B/C locality analysis.");

        auto bytes = base::ReadAllBytes(outputDirectory / "spiral6_measurement_evidence_v1.bin");
        if (!bytes) return base::Result<void, std::string>::Failure(bytes.Error());
        Require(bytes.Value().size() > 64, "Synthetic evidence is unexpectedly small.");
        auto corrupted = bytes.Value();
        corrupted[corrupted.size() / 2] ^= std::byte{0x01};
        const auto corruptPath = outputDirectory / "corrupted.bin";
        auto corruptWritten = base::WriteAllBytes(corruptPath, corrupted);
        if (!corruptWritten) return corruptWritten;
        auto corruptRead = ReadMeasurementEvidenceV1(corruptPath);
        Require(!corruptRead, "Corrupted measurement evidence was accepted.");
        std::filesystem::remove(corruptPath);
        return base::Result<void, std::string>::Success();
    }
    catch (const std::exception& error)
    {
        return base::Result<void, std::string>::Failure(error.what());
    }
}
}
