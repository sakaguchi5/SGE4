#pragma once

#include "../00_Foundation/Base.h"
#include "../61_Spiral1Contracts/Spiral1Contracts.h"
#include "../62_Spiral1Corpus/Spiral1Corpus.h"

#include <span>
#include <string>
#include <vector>

namespace sge4_5::spiral1::observer
{
struct ObservationResultV1 final
{
    std::vector<contracts::PointComparisonRecordV1> records;
    contracts::ScenarioReportV1 report;
};

[[nodiscard]] base::Result<ObservationResultV1, std::string> ObserveCaseV1(
    const corpus::QualificationCaseV1& qualificationCase,
    std::span<const semantic::Float4Point> matrixOutput,
    std::span<const semantic::Float4Point> pgaOutput);
[[nodiscard]] base::Result<std::vector<std::byte>, std::string> BuildObservationFoundationBundleV1(
    const corpus::QualificationCorpusV1& corpusValue);
}
