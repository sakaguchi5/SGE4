#include "PgaRigidTransformSemantic.h"

#include <bit>
#include <cmath>
#include <limits>

namespace sge4_5::spiral1::semantic
{
namespace
{
constexpr double MinimumNormSquared = 0x1p-40;
constexpr double NormalizedTolerance = 0x1p-48;
constexpr double SignEpsilon = 0x1p-52;
constexpr double DualOrthogonalityTolerance = 0x1p-44;
constexpr std::size_t SerializedMotorBytes = 72;

[[nodiscard]] double CanonicalZero(double value) noexcept
{
    return value == 0.0 ? 0.0 : value;
}

[[nodiscard]] float CanonicalZero(float value) noexcept
{
    return value == 0.0f ? 0.0f : value;
}

[[nodiscard]] bool IsNegativeZero(double value) noexcept
{
    return value == 0.0 && std::signbit(value);
}

[[nodiscard]] std::array<double, 4> Multiply(
    const std::array<double, 4>& a,
    const std::array<double, 4>& b) noexcept
{
    return {
        a[0] * b[0] - a[1] * b[1] - a[2] * b[2] - a[3] * b[3],
        a[0] * b[1] + a[1] * b[0] + a[2] * b[3] - a[3] * b[2],
        a[0] * b[2] - a[1] * b[3] + a[2] * b[0] + a[3] * b[1],
        a[0] * b[3] + a[1] * b[2] - a[2] * b[1] + a[3] * b[0]
    };
}

[[nodiscard]] std::array<double, 4> Conjugate(const std::array<double, 4>& q) noexcept
{
    return {q[0], -q[1], -q[2], -q[3]};
}

[[nodiscard]] double NormSquared(const std::array<double, 4>& q) noexcept
{
    return q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3];
}

[[nodiscard]] double Dot(
    const std::array<double, 4>& a,
    const std::array<double, 4>& b) noexcept
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
}

void CanonicalizeSign(std::array<double, 4>& qr, std::array<double, 4>* qd = nullptr) noexcept
{
    bool flip = false;
    for (const double component : qr)
    {
        if (std::abs(component) > SignEpsilon)
        {
            flip = component < 0.0;
            break;
        }
    }
    if (flip)
    {
        for (double& component : qr) component = -component;
        if (qd != nullptr) for (double& component : *qd) component = -component;
    }
    for (double& component : qr) component = CanonicalZero(component);
    if (qd != nullptr) for (double& component : *qd) component = CanonicalZero(component);
}

[[nodiscard]] bool HasCanonicalSign(const std::array<double, 4>& qr) noexcept
{
    for (const double component : qr)
    {
        if (std::abs(component) > SignEpsilon) return component > 0.0;
    }
    return false;
}

[[nodiscard]] base::Result<void, std::string> ValidateMotorArrays(
    const std::array<double, 4>& qr,
    const std::array<double, 4>& qd)
{
    for (const double component : qr)
    {
        if (!std::isfinite(component)) return base::Result<void, std::string>::Failure("non-finite qr coefficient");
        if (IsNegativeZero(component)) return base::Result<void, std::string>::Failure("non-canonical negative zero in qr");
    }
    for (const double component : qd)
    {
        if (!std::isfinite(component)) return base::Result<void, std::string>::Failure("non-finite qd coefficient");
        if (IsNegativeZero(component)) return base::Result<void, std::string>::Failure("non-canonical negative zero in qd");
    }
    const double normSquared = NormSquared(qr);
    if (!std::isfinite(normSquared) || std::abs(normSquared - 1.0) > NormalizedTolerance)
        return base::Result<void, std::string>::Failure("qr is not canonical unit length");
    if (!HasCanonicalSign(qr))
        return base::Result<void, std::string>::Failure("qr sign is not canonical");
    if (std::abs(Dot(qr, qd)) > DualOrthogonalityTolerance)
        return base::Result<void, std::string>::Failure("qd is not orthogonal to qr");
    return base::Result<void, std::string>::Success();
}

