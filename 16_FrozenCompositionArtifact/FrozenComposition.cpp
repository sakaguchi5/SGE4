#include "FrozenComposition.h"

namespace sge4::composition
{
const FrozenCompositionSectionView* FrozenComposition::FindSection(FrozenCompositionSectionKind kind) const noexcept
{
    for (const auto& section : sections_)
        if (section.descriptor.kind == kind) return &section;
    return nullptr;
}
}
