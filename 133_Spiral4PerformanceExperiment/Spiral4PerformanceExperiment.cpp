#include "Spiral4PerformanceExperiment.h"

#include "../00_Foundation/BinaryIO.h"
#include "../00_Foundation/FileIO.h"
#include "../00_Foundation/Sha256.h"
#include "../128_ActiveWorkCandidateFamily/ActiveWorkCandidateFamily.h"
#include "../129_ActiveWorkCandidateFamilyPlanner/ActiveWorkCandidateFamilyPlanner.h"

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

namespace sge4_5::spiral4::performance
{
namespace
{
constexpr std::string_view EvidenceMagic =
    "SGE4-5.Spiral4.MeasurementEvidence.V1";
constexpr std::string_view MeasurementProfileDomain =
    "SGE4-5.Spiral4.MeasurementProfile.V1";
constexpr std::string_view OrderBalanceDomain =
    "SGE4-5.Spiral4.BalancedOrders.V1";
constexpr std::string_view VerificationContextDomain =
    "SGE4-5.Spiral4.CandidateFamilyVerificationContext.V1";

constexpr std::string_view ArchitectureEvidenceHex =
    "60cf81fb8bac565cd35d696612b7050f464576dede5cef7e2fe61b0cdfa7c32e";
constexpr std::string_view ControlledRecoveryEvidenceHex =
    "4ccaa3f2bb250a0af16037c74da8cff8f65a0834f912803adc3c8c7cf607b4d9";
constexpr std::string_view FreshRematerializationEvidenceHex =
    "e1794f15d6808b4c220ee0d2527564551ee7fa1d2daa70b4f360531819f13dc3";

void Require(bool condition, std::string_view message)
{
    if (!condition)
    {
        throw std::runtime_error(std::string(message));
    }
}

base::Digest256 LiteralIdentity(std::string_view value)
{
    return base::Sha256(
        std::as_bytes(std::span(value.data(), value.size())));
}

base::Digest256 DigestFromHex(std::string_view value)
{
    if (value.size() != 64)
    {
        throw std::runtime_error("Digest hex length must be 64.");
    }

    auto nibble = [](char c) -> std::uint8_t
    {
        if (c >= '0' && c <= '9')
        {
            return static_cast<std::uint8_t>(c - '0');
        }
        if (c >= 'a' && c <= 'f')
        {
            return static_cast<std::uint8_t>(10 + c - 'a');
        }
        if (c >= 'A' && c <= 'F')
        {
            return static_cast<std::uint8_t>(10 + c - 'A');
        }
        throw std::runtime_error("Digest contains a non-hex character.");
    };

    base::Digest256 result{};
    for (std::size_t index = 0; index < result.size(); ++index)
    {
        const std::uint8_t high = nibble(value[index * 2]);
        const std::uint8_t low = nibble(value[index * 2 + 1]);
        result[index] = static_cast<std::byte>((high << 4u) | low);
    }
    return result;
}

void WriteString(base::BinaryWriter& writer, std::string_view value)
{
    writer.WriteU32(static_cast<std::uint32_t>(value.size()));
    writer.WriteBytes(
        std::as_bytes(std::span(value.data(), value.size())));
}

struct ReadText final { std::string value; };

base::Result<ReadText, std::string> ReadString(
    base::BinaryReader& reader,
    std::uint32_t maximumLength = 16u * 1024u * 1024u)
{
    auto size = reader.ReadU32();
    if (!size)
    {
        return base::Result<ReadText, std::string>::Failure(
            size.Error());
    }
    if (size.Value() > maximumLength)
    {
        return base::Result<ReadText, std::string>::Failure(
            "String length exceeds the evidence limit.");
    }
    auto bytes = reader.ReadBytes(size.Value());
    if (!bytes)
    {
        return base::Result<ReadText, std::string>::Failure(
            bytes.Error());
    }
    return base::Result<ReadText, std::string>::Success(ReadText{
        std::string(
            reinterpret_cast<const char*>(bytes.Value().data()),
            bytes.Value().size())});
}

void WriteDigest(base::BinaryWriter& writer, const base::Digest256& value)
{
    writer.WriteBytes(value);
}

base::Result<base::Digest256, std::string> ReadDigest(
    base::BinaryReader& reader)
{
    auto bytes = reader.ReadBytes(32);
    if (!bytes)
    {
        return base::Result<base::Digest256, std::string>::Failure(
            bytes.Error());
    }
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
    if (!value)
    {
        return base::Result<double, std::string>::Failure(value.Error());
    }
    return base::Result<double, std::string>::Success(
        std::bit_cast<double>(value.Value()));
}

void WriteBool(base::BinaryWriter& writer, bool value)
{
    writer.WriteU8(value ? 1u : 0u);
}

base::Result<bool, std::string> ReadBool(base::BinaryReader& reader)
{
    auto value = reader.ReadU8();
    if (!value)
    {
        return base::Result<bool, std::string>::Failure(value.Error());
    }
    if (value.Value() > 1u)
    {
        return base::Result<bool, std::string>::Failure(
            "Boolean evidence byte is not canonical.");
    }
    return base::Result<bool, std::string>::Success(
        value.Value() != 0u);
}

std::string JsonEscape(std::string_view value)
{
    std::string result;
    result.reserve(value.size());
    for (const char c : value)
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

std::size_t CandidateIndex(const CandidateIdV1& candidate)
{
    const auto canonical = CanonicalCandidateIdsV1();
    for (std::size_t index = 0; index < canonical.size(); ++index)
    {
        if (canonical[index] == candidate)
        {
            return index;
        }
    }
    throw std::runtime_error("Candidate is not in the canonical CU6 corpus.");
}

bool IsBatchCandidate(const CandidateIdV1& candidate)
{
    return candidate.kind ==
        family_contract::CandidateFamilyKindV1::
            BatchedExecuteIndirectDispatch;
}

std::string PairWinnerName(PairWinnerV1 value)
{
    switch (value)
    {
    case PairWinnerV1::Left: return "Left";
    case PairWinnerV1::Right: return "Right";
    case PairWinnerV1::NoiseEquivalent: return "NoiseEquivalent";
    default: return "Unknown";
    }
}

std::string TransitionClassificationName(
    TransitionClassificationV1 value)
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
    default:
        return "Unknown";
    }
}

bool IsFiniteNonNegative(double value)
{
    return std::isfinite(value) && value >= 0.0;
}

void ValidateConfig(const MeasurementConfigV1& config)
{
    Require(
        config.warmupCyclesPerOrder <= 1000,
        "Warmup cycle count is unreasonable.");
    Require(
        config.measurementCyclesPerOrder > 0 &&
            config.measurementCyclesPerOrder <= 1000,
        "Measurement cycle count is outside the qualified range.");
    Require(
        config.runCount > 0 && config.runCount <= 100,
        "Run count is outside the qualified range.");
    Require(
        !config.clockPolicy.empty(),
        "Clock policy must be recorded.");
    Require(
        config.primaryMetric == "GpuCandidatePathNanoseconds",
        "Primary metric differs from the frozen CU6 contract.");
    Require(
        IsFiniteNonNegative(config.absoluteNoiseFloorNanoseconds),
        "Absolute noise floor is invalid.");
    Require(
        std::isfinite(config.relativeNoiseFloor) &&
            config.relativeNoiseFloor >= 0.0 &&
            config.relativeNoiseFloor <= 1.0,
        "Relative noise floor is invalid.");
}

base::Digest256 BuildMeasurementProfileIdentity(
    const MeasurementConfigV1& config)
{
    base::BinaryWriter writer;
    WriteString(writer, MeasurementProfileDomain);
    writer.WriteU32(config.warmupCyclesPerOrder);
    writer.WriteU32(config.measurementCyclesPerOrder);
    writer.WriteU32(config.runCount);
    writer.WriteU32(config.adapterIndex);
    WriteString(writer, config.clockPolicy);
    WriteString(writer, config.primaryMetric);
    WriteDouble(writer, config.absoluteNoiseFloorNanoseconds);
    WriteDouble(writer, config.relativeNoiseFloor);
    return base::Sha256(std::move(writer).Take());
}

base::Digest256 BuildOrderBalanceIdentity()
{
    base::BinaryWriter writer;
    WriteString(writer, OrderBalanceDomain);
    const auto orders = CanonicalBalancedOrdersV1();
    writer.WriteU32(static_cast<std::uint32_t>(orders.size()));
    for (const auto& order : orders)
    {
        for (const std::uint32_t index : order)
        {
            writer.WriteU32(index);
        }
    }
    return base::Sha256(std::move(writer).Take());
}

base::Digest256 CanonicalVerificationContextIdentity()
{
    base::BinaryWriter writer;
    WriteString(writer, VerificationContextDomain);
    writer.WriteBytes(LiteralIdentity(
        "SGE4-5.Spiral4.TargetProfile.D3D12.CandidateFamily.WarpQualified.V1"));
    writer.WriteBytes(LiteralIdentity(
        "SGE4-5.Spiral4.ActiveCountContract.Prefix.Nmax4096.V1"));
    writer.WriteBytes(LiteralIdentity(
        "SGE4-5.Spiral4.Observation.ActiveReference.InactiveSentinel.V1"));
    return base::Sha256(std::move(writer).Take());
}

bool IsZeroDigest(const base::Digest256& value)
{
    return std::all_of(
        value.begin(),
        value.end(),
        [](std::byte byte) { return byte == std::byte{0}; });
}

void ValidateOrderBalance()
{
    const auto orders = CanonicalBalancedOrdersV1();
    Require(
        orders.size() == CanonicalOrderCountV1,
        "Canonical order count differs from 14.");

    std::array<std::array<std::uint32_t, CanonicalCandidateCountV1>,
               CanonicalCandidateCountV1> counts{};
    for (const auto& order : orders)
    {
        std::array<bool, CanonicalCandidateCountV1> seen{};
        for (std::size_t position = 0; position < order.size(); ++position)
        {
            const std::uint32_t candidate = order[position];
            Require(
                candidate < CanonicalCandidateCountV1,
                "Balanced order contains an invalid Candidate index.");
            Require(
                !seen[candidate],
                "Balanced order repeats a Candidate.");
            seen[candidate] = true;
            ++counts[candidate][position];
        }
    }

    for (const auto& candidateCounts : counts)
    {
        for (const std::uint32_t count : candidateCounts)
        {
            Require(
                count == 2u,
                "Each Candidate must appear twice in every order position.");
        }
    }
}

void ValidateEvidenceShape(const MeasurementEvidenceV1& evidence)
{
    Require(
        evidence.schemaVersion == MeasurementEvidenceSchemaVersionV1,
        "Measurement evidence schema mismatch.");
    ValidateConfig(evidence.config);
    ValidateOrderBalance();
    Require(
        evidence.measurementProfileIdentity ==
            BuildMeasurementProfileIdentity(evidence.config),
        "Measurement profile identity mismatch.");
    Require(
        evidence.orderBalanceIdentity == BuildOrderBalanceIdentity(),
        "Order-balance identity mismatch.");
    Require(
        evidence.architectureEvidenceIdentity ==
            DigestFromHex(ArchitectureEvidenceHex),
        "Architecture evidence binding differs from CU5.");
    Require(
        evidence.controlledRecoveryEvidenceIdentity ==
            DigestFromHex(ControlledRecoveryEvidenceHex),
        "Controlled Recovery evidence binding differs from CU5.");
    Require(
        evidence.freshRematerializationEvidenceIdentity ==
            DigestFromHex(FreshRematerializationEvidenceHex),
        "Fresh-rematerialization evidence binding differs from CU5.");
    Require(
        evidence.candidates.size() == CanonicalCandidateCountV1,
        "Measurement evidence must contain seven Candidate profiles.");
    Require(
        evidence.cases.size() == CanonicalActiveCountCaseCountV1,
        "Measurement evidence must contain nineteen Active Count cases.");
    Require(
        evidence.adapter.timestampFrequency > 0 &&
            !evidence.adapter.description.empty() &&
            !IsZeroDigest(evidence.adapter.fingerprint),
        "Measurement evidence contains an incomplete hardware identity.");
    const auto canonicalSemantic =
        semantic::BuildCanonicalActiveWorkSemanticV1();
    Require(
        evidence.semanticIdentity ==
            semantic::ActiveWorkSemanticIdentityV1(canonicalSemantic),
        "Measurement Semantic identity is not canonical.");
    Require(
        evidence.verificationContextIdentity ==
            CanonicalVerificationContextIdentity(),
        "Measurement verification-context identity is not canonical.");

    const auto canonicalCandidates = CanonicalCandidateIdsV1();
    for (std::size_t index = 0; index < evidence.candidates.size(); ++index)
    {
        const auto& profile = evidence.candidates[index];
        Require(
            profile.candidate == canonicalCandidates[index],
            "Candidate profile order differs from the frozen CU6 corpus.");
        Require(
            !IsZeroDigest(profile.rawCandidateIdentity) &&
                !IsZeroDigest(profile.verifiedPlanIdentity) &&
                !IsZeroDigest(profile.verifierCertificateIdentity) &&
                !IsZeroDigest(profile.artifactIdentity) &&
                !IsZeroDigest(profile.frozenBindingIdentity) &&
                !IsZeroDigest(profile.producerProgramTemplateIdentity) &&
                !IsZeroDigest(profile.consumerProgramTemplateIdentity) &&
                !IsZeroDigest(profile.producerShaderBytecodeDigest) &&
                !IsZeroDigest(profile.consumerShaderBytecodeDigest),
            "Candidate profile contains an unbound identity.");

        if (index == 0)
        {
            Require(
                profile.maxBatchSlots == 0 &&
                    profile.argumentStrideBytes == 0 &&
                    profile.argumentBufferBytes == 0 &&
                    profile.maxCommandCount == 0,
                "Fixed Maximum measurement profile is structurally invalid.");
        }
        else if (index == 1)
        {
            Require(
                profile.maxBatchSlots == 1 &&
                    profile.argumentStrideBytes == 12 &&
                    profile.argumentBufferBytes == 12 &&
                    profile.maxCommandCount == 1,
                "Single Indirect measurement profile is structurally invalid.");
        }
        else
        {
            const std::uint32_t expectedSlots =
                4096u / profile.candidate.batchSize;
            Require(
                profile.maxBatchSlots == expectedSlots &&
                    profile.argumentStrideBytes == 16 &&
                    profile.argumentBufferBytes == expectedSlots * 16u &&
                    profile.maxCommandCount == expectedSlots,
                "Batched measurement profile is structurally invalid.");
        }
    }

    const auto activeCounts = semantic::ActiveCountCorpusV1();
    Require(
        activeCounts.size() == CanonicalActiveCountCaseCountV1,
        "Canonical Active Count corpus no longer contains nineteen cases.");

    const std::uint64_t expectedSamplesPerCase =
        static_cast<std::uint64_t>(CanonicalOrderCountV1) *
        evidence.config.measurementCyclesPerOrder *
        evidence.config.runCount *
        CanonicalCandidateCountV1;

    for (std::size_t caseIndex = 0;
         caseIndex < evidence.cases.size();
         ++caseIndex)
    {
        const auto& measurementCase = evidence.cases[caseIndex];
        Require(
            measurementCase.activeWorkCount == activeCounts[caseIndex],
            "Active Count case order differs from the frozen corpus.");
        Require(
            measurementCase.samples.size() == expectedSamplesPerCase,
            "Measurement case sample count is incomplete.");

        std::vector<std::vector<std::uint32_t>> positionCounts(
            CanonicalCandidateCountV1,
            std::vector<std::uint32_t>(CanonicalCandidateCountV1, 0));
        std::vector<bool> observedSlots(
            static_cast<std::size_t>(expectedSamplesPerCase),
            false);
        const auto balancedOrders = CanonicalBalancedOrdersV1();

        for (const auto& sample : measurementCase.samples)
        {
            Require(
                sample.activeWorkCount == measurementCase.activeWorkCount,
                "Sample Active Count differs from its case.");
            Require(
                sample.run < evidence.config.runCount,
                "Sample run index is out of range.");
            Require(
                sample.orderIndex < CanonicalOrderCountV1,
                "Sample order index is out of range.");
            Require(
                sample.cycle < evidence.config.measurementCyclesPerOrder,
                "Sample cycle index is out of range.");
            Require(
                sample.orderPosition < CanonicalCandidateCountV1,
                "Sample order position is out of range.");
            const std::size_t candidateIndex = CandidateIndex(sample.candidate);
            Require(
                candidateIndex ==
                    balancedOrders[sample.orderIndex][sample.orderPosition],
                "Measured Candidate differs from its canonical balanced order.");
            const std::size_t slot =
                (((static_cast<std::size_t>(sample.run) *
                    CanonicalOrderCountV1 + sample.orderIndex) *
                    evidence.config.measurementCyclesPerOrder +
                    sample.cycle) * CanonicalCandidateCountV1) +
                sample.orderPosition;
            Require(
                slot < observedSlots.size() && !observedSlots[slot],
                "Measurement evidence repeats one run/order/cycle/position slot.");
            observedSlots[slot] = true;
            ++positionCounts[candidateIndex][sample.orderPosition];

            const double values[] = {
                sample.activeInputPreparationNanoseconds,
                sample.commandRecordingNanoseconds,
                sample.submitAndWaitNanoseconds,
                sample.numericObservationNanoseconds,
                sample.cpuEndToEndNanoseconds,
                sample.gpuArgumentGenerationNanoseconds,
                sample.gpuStateTransitionNanoseconds,
                sample.gpuExecutionNanoseconds,
                sample.gpuOutputSynchronizationNanoseconds,
                sample.gpuObservationCopyNanoseconds,
                sample.gpuCandidatePathNanoseconds,
                sample.gpuTotalNanoseconds,
                sample.maxAbsoluteError,
                sample.maxRelativeError
            };
            for (const double value : values)
            {
                Require(
                    IsFiniteNonNegative(value),
                    "Timing or numeric evidence contains an invalid value.");
            }
            Require(
                sample.activeMismatchCount == 0 &&
                sample.nonFiniteCount == 0 &&
                sample.homogeneousCoordinateMismatchCount == 0 &&
                sample.duplicateWorkCount == 0 &&
                sample.missingWorkCount == 0 &&
                sample.reorderedWorkCount == 0 &&
                sample.outOfRangeWorkCount == 0 &&
                sample.inactiveTailPreserved,
                "Measurement evidence contains a failed Observation Contract.");
        }

        Require(
            std::all_of(
                observedSlots.begin(),
                observedSlots.end(),
                [](bool value) { return value; }),
            "Measurement evidence omits one run/order/cycle/position slot.");

        const std::uint32_t expectedPositionCount =
            2u * evidence.config.measurementCyclesPerOrder *
            evidence.config.runCount;
        for (const auto& candidateCounts : positionCounts)
        {
            for (const std::uint32_t count : candidateCounts)
            {
                Require(
                    count == expectedPositionCount,
                    "Measured Candidate order is not position-balanced.");
            }
        }
    }
}

void WriteCandidateId(base::BinaryWriter& writer, const CandidateIdV1& value)
{
    writer.WriteU32(static_cast<std::uint32_t>(value.kind));
    writer.WriteU32(value.batchSize);
}

base::Result<CandidateIdV1, std::string> ReadCandidateId(
    base::BinaryReader& reader)
{
    auto kind = reader.ReadU32();
    if (!kind)
    {
        return base::Result<CandidateIdV1, std::string>::Failure(
            kind.Error());
    }
    auto batchSize = reader.ReadU32();
    if (!batchSize)
    {
        return base::Result<CandidateIdV1, std::string>::Failure(
            batchSize.Error());
    }
    CandidateIdV1 value{
        static_cast<family_contract::CandidateFamilyKindV1>(kind.Value()),
        batchSize.Value()
    };
    try
    {
        (void)CandidateIndex(value);
    }
    catch (const std::exception& error)
    {
        return base::Result<CandidateIdV1, std::string>::Failure(
            error.what());
    }
    return base::Result<CandidateIdV1, std::string>::Success(value);
}

void WriteCandidateProfile(
    base::BinaryWriter& writer,
    const CandidateProfileV1& value)
{
    WriteCandidateId(writer, value.candidate);
    writer.WriteU32(value.maxBatchSlots);
    writer.WriteU32(value.argumentStrideBytes);
    writer.WriteU32(value.argumentBufferBytes);
    writer.WriteU32(value.maxCommandCount);
    WriteDigest(writer, value.rawCandidateIdentity);
    WriteDigest(writer, value.verifiedPlanIdentity);
    WriteDigest(writer, value.verifierCertificateIdentity);
    WriteDigest(writer, value.artifactIdentity);
    WriteDigest(writer, value.frozenBindingIdentity);
    WriteDigest(writer, value.producerProgramTemplateIdentity);
    WriteDigest(writer, value.consumerProgramTemplateIdentity);
    WriteDigest(writer, value.producerShaderBytecodeDigest);
    WriteDigest(writer, value.consumerShaderBytecodeDigest);
}

base::Result<CandidateProfileV1, std::string> ReadCandidateProfile(
    base::BinaryReader& reader)
{
    CandidateProfileV1 value;
    auto candidate = ReadCandidateId(reader);
    if (!candidate)
    {
        return base::Result<CandidateProfileV1, std::string>::Failure(
            candidate.Error());
    }
    value.candidate = candidate.Value();

    auto maxBatchSlots = reader.ReadU32();
    auto argumentStrideBytes = reader.ReadU32();
    auto argumentBufferBytes = reader.ReadU32();
    auto maxCommandCount = reader.ReadU32();
    if (!maxBatchSlots || !argumentStrideBytes ||
        !argumentBufferBytes || !maxCommandCount)
    {
        return base::Result<CandidateProfileV1, std::string>::Failure(
            "Candidate profile integer field is truncated.");
    }
    value.maxBatchSlots = maxBatchSlots.Value();
    value.argumentStrideBytes = argumentStrideBytes.Value();
    value.argumentBufferBytes = argumentBufferBytes.Value();
    value.maxCommandCount = maxCommandCount.Value();

    base::Digest256* digests[] = {
        &value.rawCandidateIdentity,
        &value.verifiedPlanIdentity,
        &value.verifierCertificateIdentity,
        &value.artifactIdentity,
        &value.frozenBindingIdentity,
        &value.producerProgramTemplateIdentity,
        &value.consumerProgramTemplateIdentity,
        &value.producerShaderBytecodeDigest,
        &value.consumerShaderBytecodeDigest
    };
    for (auto* digest : digests)
    {
        auto read = ReadDigest(reader);
        if (!read)
        {
            return base::Result<CandidateProfileV1, std::string>::Failure(
                read.Error());
        }
        *digest = read.Value();
    }
    return base::Result<CandidateProfileV1, std::string>::Success(value);
}

void WriteTimingSample(base::BinaryWriter& writer, const TimingSampleV1& value)
{
    writer.WriteU32(value.run);
    writer.WriteU32(value.orderIndex);
    writer.WriteU32(value.cycle);
    writer.WriteU32(value.orderPosition);
    WriteCandidateId(writer, value.candidate);
    writer.WriteU32(value.activeWorkCount);

    const double timingValues[] = {
        value.activeInputPreparationNanoseconds,
        value.commandRecordingNanoseconds,
        value.submitAndWaitNanoseconds,
        value.numericObservationNanoseconds,
        value.cpuEndToEndNanoseconds,
        value.gpuArgumentGenerationNanoseconds,
        value.gpuStateTransitionNanoseconds,
        value.gpuExecutionNanoseconds,
        value.gpuOutputSynchronizationNanoseconds,
        value.gpuObservationCopyNanoseconds,
        value.gpuCandidatePathNanoseconds,
        value.gpuTotalNanoseconds
    };
    for (const double timing : timingValues)
    {
        WriteDouble(writer, timing);
    }

    writer.WriteU32(value.issuedThreadGroupCount);
    writer.WriteU32(value.activeMismatchCount);
    writer.WriteU32(value.nonFiniteCount);
    writer.WriteU32(value.homogeneousCoordinateMismatchCount);
    writer.WriteU32(value.duplicateWorkCount);
    writer.WriteU32(value.missingWorkCount);
    writer.WriteU32(value.reorderedWorkCount);
    writer.WriteU32(value.outOfRangeWorkCount);
    WriteBool(writer, value.inactiveTailPreserved);
    WriteDouble(writer, value.maxAbsoluteError);
    WriteDouble(writer, value.maxRelativeError);
    writer.WriteU32(value.maxUlpError);
    WriteDigest(writer, value.outputDigest);
}

base::Result<TimingSampleV1, std::string> ReadTimingSample(
    base::BinaryReader& reader)
{
    TimingSampleV1 value;
    auto run = reader.ReadU32();
    auto orderIndex = reader.ReadU32();
    auto cycle = reader.ReadU32();
    auto orderPosition = reader.ReadU32();
    if (!run || !orderIndex || !cycle || !orderPosition)
    {
        return base::Result<TimingSampleV1, std::string>::Failure(
            "Timing sample header is truncated.");
    }
    value.run = run.Value();
    value.orderIndex = orderIndex.Value();
    value.cycle = cycle.Value();
    value.orderPosition = orderPosition.Value();
    auto candidate = ReadCandidateId(reader);
    if (!candidate)
    {
        return base::Result<TimingSampleV1, std::string>::Failure(
            candidate.Error());
    }
    value.candidate = candidate.Value();
    auto active = reader.ReadU32();
    if (!active)
    {
        return base::Result<TimingSampleV1, std::string>::Failure(
            active.Error());
    }
    value.activeWorkCount = active.Value();

    double* timings[] = {
        &value.activeInputPreparationNanoseconds,
        &value.commandRecordingNanoseconds,
        &value.submitAndWaitNanoseconds,
        &value.numericObservationNanoseconds,
        &value.cpuEndToEndNanoseconds,
        &value.gpuArgumentGenerationNanoseconds,
        &value.gpuStateTransitionNanoseconds,
        &value.gpuExecutionNanoseconds,
        &value.gpuOutputSynchronizationNanoseconds,
        &value.gpuObservationCopyNanoseconds,
        &value.gpuCandidatePathNanoseconds,
        &value.gpuTotalNanoseconds
    };
    for (auto* timing : timings)
    {
        auto read = ReadDouble(reader);
        if (!read)
        {
            return base::Result<TimingSampleV1, std::string>::Failure(
                read.Error());
        }
        *timing = read.Value();
    }

    std::uint32_t* counters[] = {
        &value.issuedThreadGroupCount,
        &value.activeMismatchCount,
        &value.nonFiniteCount,
        &value.homogeneousCoordinateMismatchCount,
        &value.duplicateWorkCount,
        &value.missingWorkCount,
        &value.reorderedWorkCount,
        &value.outOfRangeWorkCount
    };
    for (auto* counter : counters)
    {
        auto read = reader.ReadU32();
        if (!read)
        {
            return base::Result<TimingSampleV1, std::string>::Failure(
                read.Error());
        }
        *counter = read.Value();
    }

    auto inactive = ReadBool(reader);
    if (!inactive)
    {
        return base::Result<TimingSampleV1, std::string>::Failure(
            inactive.Error());
    }
    value.inactiveTailPreserved = inactive.Value();

    auto absolute = ReadDouble(reader);
    auto relative = ReadDouble(reader);
    auto ulp = reader.ReadU32();
    if (!absolute || !relative || !ulp)
    {
        return base::Result<TimingSampleV1, std::string>::Failure(
            "Timing sample numeric summary is truncated.");
    }
    value.maxAbsoluteError = absolute.Value();
    value.maxRelativeError = relative.Value();
    value.maxUlpError = ulp.Value();

    auto outputDigest = ReadDigest(reader);
    if (!outputDigest)
    {
        return base::Result<TimingSampleV1, std::string>::Failure(
            outputDigest.Error());
    }
    value.outputDigest = outputDigest.Value();
    return base::Result<TimingSampleV1, std::string>::Success(value);
}

void WriteAdapterProfile(
    base::BinaryWriter& writer,
    const AdapterProfileV1& value)
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

base::Result<AdapterProfileV1, std::string> ReadAdapterProfile(
    base::BinaryReader& reader)
{
    AdapterProfileV1 value;
    auto description = ReadString(reader, 4096);
    if (!description)
    {
        return base::Result<AdapterProfileV1, std::string>::Failure(
            description.Error());
    }
    value.description = std::move(description.Value().value);

    auto vendor = reader.ReadU32();
    auto device = reader.ReadU32();
    auto subsystem = reader.ReadU32();
    auto revision = reader.ReadU32();
    auto luidLow = reader.ReadU32();
    auto luidHigh = reader.ReadI32();
    auto driverVersion = reader.ReadU64();
    auto timestampFrequency = reader.ReadU64();
    if (!vendor || !device || !subsystem || !revision ||
        !luidLow || !luidHigh || !driverVersion || !timestampFrequency)
    {
        return base::Result<AdapterProfileV1, std::string>::Failure(
            "Adapter profile is truncated.");
    }
    value.vendorId = vendor.Value();
    value.deviceId = device.Value();
    value.subsystemId = subsystem.Value();
    value.revision = revision.Value();
    value.luidLow = luidLow.Value();
    value.luidHigh = luidHigh.Value();
    value.driverVersion = driverVersion.Value();
    value.timestampFrequency = timestampFrequency.Value();

    auto uma = ReadBool(reader);
    auto coherent = ReadBool(reader);
    if (!uma || !coherent)
    {
        return base::Result<AdapterProfileV1, std::string>::Failure(
            "Adapter architecture flags are truncated.");
    }
    value.uma = uma.Value();
    value.cacheCoherentUma = coherent.Value();
    auto fingerprint = ReadDigest(reader);
    if (!fingerprint)
    {
        return base::Result<AdapterProfileV1, std::string>::Failure(
            fingerprint.Error());
    }
    value.fingerprint = fingerprint.Value();
    return base::Result<AdapterProfileV1, std::string>::Success(value);
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

base::Result<MeasurementConfigV1, std::string> ReadConfig(
    base::BinaryReader& reader)
{
    MeasurementConfigV1 value;
    auto warmup = reader.ReadU32();
    auto measurement = reader.ReadU32();
    auto runs = reader.ReadU32();
    auto adapterIndex = reader.ReadU32();
    if (!warmup || !measurement || !runs || !adapterIndex)
    {
        return base::Result<MeasurementConfigV1, std::string>::Failure(
            "Measurement config is truncated.");
    }
    value.warmupCyclesPerOrder = warmup.Value();
    value.measurementCyclesPerOrder = measurement.Value();
    value.runCount = runs.Value();
    value.adapterIndex = adapterIndex.Value();

    auto clockPolicy = ReadString(reader, 4096);
    auto primaryMetric = ReadString(reader, 4096);
    auto absoluteNoise = ReadDouble(reader);
    auto relativeNoise = ReadDouble(reader);
    if (!clockPolicy || !primaryMetric || !absoluteNoise || !relativeNoise)
    {
        return base::Result<MeasurementConfigV1, std::string>::Failure(
            "Measurement config text or noise policy is truncated.");
    }
    value.clockPolicy = std::move(clockPolicy.Value().value);
    value.primaryMetric = std::move(primaryMetric.Value().value);
    value.absoluteNoiseFloorNanoseconds = absoluteNoise.Value();
    value.relativeNoiseFloor = relativeNoise.Value();
    try
    {
        ValidateConfig(value);
    }
    catch (const std::exception& error)
    {
        return base::Result<MeasurementConfigV1, std::string>::Failure(
            error.what());
    }
    return base::Result<MeasurementConfigV1, std::string>::Success(value);
}

std::vector<double> CandidateMetricValues(
    const MeasurementCaseV1& measurementCase,
    const CandidateIdV1& candidate)
{
    std::vector<double> values;
    for (const auto& sample : measurementCase.samples)
    {
        if (sample.candidate == candidate)
        {
            values.push_back(sample.gpuCandidatePathNanoseconds);
        }
    }
    if (values.empty())
    {
        throw std::runtime_error(
            "Candidate has no samples in one Active Count case.");
    }
    return values;
}

double Median(std::vector<double> values)
{
    Require(!values.empty(), "Median requires at least one value.");
    std::sort(values.begin(), values.end());
    const std::size_t middle = values.size() / 2;
    if ((values.size() & 1u) != 0u)
    {
        return values[middle];
    }
    return (values[middle - 1] + values[middle]) * 0.5;
}

bool NoiseEquivalent(
    double a,
    double b,
    const MeasurementConfigV1& config)
{
    const double best = std::min(a, b);
    const double threshold = std::max(
        config.absoluteNoiseFloorNanoseconds,
        best * config.relativeNoiseFloor);
    return std::abs(a - b) <= threshold;
}

PairWinnerV1 ComparePair(
    double left,
    double right,
    const MeasurementConfigV1& config)
{
    if (NoiseEquivalent(left, right, config))
    {
        return PairWinnerV1::NoiseEquivalent;
    }
    return left < right ? PairWinnerV1::Left : PairWinnerV1::Right;
}

std::pair<TransitionClassificationV1, std::vector<WinnerTransitionV1>>
ClassifyPairTransitions(
    const std::vector<PerActiveCountDecisionV1>& cases,
    bool fixedVersusSingle)
{
    std::vector<WinnerTransitionV1> transitions;
    bool unresolved = false;
    std::optional<PairWinnerV1> previousSide;
    std::optional<CandidateIdV1> previousWinner;
    std::uint32_t previousActiveCount = 0;

    const CandidateIdV1 fixed{
        family_contract::CandidateFamilyKindV1::FixedMaximumGuarded,
        0};
    const CandidateIdV1 single{
        family_contract::CandidateFamilyKindV1::
            SingleExecuteIndirectDispatch,
        0};

    for (const auto& value : cases)
    {
        const PairWinnerV1 result = fixedVersusSingle
            ? value.fixedVersusSingle
            : value.singleVersusBestBatch;
        if (result == PairWinnerV1::NoiseEquivalent)
        {
            unresolved = true;
            continue;
        }

        CandidateIdV1 winner;
        if (fixedVersusSingle)
        {
            winner = result == PairWinnerV1::Left ? fixed : single;
        }
        else
        {
            winner = result == PairWinnerV1::Left
                ? single
                : value.bestObservedBatch;
        }

        if (previousSide &&
            result != *previousSide &&
            previousWinner)
        {
            transitions.push_back(WinnerTransitionV1{
                previousActiveCount,
                value.activeWorkCount,
                *previousWinner,
                winner});
        }

        previousSide = result;
        previousWinner = winner;
        previousActiveCount = value.activeWorkCount;
    }

    if (unresolved)
    {
        return {
            TransitionClassificationV1::NoiseEquivalentOrUnresolved,
            std::move(transitions)};
    }
    if (transitions.empty())
    {
        return {
            TransitionClassificationV1::NoObservedWinnerTransition,
            {}};
    }
    if (transitions.size() == 1)
    {
        return {
            TransitionClassificationV1::SingleStableWinnerTransition,
            std::move(transitions)};
    }
    return {
        TransitionClassificationV1::MultipleWinnerTransitions,
        std::move(transitions)};
}

std::string WinnerSetName(const std::vector<CandidateIdV1>& winners)
{
    if (winners.empty())
    {
        return "None";
    }
    if (winners.size() == 1)
    {
        return CandidateNameV1(winners.front());
    }
    std::string result = "NoiseEquivalent(";
    for (std::size_t index = 0; index < winners.size(); ++index)
    {
        if (index != 0)
        {
            result += ",";
        }
        result += CandidateNameV1(winners[index]);
    }
    result += ")";
    return result;
}

std::string FormatNanoseconds(double value)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(1) << value;
    return stream.str();
}

std::string OrderName(
    const std::array<std::uint32_t, CanonicalCandidateCountV1>& order)
{
    std::string result;
    for (const std::uint32_t index : order)
    {
        result.push_back(CandidateLetterV1(CanonicalCandidateIdsV1()[index]));
    }
    return result;
}

MeasurementEvidenceV1 BuildSyntheticEvidence()
{
    MeasurementEvidenceV1 evidence;
    evidence.captureUnixNanoseconds = 1;
    evidence.config = MeasurementConfigV1{};
    evidence.adapter.description = "Synthetic Spiral 4 Adapter";
    evidence.adapter.vendorId = 0x1234;
    evidence.adapter.deviceId = 0x5678;
    evidence.adapter.timestampFrequency = 1'000'000'000ull;
    evidence.adapter.fingerprint = LiteralIdentity(
        "SGE4-5.Spiral4.SyntheticAdapter.V1");
    evidence.architectureEvidenceIdentity =
        DigestFromHex(ArchitectureEvidenceHex);
    evidence.controlledRecoveryEvidenceIdentity =
        DigestFromHex(ControlledRecoveryEvidenceHex);
    evidence.freshRematerializationEvidenceIdentity =
        DigestFromHex(FreshRematerializationEvidenceHex);

    const auto semanticValue = semantic::BuildCanonicalActiveWorkSemanticV1();
    evidence.semanticIdentity =
        semantic::ActiveWorkSemanticIdentityV1(semanticValue);
    evidence.verificationContextIdentity =
        CanonicalVerificationContextIdentity();
    evidence.measurementProfileIdentity =
        BuildMeasurementProfileIdentity(evidence.config);
    evidence.orderBalanceIdentity = BuildOrderBalanceIdentity();

    const auto candidates = CanonicalCandidateIdsV1();
    for (std::size_t index = 0; index < candidates.size(); ++index)
    {
        CandidateProfileV1 profile;
        profile.candidate = candidates[index];
        profile.maxBatchSlots = IsBatchCandidate(candidates[index])
            ? 4096u / candidates[index].batchSize
            : (index == 1 ? 1u : 0u);
        profile.argumentStrideBytes = IsBatchCandidate(candidates[index])
            ? 16u
            : (index == 1 ? 12u : 0u);
        profile.argumentBufferBytes =
            profile.maxBatchSlots * profile.argumentStrideBytes;
        profile.maxCommandCount = profile.maxBatchSlots;
        const std::string name = CandidateNameV1(candidates[index]);
        profile.rawCandidateIdentity = LiteralIdentity(name + ".Raw");
        profile.verifiedPlanIdentity = LiteralIdentity(name + ".Plan");
        profile.verifierCertificateIdentity = LiteralIdentity(name + ".Certificate");
        profile.artifactIdentity = LiteralIdentity(name + ".Artifact");
        profile.frozenBindingIdentity = LiteralIdentity(name + ".Binding");
        profile.producerProgramTemplateIdentity = LiteralIdentity(name + ".Producer");
        profile.consumerProgramTemplateIdentity = LiteralIdentity(name + ".Consumer");
        profile.producerShaderBytecodeDigest = LiteralIdentity(name + ".ProducerShader");
        profile.consumerShaderBytecodeDigest = LiteralIdentity(name + ".ConsumerShader");
        evidence.candidates.push_back(std::move(profile));
    }

    const auto activeCounts = semantic::ActiveCountCorpusV1();
    const auto orders = CanonicalBalancedOrdersV1();
    for (const std::uint32_t active : activeCounts)
    {
        MeasurementCaseV1 measurementCase;
        measurementCase.activeWorkCount = active;
        for (std::uint32_t run = 0; run < evidence.config.runCount; ++run)
        {
            for (std::uint32_t orderIndex = 0;
                 orderIndex < orders.size();
                 ++orderIndex)
            {
                for (std::uint32_t cycle = 0;
                     cycle < evidence.config.measurementCyclesPerOrder;
                     ++cycle)
                {
                    for (std::uint32_t position = 0;
                         position < CanonicalCandidateCountV1;
                         ++position)
                    {
                        const std::uint32_t candidateIndex =
                            orders[orderIndex][position];
                        const CandidateIdV1 candidate =
                            candidates[candidateIndex];

                        double primary = 0.0;
                        if (candidateIndex == 0)
                        {
                            primary = active <= 64 ? 500.0 :
                                1200.0 + static_cast<double>(active) * 0.10;
                        }
                        else if (candidateIndex == 1)
                        {
                            primary = active <= 64 ? 1000.0 :
                                500.0 + static_cast<double>(active) * 0.05;
                        }
                        else
                        {
                            const double syntheticBestBatch =
                                active <= 1024 ? 64.0 :
                                active <= 2048 ? 256.0 :
                                1024.0;
                            const double batchPenalty =
                                std::abs(
                                    static_cast<double>(candidate.batchSize) -
                                    syntheticBestBatch) * 0.2;
                            primary = active <= 512 ?
                                1300.0 + batchPenalty :
                                200.0 +
                                    static_cast<double>(active) * 0.02 +
                                    batchPenalty;
                        }
                        primary += static_cast<double>(
                            (run + orderIndex + cycle + position) % 3u);

                        TimingSampleV1 sample;
                        sample.run = run;
                        sample.orderIndex = orderIndex;
                        sample.cycle = cycle;
                        sample.orderPosition = position;
                        sample.candidate = candidate;
                        sample.activeWorkCount = active;
                        sample.activeInputPreparationNanoseconds = 20.0;
                        sample.commandRecordingNanoseconds = 50.0;
                        sample.submitAndWaitNanoseconds = primary + 200.0;
                        sample.numericObservationNanoseconds = 30.0;
                        sample.cpuEndToEndNanoseconds = primary + 300.0;
                        sample.gpuArgumentGenerationNanoseconds =
                            candidateIndex == 0 ? 0.0 : 100.0;
                        sample.gpuStateTransitionNanoseconds =
                            candidateIndex == 0 ? 0.0 : 50.0;
                        sample.gpuExecutionNanoseconds =
                            std::max(0.0, primary - 200.0);
                        sample.gpuOutputSynchronizationNanoseconds = 50.0;
                        sample.gpuObservationCopyNanoseconds = 100.0;
                        sample.gpuCandidatePathNanoseconds = primary;
                        sample.gpuTotalNanoseconds = primary + 100.0;
                        sample.issuedThreadGroupCount =
                            active == 0 ? 0u : 1u + ((active - 1u) / 64u);
                        sample.inactiveTailPreserved = true;
                        sample.outputDigest = LiteralIdentity(
                            std::to_string(active) + ".Output");
                        measurementCase.samples.push_back(std::move(sample));
                    }
                }
            }
        }
        evidence.cases.push_back(std::move(measurementCase));
    }
    return evidence;
}
}

