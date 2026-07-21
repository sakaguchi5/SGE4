#include "Spiral3Scenarios.h"
#include "../03_SemanticBuilder/SemanticBuilder.h"
#include "../12_SGE4_5Compiler/SGE4_5Compiler.h"
#include "../18_CompositionPlan/CompositionPlan.h"
#include "../19_CompositionVerifier/CompositionVerifier.h"
#include "../66_Spiral1LeafCompiler/Spiral1LeafCompiler.h"
#include "../81_Spiral2Contracts/Spiral2Contracts.h"
#include "../104_ReuseRepresentationPlanner/ReuseRepresentationPlanner.h"
#include "../105_ReuseRepresentationVerifier/ReuseRepresentationVerifier.h"
#include <algorithm>
#include <array>
#include <span>
#include <string_view>

namespace sge4_5::spiral3::scenarios
{
namespace
{
namespace sem=::sge4_5::semantic;
namespace can=composition::canonical;
template<class T> base::Result<T,ScenarioErrorV1> Fail(std::string stage,std::string message)
{return base::Result<T,ScenarioErrorV1>::Failure({std::move(stage),std::move(message)});}
base::Digest256 Lit(std::string_view value){return base::Sha256(std::as_bytes(std::span(value.data(),value.size())));}
can::EndpointReferenceDeclaration Ref(const std::string& leaf,const std::string& endpoint){return {leaf,endpoint};}
can::LeafPackageDeclaration Decl(const leaf::FrozenReuseLeafV1& item,std::initializer_list<can::LeafEndpointDeclaration> endpoints)
{can::LeafPackageDeclaration value;value.stableKey=item.role;value.packageBytes=item.packageBytes;value.endpoints=endpoints;return value;}

base::Result<leaf::FrozenReuseLeafV1,ScenarioErrorV1> CompileObserver(const semantic::ReuseSweepSemanticV1& reuseSemantic)
{
    const std::uint32_t points=reuseSemantic.pointCount;
    constexpr std::uint32_t recordStride=304;
    sem::SemanticBuilder b;
    auto reference=b.AddExternalBuffer("Spiral3.Observer.Reference",static_cast<std::uint64_t>(points)*16,16);
    auto a=b.AddExternalBuffer("Spiral3.Observer.A",static_cast<std::uint64_t>(points)*16,16);
    auto d=b.AddExternalBuffer("Spiral3.Observer.B",static_cast<std::uint64_t>(points)*16,16);
    auto c=b.AddExternalBuffer("Spiral3.Observer.C",static_cast<std::uint64_t>(points)*16,16);
    auto records=b.AddExternalBuffer("Spiral3.Observer.Records",static_cast<std::uint64_t>(points)*recordStride,recordStride);
    if(!reference||!a||!d||!c||!records)return Fail<leaf::FrozenReuseLeafV1>("observer/resources","resource construction failed");
    auto ur=b.AddUse(reference.Value(),sem::Effect::Read,sem::ViewRole::ShaderBuffer);auto ua=b.AddUse(a.Value(),sem::Effect::Read,sem::ViewRole::ShaderBuffer);auto ub=b.AddUse(d.Value(),sem::Effect::Read,sem::ViewRole::ShaderBuffer);auto uc=b.AddUse(c.Value(),sem::Effect::Read,sem::ViewRole::ShaderBuffer);auto uo=b.AddUse(records.Value(),sem::Effect::Write,sem::ViewRole::StorageBuffer);
    if(!ur||!ua||!ub||!uc||!uo)return Fail<leaf::FrozenReuseLeafV1>("observer/uses","resource use construction failed");
    sem::ProgramInterface pi;pi.parameters={{{0},"Reference",sem::ProgramParameterKind::ReadOnlyBuffer,sem::ShaderStage::Compute,0,0,1},{{1},"A",sem::ProgramParameterKind::ReadOnlyBuffer,sem::ShaderStage::Compute,1,0,1},{{2},"B",sem::ProgramParameterKind::ReadOnlyBuffer,sem::ShaderStage::Compute,2,0,1},{{3},"C",sem::ProgramParameterKind::ReadOnlyBuffer,sem::ShaderStage::Compute,3,0,1},{{4},"Records",sem::ProgramParameterKind::UnorderedBuffer,sem::ShaderStage::Compute,0,0,1}};
    const std::string shader="static const uint PointCount="+std::to_string(points)+"u;\nstatic const float RefAbs="+std::to_string(spiral2::contracts::ObservationAbsoluteToleranceV2)+"f;\nstatic const float RefRel="+std::to_string(spiral2::contracts::ObservationRelativeToleranceV2)+"f;\nstatic const float PairAbs="+std::to_string(spiral2::contracts::ObservationPairwiseAbsoluteToleranceV2)+"f;\nstatic const float PairRel="+std::to_string(spiral2::contracts::ObservationPairwiseRelativeToleranceV2)+R"hlsl(f;
StructuredBuffer<float4>Reference:register(t0);StructuredBuffer<float4>A:register(t1);StructuredBuffer<float4>B:register(t2);StructuredBuffer<float4>C:register(t3);
struct ObservationRecordV2{uint4 absARef;uint4 absBRef;uint4 absCRef;uint4 absAB;uint4 absAC;uint4 absBC;uint4 relARef;uint4 relBRef;uint4 relCRef;uint4 relAB;uint4 relAC;uint4 relBC;uint4 ulpARef;uint4 ulpBRef;uint4 ulpCRef;uint4 ulpAB;uint4 ulpAC;uint4 ulpBC;uint4 flags;};RWStructuredBuffer<ObservationRecordV2>Records:register(u0);
float3 AE(float4 x,float4 y){return abs(x.xyz-y.xyz);}float3 RR(float4 x,float4 r){float3 scale=max(float3(1,1,1),abs(r.xyz));return AE(x,r)/scale;}float3 RP(float4 x,float4 y){float3 scale=max(float3(1,1,1),max(abs(x.xyz),abs(y.xyz)));return AE(x,y)/scale;}
uint OrderedFloatBitsGpu(float v){uint x=asuint(v);uint ordered=0x80000000u;if((x&0x7fffffffu)!=0u){ordered=(x&0x80000000u)!=0u?~x:(x|0x80000000u);}return ordered;}uint U1(float x,float y){uint a=OrderedFloatBitsGpu(x);uint b=OrderedFloatBitsGpu(y);return a>b?a-b:b-a;}uint3 U3(float4 x,float4 y){return uint3(U1(x.x,y.x),U1(x.y,y.y),U1(x.z,y.z));}
bool PR(float4 x,float4 r,float aa,float rr){float3 scale=max(float3(1,1,1),abs(r.xyz));return all(AE(x,r)<=aa+rr*scale);}bool PP(float4 x,float4 y,float aa,float rr){float3 scale=max(float3(1,1,1),max(abs(x.xyz),abs(y.xyz)));return all(AE(x,y)<=aa+rr*scale);}
[numthreads(64,1,1)]void CSMain(uint3 id:SV_DispatchThreadID){uint i=id.x;if(i>=PointCount)return;float4 r=Reference[i],a=A[i],b=B[i],c=C[i];ObservationRecordV2 q;q.absARef=asuint(float4(AE(a,r),0));q.absBRef=asuint(float4(AE(b,r),0));q.absCRef=asuint(float4(AE(c,r),0));q.absAB=asuint(float4(AE(a,b),0));q.absAC=asuint(float4(AE(a,c),0));q.absBC=asuint(float4(AE(b,c),0));q.relARef=asuint(float4(RR(a,r),0));q.relBRef=asuint(float4(RR(b,r),0));q.relCRef=asuint(float4(RR(c,r),0));q.relAB=asuint(float4(RP(a,b),0));q.relAC=asuint(float4(RP(a,c),0));q.relBC=asuint(float4(RP(b,c),0));q.ulpARef=uint4(U3(a,r),0);q.ulpBRef=uint4(U3(b,r),0);q.ulpCRef=uint4(U3(c,r),0);q.ulpAB=uint4(U3(a,b),0);q.ulpAC=uint4(U3(a,c),0);q.ulpBC=uint4(U3(b,c),0);uint finite=all(isfinite(r))&&all(isfinite(a))&&all(isfinite(b))&&all(isfinite(c));uint homogeneous=asuint(r.w)==0x3f800000u&&asuint(a.w)==0x3f800000u&&asuint(b.w)==0x3f800000u&&asuint(c.w)==0x3f800000u;uint passFlag=PR(a,r,RefAbs,RefRel)&&PR(b,r,RefAbs,RefRel)&&PR(c,r,RefAbs,RefRel)&&PP(a,b,PairAbs,PairRel)&&PP(a,c,PairAbs,PairRel)&&PP(b,c,PairAbs,PairRel);q.flags=uint4(finite,homogeneous,passFlag,i);Records[i]=q;})hlsl";
    auto program=b.AddComputeProgram("Spiral3.Observer.Program.V1",std::move(pi),{shader,{}, {},"CSMain"});if(!program)return Fail<leaf::FrozenReuseLeafV1>("observer/program",program.Error());
    const std::array operands={sem::WorkOperand{sem::WorkOperandKind::ProgramParameter,ur.Value(),{0}},sem::WorkOperand{sem::WorkOperandKind::ProgramParameter,ua.Value(),{1}},sem::WorkOperand{sem::WorkOperandKind::ProgramParameter,ub.Value(),{2}},sem::WorkOperand{sem::WorkOperandKind::ProgramParameter,uc.Value(),{3}},sem::WorkOperand{sem::WorkOperandKind::ProgramParameter,uo.Value(),{4}}};const std::uint32_t dispatch=(points+63u)/64u;auto work=b.AddComputeWorkGeneric("Spiral3.Observer.Work.V1",program.Value(),operands,dispatch,1,1);if(!work)return Fail<leaf::FrozenReuseLeafV1>("observer/work",work.Error());
    auto profile=spiral1::leaf::CanonicalLeafD3D12TargetProfileV1();profile.shaderDescriptorCount=8;auto compiled=compiler::CompileCanonical(std::move(b).Build(),profile);if(!compiled)return Fail<leaf::FrozenReuseLeafV1>(compiled.Error().stage,compiled.Error().message);
    leaf::FrozenReuseLeafV1 out;out.role=std::string(ObserverLeafRoleV1);out.reuseSemanticIdentity=semantic::ReuseSweepSemanticIdentityV1(reuseSemantic);out.localPointCorpusIdentity=reuseSemantic.localPointCorpusIdentity;out.verifiedRepresentationCertificate=Lit("SGE4-5.Spiral3.Observer.Authority.V1");base::BinaryWriter planWriter;planWriter.WriteBytes(out.verifiedRepresentationCertificate);planWriter.WriteBytes(out.reuseSemanticIdentity);planWriter.WriteU32(points);out.verifiedPlanIdentity=base::Sha256(std::move(planWriter).Take());out.verifiedOperationIdentity=Lit("SGE4-5.Spiral3.Observer.Operation.V1");out.verifiedProgramTemplateIdentity=Lit("SGE4-5.Spiral3.Observer.ProgramTemplate.V1");out.candidateKind=static_cast<candidate::CandidateKindV1>(0);out.loweringPosition=static_cast<candidate::MatrixLoweringPositionV1>(0);out.operation=static_cast<candidate::OperationKindV1>(0);out.reuseCount=reuseSemantic.reuseCount;out.pointCount=points;out.plannedDispatchX=dispatch;out.primaryInputStride=16;out.primaryInputElementCount=points;out.primaryOutputStride=recordStride;out.primaryOutputElementCount=points;out.expectedExternalSlotCount=5;out.packageBytes=std::move(compiled.Value().packageBytes);auto sealed=leaf::SealFrozenReuseLeafBindingV1(out);if(!sealed)return Fail<leaf::FrozenReuseLeafV1>(sealed.Error().stage,sealed.Error().message);return base::Result<leaf::FrozenReuseLeafV1,ScenarioErrorV1>::Success(std::move(out));
}

can::ContractBuildInput BuildInput(const std::vector<leaf::FrozenReuseLeafV1>& l)
{
    can::ContractBuildInput input;
    input.leaves={Decl(l[0],{{0,"motors"},{1,"reference"}}),Decl(l[1],{{0,"points"}}),Decl(l[2],{{0,"input"},{1,"output"}}),Decl(l[3],{{0,"input"},{1,"output"}}),Decl(l[4],{{0,"transforms"},{1,"points"},{2,"output"}}),Decl(l[5],{{0,"input"},{1,"output"}}),Decl(l[6],{{0,"transforms"},{1,"points"},{2,"output"}}),Decl(l[7],{{0,"input"},{1,"output"}}),Decl(l[8],{{0,"input"},{1,"output"}}),Decl(l[9],{{0,"transforms"},{1,"points"},{2,"output"}}),Decl(l[10],{{0,"reference"},{1,"a"},{2,"b"},{3,"c"},{4,"records"}})};
    auto flow=[&](std::string key,std::string producer,std::string endpoint,std::initializer_list<can::EndpointReferenceDeclaration> consumers,can::ResourceBoundary boundary=can::ResourceBoundary::Internal){can::ResourceFlowDeclaration f;f.stableKey=std::move(key);f.boundary=boundary;f.producer=Ref(producer,endpoint);f.consumers=consumers;input.resources.push_back(std::move(f));};
    flow(std::string(MotorsFlowKeyV1),l[0].role,"motors",{Ref(l[2].role,"input"),Ref(l[5].role,"input"),Ref(l[7].role,"input")});
    flow(std::string(ReferenceFlowKeyV1),l[0].role,"reference",{Ref(l[10].role,"reference")});
    flow(std::string(PointsFlowKeyV1),l[1].role,"points",{Ref(l[4].role,"points"),Ref(l[6].role,"points"),Ref(l[9].role,"points")});
    flow(std::string(ALocalMatrixFlowKeyV1),l[2].role,"output",{Ref(l[3].role,"input")});
    flow(std::string(AGlobalMatrixFlowKeyV1),l[3].role,"output",{Ref(l[4].role,"transforms")});
    flow(std::string(AOutputFlowKeyV1),l[4].role,"output",{Ref(l[10].role,"a")});
    flow(std::string(BGlobalMotorFlowKeyV1),l[5].role,"output",{Ref(l[6].role,"transforms")});
    flow(std::string(BOutputFlowKeyV1),l[6].role,"output",{Ref(l[10].role,"b")});
    flow(std::string(CGlobalMotorFlowKeyV1),l[7].role,"output",{Ref(l[8].role,"input")});
    flow(std::string(CGlobalMatrixFlowKeyV1),l[8].role,"output",{Ref(l[9].role,"transforms")});
    flow(std::string(COutputFlowKeyV1),l[9].role,"output",{Ref(l[10].role,"c")});
    flow(std::string(RecordsFlowKeyV1),l[10].role,"records",{},can::ResourceBoundary::CompositionOutput);
    return input;
}

struct EndpointExpectationV1 final
{
    std::string_view leafKey;
    std::string_view endpointKey;
};

can::CompositionEndpointId FindEndpointByAuthorKeys(
    const can::PackageCompositionContract& contract,
    std::string_view leafKey,
    std::string_view endpointKey) noexcept
{
    const auto leaf=FindLeafByAuthorKeyV1(contract,leafKey);
    if(!leaf.IsValid())return {};
    const auto stableEndpointKey=can::ComputeStableEndpointKey(endpointKey);
    for(const auto& endpoint:contract.endpoints)
        if(endpoint.leaf==leaf&&endpoint.stableKey==stableEndpointKey)return endpoint.id;
    return {};
}

bool FlowMatches(
    const can::PackageCompositionContract& contract,
    std::string_view flowKey,
    EndpointExpectationV1 producer,
    std::initializer_list<EndpointExpectationV1> consumers,
    std::uint64_t bytes,
    can::ResourceBoundary boundary)
{
    const auto id=FindFlowByAuthorKeyV1(contract,flowKey);
    if(!id.IsValid())return false;
    const auto& flow=contract.resources[id.value];
    if(flow.sizeBytes!=bytes||flow.boundary!=boundary||flow.consumers.size()!=consumers.size())return false;

    const auto expectedProducer=FindEndpointByAuthorKeys(contract,producer.leafKey,producer.endpointKey);
    if(!expectedProducer.IsValid()||flow.producer!=expectedProducer)return false;

    std::vector<std::uint32_t> actual;
    actual.reserve(flow.consumers.size());
    for(const auto endpoint:flow.consumers)actual.push_back(endpoint.value);
    std::sort(actual.begin(),actual.end());

    std::vector<std::uint32_t> expected;
    expected.reserve(consumers.size());
    for(const auto& item:consumers)
    {
        const auto endpoint=FindEndpointByAuthorKeys(contract,item.leafKey,item.endpointKey);
        if(!endpoint.IsValid())return false;
        expected.push_back(endpoint.value);
    }
    std::sort(expected.begin(),expected.end());
    return actual==expected;
}
}

