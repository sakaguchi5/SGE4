#pragma once

#include "../00_Foundation/Result.h"
#include "../18_LinkPlanVerifier/LinkPlanVerifier.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace sge4::composition
{
enum class CompositionSectionKind : std::uint32_t
{
    Manifest = 1, EmbeddedLeafTable = 2, EmbeddedLeafBytes = 3, SharedResourceTable = 4,
    EndpointBindingTable = 5, LinkTable = 6, PackageSchedule = 7, RecoveryPlan = 8
};

struct CompositionArtifactError final { std::string stage; std::string message; };

class FrozenComposition final
{
public:
    FrozenComposition(FrozenComposition&&) noexcept = default;
    FrozenComposition& operator=(FrozenComposition&&) noexcept = default;
    FrozenComposition(const FrozenComposition&) = delete;
    FrozenComposition& operator=(const FrozenComposition&) = delete;
    [[nodiscard]] const PackageCompositionContract& Contract() const noexcept { return contract_; }
    [[nodiscard]] const VerifiedLinkPlan& VerifiedPlan() const noexcept { return verifiedPlan_; }
    [[nodiscard]] std::span<const std::byte> FileBytes() const noexcept { return *storage_; }
private:
    friend base::Result<FrozenComposition, CompositionArtifactError> ReadFrozenComposition(std::vector<std::byte>);
    FrozenComposition(std::shared_ptr<const std::vector<std::byte>> storage,
                      PackageCompositionContract contract,
                      VerifiedLinkPlan plan)
        : storage_(std::move(storage)), contract_(std::move(contract)), verifiedPlan_(std::move(plan)) {}
    std::shared_ptr<const std::vector<std::byte>> storage_;
    PackageCompositionContract contract_;
    VerifiedLinkPlan verifiedPlan_;
};

[[nodiscard]] base::Result<std::vector<std::byte>, CompositionArtifactError> WriteFrozenComposition(
    const PackageCompositionContract& contract,
    const VerifiedLinkPlan& verifiedPlan);
[[nodiscard]] base::Result<FrozenComposition, CompositionArtifactError> ReadFrozenComposition(std::vector<std::byte> bytes);
[[nodiscard]] base::Result<FrozenComposition, CompositionArtifactError> ReadFrozenComposition(std::span<const std::byte> bytes);
}
