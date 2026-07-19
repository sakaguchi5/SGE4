#include "Spiral1Contracts.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>

namespace sge4_5::spiral1::contracts
{
namespace
{
[[nodiscard]] bool IsFinitePoint(const semantic::Float4Point& point) noexcept
{
    return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z) && std::isfinite(point.w);
}

[[nodiscard]] bool IsExactW(const semantic::Float4Point& point) noexcept
{
    return std::bit_cast<std::uint32_t>(point.w) == 0x3f800000u;
}

[[nodiscard]] std::uint32_t OrderedFloat(float value) noexcept
{
    const std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
    return (bits & 0x8000'0000u) != 0u ? 0x8000'0000u - bits : 0x8000'0000u + bits;
}

void WriteF32(base::BinaryWriter& writer, float value)
{
    writer.WriteU32(std::bit_cast<std::uint32_t>(value == 0.0f ? 0.0f : value));
}

void WriteF64(base::BinaryWriter& writer, double value)
{
    writer.WriteU64(std::bit_cast<std::uint64_t>(value == 0.0 ? 0.0 : value));
}
}

ObservationContractV1 CanonicalObservationContractV1() { return {}; }

std::vector<std::byte> SerializeObservationContractV1(const ObservationContractV1& value)
{
    base::BinaryWriter writer;
    constexpr std::string_view schema = "SGE4-5.Spiral1.ObservationContract.V1";
    writer.WriteU32(static_cast<std::uint32_t>(schema.size()));
    writer.WriteBytes(std::as_bytes(std::span(schema.data(), schema.size())));
    writer.WriteU32(value.schemaVersion);
    writer.WriteU32(value.referenceAlgorithmVersion);
    writer.WriteU32(value.recordVersion);
    writer.WriteU32(value.recordBytes);
    WriteF32(writer, value.absoluteTolerance);
    WriteF32(writer, value.relativeTolerance);
    WriteF32(writer, value.pairAbsoluteTolerance);
    writer.WriteU32(value.reserved);
    writer.WriteU32(0); // finite flag layout version
    writer.WriteU32(0); // mismatch flag layout version
    writer.WriteU32(1); // point-index ascending aggregation
    return std::move(writer).Take();
}

base::Digest256 ObservationContractIdentityV1()
{
    const auto bytes = SerializeObservationContractV1(CanonicalObservationContractV1());
    return base::Sha256(bytes);
}


ObservationContractV2 CanonicalObservationContractV2() { return {}; }

std::vector<std::byte> SerializeObservationContractV2(const ObservationContractV2& value)
{
    base::BinaryWriter writer;
    constexpr std::string_view schema = "SGE4-5.Spiral1.ObservationContract.V2";
    writer.WriteU32(static_cast<std::uint32_t>(schema.size()));
    writer.WriteBytes(std::as_bytes(std::span(schema.data(), schema.size())));
    writer.WriteU32(value.schemaVersion);
    writer.WriteU32(value.referenceAlgorithmVersion);
    writer.WriteU32(value.recordVersion);
    writer.WriteU32(value.recordBytes);
    WriteF32(writer, value.absoluteTolerance);
    WriteF32(writer, value.relativeTolerance);
    WriteF32(writer, value.pairAbsoluteTolerance);
    writer.WriteU32(value.toleranceScaleAlgorithmVersion);
    writer.WriteU32(0); // finite flag layout version
    writer.WriteU32(0); // mismatch flag layout version
    writer.WriteU32(1); // point-index ascending aggregation
    return std::move(writer).Take();
}

base::Digest256 ObservationContractIdentityV2()
{
    const auto bytes = SerializeObservationContractV2(CanonicalObservationContractV2());
    return base::Sha256(bytes);
}

std::uint32_t UlpDistance(float a, float b) noexcept
{
    if (a == 0.0f && b == 0.0f) return 0;
    if (!std::isfinite(a) || !std::isfinite(b)) return std::numeric_limits<std::uint32_t>::max();
    const std::uint32_t oa = OrderedFloat(a);
    const std::uint32_t ob = OrderedFloat(b);
    return oa >= ob ? oa - ob : ob - oa;
}

bool WithinReferenceTolerance(float actual, float reference) noexcept
{
    if (!std::isfinite(actual) || !std::isfinite(reference)) return false;
    const float difference = std::abs(actual - reference);
    const float magnitude = std::max(std::abs(reference), 1.0f);
    const float scaledTolerance = RelativeToleranceV1 * magnitude;
    const float limit = AbsoluteToleranceV1 + scaledTolerance;
    return difference <= limit;
}

bool WithinPairTolerance(float matrixValue, float pgaValue) noexcept
{
    if (!std::isfinite(matrixValue) || !std::isfinite(pgaValue)) return false;
    const float difference = std::abs(matrixValue - pgaValue);
    const float magnitude = std::max({std::abs(matrixValue), std::abs(pgaValue), 1.0f});
    const float scaledTolerance = RelativeToleranceV1 * magnitude;
    const float limit = PairAbsoluteToleranceV1 + scaledTolerance;
    return difference <= limit;
}

PointComparisonRecordV1 BuildComparisonRecordV1(
    std::uint32_t pointIndex,
    const semantic::Float4Point& reference,
    const semantic::Float4Point& matrixOutput,
    const semantic::Float4Point& pgaOutput)
{
    PointComparisonRecordV1 record{};
    record.pointIndex = pointIndex;
    const bool referenceFinite = IsFinitePoint(reference);
    const bool matrixFinite = IsFinitePoint(matrixOutput);
    const bool pgaFinite = IsFinitePoint(pgaOutput);
    const bool matrixW = IsExactW(matrixOutput);
    const bool pgaW = IsExactW(pgaOutput);
    if (referenceFinite) record.finiteFlags |= 1u << 0;
    if (matrixFinite) record.finiteFlags |= 1u << 1;
    if (pgaFinite) record.finiteFlags |= 1u << 2;
    if (matrixW) record.finiteFlags |= 1u << 3;
    if (pgaW) record.finiteFlags |= 1u << 4;
    if (!referenceFinite) record.mismatchFlags |= 1u << 0;
    if (!matrixFinite) record.mismatchFlags |= 1u << 1;
    if (!pgaFinite) record.mismatchFlags |= 1u << 2;
    if (!matrixW) record.mismatchFlags |= 1u << 3;
    if (!pgaW) record.mismatchFlags |= 1u << 4;

    const std::array<float, 3> r{reference.x, reference.y, reference.z};
    const std::array<float, 3> m{matrixOutput.x, matrixOutput.y, matrixOutput.z};
    const std::array<float, 3> p{pgaOutput.x, pgaOutput.y, pgaOutput.z};
    for (std::size_t component = 0; component < 3; ++component)
    {
        const float matrixDifference = m[component] - r[component];
        const float pgaDifference = p[component] - r[component];
        const float pairDifference = m[component] - p[component];
        record.matrixAbsError[component] = std::abs(matrixDifference);
        record.pgaAbsError[component] = std::abs(pgaDifference);
        record.pairAbsError[component] = std::abs(pairDifference);
        record.matrixUlp[component] = UlpDistance(m[component], r[component]);
        record.pgaUlp[component] = UlpDistance(p[component], r[component]);
        record.pairUlp[component] = UlpDistance(m[component], p[component]);
        if (!WithinReferenceTolerance(m[component], r[component])) record.mismatchFlags |= 1u << (8u + static_cast<std::uint32_t>(component));
        if (!WithinReferenceTolerance(p[component], r[component])) record.mismatchFlags |= 1u << (12u + static_cast<std::uint32_t>(component));
        if (!WithinPairTolerance(m[component], p[component])) record.mismatchFlags |= 1u << (16u + static_cast<std::uint32_t>(component));
    }
    return record;
}

float ReferencePointScaleV2(const semantic::Float4Point& reference) noexcept
{
    return std::max({std::abs(reference.x), std::abs(reference.y), std::abs(reference.z), 1.0f});
}

float PairPointScaleV2(
    const semantic::Float4Point& matrixOutput,
    const semantic::Float4Point& pgaOutput) noexcept
{
    return std::max({
        std::abs(matrixOutput.x), std::abs(matrixOutput.y), std::abs(matrixOutput.z),
        std::abs(pgaOutput.x), std::abs(pgaOutput.y), std::abs(pgaOutput.z), 1.0f});
}

bool WithinReferenceToleranceV2(
    float actual, float reference, float referencePointScale) noexcept
{
    if (!std::isfinite(actual) || !std::isfinite(reference) || !std::isfinite(referencePointScale)) return false;
    const float difference = std::abs(actual - reference);
    const float scaledTolerance = RelativeToleranceV1 * std::max(referencePointScale, 1.0f);
    const float limit = AbsoluteToleranceV1 + scaledTolerance;
    return difference <= limit;
}

bool WithinPairToleranceV2(
    float matrixValue, float pgaValue, float pairPointScale) noexcept
{
    if (!std::isfinite(matrixValue) || !std::isfinite(pgaValue) || !std::isfinite(pairPointScale)) return false;
    const float difference = std::abs(matrixValue - pgaValue);
    const float scaledTolerance = RelativeToleranceV1 * std::max(pairPointScale, 1.0f);
    const float limit = PairAbsoluteToleranceV1 + scaledTolerance;
    return difference <= limit;
}

PointComparisonRecordV1 BuildComparisonRecordV2(
    std::uint32_t pointIndex,
    const semantic::Float4Point& reference,
    const semantic::Float4Point& matrixOutput,
    const semantic::Float4Point& pgaOutput)
{
    PointComparisonRecordV1 record{};
    record.pointIndex = pointIndex;
    const bool referenceFinite = IsFinitePoint(reference);
    const bool matrixFinite = IsFinitePoint(matrixOutput);
    const bool pgaFinite = IsFinitePoint(pgaOutput);
    const bool matrixW = IsExactW(matrixOutput);
    const bool pgaW = IsExactW(pgaOutput);
    if (referenceFinite) record.finiteFlags |= 1u << 0;
    if (matrixFinite) record.finiteFlags |= 1u << 1;
    if (pgaFinite) record.finiteFlags |= 1u << 2;
    if (matrixW) record.finiteFlags |= 1u << 3;
    if (pgaW) record.finiteFlags |= 1u << 4;
    if (!referenceFinite) record.mismatchFlags |= 1u << 0;
    if (!matrixFinite) record.mismatchFlags |= 1u << 1;
    if (!pgaFinite) record.mismatchFlags |= 1u << 2;
    if (!matrixW) record.mismatchFlags |= 1u << 3;
    if (!pgaW) record.mismatchFlags |= 1u << 4;

    const float referenceScale = ReferencePointScaleV2(reference);
    const float pairScale = PairPointScaleV2(matrixOutput, pgaOutput);
    const std::array<float, 3> r{reference.x, reference.y, reference.z};
    const std::array<float, 3> m{matrixOutput.x, matrixOutput.y, matrixOutput.z};
    const std::array<float, 3> p{pgaOutput.x, pgaOutput.y, pgaOutput.z};
    for (std::size_t component = 0; component < 3; ++component)
    {
        const float matrixDifference = m[component] - r[component];
        const float pgaDifference = p[component] - r[component];
        const float pairDifference = m[component] - p[component];
        record.matrixAbsError[component] = std::abs(matrixDifference);
        record.pgaAbsError[component] = std::abs(pgaDifference);
        record.pairAbsError[component] = std::abs(pairDifference);
        record.matrixUlp[component] = UlpDistance(m[component], r[component]);
        record.pgaUlp[component] = UlpDistance(p[component], r[component]);
        record.pairUlp[component] = UlpDistance(m[component], p[component]);
        if (!WithinReferenceToleranceV2(m[component], r[component], referenceScale)) record.mismatchFlags |= 1u << (8u + static_cast<std::uint32_t>(component));
        if (!WithinReferenceToleranceV2(p[component], r[component], referenceScale)) record.mismatchFlags |= 1u << (12u + static_cast<std::uint32_t>(component));
        if (!WithinPairToleranceV2(m[component], p[component], pairScale)) record.mismatchFlags |= 1u << (16u + static_cast<std::uint32_t>(component));
    }
    return record;
}

std::vector<std::byte> SerializeComparisonRecordV1(const PointComparisonRecordV1& value)
{
    base::BinaryWriter writer;
    writer.WriteU32(value.pointIndex);
    writer.WriteU32(value.finiteFlags);
    for (float x : value.matrixAbsError) WriteF32(writer, x);
    for (float x : value.pgaAbsError) WriteF32(writer, x);
    for (float x : value.pairAbsError) WriteF32(writer, x);
    for (std::uint32_t x : value.matrixUlp) writer.WriteU32(x);
    for (std::uint32_t x : value.pgaUlp) writer.WriteU32(x);
    for (std::uint32_t x : value.pairUlp) writer.WriteU32(x);
    writer.WriteU32(value.mismatchFlags);
    for (std::uint32_t x : value.reserved) writer.WriteU32(x);
    return std::move(writer).Take();
}

std::vector<std::byte> SerializeComparisonRecordsV1(std::span<const PointComparisonRecordV1> values)
{
    base::BinaryWriter writer;
    for (const auto& value : values)
    {
        const auto bytes = SerializeComparisonRecordV1(value);
        writer.WriteBytes(bytes);
    }
    return std::move(writer).Take();
}

std::vector<std::byte> SerializeScenarioReportV1(const ScenarioReportV1& value)
{
    base::BinaryWriter writer;
    constexpr std::string_view schema = "SGE4-5.Spiral1.ScenarioReport.V1";
    writer.WriteU32(static_cast<std::uint32_t>(schema.size()));
    writer.WriteBytes(std::as_bytes(std::span(schema.data(), schema.size())));
    writer.WriteBytes(value.scenarioIdentity);
    writer.WriteU32(value.pointCount);
    writer.WriteU32(value.nonFiniteCount);
    writer.WriteU32(value.mismatchCount);
    writer.WriteU32(value.firstMismatchIndex);
    const auto writeSummary = [&](const ErrorSummaryV1& summary) {
        WriteF64(writer, summary.maxAbsoluteError);
        WriteF64(writer, summary.maxRelativeError);
        writer.WriteU32(summary.maxUlpDistance);
        writer.WriteU32(0);
        WriteF64(writer, summary.rmsEuclideanError);
    };
    writeSummary(value.matrix);
    writeSummary(value.pga);
    writeSummary(value.pair);
    writer.WriteBytes(value.inputDigest);
    writer.WriteBytes(value.referenceDigest);
    writer.WriteBytes(value.matrixOutputDigest);
    writer.WriteBytes(value.pgaOutputDigest);
    writer.WriteBytes(value.readbackDigest);
    return std::move(writer).Take();
}
}
