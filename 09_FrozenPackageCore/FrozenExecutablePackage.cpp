#include "FrozenExecutablePackage.h"

namespace sge4_5::package
{
const SectionView* FrozenExecutablePackage::FindSection(SectionKind kind) const noexcept
{
    for (const auto& section : sections_)
        if (section.descriptor.sectionKind == kind) return &section;
    return nullptr;
}
}
