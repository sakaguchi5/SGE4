#pragma once

#include "../00_Foundation/Result.h"
#include "../02_SemanticModel/SemanticModel.h"
#include "../20_ExperimentDomain/ExperimentDomain.h"

#include <string>

namespace sge4_5::classical
{
[[nodiscard]] experiment::TriangleGeometry BuildTriangleGeometry() noexcept;
[[nodiscard]] base::Result<semantic::SemanticGraph, std::string> BuildTriangleGraph();
}
