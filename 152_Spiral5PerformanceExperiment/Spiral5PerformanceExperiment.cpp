#include "Spiral5PerformanceExperiment.h"

#include "../00_Foundation/BinaryIO.h"
#include "../00_Foundation/FileIO.h"
#include "../148_TemporalCandidateFamilyPlanner/TemporalCandidateFamilyPlanner.h"
#include "../149_TemporalCandidateFamilyVerifier/TemporalCandidateFamilyVerifier.h"

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

namespace sge4_5::spiral5::performance
{
namespace
{
constexpr std::string_view EvidenceMagic =
    "SGE4-5.Spiral5.MeasurementEvidence.V1";
constexpr std::string_view MeasurementProfileDomain =
    "SGE4-5.Spiral5.MeasurementProfile.V1";
constexpr std::string_view OrderBalanceDomain =
    "SGE4-5.Spiral5.BalancedOrders.V1";
constexpr std::string_view ArchitectureEvidenceHex =
    "eb4937b1181e3377fd5367d02f218c682aa1f402a5c4099d8b5f452c103d84a1";
constexpr std::string_view ControlledRecoveryEvidenceHex =
    "6dfba1f98f1338be96e7985c7532a20a1b4144b81408bea0ffd8c03adca4db17";
constexpr std::string_view FreshRematerializationEvidenceHex =
    "6b0d6f9a617d257268c84aff523fba6d4f9a727a0ea130b956e7db992123300b";

void Require(bool condition, std::string_view message)
{
    if (!condition)
    {
        throw std::runtime_error(std::string(message));
    }
}

base::Digest256 LiteralIdentity(std::string_view value)
{
    return base::Sha256(std::as_bytes(std::span(value.data(), value.size())));
}

base::Digest256 DigestFromHex(std::string_view value)
{
    if (value.size() != 64)
    {
        throw std::runtime_error("Digest hex length must be 64.");
    }
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
    writer.WriteU32(static_cast<std::uint32_t>(value.size()));
    writer.WriteBytes(std::as_bytes(std::span(value.data(), value.size())));
}

struct ReadText final { std::string value; };

base::Result<ReadText, std::string> ReadString(
    base::BinaryReader& reader,
    std::uint32_t maximumLength = 16u * 1024u * 1024u)
{
    auto size = reader.ReadU32();
    if (!size) return base::Result<ReadText, std::string>::Failure(size.Error());
    if (size.Value() > maximumLength)
    {
        return base::Result<ReadText, std::string>::Failure(
            "String length exceeds the evidence limit.");
    }
    auto bytes = reader.ReadBytes(size.Value());
    if (!bytes) return base::Result<ReadText, std::string>::Failure(bytes.Error());
    return base::Result<ReadText, std::string>::Success(ReadText{std::string(
        reinterpret_cast<const char*>(bytes.Value().data()), bytes.Value().size())});
}

void WriteDigest(base::BinaryWriter& writer, const base::Digest256& value)
{
    writer.WriteBytes(value);
}

base::Result<base::Digest256, std::string> ReadDigest(base::BinaryReader& reader)
{
    auto bytes = reader.ReadBytes(32);
    if (!bytes) return base::Result<base::Digest256, std::string>::Failure(bytes.Error());
    base::Digest256 value{};
    std::memcpy(value.data(), bytes.Value().data(), value.size());
    return base::Result<base::Digest256, std::string>::Success(value);
}

void WriteDouble(base::BinaryWriter& writer, double value)
{
    writer.WriteU64(std::bit_cast<std::uint64_t>(value));
}

base::Result<double, std::string> ReadDouble(base::BinaryReader& reader)
{
    auto value = reader.ReadU64();
    if (!value) return base::Result<double, std::string>::Failure(value.Error());
    return base::Result<double, std::string>::Success(std::bit_cast<double>(value.Value()));
}

void WriteBool(base::BinaryWriter& writer, bool value)
{
    writer.WriteU8(value ? 1u : 0u);
}

base::Result<bool, std::string> ReadBool(base::BinaryReader& reader)
{
    auto value = reader.ReadU8();
    if (!value) return base::Result<bool, std::string>::Failure(value.Error());
    if (value.Value() > 1u)
    {
        return base::Result<bool, std::string>::Failure("Boolean evidence byte is not canonical.");
    }
    return base::Result<bool, std::string>::Success(value.Value() != 0u);
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
    case TransitionClassificationV1::NoObservedWinnerTransition:
        return "NoObservedWinnerTransition";
    case TransitionClassificationV1::SingleStableWinnerTransition:
        return "SingleStableWinnerTransition";
    case TransitionClassificationV1::MultipleWinnerTransitions:
        return "MultipleWinnerTransitions";
    case TransitionClassificationV1::NoiseEquivalentOrUnresolved:
        return "NoiseEquivalentOrUnresolved";
    }
    return "Unknown";
}

std::size_t CandidateIndex(CandidateKindV1 kind)
{
    switch (kind)
    {
    case CandidateKindV1::EveryInvocationRecompute: return 0;
    case CandidateKindV1::GlobalMotorHistoryReuse: return 1;
    case CandidateKindV1::FinalOutputHistoryReuse: return 2;
    }
    throw std::runtime_error("Unknown Temporal Candidate kind.");
}

CandidateKindV1 CandidateFromU32(std::uint32_t value)
{
    switch (value)
    {
    case 1: return CandidateKindV1::EveryInvocationRecompute;
    case 2: return CandidateKindV1::GlobalMotorHistoryReuse;
    case 3: return CandidateKindV1::FinalOutputHistoryReuse;
    default: throw std::runtime_error("Measurement evidence contains an unknown Candidate kind.");
    }
}

family_candidate::TemporalFamilyHistoryRoleV1 HistoryRoleFromU32(std::uint32_t value)
{
    switch (value)
    {
    case 0: return family_candidate::TemporalFamilyHistoryRoleV1::None;
    case 1: return family_candidate::TemporalFamilyHistoryRoleV1::GlobalMotorHistory;
    case 2: return family_candidate::TemporalFamilyHistoryRoleV1::FinalOutputHistory;
    default: throw std::runtime_error("Measurement evidence contains an unknown History Role.");
    }
}

void ValidateConfig(const MeasurementConfigV1& config)
{
    Require(config.measurementTimelinesPerOrder > 0 && config.measurementTimelinesPerOrder <= 1000,
        "Measurement timelines per order are outside the qualified range.");
    Require(config.warmupTimelinesPerOrder <= 1000,
        "Warm-up timelines per order are outside the qualified range.");
    Require(config.runCount > 0 && config.runCount <= 100,
        "Measurement run count is outside the qualified range.");
    Require(config.clockPolicy.size() <= 256 && !config.clockPolicy.empty(),
        "Clock policy is invalid.");
    Require(config.primaryMetric == "GpuCandidateTimelineNanoseconds",
        "Primary metric is not canonical.");
    Require(std::isfinite(config.absoluteNoiseFloorNanoseconds) &&
            config.absoluteNoiseFloorNanoseconds >= 0.0,
        "Absolute noise floor is invalid.");
    Require(std::isfinite(config.relativeNoiseFloor) &&
            config.relativeNoiseFloor >= 0.0 && config.relativeNoiseFloor <= 1.0,
        "Relative noise floor is invalid.");
}

void ValidateOrders()
{
    const auto orders = CanonicalBalancedOrdersV1();
    Require(orders.size() == CanonicalOrderCountV1,
        "Balanced Candidate order count is invalid.");
    std::array<std::array<std::uint32_t, CanonicalCandidateCountV1>, CanonicalCandidateCountV1> counts{};
    std::set<std::array<std::uint32_t, CanonicalCandidateCountV1>> unique;
    for (const auto& order : orders)
    {
        std::array<bool, CanonicalCandidateCountV1> seen{};
        for (std::size_t position = 0; position < order.size(); ++position)
        {
            Require(order[position] < CanonicalCandidateCountV1,
                "Balanced order Candidate index is outside the corpus.");
            Require(!seen[order[position]], "Balanced order repeats a Candidate.");
            seen[order[position]] = true;
            ++counts[order[position]][position];
        }
        unique.insert(order);
    }
    Require(unique.size() == orders.size(), "Balanced Candidate orders are not unique.");
    for (const auto& candidate : counts)
    {
        for (std::uint32_t count : candidate)
        {
            Require(count == 2, "Each Candidate must appear twice in every order position.");
        }
    }
}

void WriteConfig(base::BinaryWriter& writer, const MeasurementConfigV1& value)
{
    writer.WriteU32(value.warmupTimelinesPerOrder);
    writer.WriteU32(value.measurementTimelinesPerOrder);
    writer.WriteU32(value.runCount);
    writer.WriteU32(value.adapterIndex);
    WriteString(writer, value.clockPolicy);
    WriteString(writer, value.primaryMetric);
    WriteDouble(writer, value.absoluteNoiseFloorNanoseconds);
    WriteDouble(writer, value.relativeNoiseFloor);
}

base::Result<MeasurementConfigV1, std::string> ReadConfig(base::BinaryReader& reader)
{
    auto warmup = reader.ReadU32();
    auto measurement = reader.ReadU32();
    auto runs = reader.ReadU32();
    auto adapter = reader.ReadU32();
    auto clock = ReadString(reader, 256);
    auto metric = ReadString(reader, 256);
    auto absolute = ReadDouble(reader);
    auto relative = ReadDouble(reader);
    if (!warmup || !measurement || !runs || !adapter || !clock || !metric || !absolute || !relative)
    {
        return base::Result<MeasurementConfigV1, std::string>::Failure(
            "Measurement configuration is truncated.");
    }
    MeasurementConfigV1 value;
    value.warmupTimelinesPerOrder = warmup.Value();
    value.measurementTimelinesPerOrder = measurement.Value();
    value.runCount = runs.Value();
    value.adapterIndex = adapter.Value();
    value.clockPolicy = std::move(clock).Value().value;
    value.primaryMetric = std::move(metric).Value().value;
    value.absoluteNoiseFloorNanoseconds = absolute.Value();
    value.relativeNoiseFloor = relative.Value();
    try { ValidateConfig(value); }
    catch (const std::exception& error)
    {
        return base::Result<MeasurementConfigV1, std::string>::Failure(error.what());
    }
    return base::Result<MeasurementConfigV1, std::string>::Success(std::move(value));
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

base::Result<AdapterProfileV1, std::string> ReadAdapter(base::BinaryReader& reader)
{
    auto description = ReadString(reader, 4096);
    auto vendor = reader.ReadU32();
    auto device = reader.ReadU32();
    auto subsystem = reader.ReadU32();
    auto revision = reader.ReadU32();
    auto luidLow = reader.ReadU32();
    auto luidHigh = reader.ReadI32();
    auto driver = reader.ReadU64();
    auto frequency = reader.ReadU64();
    auto uma = ReadBool(reader);
    auto coherent = ReadBool(reader);
    auto fingerprint = ReadDigest(reader);
    if (!description || !vendor || !device || !subsystem || !revision || !luidLow || !luidHigh ||
        !driver || !frequency || !uma || !coherent || !fingerprint)
    {
        return base::Result<AdapterProfileV1, std::string>::Failure("Adapter profile is truncated.");
    }
    AdapterProfileV1 value;
    value.description = std::move(description).Value().value;
    value.vendorId = vendor.Value();
    value.deviceId = device.Value();
    value.subsystemId = subsystem.Value();
    value.revision = revision.Value();
    value.luidLow = luidLow.Value();
    value.luidHigh = luidHigh.Value();
    value.driverVersion = driver.Value();
    value.timestampFrequency = frequency.Value();
    value.uma = uma.Value();
    value.cacheCoherentUma = coherent.Value();
    value.fingerprint = fingerprint.Value();
    return base::Result<AdapterProfileV1, std::string>::Success(std::move(value));
}

void WriteCandidateProfile(base::BinaryWriter& writer, const CandidateProfileV1& value)
{
    writer.WriteU32(static_cast<std::uint32_t>(value.kind));
    writer.WriteU32(static_cast<std::uint32_t>(value.historyRole));
    writer.WriteU32(value.historyDepth);
    writer.WriteU32(value.historyBytes);
    WriteDigest(writer, value.targetProfileIdentity);
    WriteDigest(writer, value.observationContractIdentity);
    WriteDigest(writer, value.argumentProducerProgramIdentity);
    WriteDigest(writer, value.hierarchyProgramIdentity);
    WriteDigest(writer, value.consumerProgramIdentity);
    WriteDigest(writer, value.argumentProducerShaderBytecodeDigest);
    WriteDigest(writer, value.hierarchyShaderBytecodeDigest);
    WriteDigest(writer, value.consumerShaderBytecodeDigest);
}

base::Result<CandidateProfileV1, std::string> ReadCandidateProfile(base::BinaryReader& reader)
{
    auto kind = reader.ReadU32();
    auto role = reader.ReadU32();
    auto depth = reader.ReadU32();
    auto bytes = reader.ReadU32();
    auto target = ReadDigest(reader);
    auto observation = ReadDigest(reader);
    auto producerProgram = ReadDigest(reader);
    auto hierarchyProgram = ReadDigest(reader);
    auto consumerProgram = ReadDigest(reader);
    auto producerShader = ReadDigest(reader);
    auto hierarchyShader = ReadDigest(reader);
    auto consumerShader = ReadDigest(reader);
    if (!kind || !role || !depth || !bytes || !target || !observation || !producerProgram ||
        !hierarchyProgram || !consumerProgram || !producerShader || !hierarchyShader || !consumerShader)
    {
        return base::Result<CandidateProfileV1, std::string>::Failure("Candidate profile is truncated.");
    }
    CandidateProfileV1 value;
    try
    {
        value.kind = CandidateFromU32(kind.Value());
        value.historyRole = HistoryRoleFromU32(role.Value());
    }
    catch (const std::exception& error)
    {
        return base::Result<CandidateProfileV1, std::string>::Failure(error.what());
    }
    value.historyDepth = depth.Value();
    value.historyBytes = bytes.Value();
    value.targetProfileIdentity = target.Value();
    value.observationContractIdentity = observation.Value();
    value.argumentProducerProgramIdentity = producerProgram.Value();
    value.hierarchyProgramIdentity = hierarchyProgram.Value();
    value.consumerProgramIdentity = consumerProgram.Value();
    value.argumentProducerShaderBytecodeDigest = producerShader.Value();
    value.hierarchyShaderBytecodeDigest = hierarchyShader.Value();
    value.consumerShaderBytecodeDigest = consumerShader.Value();
    return base::Result<CandidateProfileV1, std::string>::Success(std::move(value));
}

void WriteAuthority(base::BinaryWriter& writer, const CandidateCaseAuthorityV1& value)
{
    writer.WriteU32(static_cast<std::uint32_t>(value.kind));
    WriteDigest(writer, value.rawCandidateIdentity);
    WriteDigest(writer, value.verifiedPlanIdentity);
    WriteDigest(writer, value.verifierCertificateIdentity);
    WriteDigest(writer, value.artifactIdentity);
    WriteDigest(writer, value.historyResourceIdentity);
    WriteDigest(writer, value.invocationAuthorityIdentity);
}

base::Result<CandidateCaseAuthorityV1, std::string> ReadAuthority(base::BinaryReader& reader)
{
    auto kind = reader.ReadU32();
    auto raw = ReadDigest(reader);
    auto plan = ReadDigest(reader);
    auto certificate = ReadDigest(reader);
    auto artifact = ReadDigest(reader);
    auto history = ReadDigest(reader);
    auto invocation = ReadDigest(reader);
    if (!kind || !raw || !plan || !certificate || !artifact || !history || !invocation)
    {
        return base::Result<CandidateCaseAuthorityV1, std::string>::Failure(
            "Candidate case authority is truncated.");
    }
    CandidateCaseAuthorityV1 value;
    try { value.kind = CandidateFromU32(kind.Value()); }
    catch (const std::exception& error)
    {
        return base::Result<CandidateCaseAuthorityV1, std::string>::Failure(error.what());
    }
    value.rawCandidateIdentity = raw.Value();
    value.verifiedPlanIdentity = plan.Value();
    value.verifierCertificateIdentity = certificate.Value();
    value.artifactIdentity = artifact.Value();
    value.historyResourceIdentity = history.Value();
    value.invocationAuthorityIdentity = invocation.Value();
    return base::Result<CandidateCaseAuthorityV1, std::string>::Success(std::move(value));
}

void WriteSample(base::BinaryWriter& writer, const TimingSampleV1& value)
{
    writer.WriteU32(value.run);
    writer.WriteU32(value.orderIndex);
    writer.WriteU32(value.cycle);
    writer.WriteU32(value.orderPosition);
    writer.WriteU32(static_cast<std::uint32_t>(value.candidate));
    writer.WriteU32(value.updateInterval);
    WriteDouble(writer, value.temporalInputPreparationNanoseconds);
    WriteDouble(writer, value.commandRecordingNanoseconds);
    WriteDouble(writer, value.submitAndWaitNanoseconds);
    WriteDouble(writer, value.numericObservationNanoseconds);
    WriteDouble(writer, value.cpuEndToEndNanoseconds);
    WriteDouble(writer, value.gpuArgumentGenerationNanoseconds);
    WriteDouble(writer, value.gpuHierarchyExecutionNanoseconds);
    WriteDouble(writer, value.gpuConsumerExecutionNanoseconds);
    WriteDouble(writer, value.gpuRetainedCompletionNanoseconds);
    WriteDouble(writer, value.gpuObservationCopyNanoseconds);
    WriteDouble(writer, value.gpuCandidateTimelineNanoseconds);
    WriteDouble(writer, value.gpuTotalNanoseconds);
    WriteDouble(writer, value.gpuUpdatePathNanoseconds);
    WriteDouble(writer, value.gpuHoldPathNanoseconds);
    writer.WriteU32(value.updateInvocationCount);
    writer.WriteU32(value.holdInvocationCount);
    writer.WriteU32(value.mismatchCount);
    writer.WriteU32(value.nonFiniteCount);
    writer.WriteU32(value.homogeneousCoordinateMismatchCount);
    WriteDouble(writer, value.maxAbsoluteError);
    WriteDouble(writer, value.maxRelativeError);
    writer.WriteU32(value.maxUlpError);
    WriteDigest(writer, value.outputDigest);
}

base::Result<TimingSampleV1, std::string> ReadSample(base::BinaryReader& reader)
{
    TimingSampleV1 value;
    auto run = reader.ReadU32(); auto order = reader.ReadU32(); auto cycle = reader.ReadU32();
    auto position = reader.ReadU32(); auto candidate = reader.ReadU32(); auto interval = reader.ReadU32();
    if (!run || !order || !cycle || !position || !candidate || !interval)
    {
        return base::Result<TimingSampleV1, std::string>::Failure("Timing sample header is truncated.");
    }
    value.run = run.Value(); value.orderIndex = order.Value(); value.cycle = cycle.Value();
    value.orderPosition = position.Value(); value.updateInterval = interval.Value();
    try { value.candidate = CandidateFromU32(candidate.Value()); }
    catch (const std::exception& error)
    {
        return base::Result<TimingSampleV1, std::string>::Failure(error.what());
    }
    double* fields[] = {
        &value.temporalInputPreparationNanoseconds,
        &value.commandRecordingNanoseconds,
        &value.submitAndWaitNanoseconds,
        &value.numericObservationNanoseconds,
        &value.cpuEndToEndNanoseconds,
        &value.gpuArgumentGenerationNanoseconds,
        &value.gpuHierarchyExecutionNanoseconds,
        &value.gpuConsumerExecutionNanoseconds,
        &value.gpuRetainedCompletionNanoseconds,
        &value.gpuObservationCopyNanoseconds,
        &value.gpuCandidateTimelineNanoseconds,
        &value.gpuTotalNanoseconds,
        &value.gpuUpdatePathNanoseconds,
        &value.gpuHoldPathNanoseconds};
    for (double* field : fields)
    {
        auto read = ReadDouble(reader);
        if (!read) return base::Result<TimingSampleV1, std::string>::Failure("Timing sample values are truncated.");
        *field = read.Value();
        if (!std::isfinite(*field) || *field < 0.0)
        {
            return base::Result<TimingSampleV1, std::string>::Failure("Timing sample contains an invalid duration.");
        }
    }
    auto updateCount = reader.ReadU32(); auto holdCount = reader.ReadU32();
    auto mismatch = reader.ReadU32(); auto nonFinite = reader.ReadU32(); auto homogeneous = reader.ReadU32();
    auto maxAbsolute = ReadDouble(reader); auto maxRelative = ReadDouble(reader); auto maxUlp = reader.ReadU32();
    auto digest = ReadDigest(reader);
    if (!updateCount || !holdCount || !mismatch || !nonFinite || !homogeneous ||
        !maxAbsolute || !maxRelative || !maxUlp || !digest)
    {
        return base::Result<TimingSampleV1, std::string>::Failure("Timing sample observation is truncated.");
    }
    value.updateInvocationCount = updateCount.Value(); value.holdInvocationCount = holdCount.Value();
    value.mismatchCount = mismatch.Value(); value.nonFiniteCount = nonFinite.Value();
    value.homogeneousCoordinateMismatchCount = homogeneous.Value();
    value.maxAbsoluteError = maxAbsolute.Value(); value.maxRelativeError = maxRelative.Value();
    value.maxUlpError = maxUlp.Value(); value.outputDigest = digest.Value();
    return base::Result<TimingSampleV1, std::string>::Success(std::move(value));
}

std::vector<std::byte> SerializePayload(const MeasurementEvidenceV1& evidence)
{
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
    WriteDigest(writer, evidence.measurementProfileIdentity);
    WriteDigest(writer, evidence.orderBalanceIdentity);
    writer.WriteU32(CanonicalCandidateCountV1);
    for (const auto& candidate : evidence.candidates) WriteCandidateProfile(writer, candidate);
    writer.WriteU32(static_cast<std::uint32_t>(evidence.cases.size()));
    for (const auto& measurementCase : evidence.cases)
    {
        writer.WriteU32(measurementCase.updateInterval);
        WriteDigest(writer, measurementCase.scheduleIdentity);
        writer.WriteU32(CanonicalCandidateCountV1);
        for (const auto& authority : measurementCase.authorities) WriteAuthority(writer, authority);
        writer.WriteU32(static_cast<std::uint32_t>(measurementCase.samples.size()));
        for (const auto& sample : measurementCase.samples) WriteSample(writer, sample);
    }
    return std::move(writer).Take();
}

base::Result<MeasurementEvidenceV1, std::string> ParsePayload(std::span<const std::byte> bytes)
{
    base::BinaryReader reader(bytes);
    auto magic = ReadString(reader, 256);
    auto schema = reader.ReadU32();
    auto capture = reader.ReadU64();
    auto config = ReadConfig(reader);
    auto adapter = ReadAdapter(reader);
    auto architecture = ReadDigest(reader);
    auto controlled = ReadDigest(reader);
    auto fresh = ReadDigest(reader);
    auto semantic = ReadDigest(reader);
    auto profile = ReadDigest(reader);
    auto order = ReadDigest(reader);
    if (!magic || !schema || !capture || !config || !adapter || !architecture || !controlled ||
        !fresh || !semantic || !profile || !order)
    {
        return base::Result<MeasurementEvidenceV1, std::string>::Failure("Measurement evidence header is truncated.");
    }
    if (magic.Value().value != EvidenceMagic || schema.Value() != MeasurementEvidenceSchemaVersionV1)
    {
        return base::Result<MeasurementEvidenceV1, std::string>::Failure("Measurement evidence schema mismatch.");
    }
    MeasurementEvidenceV1 evidence;
    evidence.schemaVersion = schema.Value(); evidence.captureUnixNanoseconds = capture.Value();
    evidence.config = std::move(config).Value(); evidence.adapter = std::move(adapter).Value();
    evidence.architectureEvidenceIdentity = architecture.Value();
    evidence.controlledRecoveryEvidenceIdentity = controlled.Value();
    evidence.freshRematerializationEvidenceIdentity = fresh.Value();
    evidence.semanticIdentity = semantic.Value(); evidence.measurementProfileIdentity = profile.Value();
    evidence.orderBalanceIdentity = order.Value();

    auto candidateCount = reader.ReadU32();
    if (!candidateCount || candidateCount.Value() != CanonicalCandidateCountV1)
    {
        return base::Result<MeasurementEvidenceV1, std::string>::Failure("Measurement Candidate count mismatch.");
    }
    for (std::size_t i = 0; i < evidence.candidates.size(); ++i)
    {
        auto candidate = ReadCandidateProfile(reader);
        if (!candidate) return base::Result<MeasurementEvidenceV1, std::string>::Failure(candidate.Error());
        evidence.candidates[i] = std::move(candidate).Value();
    }
    auto caseCount = reader.ReadU32();
    if (!caseCount || caseCount.Value() > 1024)
    {
        return base::Result<MeasurementEvidenceV1, std::string>::Failure("Measurement case count is invalid.");
    }
    evidence.cases.reserve(caseCount.Value());
    for (std::uint32_t caseIndex = 0; caseIndex < caseCount.Value(); ++caseIndex)
    {
        MeasurementCaseV1 measurementCase;
        auto interval = reader.ReadU32(); auto schedule = ReadDigest(reader); auto authorityCount = reader.ReadU32();
        if (!interval || !schedule || !authorityCount || authorityCount.Value() != CanonicalCandidateCountV1)
        {
            return base::Result<MeasurementEvidenceV1, std::string>::Failure("Measurement case header is invalid.");
        }
        measurementCase.updateInterval = interval.Value(); measurementCase.scheduleIdentity = schedule.Value();
        for (auto& authority : measurementCase.authorities)
        {
            auto parsed = ReadAuthority(reader);
            if (!parsed) return base::Result<MeasurementEvidenceV1, std::string>::Failure(parsed.Error());
            authority = std::move(parsed).Value();
        }
        auto sampleCount = reader.ReadU32();
        if (!sampleCount || sampleCount.Value() > 10u * 1000u * 1000u)
        {
            return base::Result<MeasurementEvidenceV1, std::string>::Failure("Measurement sample count is invalid.");
        }
        measurementCase.samples.reserve(sampleCount.Value());
        for (std::uint32_t sampleIndex = 0; sampleIndex < sampleCount.Value(); ++sampleIndex)
        {
            auto sample = ReadSample(reader);
            if (!sample) return base::Result<MeasurementEvidenceV1, std::string>::Failure(sample.Error());
            if (sample.Value().updateInterval != measurementCase.updateInterval)
            {
                return base::Result<MeasurementEvidenceV1, std::string>::Failure(
                    "Measurement sample update interval differs from its case.");
            }
            measurementCase.samples.push_back(std::move(sample).Value());
        }
        evidence.cases.push_back(std::move(measurementCase));
    }
    if (reader.Remaining() != 0)
    {
        return base::Result<MeasurementEvidenceV1, std::string>::Failure("Measurement evidence has trailing bytes.");
    }
    if (evidence.architectureEvidenceIdentity != DigestFromHex(ArchitectureEvidenceHex) ||
        evidence.controlledRecoveryEvidenceIdentity != DigestFromHex(ControlledRecoveryEvidenceHex) ||
        evidence.freshRematerializationEvidenceIdentity != DigestFromHex(FreshRematerializationEvidenceHex))
    {
        return base::Result<MeasurementEvidenceV1, std::string>::Failure(
            "Measurement evidence does not bind the qualified CU5 evidence.");
    }
    if (evidence.measurementProfileIdentity != BuildMeasurementProfileIdentityV1(evidence.config) ||
        evidence.orderBalanceIdentity != BuildOrderBalanceIdentityV1())
    {
        return base::Result<MeasurementEvidenceV1, std::string>::Failure(
            "Measurement profile or order-balance identity mismatch.");
    }
    return base::Result<MeasurementEvidenceV1, std::string>::Success(std::move(evidence));
}

double Median(std::vector<double> values)
{
    if (values.empty()) throw std::runtime_error("Median requires at least one sample.");
    std::sort(values.begin(), values.end());
    const std::size_t middle = values.size() / 2;
    return values.size() % 2 == 0 ? (values[middle - 1] + values[middle]) * 0.5 : values[middle];
}

double NoiseThreshold(double a, double b, const MeasurementConfigV1& config)
{
    return std::max(config.absoluteNoiseFloorNanoseconds,
        config.relativeNoiseFloor * std::min(a, b));
}

PairWinnerV1 Compare(double left, double right, const MeasurementConfigV1& config)
{
    if (std::abs(left - right) <= NoiseThreshold(left, right, config))
    {
        return PairWinnerV1::NoiseEquivalent;
    }
    return left < right ? PairWinnerV1::Left : PairWinnerV1::Right;
}

std::pair<TransitionClassificationV1, std::vector<WinnerTransitionV1>> ClassifyTransitions(
    const std::vector<PerUpdateIntervalDecisionV1>& cases,
    CandidateKindV1 left,
    CandidateKindV1 right,
    bool useFirstPair)
{
    std::vector<WinnerTransitionV1> transitions;
    std::optional<PairWinnerV1> previous;
    std::uint32_t previousInterval = 0;
    bool hasNoise = false;
    for (const auto& value : cases)
    {
        const PairWinnerV1 current = useFirstPair
            ? value.recomputeVersusGlobalMotor
            : value.globalMotorVersusFinalOutput;
        if (current == PairWinnerV1::NoiseEquivalent)
        {
            hasNoise = true;
            continue;
        }
        if (previous && *previous != current)
        {
            transitions.push_back(WinnerTransitionV1{
                previousInterval,
                value.updateInterval,
                *previous == PairWinnerV1::Left ? left : right,
                current == PairWinnerV1::Left ? left : right});
        }
        previous = current;
        previousInterval = value.updateInterval;
    }
    if (hasNoise || !previous)
    {
        return {TransitionClassificationV1::NoiseEquivalentOrUnresolved, std::move(transitions)};
    }
    if (transitions.empty())
    {
        return {TransitionClassificationV1::NoObservedWinnerTransition, {}};
    }
    if (transitions.size() == 1)
    {
        return {TransitionClassificationV1::SingleStableWinnerTransition, std::move(transitions)};
    }
    return {TransitionClassificationV1::MultipleWinnerTransitions, std::move(transitions)};
}

std::string WinnerSetText(const std::vector<CandidateKindV1>& values)
{
    std::ostringstream stream;
    for (std::size_t i = 0; i < values.size(); ++i)
    {
        if (i != 0) stream << ", ";
        stream << CandidateLetterV1(values[i]) << "." << CandidateNameV1(values[i]);
    }
    return stream.str();
}

CandidateProfileV1 SyntheticCandidateProfile(CandidateKindV1 kind)
{
    CandidateProfileV1 profile;
    profile.kind = kind;
    if (kind == CandidateKindV1::EveryInvocationRecompute)
    {
        profile.historyRole = family_candidate::TemporalFamilyHistoryRoleV1::None;
        profile.historyDepth = 0; profile.historyBytes = 0;
    }
    else if (kind == CandidateKindV1::GlobalMotorHistoryReuse)
    {
        profile.historyRole = family_candidate::TemporalFamilyHistoryRoleV1::GlobalMotorHistory;
        profile.historyDepth = 1; profile.historyBytes = 256;
    }
    else
    {
        profile.historyRole = family_candidate::TemporalFamilyHistoryRoleV1::FinalOutputHistory;
        profile.historyDepth = 1; profile.historyBytes = 65536;
    }
    const std::string name = CandidateNameV1(kind);
    profile.targetProfileIdentity = LiteralIdentity("Synthetic.Target." + name);
    profile.observationContractIdentity = LiteralIdentity("Synthetic.Observation." + name);
    profile.argumentProducerProgramIdentity = LiteralIdentity("Synthetic.ArgumentProgram." + name);
    profile.hierarchyProgramIdentity = LiteralIdentity("Synthetic.HierarchyProgram." + name);
    profile.consumerProgramIdentity = LiteralIdentity("Synthetic.ConsumerProgram." + name);
    profile.argumentProducerShaderBytecodeDigest = LiteralIdentity("Synthetic.ArgumentShader." + name);
    profile.hierarchyShaderBytecodeDigest = LiteralIdentity("Synthetic.HierarchyShader." + name);
    profile.consumerShaderBytecodeDigest = LiteralIdentity("Synthetic.ConsumerShader." + name);
    return profile;
}

MeasurementEvidenceV1 BuildSyntheticEvidence()
{
    MeasurementEvidenceV1 evidence;
    evidence.captureUnixNanoseconds = 0;
    evidence.config.warmupTimelinesPerOrder = 0;
    evidence.config.measurementTimelinesPerOrder = 1;
    evidence.config.runCount = 1;
    evidence.config.adapterIndex = 0;
    evidence.config.clockPolicy = "synthetic";
    evidence.adapter.description = "Synthetic Adapter";
    evidence.adapter.timestampFrequency = 1'000'000'000ull;
    evidence.adapter.fingerprint = LiteralIdentity("Synthetic.Adapter.V1");
    evidence.architectureEvidenceIdentity = DigestFromHex(ArchitectureEvidenceHex);
    evidence.controlledRecoveryEvidenceIdentity = DigestFromHex(ControlledRecoveryEvidenceHex);
    evidence.freshRematerializationEvidenceIdentity = DigestFromHex(FreshRematerializationEvidenceHex);
    const auto semanticValue = semantic::BuildCanonicalTemporalStateSemanticV1();
    evidence.semanticIdentity = semantic::TemporalStateSemanticIdentityV1(semanticValue);
    evidence.measurementProfileIdentity = BuildMeasurementProfileIdentityV1(evidence.config);
    evidence.orderBalanceIdentity = BuildOrderBalanceIdentityV1();
    const auto kinds = CanonicalCandidateKindsV1();
    for (std::size_t i = 0; i < kinds.size(); ++i) evidence.candidates[i] = SyntheticCandidateProfile(kinds[i]);

    const std::array<std::array<double, 3>, 7> syntheticMedians{{
        {1000.0, 1300.0, 1600.0},
        {1000.0, 1200.0, 1500.0},
        {1000.0,  800.0, 1300.0},
        {1000.0,  700.0, 1000.0},
        {1000.0,  650.0,  500.0},
        {1000.0,  600.0,  400.0},
        {1000.0,  550.0,  300.0}}};
    const auto orders = CanonicalBalancedOrdersV1();
    const auto intervals = CanonicalUpdateIntervalsV1();
    for (std::size_t uIndex = 0; uIndex < intervals.size(); ++uIndex)
    {
        MeasurementCaseV1 measurementCase;
        measurementCase.updateInterval = intervals[uIndex];
        const auto schedule = semantic::BuildPeriodicUpdateScheduleV1(intervals[uIndex]);
        if (!schedule) throw std::runtime_error(schedule.Error().stage + ": " + schedule.Error().message);
        measurementCase.scheduleIdentity = semantic::UpdateScheduleIdentityV1(schedule.Value());
        for (std::size_t i = 0; i < kinds.size(); ++i)
        {
            auto& authority = measurementCase.authorities[i];
            authority.kind = kinds[i];
            const std::string stem = "Synthetic.U" + std::to_string(intervals[uIndex]) + "." + CandidateNameV1(kinds[i]);
            authority.rawCandidateIdentity = LiteralIdentity(stem + ".Raw");
            authority.verifiedPlanIdentity = LiteralIdentity(stem + ".Plan");
            authority.verifierCertificateIdentity = LiteralIdentity(stem + ".Certificate");
            authority.artifactIdentity = LiteralIdentity(stem + ".Artifact");
            authority.historyResourceIdentity = LiteralIdentity(stem + ".History");
            authority.invocationAuthorityIdentity = LiteralIdentity(stem + ".Invocation");
        }
        for (std::size_t orderIndex = 0; orderIndex < orders.size(); ++orderIndex)
        {
            for (std::size_t position = 0; position < kinds.size(); ++position)
            {
                const std::size_t candidateIndex = orders[orderIndex][position];
                TimingSampleV1 sample;
                sample.run = 0; sample.orderIndex = static_cast<std::uint32_t>(orderIndex);
                sample.cycle = 0; sample.orderPosition = static_cast<std::uint32_t>(position);
                sample.candidate = kinds[candidateIndex]; sample.updateInterval = intervals[uIndex];
                sample.updateInvocationCount = 1u + (127u / intervals[uIndex]);
                sample.holdInvocationCount = 128u - sample.updateInvocationCount;
                sample.gpuCandidateTimelineNanoseconds = syntheticMedians[uIndex][candidateIndex];
                sample.gpuUpdatePathNanoseconds = sample.gpuCandidateTimelineNanoseconds * 0.6;
                sample.gpuHoldPathNanoseconds = sample.gpuCandidateTimelineNanoseconds * 0.4;
                sample.gpuTotalNanoseconds = sample.gpuCandidateTimelineNanoseconds + 50.0;
                sample.gpuObservationCopyNanoseconds = 50.0;
                sample.outputDigest = LiteralIdentity("Synthetic.Output.U" + std::to_string(intervals[uIndex]));
                measurementCase.samples.push_back(sample);
            }
        }
        evidence.cases.push_back(std::move(measurementCase));
    }
    return evidence;
}
}

std::string CandidateNameV1(CandidateKindV1 value)
{
    switch (value)
    {
    case CandidateKindV1::EveryInvocationRecompute: return "EveryInvocationRecompute";
    case CandidateKindV1::GlobalMotorHistoryReuse: return "GlobalMotorHistoryReuse";
    case CandidateKindV1::FinalOutputHistoryReuse: return "FinalOutputHistoryReuse";
    }
    return "Unknown";
}

char CandidateLetterV1(CandidateKindV1 value)
{
    switch (value)
    {
    case CandidateKindV1::EveryInvocationRecompute: return 'A';
    case CandidateKindV1::GlobalMotorHistoryReuse: return 'B';
    case CandidateKindV1::FinalOutputHistoryReuse: return 'C';
    }
    return '?';
}

std::array<CandidateKindV1, CanonicalCandidateCountV1> CanonicalCandidateKindsV1()
{
    return {
        CandidateKindV1::EveryInvocationRecompute,
        CandidateKindV1::GlobalMotorHistoryReuse,
        CandidateKindV1::FinalOutputHistoryReuse};
}

const std::array<std::uint32_t, CanonicalUpdateIntervalCountV1>&
CanonicalUpdateIntervalsV1() noexcept
{
    static constexpr std::array<std::uint32_t, CanonicalUpdateIntervalCountV1> values{
        1u, 2u, 4u, 8u, 16u, 32u, 64u};
    return values;
}

std::vector<std::array<std::uint32_t, CanonicalCandidateCountV1>>
CanonicalBalancedOrdersV1()
{
    return {
        {0u, 1u, 2u},
        {0u, 2u, 1u},
        {1u, 0u, 2u},
        {1u, 2u, 0u},
        {2u, 0u, 1u},
        {2u, 1u, 0u}};
}

base::Digest256 BuildMeasurementProfileIdentityV1(const MeasurementConfigV1& config)
{
    ValidateConfig(config);
    base::BinaryWriter writer;
    WriteString(writer, MeasurementProfileDomain);
    WriteConfig(writer, config);
    writer.WriteU32(semantic::TimelineLengthV1);
    writer.WriteU32(semantic::WorkCountV1);
    writer.WriteU32(semantic::ThreadsPerGroupV1);
    writer.WriteU32(CanonicalCandidateCountV1);
    writer.WriteU32(CanonicalUpdateIntervalCountV1);
    writer.WriteU32(CanonicalOrderCountV1);
    for (std::uint32_t value : CanonicalUpdateIntervalsV1()) writer.WriteU32(value);
    return base::Sha256(writer.Bytes());
}

base::Digest256 BuildOrderBalanceIdentityV1()
{
    ValidateOrders();
    base::BinaryWriter writer;
    WriteString(writer, OrderBalanceDomain);
    const auto orders = CanonicalBalancedOrdersV1();
    writer.WriteU32(static_cast<std::uint32_t>(orders.size()));
    for (const auto& order : orders)
    {
        for (std::uint32_t value : order) writer.WriteU32(value);
    }
    return base::Sha256(writer.Bytes());
}

std::vector<std::byte> SerializeMeasurementEvidenceV1(const MeasurementEvidenceV1& evidence)
{
    const auto payload = SerializePayload(evidence);
    const auto checksum = base::Sha256(payload);
    base::BinaryWriter writer;
    writer.WriteBytes(payload);
    writer.WriteBytes(checksum);
    return std::move(writer).Take();
}

base::Result<MeasurementEvidenceV1, std::string>
ReadMeasurementEvidenceV1(const std::filesystem::path& evidencePath)
{
    auto bytes = base::ReadAllBytes(evidencePath);
    if (!bytes) return base::Result<MeasurementEvidenceV1, std::string>::Failure(bytes.Error());
    if (bytes.Value().size() < 32)
    {
        return base::Result<MeasurementEvidenceV1, std::string>::Failure("Measurement evidence is too short.");
    }
    const std::size_t payloadSize = bytes.Value().size() - 32;
    const auto payload = std::span<const std::byte>(bytes.Value()).first(payloadSize);
    const auto stored = std::span<const std::byte>(bytes.Value()).subspan(payloadSize);
    const auto calculated = base::Sha256(payload);
    if (!std::equal(calculated.begin(), calculated.end(), stored.begin(), stored.end()))
    {
        return base::Result<MeasurementEvidenceV1, std::string>::Failure(
            "Measurement evidence checksum mismatch.");
    }
    return ParsePayload(payload);
}

base::Result<void, std::string> WriteMeasurementArtifactsV1(
    const MeasurementEvidenceV1& evidence,
    const std::filesystem::path& outputDirectory)
{
    try
    {
        std::filesystem::create_directories(outputDirectory);
        const auto bytes = SerializeMeasurementEvidenceV1(evidence);
        auto write = base::WriteAllBytes(outputDirectory / "spiral5_measurement_evidence_v1.bin", bytes);
        if (!write) return base::Result<void, std::string>::Failure(write.Error());

        std::ofstream csv(outputDirectory / "spiral5_measurement_samples_v1.csv", std::ios::binary);
        if (!csv) return base::Result<void, std::string>::Failure("Could not create the measurement CSV.");
        csv << "update_interval,run,order_index,cycle,order_position,candidate,"
               "temporal_input_ns,command_recording_ns,submit_wait_ns,numeric_observation_ns,cpu_end_to_end_ns,"
               "gpu_argument_ns,gpu_hierarchy_ns,gpu_consumer_ns,gpu_completion_ns,gpu_observation_copy_ns,"
               "gpu_candidate_timeline_ns,gpu_total_ns,gpu_update_path_ns,gpu_hold_path_ns,"
               "update_count,hold_count,mismatch_count,non_finite_count,homogeneous_mismatch_count,"
               "max_absolute_error,max_relative_error,max_ulp_error,output_digest\n";
        csv << std::setprecision(17);
        for (const auto& measurementCase : evidence.cases)
        {
            for (const auto& sample : measurementCase.samples)
            {
                csv << sample.updateInterval << ',' << sample.run << ',' << sample.orderIndex << ','
                    << sample.cycle << ',' << sample.orderPosition << ','
                    << CandidateLetterV1(sample.candidate) << '.' << CandidateNameV1(sample.candidate) << ','
                    << sample.temporalInputPreparationNanoseconds << ',' << sample.commandRecordingNanoseconds << ','
                    << sample.submitAndWaitNanoseconds << ',' << sample.numericObservationNanoseconds << ','
                    << sample.cpuEndToEndNanoseconds << ',' << sample.gpuArgumentGenerationNanoseconds << ','
                    << sample.gpuHierarchyExecutionNanoseconds << ',' << sample.gpuConsumerExecutionNanoseconds << ','
                    << sample.gpuRetainedCompletionNanoseconds << ',' << sample.gpuObservationCopyNanoseconds << ','
                    << sample.gpuCandidateTimelineNanoseconds << ',' << sample.gpuTotalNanoseconds << ','
                    << sample.gpuUpdatePathNanoseconds << ',' << sample.gpuHoldPathNanoseconds << ','
                    << sample.updateInvocationCount << ',' << sample.holdInvocationCount << ','
                    << sample.mismatchCount << ',' << sample.nonFiniteCount << ','
                    << sample.homogeneousCoordinateMismatchCount << ',' << sample.maxAbsoluteError << ','
                    << sample.maxRelativeError << ',' << sample.maxUlpError << ','
                    << base::ToHex(sample.outputDigest) << '\n';
            }
        }

        auto analysis = AnalyzeDecisionEvidenceV1(evidence);
        if (!analysis) return base::Result<void, std::string>::Failure(analysis.Error());
        std::ofstream json(outputDirectory / "spiral5_measurement_summary_v1.json", std::ios::binary);
        if (!json) return base::Result<void, std::string>::Failure("Could not create the measurement summary JSON.");
        json << "{\n"
             << "  \"schema\": \"SGE4-5.Spiral5.MeasurementSummary.V1\",\n"
             << "  \"adapter\": \"" << JsonEscape(evidence.adapter.description) << "\",\n"
             << "  \"primaryMetric\": \"GpuCandidateTimelineNanoseconds\",\n"
             << "  \"runtimePolicyAuthorization\": \"None\",\n"
             << "  \"nextCapabilitySelection\": \"DeferredByOwner\",\n"
             << "  \"selectionStatus\": \"OWNER_DECISION_REQUIRED\",\n"
             << "  \"recomputeGlobalClassification\": \""
             << TransitionName(analysis.Value().recomputeGlobalClassification) << "\",\n"
             << "  \"globalFinalClassification\": \""
             << TransitionName(analysis.Value().globalFinalClassification) << "\",\n"
             << "  \"cases\": [\n";
        for (std::size_t caseIndex = 0; caseIndex < analysis.Value().cases.size(); ++caseIndex)
        {
            const auto& value = analysis.Value().cases[caseIndex];
            json << "    {\"U\": " << value.updateInterval << ", \"winnerSet\": [";
            for (std::size_t i = 0; i < value.globalWinnerSet.size(); ++i)
            {
                if (i != 0) json << ", ";
                json << '"' << CandidateLetterV1(value.globalWinnerSet[i]) << '"';
            }
            json << "], \"medianTimelineNs\": {\"A\": "
                 << value.medianGpuCandidateTimelineNanoseconds[0] << ", \"B\": "
                 << value.medianGpuCandidateTimelineNanoseconds[1] << ", \"C\": "
                 << value.medianGpuCandidateTimelineNanoseconds[2] << "}}";
            if (caseIndex + 1 != analysis.Value().cases.size()) json << ',';
            json << '\n';
        }
        json << "  ]\n}\n";
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
        ValidateOrders();
        DecisionAnalysisV1 analysis;
        std::array<std::vector<double>, CanonicalCandidateCountV1> overallMedians;
        for (const auto& measurementCase : evidence.cases)
        {
            PerUpdateIntervalDecisionV1 decision;
            decision.updateInterval = measurementCase.updateInterval;
            for (CandidateKindV1 kind : CanonicalCandidateKindsV1())
            {
                std::vector<double> primary;
                std::vector<double> updateAverage;
                std::vector<double> holdAverage;
                for (const auto& sample : measurementCase.samples)
                {
                    if (sample.candidate != kind) continue;
                    if (sample.mismatchCount != 0 || sample.nonFiniteCount != 0 ||
                        sample.homogeneousCoordinateMismatchCount != 0)
                    {
                        throw std::runtime_error("Decision evidence contains a failed numeric observation.");
                    }
                    primary.push_back(sample.gpuCandidateTimelineNanoseconds);
                    updateAverage.push_back(sample.updateInvocationCount == 0 ? 0.0 :
                        sample.gpuUpdatePathNanoseconds / sample.updateInvocationCount);
                    holdAverage.push_back(sample.holdInvocationCount == 0 ? 0.0 :
                        sample.gpuHoldPathNanoseconds / sample.holdInvocationCount);
                }
                if (primary.empty()) throw std::runtime_error("Decision evidence is missing a Candidate sample set.");
                const std::size_t index = CandidateIndex(kind);
                decision.medianGpuCandidateTimelineNanoseconds[index] = Median(primary);
                decision.medianGpuUpdateInvocationNanoseconds[index] = Median(updateAverage);
                decision.medianGpuHoldInvocationNanoseconds[index] = Median(holdAverage);
                overallMedians[index].push_back(decision.medianGpuCandidateTimelineNanoseconds[index]);
            }
            const double fastest = *std::min_element(
                decision.medianGpuCandidateTimelineNanoseconds.begin(),
                decision.medianGpuCandidateTimelineNanoseconds.end());
            for (CandidateKindV1 kind : CanonicalCandidateKindsV1())
            {
                const double value = decision.medianGpuCandidateTimelineNanoseconds[CandidateIndex(kind)];
                if (std::abs(value - fastest) <= NoiseThreshold(value, fastest, evidence.config))
                {
                    decision.globalWinnerSet.push_back(kind);
                }
            }
            decision.recomputeVersusGlobalMotor = Compare(
                decision.medianGpuCandidateTimelineNanoseconds[0],
                decision.medianGpuCandidateTimelineNanoseconds[1], evidence.config);
            decision.globalMotorVersusFinalOutput = Compare(
                decision.medianGpuCandidateTimelineNanoseconds[1],
                decision.medianGpuCandidateTimelineNanoseconds[2], evidence.config);
            analysis.cases.push_back(std::move(decision));
        }
        std::sort(analysis.cases.begin(), analysis.cases.end(),
            [](const auto& a, const auto& b) { return a.updateInterval < b.updateInterval; });
        const auto first = ClassifyTransitions(analysis.cases,
            CandidateKindV1::EveryInvocationRecompute,
            CandidateKindV1::GlobalMotorHistoryReuse, true);
        analysis.recomputeGlobalClassification = first.first;
        analysis.recomputeGlobalTransitions = first.second;
        const auto second = ClassifyTransitions(analysis.cases,
            CandidateKindV1::GlobalMotorHistoryReuse,
            CandidateKindV1::FinalOutputHistoryReuse, false);
        analysis.globalFinalClassification = second.first;
        analysis.globalFinalTransitions = second.second;

        std::array<double, CanonicalCandidateCountV1> overall{};
        for (std::size_t i = 0; i < overall.size(); ++i) overall[i] = Median(overallMedians[i]);
        const double overallFastest = *std::min_element(overall.begin(), overall.end());
        for (CandidateKindV1 kind : CanonicalCandidateKindsV1())
        {
            const double value = overall[CandidateIndex(kind)];
            if (std::abs(value - overallFastest) <= NoiseThreshold(value, overallFastest, evidence.config))
            {
                analysis.overallObservedWinnerSet.push_back(kind);
            }
        }
        if (!analysis.cases.empty())
        {
            const auto& initial = analysis.cases.front().globalWinnerSet;
            analysis.globalWinnerSetChanges = std::any_of(
                analysis.cases.begin() + 1, analysis.cases.end(),
                [&](const auto& value) { return value.globalWinnerSet != initial; });
        }
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
    auto analysis = AnalyzeDecisionEvidenceV1(evidence);
    if (!analysis) return base::Result<void, std::string>::Failure(analysis.Error());
    try
    {
        std::filesystem::create_directories(outputDirectory);
        std::ofstream report(outputDirectory / "SPIRAL5_DECISION_EVIDENCE_REPORT.md", std::ios::binary);
        if (!report) return base::Result<void, std::string>::Failure("Could not create the Decision Evidence Report.");
        report << "# SGE4-5 Spiral 5 Decision Evidence Report\n\n"
               << "## Completion boundary\n\n"
               << "```text\n"
               << "RuntimePolicyAuthorization = None\n"
               << "NextCapabilitySelection = DeferredByOwner\n"
               << "SelectionStatus = OWNER_DECISION_REQUIRED\n"
               << "```\n\n"
               << "Performance superiority and crossover are not completion gates.\n"
               << "No observed crossover is a valid result.\n"
               << "This report does not authorize Runtime adaptive selection.\n\n"
               << "## Measurement profile\n\n"
               << "- Adapter: `" << evidence.adapter.description << "`\n"
               << "- Timestamp frequency: `" << evidence.adapter.timestampFrequency << "`\n"
               << "- Primary metric: `GpuCandidateTimelineNanoseconds`\n"
               << "- Balanced order: all six A/B/C permutations; each Candidate appears twice per position\n"
               << "- Samples per Candidate and U: `"
               << CanonicalOrderCountV1 * evidence.config.measurementTimelinesPerOrder * evidence.config.runCount
               << "`\n"
               << "- Noise rule: `abs(a-b) <= max(" << evidence.config.absoluteNoiseFloorNanoseconds
               << " ns, " << evidence.config.relativeNoiseFloor << " * min(a,b))`\n\n"
               << "## Per-U observations\n\n"
               << "| U | Winner set | A timeline ns | B timeline ns | C timeline ns | A update ns | B update ns | C update ns | A hold ns | B hold ns | C hold ns | A/B | B/C |\n"
               << "|---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---|\n";
        report << std::setprecision(10);
        for (const auto& value : analysis.Value().cases)
        {
            report << "| " << value.updateInterval << " | " << WinnerSetText(value.globalWinnerSet) << " | "
                   << value.medianGpuCandidateTimelineNanoseconds[0] << " | "
                   << value.medianGpuCandidateTimelineNanoseconds[1] << " | "
                   << value.medianGpuCandidateTimelineNanoseconds[2] << " | "
                   << value.medianGpuUpdateInvocationNanoseconds[0] << " | "
                   << value.medianGpuUpdateInvocationNanoseconds[1] << " | "
                   << value.medianGpuUpdateInvocationNanoseconds[2] << " | "
                   << value.medianGpuHoldInvocationNanoseconds[0] << " | "
                   << value.medianGpuHoldInvocationNanoseconds[1] << " | "
                   << value.medianGpuHoldInvocationNanoseconds[2] << " | "
                   << PairWinnerName(value.recomputeVersusGlobalMotor) << " | "
                   << PairWinnerName(value.globalMotorVersusFinalOutput) << " |\n";
        }
        report << "\n## Crossover classification\n\n"
               << "- A.EveryInvocationRecompute / B.GlobalMotorHistoryReuse: `"
               << TransitionName(analysis.Value().recomputeGlobalClassification) << "`\n"
               << "- B.GlobalMotorHistoryReuse / C.FinalOutputHistoryReuse: `"
               << TransitionName(analysis.Value().globalFinalClassification) << "`\n";
        for (const auto& transition : analysis.Value().recomputeGlobalTransitions)
        {
            report << "- A/B transition U" << transition.lowerUpdateInterval << "-U"
                   << transition.upperUpdateInterval << ": "
                   << CandidateLetterV1(transition.from) << "->" << CandidateLetterV1(transition.to) << "\n";
        }
        for (const auto& transition : analysis.Value().globalFinalTransitions)
        {
            report << "- B/C transition U" << transition.lowerUpdateInterval << "-U"
                   << transition.upperUpdateInterval << ": "
                   << CandidateLetterV1(transition.from) << "->" << CandidateLetterV1(transition.to) << "\n";
        }
        report << "\n## Observed history materialization position\n\n"
               << "- Per-U winner sets are authoritative only for this evidence profile.\n"
               << "- Overall observed winner set by median of the seven per-U medians: `"
               << WinnerSetText(analysis.Value().overallObservedWinnerSet) << "`\n"
               << "- Global winner set changes across U: `"
               << (analysis.Value().globalWinnerSetChanges ? "true" : "false") << "`\n\n"
               << "## Scope limitation\n\n"
               << "The result is limited to the measured adapter, driver, clock policy, fixed 128-invocation timeline, 4096 Work records, DirectPgaThroughConsumerV1, and the frozen Candidate corpus.\n"
               << "`SPIRAL 5 CLOSED` remains withheld until the Owner selects the next capability or explicitly closes the line.\n";
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
        std::filesystem::create_directories(outputDirectory);
        ValidateOrders();
        auto evidence = BuildSyntheticEvidence();
        auto write = WriteMeasurementArtifactsV1(evidence, outputDirectory);
        if (!write) return write;
        const auto evidencePath = outputDirectory / "spiral5_measurement_evidence_v1.bin";
        auto parsed = ReadMeasurementEvidenceV1(evidencePath);
        if (!parsed) return base::Result<void, std::string>::Failure(parsed.Error());
        const auto originalBytes = SerializeMeasurementEvidenceV1(evidence);
        const auto parsedBytes = SerializeMeasurementEvidenceV1(parsed.Value());
        Require(originalBytes == parsedBytes, "Measurement evidence round-trip changed canonical bytes.");
        auto corrupted = originalBytes;
        corrupted[corrupted.size() / 2] ^= std::byte{0x01};
        auto corruptWrite = base::WriteAllBytes(outputDirectory / "corrupted.bin", corrupted);
        if (!corruptWrite) return base::Result<void, std::string>::Failure(corruptWrite.Error());
        auto corruptRead = ReadMeasurementEvidenceV1(outputDirectory / "corrupted.bin");
        Require(!corruptRead, "Corrupted measurement evidence was accepted.");
        auto analysis = AnalyzeDecisionEvidenceV1(evidence);
        if (!analysis) return base::Result<void, std::string>::Failure(analysis.Error());
        Require(analysis.Value().recomputeGlobalClassification ==
            TransitionClassificationV1::SingleStableWinnerTransition,
            "Synthetic A/B crossover classification failed.");
        Require(analysis.Value().globalFinalClassification ==
            TransitionClassificationV1::SingleStableWinnerTransition,
            "Synthetic B/C crossover classification failed.");
        Require(analysis.Value().recomputeGlobalTransitions.size() == 1 &&
                analysis.Value().globalFinalTransitions.size() == 1,
            "Synthetic crossover transition count failed.");
        auto report = WriteDecisionEvidenceReportV1(evidence, outputDirectory);
        if (!report) return report;
        std::filesystem::remove(outputDirectory / "corrupted.bin");
        return base::Result<void, std::string>::Success();
    }
    catch (const std::exception& error)
    {
        return base::Result<void, std::string>::Failure(error.what());
    }
}
}
