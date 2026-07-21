#pragma once
#include "../00_Foundation/Base.h"
#include "../80_HierarchySemantic/HierarchySemantic.h"
#include "../100_ReuseSweepSemantic/ReuseSweepSemantic.h"
#include <cstdint>
#include <string>
#include <vector>
namespace sge4_5::spiral3::candidate
{
enum class CandidateKindV1:std::uint32_t{MatrixHierarchy=1,DirectPgaHierarchy=2,HybridHierarchy=3};
enum class OperationKindV1:std::uint32_t{DynamicInvocationSource=1,ConsumerPointSource=2,LocalMotorToMatrix=3,MatrixHierarchy=4,MatrixConsumerApply=5,MotorHierarchy=6,DirectPgaConsumerApply=7,GlobalMotorToMatrix=8};
enum class MatrixLoweringPositionV1:std::uint32_t{BeforeHierarchy=1,Absent=2,AfterHierarchy=3};
enum class HierarchyExecutionModelV1:std::uint32_t{SingleDispatchCanonicalOrderSerial=1};
struct CandidateNodeV1 final{std::uint32_t ordinal=0;OperationKindV1 operation{};std::uint32_t primaryInputStride=0,primaryInputElementCount=0,primaryOutputStride=0,primaryOutputElementCount=0;std::uint32_t secondaryInputStride=0,secondaryInputElementCount=0,secondaryOutputStride=0,secondaryOutputElementCount=0;std::uint32_t dispatchX=0,dispatchY=1,dispatchZ=1;base::Digest256 programIdentity{};};
struct CandidateEdgeV1 final{std::uint32_t source=0,destination=0;};
struct RawCandidateGraphV1 final{base::Digest256 reuseSemanticIdentity{},hierarchySemanticIdentity{},dynamicInvocationSchemaIdentity{},localPointCorpusIdentity{},observationContractIdentity{};CandidateKindV1 kind{};std::uint32_t boneCount=0,reuseCount=0,pointCount=0;std::vector<CandidateNodeV1> nodes;std::vector<CandidateEdgeV1> edges;MatrixLoweringPositionV1 proposedLoweringPosition{};HierarchyExecutionModelV1 proposedHierarchyExecutionModel{};std::uint32_t hierarchyDepthCount=0,commonSourceDispatchCount=0,proposedCandidateLeafDispatchCount=0;};
[[nodiscard]] base::Digest256 ProgramIdentityV1(OperationKindV1 operation);
[[nodiscard]] std::vector<std::byte> SerializeCandidateNodeV1(const CandidateNodeV1& node);
[[nodiscard]] base::Digest256 CandidateNodeIdentityV1(const CandidateNodeV1& node);
[[nodiscard]] base::Result<RawCandidateGraphV1,std::string> BuildRawCandidateGraphV1(const spiral2::hierarchy::RigidHierarchySemanticV1& hierarchy,const semantic::ReuseSweepSemanticV1& semantic,CandidateKindV1 kind);
[[nodiscard]] std::vector<std::byte> SerializeRawCandidateGraphV1(const RawCandidateGraphV1& graph);
[[nodiscard]] base::Digest256 RawCandidateGraphIdentityV1(const RawCandidateGraphV1& graph);
}
