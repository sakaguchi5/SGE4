#pragma once

#include "../00_Foundation/Result.h"
#include "../00_Foundation/Sha256.h"
#include "../120_ActiveWorkSemantic/ActiveWorkSemantic.h"

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace sge4_5::spiral5::semantic
{
inline constexpr std::uint32_t TimelineLengthV1 = 128;
inline constexpr std::uint32_t BoneCountV1 = 8;
inline constexpr std::uint32_t WorkCountV1 = 4096;
inline constexpr std::uint32_t ReusePerBoneV1 = 512;
inline constexpr std::uint32_t ThreadsPerGroupV1 = 64;
inline constexpr std::uint32_t HistoryDepthV1 = 1;
inline constexpr std::uint32_t InvalidGenerationV1 = 0xFFFF'FFFFu;

struct Float4V1 final
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;
    auto operator<=>(const Float4V1&) const = default;
};
static_assert(sizeof(Float4V1) == 16);

struct PgaMotorRecordV1 final
{
    Float4V1 qr{};
    Float4V1 qd{};
    auto operator<=>(const PgaMotorRecordV1&) const = default;
};
static_assert(sizeof(PgaMotorRecordV1) == 32);

struct PointRecordV1 final
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;
    auto operator<=>(const PointRecordV1&) const = default;
};
static_assert(sizeof(PointRecordV1) == 16);

enum class UpdateEventV1 : std::uint8_t
{
    Hold = 0,
    Update = 1
};

struct TemporalInvocationV1 final
{
    std::uint32_t invocationIndex = 0;
    UpdateEventV1 event = UpdateEventV1::Hold;
    std::uint32_t sourceGeneration = 0;
};

struct UpdateScheduleV1 final
{
    std::string id;
    std::uint32_t periodicInterval = 0;
    std::vector<TemporalInvocationV1> invocations;
    base::Digest256 identity{};
};

struct TemporalStateSemanticV1 final
{
    std::uint32_t conventionVersion = 1;
    std::uint32_t temporalRule = 1; // LastUpdateWinsPiecewiseConstant
    std::uint32_t timelineLength = TimelineLengthV1;
    std::uint32_t boneCount = BoneCountV1;
    std::uint32_t workCount = WorkCountV1;
    std::uint32_t activeWorkCount = WorkCountV1;
    std::uint32_t threadsPerGroup = ThreadsPerGroupV1;
    std::uint32_t historyDepth = HistoryDepthV1;
    std::uint32_t representationControl = 1; // DirectPgaThroughConsumerV1
    base::Digest256 activeWorkSemanticIdentity{};
};

struct SemanticErrorV1 final
{
    std::string stage;
    std::string message;
};

[[nodiscard]] TemporalStateSemanticV1 BuildCanonicalTemporalStateSemanticV1();
[[nodiscard]] std::vector<std::byte> SerializeTemporalStateSemanticV1(
    const TemporalStateSemanticV1& value);
[[nodiscard]] base::Digest256 TemporalStateSemanticIdentityV1(
    const TemporalStateSemanticV1& value);

[[nodiscard]] base::Result<UpdateScheduleV1, SemanticErrorV1>
BuildPeriodicUpdateScheduleV1(std::uint32_t interval);
[[nodiscard]] base::Result<UpdateScheduleV1, SemanticErrorV1>
BuildNamedUpdateScheduleV1(std::string_view id);
[[nodiscard]] base::Result<void, SemanticErrorV1>
ValidateUpdateScheduleV1(const UpdateScheduleV1& schedule);
[[nodiscard]] std::vector<std::byte> SerializeUpdateScheduleV1(
    const UpdateScheduleV1& schedule);
[[nodiscard]] base::Digest256 UpdateScheduleIdentityV1(
    const UpdateScheduleV1& schedule);
[[nodiscard]] const std::array<std::uint32_t, 7>& MeasurementUpdateIntervalsV1() noexcept;

[[nodiscard]] std::array<PgaMotorRecordV1, BoneCountV1>
BuildCanonicalLocalMotorGenerationV1(std::uint32_t generation);
[[nodiscard]] std::array<PgaMotorRecordV1, BoneCountV1>
ComposeCanonicalGlobalMotorsCpuV1(
    const std::array<PgaMotorRecordV1, BoneCountV1>& localMotors);
[[nodiscard]] std::vector<PointRecordV1> BuildCanonicalPointCorpusV1();
[[nodiscard]] std::vector<PointRecordV1> ApplyGlobalMotorsCpuV1(
    const std::array<PgaMotorRecordV1, BoneCountV1>& globalMotors,
    const std::vector<PointRecordV1>& points);

[[nodiscard]] std::vector<std::byte> SerializeMotorRecordsV1(
    const std::array<PgaMotorRecordV1, BoneCountV1>& motors);
[[nodiscard]] std::vector<std::byte> SerializePointRecordsV1(
    const std::vector<PointRecordV1>& points);
}