void WriteF64(base::BinaryWriter& writer, double value)
{
    writer.WriteU64(std::bit_cast<std::uint64_t>(CanonicalZero(value)));
}

[[nodiscard]] base::Digest256 LiteralIdentity(std::string_view text)
{
    const auto bytes = std::as_bytes(std::span(text.data(), text.size()));
    return base::Sha256(bytes);
}
}

base::Result<UnitQuaternion, std::string> UnitQuaternion::FromComponents(
    double w, double x, double y, double z)
{
    std::array<double, 4> q{w, x, y, z};
    for (const double component : q)
        if (!std::isfinite(component)) return base::Result<UnitQuaternion, std::string>::Failure("quaternion component is not finite");

    const double normSquared = NormSquared(q);
    if (!std::isfinite(normSquared) || normSquared < MinimumNormSquared)
        return base::Result<UnitQuaternion, std::string>::Failure("quaternion norm is too small");

    const double inverseNorm = 1.0 / std::sqrt(normSquared);
    for (double& component : q) component *= inverseNorm;
    CanonicalizeSign(q);

    const double normalizedNormSquared = NormSquared(q);
    if (!std::isfinite(normalizedNormSquared) || std::abs(normalizedNormSquared - 1.0) > NormalizedTolerance)
        return base::Result<UnitQuaternion, std::string>::Failure("quaternion normalization did not satisfy the canonical tolerance");

    return base::Result<UnitQuaternion, std::string>::Success(UnitQuaternion(q));
}

base::Result<UnitQuaternion, std::string> UnitQuaternion::FromAxisAngle(
    Double3 axis, double angleRadians)
{
    if (!std::isfinite(axis.x) || !std::isfinite(axis.y) || !std::isfinite(axis.z) || !std::isfinite(angleRadians))
        return base::Result<UnitQuaternion, std::string>::Failure("axis-angle input is not finite");
    const double axisNormSquared = axis.x * axis.x + axis.y * axis.y + axis.z * axis.z;
    if (!std::isfinite(axisNormSquared) || axisNormSquared < MinimumNormSquared)
        return base::Result<UnitQuaternion, std::string>::Failure("axis norm is too small");
    const double inverseAxisNorm = 1.0 / std::sqrt(axisNormSquared);
    const double halfAngle = angleRadians * 0.5;
    const double scale = std::sin(halfAngle) * inverseAxisNorm;
    return FromComponents(std::cos(halfAngle), axis.x * scale, axis.y * scale, axis.z * scale);
}

CanonicalPgaMotorV1 CanonicalPgaMotorV1::Identity()
{
    return CanonicalPgaMotorV1({1.0, 0.0, 0.0, 0.0}, {0.0, 0.0, 0.0, 0.0});
}

base::Result<CanonicalPgaMotorV1, std::string> CanonicalPgaMotorV1::Build(
    const UnitQuaternion& rotation,
    Double3 translation)
{
    if (!std::isfinite(translation.x) || !std::isfinite(translation.y) || !std::isfinite(translation.z))
        return base::Result<CanonicalPgaMotorV1, std::string>::Failure("translation is not finite");

    auto qr = rotation.Components();
    const std::array<double, 4> translationQuaternion{0.0, translation.x, translation.y, translation.z};
    auto qd = Multiply(translationQuaternion, qr);
    for (double& component : qd) component *= 0.5;
    CanonicalizeSign(qr, &qd);
    auto validation = ValidateMotorArrays(qr, qd);
    if (!validation) return base::Result<CanonicalPgaMotorV1, std::string>::Failure(validation.Error());
    return base::Result<CanonicalPgaMotorV1, std::string>::Success(CanonicalPgaMotorV1(qr, qd));
}

