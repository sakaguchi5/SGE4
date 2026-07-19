#include "Spiral2LeafCompiler.h"
#include "../03_SemanticBuilder/SemanticBuilder.h"
#include "../09_FrozenPackageCore/PackageReader.h"
#include "../12_SGE4_5Compiler/SGE4_5Compiler.h"
#include "../66_Spiral1LeafCompiler/Spiral1LeafCompiler.h"
#include <array>
#include <span>
#include <sstream>

namespace sge4_5::spiral2::leaf
{
namespace
{
namespace sem=::sge4_5::semantic;
LeafCompileErrorV1 Err(std::string stage,std::string message){return {std::move(stage),std::move(message)};}
target::D3D12TargetProfile Profile(){auto p=spiral1::leaf::CanonicalLeafD3D12TargetProfileV1();p.shaderDescriptorCount=8;return p;}
std::string Preamble(std::uint32_t bones,const hierarchy::RigidHierarchySemanticV1& h)
{
    std::ostringstream s;s<<"static const uint BoneCount="<<bones<<"u;\nstatic const uint ProbeCount=5u;\n";
    s<<"static const int Parent["<<bones<<"]={";for(std::uint32_t i=0;i<bones;++i){if(i)s<<',';s<<h.parentIndices[i];}s<<"};\n";
    s<<"static const uint Order["<<bones<<"]={";for(std::uint32_t i=0;i<bones;++i){if(i)s<<',';s<<h.canonicalBoneOrder[i]<<'u';}s<<"};\n";
    return s.str();
}
const char* CommonTypes=R"hlsl(
struct Motor { float4 qr; float4 qd; };
struct Matrix34 { float4 r0; float4 r1; float4 r2; };
float4 QMul(float4 a,float4 b){return float4(a.x*b.x-dot(a.yzw,b.yzw),a.x*b.yzw+b.x*a.yzw+cross(a.yzw,b.yzw));}
float4 QConj(float4 q){return float4(q.x,-q.y,-q.z,-q.w);}
Motor DqMul(Motor a,Motor b){Motor o;o.qr=QMul(a.qr,b.qr);o.qd=QMul(a.qr,b.qd)+QMul(a.qd,b.qr);return o;}
Matrix34 ToMatrix(Motor m){float w=m.qr.x,x=m.qr.y,y=m.qr.z,z=m.qr.w;float3 t=2.0f*QMul(m.qd,QConj(m.qr)).yzw;Matrix34 o;o.r0=float4(1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w),t.x);o.r1=float4(2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w),t.y);o.r2=float4(2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y),t.z);return o;}
Matrix34 MMul(Matrix34 a,Matrix34 b){Matrix34 o;float3x3 ar=float3x3(a.r0.xyz,a.r1.xyz,a.r2.xyz);float3x3 br=float3x3(b.r0.xyz,b.r1.xyz,b.r2.xyz);float3x3 r=mul(ar,br);float3 t=mul(ar,float3(b.r0.w,b.r1.w,b.r2.w))+float3(a.r0.w,a.r1.w,a.r2.w);o.r0=float4(r[0],t.x);o.r1=float4(r[1],t.y);o.r2=float4(r[2],t.z);return o;}
float3 Probe(uint i){float3 p=float3(0.37f,-0.61f,1.13f);if(i==0)p=float3(0,0,0);else if(i==1)p=float3(1,0,0);else if(i==2)p=float3(0,1,0);else if(i==3)p=float3(0,0,1);return p;}
)hlsl";

