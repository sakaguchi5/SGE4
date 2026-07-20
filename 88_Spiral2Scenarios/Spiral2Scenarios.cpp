#include "Spiral2Scenarios.h"
#include "../03_SemanticBuilder/SemanticBuilder.h"
#include "../09_FrozenPackageCore/PackageReader.h"
#include "../12_SGE4_5Compiler/SGE4_5Compiler.h"
#include "../17_CompositionContract/CompositionContract.h"
#include "../18_CompositionPlan/CompositionPlan.h"
#include "../19_CompositionVerifier/CompositionVerifier.h"
#include "../22_CompositionRuntime/CompositionRuntime.h"
#include "../66_Spiral1LeafCompiler/Spiral1LeafCompiler.h"
#include "../84_HierarchyRepresentationPlanner/HierarchyRepresentationPlanner.h"
#include <array>
#include <bit>
#include <cmath>
#include <span>
#include <string_view>

namespace sge4_5::spiral2::scenarios
{
namespace
{
namespace sem=::sge4_5::semantic;namespace can=composition::canonical;
template<class T>base::Result<T,ScenarioErrorV1> Fail(std::string s,std::string m){return base::Result<T,ScenarioErrorV1>::Failure({std::move(s),std::move(m)});}
std::uint32_t OrderedFloatBits(float value){const auto bits=std::bit_cast<std::uint32_t>(value);if((bits&0x7fffffffu)==0u)return 0x80000000u;return (bits&0x80000000u)!=0u?~bits:(bits|0x80000000u);}
std::uint32_t UlpDistance(float a,float b){const auto aa=OrderedFloatBits(a),bb=OrderedFloatBits(b);return aa>bb?aa-bb:bb-aa;}
bool MetricNear(float actual,float expected){const float tolerance=1.0e-7f+1.0e-6f*std::max(1.0f,std::abs(expected));return std::isfinite(actual)&&std::abs(actual-expected)<=tolerance;}
base::Result<leaf::FrozenHierarchyLeafV1,ScenarioErrorV1> CompileObserver(std::uint32_t points)
{
    constexpr std::uint32_t RecordStride=304;sem::SemanticBuilder b;auto reference=b.AddExternalBuffer("S2.Observer.Reference",static_cast<std::uint64_t>(points)*16,16);auto a=b.AddExternalBuffer("S2.Observer.A",static_cast<std::uint64_t>(points)*16,16);auto d=b.AddExternalBuffer("S2.Observer.B",static_cast<std::uint64_t>(points)*16,16);auto c=b.AddExternalBuffer("S2.Observer.C",static_cast<std::uint64_t>(points)*16,16);auto o=b.AddExternalBuffer("S2.Observer.Records",static_cast<std::uint64_t>(points)*RecordStride,RecordStride);if(!reference||!a||!d||!c||!o)return Fail<leaf::FrozenHierarchyLeafV1>("observer/resources","resource construction failed");auto ur=b.AddUse(reference.Value(),sem::Effect::Read,sem::ViewRole::ShaderBuffer);auto ua=b.AddUse(a.Value(),sem::Effect::Read,sem::ViewRole::ShaderBuffer);auto ub=b.AddUse(d.Value(),sem::Effect::Read,sem::ViewRole::ShaderBuffer);auto uc=b.AddUse(c.Value(),sem::Effect::Read,sem::ViewRole::ShaderBuffer);auto uo=b.AddUse(o.Value(),sem::Effect::Write,sem::ViewRole::StorageBuffer);sem::ProgramInterface pi;pi.parameters={{{0},"Reference",sem::ProgramParameterKind::ReadOnlyBuffer,sem::ShaderStage::Compute,0,0,1},{{1},"A",sem::ProgramParameterKind::ReadOnlyBuffer,sem::ShaderStage::Compute,1,0,1},{{2},"B",sem::ProgramParameterKind::ReadOnlyBuffer,sem::ShaderStage::Compute,2,0,1},{{3},"C",sem::ProgramParameterKind::ReadOnlyBuffer,sem::ShaderStage::Compute,3,0,1},{{4},"Records",sem::ProgramParameterKind::UnorderedBuffer,sem::ShaderStage::Compute,0,0,1}};
    const std::string shader="static const uint PointCount="+std::to_string(points)+"u;\nstatic const float RefAbs="+std::to_string(contracts::ObservationAbsoluteToleranceV2)+"f;\nstatic const float RefRel="+std::to_string(contracts::ObservationRelativeToleranceV2)+"f;\nstatic const float PairAbs="+std::to_string(contracts::ObservationPairwiseAbsoluteToleranceV2)+"f;\nstatic const float PairRel="+std::to_string(contracts::ObservationPairwiseRelativeToleranceV2)+R"hlsl(f;
StructuredBuffer<float4>Reference:register(t0);StructuredBuffer<float4>A:register(t1);StructuredBuffer<float4>B:register(t2);StructuredBuffer<float4>C:register(t3);
struct ObservationRecordV2{
uint4 absARef;uint4 absBRef;uint4 absCRef;uint4 absAB;uint4 absAC;uint4 absBC;
uint4 relARef;uint4 relBRef;uint4 relCRef;uint4 relAB;uint4 relAC;uint4 relBC;
uint4 ulpARef;uint4 ulpBRef;uint4 ulpCRef;uint4 ulpAB;uint4 ulpAC;uint4 ulpBC;
uint4 flags;};RWStructuredBuffer<ObservationRecordV2>Records:register(u0);
float3 AE(float4 x,float4 y){return abs(x.xyz-y.xyz);}float3 RR(float4 x,float4 r){float3 scale=max(float3(1,1,1),abs(r.xyz));return AE(x,r)/scale;}float3 RP(float4 x,float4 y){float3 scale=max(float3(1,1,1),max(abs(x.xyz),abs(y.xyz)));return AE(x,y)/scale;}
uint OrderedFloatBitsGpu(float v){uint x=asuint(v);uint ordered=0x80000000u;if((x&0x7fffffffu)!=0u){ordered=(x&0x80000000u)!=0u?~x:(x|0x80000000u);}return ordered;}uint U1(float x,float y){uint a=OrderedFloatBitsGpu(x);uint b=OrderedFloatBitsGpu(y);return a>b?a-b:b-a;}uint3 U3(float4 x,float4 y){return uint3(U1(x.x,y.x),U1(x.y,y.y),U1(x.z,y.z));}
bool PR(float4 x,float4 r,float aa,float rr){float3 scale=max(float3(1,1,1),abs(r.xyz));return all(AE(x,r)<=aa+rr*scale);}bool PP(float4 x,float4 y,float aa,float rr){float3 scale=max(float3(1,1,1),max(abs(x.xyz),abs(y.xyz)));return all(AE(x,y)<=aa+rr*scale);}
[numthreads(64,1,1)]void CSMain(uint3 id:SV_DispatchThreadID){uint i=id.x;if(i>=PointCount)return;float4 r=Reference[i],a=A[i],b=B[i],c=C[i];ObservationRecordV2 q;
q.absARef=asuint(float4(AE(a,r),0));q.absBRef=asuint(float4(AE(b,r),0));q.absCRef=asuint(float4(AE(c,r),0));q.absAB=asuint(float4(AE(a,b),0));q.absAC=asuint(float4(AE(a,c),0));q.absBC=asuint(float4(AE(b,c),0));
q.relARef=asuint(float4(RR(a,r),0));q.relBRef=asuint(float4(RR(b,r),0));q.relCRef=asuint(float4(RR(c,r),0));q.relAB=asuint(float4(RP(a,b),0));q.relAC=asuint(float4(RP(a,c),0));q.relBC=asuint(float4(RP(b,c),0));
q.ulpARef=uint4(U3(a,r),0);q.ulpBRef=uint4(U3(b,r),0);q.ulpCRef=uint4(U3(c,r),0);q.ulpAB=uint4(U3(a,b),0);q.ulpAC=uint4(U3(a,c),0);q.ulpBC=uint4(U3(b,c),0);
uint finite=all(isfinite(r))&&all(isfinite(a))&&all(isfinite(b))&&all(isfinite(c))?1u:0u;uint homogeneous=asuint(r.w)==0x3f800000u&&asuint(a.w)==0x3f800000u&&asuint(b.w)==0x3f800000u&&asuint(c.w)==0x3f800000u?1u:0u;uint contractPass=PR(a,r,RefAbs,RefRel)&&PR(b,r,RefAbs,RefRel)&&PR(c,r,RefAbs,RefRel)&&PP(a,b,PairAbs,PairRel)&&PP(a,c,PairAbs,PairRel)&&PP(b,c,PairAbs,PairRel)?1u:0u;q.flags=uint4(finite,homogeneous,contractPass,i);Records[i]=q;})hlsl";
    auto program=b.AddComputeProgram("Spiral2.Observer.V2",std::move(pi),{shader,{}, {},"CSMain"});if(!program)return Fail<leaf::FrozenHierarchyLeafV1>("observer/program",program.Error());const std::array ops={sem::WorkOperand{sem::WorkOperandKind::ProgramParameter,ur.Value(),{0}},sem::WorkOperand{sem::WorkOperandKind::ProgramParameter,ua.Value(),{1}},sem::WorkOperand{sem::WorkOperandKind::ProgramParameter,ub.Value(),{2}},sem::WorkOperand{sem::WorkOperandKind::ProgramParameter,uc.Value(),{3}},sem::WorkOperand{sem::WorkOperandKind::ProgramParameter,uo.Value(),{4}}};const std::uint32_t dispatch=(points+63u)/64u;auto work=b.AddComputeWorkGeneric("Spiral2.Observer.Work.V2",program.Value(),ops,dispatch,1,1);if(!work)return Fail<leaf::FrozenHierarchyLeafV1>("observer/work",work.Error());auto profile=spiral1::leaf::CanonicalLeafD3D12TargetProfileV1();profile.shaderDescriptorCount=8;auto compiled=compiler::CompileCanonical(std::move(b).Build(),profile);if(!compiled)return Fail<leaf::FrozenHierarchyLeafV1>(compiled.Error().stage,compiled.Error().message);leaf::FrozenHierarchyLeafV1 out;out.role="L9.Observer";constexpr std::string_view authority="S2.Observer.ObservationContractAuthority.V2";out.verifiedRepresentationCertificate=base::Sha256(std::as_bytes(std::span(authority.data(),authority.size())));out.verifiedOperationIdentity=out.verifiedRepresentationCertificate;constexpr std::string_view observerProgram="S2.Observer.ProgramTemplate.V2";out.verifiedProgramTemplateIdentity=base::Sha256(std::as_bytes(std::span(observerProgram.data(),observerProgram.size())));out.operation=static_cast<candidate::OperationKindV1>(0);out.operationOrdinal=0;out.plannedDispatchX=dispatch;out.primaryInputElementCount=points;out.primaryOutputElementCount=points;out.inputStride=16;out.outputStride=RecordStride;out.expectedExternalSlotCount=5;out.packageBytes=std::move(compiled.Value().packageBytes);auto sealed=leaf::SealFrozenHierarchyLeafBindingV1(out);if(!sealed)return Fail<leaf::FrozenHierarchyLeafV1>(sealed.Error().stage,sealed.Error().message);return base::Result<leaf::FrozenHierarchyLeafV1,ScenarioErrorV1>::Success(std::move(out));
}
can::EndpointReferenceDeclaration Ref(const std::string& leaf,const std::string& endpoint){return {leaf,endpoint};}
can::LeafPackageDeclaration Decl(const leaf::FrozenHierarchyLeafV1& item,std::initializer_list<can::LeafEndpointDeclaration> endpoints){can::LeafPackageDeclaration d;d.stableKey=item.role;d.packageBytes=item.packageBytes;d.endpoints=endpoints;return d;}
can::LeafPackageId FindLeaf(const can::PackageCompositionContract& contract,std::string_view authorKey) noexcept
{
    const auto key=can::ComputeStableLeafKey(authorKey);for(const auto& item:contract.leaves)if(item.stableKey==key)return item.id;return {};
}
can::ResourceFlowId FindFlow(const can::PackageCompositionContract& contract,std::string_view authorKey) noexcept
{
    const auto key=can::ComputeStableResourceKey(authorKey);for(const auto& item:contract.resources)if(item.stableKey==key)return item.id;return {};
}
base::Result<std::vector<corpus::ObservedPointV1>,ScenarioErrorV1> DecodePoints(std::span<const std::byte> bytes,std::uint32_t expected)
{
    if(bytes.size()!=static_cast<std::size_t>(expected)*sizeof(corpus::ObservedPointV1))return Fail<std::vector<corpus::ObservedPointV1>>("scenario/readback","observation byte count is invalid");base::BinaryReader reader(bytes);std::vector<corpus::ObservedPointV1> out;out.reserve(expected);for(std::uint32_t i=0;i<expected;++i){auto x=reader.ReadU32();auto y=reader.ReadU32();auto z=reader.ReadU32();auto w=reader.ReadU32();if(!x||!y||!z||!w)return Fail<std::vector<corpus::ObservedPointV1>>("scenario/readback","observation encoding is truncated");out.push_back({std::bit_cast<float>(x.Value()),std::bit_cast<float>(y.Value()),std::bit_cast<float>(z.Value()),std::bit_cast<float>(w.Value())});}return base::Result<std::vector<corpus::ObservedPointV1>,ScenarioErrorV1>::Success(std::move(out));
}
}

