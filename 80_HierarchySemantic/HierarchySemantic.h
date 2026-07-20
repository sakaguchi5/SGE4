#pragma once

#include "../00_Foundation/Base.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace sge4_5::spiral2::hierarchy
{
inline constexpr std::uint32_t HierarchySemanticVersionV1 = 1;
inline constexpr std::uint32_t MaximumBoneCountV1 = 4096;

struct RigidHierarchySemanticV1 final
{
    std::uint32_t boneCount = 0;
    std::uint32_t rootBone = 0;
    std::vector<std::int32_t> parentIndices;
    std::vector<std::uint32_t> canonicalBoneOrder;
    std::vector<std::uint32_t> depthOffsets;
    std::vector<std::uint32_t> depthBoneIndices;
    base::Digest256 localMotorSchemaIdentity{};
    base::Digest256 globalTransformMeaningIdentity{};
    base::Digest256 probeSchemaIdentity{};
    base::Digest256 observationContractIdentity{};
};

[[nodiscard]] base::Digest256 LocalMotorSchemaIdentityV1();
[[nodiscard]] base::Digest256 GlobalTransformMeaningIdentityV1();
[[nodiscard]] base::Digest256 ProbeSchemaIdentityV1();
[[nodiscard]] base::Digest256 ObservationContractIdentityV1();
[[nodiscard]] base::Digest256 ObservationContractIdentityV2();
[[nodiscard]] base::Result<RigidHierarchySemanticV1, std::string> BuildRigidHierarchySemanticV1(
    std::span<const std::int32_t> parentIndices);
[[nodiscard]] base::Result<void, std::string> ValidateRigidHierarchySemanticV1(
    const RigidHierarchySemanticV1& value);
[[nodiscard]] std::vector<std::byte> SerializeRigidHierarchySemanticV1(
    const RigidHierarchySemanticV1& value);
[[nodiscard]] base::Digest256 RigidHierarchySemanticIdentityV1(
    const RigidHierarchySemanticV1& value);
}
