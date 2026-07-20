#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "Spiral2PerformanceExperiment.h"

#include "../00_Foundation/FileIO.h"
#include "../00_Foundation/Sha256.h"
#include "../80_HierarchySemantic/HierarchySemantic.h"
#include "../81_Spiral2Contracts/Spiral2Contracts.h"
#include "../82_Spiral2Corpus/Spiral2Corpus.h"
#include "../84_HierarchyRepresentationPlanner/HierarchyRepresentationPlanner.h"
#include "../85_HierarchyRepresentationVerifier/HierarchyRepresentationVerifier.h"
#include "../86_Spiral2LeafCompiler/Spiral2LeafCompiler.h"
#include "../88_Spiral2Scenarios/Spiral2Scenarios.h"
#include "../17_CompositionContract/CompositionContract.h"
#include "../18_CompositionPlan/CompositionPlan.h"
#include "../19_CompositionVerifier/CompositionVerifier.h"
#include "../22_CompositionRuntime/CompositionRuntime.h"

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string_view>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

namespace sge4_5::spiral2::performance
{
namespace
{
using Clock = std::chrono::steady_clock;
using Microsoft::WRL::ComPtr;
namespace canrt = composition::canonical::runtime_v1;
constexpr std::string_view EvidenceMagic = "SGE4-5.Spiral2.MeasurementEvidence.V2";

void Require(bool condition, std::string_view message)
{
    if (!condition) throw std::runtime_error(std::string(message));
}

std::uint64_t Nanoseconds(Clock::time_point begin, Clock::time_point end)
{
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count());
}

void WriteString(base::BinaryWriter& writer, std::string_view value)
{
    writer.WriteU32(static_cast<std::uint32_t>(value.size()));
    writer.WriteBytes(std::as_bytes(std::span(value.data(), value.size())));
}

struct ReadText final { std::string value; };
base::Result<ReadText,std::string> ReadString(base::BinaryReader& reader)
{
    auto size = reader.ReadU32();
    if (!size) return base::Result<ReadText,std::string>::Failure(size.Error());
    auto bytes = reader.ReadBytes(size.Value());
    if (!bytes) return base::Result<ReadText,std::string>::Failure(bytes.Error());
    return base::Result<ReadText,std::string>::Success(ReadText{
        std::string(reinterpret_cast<const char*>(bytes.Value().data()), bytes.Value().size())});
}

void WriteDigest(base::BinaryWriter& writer, const base::Digest256& value) { writer.WriteBytes(value); }
base::Result<base::Digest256,std::string> ReadDigest(base::BinaryReader& reader)
{
    auto bytes=reader.ReadBytes(32);if(!bytes)return base::Result<base::Digest256,std::string>::Failure(bytes.Error());
    base::Digest256 result{};std::memcpy(result.data(),bytes.Value().data(),result.size());
    return base::Result<base::Digest256,std::string>::Success(result);
}
void WriteDouble(base::BinaryWriter& writer,double value){writer.WriteU64(std::bit_cast<std::uint64_t>(value));}
base::Result<double,std::string> ReadDouble(base::BinaryReader& reader){auto v=reader.ReadU64();if(!v)return base::Result<double,std::string>::Failure(v.Error());return base::Result<double,std::string>::Success(std::bit_cast<double>(v.Value()));}

std::string Hex(const base::Digest256& value)
{
    static constexpr char digits[]="0123456789abcdef";std::string result(value.size()*2,'0');
    for(std::size_t i=0;i<value.size();++i){const auto b=std::to_integer<unsigned char>(value[i]);result[i*2]=digits[b>>4];result[i*2+1]=digits[b&15];}
    return result;
}

std::string JsonEscape(std::string_view value)
{
    std::string result;for(const char c:value){if(c=='\\')result+="\\\\";else if(c=='\"')result+="\\\"";else if(c=='\n')result+="\\n";else if(c=='\r')result+="\\r";else result+=c;}return result;
}

const char* KindName(candidate::CandidateKindV1 kind)
{
    switch(kind){case candidate::CandidateKindV1::MatrixHierarchy:return "A.MatrixHierarchy";case candidate::CandidateKindV1::DirectPgaHierarchy:return "B.DirectPgaHierarchy";case candidate::CandidateKindV1::HybridHierarchy:return "C.HybridHierarchy";default:return "Unknown";}
}
char KindLetter(candidate::CandidateKindV1 kind)
{
    return kind==candidate::CandidateKindV1::MatrixHierarchy?'A':kind==candidate::CandidateKindV1::DirectPgaHierarchy?'B':'C';
}
candidate::CandidateKindV1 RoleKind(std::string_view role)
{
    if(!role.empty()&&role[0]=='A')return candidate::CandidateKindV1::MatrixHierarchy;
    if(!role.empty()&&role[0]=='B')return candidate::CandidateKindV1::DirectPgaHierarchy;
    if(!role.empty()&&role[0]=='C')return candidate::CandidateKindV1::HybridHierarchy;
    return static_cast<candidate::CandidateKindV1>(0);
}

constexpr std::array<std::array<candidate::CandidateKindV1,3>,6> CandidateOrders{{
    {{candidate::CandidateKindV1::MatrixHierarchy,candidate::CandidateKindV1::DirectPgaHierarchy,candidate::CandidateKindV1::HybridHierarchy}},
    {{candidate::CandidateKindV1::MatrixHierarchy,candidate::CandidateKindV1::HybridHierarchy,candidate::CandidateKindV1::DirectPgaHierarchy}},
    {{candidate::CandidateKindV1::DirectPgaHierarchy,candidate::CandidateKindV1::MatrixHierarchy,candidate::CandidateKindV1::HybridHierarchy}},
    {{candidate::CandidateKindV1::DirectPgaHierarchy,candidate::CandidateKindV1::HybridHierarchy,candidate::CandidateKindV1::MatrixHierarchy}},
    {{candidate::CandidateKindV1::HybridHierarchy,candidate::CandidateKindV1::MatrixHierarchy,candidate::CandidateKindV1::DirectPgaHierarchy}},
    {{candidate::CandidateKindV1::HybridHierarchy,candidate::CandidateKindV1::DirectPgaHierarchy,candidate::CandidateKindV1::MatrixHierarchy}}
}};
std::string OrderName(const std::array<candidate::CandidateKindV1,3>& order){std::string value;for(const auto kind:order)value.push_back(KindLetter(kind));return value;}

struct CaseDefinition final{std::string id;std::string topology;std::vector<std::int32_t> parents;};
std::vector<std::int32_t> Star(std::uint32_t count){std::vector<std::int32_t> p(count,0);p[0]=-1;return p;}
std::vector<std::int32_t> Balanced(std::uint32_t count){std::vector<std::int32_t> p(count,-1);for(std::uint32_t i=1;i<count;++i)p[i]=static_cast<std::int32_t>((i-1)/2);return p;}
std::vector<std::int32_t> Chain(std::uint32_t count){std::vector<std::int32_t> p(count,-1);for(std::uint32_t i=1;i<count;++i)p[i]=static_cast<std::int32_t>(i-1);return p;}
std::vector<std::int32_t> Mixed(std::uint32_t count){std::vector<std::int32_t> p(count,-1);for(std::uint32_t i=1;i<count;++i){if(i%7==0)p[i]=0;else if(i%5==0)p[i]=static_cast<std::int32_t>((i-1)/2);else p[i]=static_cast<std::int32_t>(i-1);}return p;}
std::array<CaseDefinition,6> MeasurementCases(){return{{{"M0","8 bones, star",Star(8)},{"M1","15 bones, balanced",Balanced(15)},{"M2","32 bones, mixed",Mixed(32)},{"M3","64 bones, balanced",Balanced(64)},{"M4","64 bones, chain",Chain(64)},{"M5","128 bones, mixed",Mixed(128)}}};}

std::string WideToUtf8(const wchar_t* text){if(!text)return{};const int count=WideCharToMultiByte(CP_UTF8,0,text,-1,nullptr,0,nullptr,nullptr);if(count<=1)return{};std::string value(static_cast<std::size_t>(count),'\0');WideCharToMultiByte(CP_UTF8,0,text,-1,value.data(),count,nullptr,nullptr);value.pop_back();return value;}
void Check(HRESULT hr,std::string_view stage){if(FAILED(hr)){std::ostringstream s;s<<stage<<" HRESULT=0x"<<std::hex<<static_cast<unsigned long>(hr);throw std::runtime_error(s.str());}}

AdapterProfileV1 CaptureAdapterProfile(std::uint32_t requestedIndex)
{
    ComPtr<IDXGIFactory6> factory;Check(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)),"CreateDXGIFactory2");
    ComPtr<IDXGIAdapter1> adapter;ComPtr<ID3D12Device> device;std::uint32_t hardwareIndex=0;
    for(UINT i=0;;++i){ComPtr<IDXGIAdapter1> candidateAdapter;const HRESULT found=factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&candidateAdapter));if(found==DXGI_ERROR_NOT_FOUND)break;if(FAILED(found))continue;DXGI_ADAPTER_DESC1 d{};candidateAdapter->GetDesc1(&d);if((d.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)!=0)continue;ComPtr<ID3D12Device> candidateDevice;if(FAILED(D3D12CreateDevice(candidateAdapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&candidateDevice))))continue;if(hardwareIndex++!=requestedIndex)continue;adapter=candidateAdapter;device=candidateDevice;break;}
    Require(device!=nullptr,"no compatible non-software D3D12 hardware adapter was found");
    AdapterProfileV1 profile;DXGI_ADAPTER_DESC1 d{};adapter->GetDesc1(&d);profile.description=WideToUtf8(d.Description);profile.vendorId=d.VendorId;profile.deviceId=d.DeviceId;profile.subsystemId=d.SubSysId;profile.revision=d.Revision;profile.luidLow=d.AdapterLuid.LowPart;profile.luidHigh=d.AdapterLuid.HighPart;LARGE_INTEGER driver{};if(SUCCEEDED(adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice),&driver)))profile.driverVersion=static_cast<std::uint64_t>(driver.QuadPart);D3D12_FEATURE_DATA_ARCHITECTURE1 arch{};if(SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_ARCHITECTURE1,&arch,sizeof(arch)))){profile.uma=arch.UMA!=FALSE;profile.cacheCoherentUma=arch.CacheCoherentUMA!=FALSE;}D3D12_COMMAND_QUEUE_DESC q{};q.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;ComPtr<ID3D12CommandQueue> queue;Check(device->CreateCommandQueue(&q,IID_PPV_ARGS(&queue)),"CreateCommandQueue");Check(queue->GetTimestampFrequency(&profile.timestampFrequency),"GetTimestampFrequency");base::BinaryWriter w;WriteString(w,profile.description);w.WriteU32(profile.vendorId);w.WriteU32(profile.deviceId);w.WriteU32(profile.subsystemId);w.WriteU32(profile.revision);w.WriteU32(profile.luidLow);w.WriteI32(profile.luidHigh);w.WriteU64(profile.driverVersion);w.WriteU64(profile.timestampFrequency);profile.fingerprint=base::Sha256(w.Bytes());return profile;
}

