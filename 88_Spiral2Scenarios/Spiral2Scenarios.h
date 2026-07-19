#pragma once
#include "../80_HierarchySemantic/HierarchySemantic.h"
#include "../81_Spiral2Contracts/Spiral2Contracts.h"
#include "../82_Spiral2Corpus/Spiral2Corpus.h"
#include "../86_Spiral2LeafCompiler/Spiral2LeafCompiler.h"
#include "../87_Spiral2Observer/Spiral2Observer.h"
#include "../14_D3D12Backend/D3D12Backend.h"
#include "../22_CompositionRuntime/CompositionRuntime.h"
#include <string>
#include <span>
#include <vector>

namespace sge4_5::spiral2::scenarios
{
struct ScenarioErrorV1 final{std::string stage;std::string message;};
struct FrozenHierarchyComparisonScenarioV1 final
{
    base::Digest256 hierarchyIdentity{};
    std::uint32_t boneCount=0;
    std::vector<leaf::FrozenHierarchyLeafV1> leaves;
    std::vector<std::byte> frozenCompositionBytes;
    base::Digest256 frozenCompositionDigest{};
};
struct HierarchyComparisonExecutionResultV1 final
{
    std::vector<corpus::ObservedPointV1> matrix;
    std::vector<corpus::ObservedPointV1> direct;
    std::vector<corpus::ObservedPointV1> hybrid;
    observer::ObservationReportV1 observation;
    std::vector<std::byte> gpuRecords;
    std::uint64_t deviceEpoch=0;
};
struct HierarchyCorpusExecutionResultV1 final
{
    std::vector<HierarchyComparisonExecutionResultV1> frames;
    base::Digest256 frozenDigestBefore{};
    base::Digest256 frozenDigestAfter{};
};
[[nodiscard]] base::Result<FrozenHierarchyComparisonScenarioV1,ScenarioErrorV1> BuildFrozenHierarchyComparisonScenarioV1(const hierarchy::RigidHierarchySemanticV1& hierarchy);
[[nodiscard]] base::Result<void,ScenarioErrorV1> ValidateFrozenHierarchyComparisonScenarioV1(const FrozenHierarchyComparisonScenarioV1& scenario);
[[nodiscard]] base::Result<HierarchyComparisonExecutionResultV1,ScenarioErrorV1> ExecuteFrozenHierarchyComparisonScenarioV1(
    const FrozenHierarchyComparisonScenarioV1& scenario,
    const hierarchy::RigidHierarchySemanticV1& hierarchy,
    const contracts::DynamicLocalMotorPaletteV1& palette,
    d3d12::D3D12Backend& backend,
    std::uint64_t frameNumber);
[[nodiscard]] base::Result<HierarchyComparisonExecutionResultV1,ScenarioErrorV1> ExecuteLoadedHierarchyComparisonScenarioV1(
    const FrozenHierarchyComparisonScenarioV1& scenario,
    const hierarchy::RigidHierarchySemanticV1& hierarchy,
    const contracts::DynamicLocalMotorPaletteV1& palette,
    composition::canonical::runtime_v1::LoadedStaticComposition& loaded,
    std::uint64_t frameNumber);
[[nodiscard]] base::Result<HierarchyCorpusExecutionResultV1,ScenarioErrorV1> ExecuteFrozenHierarchyCorpusScenarioV1(
    const FrozenHierarchyComparisonScenarioV1& scenario,
    const hierarchy::RigidHierarchySemanticV1& hierarchy,
    std::span<const corpus::FrameCaseV1> frames,
    d3d12::D3D12Backend& backend,
    std::uint64_t firstFrameNumber=0);
[[nodiscard]] std::vector<std::byte> SerializeFrozenHierarchyComparisonScenarioEvidenceV1(const FrozenHierarchyComparisonScenarioV1& scenario);
}
