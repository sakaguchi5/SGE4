#pragma once

#include "../../../canonical/base/Expected.h"
#include "./PackageFormat.h"

#include <cstddef>
#include <vector>

namespace sge4::package
{
class PackageWriter final
{
public:
    [[nodiscard]] static base::Expected<std::vector<std::byte>, PackageError> Write(PackageBuildInput input);
};
}