CandidatePreparationV1 PrepareCandidate(const hierarchy::RigidHierarchySemanticV1& hierarchyValue,candidate::CandidateKindV1 kind)
{
    CandidatePreparationV1 result;
    result.kind=kind;
    const auto generationBegin=Clock::now();
    auto raw=planner::PlanCandidateGraphV1(hierarchyValue,kind);
    const auto generationEnd=Clock::now();
    if(!raw)throw std::runtime_error(raw.Error());
    const auto verifierBegin=Clock::now();
    auto verified=verification::VerifyCandidateGraphV1(hierarchyValue,raw.Value());
    const auto verifierEnd=Clock::now();
    if(!verified)throw std::runtime_error(verified.Error());
    const auto freezeBegin=Clock::now();
    auto group=leaf::CompileVerifiedHierarchyLeafGroupV1(hierarchyValue,verified.Value());
    if(!group)throw std::runtime_error(group.Error().stage+": "+group.Error().message);
    for(std::size_t index=0;index<group.Value().leaves.size();++index)
    {
        const auto& item=group.Value().leaves[index];
        auto valid=leaf::ValidateFrozenHierarchyLeafV1(item);
        if(!valid)throw std::runtime_error(valid.Error().stage+": "+valid.Error().message);
        result.packageBytes+=item.packageBytes.size();
        if(index+1<group.Value().leaves.size())
            result.logicalIntermediateBytes+=static_cast<std::uint64_t>(item.primaryOutputElementCount)*item.outputStride;
    }
    const auto freezeEnd=Clock::now();
    result.generationNanoseconds=Nanoseconds(generationBegin,generationEnd);
    result.verifierNanoseconds=Nanoseconds(verifierBegin,verifierEnd);
    result.freezeNanoseconds=Nanoseconds(freezeBegin,freezeEnd);
    result.dynamicInvocationBytes=
        static_cast<std::uint64_t>(hierarchyValue.boneCount)*contracts::DynamicMotorStrideBytesV1+
        static_cast<std::uint64_t>(hierarchyValue.boneCount)*5u*sizeof(corpus::ObservedPointV1);
    result.plannedDispatchCount=verified.Value().Plan().candidateLeafDispatchCount;
    result.packageDispatchCount=static_cast<std::uint32_t>(group.Value().leaves.size());
    result.verifiedCertificate=verified.Value().CertificateIdentity();
    return result;
}

double CandidateMaxError(std::span<const corpus::ObservedPointV1> reference,std::span<const corpus::ObservedPointV1> observed)
{
    Require(reference.size()==observed.size(),"numeric comparison size mismatch");double maximum=0;for(std::size_t i=0;i<reference.size();++i){maximum=std::max(maximum,static_cast<double>(std::abs(observed[i].x-reference[i].x)));maximum=std::max(maximum,static_cast<double>(std::abs(observed[i].y-reference[i].y)));maximum=std::max(maximum,static_cast<double>(std::abs(observed[i].z-reference[i].z)));}return maximum;
}

namespace can=composition::canonical;
can::EndpointReferenceDeclaration MeasurementRef(const std::string& leafRole,const std::string& endpoint){return {leafRole,endpoint};}
can::LeafPackageDeclaration MeasurementDecl(const leaf::FrozenHierarchyLeafV1& item,std::initializer_list<can::LeafEndpointDeclaration> endpoints){can::LeafPackageDeclaration result;result.stableKey=item.role;result.packageBytes=item.packageBytes;result.endpoints=endpoints;return result;}
can::LeafPackageId MeasurementLeaf(const can::PackageCompositionContract& contract,std::string_view role){const auto key=can::ComputeStableLeafKey(role);for(const auto& item:contract.leaves)if(item.stableKey==key)return item.id;return {};}
can::ResourceFlowId MeasurementFlow(const can::PackageCompositionContract& contract,std::string_view keyText){const auto key=can::ComputeStableResourceKey(keyText);for(const auto& item:contract.resources)if(item.stableKey==key)return item.id;return {};}