std::vector<std::byte> CanonicalPgaMotorV1::Serialize() const
{
    base::BinaryWriter writer;
    writer.WriteU32(MotorConventionVersionV1);
    writer.WriteU32(0);
    for (const double component : qr_) WriteF64(writer, component);
    for (const double component : qd_) WriteF64(writer, component);
    return std::move(writer).Take();
}

base::Digest256 CanonicalPgaMotorV1::IdentityDigest() const
{
    const auto bytes = Serialize();
    return base::Sha256(bytes);
}

base::Result<CanonicalPgaMotorV1, std::string> CanonicalPgaMotorV1::Deserialize(
    std::span<const std::byte> bytes)
{
    if (bytes.size() != SerializedMotorBytes)
        return base::Result<CanonicalPgaMotorV1, std::string>::Failure("unexpected motor byte count");
    base::BinaryReader reader(bytes);
    auto version = reader.ReadU32();
    auto reserved = reader.ReadU32();
    if (!version || !reserved) return base::Result<CanonicalPgaMotorV1, std::string>::Failure("truncated motor header");
    if (version.Value() != MotorConventionVersionV1) return base::Result<CanonicalPgaMotorV1, std::string>::Failure("unknown motor convention version");
    if (reserved.Value() != 0) return base::Result<CanonicalPgaMotorV1, std::string>::Failure("motor reserved field is non-zero");
    std::array<double, 4> qr{};
    std::array<double, 4> qd{};
    for (double& component : qr)
    {
        auto bits = reader.ReadU64();
        if (!bits) return base::Result<CanonicalPgaMotorV1, std::string>::Failure("truncated qr coefficient");
        component = std::bit_cast<double>(bits.Value());
    }
    for (double& component : qd)
    {
        auto bits = reader.ReadU64();
        if (!bits) return base::Result<CanonicalPgaMotorV1, std::string>::Failure("truncated qd coefficient");
        component = std::bit_cast<double>(bits.Value());
    }
    auto validation = ValidateMotorArrays(qr, qd);
    if (!validation) return base::Result<CanonicalPgaMotorV1, std::string>::Failure(validation.Error());
    return base::Result<CanonicalPgaMotorV1, std::string>::Success(CanonicalPgaMotorV1(qr, qd));
}

CanonicalPgaMotorV1 BuildIdentityMotor() { return CanonicalPgaMotorV1::Identity(); }

base::Result<CanonicalPgaMotorV1, std::string> BuildMotor(
    const UnitQuaternion& rotation,
    Double3 translation)
{
    return CanonicalPgaMotorV1::Build(rotation, translation);
}

base::Result<CanonicalPgaMotorV1, std::string> ComposeMotors(
    const CanonicalPgaMotorV1& first,
    const CanonicalPgaMotorV1& second)
{
    auto qr = Multiply(second.Qr(), first.Qr());
    const auto left = Multiply(second.Qr(), first.Qd());
    const auto right = Multiply(second.Qd(), first.Qr());
    std::array<double, 4> qd{};
    for (std::size_t i = 0; i < qd.size(); ++i) qd[i] = left[i] + right[i];

    auto rotation = UnitQuaternion::FromComponents(qr[0], qr[1], qr[2], qr[3]);
    if (!rotation) return base::Result<CanonicalPgaMotorV1, std::string>::Failure(rotation.Error());
    const auto normalizedQr = rotation.Value().Components();
    const auto translated = Multiply(qd, Conjugate(qr));
    const double normSquared = NormSquared(qr);
    if (!std::isfinite(normSquared) || normSquared < MinimumNormSquared)
        return base::Result<CanonicalPgaMotorV1, std::string>::Failure("composed motor has invalid norm");
    const Double3 translation{
        2.0 * translated[1] / normSquared,
        2.0 * translated[2] / normSquared,
        2.0 * translated[3] / normSquared
    };
    (void)normalizedQr;
    return BuildMotor(rotation.Value(), translation);
}

