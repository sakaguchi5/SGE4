#pragma once

#include "../00_Foundation/Result.h"
#include "PackageFormat.h"

#include <cstddef>
#include <vector>

namespace sge4_5::package
{
class PackageWriter final
{
public:
    [[nodiscard]] static base::Result<std::vector<std::byte>, PackageError> Write(PackageBuildInput input);
};
}