base::Result<FrozenHierarchyLeafV1,LeafCompileErrorV1> Finish(std::string role,sem::SemanticBuilder builder,std::uint32_t dynamicBytes,std::uint32_t inStride,std::uint32_t outStride,const base::Digest256& cert)
{
    auto compiled=compiler::CompileCanonical(std::move(builder).Build(),Profile());if(!compiled)return base::Result<FrozenHierarchyLeafV1,LeafCompileErrorV1>::Failure(Err(compiled.Error().stage,compiled.Error().message));
    auto package=package::PackageReader::Read(compiled.Value().packageBytes);if(!package)return base::Result<FrozenHierarchyLeafV1,LeafCompileErrorV1>::Failure(Err("package-read",package.Error().message));
    FrozenHierarchyLeafV1 out;out.role=std::move(role);out.verifiedRepresentationCertificate=cert;out.executionDigest=package.Value().ExecutionDigest();out.fileDigest=package.Value().Header().fileDigest;out.lockedDynamicSlot=dynamicBytes?0u:base::InvalidIndex;out.lockedDynamicBytes=dynamicBytes;out.inputStride=inStride;out.outputStride=outStride;out.packageBytes=std::move(compiled.Value().packageBytes);return base::Result<FrozenHierarchyLeafV1,LeafCompileErrorV1>::Success(std::move(out));
}

base::Result<FrozenHierarchyLeafV1,LeafCompileErrorV1> Unary(std::string role,const hierarchy::RigidHierarchySemanticV1& h,std::uint32_t inputCount,std::uint32_t inputStride,std::uint32_t outputCount,std::uint32_t outputStride,std::string shader,const base::Digest256& cert)
{
    sem::SemanticBuilder b;auto input=b.AddExternalBuffer(role+".Input",static_cast<std::uint64_t>(inputCount)*inputStride,inputStride);auto output=b.AddExternalBuffer(role+".Output",static_cast<std::uint64_t>(outputCount)*outputStride,outputStride);if(!input||!output)return base::Result<FrozenHierarchyLeafV1,LeafCompileErrorV1>::Failure(Err("resources","external resource construction failed"));auto ui=b.AddUse(input.Value(),sem::Effect::Read,sem::ViewRole::ShaderBuffer);auto uo=b.AddUse(output.Value(),sem::Effect::Write,sem::ViewRole::StorageBuffer);if(!ui||!uo)return base::Result<FrozenHierarchyLeafV1,LeafCompileErrorV1>::Failure(Err("uses","resource use construction failed"));sem::ProgramInterface pi;pi.parameters={{{0},"Input",sem::ProgramParameterKind::ReadOnlyBuffer,sem::ShaderStage::Compute,0,0,1},{{1},"Output",sem::ProgramParameterKind::UnorderedBuffer,sem::ShaderStage::Compute,0,0,1}};auto program=b.AddComputeProgram(role+".Program",std::move(pi),{Preamble(h.boneCount,h)+CommonTypes+shader,{}, {},"CSMain"});if(!program)return base::Result<FrozenHierarchyLeafV1,LeafCompileErrorV1>::Failure(Err("program",program.Error()));const std::array ops={sem::WorkOperand{sem::WorkOperandKind::ProgramParameter,ui.Value(),{0}},sem::WorkOperand{sem::WorkOperandKind::ProgramParameter,uo.Value(),{1}}};auto work=b.AddComputeWorkGeneric(role+".Work",program.Value(),ops,(outputCount+63)/64,1,1);if(!work)return base::Result<FrozenHierarchyLeafV1,LeafCompileErrorV1>::Failure(Err("work",work.Error()));return Finish(std::move(role),std::move(b),0,inputStride,outputStride,cert);
}
}