base::Result<void, std::string> ValidatePoint(const Float4Point& point)
{
    if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z) || !std::isfinite(point.w))
        return base::Result<void, std::string>::Failure("point contains a non-finite component");
    if (std::bit_cast<std::uint32_t>(point.w) != 0x3f800000u)
        return base::Result<void, std::string>::Failure("point w is not bit-exact 1.0f");
    if (std::abs(point.x) > 10000.0f || std::abs(point.y) > 10000.0f || std::abs(point.z) > 10000.0f)
        return base::Result<void, std::string>::Failure("point is outside the qualification range");
    return base::Result<void, std::string>::Success();
}

std::vector<std::byte> SerializePoints(std::span<const Float4Point> points)
{
    base::BinaryWriter writer;
    for (const auto& point : points)
    {
        writer.WriteU32(std::bit_cast<std::uint32_t>(CanonicalZero(point.x)));
        writer.WriteU32(std::bit_cast<std::uint32_t>(CanonicalZero(point.y)));
        writer.WriteU32(std::bit_cast<std::uint32_t>(CanonicalZero(point.z)));
        writer.WriteU32(std::bit_cast<std::uint32_t>(point.w));
    }
    return std::move(writer).Take();
}

base::Digest256 Float4PointSchemaIdentityV1()
{
    return LiteralIdentity("SGE4-5.Spiral1.Float4Point.InputSchema.V1|binary32-xyzw|stride=16|w=1");
}

base::Digest256 Float4PointOutputSchemaIdentityV1()
{
    return LiteralIdentity("SGE4-5.Spiral1.Float4Point.OutputSchema.V1|binary32-xyzw|stride=16|w=1");
}

base::Result<ApplyPgaMotorSemanticV1, std::string> BuildApplyPgaMotorSemanticV1(
    const CanonicalPgaMotorV1& motor,
    std::uint32_t pointCount,
    const base::Digest256& corpusIdentity,
    const base::Digest256& observationContractIdentity)
{
    if (pointCount == 0 || pointCount > MaximumQualificationPointCountV1)
        return base::Result<ApplyPgaMotorSemanticV1, std::string>::Failure("point count is outside the canonical qualification range");
    if (corpusIdentity == base::Digest256{})
        return base::Result<ApplyPgaMotorSemanticV1, std::string>::Failure("corpus identity is absent");
    if (observationContractIdentity == base::Digest256{})
        return base::Result<ApplyPgaMotorSemanticV1, std::string>::Failure("observation contract identity is absent");
    auto motorRoundTrip = CanonicalPgaMotorV1::Deserialize(motor.Serialize());
    if (!motorRoundTrip) return base::Result<ApplyPgaMotorSemanticV1, std::string>::Failure(motorRoundTrip.Error());
    return base::Result<ApplyPgaMotorSemanticV1, std::string>::Success(
        ApplyPgaMotorSemanticV1(motor, pointCount, corpusIdentity, observationContractIdentity));
}

std::vector<std::byte> SerializeApplyPgaMotorSemanticV1(const ApplyPgaMotorSemanticV1& value)
{
    base::BinaryWriter writer;
    const auto schema = LiteralIdentity("SGE4-5.Spiral1.ApplyPgaMotorSemantic.V1");
    writer.WriteBytes(schema);
    const auto motorBytes = value.motor.Serialize();
    writer.WriteBytes(motorBytes);
    writer.WriteBytes(Float4PointSchemaIdentityV1());
    writer.WriteU32(value.pointCount);
    writer.WriteU32(0);
    writer.WriteBytes(value.corpusIdentity);
    writer.WriteBytes(Float4PointOutputSchemaIdentityV1());
    writer.WriteBytes(value.observationContractIdentity);
    return std::move(writer).Take();
}

base::Digest256 ApplyPgaMotorSemanticIdentityV1(const ApplyPgaMotorSemanticV1& value)
{
    const auto bytes = SerializeApplyPgaMotorSemanticV1(value);
    return base::Sha256(bytes);
}
}
