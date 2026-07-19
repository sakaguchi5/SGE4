#pragma once

#include "../00_Foundation/Base.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace sge4_5::spiral1::semantic
{
inline constexpr std::uint32_t MotorConventionVersionV1 = 1;
inline constexpr std::uint32_t Float4PointElementKindV1 = 1;
inline constexpr std::uint32_t Float4PointStrideBytesV1 = 16;
inline constexpr std::uint32_t MaximumQualificationPointCountV1 = 4096;

struct Double3 final
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct Float4Point final
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;
};
static_assert(sizeof(Float4Point) == Float4PointStrideBytesV1);

class UnitQuaternion final
{
public:
    [[nodiscard]] static base::Result<UnitQuaternion, std::string> FromComponents(
        double w, double x, double y, double z);
    [[nodiscard]] static base::Result<UnitQuaternion, std::string> FromAxisAngle(
        Double3 axis, double angleRadians);

    [[nodiscard]] const std::array<double, 4>& Components() const noexcept { return components_; }

private:
    explicit UnitQuaternion(std::array<double, 4> components) : components_(components) {}
    std::array<double, 4> components_{};
};

class CanonicalPgaMotorV1 final
{
public:
    [[nodiscard]] static CanonicalPgaMotorV1 Identity();
    [[nodiscard]] static base::Result<CanonicalPgaMotorV1, std::string> Build(
        const UnitQuaternion& rotation,
        Double3 translation);
    [[nodiscard]] static base::Result<CanonicalPgaMotorV1, std::string> Deserialize(
        std::span<const std::byte> bytes);

    [[nodiscard]] const std::array<double, 4>& Qr() const noexcept { return qr_; }
    [[nodiscard]] const std::array<double, 4>& Qd() const noexcept { return qd_; }
    [[nodiscard]] std::vector<std::byte> Serialize() const;
    [[nodiscard]] base::Digest256 IdentityDigest() const;

private:
    CanonicalPgaMotorV1(std::array<double, 4> qr, std::array<double, 4> qd) : qr_(qr), qd_(qd) {}
    std::array<double, 4> qr_{};
    std::array<double, 4> qd_{};
};

[[nodiscard]] CanonicalPgaMotorV1 BuildIdentityMotor();
[[nodiscard]] base::Result<CanonicalPgaMotorV1, std::string> BuildMotor(
    const UnitQuaternion& rotation,
    Double3 translation);
[[nodiscard]] base::Result<CanonicalPgaMotorV1, std::string> ComposeMotors(
    const CanonicalPgaMotorV1& first,
    const CanonicalPgaMotorV1& second);

[[nodiscard]] base::Result<void, std::string> ValidatePoint(const Float4Point& point);
[[nodiscard]] std::vector<std::byte> SerializePoints(std::span<const Float4Point> points);
[[nodiscard]] base::Digest256 Float4PointSchemaIdentityV1();
[[nodiscard]] base::Digest256 Float4PointOutputSchemaIdentityV1();

struct ApplyPgaMotorSemanticV1 final
{
    CanonicalPgaMotorV1 motor;
    std::uint32_t pointCount = 0;
    base::Digest256 corpusIdentity{};
    base::Digest256 observationContractIdentity{};

    ApplyPgaMotorSemanticV1(
        CanonicalPgaMotorV1 motorValue,
        std::uint32_t count,
        base::Digest256 corpus,
        base::Digest256 observation)
        : motor(std::move(motorValue)),
          pointCount(count),
          corpusIdentity(corpus),
          observationContractIdentity(observation) {}
};

[[nodiscard]] base::Result<ApplyPgaMotorSemanticV1, std::string> BuildApplyPgaMotorSemanticV1(
    const CanonicalPgaMotorV1& motor,
    std::uint32_t pointCount,
    const base::Digest256& corpusIdentity,
    const base::Digest256& observationContractIdentity);
[[nodiscard]] std::vector<std::byte> SerializeApplyPgaMotorSemanticV1(
    const ApplyPgaMotorSemanticV1& value);
[[nodiscard]] base::Digest256 ApplyPgaMotorSemanticIdentityV1(
    const ApplyPgaMotorSemanticV1& value);
}