composition::canonical::LeafPackageId FindLeafByAuthorKeyV1(const composition::canonical::PackageCompositionContract& contract,std::string_view authorKey) noexcept
{const auto key=composition::canonical::ComputeStableLeafKey(authorKey);for(const auto& item:contract.leaves)if(item.stableKey==key)return item.id;return {};}
composition::canonical::ResourceFlowId FindFlowByAuthorKeyV1(const composition::canonical::PackageCompositionContract& contract,std::string_view authorKey) noexcept
{const auto key=composition::canonical::ComputeStableResourceKey(authorKey);for(const auto& item:contract.resources)if(item.stableKey==key)return item.id;return {};}

base::Result<FrozenReuseComparisonScenarioV1,ScenarioErrorV1> BuildFrozenReuseComparisonScenarioV1(const spiral2::hierarchy::RigidHierarchySemanticV1& hierarchy,const corpus::ReuseCaseV1& reuseCase)
{
    auto hv=spiral2::hierarchy::ValidateRigidHierarchySemanticV1(hierarchy);if(!hv)return Fail<FrozenReuseComparisonScenarioV1>("hierarchy",hv.Error());
    auto sv=semantic::ValidateReuseSweepSemanticV1(hierarchy,reuseCase.semantic);if(!sv)return Fail<FrozenReuseComparisonScenarioV1>("semantic",sv.Error());
    const std::array kinds{candidate::CandidateKindV1::MatrixHierarchy,candidate::CandidateKindV1::DirectPgaHierarchy,candidate::CandidateKindV1::HybridHierarchy};
    auto rawA=planner::PlanCandidateGraphV1(hierarchy,reuseCase.semantic,kinds[0]);
    auto rawB=planner::PlanCandidateGraphV1(hierarchy,reuseCase.semantic,kinds[1]);
    auto rawC=planner::PlanCandidateGraphV1(hierarchy,reuseCase.semantic,kinds[2]);
    if(!rawA||!rawB||!rawC)return Fail<FrozenReuseComparisonScenarioV1>("planner",!rawA?rawA.Error():!rawB?rawB.Error():rawC.Error());
    std::array<base::Result<verification::VerifiedReuseRepresentationV1,std::string>,3> verified={verification::VerifyCandidateGraphV1(hierarchy,reuseCase.semantic,rawA.Value()),verification::VerifyCandidateGraphV1(hierarchy,reuseCase.semantic,rawB.Value()),verification::VerifyCandidateGraphV1(hierarchy,reuseCase.semantic,rawC.Value())};
    for(const auto& item:verified)if(!item)return Fail<FrozenReuseComparisonScenarioV1>("verifier",item.Error());
    FrozenReuseComparisonScenarioV1 out;out.hierarchyIdentity=spiral2::hierarchy::RigidHierarchySemanticIdentityV1(hierarchy);out.reuseSemanticIdentity=semantic::ReuseSweepSemanticIdentityV1(reuseCase.semantic);out.localPointCorpusIdentity=reuseCase.semantic.localPointCorpusIdentity;out.reuseCount=reuseCase.semantic.reuseCount;out.pointCount=reuseCase.semantic.pointCount;
    auto dynamic=leaf::CompileDynamicInvocationSourceV1(hierarchy,reuseCase.semantic,verified[0].Value());if(!dynamic)return Fail<FrozenReuseComparisonScenarioV1>(dynamic.Error().stage,dynamic.Error().message);out.leaves.push_back(std::move(dynamic.Value()));
    auto points=leaf::CompileConsumerPointSourceV1(reuseCase,verified[0].Value());if(!points)return Fail<FrozenReuseComparisonScenarioV1>(points.Error().stage,points.Error().message);out.leaves.push_back(std::move(points.Value()));
    for(std::size_t i=0;i<verified.size();++i){auto group=leaf::CompileVerifiedReuseLeafGroupV1(hierarchy,reuseCase.semantic,verified[i].Value());if(!group)return Fail<FrozenReuseComparisonScenarioV1>(group.Error().stage,group.Error().message);for(auto& item:group.Value().leaves)out.leaves.push_back(std::move(item));}
    auto observer=CompileObserver(reuseCase.semantic);if(!observer)return base::Result<FrozenReuseComparisonScenarioV1,ScenarioErrorV1>::Failure(observer.Error());out.leaves.push_back(std::move(observer.Value()));
    if(out.leaves.size()!=11)return Fail<FrozenReuseComparisonScenarioV1>("leaves","Spiral 3 comparison must contain eleven Leaves");
    auto contract=can::BuildCompositionContract(BuildInput(out.leaves));if(!contract)return Fail<FrozenReuseComparisonScenarioV1>(contract.Error().stage,contract.Error().message);
    auto raw=can::planning::ProposeCompositionPlan(contract.Value());if(!raw)return Fail<FrozenReuseComparisonScenarioV1>(raw.Error().stage,raw.Error().message);
    auto sealed=can::verification::VerifyAndSeal(contract.Value(),raw.Value());if(!sealed)return Fail<FrozenReuseComparisonScenarioV1>(sealed.Error().stage,sealed.Error().message);
    auto frozen=can::verification::FreezeVerifiedComposition(contract.Value(),sealed.Value());if(!frozen)return Fail<FrozenReuseComparisonScenarioV1>(frozen.Error().stage,frozen.Error().message);
    out.frozenCompositionBytes=std::move(frozen.Value());out.frozenCompositionDigest=base::Sha256(out.frozenCompositionBytes);out.scenarioIdentity=ComputeFrozenReuseComparisonScenarioIdentityV1(out);
    auto valid=ValidateFrozenReuseComparisonScenarioV1(hierarchy,reuseCase,out);if(!valid)return base::Result<FrozenReuseComparisonScenarioV1,ScenarioErrorV1>::Failure(valid.Error());
    return base::Result<FrozenReuseComparisonScenarioV1,ScenarioErrorV1>::Success(std::move(out));
}

