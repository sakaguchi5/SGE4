#pragma once
#include "../00_Foundation/Base.h"
#include "../80_HierarchySemantic/HierarchySemantic.h"
#include "../81_Spiral2Contracts/Spiral2Contracts.h"
#include <cstdint>
#include <string>
#include <vector>

namespace sge4_5::spiral2::candidate
{
enum class CandidateKindV1 : std::uint32_t { MatrixHierarchy=1, DirectPgaHierarchy=2, HybridHierarchy=3 };
enum class OperationKindV1 : std::uint32_t { DynamicInvocationSource=1,LocalMotorToMatrix=2,MatrixHierarchy=3,MatrixProbeApply=4,MotorHierarchy=5,DirectPgaProbeApply=6,GlobalMotorToMatrix=7 };
enum class MatrixLoweringPositionV1 : std::uint32_t { BeforeHierarchy=1,Absent=2,AfterHierarchy=3 };
struct CandidateNodeV1 final { std::uint32_t ordinal=0;OperationKindV1 operation{};std::uint32_t inputStride=0;std::uint32_t outputStride=0;base::Digest256 programIdentity{}; };
struct CandidateEdgeV1 final { std::uint32_t source=0,destination=0; };
struct RawCandidateGraphV1 final
{
    base::Digest256 hierarchySemanticIdentity{};
    base::Digest256 dynamicInvocationSchemaIdentity{};
    base::Digest256 observationContractIdentity{};
    CandidateKindV1 kind{};
    std::uint32_t boneCount=0;
    std::vector<CandidateNodeV1> nodes;
    std::vector<CandidateEdgeV1> edges;
    MatrixLoweringPositionV1 proposedLoweringPosition{};
    std::uint32_t proposedHierarchyDepthCount=0;
    std::uint32_t proposedDispatchCount=0;
};
[[nodiscard]] base::Digest256 ProgramIdentityV1(OperationKindV1 operation);
[[nodiscard]] base::Result<RawCandidateGraphV1,std::string> BuildRawCandidateGraphV1(const hierarchy::RigidHierarchySemanticV1& hierarchy,CandidateKindV1 kind);
[[nodiscard]] std::vector<std::byte> SerializeRawCandidateGraphV1(const RawCandidateGraphV1& graph);
[[nodiscard]] base::Digest256 RawCandidateGraphIdentityV1(const RawCandidateGraphV1& graph);
}
