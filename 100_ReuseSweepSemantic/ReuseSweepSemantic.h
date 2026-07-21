#pragma once
#include "../00_Foundation/Base.h"
#include "../80_HierarchySemantic/HierarchySemantic.h"
#include "../101_Spiral3Contracts/Spiral3Contracts.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace sge4_5::spiral3::semantic
{
inline constexpr std::uint32_t ReuseSweepSemanticVersionV1 = 1;
struct ReuseSweepSemanticV1 final
{
    base::Digest256 hierarchySemanticIdentity{};
    base::Digest256 consumerMeaningIdentity{};
    base::Digest256 localPointSchemaIdentity{};
    base::Digest256 localPointCorpusIdentity{};
    base::Digest256 observationContractIdentity{};
    std::uint32_t boneCount = 0;
    std::uint32_t reuseCount = 0;
    std::uint32_t pointCount = 0;
};

[[nodiscard]] base::Digest256 ConsumerMeaningIdentityV1();
[[nodiscard]] base::Digest256 LocalPointSchemaIdentityV1();
[[nodiscard]] base::Digest256 ObservationContractIdentityV1();
[[nodiscard]] base::Result<ReuseSweepSemanticV1,std::string> BuildReuseSweepSemanticV1(
    const spiral2::hierarchy::RigidHierarchySemanticV1& hierarchy,
    contracts::ReuseCountV1 reuseCount,
    const base::Digest256& localPointCorpusIdentity);
[[nodiscard]] base::Result<void,std::string> ValidateReuseSweepSemanticV1(
    const spiral2::hierarchy::RigidHierarchySemanticV1& hierarchy,
    const ReuseSweepSemanticV1& value);
[[nodiscard]] std::vector<std::byte> SerializeReuseSweepSemanticV1(const ReuseSweepSemanticV1& value);
[[nodiscard]] base::Digest256 ReuseSweepSemanticIdentityV1(const ReuseSweepSemanticV1& value);
}