base::Result<void,ScenarioErrorV1> ValidateFrozenReuseComparisonScenarioV1(const spiral2::hierarchy::RigidHierarchySemanticV1& hierarchy,const corpus::ReuseCaseV1& reuseCase,const FrozenReuseComparisonScenarioV1& scenario)
{
    if(scenario.hierarchyIdentity!=spiral2::hierarchy::RigidHierarchySemanticIdentityV1(hierarchy)||scenario.reuseSemanticIdentity!=semantic::ReuseSweepSemanticIdentityV1(reuseCase.semantic)||scenario.localPointCorpusIdentity!=reuseCase.semantic.localPointCorpusIdentity||scenario.reuseCount!=reuseCase.semantic.reuseCount||scenario.pointCount!=reuseCase.semantic.pointCount)return Fail<void>("scenario/identity","scenario does not match hierarchy or reuse Semantic");
    if(scenario.leaves.size()!=11||scenario.frozenCompositionBytes.empty()||scenario.frozenCompositionDigest!=base::Sha256(scenario.frozenCompositionBytes)||scenario.scenarioIdentity!=ComputeFrozenReuseComparisonScenarioIdentityV1(scenario))return Fail<void>("scenario/evidence","aggregate scenario evidence mismatch");
    const std::array<std::string_view,11> roles{"L0.DynamicInvocationSource","L1.ConsumerPointSource","A0.LocalMotorToMatrix","A1.MatrixHierarchy","A2.MatrixConsumerApply","B0.MotorHierarchy","B1.DirectPgaConsumerApply","C0.MotorHierarchy","C1.GlobalMotorToMatrix","C2.MatrixConsumerApply",ObserverLeafRoleV1};
    for(std::size_t i=0;i<scenario.leaves.size();++i){if(scenario.leaves[i].role!=roles[i]||scenario.leaves[i].reuseSemanticIdentity!=scenario.reuseSemanticIdentity||scenario.leaves[i].localPointCorpusIdentity!=scenario.localPointCorpusIdentity||scenario.leaves[i].reuseCount!=scenario.reuseCount||scenario.leaves[i].pointCount!=scenario.pointCount)return Fail<void>("scenario/leaf-role","Leaf role or reuse authority mismatch");auto valid=leaf::ValidateFrozenReuseLeafV1(scenario.leaves[i]);if(!valid)return Fail<void>(valid.Error().stage,valid.Error().message);}
    auto frozen=can::verification::ReadVerifiedFrozenComposition(scenario.frozenCompositionBytes);if(!frozen)return Fail<void>(frozen.Error().stage,frozen.Error().message);
    const auto& contract=frozen.Value().ValidatedContract().Contract();const auto& plan=frozen.Value().VerifiedPlan().Plan();
    if(contract.leaves.size()!=11||contract.resources.size()!=12||plan.schedule.size()!=11||plan.allocations.size()!=12||plan.handoffs.size()!=15||plan.signals.size()!=11||plan.waits.size()!=15||plan.recovery.recreateLeaves.size()!=11||plan.recovery.recreateResources.size()!=11)return Fail<void>("scenario/topology","eleven-Leaf/twelve-Flow plan topology mismatch");
    for(const auto& item:scenario.leaves){auto id=FindLeafByAuthorKeyV1(contract,item.role);if(!id.IsValid())return Fail<void>("scenario/leaf","Frozen Composition is missing a required Leaf");const auto& bound=contract.leaves[id.value];if(bound.executionDigest!=item.executionDigest||bound.fileDigest!=item.fileDigest)return Fail<void>("scenario/leaf","Frozen Composition Leaf package does not match bound evidence");}
    const std::uint64_t motors=static_cast<std::uint64_t>(hierarchy.boneCount)*32u,points=static_cast<std::uint64_t>(scenario.pointCount)*16u,matrix=static_cast<std::uint64_t>(hierarchy.boneCount)*48u,records=static_cast<std::uint64_t>(scenario.pointCount)*304u;
    if(!FlowMatches(contract,MotorsFlowKeyV1,{roles[0],"motors"},{{roles[2],"input"},{roles[5],"input"},{roles[7],"input"}},motors,can::ResourceBoundary::Internal)||
       !FlowMatches(contract,ReferenceFlowKeyV1,{roles[0],"reference"},{{roles[10],"reference"}},points,can::ResourceBoundary::Internal)||
       !FlowMatches(contract,PointsFlowKeyV1,{roles[1],"points"},{{roles[4],"points"},{roles[6],"points"},{roles[9],"points"}},points,can::ResourceBoundary::Internal)||
       !FlowMatches(contract,ALocalMatrixFlowKeyV1,{roles[2],"output"},{{roles[3],"input"}},matrix,can::ResourceBoundary::Internal)||
       !FlowMatches(contract,AGlobalMatrixFlowKeyV1,{roles[3],"output"},{{roles[4],"transforms"}},matrix,can::ResourceBoundary::Internal)||
       !FlowMatches(contract,AOutputFlowKeyV1,{roles[4],"output"},{{roles[10],"a"}},points,can::ResourceBoundary::Internal)||
       !FlowMatches(contract,BGlobalMotorFlowKeyV1,{roles[5],"output"},{{roles[6],"transforms"}},motors,can::ResourceBoundary::Internal)||
       !FlowMatches(contract,BOutputFlowKeyV1,{roles[6],"output"},{{roles[10],"b"}},points,can::ResourceBoundary::Internal)||
       !FlowMatches(contract,CGlobalMotorFlowKeyV1,{roles[7],"output"},{{roles[8],"input"}},motors,can::ResourceBoundary::Internal)||
       !FlowMatches(contract,CGlobalMatrixFlowKeyV1,{roles[8],"output"},{{roles[9],"transforms"}},matrix,can::ResourceBoundary::Internal)||
       !FlowMatches(contract,COutputFlowKeyV1,{roles[9],"output"},{{roles[10],"c"}},points,can::ResourceBoundary::Internal)||
       !FlowMatches(contract,RecordsFlowKeyV1,{roles[10],"records"},{},records,can::ResourceBoundary::CompositionOutput))
        return Fail<void>("scenario/flow-authority","Frozen Flow endpoint role, size, fan-out or boundary mismatch");
    return base::Result<void,ScenarioErrorV1>::Success();
}

