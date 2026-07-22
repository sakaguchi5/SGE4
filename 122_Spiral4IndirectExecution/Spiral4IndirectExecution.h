#pragma once

#include "../00_Foundation/Result.h"
#include "../00_Foundation/Sha256.h"
#include "../120_ActiveWorkSemantic/ActiveWorkSemantic.h"
#include "../121_Spiral4Contracts/Spiral4Contracts.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

struct ID3D12Device;

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
    std::uint32_t nonFiniteCount = 0;
    std::uint32_t homogeneousCoordinateMismatchCount = 0;
    std::uint32_t duplicateWorkCount = 0;
    std::uint32_t missingWorkCount = 0;
    std::uint32_t reorderedWorkCount = 0;
    std::uint32_t outOfRangeWorkCount = 0;
    float maxAbsoluteError = 0.0f;
    float maxRelativeError = 0.0f;
    std::uint32_t maxUlpError = 0;
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
RunSingleIndirectArchitectureOnDeviceV1(
    ID3D12Device* device,
    std::string_view adapterDescription,
    const semantic::ActiveWorkSemanticV1& semantic,
    const contract::FrozenSingleIndirectArtifactV1& artifact,
    std::span<const std::uint32_t> activeCounts);

[[nodiscard]] base::Result<SingleIndirectArchitectureResultV1, IndirectExecutionErrorV1>
RunSingleIndirectArchitectureOnWarpV1(
    const semantic::ActiveWorkSemanticV1& semantic,
    const contract::FrozenSingleIndirectArtifactV1& artifact,
    std::span<const std::uint32_t> activeCounts);
}
