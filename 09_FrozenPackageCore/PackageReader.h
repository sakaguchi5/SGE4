#pragma once

#include "../00_Foundation/Result.h"
#include "FrozenExecutablePackage.h"

#include <cstddef>
#include <span>
#include <vector>

namespace sge4_5::package
{
class PackageReader final
{
public:
    [[nodiscard]] static base::Result<FrozenExecutablePackage, PackageError> Read(std::span<const std::byte> bytes);
    [[nodiscard]] static base::Result<FrozenExecutablePackage, PackageError> Read(std::vector<std::byte> bytes);
};
}
