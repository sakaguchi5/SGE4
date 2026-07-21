#pragma once
#include "../00_Foundation/Base.h"
#include "../80_HierarchySemantic/HierarchySemantic.h"
#include "../81_Spiral2Contracts/Spiral2Contracts.h"
#include "../82_Spiral2Corpus/Spiral2Corpus.h"
#include "../100_ReuseSweepSemantic/ReuseSweepSemantic.h"
#include "../101_Spiral3Contracts/Spiral3Contracts.h"
#include <cstdint>
#include <string>
#include <vector>

namespace sge4_5::spiral3::corpus
{
using LocalPointV1=spiral2::corpus::ObservedPointV1;
using ObservedPointV1=spiral2::corpus::ObservedPointV1;
struct ReuseCaseV1 final
{
    std::string id;
    std::uint32_t reuseCount=0;
    std::vector<LocalPointV1> localPoints;
    semantic::ReuseSweepSemanticV1 semantic;
};
[[nodiscard]] base::Result<spiral2::hierarchy::RigidHierarchySemanticV1,std::string> BuildCanonicalMeasurementHierarchyV1();
[[nodiscard]] base::Result<std::vector<LocalPointV1>,std::string> BuildLocalPointCorpusV1(contracts::BoneCountV1 boneCount,contracts::ReuseCountV1 reuseCount);
[[nodiscard]] std::vector<std::byte> SerializePointsV1(std::span<const LocalPointV1> points);
[[nodiscard]] base::Digest256 LocalPointCorpusIdentityV1(std::span<const LocalPointV1> points);
[[nodiscard]] base::Result<std::vector<ReuseCaseV1>,std::string> BuildReuseCorpusV1(const spiral2::hierarchy::RigidHierarchySemanticV1& hierarchy);
[[nodiscard]] base::Result<std::vector<ObservedPointV1>,std::string> BuildIndependentCpuReferenceV1(
    const spiral2::hierarchy::RigidHierarchySemanticV1& hierarchy,
    const spiral2::contracts::DynamicLocalMotorPaletteV1& palette,
    std::span<const LocalPointV1> localPoints,
    contracts::ReuseCountV1 reuseCount);
[[nodiscard]] base::Result<std::vector<std::byte>,std::string> BuildCorpusFoundationBundleV1();
}
