#include "HierarchyRepresentationCandidate.h"
#include <span>
#include <string_view>

namespace sge4_5::spiral2::candidate
{
namespace { base::Digest256 Lit(std::string_view v){return base::Sha256(std::as_bytes(std::span(v.data(),v.size())));} }
base::Digest256 ProgramIdentityV1(OperationKindV1 op)
{
    switch(op){case OperationKindV1::DynamicInvocationSource:return Lit("S2.L0.DynamicInvocationSource.V1");case OperationKindV1::LocalMotorToMatrix:return Lit("S2.A0.LocalMotorToMatrix.V1");case OperationKindV1::MatrixHierarchy:return Lit("S2.A1.MatrixHierarchy.V1");case OperationKindV1::MatrixProbeApply:return Lit("S2.MatrixProbeApply.V1");case OperationKindV1::MotorHierarchy:return Lit("S2.MotorHierarchy.V1");case OperationKindV1::DirectPgaProbeApply:return Lit("S2.B1.DirectPgaProbeApply.V1");case OperationKindV1::GlobalMotorToMatrix:return Lit("S2.C1.GlobalMotorToMatrix.V1");default:return {};}
}
base::Result<RawCandidateGraphV1,std::string> BuildRawCandidateGraphV1(const hierarchy::RigidHierarchySemanticV1& value,CandidateKindV1 kind)
{
    auto valid=hierarchy::ValidateRigidHierarchySemanticV1(value);if(!valid)return base::Result<RawCandidateGraphV1,std::string>::Failure(valid.Error());
    RawCandidateGraphV1 g;g.hierarchySemanticIdentity=hierarchy::RigidHierarchySemanticIdentityV1(value);g.dynamicInvocationSchemaIdentity=contracts::DynamicPaletteInvocationSchemaIdentityV1(contracts::BuildDynamicPaletteInvocationSchemaV1(value));g.observationContractIdentity=value.observationContractIdentity;g.kind=kind;g.boneCount=value.boneCount;g.proposedHierarchyDepthCount=static_cast<std::uint32_t>(value.depthOffsets.size()-1);
    auto add=[&](OperationKindV1 op,std::uint32_t in,std::uint32_t out){g.nodes.push_back({static_cast<std::uint32_t>(g.nodes.size()),op,in,out,ProgramIdentityV1(op)});if(g.nodes.size()>1)g.edges.push_back({static_cast<std::uint32_t>(g.nodes.size()-2),static_cast<std::uint32_t>(g.nodes.size()-1)});};
    add(OperationKindV1::DynamicInvocationSource,32,32);
    switch(kind)
    {
    case CandidateKindV1::MatrixHierarchy:add(OperationKindV1::LocalMotorToMatrix,32,48);add(OperationKindV1::MatrixHierarchy,48,48);add(OperationKindV1::MatrixProbeApply,48,16);g.proposedLoweringPosition=MatrixLoweringPositionV1::BeforeHierarchy;g.proposedDispatchCount=g.proposedHierarchyDepthCount+2;break;
    case CandidateKindV1::DirectPgaHierarchy:add(OperationKindV1::MotorHierarchy,32,32);add(OperationKindV1::DirectPgaProbeApply,32,16);g.proposedLoweringPosition=MatrixLoweringPositionV1::Absent;g.proposedDispatchCount=g.proposedHierarchyDepthCount+1;break;
    case CandidateKindV1::HybridHierarchy:add(OperationKindV1::MotorHierarchy,32,32);add(OperationKindV1::GlobalMotorToMatrix,32,48);add(OperationKindV1::MatrixProbeApply,48,16);g.proposedLoweringPosition=MatrixLoweringPositionV1::AfterHierarchy;g.proposedDispatchCount=g.proposedHierarchyDepthCount+2;break;
    default:return base::Result<RawCandidateGraphV1,std::string>::Failure("unknown candidate kind");
    }
    return base::Result<RawCandidateGraphV1,std::string>::Success(std::move(g));
}
std::vector<std::byte> SerializeRawCandidateGraphV1(const RawCandidateGraphV1& g)
{
    base::BinaryWriter w;w.WriteBytes(Lit("SGE4-5.Spiral2.RawCandidateGraph.V1"));w.WriteBytes(g.hierarchySemanticIdentity);w.WriteBytes(g.dynamicInvocationSchemaIdentity);w.WriteBytes(g.observationContractIdentity);w.WriteU32(static_cast<std::uint32_t>(g.kind));w.WriteU32(g.boneCount);w.WriteU32(static_cast<std::uint32_t>(g.nodes.size()));w.WriteU32(static_cast<std::uint32_t>(g.edges.size()));w.WriteU32(static_cast<std::uint32_t>(g.proposedLoweringPosition));w.WriteU32(g.proposedHierarchyDepthCount);w.WriteU32(g.proposedDispatchCount);w.WriteU32(0);
    for(const auto& n:g.nodes){w.WriteU32(n.ordinal);w.WriteU32(static_cast<std::uint32_t>(n.operation));w.WriteU32(n.inputStride);w.WriteU32(n.outputStride);w.WriteBytes(n.programIdentity);}for(const auto& e:g.edges){w.WriteU32(e.source);w.WriteU32(e.destination);}return std::move(w).Take();
}
base::Digest256 RawCandidateGraphIdentityV1(const RawCandidateGraphV1& g){return base::Sha256(SerializeRawCandidateGraphV1(g));}
}
