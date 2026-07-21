#include "../100_ReuseSweepSemantic/ReuseSweepSemantic.h"
#include "../101_Spiral3Contracts/Spiral3Contracts.h"
#include "../102_Spiral3Corpus/Spiral3Corpus.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <type_traits>

namespace base = sge4_5::base;
namespace semantic = sge4_5::spiral3::semantic;
namespace contracts = sge4_5::spiral3::contracts;
namespace corpus = sge4_5::spiral3::corpus;

static_assert(!std::is_convertible_v<contracts::BoneCountV1, contracts::ReuseCountV1>);
static_assert(!std::is_convertible_v<contracts::ReuseCountV1, contracts::BoneCountV1>);
static_assert(!std::is_convertible_v<std::uint32_t, contracts::BoneCountV1>);
static_assert(!std::is_convertible_v<std::uint32_t, contracts::ReuseCountV1>);
static_assert(!std::is_same_v<contracts::BoneCountV1, contracts::PointCountV1>);
static_assert(!std::is_same_v<contracts::ReuseCountV1, contracts::PointCountV1>);

int main(int argc, char** argv)
{
    auto hierarchy = corpus::BuildCanonicalMeasurementHierarchyV1();
    if (!hierarchy || hierarchy.Value().boneCount != 8 || hierarchy.Value().depthOffsets.size() != 9)
        return 1;

    auto shape = contracts::BuildFixedReuseWorkloadShapeV1(
        contracts::BoneCountV1{hierarchy.Value().boneCount},
        contracts::ReuseCountV1{512});
    if (!shape || shape.Value().pointCount.Value() != 4096)
        return 2;

    if (contracts::BuildFixedReuseWorkloadShapeV1(
            contracts::BoneCountV1{0}, contracts::ReuseCountV1{1}))
        return 3;
    if (contracts::BuildFixedReuseWorkloadShapeV1(
            contracts::BoneCountV1{8}, contracts::ReuseCountV1{0}))
        return 4;
    if (contracts::BuildFixedReuseWorkloadShapeV1(
            contracts::BoneCountV1{8}, contracts::ReuseCountV1{513}))
        return 5;

    auto cases = corpus::BuildReuseCorpusV1(hierarchy.Value());
    if (!cases || cases.Value().size() != 6)
        return 6;

    for (std::size_t index = 0; index < cases.Value().size(); ++index)
    {
        const auto& reuseCase = cases.Value()[index];
        if (reuseCase.reuseCount != contracts::CanonicalReuseCountsV1[index])
            return 7;
        if (reuseCase.semantic.pointCount != hierarchy.Value().boneCount * reuseCase.reuseCount)
            return 8;
        if (!semantic::ValidateReuseSweepSemanticV1(hierarchy.Value(), reuseCase.semantic))
            return 9;
        if (corpus::LocalPointCorpusIdentityV1(reuseCase.localPoints) !=
            reuseCase.semantic.localPointCorpusIdentity)
            return 10;
        if (index && semantic::ReuseSweepSemanticIdentityV1(cases.Value()[index - 1].semantic) ==
                         semantic::ReuseSweepSemanticIdentityV1(reuseCase.semantic))
            return 11;
    }

    auto points = corpus::BuildLocalPointCorpusV1(
        contracts::BoneCountV1{hierarchy.Value().boneCount}, contracts::ReuseCountV1{512});
    if (!points || points.Value()[0].x != 0.0f || points.Value()[1].x != 1.0f ||
        points.Value()[2].y != 1.0f || points.Value()[3].z != 1.0f)
        return 12;

    if (corpus::BuildLocalPointCorpusV1(
            contracts::BoneCountV1{hierarchy.Value().boneCount}, contracts::ReuseCountV1{0}))
        return 13;

    const auto corpusIdentity = corpus::LocalPointCorpusIdentityV1(points.Value());
    if (semantic::BuildReuseSweepSemanticV1(
            hierarchy.Value(), contracts::ReuseCountV1{0}, corpusIdentity))
        return 14;
    if (semantic::BuildReuseSweepSemanticV1(
            hierarchy.Value(), contracts::ReuseCountV1{513}, corpusIdentity))
        return 15;

    auto evidence = corpus::BuildCorpusFoundationBundleV1();
    if (!evidence)
        return 16;

    if (argc == 3 && std::string(argv[1]) == "--emit")
    {
        auto write = base::WriteAllBytes(std::filesystem::path(argv[2]), evidence.Value());
        if (!write)
            return 17;
        std::cout << base::ToHex(base::Sha256(evidence.Value())) << '\n';
        return 0;
    }

    std::cout << "Spiral 3 fixed reuse semantic passed: R1/R4/R16/R64/R256/R512, "
                 "8-bone chain; strong Bone/Reuse/Point count boundaries passed. SHA-256: "
              << base::ToHex(base::Sha256(evidence.Value())) << '\n';
    return 0;
}
