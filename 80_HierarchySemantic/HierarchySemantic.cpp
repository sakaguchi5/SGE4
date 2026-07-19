#include "HierarchySemantic.h"

#include <algorithm>
#include <numeric>
#include <string_view>

namespace sge4_5::spiral2::hierarchy
{
namespace
{
[[nodiscard]] base::Digest256 Literal(std::string_view value)
{
    return base::Sha256(std::as_bytes(std::span(value.data(), value.size())));
}
}

base::Digest256 LocalMotorSchemaIdentityV1() { return Literal("SGE4-5.Spiral2.LocalMotorPalette.V1|binary32|qr-wxyz|qd-wxyz|stride=32"); }
base::Digest256 GlobalTransformMeaningIdentityV1() { return Literal("SGE4-5.Spiral2.GlobalRigidTransform.V1|global[parent]*local[bone]"); }
base::Digest256 ProbeSchemaIdentityV1() { return Literal("SGE4-5.Spiral2.Probes.V1|P0-origin|P1-X|P2-Y|P3-Z|P4-asymmetric"); }
base::Digest256 ObservationContractIdentityV1() { return Literal("SGE4-5.Spiral2.HierarchyObservation.V1|bone-major|probe-major|float4|abs=2e-4|rel=2e-5|pair-abs=1.5e-4|pair-rel=2e-5|rigid=5e-4"); }

base::Result<RigidHierarchySemanticV1, std::string> BuildRigidHierarchySemanticV1(
    std::span<const std::int32_t> parents)
{
    if (parents.empty()) return base::Result<RigidHierarchySemanticV1, std::string>::Failure("bone count is zero");
    if (parents.size() > MaximumBoneCountV1) return base::Result<RigidHierarchySemanticV1, std::string>::Failure("bone count exceeds V1 maximum");
    const auto count = static_cast<std::uint32_t>(parents.size());
    std::uint32_t root = 0;
    std::uint32_t roots = 0;
    for (std::uint32_t bone = 0; bone < count; ++bone)
    {
        const auto parent = parents[bone];
        if (parent == -1) { root = bone; ++roots; continue; }
        if (parent < 0 || static_cast<std::uint32_t>(parent) >= count)
            return base::Result<RigidHierarchySemanticV1, std::string>::Failure("parent index is outside the hierarchy");
        if (static_cast<std::uint32_t>(parent) == bone)
            return base::Result<RigidHierarchySemanticV1, std::string>::Failure("bone is its own parent");
    }
    if (roots != 1) return base::Result<RigidHierarchySemanticV1, std::string>::Failure("hierarchy must have exactly one root");

    std::vector<std::uint32_t> depth(count, 0);
    for (std::uint32_t bone = 0; bone < count; ++bone)
    {
        std::vector<bool> seen(count, false);
        std::uint32_t cursor = bone;
        std::uint32_t value = 0;
        while (parents[cursor] != -1)
        {
            if (seen[cursor]) return base::Result<RigidHierarchySemanticV1, std::string>::Failure("hierarchy contains a cycle");
            seen[cursor] = true;
            cursor = static_cast<std::uint32_t>(parents[cursor]);
            if (++value >= count) return base::Result<RigidHierarchySemanticV1, std::string>::Failure("hierarchy contains a cycle");
        }
        if (cursor != root) return base::Result<RigidHierarchySemanticV1, std::string>::Failure("bone is disconnected from the root");
        depth[bone] = value;
    }

    RigidHierarchySemanticV1 result;
    result.boneCount = count;
    result.rootBone = root;
    result.parentIndices.assign(parents.begin(), parents.end());
    result.canonicalBoneOrder.resize(count);
    std::iota(result.canonicalBoneOrder.begin(), result.canonicalBoneOrder.end(), 0u);
    std::ranges::sort(result.canonicalBoneOrder, [&](std::uint32_t a, std::uint32_t b) {
        return depth[a] == depth[b] ? a < b : depth[a] < depth[b];
    });
    const auto maximumDepth = *std::ranges::max_element(depth);
    result.depthOffsets.assign(static_cast<std::size_t>(maximumDepth) + 2u, 0u);
    for (const auto value : depth) ++result.depthOffsets[value + 1u];
    for (std::size_t index = 1; index < result.depthOffsets.size(); ++index)
        result.depthOffsets[index] += result.depthOffsets[index - 1];
    result.depthBoneIndices = result.canonicalBoneOrder;
    result.localMotorSchemaIdentity = LocalMotorSchemaIdentityV1();
    result.globalTransformMeaningIdentity = GlobalTransformMeaningIdentityV1();
    result.probeSchemaIdentity = ProbeSchemaIdentityV1();
    result.observationContractIdentity = ObservationContractIdentityV1();
    return base::Result<RigidHierarchySemanticV1, std::string>::Success(std::move(result));
}

base::Result<void, std::string> ValidateRigidHierarchySemanticV1(const RigidHierarchySemanticV1& value)
{
    auto rebuilt = BuildRigidHierarchySemanticV1(value.parentIndices);
    if (!rebuilt) return base::Result<void, std::string>::Failure(rebuilt.Error());
    const auto& expected = rebuilt.Value();
    if (value.boneCount != expected.boneCount || value.rootBone != expected.rootBone)
        return base::Result<void, std::string>::Failure("hierarchy header differs from canonical derivation");
    if (value.canonicalBoneOrder != expected.canonicalBoneOrder)
        return base::Result<void, std::string>::Failure("bone order is not canonical");
    if (value.depthOffsets != expected.depthOffsets || value.depthBoneIndices != expected.depthBoneIndices)
        return base::Result<void, std::string>::Failure("depth table differs from canonical derivation");
    if (value.localMotorSchemaIdentity != expected.localMotorSchemaIdentity ||
        value.globalTransformMeaningIdentity != expected.globalTransformMeaningIdentity ||
        value.probeSchemaIdentity != expected.probeSchemaIdentity ||
        value.observationContractIdentity != expected.observationContractIdentity)
        return base::Result<void, std::string>::Failure("hierarchy semantic identity field mismatch");
    return base::Result<void, std::string>::Success();
}

std::vector<std::byte> SerializeRigidHierarchySemanticV1(const RigidHierarchySemanticV1& value)
{
    base::BinaryWriter writer;
    writer.WriteBytes(Literal("SGE4-5.Spiral2.RigidHierarchySemantic.V1"));
    writer.WriteU32(HierarchySemanticVersionV1);
    writer.WriteU32(value.boneCount);
    writer.WriteU32(value.rootBone);
    writer.WriteU32(static_cast<std::uint32_t>(value.depthOffsets.size() - 1u));
    for (const auto parent : value.parentIndices) writer.WriteU32(static_cast<std::uint32_t>(parent));
    for (const auto bone : value.canonicalBoneOrder) writer.WriteU32(bone);
    for (const auto offset : value.depthOffsets) writer.WriteU32(offset);
    for (const auto bone : value.depthBoneIndices) writer.WriteU32(bone);
    writer.WriteBytes(value.localMotorSchemaIdentity);
    writer.WriteBytes(value.globalTransformMeaningIdentity);
    writer.WriteBytes(value.probeSchemaIdentity);
    writer.WriteBytes(value.observationContractIdentity);
    return std::move(writer).Take();
}

base::Digest256 RigidHierarchySemanticIdentityV1(const RigidHierarchySemanticV1& value)
{
    return base::Sha256(SerializeRigidHierarchySemanticV1(value));
}
}