std::string CandidateNameV1(const CandidateIdV1& value)
{
    using K = family_contract::CandidateFamilyKindV1;
    switch (value.kind)
    {
    case K::FixedMaximumGuarded:
        return "A.FixedMaximumGuarded";
    case K::SingleExecuteIndirectDispatch:
        return "B.SingleExecuteIndirectDispatch";
    case K::BatchedExecuteIndirectDispatch:
        return "C.BatchedExecuteIndirect.B" +
            std::to_string(value.batchSize);
    default:
        return "Unknown";
    }
}

char CandidateLetterV1(const CandidateIdV1& value)
{
    using K = family_contract::CandidateFamilyKindV1;
    switch (value.kind)
    {
    case K::FixedMaximumGuarded: return 'A';
    case K::SingleExecuteIndirectDispatch: return 'B';
    case K::BatchedExecuteIndirectDispatch:
        switch (value.batchSize)
        {
        case 64: return 'C';
        case 128: return 'D';
        case 256: return 'E';
        case 512: return 'F';
        case 1024: return 'G';
        default: return '?';
        }
    default: return '?';
    }
}

std::array<CandidateIdV1, CanonicalCandidateCountV1>
CanonicalCandidateIdsV1()
{
    using K = family_contract::CandidateFamilyKindV1;
    return {{
        {K::FixedMaximumGuarded, 0},
        {K::SingleExecuteIndirectDispatch, 0},
        {K::BatchedExecuteIndirectDispatch, 64},
        {K::BatchedExecuteIndirectDispatch, 128},
        {K::BatchedExecuteIndirectDispatch, 256},
        {K::BatchedExecuteIndirectDispatch, 512},
        {K::BatchedExecuteIndirectDispatch, 1024}
    }};
}

