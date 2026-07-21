#pragma once
#include "../00_Foundation/Base.h"

#include <array>
#include <compare>
#include <cstdint>
#include <string>

namespace sge4_5::spiral3::contracts
{
inline constexpr std::array<std::uint32_t, 6> CanonicalReuseCountsV1{1, 4, 16, 64, 256, 512};
inline constexpr std::uint32_t LocalPointStrideBytesV1 = 16;
inline constexpr std::uint32_t ObservationRecordStrideBytesV1 = 304;
inline constexpr double ObservationAbsoluteToleranceV1 = 2.5e-4;
inline constexpr double ObservationRelativeToleranceV1 = 2.0e-5;
inline constexpr double ObservationPairwiseAbsoluteToleranceV1 = 2.5e-4;
inline constexpr double ObservationPairwiseRelativeToleranceV1 = 2.0e-5;
inline constexpr double ObservationRigidityToleranceV1 = 5.0e-4;
inline constexpr std::uint32_t MaximumConsumerPointCountV1 = 4096;

class BoneCountV1 final
{
public:
    explicit constexpr BoneCountV1(std::uint32_t value) noexcept : value_(value) {}
    [[nodiscard]] constexpr std::uint32_t Value() const noexcept { return value_; }
    auto operator<=>(const BoneCountV1&) const = default;
private:
    std::uint32_t value_ = 0;
};

class ReuseCountV1 final
{
public:
    explicit constexpr ReuseCountV1(std::uint32_t value) noexcept : value_(value) {}
    [[nodiscard]] constexpr std::uint32_t Value() const noexcept { return value_; }
    auto operator<=>(const ReuseCountV1&) const = default;
private:
    std::uint32_t value_ = 0;
};

class PointCountV1 final
{
public:
    explicit constexpr PointCountV1(std::uint32_t value) noexcept : value_(value) {}
    [[nodiscard]] constexpr std::uint32_t Value() const noexcept { return value_; }
    auto operator<=>(const PointCountV1&) const = default;
private:
    std::uint32_t value_ = 0;
};

struct FixedReuseWorkloadShapeV1 final
{
    BoneCountV1 boneCount{0};
    ReuseCountV1 reuseCount{0};
    PointCountV1 pointCount{0};
};

[[nodiscard]] base::Result<FixedReuseWorkloadShapeV1, std::string>
BuildFixedReuseWorkloadShapeV1(BoneCountV1 boneCount, ReuseCountV1 reuseCount);

[[nodiscard]] bool IsCanonicalReuseCountV1(std::uint32_t value) noexcept;
[[nodiscard]] base::Digest256 CanonicalReuseSetIdentityV1();
}