base::Result<FrozenHierarchyLeafV1,LeafCompileErrorV1> CompileDynamicInvocationSourceV1(const hierarchy::RigidHierarchySemanticV1& h,const verification::VerifiedHierarchyRepresentationV1& verified)
{
    if(verified.Graph().hierarchySemanticIdentity!=hierarchy::RigidHierarchySemanticIdentityV1(h))return base::Result<FrozenHierarchyLeafV1,LeafCompileErrorV1>::Failure(Err("authority","verified semantic mismatch"));
    const std::uint32_t bytes=h.boneCount*32u,points=h.boneCount*5u,referenceBytes=points*16u;sem::SemanticBuilder b;auto dynamic=b.AddDynamicBuffer("Spiral2.DynamicMotors",bytes,16);auto reference=b.AddDynamicBuffer("Spiral2.ReferenceObservationPoints",referenceBytes,16);auto output=b.AddExternalBuffer("Spiral2.DynamicSource.Output",bytes,32);auto referenceOutput=b.AddExternalBuffer("Spiral2.DynamicSource.ReferenceOutput",referenceBytes,16);if(!dynamic||!reference||!output||!referenceOutput)return base::Result<FrozenHierarchyLeafV1,LeafCompileErrorV1>::Failure(Err("resources","dynamic source resource construction failed"));auto ud=b.AddUse(dynamic.Value(),sem::Effect::Read,sem::ViewRole::ConstantData);auto ur=b.AddUse(reference.Value(),sem::Effect::Read,sem::ViewRole::ConstantData);auto uo=b.AddUse(output.Value(),sem::Effect::Write,sem::ViewRole::StorageBuffer);auto uro=b.AddUse(referenceOutput.Value(),sem::Effect::Write,sem::ViewRole::StorageBuffer);sem::ProgramInterface pi;pi.parameters={{{0},"Dynamic",sem::ProgramParameterKind::ConstantBuffer,sem::ShaderStage::Compute,0,bytes,16},{{1},"Reference",sem::ProgramParameterKind::ConstantBuffer,sem::ShaderStage::Compute,1,referenceBytes,16},{{2},"Output",sem::ProgramParameterKind::UnorderedBuffer,sem::ShaderStage::Compute,0,0,1},{{3},"ReferenceOutput",sem::ProgramParameterKind::UnorderedBuffer,sem::ShaderStage::Compute,1,0,1}};std::ostringstream ss;ss<<"static const uint BoneCount="<<h.boneCount<<"u;\nstatic const uint PointCount="<<points<<"u;\ncbuffer Dynamic:register(b0){float4 Words["<<h.boneCount*2<<"];};\ncbuffer Reference:register(b1){float4 ReferenceWords["<<points<<"];};\n"<<CommonTypes<<R"hlsl(
RWStructuredBuffer<Motor> Output:register(u0);RWStructuredBuffer<float4> ReferenceOutput:register(u1);[numthreads(64,1,1)]void CSMain(uint3 id:SV_DispatchThreadID){uint i=id.x;if(i<BoneCount){Motor m;m.qr=Words[i*2];m.qd=Words[i*2+1];Output[i]=m;}if(i<PointCount)ReferenceOutput[i]=ReferenceWords[i];})hlsl";
    auto program=b.AddComputeProgram("Spiral2.DynamicInvocationSource.V1",std::move(pi),{ss.str(),{}, {},"CSMain"});if(!program)return base::Result<FrozenHierarchyLeafV1,LeafCompileErrorV1>::Failure(Err("program",program.Error()));const std::array ops={sem::WorkOperand{sem::WorkOperandKind::ProgramParameter,ud.Value(),{0}},sem::WorkOperand{sem::WorkOperandKind::ProgramParameter,ur.Value(),{1}},sem::WorkOperand{sem::WorkOperandKind::ProgramParameter,uo.Value(),{2}},sem::WorkOperand{sem::WorkOperandKind::ProgramParameter,uro.Value(),{3}}};auto work=b.AddComputeWorkGeneric("Spiral2.DynamicInvocationSource.Work.V1",program.Value(),ops,(points+63)/64,1,1);if(!work)return base::Result<FrozenHierarchyLeafV1,LeafCompileErrorV1>::Failure(Err("work",work.Error()));auto finished=Finish("L0.DynamicInvocationSource",std::move(b),bytes,32,32,verified.CertificateIdentity());if(finished){finished.Value().lockedReferenceSlot=1;finished.Value().lockedReferenceBytes=referenceBytes;}return finished;
}

