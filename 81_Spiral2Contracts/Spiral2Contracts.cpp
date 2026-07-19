#include "Spiral2Contracts.h"

#include <bit>
#include <cmath>
#include <string_view>

namespace sge4_5::spiral2::contracts
{
namespace
{
constexpr double MinimumNormSquared = 0x1p-40;
constexpr double SignEpsilon = 0x1p-52;
constexpr double FloatUnitTolerance = 3.0e-6;
constexpr double FloatDualTolerance = 3.0e-5;

[[nodiscard]] base::Digest256 Literal(std::string_view value)
{
    return base::Sha256(std::as_bytes(std::span(value.data(), value.size())));
}

[[nodiscard]] std::array<double, 4> Multiply(const std::array<double, 4>& a, const std::array<double, 4>& b)
{
    return {a[0]*b[0]-a[1]*b[1]-a[2]*b[2]-a[3]*b[3],
            a[0]*b[1]+a[1]*b[0]+a[2]*b[3]-a[3]*b[2],
            a[0]*b[2]-a[1]*b[3]+a[2]*b[0]+a[3]*b[1],
            a[0]*b[3]+a[1]*b[2]-a[2]*b[1]+a[3]*b[0]};
}

[[nodiscard]] float CanonicalZero(float value) noexcept { return value == 0.0f ? 0.0f : value; }

[[nodiscard]] base::Result<DynamicMotorRecordV1, std::string> BuildRecord(const RotationTranslationV1& input)
{
    const std::array<double, 7> all{input.qw,input.qx,input.qy,input.qz,input.tx,input.ty,input.tz};
    for (const auto value : all) if (!std::isfinite(value))
        return base::Result<DynamicMotorRecordV1, std::string>::Failure("motor input contains NaN or Infinity");
    std::array<double,4> qr{input.qw,input.qx,input.qy,input.qz};
    const double norm2 = qr[0]*qr[0]+qr[1]*qr[1]+qr[2]*qr[2]+qr[3]*qr[3];
    if (!std::isfinite(norm2) || norm2 < MinimumNormSquared)
        return base::Result<DynamicMotorRecordV1, std::string>::Failure("rotation quaternion has zero length");
    const double inverse = 1.0 / std::sqrt(norm2);
    for (auto& value : qr) value *= inverse;
    bool flip = false;
    for (const auto value : qr) if (std::abs(value) > SignEpsilon) { flip = value < 0.0; break; }
    if (flip) for (auto& value : qr) value = -value;
    const auto qd64 = Multiply({0.0,input.tx,input.ty,input.tz}, qr);
    DynamicMotorRecordV1 result;
    for (std::size_t i=0;i<4;++i)
    {
        result.qr[i] = CanonicalZero(static_cast<float>(qr[i]));
        result.qd[i] = CanonicalZero(static_cast<float>(qd64[i] * 0.5));
    }
    return base::Result<DynamicMotorRecordV1, std::string>::Success(result);
}

[[nodiscard]] base::Result<void, std::string> ValidateRecord(const DynamicMotorRecordV1& record)
{
    double norm2 = 0.0, dot = 0.0;
    for (std::size_t i=0;i<4;++i)
    {
        if (!std::isfinite(record.qr[i]) || !std::isfinite(record.qd[i]))
            return base::Result<void, std::string>::Failure("motor bytes contain NaN or Infinity");
        if ((record.qr[i] == 0.0f && std::signbit(record.qr[i])) ||
            (record.qd[i] == 0.0f && std::signbit(record.qd[i])))
            return base::Result<void, std::string>::Failure("motor bytes contain negative zero");
        norm2 += static_cast<double>(record.qr[i]) * record.qr[i];
        dot += static_cast<double>(record.qr[i]) * record.qd[i];
    }
    if (std::abs(norm2 - 1.0) > FloatUnitTolerance)
        return base::Result<void, std::string>::Failure("rotation is outside the binary32 unit tolerance");
    if (std::abs(dot) > FloatDualTolerance)
        return base::Result<void, std::string>::Failure("dual quaternion orthogonality is invalid");
    for (const auto value : record.qr)
        if (std::abs(value) > 1.0e-12f) return value > 0.0f
            ? base::Result<void, std::string>::Success()
            : base::Result<void, std::string>::Failure("rotation sign is not canonical");
    return base::Result<void, std::string>::Failure("rotation quaternion has zero length");
}
}

base::Digest256 DynamicPaletteSlotIdentityV1() { return Literal("SGE4-5.Spiral2.DynamicLocalMotorPalette.Slot.V1"); }

base::Result<DynamicLocalMotorPaletteV1, std::string> BuildDynamicLocalMotorPaletteV1(
    std::uint32_t expectedBoneCount, std::span<const RotationTranslationV1> values)
{
    if (expectedBoneCount == 0 || values.size() != expectedBoneCount)
        return base::Result<DynamicLocalMotorPaletteV1, std::string>::Failure("bone count does not match invocation schema");
    DynamicLocalMotorPaletteV1 result;
    result.boneCount = expectedBoneCount;
    result.motors.reserve(values.size());
    for (const auto& input : values)
    {
        auto record = BuildRecord(input);
        if (!record) return base::Result<DynamicLocalMotorPaletteV1, std::string>::Failure(record.Error());
        result.motors.push_back(record.Value());
    }
    return base::Result<DynamicLocalMotorPaletteV1, std::string>::Success(std::move(result));
}

base::Result<void, std::string> ValidateDynamicLocalMotorPaletteV1(
    const DynamicLocalMotorPaletteV1& palette, std::uint32_t expectedBoneCount)
{
    if (palette.boneCount != expectedBoneCount || palette.motors.size() != expectedBoneCount)
        return base::Result<void, std::string>::Failure("palette bone count does not match invocation schema");
    for (const auto& record : palette.motors)
    {
        auto valid = ValidateRecord(record);
        if (!valid) return valid;
    }
    return base::Result<void, std::string>::Success();
}

std::vector<std::byte> SerializeDynamicLocalMotorPaletteV1(const DynamicLocalMotorPaletteV1& palette)
{
    base::BinaryWriter writer;
    for (const auto& motor : palette.motors)
    {
        for (const auto value : motor.qr) writer.WriteU32(std::bit_cast<std::uint32_t>(CanonicalZero(value)));
        for (const auto value : motor.qd) writer.WriteU32(std::bit_cast<std::uint32_t>(CanonicalZero(value)));
    }
    return std::move(writer).Take();
}

base::Result<DynamicLocalMotorPaletteV1, std::string> DeserializeDynamicLocalMotorPaletteV1(
    std::uint32_t expectedBoneCount, std::span<const std::byte> bytes)
{
    if (bytes.size() != static_cast<std::size_t>(expectedBoneCount) * DynamicMotorStrideBytesV1)
        return base::Result<DynamicLocalMotorPaletteV1, std::string>::Failure("dynamic slot byte size is not exact");
    base::BinaryReader reader(bytes);
    DynamicLocalMotorPaletteV1 result;
    result.boneCount = expectedBoneCount;
    result.motors.resize(expectedBoneCount);
    for (auto& record : result.motors)
    {
        for (auto& value : record.qr) { auto bits=reader.ReadU32(); if(!bits) return base::Result<DynamicLocalMotorPaletteV1,std::string>::Failure("truncated qr"); value=std::bit_cast<float>(bits.Value()); }
        for (auto& value : record.qd) { auto bits=reader.ReadU32(); if(!bits) return base::Result<DynamicLocalMotorPaletteV1,std::string>::Failure("truncated qd"); value=std::bit_cast<float>(bits.Value()); }
        auto valid = ValidateRecord(record);
        if (!valid) return base::Result<DynamicLocalMotorPaletteV1, std::string>::Failure(valid.Error());
    }
    return base::Result<DynamicLocalMotorPaletteV1, std::string>::Success(std::move(result));
}

DynamicPaletteInvocationSchemaV1 BuildDynamicPaletteInvocationSchemaV1(const hierarchy::RigidHierarchySemanticV1& hierarchyValue)
{
    return {hierarchy::RigidHierarchySemanticIdentityV1(hierarchyValue), DynamicPaletteSlotIdentityV1(),
            hierarchyValue.boneCount, hierarchyValue.boneCount * DynamicMotorStrideBytesV1, DynamicPaletteAlignmentV1};
}

std::vector<std::byte> SerializeDynamicPaletteInvocationSchemaV1(const DynamicPaletteInvocationSchemaV1& schema)
{
    base::BinaryWriter writer;
    writer.WriteBytes(Literal("SGE4-5.Spiral2.DynamicPaletteInvocationSchema.V1"));
    writer.WriteBytes(schema.hierarchySemanticIdentity);
    writer.WriteBytes(schema.slotIdentity);
    writer.WriteU32(schema.boneCount);
    writer.WriteU32(schema.requiredBytes);
    writer.WriteU32(schema.alignment);
    writer.WriteU32(DynamicMotorConventionVersionV1);
    return std::move(writer).Take();
}

base::Digest256 DynamicPaletteInvocationSchemaIdentityV1(const DynamicPaletteInvocationSchemaV1& schema)
{
    return base::Sha256(SerializeDynamicPaletteInvocationSchemaV1(schema));
}
}
