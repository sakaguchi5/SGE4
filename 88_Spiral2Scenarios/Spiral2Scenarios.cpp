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

namespace sge4_5::spiral2::scenarios
{
namespace
{
namespace sem=::sge4_5::semantic;namespace can=composition::canonical;
template<class T>base::Result<T,ScenarioErrorV1> Fail(std::string s,std::string m){return base::Result<T,ScenarioErrorV1>::Failure({std::move(s),std::move(m)});}
base::Result<leaf::FrozenHierarchyLeafV1,ScenarioErrorV1> CompileObserver(std::uint32_t points)
{
    sem::SemanticBuilder b;auto reference=b.AddExternalBuffer("S2.Observer.Reference",static_cast<std::uint64_t>(points)*16,16);auto a=b.AddExternalBuffer("S2.Observer.A",static_cast<std::uint64_t>(points)*16,16);auto d=b.AddExternalBuffer("S2.Observer.B",static_cast<std::uint64_t>(points)*16,16);auto c=b.AddExternalBuffer("S2.Observer.C",static_cast<std::uint64_t>(points)*16,16);auto o=b.AddExternalBuffer("S2.Observer.Records",static_cast<std::uint64_t>(points)*16,16);if(!reference||!a||!d||!c||!o)return Fail<leaf::FrozenHierarchyLeafV1>("observer/resources","resource construction failed");auto ur=b.AddUse(reference.Value(),sem::Effect::Read,sem::ViewRole::ShaderBuffer);auto ua=b.AddUse(a.Value(),sem::Effect::Read,sem::ViewRole::ShaderBuffer);auto ub=b.AddUse(d.Value(),sem::Effect::Read,sem::ViewRole::ShaderBuffer);auto uc=b.AddUse(c.Value(),sem::Effect::Read,sem::ViewRole::ShaderBuffer);auto uo=b.AddUse(o.Value(),sem::Effect::Write,sem::ViewRole::StorageBuffer);sem::ProgramInterface pi;pi.parameters={{{0},"Reference",sem::ProgramParameterKind::ReadOnlyBuffer,sem::ShaderStage::Compute,0,0,1},{{1},"A",sem::ProgramParameterKind::ReadOnlyBuffer,sem::ShaderStage::Compute,1,0,1},{{2},"B",sem::ProgramParameterKind::ReadOnlyBuffer,sem::ShaderStage::Compute,2,0,1},{{3},"C",sem::ProgramParameterKind::ReadOnlyBuffer,sem::ShaderStage::Compute,3,0,1},{{4},"Records",sem::ProgramParameterKind::UnorderedBuffer,sem::ShaderStage::Compute,0,0,1}};const std::string shader="static const uint PointCount="+std::to_string(points)+R"hlsl(u;
StructuredBuffer<float4>Reference:register(t0);StructuredBuffer<float4>A:register(t1);StructuredBuffer<float4>B:register(t2);StructuredBuffer<float4>C:register(t3);RWStructuredBuffer<uint4>Records:register(u0);float E(float4 x,float4 y){return max(max(abs(x.x-y.x),abs(x.y-y.y)),abs(x.z-y.z));}[numthreads(64,1,1)]void CSMain(uint3 id:SV_DispatchThreadID){uint i=id.x;if(i>=PointCount)return;float4 r=Reference[i],a=A[i],b=B[i],c=C[i];float e=max(max(max(E(a,r),E(b,r)),E(c,r)),max(max(E(a,b),E(a,c)),E(b,c)));uint finite=all(isfinite(r))&&all(isfinite(a))&&all(isfinite(b))&&all(isfinite(c))?1u:0u;Records[i]=uint4(asuint(e),finite,(asuint(r.w)==0x3f800000u&&asuint(a.w)==0x3f800000u&&asuint(b.w)==0x3f800000u&&asuint(c.w)==0x3f800000u)?1u:0u,0u);})hlsl";auto program=b.AddComputeProgram("Spiral2.Observer.V1",std::move(pi),{shader,{}, {},"CSMain"});if(!program)return Fail<leaf::FrozenHierarchyLeafV1>("observer/program",program.Error());const std::array ops={sem::WorkOperand{sem::WorkOperandKind::ProgramParameter,ur.Value(),{0}},sem::WorkOperand{sem::WorkOperandKind::ProgramParameter,ua.Value(),{1}},sem::WorkOperand{sem::WorkOperandKind::ProgramParameter,ub.Value(),{2}},sem::WorkOperand{sem::WorkOperandKind::ProgramParameter,uc.Value(),{3}},sem::WorkOperand{sem::WorkOperandKind::ProgramParameter,uo.Value(),{4}}};auto work=b.AddComputeWorkGeneric("Spiral2.Observer.Work.V1",program.Value(),ops,(points+63)/64,1,1);if(!work)return Fail<leaf::FrozenHierarchyLeafV1>("observer/work",work.Error());auto profile=spiral1::leaf::CanonicalLeafD3D12TargetProfileV1();profile.shaderDescriptorCount=8;auto compiled=compiler::CompileCanonical(std::move(b).Build(),profile);if(!compiled)return Fail<leaf::FrozenHierarchyLeafV1>(compiled.Error().stage,compiled.Error().message);auto package=package::PackageReader::Read(compiled.Value().packageBytes);if(!package)return Fail<leaf::FrozenHierarchyLeafV1>("observer/package",package.Error().message);leaf::FrozenHierarchyLeafV1 out;out.role="L9.Observer";out.verifiedRepresentationCertificate=base::Sha256(std::as_bytes(std::span("S2.Observer.Seal.V1",19)));out.executionDigest=package.Value().ExecutionDigest();out.fileDigest=package.Value().Header().fileDigest;out.inputStride=16;out.outputStride=16;out.packageBytes=std::move(compiled.Value().packageBytes);return base::Result<leaf::FrozenHierarchyLeafV1,ScenarioErrorV1>::Success(std::move(out));
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
base::Result<HierarchyComparisonExecutionResultV1,ScenarioErrorV1> ExecuteFrozenHierarchyComparisonScenarioV1(const FrozenHierarchyComparisonScenarioV1& s,const hierarchy::RigidHierarchySemanticV1& h,const contracts::DynamicLocalMotorPaletteV1& palette,d3d12::D3D12Backend& backend,std::uint64_t frameNumber)
{
    auto scenarioValid=ValidateFrozenHierarchyComparisonScenarioV1(s);
    if(!scenarioValid)return base::Result<HierarchyComparisonExecutionResultV1,ScenarioErrorV1>::Failure(scenarioValid.Error());
    if(s.hierarchyIdentity!=hierarchy::RigidHierarchySemanticIdentityV1(h)||s.boneCount!=h.boneCount)return Fail<HierarchyComparisonExecutionResultV1>("scenario/runtime","hierarchy does not match the Frozen scenario");
    auto paletteValid=contracts::ValidateDynamicLocalMotorPaletteV1(palette,h.boneCount);
    if(!paletteValid)return Fail<HierarchyComparisonExecutionResultV1>("scenario/runtime",paletteValid.Error());
    auto reference=corpus::BuildIndependentCpuReferenceV1(h,palette);
    if(!reference)return Fail<HierarchyComparisonExecutionResultV1>("scenario/reference",reference.Error());
    auto frozen=can::verification::ReadVerifiedFrozenComposition(s.frozenCompositionBytes);
    if(!frozen)return Fail<HierarchyComparisonExecutionResultV1>(frozen.Error().stage,frozen.Error().message);
    const auto& contract=frozen.Value().ValidatedContract().Contract();
    const auto source=FindLeaf(contract,s.leaves[0].role);
    const auto referenceFlow=FindFlow(contract,"s2/reference"),aFlow=FindFlow(contract,"s2/a/observations"),bFlow=FindFlow(contract,"s2/b/observations"),cFlow=FindFlow(contract,"s2/c/observations"),recordsFlow=FindFlow(contract,"s2/records");
    if(!source.IsValid()||!referenceFlow.IsValid()||!aFlow.IsValid()||!bFlow.IsValid()||!cFlow.IsValid()||!recordsFlow.IsValid())return Fail<HierarchyComparisonExecutionResultV1>("scenario/runtime","canonical Leaf or Resource Flow identity was not found");
    namespace runtime=can::runtime_v1;
    auto loaded=runtime::LoadStaticComposition(s.frozenCompositionBytes,backend);
    if(!loaded)return Fail<HierarchyComparisonExecutionResultV1>(loaded.Error().stage,loaded.Error().message);
    runtime::StaticCompositionFrameInvocation invocation;
    invocation.frameNumber=frameNumber;
    invocation.dynamicData={{source,s.leaves[0].lockedDynamicSlot,contracts::SerializeDynamicLocalMotorPaletteV1(palette)},{source,s.leaves[0].lockedReferenceSlot,corpus::SerializeReferencePointsV1(reference.Value())}};
    auto submitted=runtime::SubmitStaticComposition(loaded.Value(),invocation);
    if(!submitted)return Fail<HierarchyComparisonExecutionResultV1>(submitted.Error().stage,submitted.Error().message);
    if(submitted.Value().executionOrder.size()!=10||submitted.Value().leaves.size()!=10)return Fail<HierarchyComparisonExecutionResultV1>("scenario/runtime","Frozen Composition did not execute exactly ten Leaves");
    auto referenceRead=runtime::ReadStaticCompositionBuffer(loaded.Value(),referenceFlow),a=runtime::ReadStaticCompositionBuffer(loaded.Value(),aFlow),b=runtime::ReadStaticCompositionBuffer(loaded.Value(),bFlow),c=runtime::ReadStaticCompositionBuffer(loaded.Value(),cFlow),records=runtime::ReadStaticCompositionBuffer(loaded.Value(),recordsFlow);
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
    if(observed.Value().mismatchCount!=0||observed.Value().nonFiniteCount!=0)return Fail<HierarchyComparisonExecutionResultV1>("scenario/observer","WARP observations violate the independent reference or representation agreement");
    if(records.Value().bytes.size()!=static_cast<std::size_t>(points)*16)return Fail<HierarchyComparisonExecutionResultV1>("scenario/observer","GPU observer record byte count is invalid");
    base::BinaryReader recordReader(records.Value().bytes);
    for(std::uint32_t i=0;i<points;++i){auto errorBits=recordReader.ReadU32();auto finite=recordReader.ReadU32();auto homogeneous=recordReader.ReadU32();auto reserved=recordReader.ReadU32();if(!errorBits||!finite||!homogeneous||!reserved||finite.Value()!=1||homogeneous.Value()!=1||reserved.Value()!=0||!std::isfinite(std::bit_cast<float>(errorBits.Value()))||std::bit_cast<float>(errorBits.Value())>6.0e-5f)return Fail<HierarchyComparisonExecutionResultV1>("scenario/observer","GPU observer record validation failed");}
    HierarchyComparisonExecutionResultV1 out;out.matrix=std::move(ap.Value());out.direct=std::move(bp.Value());out.hybrid=std::move(cp.Value());out.observation=observed.Value();out.gpuRecords=std::move(records.Value().bytes);out.deviceEpoch=submitted.Value().deviceEpoch;
    return base::Result<HierarchyComparisonExecutionResultV1,ScenarioErrorV1>::Success(std::move(out));
}
std::vector<std::byte> SerializeFrozenHierarchyComparisonScenarioEvidenceV1(const FrozenHierarchyComparisonScenarioV1& s){base::BinaryWriter w;w.WriteBytes(s.hierarchyIdentity);w.WriteU32(s.boneCount);w.WriteU32(static_cast<std::uint32_t>(s.leaves.size()));for(const auto& item:s.leaves){auto bytes=leaf::SerializeFrozenHierarchyLeafEvidenceV1(item);w.WriteU64(bytes.size());w.WriteBytes(bytes);}w.WriteU64(s.frozenCompositionBytes.size());w.WriteBytes(s.frozenCompositionBytes);w.WriteBytes(s.frozenCompositionDigest);return std::move(w).Take();}
}
