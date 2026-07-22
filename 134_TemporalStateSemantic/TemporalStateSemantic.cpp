#include "TemporalStateSemantic.h"

#include "../00_Foundation/BinaryIO.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace sge4_5::spiral5::semantic
{
namespace
{
constexpr std::uint32_t SemanticMagic = 0x31543553u; // "S5T1"
constexpr std::uint32_t ScheduleMagic = 0x31553553u; // "S5U1"

SemanticErrorV1 Error(std::string stage, std::string message)
{
    return {std::move(stage), std::move(message)};
}

void WriteString(base::BinaryWriter& writer, std::string_view value)
{
    writer.WriteU32(static_cast<std::uint32_t>(value.size()));
    writer.WriteBytes(std::as_bytes(std::span(value.data(), value.size())));
}

Float4V1 QuaternionMultiply(const Float4V1& a, const Float4V1& b) noexcept
{
    return {
        a.w * b.x + b.w * a.x + a.y * b.z - a.z * b.y,
        a.w * b.y + b.w * a.y + a.z * b.x - a.x * b.z,
        a.w * b.z + b.w * a.z + a.x * b.y - a.y * b.x,
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}

Float4V1 QuaternionAdd(const Float4V1& a, const Float4V1& b) noexcept
{
    return {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w};
}

Float4V1 QuaternionConjugate(const Float4V1& q) noexcept
{
    return {-q.x, -q.y, -q.z, q.w};
}

PgaMotorRecordV1 Compose(const PgaMotorRecordV1& parent, const PgaMotorRecordV1& local) noexcept
{
    return {
        QuaternionMultiply(parent.qr, local.qr),
        QuaternionAdd(
            QuaternionMultiply(parent.qr, local.qd),
            QuaternionMultiply(parent.qd, local.qr))};
}

PointRecordV1 TransformPoint(const PgaMotorRecordV1& motor, const PointRecordV1& point) noexcept
{
    const auto conjugate = QuaternionConjugate(motor.qr);
    const auto rotated = QuaternionMultiply(
        QuaternionMultiply(motor.qr, {point.x, point.y, point.z, 0.0f}),
        conjugate);
    const auto translationQ = QuaternionMultiply(motor.qd, conjugate);
    return {
        rotated.x + 2.0f * translationQ.x,
        rotated.y + 2.0f * translationQ.y,
        rotated.z + 2.0f * translationQ.z,
        1.0f};
}

std::vector<std::uint32_t> NamedUpdateFrames(std::string_view id)
{
    if (id == "BurstThenHold")
        return {0u, 1u, 2u, 3u, 64u, 65u, 127u};
    if (id == "Irregular")
        return {0u, 2u, 5u, 11u, 17u, 29u, 47u, 79u, 127u};
    return {};
}

UpdateScheduleV1 BuildFromFrames(
    std::string id,
    std::uint32_t periodicInterval,
    const std::vector<std::uint32_t>& updateFrames)
{
    UpdateScheduleV1 schedule;
    schedule.id = std::move(id);
    schedule.periodicInterval = periodicInterval;
    schedule.invocations.reserve(TimelineLengthV1);

    std::uint32_t generation = 0;
    for (std::uint32_t invocation = 0; invocation < TimelineLengthV1; ++invocation)
    {
        const bool update = std::binary_search(
            updateFrames.begin(), updateFrames.end(), invocation);
        if (invocation != 0 && update)
            ++generation;
        schedule.invocations.push_back({
            invocation,
            update ? UpdateEventV1::Update : UpdateEventV1::Hold,
            generation});
    }
    schedule.identity = UpdateScheduleIdentityV1(schedule);
    return schedule;
}
}

TemporalStateSemanticV1 BuildCanonicalTemporalStateSemanticV1()
{
    TemporalStateSemanticV1 semantic;
    const auto active = spiral4::semantic::BuildCanonicalActiveWorkSemanticV1();
    semantic.activeWorkSemanticIdentity =
        spiral4::semantic::ActiveWorkSemanticIdentityV1(active);
    return semantic;
}

std::vector<std::byte> SerializeTemporalStateSemanticV1(
    const TemporalStateSemanticV1& value)
{
    const auto active = spiral4::semantic::BuildCanonicalActiveWorkSemanticV1();
    const auto activeIdentity = spiral4::semantic::ActiveWorkSemanticIdentityV1(active);
    if (value.conventionVersion != 1 ||
        value.temporalRule != 1 ||
        value.timelineLength != TimelineLengthV1 ||
        value.boneCount != BoneCountV1 ||
        value.workCount != WorkCountV1 ||
        value.activeWorkCount != WorkCountV1 ||
        value.threadsPerGroup != ThreadsPerGroupV1 ||
        value.historyDepth != HistoryDepthV1 ||
        value.representationControl != 1 ||
        value.activeWorkSemanticIdentity != activeIdentity)
    {
        throw std::invalid_argument("TemporalStateSemanticV1 is not canonical.");
    }

    base::BinaryWriter writer;
    writer.WriteU32(SemanticMagic);
    writer.WriteU32(value.conventionVersion);
    writer.WriteU32(value.temporalRule);
    writer.WriteU32(value.timelineLength);
    writer.WriteU32(value.boneCount);
    writer.WriteU32(value.workCount);
    writer.WriteU32(value.activeWorkCount);
    writer.WriteU32(value.threadsPerGroup);
    writer.WriteU32(value.historyDepth);
    writer.WriteU32(value.representationControl);
    writer.WriteBytes(value.activeWorkSemanticIdentity);
    return std::move(writer).Take();
}

base::Digest256 TemporalStateSemanticIdentityV1(
    const TemporalStateSemanticV1& value)
{
    const auto bytes = SerializeTemporalStateSemanticV1(value);
    return base::Sha256(bytes);
}

base::Result<UpdateScheduleV1, SemanticErrorV1>
BuildPeriodicUpdateScheduleV1(std::uint32_t interval)
{
    const auto& allowed = MeasurementUpdateIntervalsV1();
    if (std::find(allowed.begin(), allowed.end(), interval) == allowed.end())
    {
        return base::Result<UpdateScheduleV1, SemanticErrorV1>::Failure(
            Error("schedule/interval", "Update interval is outside the frozen V1 corpus."));
    }

    std::vector<std::uint32_t> frames;
    for (std::uint32_t frame = 0; frame < TimelineLengthV1; frame += interval)
        frames.push_back(frame);

    auto schedule = BuildFromFrames("P" + std::to_string(interval), interval, frames);
    auto valid = ValidateUpdateScheduleV1(schedule);
    if (!valid)
        return base::Result<UpdateScheduleV1, SemanticErrorV1>::Failure(valid.Error());
    return base::Result<UpdateScheduleV1, SemanticErrorV1>::Success(std::move(schedule));
}

base::Result<UpdateScheduleV1, SemanticErrorV1>
BuildNamedUpdateScheduleV1(std::string_view id)
{
    auto frames = NamedUpdateFrames(id);
    if (frames.empty())
    {
        return base::Result<UpdateScheduleV1, SemanticErrorV1>::Failure(
            Error("schedule/name", "Unknown non-periodic schedule identifier."));
    }
    auto schedule = BuildFromFrames(std::string(id), 0, frames);
    auto valid = ValidateUpdateScheduleV1(schedule);
    if (!valid)
        return base::Result<UpdateScheduleV1, SemanticErrorV1>::Failure(valid.Error());
    return base::Result<UpdateScheduleV1, SemanticErrorV1>::Success(std::move(schedule));
}

base::Result<void, SemanticErrorV1>
ValidateUpdateScheduleV1(const UpdateScheduleV1& schedule)
{
    if (schedule.id.empty())
        return base::Result<void, SemanticErrorV1>::Failure(
            Error("schedule/id", "Schedule identifier is empty."));
    if (schedule.invocations.size() != TimelineLengthV1)
        return base::Result<void, SemanticErrorV1>::Failure(
            Error("schedule/length", "Schedule does not contain exactly 128 invocations."));

    if (schedule.invocations.front().invocationIndex != 0 ||
        schedule.invocations.front().event != UpdateEventV1::Update ||
        schedule.invocations.front().sourceGeneration != 0)
    {
        return base::Result<void, SemanticErrorV1>::Failure(
            Error("schedule/initial", "Invocation zero must update and seed generation zero."));
    }

    std::uint32_t expectedGeneration = 0;
    for (std::uint32_t index = 0; index < TimelineLengthV1; ++index)
    {
        const auto& value = schedule.invocations[index];
        if (value.invocationIndex != index)
            return base::Result<void, SemanticErrorV1>::Failure(
                Error("schedule/order", "Invocation indices are not contiguous."));
        if (index > 0 && value.event == UpdateEventV1::Update)
            ++expectedGeneration;
        if (value.sourceGeneration != expectedGeneration)
            return base::Result<void, SemanticErrorV1>::Failure(
                Error("schedule/generation", "Source generation does not follow the frozen increment rule."));
    }

    if (schedule.identity != base::Digest256{} &&
        schedule.identity != UpdateScheduleIdentityV1(schedule))
    {
        return base::Result<void, SemanticErrorV1>::Failure(
            Error("schedule/identity", "Schedule identity does not match canonical bytes."));
    }
    return base::Result<void, SemanticErrorV1>::Success();
}

std::vector<std::byte> SerializeUpdateScheduleV1(const UpdateScheduleV1& schedule)
{
    auto copy = schedule;
    copy.identity = {};
    auto valid = ValidateUpdateScheduleV1(copy);
    if (!valid)
        throw std::invalid_argument(valid.Error().message);

    base::BinaryWriter writer;
    writer.WriteU32(ScheduleMagic);
    WriteString(writer, schedule.id);
    writer.WriteU32(schedule.periodicInterval);
    writer.WriteU32(static_cast<std::uint32_t>(schedule.invocations.size()));
    for (const auto& invocation : schedule.invocations)
    {
        writer.WriteU32(invocation.invocationIndex);
        writer.WriteU8(static_cast<std::uint8_t>(invocation.event));
        writer.WriteU8(0);
        writer.WriteU16(0);
        writer.WriteU32(invocation.sourceGeneration);
    }
    return std::move(writer).Take();
}

base::Digest256 UpdateScheduleIdentityV1(const UpdateScheduleV1& schedule)
{
    const auto bytes = SerializeUpdateScheduleV1(schedule);
    return base::Sha256(bytes);
}

const std::array<std::uint32_t, 7>& MeasurementUpdateIntervalsV1() noexcept
{
    static constexpr std::array<std::uint32_t, 7> values{1u, 2u, 4u, 8u, 16u, 32u, 64u};
    return values;
}

std::array<PgaMotorRecordV1, BoneCountV1>
BuildCanonicalLocalMotorGenerationV1(std::uint32_t generation)
{
    std::array<PgaMotorRecordV1, BoneCountV1> motors{};
    for (std::uint32_t bone = 0; bone < BoneCountV1; ++bone)
    {
        const float scale = static_cast<float>((generation + 1u) * (bone + 1u));
        const float tx = scale * 0.0005f;
        const float ty = -static_cast<float>(((generation % 7u) + 1u) * (bone + 1u)) * 0.00075f;
        const float tz = static_cast<float>(((generation % 5u) + 1u) * (bone + 1u)) * 0.000375f;
        motors[bone].qr = {0.0f, 0.0f, 0.0f, 1.0f};
        motors[bone].qd = {tx * 0.5f, ty * 0.5f, tz * 0.5f, 0.0f};
    }
    return motors;
}

std::array<PgaMotorRecordV1, BoneCountV1>
ComposeCanonicalGlobalMotorsCpuV1(
    const std::array<PgaMotorRecordV1, BoneCountV1>& localMotors)
{
    std::array<PgaMotorRecordV1, BoneCountV1> global{};
    PgaMotorRecordV1 parent{{0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f}};
    for (std::uint32_t bone = 0; bone < BoneCountV1; ++bone)
    {
        global[bone] = Compose(parent, localMotors[bone]);
        parent = global[bone];
    }
    return global;
}

std::vector<PointRecordV1> BuildCanonicalPointCorpusV1()
{
    std::vector<PointRecordV1> points;
    points.reserve(WorkCountV1);
    for (std::uint32_t index = 0; index < WorkCountV1; ++index)
    {
        const auto local = index % ReusePerBoneV1;
        points.push_back({
            static_cast<float>(static_cast<int>(local % 17u) - 8) * 0.03125f,
            static_cast<float>(static_cast<int>((local / 17u) % 19u) - 9) * 0.021875f,
            static_cast<float>(static_cast<int>((local / (17u * 19u)) % 11u) - 5) * 0.046875f,
            1.0f});
    }
    return points;
}

std::vector<PointRecordV1> ApplyGlobalMotorsCpuV1(
    const std::array<PgaMotorRecordV1, BoneCountV1>& globalMotors,
    const std::vector<PointRecordV1>& points)
{
    if (points.size() != WorkCountV1)
        throw std::invalid_argument("Point corpus does not contain 4096 records.");
    std::vector<PointRecordV1> output;
    output.reserve(points.size());
    for (std::uint32_t index = 0; index < WorkCountV1; ++index)
    {
        const std::uint32_t bone = index / ReusePerBoneV1;
        output.push_back(TransformPoint(globalMotors[bone], points[index]));
    }
    return output;
}

std::vector<std::byte> SerializeMotorRecordsV1(
    const std::array<PgaMotorRecordV1, BoneCountV1>& motors)
{
    const auto span = std::as_bytes(std::span(motors));
    return {span.begin(), span.end()};
}

std::vector<std::byte> SerializePointRecordsV1(
    const std::vector<PointRecordV1>& points)
{
    const auto span = std::as_bytes(std::span(points));
    return {span.begin(), span.end()};
}
}
