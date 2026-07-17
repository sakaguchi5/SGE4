#pragma once

#include "D3D12CompositionSchema18.h"

namespace sge4::package::d3d12_v18
{
[[nodiscard]] base::Result<std::vector<std::byte>, PackageError> BuildCompositionLeafPackage(
    const FrozenExecutablePackage& schema17Package,
    std::span<const LeafCompositionEndpointArtifact> endpoints);
}
