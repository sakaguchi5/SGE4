#include "HierarchyRepresentationVerifier.h"
#include "../81_Spiral2Contracts/Spiral2Contracts.h"
#include <span>
#include <string_view>

namespace sge4_5::spiral2::verification
{
namespace
{
base::Digest256 Lit(std::string_view v){return base::Sha256(std::as_bytes(std::span(v.data(),v.size())));}
struct Shape{std::vector<candidate::OperationKindV1> ops;std::vector<std::pair<std::uint32_t,std::uint32_t>> strides;candidate::MatrixLoweringPositionV1 lowering{};std::uint32_t extra=0;};
base::Result<Shape,std::string> Expected(candidate::CandidateKindV1 kind)
{
    using O=candidate::OperationKindV1;using L=candidate::MatrixLoweringPositionV1;
    switch(kind){case candidate::CandidateKindV1::MatrixHierarchy:return base::Result<Shape,std::string>::Success({{O::DynamicInvocationSource,O::LocalMotorToMatrix,O::MatrixHierarchy,O::MatrixProbeApply},{{32,32},{32,48},{48,48},{48,16}},L::BeforeHierarchy,2});case candidate::CandidateKindV1::DirectPgaHierarchy:return base::Result<Shape,std::string>::Success({{O::DynamicInvocationSource,O::MotorHierarchy,O::DirectPgaProbeApply},{{32,32},{32,32},{32,16}},L::Absent,1});case candidate::CandidateKindV1::HybridHierarchy:return base::Result<Shape,std::string>::Success({{O::DynamicInvocationSource,O::MotorHierarchy,O::GlobalMotorToMatrix,O::MatrixProbeApply},{{32,32},{32,32},{32,48},{48,16}},L::AfterHierarchy,2});default:return base::Result<Shape,std::string>::Failure("unknown candidate kind");}
}
}
base::Result<VerifiedHierarchyRepresentationV1,std::string> VerifyCandidateGraphV1(const hierarchy::RigidHierarchySemanticV1& h,const candidate::RawCandidateGraphV1& p)
{
    auto valid=hierarchy::ValidateRigidHierarchySemanticV1(h);if(!valid)return base::Result<VerifiedHierarchyRepresentationV1,std::string>::Failure(valid.Error());
    if(p.hierarchySemanticIdentity!=hierarchy::RigidHierarchySemanticIdentityV1(h))return base::Result<VerifiedHierarchyRepresentationV1,std::string>::Failure("semantic identity mismatch");
    const auto schema=contracts::BuildDynamicPaletteInvocationSchemaV1(h);if(p.dynamicInvocationSchemaIdentity!=contracts::DynamicPaletteInvocationSchemaIdentityV1(schema))return base::Result<VerifiedHierarchyRepresentationV1,std::string>::Failure("dynamic invocation schema mismatch");
    if(p.observationContractIdentity!=h.observationContractIdentity)return base::Result<VerifiedHierarchyRepresentationV1,std::string>::Failure("observation contract mismatch");
    if(p.boneCount!=h.boneCount)return base::Result<VerifiedHierarchyRepresentationV1,std::string>::Failure("bone count mismatch");
    auto expected=Expected(p.kind);if(!expected)return base::Result<VerifiedHierarchyRepresentationV1,std::string>::Failure(expected.Error());const auto& s=expected.Value();
    if(p.nodes.size()!=s.ops.size()||p.edges.size()+1!=p.nodes.size())return base::Result<VerifiedHierarchyRepresentationV1,std::string>::Failure("candidate graph shape mismatch");
    for(std::size_t i=0;i<p.nodes.size();++i){const auto& n=p.nodes[i];if(n.ordinal!=i||n.operation!=s.ops[i]||n.inputStride!=s.strides[i].first||n.outputStride!=s.strides[i].second||n.programIdentity!=candidate::ProgramIdentityV1(s.ops[i]))return base::Result<VerifiedHierarchyRepresentationV1,std::string>::Failure("node decision mismatch");}
    for(std::size_t i=0;i<p.edges.size();++i)if(p.edges[i].source!=i||p.edges[i].destination!=i+1)return base::Result<VerifiedHierarchyRepresentationV1,std::string>::Failure("edge order mismatch");
    const auto depths=static_cast<std::uint32_t>(h.depthOffsets.size()-1);if(p.proposedLoweringPosition!=s.lowering||p.proposedHierarchyDepthCount!=depths||p.proposedDispatchCount!=depths+s.extra)return base::Result<VerifiedHierarchyRepresentationV1,std::string>::Failure("schedule or lowering decision mismatch");
    const auto graphBytes=candidate::SerializeRawCandidateGraphV1(p);base::BinaryWriter w;w.WriteBytes(Lit("SGE4-5.Spiral2.VerifierCertificate.V1"));w.WriteBytes(base::Sha256(graphBytes));w.WriteBytes(p.hierarchySemanticIdentity);w.WriteU32(static_cast<std::uint32_t>(p.kind));w.WriteU32(p.proposedDispatchCount);auto cert=base::Sha256(std::move(w).Take());
    return base::Result<VerifiedHierarchyRepresentationV1,std::string>::Success(VerifiedHierarchyRepresentationV1(p,cert));
}
std::vector<std::byte> SerializeVerifiedHierarchyRepresentationV1(const VerifiedHierarchyRepresentationV1& v){base::BinaryWriter w;const auto graph=candidate::SerializeRawCandidateGraphV1(v.Graph());w.WriteBytes(Lit("SGE4-5.Spiral2.VerifiedHierarchyRepresentation.V1"));w.WriteU32(static_cast<std::uint32_t>(graph.size()));w.WriteBytes(graph);w.WriteBytes(v.CertificateIdentity());return std::move(w).Take();}
}