base::Result<FrozenHierarchyLeafGroupV1,LeafCompileErrorV1> CompileVerifiedHierarchyLeafGroupV1(const hierarchy::RigidHierarchySemanticV1& h,const verification::VerifiedHierarchyRepresentationV1& verified)
{
    if(verified.Graph().hierarchySemanticIdentity!=hierarchy::RigidHierarchySemanticIdentityV1(h))return base::Result<FrozenHierarchyLeafGroupV1,LeafCompileErrorV1>::Failure(Err("authority","verified semantic mismatch"));
    const auto cert=verified.CertificateIdentity();const auto bones=h.boneCount;const auto points=bones*5u;FrozenHierarchyLeafGroupV1 group;group.kind=verified.Graph().kind;group.hierarchySemanticIdentity=verified.Graph().hierarchySemanticIdentity;group.verifierCertificate=cert;
    LeafCompileErrorV1 lastError;auto add=[&](base::Result<FrozenHierarchyLeafV1,LeafCompileErrorV1> leaf){if(!leaf){lastError=leaf.Error();return false;}group.leaves.push_back(std::move(leaf.Value()));return true;};
    const std::string toMatrix=R"hlsl(StructuredBuffer<Motor> Input:register(t0);RWStructuredBuffer<Matrix34> Output:register(u0);[numthreads(64,1,1)]void CSMain(uint3 id:SV_DispatchThreadID){if(id.x<BoneCount)Output[id.x]=ToMatrix(Input[id.x]);})hlsl";
    const std::string matrixHierarchy=h.boneCount==1?R"hlsl(StructuredBuffer<Matrix34> Input:register(t0);RWStructuredBuffer<Matrix34> Output:register(u0);[numthreads(64,1,1)]void CSMain(uint3 id:SV_DispatchThreadID){if(id.x==0)Output[0]=Input[0];})hlsl":R"hlsl(StructuredBuffer<Matrix34> Input:register(t0);RWStructuredBuffer<Matrix34> Output:register(u0);[numthreads(64,1,1)]void CSMain(uint3 id:SV_DispatchThreadID){if(id.x!=0)return;[loop]for(uint j=0;j<BoneCount;++j){uint i=Order[j];if(Parent[i]<0)Output[i]=Input[i];else Output[i]=MMul(Output[Parent[i]],Input[i]);}})hlsl";
    const std::string matrixApply=R"hlsl(StructuredBuffer<Matrix34> Input:register(t0);RWStructuredBuffer<float4> Output:register(u0);[numthreads(64,1,1)]void CSMain(uint3 id:SV_DispatchThreadID){uint n=id.x;if(n>=BoneCount*ProbeCount)return;uint bone=n/ProbeCount;float3 p=Probe(n%ProbeCount);Matrix34 m=Input[bone];Output[n]=float4(mul(float3x3(m.r0.xyz,m.r1.xyz,m.r2.xyz),p)+float3(m.r0.w,m.r1.w,m.r2.w),1);})hlsl";
    const std::string motorHierarchy=h.boneCount==1?R"hlsl(StructuredBuffer<Motor> Input:register(t0);RWStructuredBuffer<Motor> Output:register(u0);[numthreads(64,1,1)]void CSMain(uint3 id:SV_DispatchThreadID){if(id.x==0)Output[0]=Input[0];})hlsl":R"hlsl(StructuredBuffer<Motor> Input:register(t0);RWStructuredBuffer<Motor> Output:register(u0);[numthreads(64,1,1)]void CSMain(uint3 id:SV_DispatchThreadID){if(id.x!=0)return;[loop]for(uint j=0;j<BoneCount;++j){uint i=Order[j];if(Parent[i]<0)Output[i]=Input[i];else Output[i]=DqMul(Output[Parent[i]],Input[i]);}})hlsl";
    const std::string motorApply=R"hlsl(StructuredBuffer<Motor> Input:register(t0);RWStructuredBuffer<float4> Output:register(u0);[numthreads(64,1,1)]void CSMain(uint3 id:SV_DispatchThreadID){uint n=id.x;if(n>=BoneCount*ProbeCount)return;uint bone=n/ProbeCount;float3 p=Probe(n%ProbeCount);Motor m=Input[bone];float3 q=QMul(QMul(m.qr,float4(0,p)),QConj(m.qr)).yzw;float3 t=2*QMul(m.qd,QConj(m.qr)).yzw;Output[n]=float4(q+t,1);})hlsl";
    switch(group.kind)
    {
    case candidate::CandidateKindV1::MatrixHierarchy:if(!add(Unary("A0.LocalMotorToMatrix",h,bones,32,bones,48,toMatrix,cert))||!add(Unary("A1.MatrixHierarchy",h,bones,48,bones,48,matrixHierarchy,cert))||!add(Unary("A2.MatrixProbeApply",h,bones,48,points,16,matrixApply,cert)))return base::Result<FrozenHierarchyLeafGroupV1,LeafCompileErrorV1>::Failure(lastError);break;
    case candidate::CandidateKindV1::DirectPgaHierarchy:if(!add(Unary("B0.MotorHierarchy",h,bones,32,bones,32,motorHierarchy,cert))||!add(Unary("B1.DirectPgaProbeApply",h,bones,32,points,16,motorApply,cert)))return base::Result<FrozenHierarchyLeafGroupV1,LeafCompileErrorV1>::Failure(lastError);break;
    case candidate::CandidateKindV1::HybridHierarchy:if(!add(Unary("C0.MotorHierarchy",h,bones,32,bones,32,motorHierarchy,cert))||!add(Unary("C1.GlobalMotorToMatrix",h,bones,32,bones,48,toMatrix,cert))||!add(Unary("C2.MatrixProbeApply",h,bones,48,points,16,matrixApply,cert)))return base::Result<FrozenHierarchyLeafGroupV1,LeafCompileErrorV1>::Failure(lastError);break;
    default:return base::Result<FrozenHierarchyLeafGroupV1,LeafCompileErrorV1>::Failure(Err("authority","unknown candidate kind"));
    }
    return base::Result<FrozenHierarchyLeafGroupV1,LeafCompileErrorV1>::Success(std::move(group));
}