std::vector<std::array<std::uint32_t, CanonicalCandidateCountV1>>
CanonicalBalancedOrdersV1()
{
    std::vector<std::array<std::uint32_t, CanonicalCandidateCountV1>> orders;
    orders.reserve(CanonicalOrderCountV1);

    const std::array<std::uint32_t, CanonicalCandidateCountV1> forward{
        0, 1, 2, 3, 4, 5, 6};
    const std::array<std::uint32_t, CanonicalCandidateCountV1> reverse{
        0, 6, 5, 4, 3, 2, 1};

    auto addRotations = [&](const auto& baseOrder)
    {
        for (std::uint32_t rotation = 0;
             rotation < CanonicalCandidateCountV1;
             ++rotation)
        {
            std::array<std::uint32_t, CanonicalCandidateCountV1> order{};
            for (std::uint32_t position = 0;
                 position < CanonicalCandidateCountV1;
                 ++position)
            {
                order[position] = baseOrder[
                    (position + rotation) % CanonicalCandidateCountV1];
            }
            orders.push_back(order);
        }
    };

    addRotations(forward);
    addRotations(reverse);
    return orders;
}

std::vector<std::byte>
SerializeMeasurementEvidenceV1(const MeasurementEvidenceV1& evidence)
{
    ValidateEvidenceShape(evidence);

    base::BinaryWriter writer;
    WriteString(writer, EvidenceMagic);
    writer.WriteU32(evidence.schemaVersion);
    writer.WriteU64(evidence.captureUnixNanoseconds);
    WriteConfig(writer, evidence.config);
    WriteAdapterProfile(writer, evidence.adapter);
    WriteDigest(writer, evidence.architectureEvidenceIdentity);
    WriteDigest(writer, evidence.controlledRecoveryEvidenceIdentity);
    WriteDigest(writer, evidence.freshRematerializationEvidenceIdentity);
    WriteDigest(writer, evidence.semanticIdentity);
    WriteDigest(writer, evidence.verificationContextIdentity);
    WriteDigest(writer, evidence.measurementProfileIdentity);
    WriteDigest(writer, evidence.orderBalanceIdentity);

    writer.WriteU32(static_cast<std::uint32_t>(evidence.candidates.size()));
    for (const auto& candidate : evidence.candidates)
    {
        WriteCandidateProfile(writer, candidate);
    }

    writer.WriteU32(static_cast<std::uint32_t>(evidence.cases.size()));
    for (const auto& measurementCase : evidence.cases)
    {
        writer.WriteU32(measurementCase.activeWorkCount);
        writer.WriteU32(
            static_cast<std::uint32_t>(measurementCase.samples.size()));
        for (const auto& sample : measurementCase.samples)
        {
            WriteTimingSample(writer, sample);
        }
    }

    const auto checksum = base::Sha256(writer.Bytes());
    WriteDigest(writer, checksum);
    return std::move(writer).Take();
}

