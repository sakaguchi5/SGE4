#pragma once

#include "../00_Foundation/Base.h"
#include "../60_PgaRigidTransformSemantic/PgaRigidTransformSemantic.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace sge4_5::spiral1::contracts
{
inline constexpr float AbsoluteToleranceV1 = 1.0e-4f;
inline constexpr float RelativeToleranceV1 = 1.0e-5f;
inline constexpr float PairAbsoluteToleranceV1 = 1.0e-4f;
inline constexpr std::uint32_t ReferenceAlgorithmVersionV1 = 1;
inline constexpr std::uint32_t ComparisonRecordVersionV1 = 1;
inline constexpr std::uint32_t ComparisonRecordBytesV1 = 96;

struct ObservationContractV1 final
{
    std::uint32_t schemaVersion = 1;
    std::uint32_t referenceAlgorithmVersion = ReferenceAlgorithmVersionV1;
    std::uint32_t recordVersion = ComparisonRecordVersionV1;
    std::uint32_t recordBytes = ComparisonRecordBytesV1;
    float absoluteTolerance = AbsoluteToleranceV1;
    float relativeTolerance = RelativeToleranceV1;
    float pairAbsoluteTolerance = PairAbsoluteToleranceV1;
    std::uint32_t reserved = 0;
};

[[nodiscard]] ObservationContractV1 CanonicalObservationContractV1();
[[nodiscard]] std::vector<std::byte> SerializeObservationContractV1(const ObservationContractV1& value);
[[nodiscard]] base::Digest256 ObservationContractIdentityV1();

inline constexpr std::uint32_t ObservationContractSchemaVersionV2 = 2;
inline constexpr std::uint32_t ToleranceScaleAlgorithmVersionV2 = 2;

struct ObservationContractV2 final
{
    std::uint32_t schemaVersion = ObservationContractSchemaVersionV2;
    std::uint32_t referenceAlgorithmVersion = ReferenceAlgorithmVersionV1;
    std::uint32_t recordVersion = ComparisonRecordVersionV1;
    std::uint32_t recordBytes = ComparisonRecordBytesV1;
    float absoluteTolerance = AbsoluteToleranceV1;
    float relativeTolerance = RelativeToleranceV1;
    float pairAbsoluteTolerance = PairAbsoluteToleranceV1;
    std::uint32_t toleranceScaleAlgorithmVersion = ToleranceScaleAlgorithmVersionV2;
};

[[nodiscard]] ObservationContractV2 CanonicalObservationContractV2();
[[nodiscard]] std::vector<std::byte> SerializeObservationContractV2(const ObservationContractV2& value);
[[nodiscard]] base::Digest256 ObservationContractIdentityV2();

struct PointComparisonRecordV1 final
{
    std::uint32_t pointIndex = 0;
    std::uint32_t finiteFlags = 0;
    std::array<float, 3> matrixAbsError{};
    std::array<float, 3> pgaAbsError{};
    std::array<float, 3> pairAbsError{};
    std::array<std::uint32_t, 3> matrixUlp{};
    std::array<std::uint32_t, 3> pgaUlp{};
    std::array<std::uint32_t, 3> pairUlp{};
    std::uint32_t mismatchFlags = 0;
    std::array<std::uint32_t, 3> reserved{};
};
static_assert(sizeof(PointComparisonRecordV1) == ComparisonRecordBytesV1);

[[nodiscard]] std::uint32_t UlpDistance(float a, float b) noexcept;
[[nodiscard]] bool WithinReferenceTolerance(float actual, float reference) noexcept;
[[nodiscard]] bool WithinPairTolerance(float matrixValue, float pgaValue) noexcept;
[[nodiscard]] PointComparisonRecordV1 BuildComparisonRecordV1(
    std::uint32_t pointIndex,
    const semantic::Float4Point& reference,
    const semantic::Float4Point& matrixOutput,
    const semantic::Float4Point& pgaOutput);

[[nodiscard]] float ReferencePointScaleV2(const semantic::Float4Point& reference) noexcept;
[[nodiscard]] float PairPointScaleV2(
    const semantic::Float4Point& matrixOutput,
    const semantic::Float4Point& pgaOutput) noexcept;
[[nodiscard]] bool WithinReferenceToleranceV2(
    float actual, float reference, float referencePointScale) noexcept;
[[nodiscard]] bool WithinPairToleranceV2(
    float matrixValue, float pgaValue, float pairPointScale) noexcept;
[[nodiscard]] PointComparisonRecordV1 BuildComparisonRecordV2(
    std::uint32_t pointIndex,
    const semantic::Float4Point& reference,
    const semantic::Float4Point& matrixOutput,
    const semantic::Float4Point& pgaOutput);
[[nodiscard]] std::vector<std::byte> SerializeComparisonRecordV1(const PointComparisonRecordV1& value);
[[nodiscard]] std::vector<std::byte> SerializeComparisonRecordsV1(std::span<const PointComparisonRecordV1> values);

struct ErrorSummaryV1 final
{
    double maxAbsoluteError = 0.0;
    double maxRelativeError = 0.0;
    std::uint32_t maxUlpDistance = 0;
    double rmsEuclideanError = 0.0;
};

struct ScenarioReportV1 final
{
    base::Digest256 scenarioIdentity{};
    std::uint32_t pointCount = 0;
    std::uint32_t nonFiniteCount = 0;
    std::uint32_t mismatchCount = 0;
    std::uint32_t firstMismatchIndex = 0xffff'ffffu;
    ErrorSummaryV1 matrix{};
    ErrorSummaryV1 pga{};
    ErrorSummaryV1 pair{};
    base::Digest256 inputDigest{};
    base::Digest256 referenceDigest{};
    base::Digest256 matrixOutputDigest{};
    base::Digest256 pgaOutputDigest{};
    base::Digest256 readbackDigest{};
};

[[nodiscard]] std::vector<std::byte> SerializeScenarioReportV1(const ScenarioReportV1& value);
}
