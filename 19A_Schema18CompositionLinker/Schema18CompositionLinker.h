#pragma once

#include "../00_Foundation/Result.h"
#include "../00_Foundation/Sha256.h"
#include "../16A_Schema18CompositionContract/Schema18CompositionContract.h"
#include "../17A_Schema18ResourceFlowPlan/Schema18ResourceFlowPlan.h"
#include "../18A_Schema18ResourceFlowVerifier/Schema18ResourceFlowVerifier.h"

#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace sge4::composition::schema18::linking
{
struct LinkError final
{
    std::string stage;
    std::string message;
};

struct FrozenCompositionArtifact final
{
    PackageCompositionContract contract;
    planning::RawResourceFlowPlan plan;
    base::Digest256 verifierSeal{};
    base::Digest256 artifactDigest{};
    std::vector<std::byte> fileBytes;
};

[[nodiscard]] base::Result<FrozenCompositionArtifact, LinkError>
FreezeVerifiedComposition(
    const PackageCompositionContract& contract,
    const verification::VerifiedResourceFlowPlan& verifiedPlan);

[[nodiscard]] base::Result<FrozenCompositionArtifact, LinkError>
ReadFrozenComposition(std::span<const std::byte> bytes);
}