base::Result<MeasurementEvidenceV1, std::string>
ReadMeasurementEvidenceV1(const std::filesystem::path& evidencePath)
{
    auto bytesResult = base::ReadAllBytes(evidencePath);
    if (!bytesResult)
    {
        return base::Result<MeasurementEvidenceV1, std::string>::Failure(
            bytesResult.Error());
    }

    const auto& bytes = bytesResult.Value();
    if (bytes.size() < 32u)
    {
        return base::Result<MeasurementEvidenceV1, std::string>::Failure(
            "Measurement evidence is shorter than its checksum.");
    }

    const std::span<const std::byte> payload(
        bytes.data(), bytes.size() - 32u);
    const std::span<const std::byte> checksumBytes(
        bytes.data() + bytes.size() - 32u, 32u);
    const auto expectedChecksum = base::Sha256(payload);
    if (!std::equal(
            expectedChecksum.begin(),
            expectedChecksum.end(),
            checksumBytes.begin()))
    {
        return base::Result<MeasurementEvidenceV1, std::string>::Failure(
            "Measurement evidence checksum mismatch.");
    }

    base::BinaryReader reader(payload);
    auto magic = ReadString(reader, 4096);
    if (!magic)
    {
        return base::Result<MeasurementEvidenceV1, std::string>::Failure(
            magic.Error());
    }
    if (magic.Value().value != EvidenceMagic)
    {
        return base::Result<MeasurementEvidenceV1, std::string>::Failure(
            "Measurement evidence magic mismatch.");
    }

    MeasurementEvidenceV1 evidence;
    auto schema = reader.ReadU32();
    auto capture = reader.ReadU64();
    if (!schema || !capture)
    {
        return base::Result<MeasurementEvidenceV1, std::string>::Failure(
            "Measurement evidence header is truncated.");
    }
    evidence.schemaVersion = schema.Value();
    evidence.captureUnixNanoseconds = capture.Value();

    auto config = ReadConfig(reader);
    auto adapter = ReadAdapterProfile(reader);
    if (!config || !adapter)
    {
        return base::Result<MeasurementEvidenceV1, std::string>::Failure(
            config ? adapter.Error() : config.Error());
    }
    evidence.config = std::move(config.Value());
    evidence.adapter = std::move(adapter.Value());

    base::Digest256* identities[] = {
        &evidence.architectureEvidenceIdentity,
        &evidence.controlledRecoveryEvidenceIdentity,
        &evidence.freshRematerializationEvidenceIdentity,
        &evidence.semanticIdentity,
        &evidence.verificationContextIdentity,
        &evidence.measurementProfileIdentity,
        &evidence.orderBalanceIdentity
    };
    for (auto* identity : identities)
    {
        auto read = ReadDigest(reader);
        if (!read)
        {
            return base::Result<MeasurementEvidenceV1, std::string>::Failure(
                read.Error());
        }
        *identity = read.Value();
    }

    auto candidateCount = reader.ReadU32();
    if (!candidateCount)
    {
        return base::Result<MeasurementEvidenceV1, std::string>::Failure(
            candidateCount.Error());
    }
    if (candidateCount.Value() != CanonicalCandidateCountV1)
    {
        return base::Result<MeasurementEvidenceV1, std::string>::Failure(
            "Measurement evidence Candidate count differs from seven.");
    }
    evidence.candidates.reserve(candidateCount.Value());
    for (std::uint32_t index = 0;
         index < candidateCount.Value();
         ++index)
    {
        auto profile = ReadCandidateProfile(reader);
        if (!profile)
        {
            return base::Result<MeasurementEvidenceV1, std::string>::Failure(
                profile.Error());
        }
        evidence.candidates.push_back(std::move(profile.Value()));
    }

    auto caseCount = reader.ReadU32();
    if (!caseCount)
    {
        return base::Result<MeasurementEvidenceV1, std::string>::Failure(
            caseCount.Error());
    }
    if (caseCount.Value() != CanonicalActiveCountCaseCountV1)
    {
        return base::Result<MeasurementEvidenceV1, std::string>::Failure(
            "Measurement evidence Active Count case count differs from nineteen.");
    }
    evidence.cases.reserve(caseCount.Value());
    for (std::uint32_t caseIndex = 0;
         caseIndex < caseCount.Value();
         ++caseIndex)
    {
        MeasurementCaseV1 measurementCase;
        auto active = reader.ReadU32();
        auto sampleCount = reader.ReadU32();
        if (!active || !sampleCount)
        {
            return base::Result<MeasurementEvidenceV1, std::string>::Failure(
                "Measurement case header is truncated.");
        }
        const std::uint64_t maximumSamples =
            static_cast<std::uint64_t>(CanonicalOrderCountV1) *
            1000ull * 100ull * CanonicalCandidateCountV1;
        if (sampleCount.Value() > maximumSamples)
        {
            return base::Result<MeasurementEvidenceV1, std::string>::Failure(
                "Measurement sample count exceeds the parser limit.");
        }
        measurementCase.activeWorkCount = active.Value();
        measurementCase.samples.reserve(sampleCount.Value());
        for (std::uint32_t sampleIndex = 0;
             sampleIndex < sampleCount.Value();
             ++sampleIndex)
        {
            auto sample = ReadTimingSample(reader);
            if (!sample)
            {
                return base::Result<MeasurementEvidenceV1, std::string>::Failure(
                    sample.Error());
            }
            measurementCase.samples.push_back(std::move(sample.Value()));
        }
        evidence.cases.push_back(std::move(measurementCase));
    }

    if (reader.Remaining() != 0)
    {
        return base::Result<MeasurementEvidenceV1, std::string>::Failure(
            "Measurement evidence contains trailing payload bytes.");
    }

    try
    {
        ValidateEvidenceShape(evidence);
    }
    catch (const std::exception& error)
    {
        return base::Result<MeasurementEvidenceV1, std::string>::Failure(
            error.what());
    }
    return base::Result<MeasurementEvidenceV1, std::string>::Success(
        std::move(evidence));
}

