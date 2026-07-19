#pragma once

#include "../00_Foundation/Base.h"
#include "../80_HierarchySemantic/HierarchySemantic.h"
#include "../81_Spiral2Contracts/Spiral2Contracts.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace sge4_5::spiral2::corpus
{
struct ProbePointV1 final { double x=0.0, y=0.0, z=0.0, w=1.0; };
struct ObservedPointV1 final { float x=0.0f, y=0.0f, z=0.0f, w=1.0f; };
static_assert(sizeof(ObservedPointV1) == 16);

struct TopologyCaseV1 final
{
    std::string id;
    hierarchy::RigidHierarchySemanticV1 hierarchy;
};

struct FrameCaseV1 final
{
    std::string id;
    contracts::DynamicLocalMotorPaletteV1 palette;
};

[[nodiscard]] const std::array<ProbePointV1, 5>& CanonicalProbesV1();
[[nodiscard]] base::Result<std::vector<TopologyCaseV1>, std::string> BuildTopologyCorpusV1();
[[nodiscard]] base::Result<std::vector<FrameCaseV1>, std::string> BuildFrameCorpusV1(
    const hierarchy::RigidHierarchySemanticV1& hierarchy);
[[nodiscard]] base::Result<std::vector<ObservedPointV1>, std::string> BuildIndependentCpuReferenceV1(
    const hierarchy::RigidHierarchySemanticV1& hierarchy,
    const contracts::DynamicLocalMotorPaletteV1& palette);
[[nodiscard]] std::vector<std::byte> SerializeReferencePointsV1(
    const std::vector<ObservedPointV1>& points);
[[nodiscard]] base::Result<std::vector<std::byte>, std::string> BuildCorpusFoundationBundleV1();
}
