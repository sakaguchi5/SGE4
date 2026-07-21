#pragma once
#include "../00_Foundation/Base.h"
#include "../80_HierarchySemantic/HierarchySemantic.h"
#include "../102_Spiral3Corpus/Spiral3Corpus.h"
#include "../106_Spiral3LeafCompiler/Spiral3LeafCompiler.h"
#include "../17_CompositionContract/CompositionContract.h"
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace sge4_5::spiral3::scenarios
{
inline constexpr std::string_view MotorsFlowKeyV1="spiral3/flow/motors";
inline constexpr std::string_view ReferenceFlowKeyV1="spiral3/flow/reference";
inline constexpr std::string_view PointsFlowKeyV1="spiral3/flow/points";
inline constexpr std::string_view ALocalMatrixFlowKeyV1="spiral3/flow/a/local-matrix";
inline constexpr std::string_view AGlobalMatrixFlowKeyV1="spiral3/flow/a/global-matrix";
inline constexpr std::string_view AOutputFlowKeyV1="spiral3/flow/a/output";
inline constexpr std::string_view BGlobalMotorFlowKeyV1="spiral3/flow/b/global-motor";
inline constexpr std::string_view BOutputFlowKeyV1="spiral3/flow/b/output";
inline constexpr std::string_view CGlobalMotorFlowKeyV1="spiral3/flow/c/global-motor";
inline constexpr std::string_view CGlobalMatrixFlowKeyV1="spiral3/flow/c/global-matrix";
inline constexpr std::string_view COutputFlowKeyV1="spiral3/flow/c/output";
inline constexpr std::string_view RecordsFlowKeyV1="spiral3/flow/records";
inline constexpr std::string_view ObserverLeafRoleV1="L10.Observer";

struct ScenarioErrorV1 final{std::string stage;std::string message;};
struct FrozenReuseComparisonScenarioV1 final
{
    base::Digest256 hierarchyIdentity{};
    base::Digest256 reuseSemanticIdentity{};
    base::Digest256 localPointCorpusIdentity{};
    std::uint32_t reuseCount=0;
    std::uint32_t pointCount=0;
    std::vector<leaf::FrozenReuseLeafV1> leaves;
    std::vector<std::byte> frozenCompositionBytes;
    base::Digest256 frozenCompositionDigest{};
    base::Digest256 scenarioIdentity{};
};

[[nodiscard]] base::Result<FrozenReuseComparisonScenarioV1,ScenarioErrorV1>
BuildFrozenReuseComparisonScenarioV1(
    const spiral2::hierarchy::RigidHierarchySemanticV1& hierarchy,
    const corpus::ReuseCaseV1& reuseCase);
[[nodiscard]] base::Result<void,ScenarioErrorV1>
ValidateFrozenReuseComparisonScenarioV1(
    const spiral2::hierarchy::RigidHierarchySemanticV1& hierarchy,
    const corpus::ReuseCaseV1& reuseCase,
    const FrozenReuseComparisonScenarioV1& scenario);
[[nodiscard]] composition::canonical::LeafPackageId FindLeafByAuthorKeyV1(
    const composition::canonical::PackageCompositionContract& contract,
    std::string_view authorKey) noexcept;
[[nodiscard]] composition::canonical::ResourceFlowId FindFlowByAuthorKeyV1(
    const composition::canonical::PackageCompositionContract& contract,
    std::string_view authorKey) noexcept;
[[nodiscard]] base::Digest256 ComputeFrozenReuseComparisonScenarioIdentityV1(
    const FrozenReuseComparisonScenarioV1& scenario);
[[nodiscard]] std::vector<std::byte> SerializeFrozenReuseComparisonScenarioEvidenceV1(
    const FrozenReuseComparisonScenarioV1& scenario);
}