base::Result<DecisionAnalysisV1, std::string>
AnalyzeDecisionEvidenceV1(const MeasurementEvidenceV1& evidence)
{
    try
    {
        ValidateEvidenceShape(evidence);
        DecisionAnalysisV1 analysis;
        const auto candidates = CanonicalCandidateIdsV1();
        std::optional<std::vector<CandidateIdV1>> previousWinnerSet;

        for (const auto& measurementCase : evidence.cases)
        {
            PerActiveCountDecisionV1 decision;
            decision.activeWorkCount = measurementCase.activeWorkCount;

            for (std::size_t index = 0;
                 index < candidates.size();
                 ++index)
            {
                decision.medianGpuCandidatePathNanoseconds[index] =
                    Median(CandidateMetricValues(
                        measurementCase,
                        candidates[index]));
            }

            const double best = *std::min_element(
                decision.medianGpuCandidatePathNanoseconds.begin(),
                decision.medianGpuCandidatePathNanoseconds.end());
            const double globalThreshold = std::max(
                evidence.config.absoluteNoiseFloorNanoseconds,
                best * evidence.config.relativeNoiseFloor);
            for (std::size_t index = 0;
                 index < candidates.size();
                 ++index)
            {
                if (decision.medianGpuCandidatePathNanoseconds[index] - best <=
                    globalThreshold)
                {
                    decision.globalWinnerSet.push_back(candidates[index]);
                }
            }

            std::size_t bestBatchIndex = 2;
            for (std::size_t index = 3;
                 index < candidates.size();
                 ++index)
            {
                if (decision.medianGpuCandidatePathNanoseconds[index] <
                    decision.medianGpuCandidatePathNanoseconds[bestBatchIndex])
                {
                    bestBatchIndex = index;
                }
            }
            decision.bestObservedBatch = candidates[bestBatchIndex];
            analysis.observedBestBatchSizes.push_back(
                decision.bestObservedBatch.batchSize);

            decision.fixedVersusSingle = ComparePair(
                decision.medianGpuCandidatePathNanoseconds[0],
                decision.medianGpuCandidatePathNanoseconds[1],
                evidence.config);
            decision.singleVersusBestBatch = ComparePair(
                decision.medianGpuCandidatePathNanoseconds[1],
                decision.medianGpuCandidatePathNanoseconds[bestBatchIndex],
                evidence.config);

            if (previousWinnerSet &&
                *previousWinnerSet != decision.globalWinnerSet)
            {
                analysis.globalWinnerChanges = true;
            }
            previousWinnerSet = decision.globalWinnerSet;
            analysis.cases.push_back(std::move(decision));
        }

        const auto fixedSingle = ClassifyPairTransitions(
            analysis.cases,
            true);
        analysis.fixedSingleClassification = fixedSingle.first;
        analysis.fixedSingleTransitions = fixedSingle.second;

        const auto singleBatch = ClassifyPairTransitions(
            analysis.cases,
            false);
        analysis.singleBatchClassification = singleBatch.first;
        analysis.singleBatchTransitions = singleBatch.second;

        return base::Result<DecisionAnalysisV1, std::string>::Success(
            std::move(analysis));
    }
    catch (const std::exception& error)
    {
        return base::Result<DecisionAnalysisV1, std::string>::Failure(
            error.what());
    }
}

