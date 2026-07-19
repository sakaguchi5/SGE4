#include "Spiral1Corpus.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>

namespace sge4_5::spiral1::corpus
{
namespace
{
using semantic::CanonicalPgaMotorV1;
using semantic::Double3;
using semantic::Float4Point;
using semantic::UnitQuaternion;

class SplitMix64 final
{
public:
    explicit SplitMix64(std::uint64_t seed) : state_(seed) {}
    [[nodiscard]] std::uint64_t Next() noexcept
    {
        state_ += 0x9e3779b97f4a7c15ull;
        std::uint64_t z = state_;
        z = (z ^ (z >> 30u)) * 0xbf58476d1ce4e5b9ull;
        z = (z ^ (z >> 27u)) * 0x94d049bb133111ebull;
        return z ^ (z >> 31u);
    }
    [[nodiscard]] double Unit24() noexcept
    {
        const std::uint64_t u24 = Next() >> 40u;
        return static_cast<double>(u24) / 16777216.0;
    }
    [[nodiscard]] double Range(double minimum, double maximum) noexcept
    {
        return minimum + (maximum - minimum) * Unit24();
    }
private:
    std::uint64_t state_ = 0;
};

[[nodiscard]] double FromBits(std::uint64_t bits) noexcept { return std::bit_cast<double>(bits); }
[[nodiscard]] float CanonicalFloat(double value) noexcept
{
    const float result = static_cast<float>(value);
    return result == 0.0f ? 0.0f : result;
}
[[nodiscard]] Float4Point Point(double x, double y, double z) noexcept
{
    return {CanonicalFloat(x), CanonicalFloat(y), CanonicalFloat(z), 1.0f};
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

[[nodiscard]] base::Result<CanonicalPgaMotorV1, std::string> AxisAngleMotor(
    Double3 axis, double angle, Double3 translation)
{
    auto rotation = UnitQuaternion::FromAxisAngle(axis, angle);
    if (!rotation) return base::Result<CanonicalPgaMotorV1, std::string>::Failure(rotation.Error());
    return semantic::BuildMotor(rotation.Value(), translation);
}

[[nodiscard]] std::vector<Float4Point> Grid4()
{
    constexpr std::array<double, 4> values{-3.0, -1.0, 1.0, 3.0};
    std::vector<Float4Point> points;
    points.reserve(64);
    for (double x : values) for (double y : values) for (double z : values) points.push_back(Point(x, y, z));
    return points;
}

[[nodiscard]] std::vector<Float4Point> Grid8x8x16()
{
    std::vector<Float4Point> points;
    points.reserve(1024);
    for (std::uint32_t x = 0; x < 8; ++x)
        for (std::uint32_t y = 0; y < 8; ++y)
            for (std::uint32_t z = 0; z < 16; ++z)
                points.push_back(Point(static_cast<double>(x) - 3.5, static_cast<double>(y) - 3.5, static_cast<double>(z) - 7.5));
    return points;
}

[[nodiscard]] std::vector<Float4Point> RandomBox(
    SplitMix64& random,
    std::uint32_t count,
    double minimum,
    double maximum)
{
    std::vector<Float4Point> points;
    points.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i)
        points.push_back(Point(random.Range(minimum, maximum), random.Range(minimum, maximum), random.Range(minimum, maximum)));
    return points;
}

[[nodiscard]] Double3 RandomUnitAxis(SplitMix64& random)
{
    for (;;)
    {
        Double3 axis{random.Range(-1.0, 1.0), random.Range(-1.0, 1.0), random.Range(-1.0, 1.0)};
        const double normSquared = axis.x * axis.x + axis.y * axis.y + axis.z * axis.z;
        if (normSquared >= 0x1p-20)
        {
            const double inverse = 1.0 / std::sqrt(normSquared);
            return {axis.x * inverse, axis.y * inverse, axis.z * inverse};
        }
    }
}

[[nodiscard]] base::Result<CanonicalPgaMotorV1, std::string> RandomMotor(
    SplitMix64& random,
    double angleMinimum,
    double angleMaximum,
    double translationMinimum,
    double translationMaximum)
{
    const Double3 axis = RandomUnitAxis(random);
    const double angle = random.Range(angleMinimum, angleMaximum);
    const Double3 translation{
        random.Range(translationMinimum, translationMaximum),
        random.Range(translationMinimum, translationMaximum),
        random.Range(translationMinimum, translationMaximum)};
    return AxisAngleMotor(axis, angle, translation);
}

void WriteString(base::BinaryWriter& writer, std::string_view value)
{
    writer.WriteU32(static_cast<std::uint32_t>(value.size()));
    writer.WriteBytes(std::as_bytes(std::span(value.data(), value.size())));
}

void WriteF64(base::BinaryWriter& writer, double value) { writer.WriteU64(std::bit_cast<std::uint64_t>(value)); }

[[nodiscard]] std::vector<std::byte> CanonicalCorpusDefinitionBytes()
{
    base::BinaryWriter writer;
    WriteString(writer, "SGE4-5.Spiral1.CorpusDefinition.V1");
    WriteString(writer, "SplitMix64");
    writer.WriteU64(0x9e3779b97f4a7c15ull);
    writer.WriteU64(0xbf58476d1ce4e5b9ull);
    writer.WriteU64(0x94d049bb133111ebull);
    writer.WriteU64(0x400921fb54442d18ull);
    writer.WriteU64(0x3ff921fb54442d18ull);
    writer.WriteU64(0x3eb0000000000000ull);
    writer.WriteU64(0x400921f954442d18ull);
    constexpr std::array<std::uint64_t, 9> seeds{
        0x5307000000000001ull, 0x5308000000000001ull, 0x5309000000000001ull,
        0x5310000000000001ull, 0x5311000000000001ull, 0x5312000000000001ull,
        0x5313000000000001ull, 0x5314000000000001ull, 0x5314000000000002ull};
    for (auto seed : seeds) writer.WriteU64(seed);
    writer.WriteU64(0x5315000000000001ull);
    for (std::uint32_t i = 0; i < 16; ++i)
    {
        char id[4]{'S', static_cast<char>('0' + i / 10), static_cast<char>('0' + i % 10), 0};
        writer.WriteBytes(std::as_bytes(std::span(id, 4)));
    }
    WriteF64(writer, contracts::AbsoluteToleranceV1);
    WriteF64(writer, contracts::RelativeToleranceV1);
    writer.WriteU32(contracts::ReferenceAlgorithmVersionV1);
    return std::move(writer).Take();
}

[[nodiscard]] base::Digest256 CaseIdentity(
    const base::Digest256& corpusIdentity,
    std::string_view scenarioId,
    std::uint32_t subscenario,
    std::uint64_t seed,
    const CanonicalPgaMotorV1& motor,
    std::span<const Float4Point> points)
{
    base::BinaryWriter writer;
    writer.WriteBytes(corpusIdentity);
    WriteString(writer, scenarioId);
    writer.WriteU32(subscenario);
    writer.WriteU64(seed);
    const auto motorBytes = motor.Serialize();
    writer.WriteBytes(motorBytes);
    writer.WriteU32(static_cast<std::uint32_t>(points.size()));
    const auto pointBytes = semantic::SerializePoints(points);
    writer.WriteBytes(pointBytes);
    writer.WriteU32(contracts::ReferenceAlgorithmVersionV1);
    return base::Sha256(writer.Bytes());
}

[[nodiscard]] base::Result<void, std::string> AddCase(
    QualificationCorpusV1& corpusValue,
    std::string id,
    std::uint32_t subscenario,
    std::uint64_t seed,
    CanonicalPgaMotorV1 motor,
    std::vector<Float4Point> points)
{
    for (const auto& point : points)
    {
        auto valid = semantic::ValidatePoint(point);
        if (!valid) return base::Result<void, std::string>::Failure(id + ": " + valid.Error());
    }
    auto references = EvaluateCpuReferenceBatchV1(motor, points);
    if (!references) return base::Result<void, std::string>::Failure(id + ": " + references.Error());
    const auto identity = CaseIdentity(corpusValue.contractIdentity, id, subscenario, seed, motor, points);
    corpusValue.cases.emplace_back(std::move(id), subscenario, seed, std::move(motor), std::move(points), std::move(references.Value()), identity);
    return base::Result<void, std::string>::Success();
}
}

base::Result<Float4Point, std::string> EvaluateCpuReferenceV1(
    const CanonicalPgaMotorV1& motor,
    const Float4Point& point)
{
    auto pointValidation = semantic::ValidatePoint(point);
    if (!pointValidation) return base::Result<Float4Point, std::string>::Failure(pointValidation.Error());
    const std::array<double, 4> p{0.0, static_cast<double>(point.x), static_cast<double>(point.y), static_cast<double>(point.z)};
    const auto rotated = Multiply(Multiply(motor.Qr(), p), Conjugate(motor.Qr()));
    const auto translation = Multiply(motor.Qd(), Conjugate(motor.Qr()));
    const Float4Point result = Point(
        rotated[1] + 2.0 * translation[1],
        rotated[2] + 2.0 * translation[2],
        rotated[3] + 2.0 * translation[3]);
    if (!std::isfinite(result.x) || !std::isfinite(result.y) || !std::isfinite(result.z) ||
        std::bit_cast<std::uint32_t>(result.w) != 0x3f800000u)
        return base::Result<Float4Point, std::string>::Failure("reference output is not finite or has invalid w");
    return base::Result<Float4Point, std::string>::Success(result);
}

base::Result<std::vector<Float4Point>, std::string> EvaluateCpuReferenceBatchV1(
    const CanonicalPgaMotorV1& motor,
    std::span<const Float4Point> points)
{
    std::vector<Float4Point> output;
    output.reserve(points.size());
    for (const auto& point : points)
    {
        auto transformed = EvaluateCpuReferenceV1(motor, point);
        if (!transformed) return base::Result<std::vector<Float4Point>, std::string>::Failure(transformed.Error());
        output.push_back(transformed.Value());
    }
    return base::Result<std::vector<Float4Point>, std::string>::Success(std::move(output));
}

base::Digest256 CorpusContractIdentityV1()
{
    const auto bytes = CanonicalCorpusDefinitionBytes();
    return base::Sha256(bytes);
}

base::Result<QualificationCorpusV1, std::string> BuildQualificationCorpusV1()
{
    QualificationCorpusV1 result{};
    result.contractIdentity = CorpusContractIdentityV1();
    const auto identity = semantic::BuildIdentityMotor();
    const auto add = [&](std::string id, std::uint32_t sub, std::uint64_t seed, CanonicalPgaMotorV1 motor, std::vector<Float4Point> points) {
        return AddCase(result, std::move(id), sub, seed, std::move(motor), std::move(points));
    };

    auto status = add("S00", 0, 0, identity, {Point(0, 0, 0)}); if (!status) return base::Result<QualificationCorpusV1, std::string>::Failure(status.Error());
    status = add("S01", 0, 0, identity, Grid8x8x16()); if (!status) return base::Result<QualificationCorpusV1, std::string>::Failure(status.Error());

    auto s02 = AxisAngleMotor({1,0,0}, 0.0, {3.25,-2.5,7.75}); if (!s02) return base::Result<QualificationCorpusV1, std::string>::Failure(s02.Error());
    status = add("S02", 0, 0, s02.Value(), Grid4()); if (!status) return base::Result<QualificationCorpusV1, std::string>::Failure(status.Error());
    const double halfPi = FromBits(0x3ff921fb54442d18ull);
    const double pi = FromBits(0x400921fb54442d18ull);
    auto s03 = AxisAngleMotor({1,0,0}, halfPi, {0,0,0}); if (!s03) return base::Result<QualificationCorpusV1, std::string>::Failure(s03.Error());
    auto s04 = AxisAngleMotor({0,1,0}, halfPi, {0,0,0}); if (!s04) return base::Result<QualificationCorpusV1, std::string>::Failure(s04.Error());
    auto s05 = AxisAngleMotor({0,0,1}, halfPi, {0,0,0}); if (!s05) return base::Result<QualificationCorpusV1, std::string>::Failure(s05.Error());
    auto s06 = AxisAngleMotor({1,2,3}, pi, {0,0,0}); if (!s06) return base::Result<QualificationCorpusV1, std::string>::Failure(s06.Error());
    for (auto* pair : {&s03, &s04, &s05, &s06}) { (void)pair; }
    status = add("S03",0,0,s03.Value(),Grid4()); if (!status) return base::Result<QualificationCorpusV1, std::string>::Failure(status.Error());
    status = add("S04",0,0,s04.Value(),Grid4()); if (!status) return base::Result<QualificationCorpusV1, std::string>::Failure(status.Error());
    status = add("S05",0,0,s05.Value(),Grid4()); if (!status) return base::Result<QualificationCorpusV1, std::string>::Failure(status.Error());
    status = add("S06",0,0,s06.Value(),Grid4()); if (!status) return base::Result<QualificationCorpusV1, std::string>::Failure(status.Error());

    auto s07 = AxisAngleMotor({2,-1,3}, FromBits(0x3fe6666666666666ull), {4.5,-3.25,1.75}); if (!s07) return base::Result<QualificationCorpusV1, std::string>::Failure(s07.Error());
    SplitMix64 r07(0x5307000000000001ull);
    status = add("S07",0,0x5307000000000001ull,s07.Value(),RandomBox(r07,1024,-100,100)); if (!status) return base::Result<QualificationCorpusV1, std::string>::Failure(status.Error());
    auto s08 = AxisAngleMotor({1,-2,0.5}, FromBits(0x3eb0000000000000ull), {0.001,-0.002,0.003}); if (!s08) return base::Result<QualificationCorpusV1, std::string>::Failure(s08.Error());
    SplitMix64 r08(0x5308000000000001ull);
    status = add("S08",0,0x5308000000000001ull,s08.Value(),RandomBox(r08,1024,-10,10)); if (!status) return base::Result<QualificationCorpusV1, std::string>::Failure(status.Error());
    auto s09 = AxisAngleMotor({-2,5,1}, FromBits(0x400921f954442d18ull), {-7,4,2.5}); if (!s09) return base::Result<QualificationCorpusV1, std::string>::Failure(s09.Error());
    SplitMix64 r09(0x5309000000000001ull);
    status = add("S09",0,0x5309000000000001ull,s09.Value(),RandomBox(r09,1024,-100,100)); if (!status) return base::Result<QualificationCorpusV1, std::string>::Failure(status.Error());

    auto m1 = AxisAngleMotor({1,0,0}, FromBits(0x3fd8000000000000ull), {1,2,3}); if (!m1) return base::Result<QualificationCorpusV1, std::string>::Failure(m1.Error());
    auto m2 = AxisAngleMotor({0,1,1}, FromBits(0xbff2000000000000ull), {-2,0.5,4}); if (!m2) return base::Result<QualificationCorpusV1, std::string>::Failure(m2.Error());
    auto m3 = AxisAngleMotor({2,-1,1}, FromBits(0x3fe4000000000000ull), {0,-3,1.25}); if (!m3) return base::Result<QualificationCorpusV1, std::string>::Failure(m3.Error());
    auto m21 = semantic::ComposeMotors(m1.Value(), m2.Value()); if (!m21) return base::Result<QualificationCorpusV1, std::string>::Failure(m21.Error());
    auto s10 = semantic::ComposeMotors(m21.Value(), m3.Value()); if (!s10) return base::Result<QualificationCorpusV1, std::string>::Failure(s10.Error());
    SplitMix64 r10(0x5310000000000001ull);
    status = add("S10",0,0x5310000000000001ull,s10.Value(),RandomBox(r10,1024,-50,50)); if (!status) return base::Result<QualificationCorpusV1, std::string>::Failure(status.Error());

    SplitMix64 r11(0x5311000000000001ull);
    status = add("S11",0,0x5311000000000001ull,identity,RandomBox(r11,4096,-1000,-0.125)); if (!status) return base::Result<QualificationCorpusV1, std::string>::Failure(status.Error());
    SplitMix64 r12(0x5312000000000001ull);
    status = add("S12",0,0x5312000000000001ull,s07.Value(),RandomBox(r12,4096,-10000,10000)); if (!status) return base::Result<QualificationCorpusV1, std::string>::Failure(status.Error());
    SplitMix64 r13(0x5313000000000001ull);
    status = add("S13",0,0x5313000000000001ull,s08.Value(),RandomBox(r13,4096,-0.000244140625,0.000244140625)); if (!status) return base::Result<QualificationCorpusV1, std::string>::Failure(status.Error());
    SplitMix64 motor14(0x5314000000000001ull);
    auto s14 = RandomMotor(motor14,-3,3,-25,25); if (!s14) return base::Result<QualificationCorpusV1, std::string>::Failure(s14.Error());
    SplitMix64 points14(0x5314000000000002ull);
    status = add("S14",0,0x5314000000000001ull,s14.Value(),RandomBox(points14,4096,-250,250)); if (!status) return base::Result<QualificationCorpusV1, std::string>::Failure(status.Error());

    SplitMix64 r15(0x5315000000000001ull);
    for (std::uint32_t block = 0; block < 16; ++block)
    {
        auto motor = RandomMotor(r15,-3,3,-50,50); if (!motor) return base::Result<QualificationCorpusV1, std::string>::Failure(motor.Error());
        auto points = RandomBox(r15,256,-500,500);
        status = add("S15",block,0x5315000000000001ull,motor.Value(),std::move(points));
        if (!status) return base::Result<QualificationCorpusV1, std::string>::Failure(status.Error());
    }
    return base::Result<QualificationCorpusV1, std::string>::Success(std::move(result));
}

std::vector<std::byte> SerializeCorpusFoundationBundleV1(const QualificationCorpusV1& corpusValue)
{
    base::BinaryWriter writer;
    constexpr std::string_view magic = "SGE4-5.CU1.CorpusFoundation.V1";
    WriteString(writer, magic);
    writer.WriteBytes(corpusValue.contractIdentity);
    writer.WriteBytes(contracts::ObservationContractIdentityV2());
    writer.WriteU32(16);
    writer.WriteU32(static_cast<std::uint32_t>(corpusValue.cases.size()));
    for (const auto& value : corpusValue.cases)
    {
        WriteString(writer, value.scenarioId);
        writer.WriteU32(value.subscenarioIndex);
        writer.WriteU64(value.seed);
        writer.WriteBytes(value.caseIdentity);
        const auto motorBytes = value.motor.Serialize();
        writer.WriteU32(static_cast<std::uint32_t>(motorBytes.size()));
        writer.WriteBytes(motorBytes);
        writer.WriteU32(static_cast<std::uint32_t>(value.inputPoints.size()));
        const auto inputBytes = semantic::SerializePoints(value.inputPoints);
        const auto referenceBytes = semantic::SerializePoints(value.referencePoints);
        writer.WriteU64(static_cast<std::uint64_t>(inputBytes.size()));
        writer.WriteBytes(inputBytes);
        writer.WriteU64(static_cast<std::uint64_t>(referenceBytes.size()));
        writer.WriteBytes(referenceBytes);
        auto semanticValue = semantic::BuildApplyPgaMotorSemanticV1(
            value.motor,
            static_cast<std::uint32_t>(value.inputPoints.size()),
            value.caseIdentity,
            contracts::ObservationContractIdentityV2());
        if (!semanticValue) throw std::runtime_error(semanticValue.Error());
        const auto semanticBytes = semantic::SerializeApplyPgaMotorSemanticV1(semanticValue.Value());
        writer.WriteU32(static_cast<std::uint32_t>(semanticBytes.size()));
        writer.WriteBytes(semanticBytes);
    }
    return std::move(writer).Take();
}

std::size_t TotalPointCount(const QualificationCorpusV1& corpusValue) noexcept
{
    std::size_t total = 0;
    for (const auto& value : corpusValue.cases) total += value.inputPoints.size();
    return total;
}
}