base::Result<void,LeafCompileErrorV1> ValidateFrozenHierarchyLeafV1(const FrozenHierarchyLeafV1& v)
{
    if(v.packageBytes.empty()||v.executionDigest==base::Digest256{}||v.fileDigest==base::Digest256{}||v.verifiedRepresentationCertificate==base::Digest256{})return base::Result<void,LeafCompileErrorV1>::Failure(Err("leaf","missing frozen evidence"));auto p=package::PackageReader::Read(v.packageBytes);if(!p)return base::Result<void,LeafCompileErrorV1>::Failure(Err("leaf",p.Error().message));if(p.Value().ExecutionDigest()!=v.executionDigest||p.Value().Header().fileDigest!=v.fileDigest)return base::Result<void,LeafCompileErrorV1>::Failure(Err("leaf","package digest mismatch"));return base::Result<void,LeafCompileErrorV1>::Success();
}
std::vector<std::byte> SerializeFrozenHierarchyLeafEvidenceV1(const FrozenHierarchyLeafV1& v){base::BinaryWriter w;w.WriteU32(static_cast<std::uint32_t>(v.role.size()));w.WriteBytes(std::as_bytes(std::span(v.role.data(),v.role.size())));w.WriteBytes(v.verifiedRepresentationCertificate);w.WriteBytes(v.executionDigest);w.WriteBytes(v.fileDigest);w.WriteU32(v.lockedDynamicSlot);w.WriteU32(v.lockedDynamicBytes);w.WriteU32(v.lockedReferenceSlot);w.WriteU32(v.lockedReferenceBytes);w.WriteU32(v.inputStride);w.WriteU32(v.outputStride);w.WriteU64(v.packageBytes.size());w.WriteBytes(v.packageBytes);return std::move(w).Take();}
}
