#pragma once

#include "FrozenComposition.h"

#include <span>
#include <vector>

namespace sge4::composition
{
struct FrozenCompositionSectionInput final
{
    FrozenCompositionSectionKind kind{};
    std::uint16_t schemaVersion = 1;
    std::uint32_t flags = RequiredExecutionFlags();
    std::uint32_t alignment = SectionAlignment;
    std::uint32_t elementCount = 0;
    std::uint32_t elementStride = 0;
    std::vector<std::byte> bytes;
};

[[nodiscard]] base::Digest256 ComputeSemanticDigest(std::span<const FrozenCompositionSectionInput> sections);
[[nodiscard]] base::Digest256 ComputeSemanticDigestFromViews(std::span<const FrozenCompositionSectionView> sections);
[[nodiscard]] base::Digest256 ComputeFrozenCompositionFileDigest(std::span<const std::byte> bytes);
}