base::Result<void, std::string>
WriteMeasurementArtifactsV1(
    const MeasurementEvidenceV1& evidence,
    const std::filesystem::path& outputDirectory)
{
    try
    {
        ValidateEvidenceShape(evidence);
        std::filesystem::create_directories(outputDirectory);

        const auto bytes = SerializeMeasurementEvidenceV1(evidence);
        auto written = base::WriteAllBytes(
            outputDirectory / "spiral4_measurement_evidence_v1.bin",
            bytes);
        if (!written)
        {
            return written;
        }

        const auto analysisResult = AnalyzeDecisionEvidenceV1(evidence);
        if (!analysisResult)
        {
            return base::Result<void, std::string>::Failure(
                analysisResult.Error());
        }
        const auto& analysis = analysisResult.Value();

        std::ofstream csv(
            outputDirectory / "spiral4_measurement_samples_v1.csv",
            std::ios::binary | std::ios::trunc);
        if (!csv)
        {
            return base::Result<void, std::string>::Failure(
                "Could not open Spiral 4 measurement CSV.");
        }
        csv
            << "activeWorkCount,run,orderIndex,orderName,cycle,orderPosition,"
               "candidate,batchSize,activeInputPreparationNs,commandRecordingNs,"
               "submitAndWaitNs,numericObservationNs,cpuEndToEndNs,"
               "gpuArgumentGenerationNs,gpuStateTransitionNs,gpuExecutionNs,"
               "gpuOutputSynchronizationNs,gpuObservationCopyNs,"
               "gpuCandidatePathNs,gpuTotalNs,issuedThreadGroups,"
               "maxAbsoluteError,maxRelativeError,maxUlpError,outputDigest\n";
        const auto orders = CanonicalBalancedOrdersV1();
        csv << std::fixed << std::setprecision(3);
        for (const auto& measurementCase : evidence.cases)
        {
            for (const auto& sample : measurementCase.samples)
            {
                csv
                    << sample.activeWorkCount << ','
                    << sample.run << ','
                    << sample.orderIndex << ','
                    << OrderName(orders[sample.orderIndex]) << ','
                    << sample.cycle << ','
                    << sample.orderPosition << ','
                    << CandidateNameV1(sample.candidate) << ','
                    << sample.candidate.batchSize << ','
                    << sample.activeInputPreparationNanoseconds << ','
                    << sample.commandRecordingNanoseconds << ','
                    << sample.submitAndWaitNanoseconds << ','
                    << sample.numericObservationNanoseconds << ','
                    << sample.cpuEndToEndNanoseconds << ','
                    << sample.gpuArgumentGenerationNanoseconds << ','
                    << sample.gpuStateTransitionNanoseconds << ','
                    << sample.gpuExecutionNanoseconds << ','
                    << sample.gpuOutputSynchronizationNanoseconds << ','
                    << sample.gpuObservationCopyNanoseconds << ','
                    << sample.gpuCandidatePathNanoseconds << ','
                    << sample.gpuTotalNanoseconds << ','
                    << sample.issuedThreadGroupCount << ','
                    << sample.maxAbsoluteError << ','
                    << sample.maxRelativeError << ','
                    << sample.maxUlpError << ','
                    << base::ToHex(sample.outputDigest) << '\n';
            }
        }
        csv.close();
        if (!csv)
        {
            return base::Result<void, std::string>::Failure(
                "Could not finish Spiral 4 measurement CSV.");
        }

        std::ofstream json(
            outputDirectory / "spiral4_measurement_summary_v1.json",
            std::ios::binary | std::ios::trunc);
        if (!json)
        {
            return base::Result<void, std::string>::Failure(
                "Could not open Spiral 4 measurement summary JSON.");
        }
        json << "{\n";
        json << "  \"schema\": \"SGE4-5.Spiral4.MeasurementSummary.V1\",\n";
        json << "  \"adapter\": {\n";
        json << "    \"description\": \""
             << JsonEscape(evidence.adapter.description) << "\",\n";
        json << "    \"vendorId\": " << evidence.adapter.vendorId << ",\n";
        json << "    \"deviceId\": " << evidence.adapter.deviceId << ",\n";
        json << "    \"driverVersion\": "
             << evidence.adapter.driverVersion << ",\n";
        json << "    \"timestampFrequency\": "
             << evidence.adapter.timestampFrequency << ",\n";
        json << "    \"fingerprint\": \""
             << base::ToHex(evidence.adapter.fingerprint) << "\"\n";
        json << "  },\n";
        json << "  \"config\": {\n";
        json << "    \"warmupCyclesPerOrder\": "
             << evidence.config.warmupCyclesPerOrder << ",\n";
        json << "    \"measurementCyclesPerOrder\": "
             << evidence.config.measurementCyclesPerOrder << ",\n";
        json << "    \"runCount\": "
             << evidence.config.runCount << ",\n";
        json << "    \"orderCount\": "
             << CanonicalOrderCountV1 << ",\n";
        json << "    \"primaryMetric\": \""
             << JsonEscape(evidence.config.primaryMetric) << "\",\n";
        json << "    \"absoluteNoiseFloorNanoseconds\": "
             << evidence.config.absoluteNoiseFloorNanoseconds << ",\n";
        json << "    \"relativeNoiseFloor\": "
             << evidence.config.relativeNoiseFloor << "\n";
        json << "  },\n";
        json << "  \"architectureEvidenceIdentity\": \""
             << base::ToHex(evidence.architectureEvidenceIdentity) << "\",\n";
        json << "  \"measurementEvidenceIdentity\": \""
             << base::ToHex(base::Sha256(bytes)) << "\",\n";
        json << "  \"fixedSingleClassification\": \""
             << TransitionClassificationName(
                    analysis.fixedSingleClassification)
             << "\",\n";
        json << "  \"singleBatchClassification\": \""
             << TransitionClassificationName(
                    analysis.singleBatchClassification)
             << "\",\n";
        json << "  \"runtimePolicyAuthorization\": \"None\",\n";
        json << "  \"nextCapabilitySelection\": \"DeferredByOwner\",\n";
        json << "  \"selectionStatus\": \"OWNER_DECISION_REQUIRED\",\n";
        json << "  \"cases\": [\n";
        for (std::size_t caseIndex = 0;
             caseIndex < analysis.cases.size();
             ++caseIndex)
        {
            const auto& value = analysis.cases[caseIndex];
            json << "    {\n";
            json << "      \"activeWorkCount\": "
                 << value.activeWorkCount << ",\n";
            json << "      \"winnerSet\": [";
            for (std::size_t winnerIndex = 0;
                 winnerIndex < value.globalWinnerSet.size();
                 ++winnerIndex)
            {
                if (winnerIndex != 0) json << ", ";
                json << "\""
                     << JsonEscape(CandidateNameV1(
                            value.globalWinnerSet[winnerIndex]))
                     << "\"";
            }
            json << "],\n";
            json << "      \"bestObservedBatchSize\": "
                 << value.bestObservedBatch.batchSize << ",\n";
            json << "      \"fixedVersusSingle\": \""
                 << PairWinnerName(value.fixedVersusSingle) << "\",\n";
            json << "      \"singleVersusBestBatch\": \""
                 << PairWinnerName(value.singleVersusBestBatch) << "\",\n";
            json << "      \"medianGpuCandidatePathNanoseconds\": [";
            for (std::size_t index = 0;
                 index < value.medianGpuCandidatePathNanoseconds.size();
                 ++index)
            {
                if (index != 0) json << ", ";
                json << std::fixed << std::setprecision(3)
                     << value.medianGpuCandidatePathNanoseconds[index];
            }
            json << "]\n";
            json << "    }"
                 << (caseIndex + 1 == analysis.cases.size() ? "\n" : ",\n");
        }
        json << "  ]\n";
        json << "}\n";
        json.close();
        if (!json)
        {
            return base::Result<void, std::string>::Failure(
                "Could not finish Spiral 4 measurement summary JSON.");
        }

        return base::Result<void, std::string>::Success();
    }
    catch (const std::exception& error)
    {
        return base::Result<void, std::string>::Failure(error.what());
    }
}