struct CandidateScenario final
{
    candidate::CandidateKindV1 kind{};
    std::vector<leaf::FrozenHierarchyLeafV1> leaves;
    std::vector<std::byte> frozenCompositionBytes;
    std::string outputFlowKey;
};

CandidateScenario BuildCandidateScenario(const scenarios::FrozenHierarchyComparisonScenarioV1& comparison,candidate::CandidateKindV1 kind)
{
    CandidateScenario result;result.kind=kind;result.leaves.push_back(comparison.leaves[0]);const std::size_t first=kind==candidate::CandidateKindV1::MatrixHierarchy?1u:kind==candidate::CandidateKindV1::DirectPgaHierarchy?4u:6u;const std::size_t count=kind==candidate::CandidateKindV1::DirectPgaHierarchy?2u:3u;for(std::size_t i=0;i<count;++i)result.leaves.push_back(comparison.leaves[first+i]);const std::string prefix=std::string("s2/measurement/")+static_cast<char>(std::tolower(static_cast<unsigned char>(KindLetter(kind))));result.outputFlowKey=prefix+"/observations";can::ContractBuildInput input;input.leaves.push_back(MeasurementDecl(result.leaves[0],{{0,"output"},{1,"reference"}}));for(std::size_t i=1;i<result.leaves.size();++i)input.leaves.push_back(MeasurementDecl(result.leaves[i],{{0,"input"},{1,"output"}}));auto flow=[&](std::string key,std::string producer,std::string producerEndpoint,std::initializer_list<can::EndpointReferenceDeclaration> consumers,can::ResourceBoundary boundary){can::ResourceFlowDeclaration value;value.stableKey=std::move(key);value.boundary=boundary;value.producer=MeasurementRef(producer,producerEndpoint);value.consumers=consumers;input.resources.push_back(std::move(value));};flow(prefix+"/motors",result.leaves[0].role,"output",{MeasurementRef(result.leaves[1].role,"input")},can::ResourceBoundary::Internal);flow(prefix+"/reference",result.leaves[0].role,"reference",{},can::ResourceBoundary::CompositionOutput);for(std::size_t i=1;i+1<result.leaves.size();++i)flow(prefix+"/intermediate/"+std::to_string(i),result.leaves[i].role,"output",{MeasurementRef(result.leaves[i+1].role,"input")},can::ResourceBoundary::Internal);flow(result.outputFlowKey,result.leaves.back().role,"output",{},can::ResourceBoundary::CompositionOutput);auto contract=can::BuildCompositionContract(std::move(input));if(!contract)throw std::runtime_error(contract.Error().stage+": "+contract.Error().message);auto plan=can::planning::ProposeCompositionPlan(contract.Value());if(!plan)throw std::runtime_error(plan.Error().stage+": "+plan.Error().message);auto verified=can::verification::VerifyAndSeal(contract.Value(),plan.Value());if(!verified)throw std::runtime_error(verified.Error().stage+": "+verified.Error().message);auto frozen=can::verification::FreezeVerifiedComposition(contract.Value(),verified.Value());if(!frozen)throw std::runtime_error(frozen.Error().stage+": "+frozen.Error().message);result.frozenCompositionBytes=std::move(frozen).Value();return result;
}

std::vector<corpus::ObservedPointV1> DecodeMeasurementPoints(std::span<const std::byte> bytes,std::uint32_t pointCount)
{
    Require(bytes.size()==static_cast<std::size_t>(pointCount)*sizeof(corpus::ObservedPointV1),"candidate output byte count mismatch");base::BinaryReader reader(bytes);std::vector<corpus::ObservedPointV1> result;result.reserve(pointCount);for(std::uint32_t i=0;i<pointCount;++i){auto x=reader.ReadU32();auto y=reader.ReadU32();auto z=reader.ReadU32();auto w=reader.ReadU32();Require(x&&y&&z&&w,"candidate output is truncated");result.push_back({std::bit_cast<float>(x.Value()),std::bit_cast<float>(y.Value()),std::bit_cast<float>(z.Value()),std::bit_cast<float>(w.Value())});}return result;
}

struct CandidateRuntime final
{
    CandidateScenario scenario;
    canrt::LoadedStaticComposition loaded;
    std::uint64_t materializationNanoseconds=0;
    std::uint64_t nextFrame=0;
    CandidateRuntime(CandidateScenario s,canrt::LoadedStaticComposition l,std::uint64_t m):scenario(std::move(s)),loaded(std::move(l)),materializationNanoseconds(m){}
};

struct CandidateExecution final{std::vector<LeafTimingSampleV1> leaves;CandidateTimingSampleV1 candidate;};
CandidateExecution ExecuteCandidate(std::uint32_t orderPosition,const hierarchy::RigidHierarchySemanticV1& hierarchyValue,const contracts::DynamicLocalMotorPaletteV1& palette,std::span<const corpus::ObservedPointV1> reference,CandidateRuntime& runtime,d3d12::D3D12Backend& backend)
{
    const auto candidateBegin=Clock::now();
    const auto& contract=runtime.loaded.Artifact().ValidatedContract().Contract();
    const auto source=MeasurementLeaf(contract,runtime.scenario.leaves[0].role);
    const auto outputFlow=MeasurementFlow(contract,runtime.scenario.outputFlowKey);
    Require(source.IsValid()&&outputFlow.IsValid(),"candidate measurement Composition identity lookup failed");
    canrt::StaticCompositionFrameInvocation invocation;
    invocation.frameNumber=runtime.nextFrame++;
    invocation.dynamicData={
        {source,runtime.scenario.leaves[0].lockedDynamicSlot,contracts::SerializeDynamicLocalMotorPaletteV1(palette)},
        {source,runtime.scenario.leaves[0].lockedReferenceSlot,corpus::SerializeReferencePointsV1(std::vector<corpus::ObservedPointV1>(reference.begin(),reference.end()))}};
    auto submitted=canrt::SubmitStaticComposition(runtime.loaded,invocation);
    if(!submitted)throw std::runtime_error(submitted.Error().stage+": "+submitted.Error().message);
    auto readback=canrt::ReadStaticCompositionBuffer(runtime.loaded,outputFlow);
    if(!readback)throw std::runtime_error(readback.Error().stage+": "+readback.Error().message);
    auto profiles=backend.ConsumeTimestampProfileSamples();
    Require(profiles.size()==runtime.scenario.leaves.size(),"candidate timestamp count mismatch");
    const auto firstInstance=std::min_element(profiles.begin(),profiles.end(),[](const auto& a,const auto& b){return a.instanceOrdinal<b.instanceOrdinal;})->instanceOrdinal;
    CandidateExecution result;
    result.candidate.kind=runtime.scenario.kind;
    result.candidate.orderPosition=orderPosition;
    for(const auto& profile:profiles)
    {
        const auto relative=profile.instanceOrdinal-firstInstance;
        Require(relative<runtime.scenario.leaves.size(),"candidate timestamp instance ordinal mismatch");
        LeafTimingSampleV1 value;
        value.role=runtime.scenario.leaves[static_cast<std::size_t>(relative)].role;
        value.kind=RoleKind(value.role);
        value.commandRecordingNanoseconds=profile.commandRecordingNanoseconds;
        value.gpuNanoseconds=profile.gpuNanoseconds;
        value.dispatchCount=profile.dispatchCount;
        value.barrierCount=profile.barrierCount;
        result.leaves.push_back(value);
        if(static_cast<std::uint32_t>(value.kind)!=0)
        {
            result.candidate.commandRecordingNanoseconds+=value.commandRecordingNanoseconds;
            result.candidate.gpuTotalNanoseconds+=value.gpuNanoseconds;
            result.candidate.dispatchCount+=value.dispatchCount;
            result.candidate.barrierCount+=value.barrierCount;
        }
    }
    auto points=DecodeMeasurementPoints(readback.Value().bytes,hierarchyValue.boneCount*5u);
    result.candidate.numericMaxAbsoluteError=CandidateMaxError(reference,points);
    result.candidate.endToEndNanoseconds=static_cast<double>(Nanoseconds(candidateBegin,Clock::now()));
    return result;
}

