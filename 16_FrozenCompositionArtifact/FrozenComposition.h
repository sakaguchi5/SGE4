#pragma once

#include "FrozenCompositionFormat.h"

#include <memory>
#include <span>
#include <vector>

namespace sge4_5::composition
{
struct FrozenCompositionSectionView final
{
    FrozenCompositionSectionDescriptor descriptor;
    std::span<const std::byte> bytes;
};

struct EmbeddedLeafView final
{
    EmbeddedLeafRecord record;
    std::span<const std::byte> packageBytes;
};

class FrozenComposition final
{
public:
    [[nodiscard]] const FrozenCompositionHeader& Header() const noexcept { return header_; }
    [[nodiscard]] const FrozenCompositionManifest& Manifest() const noexcept { return manifest_; }
    [[nodiscard]] std::span<const FrozenCompositionSectionView> Sections() const noexcept { return sections_; }
    [[nodiscard]] std::span<const EmbeddedLeafView> Leaves() const noexcept { return leaves_; }
    [[nodiscard]] std::span<const std::byte> ContractBytes() const noexcept { return contractBytes_; }
    [[nodiscard]] std::span<const std::byte> VerifiedDecisionBytes() const noexcept { return verifiedDecisionBytes_; }
    [[nodiscard]] std::span<const std::byte> VerificationCertificateBytes() const noexcept { return certificateBytes_; }
    [[nodiscard]] std::span<const std::byte> FileBytes() const noexcept { return *storage_; }
    [[nodiscard]] const FrozenCompositionSectionView* FindSection(FrozenCompositionSectionKind kind) const noexcept;

private:
    friend class FrozenCompositionReader;
    std::shared_ptr<const std::vector<std::byte>> storage_;
    FrozenCompositionHeader header_;
    FrozenCompositionManifest manifest_;
    std::vector<FrozenCompositionSectionView> sections_;
    std::vector<EmbeddedLeafView> leaves_;
    std::span<const std::byte> contractBytes_;
    std::span<const std::byte> verifiedDecisionBytes_;
    std::span<const std::byte> certificateBytes_;
};
}