base::Result<void, std::string>
WriteDecisionEvidenceReportV1(
    const MeasurementEvidenceV1& evidence,
    const std::filesystem::path& outputDirectory)
{
    auto analysisResult = AnalyzeDecisionEvidenceV1(evidence);
    if (!analysisResult)
    {
        return base::Result<void, std::string>::Failure(
            analysisResult.Error());
    }

    try
    {
        std::filesystem::create_directories(outputDirectory);
        const auto& analysis = analysisResult.Value();
        std::ofstream report(
            outputDirectory / "SPIRAL4_DECISION_EVIDENCE_REPORT.md",
            std::ios::binary | std::ios::trunc);
        if (!report)
        {
            return base::Result<void, std::string>::Failure(
                "Could not open Spiral 4 Decision Evidence Report.");
        }

        const auto evidenceBytes = SerializeMeasurementEvidenceV1(evidence);
        report << "# SGE4-5 Spiral 4 Decision Evidence Report\n\n";
        report << "- Status: Measurement Evidence Complete; Owner decision pending\n";
        report << "- Adapter: `" << evidence.adapter.description << "`\n";
        report << "- Adapter fingerprint: `"
               << base::ToHex(evidence.adapter.fingerprint) << "`\n";
        report << "- Driver version: `"
               << evidence.adapter.driverVersion << "`\n";
        report << "- Timestamp frequency: `"
               << evidence.adapter.timestampFrequency << "`\n";
        report << "- Clock policy: `"
               << evidence.config.clockPolicy << "`\n";
        report << "- Primary metric: `"
               << evidence.config.primaryMetric << "`\n";
        report << "- Balanced orders: `14`\n";
        report << "- Samples per Candidate and Nf: `"
               << CanonicalOrderCountV1 *
                    evidence.config.measurementCyclesPerOrder *
                    evidence.config.runCount
               << "`\n";
        report << "- Absolute noise floor: `"
               << evidence.config.absoluteNoiseFloorNanoseconds
               << " ns`\n";
        report << "- Relative noise floor: `"
               << evidence.config.relativeNoiseFloor * 100.0
               << "%`\n";
        report << "- CU5 Architecture evidence: `"
               << base::ToHex(evidence.architectureEvidenceIdentity)
               << "`\n";
        report << "- Measurement evidence SHA-256: `"
               << base::ToHex(base::Sha256(evidenceBytes))
               << "`\n\n";

        report << "## Frozen non-selection boundary\n\n";
        report << "```text\n";
        report << "RuntimePolicyAuthorization = None\n";
        report << "NextCapabilitySelection = DeferredByOwner\n";
        report << "SelectionStatus = OWNER_DECISION_REQUIRED\n";
        report << "```\n\n";
        report << "The observed winner is evidence, not Runtime adaptive policy. "
                  "No Candidate is authorized as a universal winner by this report.\n\n";

        report << "## Per-Nf medians\n\n";
        report << "| Nf | A Fixed | B Single | C64 | C128 | C256 | C512 | C1024 | Winner set | Best B | A/B | B/C |\n";
        report << "|---:|---:|---:|---:|---:|---:|---:|---:|---|---:|---|---|\n";
        for (const auto& value : analysis.cases)
        {
            report << "| " << value.activeWorkCount;
            for (const double median :
                 value.medianGpuCandidatePathNanoseconds)
            {
                report << " | " << FormatNanoseconds(median);
            }
            report << " | " << WinnerSetName(value.globalWinnerSet)
                   << " | " << value.bestObservedBatch.batchSize
                   << " | " << PairWinnerName(value.fixedVersusSingle)
                   << " | " << PairWinnerName(value.singleVersusBestBatch)
                   << " |\n";
        }
        report << "\n";

        report << "## Pairwise crossover classification\n\n";
        report << "### Fixed Maximum versus Single Indirect\n\n";
        report << "Classification: `"
               << TransitionClassificationName(
                    analysis.fixedSingleClassification)
               << "`\n\n";
        if (analysis.fixedSingleTransitions.empty())
        {
            report << "No non-noise winner transition was recorded.\n\n";
        }
        else
        {
            for (const auto& transition :
                 analysis.fixedSingleTransitions)
            {
                report << "- Nf " << transition.lowerActiveWorkCount
                       << " to " << transition.upperActiveWorkCount
                       << ": `" << CandidateNameV1(transition.from)
                       << " -> " << CandidateNameV1(transition.to)
                       << "`\n";
            }
            report << "\n";
        }

        report << "### Single Indirect versus best observed Batched candidate\n\n";
        report << "Classification: `"
               << TransitionClassificationName(
                    analysis.singleBatchClassification)
               << "`\n\n";
        if (analysis.singleBatchTransitions.empty())
        {
            report << "No non-noise winner transition was recorded.\n\n";
        }
        else
        {
            for (const auto& transition :
                 analysis.singleBatchTransitions)
            {
                report << "- Nf " << transition.lowerActiveWorkCount
                       << " to " << transition.upperActiveWorkCount
                       << ": `" << CandidateNameV1(transition.from)
                       << " -> " << CandidateNameV1(transition.to)
                       << "`\n";
            }
            report << "\n";
        }

        std::set<std::uint32_t> uniqueBatchSizes(
            analysis.observedBestBatchSizes.begin(),
            analysis.observedBestBatchSizes.end());
        report << "## Best observed Batch size\n\n";
        if (uniqueBatchSizes.size() == 1)
        {
            report << "One Batch size was best at every measured Nf: `B"
                   << *uniqueBatchSizes.begin() << "`.\n\n";
        }
        else
        {
            report << "The best observed Batch size changed across Nf. "
                      "Observed sizes: `";
            bool first = true;
            for (const auto batchSize : uniqueBatchSizes)
            {
                if (!first) report << ", ";
                report << "B" << batchSize;
                first = false;
            }
            report << "`.\n\n";
        }

        report << "## Candidate identities\n\n";
        report << "| Candidate | Verified Plan | Certificate | Artifact | Frozen binding |\n";
        report << "|---|---|---|---|---|\n";
        for (const auto& profile : evidence.candidates)
        {
            report << "| " << CandidateNameV1(profile.candidate)
                   << " | `" << base::ToHex(profile.verifiedPlanIdentity)
                   << "` | `" << base::ToHex(profile.verifierCertificateIdentity)
                   << "` | `" << base::ToHex(profile.artifactIdentity)
                   << "` | `" << base::ToHex(profile.frozenBindingIdentity)
                   << "` |\n";
        }
        report << "\n";

        report << "## Owner boundary\n\n";
        report << "Architecture Complete remains established by CU5. "
                  "This CU6 report completes the measurement experiment, "
                  "but Spiral 4 remains open until the Owner records the next "
                  "capability selection. Performance superiority and crossover "
                  "are not completion gates.\n";
        report.close();
        if (!report)
        {
            return base::Result<void, std::string>::Failure(
                "Could not finish Spiral 4 Decision Evidence Report.");
        }
        return base::Result<void, std::string>::Success();
    }
    catch (const std::exception& error)
    {
        return base::Result<void, std::string>::Failure(error.what());
    }
}