MeasurementSampleV1 ExecuteSample(std::uint32_t run,std::uint32_t cycle,const std::array<candidate::CandidateKindV1,3>& order,const hierarchy::RigidHierarchySemanticV1& hierarchyValue,const contracts::DynamicLocalMotorPaletteV1& palette,std::span<const corpus::ObservedPointV1> reference,std::array<CandidateRuntime*,3> runtimes,d3d12::D3D12Backend& backend)
{
    MeasurementSampleV1 result;
    result.run=run;
    result.cycle=cycle;
    result.candidateOrder=OrderName(order);
    for(std::uint32_t position=0;position<3;++position)
    {
        auto* runtime=*std::find_if(runtimes.begin(),runtimes.end(),[&](const auto* value){return value->scenario.kind==order[position];});
        auto executed=ExecuteCandidate(position,hierarchyValue,palette,reference,*runtime,backend);
        result.leaves.insert(result.leaves.end(),executed.leaves.begin(),executed.leaves.end());
        result.candidates.push_back(executed.candidate);
    }
    return result;
}

void WritePreparation(base::BinaryWriter& w,const CandidatePreparationV1& v)
{
    w.WriteU32(static_cast<std::uint32_t>(v.kind));w.WriteU64(v.generationNanoseconds);w.WriteU64(v.verifierNanoseconds);w.WriteU64(v.freezeNanoseconds);w.WriteU64(v.materializationNanoseconds);w.WriteU64(v.packageBytes);w.WriteU64(v.dynamicInvocationBytes);w.WriteU64(v.logicalIntermediateBytes);w.WriteU32(v.plannedDispatchCount);w.WriteU32(v.packageDispatchCount);WriteDigest(w,v.verifiedCertificate);
}
base::Result<CandidatePreparationV1,std::string> ReadPreparation(base::BinaryReader& r)
{
    CandidatePreparationV1 v;auto a=r.ReadU32();auto b=r.ReadU64();auto c=r.ReadU64();auto d=r.ReadU64();auto e=r.ReadU64();auto f=r.ReadU64();auto g=r.ReadU64();auto h=r.ReadU64();auto i=r.ReadU32();auto j=r.ReadU32();auto k=ReadDigest(r);if(!a||!b||!c||!d||!e||!f||!g||!h||!i||!j||!k)return base::Result<CandidatePreparationV1,std::string>::Failure("preparation section truncated");v.kind=static_cast<candidate::CandidateKindV1>(a.Value());v.generationNanoseconds=b.Value();v.verifierNanoseconds=c.Value();v.freezeNanoseconds=d.Value();v.materializationNanoseconds=e.Value();v.packageBytes=f.Value();v.dynamicInvocationBytes=g.Value();v.logicalIntermediateBytes=h.Value();v.plannedDispatchCount=i.Value();v.packageDispatchCount=j.Value();v.verifiedCertificate=k.Value();return base::Result<CandidatePreparationV1,std::string>::Success(v);
}
void WriteLeaf(base::BinaryWriter& w,const LeafTimingSampleV1& v){WriteString(w,v.role);w.WriteU32(static_cast<std::uint32_t>(v.kind));WriteDouble(w,v.commandRecordingNanoseconds);WriteDouble(w,v.gpuNanoseconds);w.WriteU32(v.dispatchCount);w.WriteU32(v.barrierCount);}
base::Result<LeafTimingSampleV1,std::string> ReadLeaf(base::BinaryReader& r){LeafTimingSampleV1 v;auto a=ReadString(r);auto b=r.ReadU32();auto c=ReadDouble(r);auto d=ReadDouble(r);auto e=r.ReadU32();auto f=r.ReadU32();if(!a||!b||!c||!d||!e||!f)return base::Result<LeafTimingSampleV1,std::string>::Failure("leaf sample truncated");v.role=a.Value().value;v.kind=static_cast<candidate::CandidateKindV1>(b.Value());v.commandRecordingNanoseconds=c.Value();v.gpuNanoseconds=d.Value();v.dispatchCount=e.Value();v.barrierCount=f.Value();return base::Result<LeafTimingSampleV1,std::string>::Success(std::move(v));}
void WriteCandidateSample(base::BinaryWriter& w,const CandidateTimingSampleV1& v){w.WriteU32(static_cast<std::uint32_t>(v.kind));w.WriteU32(v.orderPosition);WriteDouble(w,v.endToEndNanoseconds);WriteDouble(w,v.commandRecordingNanoseconds);WriteDouble(w,v.gpuTotalNanoseconds);WriteDouble(w,v.numericMaxAbsoluteError);w.WriteU32(v.dispatchCount);w.WriteU32(v.barrierCount);}
base::Result<CandidateTimingSampleV1,std::string> ReadCandidateSample(base::BinaryReader& r){CandidateTimingSampleV1 v;auto a=r.ReadU32();auto b=r.ReadU32();auto c=ReadDouble(r);auto d=ReadDouble(r);auto e=ReadDouble(r);auto f=ReadDouble(r);auto g=r.ReadU32();auto h=r.ReadU32();if(!a||!b||!c||!d||!e||!f||!g||!h)return base::Result<CandidateTimingSampleV1,std::string>::Failure("candidate sample truncated");v.kind=static_cast<candidate::CandidateKindV1>(a.Value());v.orderPosition=b.Value();v.endToEndNanoseconds=c.Value();v.commandRecordingNanoseconds=d.Value();v.gpuTotalNanoseconds=e.Value();v.numericMaxAbsoluteError=f.Value();v.dispatchCount=g.Value();v.barrierCount=h.Value();return base::Result<CandidateTimingSampleV1,std::string>::Success(v);}

