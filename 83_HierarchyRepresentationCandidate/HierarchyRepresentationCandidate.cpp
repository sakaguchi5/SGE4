#include "HierarchyRepresentationCandidate.h"
#include <span>
#include <string_view>

namespace sge4_5::spiral2::candidate
{
namespace
{
base::Digest256 Lit(std::string_view v){return base::Sha256(std::as_bytes(std::span(v.data(),v.size())));}
constexpr std::uint32_t Groups(std::uint32_t count) noexcept{return (count+63u)/64u;}
}
base::Digest256 ProgramIdentityV1(OperationKindV1 op)
{
    switch(op){case OperationKindV1::DynamicInvocationSource:return Lit("S2.L0.DynamicInvocationSource.V2");case OperationKindV1::LocalMotorToMatrix:return Lit("S2.A0.LocalMotorToMatrix.V2");case OperationKindV1::MatrixHierarchy:return Lit("S2.A1.MatrixHierarchy.SerialCanonicalOrder.V2");case OperationKindV1::MatrixProbeApply:return Lit("S2.MatrixProbeApply.V2");case OperationKindV1::MotorHierarchy:return Lit("S2.MotorHierarchy.SerialCanonicalOrder.V2");case OperationKindV1::DirectPgaProbeApply:return Lit("S2.B1.DirectPgaProbeApply.V2");case OperationKindV1::GlobalMotorToMatrix:return Lit("S2.C1.GlobalMotorToMatrix.V2");default:return {};}
}
std::vector<std::byte> SerializeCandidateNodeV1(const CandidateNodeV1& n)
{
    base::BinaryWriter w;w.WriteU32(n.ordinal);w.WriteU32(static_cast<std::uint32_t>(n.operation));w.WriteU32(n.primaryInputStride);w.WriteU32(n.primaryInputElementCount);w.WriteU32(n.primaryOutputStride);w.WriteU32(n.primaryOutputElementCount);w.WriteU32(n.secondaryInputStride);w.WriteU32(n.secondaryInputElementCount);w.WriteU32(n.secondaryOutputStride);w.WriteU32(n.secondaryOutputElementCount);w.WriteU32(n.dispatchX);w.WriteU32(n.dispatchY);w.WriteU32(n.dispatchZ);w.WriteU32(0);w.WriteBytes(n.programIdentity);return std::move(w).Take();
}
base::Digest256 CandidateNodeIdentityV1(const CandidateNodeV1& n){return base::Sha256(SerializeCandidateNodeV1(n));}
base::Result<RawCandidateGraphV1,std::string> BuildRawCandidateGraphV1(const hierarchy::RigidHierarchySemanticV1& value,CandidateKindV1 kind)
{
    auto valid=hierarchy::ValidateRigidHierarchySemanticV1(value);if(!valid)return base::Result<RawCandidateGraphV1,std::string>::Failure(valid.Error());
    const std::uint32_t bones=value.boneCount,points=bones*5u;
    RawCandidateGraphV1 g;g.hierarchySemanticIdentity=hierarchy::RigidHierarchySemanticIdentityV1(value);g.dynamicInvocationSchemaIdentity=contracts::DynamicPaletteInvocationSchemaIdentityV1(contracts::BuildDynamicPaletteInvocationSchemaV1(value));g.observationContractIdentity=value.observationContractIdentity;g.kind=kind;g.boneCount=bones;g.proposedHierarchyExecutionModel=HierarchyExecutionModelV1::SingleDispatchCanonicalOrderSerial;g.hierarchyDepthCount=static_cast<std::uint32_t>(value.depthOffsets.size()-1);
    auto add=[&](OperationKindV1 op,std::uint32_t inStride,std::uint32_t inCount,std::uint32_t outStride,std::uint32_t outCount,std::uint32_t dispatchX,std::uint32_t secondStride=0,std::uint32_t secondCount=0){CandidateNodeV1 n;n.ordinal=static_cast<std::uint32_t>(g.nodes.size());n.operation=op;n.primaryInputStride=inStride;n.primaryInputElementCount=inCount;n.primaryOutputStride=outStride;n.primaryOutputElementCount=outCount;n.secondaryInputStride=secondStride;n.secondaryInputElementCount=secondCount;n.secondaryOutputStride=secondStride;n.secondaryOutputElementCount=secondCount;n.dispatchX=dispatchX;n.programIdentity=ProgramIdentityV1(op);g.nodes.push_back(n);if(g.nodes.size()>1)g.edges.push_back({static_cast<std::uint32_t>(g.nodes.size()-2),static_cast<std::uint32_t>(g.nodes.size()-1)});};
    add(OperationKindV1::DynamicInvocationSource,32,bones,32,bones,Groups(points),16,points);
    switch(kind)
    {
    case CandidateKindV1::MatrixHierarchy:add(OperationKindV1::LocalMotorToMatrix,32,bones,48,bones,Groups(bones));add(OperationKindV1::MatrixHierarchy,48,bones,48,bones,1);add(OperationKindV1::MatrixProbeApply,48,bones,16,points,Groups(points));g.proposedLoweringPosition=MatrixLoweringPositionV1::BeforeHierarchy;break;
    case CandidateKindV1::DirectPgaHierarchy:add(OperationKindV1::MotorHierarchy,32,bones,32,bones,1);add(OperationKindV1::DirectPgaProbeApply,32,bones,16,points,Groups(points));g.proposedLoweringPosition=MatrixLoweringPositionV1::Absent;break;
    case CandidateKindV1::HybridHierarchy:add(OperationKindV1::MotorHierarchy,32,bones,32,bones,1);add(OperationKindV1::GlobalMotorToMatrix,32,bones,48,bones,Groups(bones));add(OperationKindV1::MatrixProbeApply,48,bones,16,points,Groups(points));g.proposedLoweringPosition=MatrixLoweringPositionV1::AfterHierarchy;break;
    default:return base::Result<RawCandidateGraphV1,std::string>::Failure("unknown candidate kind");
    }
    g.proposedCandidateLeafDispatchCount=static_cast<std::uint32_t>(g.nodes.size()-1u);
    return base::Result<RawCandidateGraphV1,std::string>::Success(std::move(g));
}
std::vector<std::byte> SerializeRawCandidateGraphV1(const RawCandidateGraphV1& g)
{
    base::BinaryWriter w;w.WriteBytes(Lit("SGE4-5.Spiral2.RawCandidateGraph.V2"));w.WriteBytes(g.hierarchySemanticIdentity);w.WriteBytes(g.dynamicInvocationSchemaIdentity);w.WriteBytes(g.observationContractIdentity);w.WriteU32(static_cast<std::uint32_t>(g.kind));w.WriteU32(g.boneCount);w.WriteU32(static_cast<std::uint32_t>(g.nodes.size()));w.WriteU32(static_cast<std::uint32_t>(g.edges.size()));w.WriteU32(static_cast<std::uint32_t>(g.proposedLoweringPosition));w.WriteU32(static_cast<std::uint32_t>(g.proposedHierarchyExecutionModel));w.WriteU32(g.hierarchyDepthCount);w.WriteU32(g.proposedCandidateLeafDispatchCount);
    for(const auto& n:g.nodes){auto bytes=SerializeCandidateNodeV1(n);w.WriteU32(static_cast<std::uint32_t>(bytes.size()));w.WriteBytes(bytes);}for(const auto& e:g.edges){w.WriteU32(e.source);w.WriteU32(e.destination);}return std::move(w).Take();
}
base::Digest256 RawCandidateGraphIdentityV1(const RawCandidateGraphV1& g){return base::Sha256(SerializeRawCandidateGraphV1(g));}
}
