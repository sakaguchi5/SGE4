#pragma once

#include "../00_Foundation/Result.h"
#include "FrozenComposition.h"

#include <span>
#include <vector>

namespace sge4_5::composition
{
class FrozenCompositionReader final
{
public:
    [[nodiscard]] static base::Result<FrozenComposition, FrozenCompositionError> Read(
        std::span<const std::byte> bytes);
    [[nodiscard]] static base::Result<FrozenComposition, FrozenCompositionError> Read(
        std::vector<std::byte> bytes);
};
}