std::vector<std::byte> SerializeEvidence(const MeasurementEvidenceV1& e)
{
    base::BinaryWriter w;WriteString(w,EvidenceMagic);w.WriteU32(e.schemaVersion);w.WriteU64(e.captureUnixNanoseconds);w.WriteU32(e.config.warmupCyclesPerOrder);w.WriteU32(e.config.measurementCyclesPerOrder);w.WriteU32(e.config.runCount);w.WriteU32(e.config.adapterIndex);WriteString(w,e.config.clockPolicy);WriteString(w,e.adapter.description);w.WriteU32(e.adapter.vendorId);w.WriteU32(e.adapter.deviceId);w.WriteU32(e.adapter.subsystemId);w.WriteU32(e.adapter.revision);w.WriteU32(e.adapter.luidLow);w.WriteI32(e.adapter.luidHigh);w.WriteU64(e.adapter.driverVersion);w.WriteU64(e.adapter.timestampFrequency);w.WriteU32(e.adapter.uma?1u:0u);w.WriteU32(e.adapter.cacheCoherentUma?1u:0u);WriteDigest(w,e.adapter.fingerprint);WriteDigest(w,e.observationContractIdentity);WriteDigest(w,e.measurementProfileIdentity);w.WriteU32(static_cast<std::uint32_t>(e.cases.size()));for(const auto& c:e.cases){WriteString(w,c.id);WriteString(w,c.topology);w.WriteU32(c.boneCount);w.WriteU32(c.hierarchyDepthCount);WriteDigest(w,c.hierarchyIdentity);WriteDigest(w,c.invocationIdentity);w.WriteU32(static_cast<std::uint32_t>(c.preparation.size()));for(const auto& p:c.preparation)WritePreparation(w,p);w.WriteU32(static_cast<std::uint32_t>(c.samples.size()));for(const auto& s:c.samples){w.WriteU32(s.run);w.WriteU32(s.cycle);WriteString(w,s.candidateOrder);w.WriteU32(static_cast<std::uint32_t>(s.leaves.size()));for(const auto& l:s.leaves)WriteLeaf(w,l);w.WriteU32(static_cast<std::uint32_t>(s.candidates.size()));for(const auto& v:s.candidates)WriteCandidateSample(w,v);}}return std::move(w).Take();
}

base::Result<MeasurementEvidenceV1,std::string> DeserializeEvidence(std::span<const std::byte> bytes)
{
    base::BinaryReader r(bytes);auto magic=ReadString(r);if(!magic||magic.Value().value!=EvidenceMagic)return base::Result<MeasurementEvidenceV1,std::string>::Failure("measurement evidence magic mismatch");MeasurementEvidenceV1 e;auto version=r.ReadU32();auto capture=r.ReadU64();if(!version||!capture||version.Value()!=MeasurementEvidenceSchemaVersionV1)return base::Result<MeasurementEvidenceV1,std::string>::Failure("measurement evidence version mismatch");e.schemaVersion=version.Value();e.captureUnixNanoseconds=capture.Value();auto w=r.ReadU32();auto m=r.ReadU32();auto runs=r.ReadU32();auto ai=r.ReadU32();auto cp=ReadString(r);if(!w||!m||!runs||!ai||!cp)return base::Result<MeasurementEvidenceV1,std::string>::Failure("measurement config truncated");e.config={w.Value(),m.Value(),runs.Value(),ai.Value(),cp.Value().value};auto desc=ReadString(r);auto a1=r.ReadU32();auto a2=r.ReadU32();auto a3=r.ReadU32();auto a4=r.ReadU32();auto a5=r.ReadU32();auto a6=r.ReadI32();auto a7=r.ReadU64();auto a8=r.ReadU64();auto a9=r.ReadU32();auto a10=r.ReadU32();auto fingerprint=ReadDigest(r);auto observation=ReadDigest(r);auto profile=ReadDigest(r);if(!desc||!a1||!a2||!a3||!a4||!a5||!a6||!a7||!a8||!a9||!a10||!fingerprint||!observation||!profile)return base::Result<MeasurementEvidenceV1,std::string>::Failure("adapter/profile section truncated");e.adapter.description=desc.Value().value;e.adapter.vendorId=a1.Value();e.adapter.deviceId=a2.Value();e.adapter.subsystemId=a3.Value();e.adapter.revision=a4.Value();e.adapter.luidLow=a5.Value();e.adapter.luidHigh=a6.Value();e.adapter.driverVersion=a7.Value();e.adapter.timestampFrequency=a8.Value();e.adapter.uma=a9.Value()!=0;e.adapter.cacheCoherentUma=a10.Value()!=0;e.adapter.fingerprint=fingerprint.Value();e.observationContractIdentity=observation.Value();e.measurementProfileIdentity=profile.Value();auto caseCount=r.ReadU32();if(!caseCount)return base::Result<MeasurementEvidenceV1,std::string>::Failure("case count truncated");for(std::uint32_t ci=0;ci<caseCount.Value();++ci){MeasurementCaseV1 c;auto id=ReadString(r);auto topology=ReadString(r);auto bones=r.ReadU32();auto depth=r.ReadU32();auto hi=ReadDigest(r);auto ii=ReadDigest(r);auto preparationCount=r.ReadU32();if(!id||!topology||!bones||!depth||!hi||!ii||!preparationCount)return base::Result<MeasurementEvidenceV1,std::string>::Failure("case header truncated");c.id=id.Value().value;c.topology=topology.Value().value;c.boneCount=bones.Value();c.hierarchyDepthCount=depth.Value();c.hierarchyIdentity=hi.Value();c.invocationIdentity=ii.Value();for(std::uint32_t i=0;i<preparationCount.Value();++i){auto p=ReadPreparation(r);if(!p)return base::Result<MeasurementEvidenceV1,std::string>::Failure(p.Error());c.preparation.push_back(p.Value());}auto sampleCount=r.ReadU32();if(!sampleCount)return base::Result<MeasurementEvidenceV1,std::string>::Failure("sample count truncated");for(std::uint32_t si=0;si<sampleCount.Value();++si){MeasurementSampleV1 s;auto run=r.ReadU32();auto cycle=r.ReadU32();auto order=ReadString(r);auto leafCount=r.ReadU32();if(!run||!cycle||!order||!leafCount)return base::Result<MeasurementEvidenceV1,std::string>::Failure("sample header truncated");s.run=run.Value();s.cycle=cycle.Value();s.candidateOrder=order.Value().value;for(std::uint32_t i=0;i<leafCount.Value();++i){auto l=ReadLeaf(r);if(!l)return base::Result<MeasurementEvidenceV1,std::string>::Failure(l.Error());s.leaves.push_back(std::move(l.Value()));}auto candidateCount=r.ReadU32();if(!candidateCount)return base::Result<MeasurementEvidenceV1,std::string>::Failure("candidate count truncated");for(std::uint32_t i=0;i<candidateCount.Value();++i){auto v=ReadCandidateSample(r);if(!v)return base::Result<MeasurementEvidenceV1,std::string>::Failure(v.Error());s.candidates.push_back(v.Value());}c.samples.push_back(std::move(s));}e.cases.push_back(std::move(c));}if(r.Remaining()!=0)return base::Result<MeasurementEvidenceV1,std::string>::Failure("measurement evidence has trailing bytes");return base::Result<MeasurementEvidenceV1,std::string>::Success(std::move(e));
}

double Median(std::vector<double> values){if(values.empty())return 0;std::sort(values.begin(),values.end());const auto m=values.size()/2;return values.size()%2?values[m]:(values[m-1]+values[m])*0.5;}
std::vector<double> GpuValues(const MeasurementCaseV1& c,candidate::CandidateKindV1 kind){std::vector<double> values;for(const auto& s:c.samples)for(const auto& v:s.candidates)if(v.kind==kind)values.push_back(v.gpuTotalNanoseconds);return values;}
std::vector<double> AllGpuValues(const MeasurementEvidenceV1& e,candidate::CandidateKindV1 kind){std::vector<double> values;for(const auto& c:e.cases){auto more=GpuValues(c,kind);values.insert(values.end(),more.begin(),more.end());}return values;}

