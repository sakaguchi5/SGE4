#pragma once

#include "../../../../canonical/base/Expected.h"
#include "./FrozenCompositionFormat.h"

#include <vector>

namespace sge4::composition
{
class FrozenCompositionWriter final
{
public:
    [[nodiscard]] static base::Expected<std::vector<std::byte>, FrozenCompositionError> Write(
        FrozenCompositionBuildInput input);
};
}
