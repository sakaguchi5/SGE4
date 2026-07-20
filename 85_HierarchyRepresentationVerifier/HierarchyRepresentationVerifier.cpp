#include "HierarchyRepresentationVerifier.h"
#include "../81_Spiral2Contracts/Spiral2Contracts.h"
#include <span>
#include <string_view>

namespace sge4_5::spiral2::verification
{
namespace
{
base::Digest256 Lit(std::string_view v){return base::Sha256(std::as_bytes(std::span(v.data(),v.size())));}
constexpr std::uint32_t Groups(std::uint32_t count) noexcept{return (count+63u)/64u;}
base::Digest256 IndependentProgramIdentity(candidate::OperationKindV1 op)
{
    using O=candidate::OperationKindV1;
    switch(op){case O::DynamicInvocationSource:return Lit("S2.L0.DynamicInvocationSource.V2");case O::LocalMotorToMatrix:return Lit("S2.A0.LocalMotorToMatrix.V2");case O::MatrixHierarchy:return Lit("S2.A1.MatrixHierarchy.SerialCanonicalOrder.V2");case O::MatrixProbeApply:return Lit("S2.MatrixProbeApply.V2");case O::MotorHierarchy:return Lit("S2.MotorHierarchy.SerialCanonicalOrder.V2");case O::DirectPgaProbeApply:return Lit("S2.B1.DirectPgaProbeApply.V2");case O::GlobalMotorToMatrix:return Lit("S2.C1.GlobalMotorToMatrix.V2");default:return {};}
}
VerifiedHierarchyOperationV1 Make(std::uint32_t ordinal,candidate::OperationKindV1 op,std::uint32_t inStride,std::uint32_t inCount,std::uint32_t outStride,std::uint32_t outCount,std::uint32_t dispatchX,std::uint32_t secondStride=0,std::uint32_t secondCount=0)
{
    VerifiedHierarchyOperationV1 v;v.ordinal=ordinal;v.operation=op;v.primaryInputStride=inStride;v.primaryInputElementCount=inCount;v.primaryOutputStride=outStride;v.primaryOutputElementCount=outCount;v.secondaryInputStride=secondStride;v.secondaryInputElementCount=secondCount;v.secondaryOutputStride=secondStride;v.secondaryOutputElementCount=secondCount;v.dispatchX=dispatchX;v.programIdentity=IndependentProgramIdentity(op);v.operationIdentity=base::Sha256(SerializeVerifiedHierarchyOperationV1(v));return v;
}
base::Result<VerifiedHierarchyExecutionPlanV1,std::string> IndependentlyDerive(const hierarchy::RigidHierarchySemanticV1& h,candidate::CandidateKindV1 kind)
{
    using O=candidate::OperationKindV1;using L=candidate::MatrixLoweringPositionV1;
    const std::uint32_t bones=h.boneCount,points=bones*5u;VerifiedHierarchyExecutionPlanV1 p;p.hierarchySemanticIdentity=hierarchy::RigidHierarchySemanticIdentityV1(h);p.dynamicInvocationSchemaIdentity=contracts::DynamicPaletteInvocationSchemaIdentityV1(contracts::BuildDynamicPaletteInvocationSchemaV1(h));p.observationContractIdentity=h.observationContractIdentity;p.kind=kind;p.boneCount=bones;p.hierarchyDepthCount=static_cast<std::uint32_t>(h.depthOffsets.size()-1);p.hierarchyExecutionModel=candidate::HierarchyExecutionModelV1::SingleDispatchCanonicalOrderSerial;
    auto add=[&](O op,std::uint32_t inStride,std::uint32_t inCount,std::uint32_t outStride,std::uint32_t outCount,std::uint32_t dispatchX,std::uint32_t secondStride=0,std::uint32_t secondCount=0){p.operations.push_back(Make(static_cast<std::uint32_t>(p.operations.size()),op,inStride,inCount,outStride,outCount,dispatchX,secondStride,secondCount));if(p.operations.size()>1)p.edges.push_back({static_cast<std::uint32_t>(p.operations.size()-2),static_cast<std::uint32_t>(p.operations.size()-1)});};
    add(O::DynamicInvocationSource,32,bones,32,bones,Groups(points),16,points);
    switch(kind){case candidate::CandidateKindV1::MatrixHierarchy:add(O::LocalMotorToMatrix,32,bones,48,bones,Groups(bones));add(O::MatrixHierarchy,48,bones,48,bones,1);add(O::MatrixProbeApply,48,bones,16,points,Groups(points));p.loweringPosition=L::BeforeHierarchy;break;case candidate::CandidateKindV1::DirectPgaHierarchy:add(O::MotorHierarchy,32,bones,32,bones,1);add(O::DirectPgaProbeApply,32,bones,16,points,Groups(points));p.loweringPosition=L::Absent;break;case candidate::CandidateKindV1::HybridHierarchy:add(O::MotorHierarchy,32,bones,32,bones,1);add(O::GlobalMotorToMatrix,32,bones,48,bones,Groups(bones));add(O::MatrixProbeApply,48,bones,16,points,Groups(points));p.loweringPosition=L::AfterHierarchy;break;default:return base::Result<VerifiedHierarchyExecutionPlanV1,std::string>::Failure("unknown candidate kind");}
    p.candidateLeafDispatchCount=static_cast<std::uint32_t>(p.operations.size()-1u);p.planIdentity=base::Sha256(SerializeVerifiedHierarchyExecutionPlanV1(p));return base::Result<VerifiedHierarchyExecutionPlanV1,std::string>::Success(std::move(p));
}
bool SameNode(const candidate::CandidateNodeV1& a,const VerifiedHierarchyOperationV1& b)
{
    return a.ordinal==b.ordinal&&a.operation==b.operation&&a.primaryInputStride==b.primaryInputStride&&a.primaryInputElementCount==b.primaryInputElementCount&&a.primaryOutputStride==b.primaryOutputStride&&a.primaryOutputElementCount==b.primaryOutputElementCount&&a.secondaryInputStride==b.secondaryInputStride&&a.secondaryInputElementCount==b.secondaryInputElementCount&&a.secondaryOutputStride==b.secondaryOutputStride&&a.secondaryOutputElementCount==b.secondaryOutputElementCount&&a.dispatchX==b.dispatchX&&a.dispatchY==b.dispatchY&&a.dispatchZ==b.dispatchZ&&a.programIdentity==b.programIdentity;
}
}
std::vector<std::byte> SerializeVerifiedHierarchyOperationV1(const VerifiedHierarchyOperationV1& n)
{
    base::BinaryWriter w;w.WriteU32(n.ordinal);w.WriteU32(static_cast<std::uint32_t>(n.operation));w.WriteU32(n.primaryInputStride);w.WriteU32(n.primaryInputElementCount);w.WriteU32(n.primaryOutputStride);w.WriteU32(n.primaryOutputElementCount);w.WriteU32(n.secondaryInputStride);w.WriteU32(n.secondaryInputElementCount);w.WriteU32(n.secondaryOutputStride);w.WriteU32(n.secondaryOutputElementCount);w.WriteU32(n.dispatchX);w.WriteU32(n.dispatchY);w.WriteU32(n.dispatchZ);w.WriteU32(0);w.WriteBytes(n.programIdentity);return std::move(w).Take();
}
std::vector<std::byte> SerializeVerifiedHierarchyExecutionPlanV1(const VerifiedHierarchyExecutionPlanV1& p)
{
    base::BinaryWriter w;w.WriteBytes(Lit("SGE4-5.Spiral2.VerifiedHierarchyExecutionPlan.V2"));w.WriteBytes(p.hierarchySemanticIdentity);w.WriteBytes(p.dynamicInvocationSchemaIdentity);w.WriteBytes(p.observationContractIdentity);w.WriteU32(static_cast<std::uint32_t>(p.kind));w.WriteU32(static_cast<std::uint32_t>(p.loweringPosition));w.WriteU32(static_cast<std::uint32_t>(p.hierarchyExecutionModel));w.WriteU32(p.boneCount);w.WriteU32(p.hierarchyDepthCount);w.WriteU32(p.candidateLeafDispatchCount);w.WriteU32(static_cast<std::uint32_t>(p.operations.size()));w.WriteU32(static_cast<std::uint32_t>(p.edges.size()));for(const auto& n:p.operations){auto bytes=SerializeVerifiedHierarchyOperationV1(n);w.WriteU32(static_cast<std::uint32_t>(bytes.size()));w.WriteBytes(bytes);w.WriteBytes(n.operationIdentity);}for(const auto& e:p.edges){w.WriteU32(e.source);w.WriteU32(e.destination);}return std::move(w).Take();
}
base::Result<VerifiedHierarchyRepresentationV1,std::string> VerifyCandidateGraphV1(const hierarchy::RigidHierarchySemanticV1& h,const candidate::RawCandidateGraphV1& proposal)
{
    auto valid=hierarchy::ValidateRigidHierarchySemanticV1(h);if(!valid)return base::Result<VerifiedHierarchyRepresentationV1,std::string>::Failure(valid.Error());auto expected=IndependentlyDerive(h,proposal.kind);if(!expected)return base::Result<VerifiedHierarchyRepresentationV1,std::string>::Failure(expected.Error());const auto& p=expected.Value();
    if(proposal.hierarchySemanticIdentity!=p.hierarchySemanticIdentity)return base::Result<VerifiedHierarchyRepresentationV1,std::string>::Failure("semantic identity mismatch");if(proposal.dynamicInvocationSchemaIdentity!=p.dynamicInvocationSchemaIdentity)return base::Result<VerifiedHierarchyRepresentationV1,std::string>::Failure("dynamic invocation schema mismatch");if(proposal.observationContractIdentity!=p.observationContractIdentity)return base::Result<VerifiedHierarchyRepresentationV1,std::string>::Failure("observation contract mismatch");if(proposal.boneCount!=p.boneCount)return base::Result<VerifiedHierarchyRepresentationV1,std::string>::Failure("bone count mismatch");if(proposal.proposedLoweringPosition!=p.loweringPosition||proposal.proposedHierarchyExecutionModel!=p.hierarchyExecutionModel||proposal.hierarchyDepthCount!=p.hierarchyDepthCount||proposal.proposedCandidateLeafDispatchCount!=p.candidateLeafDispatchCount)return base::Result<VerifiedHierarchyRepresentationV1,std::string>::Failure("execution model, lowering, depth, or dispatch decision mismatch");if(proposal.nodes.size()!=p.operations.size()||proposal.edges.size()!=p.edges.size())return base::Result<VerifiedHierarchyRepresentationV1,std::string>::Failure("candidate graph shape mismatch");for(std::size_t i=0;i<proposal.nodes.size();++i)if(!SameNode(proposal.nodes[i],p.operations[i]))return base::Result<VerifiedHierarchyRepresentationV1,std::string>::Failure("node execution decision mismatch");for(std::size_t i=0;i<proposal.edges.size();++i)if(proposal.edges[i].source!=p.edges[i].source||proposal.edges[i].destination!=p.edges[i].destination)return base::Result<VerifiedHierarchyRepresentationV1,std::string>::Failure("edge order mismatch");
    base::BinaryWriter w;w.WriteBytes(Lit("SGE4-5.Spiral2.VerifierCertificate.V2"));w.WriteBytes(p.planIdentity);w.WriteBytes(p.hierarchySemanticIdentity);w.WriteU32(static_cast<std::uint32_t>(p.kind));w.WriteU32(p.candidateLeafDispatchCount);const auto cert=base::Sha256(std::move(w).Take());return base::Result<VerifiedHierarchyRepresentationV1,std::string>::Success(VerifiedHierarchyRepresentationV1(std::move(expected).Value(),cert));
}
std::vector<std::byte> SerializeVerifiedHierarchyRepresentationV1(const VerifiedHierarchyRepresentationV1& v){base::BinaryWriter w;const auto plan=SerializeVerifiedHierarchyExecutionPlanV1(v.Plan());w.WriteBytes(Lit("SGE4-5.Spiral2.VerifiedHierarchyRepresentation.V2"));w.WriteU32(static_cast<std::uint32_t>(plan.size()));w.WriteBytes(plan);w.WriteBytes(v.Plan().planIdentity);w.WriteBytes(v.CertificateIdentity());return std::move(w).Take();}
}