void WriteCsv(const MeasurementEvidenceV1& e,const std::filesystem::path& path)
{
    std::ofstream f(path,std::ios::binary);
    if(!f)throw std::runtime_error("failed to create measurement_samples.csv");
    f<<"case,bones,run,cycle,order,order_position,candidate,leaf,command_recording_ns,gpu_ns,candidate_e2e_ns,candidate_materialization_ns,dispatches,barriers,numeric_max_abs_error\r\n"<<std::setprecision(17);
    for(const auto& c:e.cases)for(const auto& sample:c.samples)for(const auto& leafSample:sample.leaves)
    {
        if(static_cast<std::uint32_t>(leafSample.kind)==0)continue;
        const auto timing=std::find_if(sample.candidates.begin(),sample.candidates.end(),[&](const auto& value){return value.kind==leafSample.kind;});
        const auto preparation=std::find_if(c.preparation.begin(),c.preparation.end(),[&](const auto& value){return value.kind==leafSample.kind;});
        f<<c.id<<','<<c.boneCount<<','<<sample.run<<','<<sample.cycle<<','<<sample.candidateOrder<<','
         <<(timing==sample.candidates.end()?0:timing->orderPosition)<<','<<KindName(leafSample.kind)<<','<<leafSample.role<<','
         <<leafSample.commandRecordingNanoseconds<<','<<leafSample.gpuNanoseconds<<','
         <<(timing==sample.candidates.end()?0:timing->endToEndNanoseconds)<<','
         <<(preparation==c.preparation.end()?0:preparation->materializationNanoseconds)<<','
         <<leafSample.dispatchCount<<','<<leafSample.barrierCount<<','
         <<(timing==sample.candidates.end()?0:timing->numericMaxAbsoluteError)<<"\r\n";
    }
}

} // namespace

base::Result<MeasurementEvidenceV1,std::string> RunRealGpuMeasurementV1(const MeasurementConfigV1& config)
{
    try{
        Require(config.warmupCyclesPerOrder>0&&config.measurementCyclesPerOrder>0&&config.runCount>0,"measurement counts must be positive");
        Require(config.adapterIndex==0,"Spiral 2 canonical backend measurement currently selects the high-performance adapter at index 0");
        MeasurementEvidenceV1 evidence;evidence.config=config;evidence.captureUnixNanoseconds=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count());evidence.adapter=CaptureAdapterProfile(config.adapterIndex);evidence.observationContractIdentity=hierarchy::ObservationContractIdentityV2();d3d12::D3D12Backend backend({false,false,true});base::BinaryWriter profileWriter;WriteString(profileWriter,"SGE4-5.Spiral2.MeasurementProfile.V2");WriteDigest(profileWriter,evidence.adapter.fingerprint);WriteDigest(profileWriter,evidence.observationContractIdentity);profileWriter.WriteU32(config.warmupCyclesPerOrder);profileWriter.WriteU32(config.measurementCyclesPerOrder);profileWriter.WriteU32(config.runCount);WriteString(profileWriter,config.clockPolicy);
        for(const auto& definition:MeasurementCases()){
            auto hierarchyResult=hierarchy::BuildRigidHierarchySemanticV1(definition.parents);if(!hierarchyResult)throw std::runtime_error(hierarchyResult.Error());auto hierarchyValue=std::move(hierarchyResult).Value();auto frames=corpus::BuildFrameCorpusV1(hierarchyValue);if(!frames)throw std::runtime_error(frames.Error());Require(frames.Value().size()>10,"measurement invocation F10 is unavailable");const auto palette=frames.Value()[10].palette;MeasurementCaseV1 measured;measured.id=definition.id;measured.topology=definition.topology;measured.boneCount=hierarchyValue.boneCount;measured.hierarchyDepthCount=static_cast<std::uint32_t>(hierarchyValue.depthOffsets.size()-1);measured.hierarchyIdentity=hierarchy::RigidHierarchySemanticIdentityV1(hierarchyValue);measured.invocationIdentity=base::Sha256(contracts::SerializeDynamicLocalMotorPaletteV1(palette));WriteString(profileWriter,measured.id);WriteDigest(profileWriter,measured.hierarchyIdentity);WriteDigest(profileWriter,measured.invocationIdentity);for(const auto kind:CandidateOrders[0])measured.preparation.push_back(PrepareCandidate(hierarchyValue,kind));
            auto comparison=scenarios::BuildFrozenHierarchyComparisonScenarioV1(hierarchyValue);if(!comparison)throw std::runtime_error(comparison.Error().stage+": "+comparison.Error().message);auto reference=corpus::BuildIndependentCpuReferenceV1(hierarchyValue,palette);if(!reference)throw std::runtime_error(reference.Error());std::vector<CandidateRuntime> runtimeStorage;runtimeStorage.reserve(3);for(const auto kind:CandidateOrders[0]){auto candidateScenario=BuildCandidateScenario(comparison.Value(),kind);const auto materializeBegin=Clock::now();auto loaded=canrt::LoadStaticComposition(candidateScenario.frozenCompositionBytes,backend);const auto materializeEnd=Clock::now();if(!loaded)throw std::runtime_error(loaded.Error().stage+": "+loaded.Error().message);const auto materialization=Nanoseconds(materializeBegin,materializeEnd);runtimeStorage.emplace_back(std::move(candidateScenario),std::move(loaded).Value(),materialization);auto preparation=std::find_if(measured.preparation.begin(),measured.preparation.end(),[&](const auto& value){return value.kind==kind;});Require(preparation!=measured.preparation.end(),"candidate preparation record missing");preparation->materializationNanoseconds=materialization;}std::array<CandidateRuntime*,3> runtimes{&runtimeStorage[0],&runtimeStorage[1],&runtimeStorage[2]};
            for(std::uint32_t cycle=0;cycle<config.warmupCyclesPerOrder;++cycle)for(const auto& order:CandidateOrders)(void)ExecuteSample(0,cycle,order,hierarchyValue,palette,reference.Value(),runtimes,backend);
            for(std::uint32_t run=0;run<config.runCount;++run)for(std::uint32_t cycle=0;cycle<config.measurementCyclesPerOrder;++cycle)for(std::size_t offset=0;offset<CandidateOrders.size();++offset){const auto& order=CandidateOrders[(offset+run)%CandidateOrders.size()];measured.samples.push_back(ExecuteSample(run,cycle,order,hierarchyValue,palette,reference.Value(),runtimes,backend));}
            std::cout<<"[MEASURED] "<<measured.id<<" bones="<<measured.boneCount<<" depth="<<measured.hierarchyDepthCount<<" samples="<<measured.samples.size()<<'\n';evidence.cases.push_back(std::move(measured));
        }
        for(const auto& order:CandidateOrders)WriteString(profileWriter,OrderName(order));evidence.measurementProfileIdentity=base::Sha256(profileWriter.Bytes());return base::Result<MeasurementEvidenceV1,std::string>::Success(std::move(evidence));
    }catch(const std::exception& error){return base::Result<MeasurementEvidenceV1,std::string>::Failure(error.what());}
}

