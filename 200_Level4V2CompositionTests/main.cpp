#include "../192_Level4V2CandidatePlanner/CandidatePlanner.h"
#include "../193_Level4V2IndependentVerifier/IndependentVerifier.h"
#include "../194_Level4V2FrozenAuthority/FrozenAuthority.h"
#include "../197_Level4V2CompositionPlanner/CompositionPlanner.h"
#include "../198_Level4V2CompositionVerifier/CompositionVerifier.h"
#include "../199_Level4V2FrozenComposition/FrozenComposition.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace c=sge4_5::v2::canonical;
namespace a=sge4_5::v2::authority;
namespace fa=sge4_5::v2::frozen;
namespace comp=sge4_5::v2::composition;
namespace fc=sge4_5::v2::composition::frozen_composition_detail;

namespace
{
std::vector<std::byte> Bytes(std::initializer_list<std::uint8_t> values)
{
    std::vector<std::byte> out; for(auto v:values) out.push_back(static_cast<std::byte>(v)); return out;
}
std::vector<std::byte> SeedBytes(std::uint8_t seed,std::uint8_t domain)
{
    return Bytes({domain,seed,static_cast<std::uint8_t>(seed^0x5au),static_cast<std::uint8_t>(seed+17u)});
}

fa::OpaqueFrozenArtifactV1 MakeFrozenAuthority(std::uint8_t seed,std::uint8_t targetSeed,std::uint8_t resourceSeed)
{
    auto semanticPayload=SeedBytes(seed,1u);
    auto semantic=a::MakeSemanticDefinitionV1(c::SemanticVersion(1u),semanticPayload);
    auto epoch=c::DeviceEpoch::TryCreate(1u); if(!epoch) throw std::runtime_error("epoch");
    c::DynamicExecutionDimensionsV1 dims; dims.activeCount=c::ActiveCount(static_cast<std::uint32_t>(seed)+1u);
    auto invocation=a::MakeInvocationDescriptorV1(semantic.descriptor.identity,c::TimelineOrdinal(seed),*epoch,dims);
    auto targetPayload=SeedBytes(targetSeed,2u); auto resourcePayload=SeedBytes(resourceSeed,3u); auto writePayload=SeedBytes(seed,4u);
    a::AuthorityRequestV1 request{semantic,invocation,a::MakeTargetProfileDefinitionV1(1u,targetPayload),a::MakeResourceContractDefinitionV1(1u,resourcePayload),a::MakeWriteSetDefinitionV1(1u,writePayload)};
    a::ExplicitCandidateInputV1 candidate{1u,SeedBytes(seed,5u)};
    auto proposal=a::CandidatePlannerV1::Plan(request,candidate);
    auto verified=a::IndependentVerifierV1::Verify(request,candidate,proposal);
    if(!verified.Accepted()) throw std::runtime_error("authority verification failed");
    return fa::FrozenAuthorityBuilderV1::Freeze(*verified.verified);
}

comp::LeafAuthorityV1 MakeLeaf(std::uint8_t seed,std::uint8_t targetSeed=9u,std::uint8_t resourceSeed=7u)
{
    auto artifact=MakeFrozenAuthority(seed,targetSeed,resourceSeed);
    auto key=SeedBytes(seed,10u);
    return comp::MakeLeafAuthorityV1(1u,key,artifact);
}

comp::EndpointAuthorityV1 MakeEndpoint(const comp::LeafAuthorityV1& leaf,std::uint8_t keySeed,comp::EndpointAccessV1 access,bool presenter=false,comp::ResourceKindV1 kind=comp::ResourceKindV1::Buffer)
{
    auto key=SeedBytes(keySeed,11u);
    const auto incoming=access==comp::EndpointAccessV1::ReadOnly?comp::ResourceStateV1::Read:comp::ResourceStateV1::Common;
    const auto outgoing=access==comp::EndpointAccessV1::WriteOnly?(presenter?comp::ResourceStateV1::Present:comp::ResourceStateV1::Write):comp::ResourceStateV1::Read;
    return comp::MakeEndpointAuthorityV1(leaf,1u,key,kind,access,static_cast<std::uint64_t>(keySeed+1u)*16u,incoming,outgoing,true,true,presenter);
}

comp::FlowDeclarationV1 MakeFlow(std::uint8_t seed,comp::FlowBoundaryV1 boundary,std::initializer_list<comp::EndpointIdentity> endpoints)
{
    auto key=SeedBytes(seed,12u); std::vector<comp::EndpointIdentity> ids(endpoints);
    return comp::MakeFlowDeclarationV1(1u,key,boundary,ids);
}

comp::RawCompositionContractV1 BuildLinear(bool presenter=false,bool reverse=false)
{
    auto aLeaf=MakeLeaf(1u), bLeaf=MakeLeaf(2u);
    auto aIn=MakeEndpoint(aLeaf,1u,comp::EndpointAccessV1::ReadOnly);
    auto aOut=MakeEndpoint(aLeaf,2u,comp::EndpointAccessV1::WriteOnly);
    auto bIn=MakeEndpoint(bLeaf,3u,comp::EndpointAccessV1::ReadOnly);
    auto bOut=MakeEndpoint(bLeaf,4u,comp::EndpointAccessV1::WriteOnly,presenter);
    auto f0=MakeFlow(1u,comp::FlowBoundaryV1::CompositionInput,{aIn.Identity()});
    auto f1=MakeFlow(2u,comp::FlowBoundaryV1::Internal,{aOut.Identity(),bIn.Identity()});
    auto f2=MakeFlow(3u,comp::FlowBoundaryV1::CompositionOutput,{bOut.Identity()});
    std::vector<comp::LeafAuthorityV1> leaves{aLeaf,bLeaf};
    std::vector<comp::EndpointAuthorityV1> endpoints{aIn,aOut,bIn,bOut};
    std::vector<comp::FlowDeclarationV1> flows{f0,f1,f2};
    if(reverse){std::reverse(leaves.begin(),leaves.end());std::reverse(endpoints.begin(),endpoints.end());std::reverse(flows.begin(),flows.end());for(auto& f:flows)std::reverse(f.endpoints.begin(),f.endpoints.end());}
    return comp::MakeRawCompositionContractV1(std::move(leaves),std::move(endpoints),std::move(flows));
}

comp::RawCompositionContractV1 BuildFanOut()
{
    auto p=MakeLeaf(3u),b=MakeLeaf(4u),cLeaf=MakeLeaf(5u);
    auto pOut=MakeEndpoint(p,10u,comp::EndpointAccessV1::WriteOnly);
    auto bIn=MakeEndpoint(b,11u,comp::EndpointAccessV1::ReadOnly);
    auto cIn=MakeEndpoint(cLeaf,12u,comp::EndpointAccessV1::ReadOnly);
    auto f=MakeFlow(10u,comp::FlowBoundaryV1::Internal,{pOut.Identity(),bIn.Identity(),cIn.Identity()});
    return comp::MakeRawCompositionContractV1({p,b,cLeaf},{pOut,bIn,cIn},{f});
}

comp::RawCompositionContractV1 BuildMultiInput()
{
    auto aLeaf=MakeLeaf(6u),bLeaf=MakeLeaf(7u),cLeaf=MakeLeaf(8u);
    auto aOut=MakeEndpoint(aLeaf,20u,comp::EndpointAccessV1::WriteOnly);
    auto bOut=MakeEndpoint(bLeaf,21u,comp::EndpointAccessV1::WriteOnly);
    auto cIn0=MakeEndpoint(cLeaf,22u,comp::EndpointAccessV1::ReadOnly);
    auto cIn1=MakeEndpoint(cLeaf,23u,comp::EndpointAccessV1::ReadOnly);
    auto f0=MakeFlow(20u,comp::FlowBoundaryV1::Internal,{aOut.Identity(),cIn0.Identity()});
    auto f1=MakeFlow(21u,comp::FlowBoundaryV1::Internal,{bOut.Identity(),cIn1.Identity()});
    return comp::MakeRawCompositionContractV1({aLeaf,bLeaf,cLeaf},{aOut,bOut,cIn0,cIn1},{f0,f1});
}

comp::RawCompositionContractV1 BuildDiamond()
{
    auto aLeaf=MakeLeaf(9u),bLeaf=MakeLeaf(10u),cLeaf=MakeLeaf(11u),dLeaf=MakeLeaf(12u);
    auto aOut=MakeEndpoint(aLeaf,30u,comp::EndpointAccessV1::WriteOnly);
    auto bIn=MakeEndpoint(bLeaf,31u,comp::EndpointAccessV1::ReadOnly);
    auto cIn=MakeEndpoint(cLeaf,32u,comp::EndpointAccessV1::ReadOnly);
    auto bOut=MakeEndpoint(bLeaf,33u,comp::EndpointAccessV1::WriteOnly);
    auto cOut=MakeEndpoint(cLeaf,34u,comp::EndpointAccessV1::WriteOnly);
    auto dIn0=MakeEndpoint(dLeaf,35u,comp::EndpointAccessV1::ReadOnly);
    auto dIn1=MakeEndpoint(dLeaf,36u,comp::EndpointAccessV1::ReadOnly);
    auto f0=MakeFlow(30u,comp::FlowBoundaryV1::Internal,{aOut.Identity(),bIn.Identity(),cIn.Identity()});
    auto f1=MakeFlow(31u,comp::FlowBoundaryV1::Internal,{bOut.Identity(),dIn0.Identity()});
    auto f2=MakeFlow(32u,comp::FlowBoundaryV1::Internal,{cOut.Identity(),dIn1.Identity()});
    return comp::MakeRawCompositionContractV1({aLeaf,bLeaf,cLeaf,dLeaf},{aOut,bIn,cIn,bOut,cOut,dIn0,dIn1},{f0,f1,f2});
}

comp::RawCompositionContractV1 BuildIndependent()
{
    auto aLeaf=MakeLeaf(13u),bLeaf=MakeLeaf(14u);
    auto aIn=MakeEndpoint(aLeaf,40u,comp::EndpointAccessV1::ReadOnly);
    auto aOut=MakeEndpoint(aLeaf,41u,comp::EndpointAccessV1::WriteOnly);
    auto bIn=MakeEndpoint(bLeaf,42u,comp::EndpointAccessV1::ReadOnly);
    auto bOut=MakeEndpoint(bLeaf,43u,comp::EndpointAccessV1::WriteOnly);
    return comp::MakeRawCompositionContractV1({aLeaf,bLeaf},{aIn,aOut,bIn,bOut},{
        MakeFlow(40u,comp::FlowBoundaryV1::CompositionInput,{aIn.Identity()}),
        MakeFlow(41u,comp::FlowBoundaryV1::CompositionOutput,{aOut.Identity()}),
        MakeFlow(42u,comp::FlowBoundaryV1::CompositionInput,{bIn.Identity()}),
        MakeFlow(43u,comp::FlowBoundaryV1::CompositionOutput,{bOut.Identity()})});
}

fc::OpaqueFrozenCompositionV1 VerifyAndFreeze(const comp::RawCompositionContractV1& contract)
{
    auto planned=comp::CompositionPlannerV1::Plan(contract); if(!planned.Planned()) throw std::runtime_error("planning failed");
    auto verified=comp::CompositionVerifierV1::Verify(contract,*planned.proposal); if(!verified.Accepted()) throw std::runtime_error("composition verification failed");
    return fc::FrozenCompositionBuilderV1::Freeze(*verified.verified);
}

void ExpectError(const comp::RawCompositionContractV1& contract,const comp::CompositionPlanProposalV1& proposal,comp::CompositionVerificationErrorV1 expected,std::vector<std::uint8_t>& observed)
{
    auto result=comp::CompositionVerifierV1::Verify(contract,proposal);
    if(result.error!=expected || result.verified.has_value()) throw std::runtime_error("unexpected rejection code");
    observed.push_back(static_cast<std::uint8_t>(result.error));
}

void AppendU32(std::vector<std::byte>& out,std::uint32_t value){for(std::uint32_t i=0;i<4u;++i)out.push_back(static_cast<std::byte>((value>>(i*8u))&0xffu));}
void AppendDigest(std::vector<std::byte>& out,const c::Digest256V1& digest){out.insert(out.end(),digest.begin(),digest.end());}
void Emit(const std::string& path,const std::vector<std::byte>& bytes){std::ofstream f(path,std::ios::binary);if(!f)throw std::runtime_error("open evidence");f.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));if(!f)throw std::runtime_error("write evidence");}
}

