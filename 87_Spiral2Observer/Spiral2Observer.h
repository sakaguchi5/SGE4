#pragma once
#include "../82_Spiral2Corpus/Spiral2Corpus.h"
#include <cstdint>
#include <span>
#include <string>

namespace sge4_5::spiral2::observer
{
struct ObservationReportV1 final{std::uint64_t pointCount=0,mismatchCount=0,nonFiniteCount=0;double maxAbsoluteError=0,maxPairwiseError=0,maxAxisLengthError=0,maxOrthogonalityError=0;bool orientationPreserved=true;};
[[nodiscard]] base::Result<ObservationReportV1,std::string> ObserveHierarchyV1(std::span<const corpus::ObservedPointV1> reference,std::span<const corpus::ObservedPointV1> matrix,std::span<const corpus::ObservedPointV1> direct,std::span<const corpus::ObservedPointV1> hybrid);
[[nodiscard]] std::vector<std::byte> SerializeObservationReportV1(const ObservationReportV1& report);
}
