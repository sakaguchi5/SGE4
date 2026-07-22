#include "Spiral5TemporalContracts.h"

#include "../00_Foundation/BinaryIO.h"
#include "../120_ActiveWorkSemantic/ActiveWorkSemantic.h"
#include "../121_Spiral4Contracts/Spiral4Contracts.h"

#include <array>
#include <cstring>
#include <span>
#include <string_view>
#include <stdexcept>
#include <utility>

namespace sge4_5::spiral5::contract
{
namespace
{
constexpr std::uint32_t ArtifactMagic = 0x31544853u; // "S5HT"
constexpr std::uint16_t ArtifactVersion = 1;
constexpr std::uint16_t ArtifactKind = 1;
constexpr std::uint32_t DispatchArgumentBytes = 12;
constexpr std::uint32_t HistoryBytes =
    semantic::BoneCountV1 * static_cast<std::uint32_t>(sizeof(semantic::PgaMotorRecordV1));
constexpr std::size_t DigestBytes = 32;
constexpr std::size_t PayloadBytes = 4 + 2 + 2 + (19 * 4) + (6 * DigestBytes);
constexpr std::size_t CompleteBytes = PayloadBytes + DigestBytes;

TemporalContractErrorV1 Error(std::string stage, std::string message)
{
    return {std::move(stage), std::move(message)};
}

void WriteDigest(base::BinaryWriter& writer, const base::Digest256& value)
{
    writer.WriteBytes(value);
}

base::Result<base::Digest256, TemporalContractErrorV1>
ReadDigest(base::BinaryReader& reader)
{
    auto bytes = reader.ReadBytes(DigestBytes);
    if (!bytes)
        return base::Result<base::Digest256, TemporalContractErrorV1>::Failure(
            Error("parse/digest", bytes.Error()));
    base::Digest256 result{};
    std::memcpy(result.data(), bytes.Value().data(), result.size());
    return base::Result<base::Digest256, TemporalContractErrorV1>::Success(result);
}

base::Digest256 ProgramIdentity(std::string_view name)
{
    return base::Sha256(std::as_bytes(std::span(name.data(), name.size())));
}

base::Digest256 Spiral4IndirectIdentity()
{
    const auto active = spiral4::semantic::BuildCanonicalActiveWorkSemanticV1();
    return spiral4::contract::BuildCu2SingleIndirectArtifactV1(active).ArtifactIdentity();
}
}

FrozenGlobalMotorHistoryArtifactV1::FrozenGlobalMotorHistoryArtifactV1(
    TemporalExtensionStrategyV1 extensionStrategy,
    TemporalCandidateKindV1 candidateKind,
    TemporalHistoryRoleV1 historyRole,
    RetainedHistoryCompletionV1 completionPolicy,
    std::uint32_t timelineLength,
    std::uint32_t workCount,
    std::uint32_t threadsPerGroup,
    std::uint32_t historyDepth,
    std::uint32_t historyBytes,
    std::uint32_t localMotorBytesPerGeneration,
    std::uint32_t argumentStrideBytes,
    std::uint32_t maxCommandCount,
    std::uint32_t argumentProducerDispatchX,
    std::uint32_t hierarchyUpdateDispatchX,
    std::uint32_t hierarchyHoldDispatchX,
    std::uint32_t consumerDispatchX,
    base::Digest256 semanticIdentity,
    base::Digest256 scheduleIdentity,
    base::Digest256 spiral4IndirectArtifactIdentity,
    base::Digest256 argumentProducerProgramIdentity,
    base::Digest256 hierarchyProgramIdentity,
    base::Digest256 consumerProgramIdentity,
    base::Digest256 artifactIdentity,
    std::vector<std::byte> bytes)
    : extensionStrategy_(extensionStrategy),
      candidateKind_(candidateKind),
      historyRole_(historyRole),
      completionPolicy_(completionPolicy),
      timelineLength_(timelineLength),
      workCount_(workCount),
      threadsPerGroup_(threadsPerGroup),
      historyDepth_(historyDepth),
      historyBytes_(historyBytes),
      localMotorBytesPerGeneration_(localMotorBytesPerGeneration),
      argumentStrideBytes_(argumentStrideBytes),
      maxCommandCount_(maxCommandCount),
      argumentProducerDispatchX_(argumentProducerDispatchX),
      hierarchyUpdateDispatchX_(hierarchyUpdateDispatchX),
      hierarchyHoldDispatchX_(hierarchyHoldDispatchX),
      consumerDispatchX_(consumerDispatchX),
      semanticIdentity_(semanticIdentity),
      scheduleIdentity_(scheduleIdentity),
      spiral4IndirectArtifactIdentity_(spiral4IndirectArtifactIdentity),
      argumentProducerProgramIdentity_(argumentProducerProgramIdentity),
      hierarchyProgramIdentity_(hierarchyProgramIdentity),
      consumerProgramIdentity_(consumerProgramIdentity),
      artifactIdentity_(artifactIdentity),
      bytes_(std::move(bytes))
{
}

FrozenGlobalMotorHistoryArtifactV1 BuildCu2GlobalMotorHistoryArtifactV1(
    const semantic::TemporalStateSemanticV1& semanticValue,
    const semantic::UpdateScheduleV1& schedule)
{
    auto scheduleValid = semantic::ValidateUpdateScheduleV1(schedule);
    if (!scheduleValid)
        throw std::invalid_argument(scheduleValid.Error().message);

    const auto semanticIdentity = semantic::TemporalStateSemanticIdentityV1(semanticValue);
    const auto scheduleIdentity = semantic::UpdateScheduleIdentityV1(schedule);
    const auto indirectIdentity = Spiral4IndirectIdentity();
    const auto argumentProgram = ProgramIdentity("SGE4-5.Spiral5.UpdateHoldArgumentProducer.V1");
    const auto hierarchyProgram = ProgramIdentity("SGE4-5.Spiral5.GlobalMotorHierarchyWriter.V1");
    const auto consumerProgram = ProgramIdentity("SGE4-5.Spiral5.DirectPgaHistoryConsumer.V1");

    base::BinaryWriter payload;
    payload.WriteU32(ArtifactMagic);
    payload.WriteU16(ArtifactVersion);
    payload.WriteU16(ArtifactKind);
    payload.WriteU32(static_cast<std::uint32_t>(TemporalExtensionStrategyV1::VersionedSidecarTemporalExtension));
    payload.WriteU32(static_cast<std::uint32_t>(TemporalCandidateKindV1::GlobalMotorHistoryReuse));
    payload.WriteU32(static_cast<std::uint32_t>(TemporalHistoryRoleV1::GlobalMotorHistory));
    payload.WriteU32(static_cast<std::uint32_t>(RetainedHistoryCompletionV1::ConsumerCompletionRetainsHistory));
    payload.WriteU32(1); // invalid until first successful update
    payload.WriteU32(1); // invalidate and explicit reseed after Recovery
    payload.WriteU32(semanticValue.timelineLength);
    payload.WriteU32(semanticValue.boneCount);
    payload.WriteU32(semanticValue.workCount);
    payload.WriteU32(semanticValue.threadsPerGroup);
    payload.WriteU32(semanticValue.historyDepth);
    payload.WriteU32(HistoryBytes);
    payload.WriteU32(HistoryBytes);
    payload.WriteU32(DispatchArgumentBytes);
    payload.WriteU32(1); // max command count
    payload.WriteU32(1); // producer dispatch X
    payload.WriteU32(1); // hierarchy update dispatch X
    payload.WriteU32(0); // hierarchy hold dispatch X
    payload.WriteU32(semanticValue.workCount / semanticValue.threadsPerGroup);
    WriteDigest(payload, semanticIdentity);
    WriteDigest(payload, scheduleIdentity);
    WriteDigest(payload, indirectIdentity);
    WriteDigest(payload, argumentProgram);
    WriteDigest(payload, hierarchyProgram);
    WriteDigest(payload, consumerProgram);

    if (payload.Size() != PayloadBytes)
        throw std::logic_error("Temporal artifact payload size drifted from the frozen layout.");
    const auto artifactIdentity = base::Sha256(payload.Bytes());
    base::BinaryWriter complete;
    complete.WriteBytes(payload.Bytes());
    WriteDigest(complete, artifactIdentity);
    auto bytes = std::move(complete).Take();

    return FrozenGlobalMotorHistoryArtifactV1(
        TemporalExtensionStrategyV1::VersionedSidecarTemporalExtension,
        TemporalCandidateKindV1::GlobalMotorHistoryReuse,
        TemporalHistoryRoleV1::GlobalMotorHistory,
        RetainedHistoryCompletionV1::ConsumerCompletionRetainsHistory,
        semanticValue.timelineLength,
        semanticValue.workCount,
        semanticValue.threadsPerGroup,
        semanticValue.historyDepth,
        HistoryBytes,
        HistoryBytes,
        DispatchArgumentBytes,
        1, 1, 1, 0,
        semanticValue.workCount / semanticValue.threadsPerGroup,
        semanticIdentity,
        scheduleIdentity,
        indirectIdentity,
        argumentProgram,
        hierarchyProgram,
        consumerProgram,
        artifactIdentity,
        std::move(bytes));
}

base::Result<FrozenGlobalMotorHistoryArtifactV1, TemporalContractErrorV1>
ParseGlobalMotorHistoryArtifactV1(std::span<const std::byte> bytes)
{
    if (bytes.size() != CompleteBytes)
        return base::Result<FrozenGlobalMotorHistoryArtifactV1, TemporalContractErrorV1>::Failure(
            Error("parse/size", "Temporal history artifact has a non-canonical byte size."));

    base::BinaryReader reader(bytes);
    auto magic = reader.ReadU32();
    auto version = reader.ReadU16();
    auto kind = reader.ReadU16();
    std::array<base::Result<std::uint32_t, std::string>, 19> fields{
        reader.ReadU32(), reader.ReadU32(), reader.ReadU32(), reader.ReadU32(), reader.ReadU32(),
        reader.ReadU32(), reader.ReadU32(), reader.ReadU32(), reader.ReadU32(), reader.ReadU32(),
        reader.ReadU32(), reader.ReadU32(), reader.ReadU32(), reader.ReadU32(), reader.ReadU32(),
        reader.ReadU32(), reader.ReadU32(), reader.ReadU32(), reader.ReadU32()};
    if (!magic || !version || !kind)
        return base::Result<FrozenGlobalMotorHistoryArtifactV1, TemporalContractErrorV1>::Failure(
            Error("parse/header", "Temporal history artifact header is truncated."));
    for (const auto& field : fields)
        if (!field)
            return base::Result<FrozenGlobalMotorHistoryArtifactV1, TemporalContractErrorV1>::Failure(
                Error("parse/fields", "Temporal history artifact field table is truncated."));

    auto semanticIdentity = ReadDigest(reader);
    auto scheduleIdentity = ReadDigest(reader);
    auto indirectIdentity = ReadDigest(reader);
    auto argumentProgram = ReadDigest(reader);
    auto hierarchyProgram = ReadDigest(reader);
    auto consumerProgram = ReadDigest(reader);
    auto storedIdentity = ReadDigest(reader);
    if (!semanticIdentity || !scheduleIdentity || !indirectIdentity || !argumentProgram ||
        !hierarchyProgram || !consumerProgram || !storedIdentity || reader.Remaining() != 0)
    {
        return base::Result<FrozenGlobalMotorHistoryArtifactV1, TemporalContractErrorV1>::Failure(
            Error("parse/tail", "Temporal history artifact digest tail is invalid."));
    }

    const auto calculated = base::Sha256(bytes.first(PayloadBytes));
    if (calculated != storedIdentity.Value())
        return base::Result<FrozenGlobalMotorHistoryArtifactV1, TemporalContractErrorV1>::Failure(
            Error("parse/identity", "Temporal history artifact digest mismatch."));

    const bool canonical =
        magic.Value() == ArtifactMagic && version.Value() == ArtifactVersion && kind.Value() == ArtifactKind &&
        fields[0].Value() == static_cast<std::uint32_t>(TemporalExtensionStrategyV1::VersionedSidecarTemporalExtension) &&
        fields[1].Value() == static_cast<std::uint32_t>(TemporalCandidateKindV1::GlobalMotorHistoryReuse) &&
        fields[2].Value() == static_cast<std::uint32_t>(TemporalHistoryRoleV1::GlobalMotorHistory) &&
        fields[3].Value() == static_cast<std::uint32_t>(RetainedHistoryCompletionV1::ConsumerCompletionRetainsHistory) &&
        fields[4].Value() == 1 && fields[5].Value() == 1 &&
        fields[6].Value() == semantic::TimelineLengthV1 && fields[7].Value() == semantic::BoneCountV1 &&
        fields[8].Value() == semantic::WorkCountV1 && fields[9].Value() == semantic::ThreadsPerGroupV1 &&
        fields[10].Value() == semantic::HistoryDepthV1 && fields[11].Value() == HistoryBytes &&
        fields[12].Value() == HistoryBytes && fields[13].Value() == DispatchArgumentBytes &&
        fields[14].Value() == 1 && fields[15].Value() == 1 && fields[16].Value() == 1 &&
        fields[17].Value() == 0 && fields[18].Value() == semantic::WorkCountV1 / semantic::ThreadsPerGroupV1 &&
        indirectIdentity.Value() == Spiral4IndirectIdentity() &&
        argumentProgram.Value() == ProgramIdentity("SGE4-5.Spiral5.UpdateHoldArgumentProducer.V1") &&
        hierarchyProgram.Value() == ProgramIdentity("SGE4-5.Spiral5.GlobalMotorHierarchyWriter.V1") &&
        consumerProgram.Value() == ProgramIdentity("SGE4-5.Spiral5.DirectPgaHistoryConsumer.V1");
    if (!canonical)
        return base::Result<FrozenGlobalMotorHistoryArtifactV1, TemporalContractErrorV1>::Failure(
            Error("parse/canonical", "Temporal history artifact contains a non-canonical field."));

    std::vector<std::byte> owned(bytes.begin(), bytes.end());
    return base::Result<FrozenGlobalMotorHistoryArtifactV1, TemporalContractErrorV1>::Success(
        FrozenGlobalMotorHistoryArtifactV1(
            TemporalExtensionStrategyV1::VersionedSidecarTemporalExtension,
            TemporalCandidateKindV1::GlobalMotorHistoryReuse,
            TemporalHistoryRoleV1::GlobalMotorHistory,
            RetainedHistoryCompletionV1::ConsumerCompletionRetainsHistory,
            fields[6].Value(), fields[8].Value(), fields[9].Value(), fields[10].Value(),
            fields[11].Value(), fields[12].Value(), fields[13].Value(), fields[14].Value(),
            fields[15].Value(), fields[16].Value(), fields[17].Value(), fields[18].Value(),
            semanticIdentity.Value(), scheduleIdentity.Value(), indirectIdentity.Value(),
            argumentProgram.Value(), hierarchyProgram.Value(), consumerProgram.Value(),
            storedIdentity.Value(), std::move(owned)));
}

base::Result<void, TemporalContractErrorV1>
ValidateGlobalMotorHistoryArtifactForExecutionV1(
    const FrozenGlobalMotorHistoryArtifactV1& artifact,
    const semantic::TemporalStateSemanticV1& semanticValue,
    const semantic::UpdateScheduleV1& schedule)
{
    auto parsed = ParseGlobalMotorHistoryArtifactV1(artifact.Bytes());
    if (!parsed)
        return base::Result<void, TemporalContractErrorV1>::Failure(parsed.Error());
    auto scheduleValid = semantic::ValidateUpdateScheduleV1(schedule);
    if (!scheduleValid)
        return base::Result<void, TemporalContractErrorV1>::Failure(
            Error(scheduleValid.Error().stage, scheduleValid.Error().message));

    if (artifact.ArtifactIdentity() != parsed.Value().ArtifactIdentity() ||
        artifact.SemanticIdentity() != semantic::TemporalStateSemanticIdentityV1(semanticValue) ||
        artifact.ScheduleIdentity() != semantic::UpdateScheduleIdentityV1(schedule))
    {
        return base::Result<void, TemporalContractErrorV1>::Failure(
            Error("execution/binding", "Temporal artifact is bound to another Semantic or schedule."));
    }
    if (artifact.TimelineLength() != semanticValue.timelineLength ||
        artifact.WorkCount() != semanticValue.workCount ||
        artifact.ThreadsPerGroup() != semanticValue.threadsPerGroup ||
        artifact.HistoryDepth() != 1 || artifact.HistoryBytes() != HistoryBytes ||
        artifact.ArgumentStrideBytes() != DispatchArgumentBytes || artifact.MaxCommandCount() != 1 ||
        artifact.ArgumentProducerDispatchX() != 1 || artifact.HierarchyUpdateDispatchX() != 1 ||
        artifact.HierarchyHoldDispatchX() != 0 ||
        artifact.ConsumerDispatchX() != semanticValue.workCount / semanticValue.threadsPerGroup)
    {
        return base::Result<void, TemporalContractErrorV1>::Failure(
            Error("execution/contract", "Temporal artifact execution fields are not canonical."));
    }
    return base::Result<void, TemporalContractErrorV1>::Success();
}

std::vector<TemporalInvocationArtifactV1>
BuildTemporalInvocationArtifactsV1(
    const FrozenGlobalMotorHistoryArtifactV1& artifact,
    const semantic::UpdateScheduleV1& schedule)
{
    std::vector<TemporalInvocationArtifactV1> result;
    result.reserve(schedule.invocations.size());
    for (const auto& invocation : schedule.invocations)
    {
        const bool update = invocation.event == semantic::UpdateEventV1::Update;
        result.push_back({
            invocation.invocationIndex,
            invocation.event,
            invocation.sourceGeneration,
            update ? artifact.HierarchyUpdateDispatchX() : artifact.HierarchyHoldDispatchX(),
            artifact.ConsumerDispatchX(),
            invocation.sourceGeneration,
            update ? invocation.sourceGeneration : semantic::InvalidGenerationV1,
            static_cast<std::uint64_t>(invocation.invocationIndex) + 1u});
    }
    return result;
}

std::vector<std::byte> SerializeTemporalInvocationArtifactsV1(
    std::span<const TemporalInvocationArtifactV1> invocations)
{
    base::BinaryWriter writer;
    writer.WriteU32(0x31495653u); // "S5VI"
    writer.WriteU32(static_cast<std::uint32_t>(invocations.size()));
    for (const auto& value : invocations)
    {
        writer.WriteU32(value.invocationIndex);
        writer.WriteU32(static_cast<std::uint32_t>(value.event));
        writer.WriteU32(value.sourceGeneration);
        writer.WriteU32(value.hierarchyDispatchX);
        writer.WriteU32(value.consumerDispatchX);
        writer.WriteU32(value.historyReadGeneration);
        writer.WriteU32(value.historyWriteGeneration);
        writer.WriteU64(value.retainedCompletionOrdinal);
    }
    return std::move(writer).Take();
}
}