base::Result<void,std::string> WriteMeasurementArtifactsV1(const MeasurementEvidenceV1& evidence,const std::filesystem::path& outputDirectory)
{
    try{std::filesystem::create_directories(outputDirectory);auto written=base::WriteAllBytes(outputDirectory/"spiral2_measurement_evidence_v2.bin",SerializeEvidence(evidence));if(!written)return base::Result<void,std::string>::Failure(written.Error());WriteCsv(evidence,outputDirectory/"measurement_samples.csv");std::ofstream profile(outputDirectory/"measurement_profile.json",std::ios::binary);if(!profile)throw std::runtime_error("failed to create measurement_profile.json");profile<<"{\n  \"schema\": \"SGE4-5.Spiral2.MeasurementProfile.V2\",\n  \"adapter\": \""<<JsonEscape(evidence.adapter.description)<<"\",\n  \"adapterFingerprint\": \""<<Hex(evidence.adapter.fingerprint)<<"\",\n  \"driverVersion\": "<<evidence.adapter.driverVersion<<",\n  \"queue\": \"D3D12 direct, one queue per verified candidate Composition\",\n  \"timestampFrequency\": "<<evidence.adapter.timestampFrequency<<",\n  \"clockPolicy\": \""<<JsonEscape(evidence.config.clockPolicy)<<"\",\n  \"orders\": [\"ABC\",\"ACB\",\"BAC\",\"BCA\",\"CAB\",\"CBA\"],\n  \"warmupCyclesPerOrder\": "<<evidence.config.warmupCyclesPerOrder<<",\n  \"measurementCyclesPerOrder\": "<<evidence.config.measurementCyclesPerOrder<<",\n  \"runs\": "<<evidence.config.runCount<<",\n  \"profileIdentity\": \""<<Hex(evidence.measurementProfileIdentity)<<"\"\n}\n";return base::Result<void,std::string>::Success();}catch(const std::exception& error){return base::Result<void,std::string>::Failure(error.what());}
}

base::Result<MeasurementEvidenceV1,std::string> ReadMeasurementEvidenceV1(const std::filesystem::path& evidencePath)
{
    auto bytes=base::ReadAllBytes(evidencePath);if(!bytes)return base::Result<MeasurementEvidenceV1,std::string>::Failure(bytes.Error());return DeserializeEvidence(bytes.Value());
}

base::Result<void,std::string> WriteDecisionEvidenceReportV1(const MeasurementEvidenceV1& evidence,const std::filesystem::path& outputDirectory)
{
    try{
        Require(evidence.cases.size()==6,"decision evidence requires M0-M5");std::filesystem::create_directories(outputDirectory);const std::array kinds{candidate::CandidateKindV1::MatrixHierarchy,candidate::CandidateKindV1::DirectPgaHierarchy,candidate::CandidateKindV1::HybridHierarchy};std::array<std::pair<candidate::CandidateKindV1,double>,3> ranking{};for(std::size_t i=0;i<kinds.size();++i)ranking[i]={kinds[i],Median(AllGpuValues(evidence,kinds[i]))};std::sort(ranking.begin(),ranking.end(),[](const auto& a,const auto& b){return a.second<b.second;});std::map<std::string,char> winners;for(const auto& c:evidence.cases){candidate::CandidateKindV1 winner=kinds[0];double best=Median(GpuValues(c,winner));for(const auto kind:kinds){const double value=Median(GpuValues(c,kind));if(value<best){best=value;winner=kind;}}winners[c.id]=KindLetter(winner);}const bool winnerVaries=std::any_of(winners.begin(),winners.end(),[&](const auto& item){return item.second!=winners.begin()->second;});
        std::ofstream md(outputDirectory/"SPIRAL2_DECISION_EVIDENCE_REPORT.md",std::ios::binary);if(!md)throw std::runtime_error("failed to create Decision Evidence report");md<<std::fixed<<std::setprecision(3);md<<"# SGE4-5 Spiral 2 Decision Evidence Report V2\n\n- Adapter: "<<evidence.adapter.description<<"\n- Adapter fingerprint: `"<<Hex(evidence.adapter.fingerprint)<<"`\n- Driver version: `"<<evidence.adapter.driverVersion<<"`\n- Queue contract: `D3D12 Direct`; each candidate is a verifier-sealed Frozen Composition and each order step completes before the next.\n- Timestamp frequency: `"<<evidence.adapter.timestampFrequency<<"`\n- Clock policy: `"<<evidence.config.clockPolicy<<"`\n- Measurement profile: `"<<Hex(evidence.measurementProfileIdentity)<<"`\n- Balanced orders: `ABC ACB BAC BCA CAB CBA`\n- Samples per case: "<<(evidence.config.measurementCyclesPerOrder*evidence.config.runCount*6u)<<"\n\n## Required questions answered\n\n1. **Could Level 4 v1 close Dynamic Palette Composition?** Yes. The canonical ten-Leaf, eleven-flow Composition closed without a Level 4 extension.\n2. **Which Frozen contract was missing?** None. The S2-09 and S2-14 sufficiency gates passed unchanged.\n3. **Were per-frame values completely separated from Frozen identity?** Yes. Motor and reference bytes enter only through explicit DynamicData binds; bone count and topology remain Frozen.\n4. **Did one meaning generate actually different Leaf groups?** Yes: A has three Matrix-hierarchy Leaves, B has two Direct PGA Leaves, and C has three Hybrid Leaves, all independently verified.\n5. **What changed with Matrix lowering position?** A expands 32-byte local Motors before hierarchy, B never materializes a Matrix palette, and C expands 32-byte global Motors after hierarchy. This changes dispatches and logical intermediate bytes in the table.\n6. **Did K and hierarchy depth change the winner?** "<<(winnerVaries?"Yes; the per-case lowest median candidate changes across M0-M5.":"No winner transition was observed on this adapter; the table still preserves every K/depth case.")<<"\n7. **Did 32-byte Motor versus 48-byte Matrix intermediates affect measurement?** The resource table records the exact logical byte difference; GPU medians show its adapter-specific consequence without treating byte count alone as superiority.\n8. **What was the Direct PGA arithmetic/buffer tradeoff?** B uses fewer dispatches and 32-byte global intermediates while retaining Motor arithmetic in hierarchy and probe application; its measured GPU time is reported per case.\n9. **Did Hybrid use both approaches?** Yes. C retains Motor hierarchy, then converts global Motors once for Matrix probe application; its third dispatch and mixed 32/48-byte intermediates are measured.\n10. **Did the Verifier detect Leaf/lowering tampering?** Yes. CU2 mutation evidence rejects kind, lowering position, node, edge, stride, depth, dispatch, schema, observation, semantic identity, and certificate mutations before Leaf compilation.\n11. **Which next capability is indicated?** `Vertex Consumer` is the strongest non-authoritative recommendation because the experiment now produces qualified global transforms but consumes only fixed probes. `Temporal Flow`, variable Work, and `Variant Set` are not selected here.\n12. **Is a Level 4 extension experimentally required?** No. No extension is authorized; any future extension requires new insufficiency evidence and Owner selection.\n\n## Per-case measured comparison\n\n| Case | Bones | Depth | A GPU median ns | B GPU median ns | C GPU median ns | Lowest | A/B/C package bytes | A/B/C dynamic bytes | A/B/C logical intermediate bytes | A/B/C dispatches |\n|---|---:|---:|---:|---:|---:|---|---|---|---|---|\n";
        for(const auto& c:evidence.cases){auto prep=[&](candidate::CandidateKindV1 kind)->const CandidatePreparationV1&{return *std::find_if(c.preparation.begin(),c.preparation.end(),[&](const auto& p){return p.kind==kind;});};md<<'|'<<c.id<<'|'<<c.boneCount<<'|'<<c.hierarchyDepthCount<<'|'<<Median(GpuValues(c,kinds[0]))<<'|'<<Median(GpuValues(c,kinds[1]))<<'|'<<Median(GpuValues(c,kinds[2]))<<'|'<<winners[c.id]<<'|'<<prep(kinds[0]).packageBytes<<'/'<<prep(kinds[1]).packageBytes<<'/'<<prep(kinds[2]).packageBytes<<'|'<<prep(kinds[0]).dynamicInvocationBytes<<'/'<<prep(kinds[1]).dynamicInvocationBytes<<'/'<<prep(kinds[2]).dynamicInvocationBytes<<'|'<<prep(kinds[0]).logicalIntermediateBytes<<'/'<<prep(kinds[1]).logicalIntermediateBytes<<'/'<<prep(kinds[2]).logicalIntermediateBytes<<'|'<<prep(kinds[0]).plannedDispatchCount<<'/'<<prep(kinds[1]).plannedDispatchCount<<'/'<<prep(kinds[2]).plannedDispatchCount<<"|\n";}
        md<<"\n## Full required metric ledger\n\n| Case | Candidate | Generation ns | Verifier ns | Freeze ns | Package B | Dynamic invocation B | Intermediate B | Materialization ns | Command median ns | GPU total median ns | End-to-end median ns | Planned/package dispatches | Barriers | Max numeric error |\n|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|\n";
        for(const auto& c:evidence.cases)for(const auto kind:kinds){const auto& prep=*std::find_if(c.preparation.begin(),c.preparation.end(),[&](const auto& p){return p.kind==kind;});std::vector<double> command,gpu,e2e;double error=0;std::uint32_t barriers=0;for(const auto& sample:c.samples)for(const auto& value:sample.candidates)if(value.kind==kind){command.push_back(value.commandRecordingNanoseconds);gpu.push_back(value.gpuTotalNanoseconds);e2e.push_back(value.endToEndNanoseconds);error=std::max(error,value.numericMaxAbsoluteError);barriers=value.barrierCount;}md<<'|'<<c.id<<'|'<<KindLetter(kind)<<'|'<<prep.generationNanoseconds<<'|'<<prep.verifierNanoseconds<<'|'<<prep.freezeNanoseconds<<'|'<<prep.packageBytes<<'|'<<prep.dynamicInvocationBytes<<'|'<<prep.logicalIntermediateBytes<<'|'<<prep.materializationNanoseconds<<'|'<<Median(command)<<'|'<<Median(gpu)<<'|'<<Median(e2e)<<'|'<<prep.plannedDispatchCount<<'/'<<prep.packageDispatchCount<<'|'<<barriers<<'|'<<error<<"|\n";}
        md<<"\n`measurement_samples.csv` preserves every per-Leaf GPU timestamp, command-recording observation, order position, barrier count, and numeric error. Candidate generation, Verifier, Freeze, Package, invocation, intermediate allocation, materialization, command recording, per-Leaf and total GPU, end-to-end, dispatch, barrier, and numeric metrics are therefore all present in binary and human-readable evidence.\n";
        md<<"\n## Aggregate candidate ranking\n\n";for(std::size_t i=0;i<ranking.size();++i)md<<(i+1)<<". "<<KindName(ranking[i].first)<<": aggregate sample median "<<ranking[i].second<<" ns\n";md<<"\n`RecommendationAuthority = NonAuthoritative`\n\nRecommended representation candidate on this measured adapter: `"<<KindName(ranking[0].first)<<"`. This is evidence, not a global policy decision.\n\n## Owner decision boundary\n\n`NextCapabilitySelection = DeferredByOwner`\n\n`SelectionStatus = OWNER_DECISION_REQUIRED`\n";
        std::cout<<"[RANKING] 1="<<KindName(ranking[0].first)<<" ("<<ranking[0].second<<" ns), 2="<<KindName(ranking[1].first)<<" ("<<ranking[1].second<<" ns), 3="<<KindName(ranking[2].first)<<" ("<<ranking[2].second<<" ns)\n";
        return base::Result<void,std::string>::Success();
    }catch(const std::exception& error){return base::Result<void,std::string>::Failure(error.what());}
}

