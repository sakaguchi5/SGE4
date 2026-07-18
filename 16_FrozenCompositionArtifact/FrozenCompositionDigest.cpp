#include "FrozenCompositionDigest.h"

#include "../00_Foundation/BinaryIO.h"
#include "../00_Foundation/Sha256.h"

#include <algorithm>

namespace sge4::composition
{
namespace
{
[[nodiscard]] bool IsSemanticSection(FrozenCompositionSectionKind kind) noexcept
{
    return kind == FrozenCompositionSectionKind::Manifest ||
        kind == FrozenCompositionSectionKind::LeafTable ||
        kind == FrozenCompositionSectionKind::LeafBytes ||
        kind == FrozenCompositionSectionKind::ContractData;
}

template<class Section, class GetKind, class GetBytes>
base::Digest256 Compute(std::span<const Section> sections, GetKind getKind, GetBytes getBytes)
{
    base::BinaryWriter writer;
    for (const auto& section : sections)
    {
        const auto kind = getKind(section);
        if (!IsSemanticSection(kind)) continue;
        const auto bytes = getBytes(section);
        writer.WriteU32(static_cast<std::uint32_t>(kind));
        writer.WriteU64(bytes.size());
        writer.WriteBytes(bytes);
    }
    return base::Sha256(writer.Bytes());
}
}

base::Digest256 ComputeSemanticDigest(std::span<const FrozenCompositionSectionInput> sections)
{
    return Compute<FrozenCompositionSectionInput>(sections,
        [](const auto& section) { return section.kind; },
        [](const auto& section) { return std::span<const std::byte>(section.bytes); });
}

base::Digest256 ComputeSemanticDigestFromViews(std::span<const FrozenCompositionSectionView> sections)
{
    return Compute<FrozenCompositionSectionView>(sections,
        [](const auto& section) { return section.descriptor.kind; },
        [](const auto& section) { return section.bytes; });
}

base::Digest256 ComputeFrozenCompositionFileDigest(std::span<const std::byte> bytes)
{
    std::vector<std::byte> copy(bytes.begin(), bytes.end());
    if (copy.size() >= FrozenCompositionFileDigestOffset + 32)
    {
        std::fill(
            copy.begin() + static_cast<std::ptrdiff_t>(FrozenCompositionFileDigestOffset),
            copy.begin() + static_cast<std::ptrdiff_t>(FrozenCompositionFileDigestOffset + 32),
            std::byte{0});
    }
    return base::Sha256(copy);
}
}