int main(int argc,char** argv)
{
    try
    {
        static_assert(!std::is_default_constructible_v<comp::VerifiedCompositionV1>);
        static_assert(!std::is_constructible_v<comp::VerifiedCompositionV1,comp::CompositionPlanProposalV1>);
        static_assert(!std::is_constructible_v<fc::OpaqueFrozenCompositionV1,comp::CompositionPlanProposalV1>);

        std::string emitPath;
        for(int i=1;i<argc;++i){const std::string arg=argv[i];if(arg=="--emit"&&i+1<argc)emitPath=argv[++i];}
        if(emitPath.empty()) throw std::runtime_error("--emit is required");

        std::vector<fc::OpaqueFrozenCompositionV1> accepted;
        accepted.push_back(VerifyAndFreeze(BuildIndependent()));
        accepted.push_back(VerifyAndFreeze(BuildLinear(false,false)));
        accepted.push_back(VerifyAndFreeze(BuildFanOut()));
        accepted.push_back(VerifyAndFreeze(BuildMultiInput()));
        accepted.push_back(VerifyAndFreeze(BuildDiamond()));
        accepted.push_back(VerifyAndFreeze(BuildLinear(false,true)));
        accepted.push_back(VerifyAndFreeze(BuildLinear(true,false)));

        auto canonical=VerifyAndFreeze(BuildLinear(false,false));
        auto reversed=VerifyAndFreeze(BuildLinear(false,true));
        if(canonical.Identity()!=reversed.Identity()) throw std::runtime_error("declaration order changed frozen identity");

        auto valid=BuildLinear(true,false);
        auto planned=comp::CompositionPlannerV1::Plan(valid); if(!planned.Planned()) throw std::runtime_error("baseline plan");
        auto base=*planned.proposal;
        std::vector<std::uint8_t> observed; observed.reserve(24u);

        {auto x=valid;x.leaves.push_back(x.leaves.front());ExpectError(x,base,comp::CompositionVerificationErrorV1::DuplicateLeaf,observed);}
        {auto x=valid;x.endpoints.push_back(x.endpoints.front());ExpectError(x,base,comp::CompositionVerificationErrorV1::DuplicateEndpoint,observed);}
        {auto x=valid;x.flows.push_back(x.flows.front());ExpectError(x,base,comp::CompositionVerificationErrorV1::DuplicateFlow,observed);}
        {auto x=valid;auto bad=MakeEndpoint(x.leaves.front(),90u,comp::EndpointAccessV1::ReadOnly,false,comp::ResourceKindV1::Unsupported);x.endpoints.push_back(bad);x.flows.push_back(MakeFlow(90u,comp::FlowBoundaryV1::CompositionInput,{bad.Identity()}));x.identity=comp::ComputeCompositionContractIdentityV1(x);ExpectError(x,base,comp::CompositionVerificationErrorV1::UnsupportedResourceKind,observed);}
        {auto x=valid;x.flows.erase(x.flows.begin());x.identity=comp::ComputeCompositionContractIdentityV1(x);ExpectError(x,base,comp::CompositionVerificationErrorV1::RequiredEndpointUnbound,observed);}
        {auto x=valid;auto extra=MakeEndpoint(x.leaves.back(),91u,comp::EndpointAccessV1::WriteOnly);x.endpoints.push_back(extra);x.flows[1]=MakeFlow(2u,comp::FlowBoundaryV1::Internal,{x.endpoints[1].Identity(),x.endpoints[2].Identity(),extra.Identity()});x.identity=comp::ComputeCompositionContractIdentityV1(x);ExpectError(x,base,comp::CompositionVerificationErrorV1::MultipleWriters,observed);}
        {auto x=valid;x.flows[0].boundary=comp::FlowBoundaryV1::Internal;x.flows[0].identity=comp::ComputeFlowIdentityV1(x.flows[0]);x.identity=comp::ComputeCompositionContractIdentityV1(x);ExpectError(x,base,comp::CompositionVerificationErrorV1::InvalidFlowShape,observed);}
        {auto x=valid;auto backOut=MakeEndpoint(x.leaves.back(),92u,comp::EndpointAccessV1::WriteOnly);auto backIn=MakeEndpoint(x.leaves.front(),93u,comp::EndpointAccessV1::ReadOnly);x.endpoints.push_back(backOut);x.endpoints.push_back(backIn);x.flows.push_back(MakeFlow(92u,comp::FlowBoundaryV1::Internal,{backOut.Identity(),backIn.Identity()}));x.identity=comp::ComputeCompositionContractIdentityV1(x);auto cyclePlan=base;cyclePlan.contractIdentity=x.identity;ExpectError(x,cyclePlan,comp::CompositionVerificationErrorV1::GraphCycle,observed);}
        {auto x=valid;auto p=MakeEndpoint(x.leaves.front(),94u,comp::EndpointAccessV1::WriteOnly,true);x.endpoints.push_back(p);x.flows.push_back(MakeFlow(94u,comp::FlowBoundaryV1::CompositionOutput,{p.Identity()}));x.identity=comp::ComputeCompositionContractIdentityV1(x);ExpectError(x,base,comp::CompositionVerificationErrorV1::MultiplePresenters,observed);}
        {auto x=valid;x.leaves.push_back(MakeLeaf(99u,10u));x.identity=comp::ComputeCompositionContractIdentityV1(x);ExpectError(x,base,comp::CompositionVerificationErrorV1::TargetProfileMismatch,observed);}
        {auto x=valid;x.flows.push_back(MakeFlow(95u,comp::FlowBoundaryV1::CompositionInput,{x.endpoints.front().Identity()}));x.identity=comp::ComputeCompositionContractIdentityV1(x);ExpectError(x,base,comp::CompositionVerificationErrorV1::EndpointBoundMoreThanOnce,observed);}
        {auto x=valid;x.flows.front().endpoints.front()=comp::EndpointIdentity::FromDigest({});x.flows.front().identity=comp::ComputeFlowIdentityV1(x.flows.front());x.identity=comp::ComputeCompositionContractIdentityV1(x);ExpectError(x,base,comp::CompositionVerificationErrorV1::FlowReferencesUnknownEndpoint,observed);}
        {auto x=valid;x.flows.front().identity=comp::FlowIdentity::FromDigest({});x.identity=comp::ComputeCompositionContractIdentityV1(x);ExpectError(x,base,comp::CompositionVerificationErrorV1::FlowIdentityMismatch,observed);}
        {auto x=valid;x.identity=comp::CompositionContractIdentity::FromDigest({});ExpectError(x,base,comp::CompositionVerificationErrorV1::ContractIdentityMismatch,observed);}

        {auto p=base;p.allocations.front().sizeBytes+=1u;ExpectError(valid,p,comp::CompositionVerificationErrorV1::AllocationMismatch,observed);}
        {auto p=base;p.allocations.front().identity=comp::AllocationIdentity::FromDigest({});ExpectError(valid,p,comp::CompositionVerificationErrorV1::AllocationMismatch,observed);}
        {auto p=base;std::swap(p.schedule[0],p.schedule[1]);ExpectError(valid,p,comp::CompositionVerificationErrorV1::ScheduleMismatch,observed);}
        {auto p=base;p.bindings.front().flowIdentity=comp::FlowIdentity::FromDigest({});ExpectError(valid,p,comp::CompositionVerificationErrorV1::BindingMismatch,observed);}
        {auto p=base;p.handoffs.front().consumerIncomingState=comp::ResourceStateV1::Common;ExpectError(valid,p,comp::CompositionVerificationErrorV1::HandoffMismatch,observed);}
        {auto p=base;p.synchronizations.front().waitOrdinal=comp::WaitOrdinal(55u);ExpectError(valid,p,comp::CompositionVerificationErrorV1::SynchronizationMismatch,observed);}
        {auto p=base;p.presenter.reset();ExpectError(valid,p,comp::CompositionVerificationErrorV1::PresenterMismatch,observed);}
        {auto p=base;p.recoverySet.flows.pop_back();ExpectError(valid,p,comp::CompositionVerificationErrorV1::RecoveryMismatch,observed);}
        {auto p=base;p.identity=comp::CompositionPlanIdentity::FromDigest({});ExpectError(valid,p,comp::CompositionVerificationErrorV1::PlanIdentityMismatch,observed);}
        {auto p=base;p.contractIdentity=comp::CompositionContractIdentity::FromDigest({});ExpectError(valid,p,comp::CompositionVerificationErrorV1::ContractIdentityMismatch,observed);}

        if(observed.size()!=24u) throw std::runtime_error("rejection count");
        std::vector<std::byte> evidence;
        const std::array<char,8> magic{'L','4','V','2','R','3','V','1'}; for(char ch:magic)evidence.push_back(static_cast<std::byte>(ch));
        AppendU32(evidence,static_cast<std::uint32_t>(accepted.size())); AppendU32(evidence,static_cast<std::uint32_t>(observed.size()));
        for(const auto& item:accepted){AppendDigest(evidence,item.Identity().Digest());AppendU32(evidence,item.LeafCount());AppendU32(evidence,item.FlowCount());}
        AppendDigest(evidence,canonical.Identity().Digest()); AppendDigest(evidence,reversed.Identity().Digest());
        for(auto code:observed)evidence.push_back(static_cast<std::byte>(code));
        Emit(emitPath,evidence);

        std::cout<<"Level 4 v2 R3 finite Buffer DAG composition self-test passed.\n";
        std::cout<<"Migrated graph scenarios: 7. Mutation/contract rejections: 24.\n";
        std::cout<<"Planner-independent composition verification and verified-only freeze passed.\n";
        std::cout<<"Runtime capability added: None.\n";
        return 0;
    }
    catch(const std::exception& e){std::cerr<<"R3 failure: "<<e.what()<<"\n";return 1;}
}
