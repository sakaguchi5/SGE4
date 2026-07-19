#pragma once

#include "../00_Foundation/Result.h"
#include "FrozenCompositionFormat.h"

#include <vector>

namespace sge4_5::composition
{
class FrozenCompositionWriter final
{
public:
    [[nodiscard]] static base::Result<std::vector<std::byte>, FrozenCompositionError> Write(
        FrozenCompositionBuildInput input);
};
}