base::Digest256 ComputeFrozenReuseComparisonScenarioIdentityV1(const FrozenReuseComparisonScenarioV1& s)
{
    base::BinaryWriter w;w.WriteBytes(Lit("SGE4-5.Spiral3.FrozenReuseComparisonScenario.V1"));w.WriteBytes(s.hierarchyIdentity);w.WriteBytes(s.reuseSemanticIdentity);w.WriteBytes(s.localPointCorpusIdentity);w.WriteU32(s.reuseCount);w.WriteU32(s.pointCount);w.WriteU32(static_cast<std::uint32_t>(s.leaves.size()));for(const auto& item:s.leaves){auto bytes=leaf::SerializeFrozenReuseLeafEvidenceV1(item);w.WriteU64(bytes.size());w.WriteBytes(bytes);}w.WriteU64(s.frozenCompositionBytes.size());w.WriteBytes(s.frozenCompositionBytes);w.WriteBytes(s.frozenCompositionDigest);return base::Sha256(std::move(w).Take());
}
std::vector<std::byte> SerializeFrozenReuseComparisonScenarioEvidenceV1(const FrozenReuseComparisonScenarioV1& s)
{base::BinaryWriter w;w.WriteBytes(s.hierarchyIdentity);w.WriteBytes(s.reuseSemanticIdentity);w.WriteBytes(s.localPointCorpusIdentity);w.WriteU32(s.reuseCount);w.WriteU32(s.pointCount);w.WriteU32(static_cast<std::uint32_t>(s.leaves.size()));for(const auto& item:s.leaves){auto bytes=leaf::SerializeFrozenReuseLeafEvidenceV1(item);w.WriteU64(bytes.size());w.WriteBytes(bytes);}w.WriteU64(s.frozenCompositionBytes.size());w.WriteBytes(s.frozenCompositionBytes);w.WriteBytes(s.frozenCompositionDigest);w.WriteBytes(s.scenarioIdentity);return std::move(w).Take();}
}
