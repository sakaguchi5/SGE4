#pragma once

#include "../00_Foundation/Result.h"
#include "../00_Foundation/Sha256.h"
#include "../120_ActiveWorkSemantic/ActiveWorkSemantic.h"
#include "../121_Spiral4Contracts/Spiral4Contracts.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace sge4_5::spiral4::execution
{
struct IndirectExecutionErrorV1 final
{
    std::string stage;
    std::string message;
    long hresult = 0;
};

struct IndirectExecutionCaseResultV1 final
{
    std::uint32_t activeWorkCount = 0;
    std::uint32_t observedDispatchGroupCountX = 0;
    std::uint32_t expectedDispatchGroupCountX = 0;
    std::uint32_t activeMismatchCount = 0;
    std::uint32_t firstActiveMismatch = 0xFFFF'FFFFu;
    bool inactiveTailPreserved = false;
    float maxAbsoluteError = 0.0f;
    base::Digest256 readbackDigest{};
};

struct SingleIndirectArchitectureResultV1 final
{
    std::string adapterDescription;
    base::Digest256 producerShaderBytecodeDigest{};
    base::Digest256 consumerShaderBytecodeDigest{};
    std::vector<IndirectExecutionCaseResultV1> cases;
};

[[nodiscard]] base::Result<SingleIndirectArchitectureResultV1, IndirectExecutionErrorV1>
RunSingleIndirectArchitectureOnWarpV1(
    const semantic::ActiveWorkSemanticV1& semantic,
    const contract::FrozenSingleIndirectArtifactV1& artifact,
    std::span<const std::uint32_t> activeCounts);
}