base::Result<FrozenHierarchyComparisonScenarioV1,ScenarioErrorV1> BuildFrozenHierarchyComparisonScenarioV1(const hierarchy::RigidHierarchySemanticV1& h)
{
    auto valid=hierarchy::ValidateRigidHierarchySemanticV1(h);if(!valid)return Fail<FrozenHierarchyComparisonScenarioV1>("hierarchy",valid.Error());const std::array<candidate::CandidateKindV1,3> kinds{candidate::CandidateKindV1::MatrixHierarchy,candidate::CandidateKindV1::DirectPgaHierarchy,candidate::CandidateKindV1::HybridHierarchy};FrozenHierarchyComparisonScenarioV1 out;out.hierarchyIdentity=hierarchy::RigidHierarchySemanticIdentityV1(h);out.boneCount=h.boneCount;
    for(std::size_t i=0;i<kinds.size();++i){auto raw=planner::PlanCandidateGraphV1(h,kinds[i]);if(!raw)return Fail<FrozenHierarchyComparisonScenarioV1>("planner",raw.Error());auto verified=verification::VerifyCandidateGraphV1(h,raw.Value());if(!verified)return Fail<FrozenHierarchyComparisonScenarioV1>("verifier",verified.Error());if(i==0){auto source=leaf::CompileDynamicInvocationSourceV1(h,verified.Value());if(!source)return Fail<FrozenHierarchyComparisonScenarioV1>(source.Error().stage,source.Error().message);out.leaves.push_back(std::move(source.Value()));}auto group=leaf::CompileVerifiedHierarchyLeafGroupV1(h,verified.Value());if(!group)return Fail<FrozenHierarchyComparisonScenarioV1>(group.Error().stage,group.Error().message);for(auto& item:group.Value().leaves)out.leaves.push_back(std::move(item));}
    auto observer=CompileObserver(h.boneCount*5);if(!observer)return base::Result<FrozenHierarchyComparisonScenarioV1,ScenarioErrorV1>::Failure(observer.Error());out.leaves.push_back(std::move(observer.Value()));if(out.leaves.size()!=10)return Fail<FrozenHierarchyComparisonScenarioV1>("leaves","comparison scenario must contain ten Leaves");
    can::ContractBuildInput input;input.leaves={Decl(out.leaves[0],{{0,"output"},{1,"reference"}}),Decl(out.leaves[1],{{0,"input"},{1,"output"}}),Decl(out.leaves[2],{{0,"input"},{1,"output"}}),Decl(out.leaves[3],{{0,"input"},{1,"output"}}),Decl(out.leaves[4],{{0,"input"},{1,"output"}}),Decl(out.leaves[5],{{0,"input"},{1,"output"}}),Decl(out.leaves[6],{{0,"input"},{1,"output"}}),Decl(out.leaves[7],{{0,"input"},{1,"output"}}),Decl(out.leaves[8],{{0,"input"},{1,"output"}}),Decl(out.leaves[9],{{0,"reference"},{1,"a"},{2,"b"},{3,"c"},{4,"records"}})};
    auto flow=[&](std::string key,std::string producer,std::string producerEndpoint,std::initializer_list<can::EndpointReferenceDeclaration> consumers,can::ResourceBoundary boundary=can::ResourceBoundary::Internal){can::ResourceFlowDeclaration f;f.stableKey=std::move(key);f.boundary=boundary;f.producer=Ref(producer,producerEndpoint);f.consumers=consumers;input.resources.push_back(std::move(f));};
    flow("s2/motors",out.leaves[0].role,"output",{Ref(out.leaves[1].role,"input"),Ref(out.leaves[4].role,"input"),Ref(out.leaves[6].role,"input")});flow("s2/reference",out.leaves[0].role,"reference",{Ref(out.leaves[9].role,"reference")});flow("s2/a/local-matrix",out.leaves[1].role,"output",{Ref(out.leaves[2].role,"input")});flow("s2/a/global-matrix",out.leaves[2].role,"output",{Ref(out.leaves[3].role,"input")});flow("s2/a/observations",out.leaves[3].role,"output",{Ref(out.leaves[9].role,"a")});flow("s2/b/global-motor",out.leaves[4].role,"output",{Ref(out.leaves[5].role,"input")});flow("s2/b/observations",out.leaves[5].role,"output",{Ref(out.leaves[9].role,"b")});flow("s2/c/global-motor",out.leaves[6].role,"output",{Ref(out.leaves[7].role,"input")});flow("s2/c/global-matrix",out.leaves[7].role,"output",{Ref(out.leaves[8].role,"input")});flow("s2/c/observations",out.leaves[8].role,"output",{Ref(out.leaves[9].role,"c")});flow("s2/records",out.leaves[9].role,"records",{},can::ResourceBoundary::CompositionOutput);
    auto contract=can::BuildCompositionContract(std::move(input));if(!contract)return Fail<FrozenHierarchyComparisonScenarioV1>(contract.Error().stage,contract.Error().message);auto plan=can::planning::ProposeCompositionPlan(contract.Value());if(!plan)return Fail<FrozenHierarchyComparisonScenarioV1>(plan.Error().stage,plan.Error().message);auto verifiedPlan=can::verification::VerifyAndSeal(contract.Value(),plan.Value());if(!verifiedPlan)return Fail<FrozenHierarchyComparisonScenarioV1>(verifiedPlan.Error().stage,verifiedPlan.Error().message);auto frozen=can::verification::FreezeVerifiedComposition(contract.Value(),verifiedPlan.Value());if(!frozen)return Fail<FrozenHierarchyComparisonScenarioV1>(frozen.Error().stage,frozen.Error().message);out.frozenCompositionBytes=std::move(frozen.Value());out.frozenCompositionDigest=base::Sha256(out.frozenCompositionBytes);return base::Result<FrozenHierarchyComparisonScenarioV1,ScenarioErrorV1>::Success(std::move(out));
}
base::Result<void,ScenarioErrorV1> ValidateFrozenHierarchyComparisonScenarioV1(const FrozenHierarchyComparisonScenarioV1& s){if(s.leaves.size()!=10||s.frozenCompositionDigest!=base::Sha256(s.frozenCompositionBytes))return Fail<void>("scenario","scenario evidence mismatch");for(const auto& item:s.leaves){auto v=leaf::ValidateFrozenHierarchyLeafV1(item);if(!v)return Fail<void>(v.Error().stage,v.Error().message);}auto frozen=can::verification::ReadVerifiedFrozenComposition(s.frozenCompositionBytes);if(!frozen)return Fail<void>(frozen.Error().stage,frozen.Error().message);if(frozen.Value().ValidatedContract().Contract().leaves.size()!=10||frozen.Value().ValidatedContract().Contract().resources.size()!=11)return Fail<void>("scenario","Frozen Composition topology mismatch");return base::Result<void,ScenarioErrorV1>::Success();}
base::Result<HierarchyComparisonExecutionResultV1,ScenarioErrorV1> ExecuteLoadedHierarchyComparisonScenarioV1(const FrozenHierarchyComparisonScenarioV1& s,const hierarchy::RigidHierarchySemanticV1& h,const contracts::DynamicLocalMotorPaletteV1& palette,can::runtime_v1::LoadedStaticComposition& loaded,std::uint64_t frameNumber)
{
    auto scenarioValid=ValidateFrozenHierarchyComparisonScenarioV1(s);
    if(!scenarioValid)return base::Result<HierarchyComparisonExecutionResultV1,ScenarioErrorV1>::Failure(scenarioValid.Error());
    if(s.hierarchyIdentity!=hierarchy::RigidHierarchySemanticIdentityV1(h)||s.boneCount!=h.boneCount)return Fail<HierarchyComparisonExecutionResultV1>("scenario/runtime","hierarchy does not match the Frozen scenario");
    auto paletteValid=contracts::ValidateDynamicLocalMotorPaletteV1(palette,h.boneCount);
    if(!paletteValid)return Fail<HierarchyComparisonExecutionResultV1>("scenario/runtime",paletteValid.Error());
    auto reference=corpus::BuildIndependentCpuReferenceV1(h,palette);
    if(!reference)return Fail<HierarchyComparisonExecutionResultV1>("scenario/reference",reference.Error());
    if(base::Sha256(loaded.Artifact().Artifact().FileBytes())!=s.frozenCompositionDigest)return Fail<HierarchyComparisonExecutionResultV1>("scenario/runtime","loaded Frozen Composition does not match scenario identity");
    const auto& contract=loaded.Artifact().ValidatedContract().Contract();
    const auto source=FindLeaf(contract,s.leaves[0].role);
    const auto referenceFlow=FindFlow(contract,"s2/reference"),aFlow=FindFlow(contract,"s2/a/observations"),bFlow=FindFlow(contract,"s2/b/observations"),cFlow=FindFlow(contract,"s2/c/observations"),recordsFlow=FindFlow(contract,"s2/records");
    if(!source.IsValid()||!referenceFlow.IsValid()||!aFlow.IsValid()||!bFlow.IsValid()||!cFlow.IsValid()||!recordsFlow.IsValid())return Fail<HierarchyComparisonExecutionResultV1>("scenario/runtime","canonical Leaf or Resource Flow identity was not found");
    namespace runtime=can::runtime_v1;
    runtime::StaticCompositionFrameInvocation invocation;
    invocation.frameNumber=frameNumber;
    invocation.dynamicData={{source,s.leaves[0].lockedDynamicSlot,contracts::SerializeDynamicLocalMotorPaletteV1(palette)},{source,s.leaves[0].lockedReferenceSlot,corpus::SerializeReferencePointsV1(reference.Value())}};
    auto submitted=runtime::SubmitStaticComposition(loaded,invocation);
    if(!submitted)return Fail<HierarchyComparisonExecutionResultV1>(submitted.Error().stage,submitted.Error().message);
    if(submitted.Value().executionOrder.size()!=10||submitted.Value().leaves.size()!=10)return Fail<HierarchyComparisonExecutionResultV1>("scenario/runtime","Frozen Composition did not execute exactly ten Leaves");
    auto referenceRead=runtime::ReadStaticCompositionBuffer(loaded,referenceFlow),a=runtime::ReadStaticCompositionBuffer(loaded,aFlow),b=runtime::ReadStaticCompositionBuffer(loaded,bFlow),c=runtime::ReadStaticCompositionBuffer(loaded,cFlow),records=runtime::ReadStaticCompositionBuffer(loaded,recordsFlow);
    if(!referenceRead||!a||!b||!c||!records){const auto* error=!referenceRead?&referenceRead.Error():!a?&a.Error():!b?&b.Error():!c?&c.Error():&records.Error();return Fail<HierarchyComparisonExecutionResultV1>(error->stage,error->message);}
    const auto referenceBytes=corpus::SerializeReferencePointsV1(reference.Value());
    if(referenceRead.Value().bytes!=referenceBytes)return Fail<HierarchyComparisonExecutionResultV1>("scenario/source-observation","Dynamic source did not reproduce independent reference bytes");
    const auto points=h.boneCount*5;
    auto ap=DecodePoints(a.Value().bytes,points),bp=DecodePoints(b.Value().bytes,points),cp=DecodePoints(c.Value().bytes,points);
    if(!ap)return base::Result<HierarchyComparisonExecutionResultV1,ScenarioErrorV1>::Failure(ap.Error());
    if(!bp)return base::Result<HierarchyComparisonExecutionResultV1,ScenarioErrorV1>::Failure(bp.Error());
    if(!cp)return base::Result<HierarchyComparisonExecutionResultV1,ScenarioErrorV1>::Failure(cp.Error());
    auto observed=observer::ObserveHierarchyV1(reference.Value(),ap.Value(),bp.Value(),cp.Value());
    if(!observed)return Fail<HierarchyComparisonExecutionResultV1>("scenario/observer",observed.Error());
    if(observed.Value().mismatchCount!=0||observed.Value().nonFiniteCount!=0)return Fail<HierarchyComparisonExecutionResultV1>("scenario/observer","WARP observations violate the independent reference or representation agreement: max-reference="+std::to_string(observed.Value().maxAbsoluteError)+", max-pairwise="+std::to_string(observed.Value().maxPairwiseError)+", max-axis="+std::to_string(observed.Value().maxAxisLengthError)+", max-dot="+std::to_string(observed.Value().maxOrthogonalityError));
    constexpr std::uint32_t gpuRecordStride=304;if(records.Value().bytes.size()!=static_cast<std::size_t>(points)*gpuRecordStride)return Fail<HierarchyComparisonExecutionResultV1>("scenario/observer","GPU observer V2 record byte count is invalid");
    base::BinaryReader recordReader(records.Value().bytes);
    for(std::uint32_t i=0;i<points;++i)
    {
        std::array<std::uint32_t,76> words{};for(auto& word:words){auto value=recordReader.ReadU32();if(!value)return Fail<HierarchyComparisonExecutionResultV1>("scenario/observer","GPU observer V2 record encoding is truncated");word=value.Value();}
        const std::array<const corpus::ObservedPointV1*,6> left{&ap.Value()[i],&bp.Value()[i],&cp.Value()[i],&ap.Value()[i],&ap.Value()[i],&bp.Value()[i]};
        const std::array<const corpus::ObservedPointV1*,6> right{&reference.Value()[i],&reference.Value()[i],&reference.Value()[i],&bp.Value()[i],&cp.Value()[i],&cp.Value()[i]};
        for(std::size_t relation=0;relation<left.size();++relation)
        {
            const std::array<float,3> lhs{left[relation]->x,left[relation]->y,left[relation]->z},rhs{right[relation]->x,right[relation]->y,right[relation]->z};
            for(std::size_t component=0;component<3;++component)
            {
                const float expectedAbsolute=std::abs(lhs[component]-rhs[component]);
                const float scale=relation<3?std::max(1.0f,std::abs(rhs[component])):std::max({1.0f,std::abs(lhs[component]),std::abs(rhs[component])});
                const float expectedRelative=expectedAbsolute/scale;
                const float gpuAbsolute=std::bit_cast<float>(words[relation*4+component]);
                const float gpuRelative=std::bit_cast<float>(words[24+relation*4+component]);
                if(!MetricNear(gpuAbsolute,expectedAbsolute)||!MetricNear(gpuRelative,expectedRelative)||words[48+relation*4+component]!=UlpDistance(lhs[component],rhs[component]))return Fail<HierarchyComparisonExecutionResultV1>("scenario/observer","GPU observer V2 component metric does not match CPU reconstruction");
            }
            if(words[relation*4+3]!=0||words[24+relation*4+3]!=0||words[48+relation*4+3]!=0)return Fail<HierarchyComparisonExecutionResultV1>("scenario/observer","GPU observer V2 reserved component is nonzero");
        }
        if(words[72]!=1||words[73]!=1||words[74]!=1||words[75]!=i)return Fail<HierarchyComparisonExecutionResultV1>("scenario/observer","GPU observer V2 flags or point identity failed");
    }
    HierarchyComparisonExecutionResultV1 out;out.matrix=std::move(ap.Value());out.direct=std::move(bp.Value());out.hybrid=std::move(cp.Value());out.observation=observed.Value();out.gpuRecords=std::move(records.Value().bytes);out.deviceEpoch=submitted.Value().deviceEpoch;
    return base::Result<HierarchyComparisonExecutionResultV1,ScenarioErrorV1>::Success(std::move(out));
}
base::Result<HierarchyComparisonExecutionResultV1,ScenarioErrorV1> ExecuteFrozenHierarchyComparisonScenarioV1(const FrozenHierarchyComparisonScenarioV1& s,const hierarchy::RigidHierarchySemanticV1& h,const contracts::DynamicLocalMotorPaletteV1& palette,d3d12::D3D12Backend& backend,std::uint64_t frameNumber)
{
    auto loaded=can::runtime_v1::LoadStaticComposition(s.frozenCompositionBytes,backend);if(!loaded)return Fail<HierarchyComparisonExecutionResultV1>(loaded.Error().stage,loaded.Error().message);return ExecuteLoadedHierarchyComparisonScenarioV1(s,h,palette,loaded.Value(),frameNumber);
}
base::Result<HierarchyCorpusExecutionResultV1,ScenarioErrorV1> ExecuteFrozenHierarchyCorpusScenarioV1(const FrozenHierarchyComparisonScenarioV1& s,const hierarchy::RigidHierarchySemanticV1& h,std::span<const corpus::FrameCaseV1> frames,d3d12::D3D12Backend& backend,std::uint64_t firstFrameNumber)
{
    if(frames.empty())return Fail<HierarchyCorpusExecutionResultV1>("scenario/corpus","frame corpus is empty");
    auto loaded=can::runtime_v1::LoadStaticComposition(s.frozenCompositionBytes,backend);
    if(!loaded)return Fail<HierarchyCorpusExecutionResultV1>(loaded.Error().stage,loaded.Error().message);
    HierarchyCorpusExecutionResultV1 out;out.frozenDigestBefore=base::Sha256(loaded.Value().Artifact().Artifact().FileBytes());std::uint64_t epoch=0;out.frames.reserve(frames.size());
    for(std::size_t i=0;i<frames.size();++i){auto observed=ExecuteLoadedHierarchyComparisonScenarioV1(s,h,frames[i].palette,loaded.Value(),firstFrameNumber+i);if(!observed)return Fail<HierarchyCorpusExecutionResultV1>(observed.Error().stage+"/"+frames[i].id,observed.Error().message);if(i==0)epoch=observed.Value().deviceEpoch;else if(observed.Value().deviceEpoch!=epoch)return Fail<HierarchyCorpusExecutionResultV1>("scenario/corpus","device epoch changed during multi-frame execution");out.frames.push_back(std::move(observed.Value()));}
    out.frozenDigestAfter=base::Sha256(loaded.Value().Artifact().Artifact().FileBytes());if(out.frozenDigestBefore!=out.frozenDigestAfter||out.frozenDigestBefore!=s.frozenCompositionDigest)return Fail<HierarchyCorpusExecutionResultV1>("scenario/corpus","Frozen Composition changed across dynamic frames");return base::Result<HierarchyCorpusExecutionResultV1,ScenarioErrorV1>::Success(std::move(out));
}
std::vector<std::byte> SerializeFrozenHierarchyComparisonScenarioEvidenceV1(const FrozenHierarchyComparisonScenarioV1& s){base::BinaryWriter w;w.WriteBytes(s.hierarchyIdentity);w.WriteU32(s.boneCount);w.WriteU32(static_cast<std::uint32_t>(s.leaves.size()));for(const auto& item:s.leaves){auto bytes=leaf::SerializeFrozenHierarchyLeafEvidenceV1(item);w.WriteU64(bytes.size());w.WriteBytes(bytes);}w.WriteU64(s.frozenCompositionBytes.size());w.WriteBytes(s.frozenCompositionBytes);w.WriteBytes(s.frozenCompositionDigest);return std::move(w).Take();}
}
