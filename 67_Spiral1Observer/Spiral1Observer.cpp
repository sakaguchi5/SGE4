#include "Spiral1Observer.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>

namespace sge4_5::spiral1::observer
{
namespace
{
[[nodiscard]] double RelativeError(double actual, double reference) noexcept
{
    return std::abs(actual - reference) / std::max(std::abs(reference), 1.0);
}

void UpdateSummary(
    contracts::ErrorSummaryV1& summary,
    const std::array<float, 3>& actual,
    const std::array<float, 3>& reference,
    const std::array<std::uint32_t, 3>& ulp,
    double& squaredAccumulator) noexcept
{
    double pointSquared = 0.0;
    for (std::size_t component = 0; component < 3; ++component)
    {
        const double difference = std::abs(static_cast<double>(actual[component]) - static_cast<double>(reference[component]));
        summary.maxAbsoluteError = std::max(summary.maxAbsoluteError, difference);
        summary.maxRelativeError = std::max(summary.maxRelativeError, RelativeError(actual[component], reference[component]));
        summary.maxUlpDistance = std::max(summary.maxUlpDistance, ulp[component]);
        pointSquared += difference * difference;
    }
    squaredAccumulator += pointSquared;
}

void WriteString(base::BinaryWriter& writer, std::string_view value)
{
    writer.WriteU32(static_cast<std::uint32_t>(value.size()));
    writer.WriteBytes(std::as_bytes(std::span(value.data(), value.size())));
}
}

base::Result<ObservationResultV1, std::string> ObserveCaseV1(
    const corpus::QualificationCaseV1& qualificationCase,
    std::span<const semantic::Float4Point> matrixOutput,
    std::span<const semantic::Float4Point> pgaOutput)
{
    const auto count = qualificationCase.referencePoints.size();
    if (matrixOutput.size() != count || pgaOutput.size() != count || qualificationCase.inputPoints.size() != count)
        return base::Result<ObservationResultV1, std::string>::Failure("observation point counts differ");

    ObservationResultV1 result{};
    result.records.reserve(count);
    result.report.scenarioIdentity = qualificationCase.caseIdentity;
    result.report.pointCount = static_cast<std::uint32_t>(count);
    double matrixSquared = 0.0;
    double pgaSquared = 0.0;
    double pairSquared = 0.0;

    for (std::uint32_t index = 0; index < count; ++index)
    {
        const auto& reference = qualificationCase.referencePoints[index];
        const auto& matrix = matrixOutput[index];
        const auto& pga = pgaOutput[index];
        auto record = contracts::BuildComparisonRecordV1(index, reference, matrix, pga);
        if ((record.finiteFlags & 0x7u) != 0x7u) ++result.report.nonFiniteCount;
        if (record.mismatchFlags != 0)
        {
            if (result.report.firstMismatchIndex == 0xffff'ffffu) result.report.firstMismatchIndex = index;
            ++result.report.mismatchCount;
        }
        const std::array<float,3> r{reference.x,reference.y,reference.z};
        const std::array<float,3> m{matrix.x,matrix.y,matrix.z};
        const std::array<float,3> p{pga.x,pga.y,pga.z};
        UpdateSummary(result.report.matrix,m,r,record.matrixUlp,matrixSquared);
        UpdateSummary(result.report.pga,p,r,record.pgaUlp,pgaSquared);
        UpdateSummary(result.report.pair,m,p,record.pairUlp,pairSquared);
        result.records.push_back(record);
    }

    if (count != 0)
    {
        const double divisor = static_cast<double>(count);
        result.report.matrix.rmsEuclideanError = std::sqrt(matrixSquared / divisor);
        result.report.pga.rmsEuclideanError = std::sqrt(pgaSquared / divisor);
        result.report.pair.rmsEuclideanError = std::sqrt(pairSquared / divisor);
    }
    const auto inputBytes = semantic::SerializePoints(qualificationCase.inputPoints);
    const auto referenceBytes = semantic::SerializePoints(qualificationCase.referencePoints);
    const auto matrixBytes = semantic::SerializePoints(matrixOutput);
    const auto pgaBytes = semantic::SerializePoints(pgaOutput);
    const auto readbackBytes = contracts::SerializeComparisonRecordsV1(result.records);
    result.report.inputDigest = base::Sha256(inputBytes);
    result.report.referenceDigest = base::Sha256(referenceBytes);
    result.report.matrixOutputDigest = base::Sha256(matrixBytes);
    result.report.pgaOutputDigest = base::Sha256(pgaBytes);
    result.report.readbackDigest = base::Sha256(readbackBytes);
    return base::Result<ObservationResultV1, std::string>::Success(std::move(result));
}

base::Result<std::vector<std::byte>, std::string> BuildObservationFoundationBundleV1(
    const corpus::QualificationCorpusV1& corpusValue)
{
    base::BinaryWriter writer;
    WriteString(writer, "SGE4-5.CU1.SemanticObservationFoundation.V1");
    const auto observationBytes = contracts::SerializeObservationContractV1(contracts::CanonicalObservationContractV1());
    writer.WriteU32(static_cast<std::uint32_t>(observationBytes.size()));
    writer.WriteBytes(observationBytes);
    const auto corpusBytes = corpus::SerializeCorpusFoundationBundleV1(corpusValue);
    writer.WriteU64(static_cast<std::uint64_t>(corpusBytes.size()));
    writer.WriteBytes(corpusBytes);
    writer.WriteU32(static_cast<std::uint32_t>(corpusValue.cases.size()));
    for (const auto& value : corpusValue.cases)
    {
        auto observed = ObserveCaseV1(value, value.referencePoints, value.referencePoints);
        if (!observed) return base::Result<std::vector<std::byte>, std::string>::Failure(observed.Error());
        const auto recordBytes = contracts::SerializeComparisonRecordsV1(observed.Value().records);
        const auto reportBytes = contracts::SerializeScenarioReportV1(observed.Value().report);
        writer.WriteBytes(value.caseIdentity);
        writer.WriteU64(static_cast<std::uint64_t>(recordBytes.size()));
        writer.WriteBytes(recordBytes);
        writer.WriteU32(static_cast<std::uint32_t>(reportBytes.size()));
        writer.WriteBytes(reportBytes);
    }
    return base::Result<std::vector<std::byte>, std::string>::Success(std::move(writer).Take());
}
}