base::Result<void, std::string>
RunFormatAndDecisionSelfTestV1(
    const std::filesystem::path& outputDirectory)
{
    try
    {
        std::filesystem::remove_all(outputDirectory);
        std::filesystem::create_directories(outputDirectory);
        ValidateOrderBalance();

        const auto evidence = BuildSyntheticEvidence();
        const auto bytes = SerializeMeasurementEvidenceV1(evidence);
        auto written = base::WriteAllBytes(
            outputDirectory / "spiral4_measurement_evidence_v1.bin",
            bytes);
        if (!written)
        {
            return written;
        }

        auto read = ReadMeasurementEvidenceV1(
            outputDirectory / "spiral4_measurement_evidence_v1.bin");
        if (!read)
        {
            return base::Result<void, std::string>::Failure(read.Error());
        }
        const auto roundTrip = SerializeMeasurementEvidenceV1(read.Value());
        Require(roundTrip == bytes, "Measurement round-trip bytes differ.");

        auto corrupted = bytes;
        Require(corrupted.size() > 128, "Synthetic evidence is unexpectedly small.");
        corrupted[96] ^= std::byte{0x01};
        auto corruptedWritten = base::WriteAllBytes(
            outputDirectory / "spiral4_measurement_corrupt_v1.bin",
            corrupted);
        if (!corruptedWritten)
        {
            return corruptedWritten;
        }
        auto corruptedRead = ReadMeasurementEvidenceV1(
            outputDirectory / "spiral4_measurement_corrupt_v1.bin");
        Require(!corruptedRead, "Corrupted measurement evidence was accepted.");

        auto analysis = AnalyzeDecisionEvidenceV1(read.Value());
        if (!analysis)
        {
            return base::Result<void, std::string>::Failure(
                analysis.Error());
        }
        Require(
            analysis.Value().fixedSingleClassification ==
                TransitionClassificationV1::SingleStableWinnerTransition,
            "Synthetic Fixed/Single crossover is not single and stable.");
        Require(
            analysis.Value().fixedSingleTransitions.size() == 1,
            "Synthetic Fixed/Single transition count differs from one.");
        Require(
            analysis.Value().singleBatchClassification ==
                TransitionClassificationV1::SingleStableWinnerTransition,
            "Synthetic Single/Batch crossover is not single and stable.");
        Require(
            analysis.Value().singleBatchTransitions.size() == 1,
            "Synthetic Single/Batch transition count differs from one.");
        for (const std::uint32_t expectedBatch : {64u, 256u, 1024u})
        {
            Require(
                std::find(
                    analysis.Value().observedBestBatchSizes.begin(),
                    analysis.Value().observedBestBatchSizes.end(),
                    expectedBatch) !=
                    analysis.Value().observedBestBatchSizes.end(),
                "Synthetic decision analysis did not observe the expected "
                "best-Batch change.");
        }

        auto artifacts = WriteMeasurementArtifactsV1(
            read.Value(), outputDirectory);
        if (!artifacts)
        {
            return artifacts;
        }
        auto report = WriteDecisionEvidenceReportV1(
            read.Value(), outputDirectory);
        if (!report)
        {
            return report;
        }

        return base::Result<void, std::string>::Success();
    }
    catch (const std::exception& error)
    {
        return base::Result<void, std::string>::Failure(error.what());
    }
}
}