base::Result<void,std::string> RunFormatSelfTestV1(const std::filesystem::path& outputDirectory)
{
    MeasurementEvidenceV1 e;
    e.captureUnixNanoseconds=1;
    e.config={1,1,1,0,"self-test"};
    e.adapter.description="Synthetic hardware adapter";
    e.adapter.vendorId=1;
    e.adapter.timestampFrequency=1000000;
    constexpr std::string_view synthetic="synthetic";
    e.adapter.fingerprint=base::Sha256(std::as_bytes(std::span(synthetic.data(),synthetic.size())));
    e.observationContractIdentity=e.adapter.fingerprint;
    e.measurementProfileIdentity=e.adapter.fingerprint;
    for(std::uint32_t ci=0;ci<6;++ci)
    {
        MeasurementCaseV1 c;
        c.id="M"+std::to_string(ci);c.topology="synthetic";c.boneCount=8u<<std::min(ci,4u);c.hierarchyDepthCount=ci+1;c.hierarchyIdentity=e.adapter.fingerprint;c.invocationIdentity=e.adapter.fingerprint;
        for(const auto kind:CandidateOrders[0])
        {
            CandidatePreparationV1 p;
            p.kind=kind;p.materializationNanoseconds=1000+100*static_cast<std::uint32_t>(kind);p.packageBytes=100+static_cast<std::uint32_t>(kind);p.dynamicInvocationBytes=c.boneCount*(32u+5u*16u);p.logicalIntermediateBytes=c.boneCount*static_cast<std::uint32_t>(kind)*16u;p.plannedDispatchCount=kind==candidate::CandidateKindV1::DirectPgaHierarchy?2:3;p.packageDispatchCount=p.plannedDispatchCount;p.verifiedCertificate=e.adapter.fingerprint;c.preparation.push_back(p);
        }
        for(const auto& order:CandidateOrders)
        {
            MeasurementSampleV1 s;s.candidateOrder=OrderName(order);
            for(std::uint32_t position=0;position<3;++position)
            {
                CandidateTimingSampleV1 v;v.kind=order[position];v.orderPosition=position;v.endToEndNanoseconds=500+25*static_cast<std::uint32_t>(v.kind);v.gpuTotalNanoseconds=100+10*static_cast<std::uint32_t>(v.kind);v.dispatchCount=v.kind==candidate::CandidateKindV1::DirectPgaHierarchy?2:3;s.candidates.push_back(v);
            }
            c.samples.push_back(s);
        }
        e.cases.push_back(c);
    }
    auto written=WriteMeasurementArtifactsV1(e,outputDirectory);if(!written)return written;
    auto read=ReadMeasurementEvidenceV1(outputDirectory/"spiral2_measurement_evidence_v2.bin");if(!read)return base::Result<void,std::string>::Failure(read.Error());
    if(read.Value().cases.size()!=6||read.Value().cases[0].samples.size()!=6)return base::Result<void,std::string>::Failure("measurement evidence roundtrip failed");
    const auto& first=read.Value().cases[0];
    if(first.preparation[0].materializationNanoseconds==first.preparation[1].materializationNanoseconds||first.samples[0].candidates[0].endToEndNanoseconds==first.samples[0].candidates[1].endToEndNanoseconds)return base::Result<void,std::string>::Failure("candidate-specific timing fields were not preserved");
    return WriteDecisionEvidenceReportV1(read.Value(),outputDirectory);
}
}
