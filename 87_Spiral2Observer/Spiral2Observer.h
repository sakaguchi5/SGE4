#pragma once
#include "../82_Spiral2Corpus/Spiral2Corpus.h"
#include <cstdint>
#include <limits>
#include <span>
#include <string>

namespace sge4_5::spiral2::observer
{
struct ErrorMetricSummaryV2 final
{
    double maxAbsoluteError=0;
    double maxRelativeError=0;
    std::uint64_t maxUlpDistance=0;
    double rmsEuclideanError=0;
};
struct ObservationReportV1 final
{
    std::uint64_t pointCount=0,mismatchCount=0,nonFiniteCount=0;
    std::uint64_t firstMismatchIndex=std::numeric_limits<std::uint64_t>::max();
    ErrorMetricSummaryV2 matrixReference,directReference,hybridReference;
    ErrorMetricSummaryV2 matrixDirect,matrixHybrid,directHybrid;
    // Aggregate compatibility fields.  These are derived from the complete
    // six-way metric ledger above and are not separate facts.
    double maxAbsoluteError=0,maxRelativeError=0,maxPairwiseError=0,maxPairwiseRelativeError=0;
    std::uint64_t maxUlpDistance=0,maxPairwiseUlpDistance=0;
    double maxAxisLengthError=0,maxOrthogonalityError=0,maxTranslationError=0;
    double minimumDeterminant=1.0;
    bool orientationPreserved=true;
};
[[nodiscard]] base::Result<ObservationReportV1,std::string> ObserveHierarchyV1(std::span<const corpus::ObservedPointV1> reference,std::span<const corpus::ObservedPointV1> matrix,std::span<const corpus::ObservedPointV1> direct,std::span<const corpus::ObservedPointV1> hybrid);
[[nodiscard]] std::vector<std::byte> SerializeObservationReportV1(const ObservationReportV1& report);
}
