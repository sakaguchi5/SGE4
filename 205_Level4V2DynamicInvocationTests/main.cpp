#include "../192_Level4V2CandidatePlanner/CandidatePlanner.h"
#include "../193_Level4V2IndependentVerifier/IndependentVerifier.h"
#include "../194_Level4V2FrozenAuthority/FrozenAuthority.h"
#include "../197_Level4V2CompositionPlanner/CompositionPlanner.h"
#include "../198_Level4V2CompositionVerifier/CompositionVerifier.h"
#include "../199_Level4V2FrozenComposition/FrozenComposition.h"
#include "../202_Level4V2DynamicInvocationPlanner/DynamicInvocationPlanner.h"
#include "../203_Level4V2DynamicInvocationVerifier/DynamicInvocationVerifier.h"
#include "../204_Level4V2FrozenInvocationHistory/FrozenInvocationHistory.h"

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
namespace dyn=sge4_5::v2::dynamic;
namespace fd=sge4_5::v2::dynamic::frozen_dynamic_detail;

namespace
{
std::vector<std::byte> Bytes(std::initializer_list<std::uint8_t> values)
{
    std::vector<std::byte> out; for(auto value:values) out.push_back(static_cast<std::byte>(value)); return out;
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
    c::DynamicExecutionDimensionsV1 dimensions; dimensions.activeCount=c::ActiveCount(static_cast<std::uint32_t>(seed)+1u);
    auto invocation=a::MakeInvocationDescriptorV1(semantic.descriptor.identity,c::TimelineOrdinal(seed),*epoch,dimensions);
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

comp::EndpointAuthorityV1 MakeEndpoint(const comp::LeafAuthorityV1& leaf,std::uint8_t keySeed,comp::EndpointAccessV1 access)
{
    auto key=SeedBytes(keySeed,11u);
    const auto incoming=access==comp::EndpointAccessV1::ReadOnly?comp::ResourceStateV1::Read:comp::ResourceStateV1::Common;
    const auto outgoing=access==comp::EndpointAccessV1::WriteOnly?comp::ResourceStateV1::Write:comp::ResourceStateV1::Read;
    return comp::MakeEndpointAuthorityV1(leaf,1u,key,comp::ResourceKindV1::Buffer,access,static_cast<std::uint64_t>(keySeed+1u)*16u,incoming,outgoing,true,true,false);
}

comp::FlowDeclarationV1 MakeFlow(std::uint8_t seed,comp::FlowBoundaryV1 boundary,std::initializer_list<comp::EndpointIdentity> endpoints)
{
    auto key=SeedBytes(seed,12u); std::vector<comp::EndpointIdentity> ids(endpoints);
    return comp::MakeFlowDeclarationV1(1u,key,boundary,ids);
}

fc::OpaqueFrozenCompositionV1 MakeFrozenComposition(std::uint8_t seedOffset)
{
    auto first=MakeLeaf(static_cast<std::uint8_t>(1u+seedOffset));
    auto second=MakeLeaf(static_cast<std::uint8_t>(2u+seedOffset));
    auto firstIn=MakeEndpoint(first,static_cast<std::uint8_t>(1u+seedOffset),comp::EndpointAccessV1::ReadOnly);
    auto firstOut=MakeEndpoint(first,static_cast<std::uint8_t>(2u+seedOffset),comp::EndpointAccessV1::WriteOnly);
    auto secondIn=MakeEndpoint(second,static_cast<std::uint8_t>(3u+seedOffset),comp::EndpointAccessV1::ReadOnly);
    auto secondOut=MakeEndpoint(second,static_cast<std::uint8_t>(4u+seedOffset),comp::EndpointAccessV1::WriteOnly);
    auto contract=comp::MakeRawCompositionContractV1(
        {first,second},{firstIn,firstOut,secondIn,secondOut},{
        MakeFlow(static_cast<std::uint8_t>(1u+seedOffset),comp::FlowBoundaryV1::CompositionInput,{firstIn.Identity()}),
        MakeFlow(static_cast<std::uint8_t>(2u+seedOffset),comp::FlowBoundaryV1::Internal,{firstOut.Identity(),secondIn.Identity()}),
        MakeFlow(static_cast<std::uint8_t>(3u+seedOffset),comp::FlowBoundaryV1::CompositionOutput,{secondOut.Identity()})});
    auto planned=comp::CompositionPlannerV1::Plan(contract); if(!planned.Planned()) throw std::runtime_error("composition planning failed");
    auto verified=comp::CompositionVerifierV1::Verify(contract,*planned.proposal); if(!verified.Accepted()) throw std::runtime_error("composition verification failed");
    return fc::FrozenCompositionBuilderV1::Freeze(*verified.verified);
}

c::SemanticIdentity MakeSemantic(std::uint8_t seed)
{
    auto payload=SeedBytes(seed,31u);
    return c::MakeCanonicalIdentityV1<c::SemanticIdentity>({"SGE.V2.Dynamic.TestSemantic.V1",1u,payload});
}

dyn::ExactIndexSetV1 Set(dyn::UniverseCount universe,std::initializer_list<std::uint32_t> values)
{
    std::vector<std::uint32_t> indices(values);
    auto result=dyn::BuildExactIndexSetV1(universe,indices);
    if(!result.Accepted()) throw std::runtime_error("exact set build failed");
    return *result.set;
}

fd::OpaqueFrozenDynamicInvocationV1 VerifyAndFreeze(const dyn::DynamicInvocationRequestV1& request)
{
    auto planned=dyn::DynamicInvocationPlannerV1::Plan(request); if(!planned.Planned()) throw std::runtime_error("dynamic planning failed");
    auto verified=dyn::DynamicInvocationVerifierV1::Verify(request,*planned.proposal); if(!verified.Accepted()) throw std::runtime_error("dynamic verification failed");
    return fd::FrozenDynamicInvocationBuilderV1::Freeze(*verified.verified);
}

void ExpectError(const dyn::DynamicInvocationRequestV1& request,const dyn::DynamicPlannerProposalV1& proposal,dyn::DynamicVerificationErrorV1 expected,std::vector<std::uint8_t>& observed)
{
    auto result=dyn::DynamicInvocationVerifierV1::Verify(request,proposal);
    if(result.error!=expected || result.verified.has_value()) throw std::runtime_error("unexpected dynamic rejection code");
    observed.push_back(static_cast<std::uint8_t>(result.error));
}

struct Summary final
{
    std::uint8_t mode;
    std::uint32_t active;
    std::uint32_t update;
    std::uint32_t clear;
    std::uint32_t retain;
    std::uint32_t transition;
    std::uint32_t indirect;
    std::uint64_t historyGeneration;
};

Summary Summarize(const dyn::DynamicInvocationRequestV1& request,const fd::OpaqueFrozenDynamicInvocationV1& frozen)
{
    auto planned=dyn::DynamicInvocationPlannerV1::Plan(request); if(!planned.Planned()) throw std::runtime_error("summary planning failed");
    const auto& decision=planned.proposal->decision;
    if(frozen.IndirectWorkCount().Value()!=decision.transitionSet.Count()) throw std::runtime_error("indirect quantity is not exact transition count");
    if(decision.dynamicWriteSetIdentity!=frozen.WriteSetIdentity()) throw std::runtime_error("frozen write authority mismatch");
    if(decision.updateSet.Count()+decision.retainSet.Count()!=request.activeSet.Count())
        throw std::runtime_error("update/retain partition does not reconstruct current Active set");
    if(decision.transitionSet.Count()!=decision.updateSet.Count()+decision.deactivationSet.Count())
        throw std::runtime_error("update/deactivation partition does not reconstruct Transition set");
    for(const auto active:request.activeSet.Indices())
    {
        const bool updated=decision.updateSet.Contains(active);
        const bool retained=decision.retainSet.Contains(active);
        if(updated==retained) throw std::runtime_error("current Active member is not in exactly one of update/retain");
    }
    for(const auto updated:decision.updateSet.Indices())
    {
        if(decision.deactivationSet.Contains(updated)||decision.retainSet.Contains(updated)||!decision.transitionSet.Contains(updated))
            throw std::runtime_error("update member violates exact set partition");
    }
    for(const auto deactivated:decision.deactivationSet.Indices())
    {
        if(decision.updateSet.Contains(deactivated)||decision.retainSet.Contains(deactivated)||!decision.transitionSet.Contains(deactivated))
            throw std::runtime_error("deactivation member violates exact set partition");
    }
    for(const auto retained:decision.retainSet.Indices())
        if(decision.transitionSet.Contains(retained)) throw std::runtime_error("retained member entered exact write set");
    return {static_cast<std::uint8_t>(request.mode),request.activeSet.Count(),decision.updateSet.Count(),decision.deactivationSet.Count(),decision.retainSet.Count(),decision.transitionSet.Count(),frozen.IndirectWorkCount().Value(),frozen.NextHistory().Descriptor().generation.Value()};
}

void ExpectItemGeneration(const fd::OpaqueFrozenDynamicInvocationV1& frozen,std::uint32_t member,std::uint64_t expected)
{
    const auto generations=frozen.NextHistory().ItemGenerations();
    if(member>=generations.size()||generations[member]!=expected) throw std::runtime_error("per-item generation mismatch");
}

void AppendU8(std::vector<std::byte>& out,std::uint8_t value){out.push_back(static_cast<std::byte>(value));}
void AppendU32(std::vector<std::byte>& out,std::uint32_t value){for(std::uint32_t i=0;i<4u;++i)out.push_back(static_cast<std::byte>((value>>(i*8u))&0xffu));}
void AppendU64(std::vector<std::byte>& out,std::uint64_t value){for(std::uint32_t i=0;i<8u;++i)out.push_back(static_cast<std::byte>((value>>(i*8u))&0xffu));}
void Emit(const std::string& path,const std::vector<std::byte>& bytes){std::ofstream file(path,std::ios::binary);if(!file)throw std::runtime_error("open evidence");file.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));if(!file)throw std::runtime_error("write evidence");}
}

int main(int argc,char** argv)
{
    try
    {
        static_assert(!std::is_default_constructible_v<dyn::VerifiedDynamicInvocationV1>);
        static_assert(!std::is_constructible_v<dyn::VerifiedDynamicInvocationV1,dyn::DynamicPlannerProposalV1>);
        static_assert(!std::is_constructible_v<fd::OpaqueFrozenDynamicInvocationV1,dyn::DynamicPlannerProposalV1>);
        static_assert(!std::is_default_constructible_v<dyn::VerifiedHistoryStateV1>);

        std::string emitPath;
        for(int i=1;i<argc;++i){const std::string arg=argv[i];if(arg=="--emit"&&i+1<argc)emitPath=argv[++i];}
        if(emitPath.empty()) throw std::runtime_error("--emit is required");

        auto composition=MakeFrozenComposition(0u);
        auto secondComposition=MakeFrozenComposition(20u);
        auto semantic=MakeSemantic(1u);
        auto secondSemantic=MakeSemantic(2u);
        auto epoch1=c::DeviceEpoch::TryCreate(1u); auto epoch2=c::DeviceEpoch::TryCreate(2u);
        if(!epoch1||!epoch2) throw std::runtime_error("epoch construction");
        const dyn::UniverseCount universe(12u);
        auto empty=Set(universe,{});
        if(Set(universe,{5u,1u,3u}).Identity()!=Set(universe,{1u,3u,5u}).Identity())
            throw std::runtime_error("exact-set identity depends on declaration order");

        std::vector<Summary> accepted;
        accepted.reserve(8u);

        auto initialRequest=dyn::MakeDynamicInvocationRequestV1(composition,semantic,c::TimelineOrdinal(0u),*epoch1,universe,dyn::InvocationModeV1::InitialSeed,Set(universe,{1u,3u,5u}),empty);
        auto initial=VerifyAndFreeze(initialRequest); accepted.push_back(Summarize(initialRequest,initial));

        auto steadyRequest=dyn::MakeDynamicInvocationRequestV1(composition,semantic,c::TimelineOrdinal(1u),*epoch1,universe,dyn::InvocationModeV1::ContinueHistory,Set(universe,{1u,3u,5u}),empty,initial.NextHistory());
        auto steady=VerifyAndFreeze(steadyRequest); accepted.push_back(Summarize(steadyRequest,steady));

        auto modifiedRequest=dyn::MakeDynamicInvocationRequestV1(composition,semantic,c::TimelineOrdinal(2u),*epoch1,universe,dyn::InvocationModeV1::ContinueHistory,Set(universe,{1u,3u,5u}),Set(universe,{3u}),steady.NextHistory());
        auto modified=VerifyAndFreeze(modifiedRequest); accepted.push_back(Summarize(modifiedRequest,modified));

        auto mixedRequest=dyn::MakeDynamicInvocationRequestV1(composition,semantic,c::TimelineOrdinal(3u),*epoch1,universe,dyn::InvocationModeV1::ContinueHistory,Set(universe,{3u,4u,5u}),Set(universe,{5u}),modified.NextHistory());
        auto mixed=VerifyAndFreeze(mixedRequest); accepted.push_back(Summarize(mixedRequest,mixed));

        auto emptyRequest=dyn::MakeDynamicInvocationRequestV1(composition,semantic,c::TimelineOrdinal(4u),*epoch1,universe,dyn::InvocationModeV1::ContinueHistory,empty,empty,mixed.NextHistory());
        auto emptyResult=VerifyAndFreeze(emptyRequest); accepted.push_back(Summarize(emptyRequest,emptyResult));

        auto reactivateRequest=dyn::MakeDynamicInvocationRequestV1(composition,semantic,c::TimelineOrdinal(5u),*epoch1,universe,dyn::InvocationModeV1::ContinueHistory,Set(universe,{2u,8u}),empty,emptyResult.NextHistory());
        auto reactivate=VerifyAndFreeze(reactivateRequest); accepted.push_back(Summarize(reactivateRequest,reactivate));

        auto recoveryRequest=dyn::MakeDynamicInvocationRequestV1(composition,semantic,c::TimelineOrdinal(0u),*epoch2,universe,dyn::InvocationModeV1::RecoverySeed,Set(universe,{3u,4u,5u}),empty);
        auto recovery=VerifyAndFreeze(recoveryRequest); accepted.push_back(Summarize(recoveryRequest,recovery));

        const dyn::UniverseCount largeUniverse(1000u);
        auto largeRequest=dyn::MakeDynamicInvocationRequestV1(composition,semantic,c::TimelineOrdinal(0u),*epoch1,largeUniverse,dyn::InvocationModeV1::InitialSeed,Set(largeUniverse,{999u}),Set(largeUniverse,{}));
        auto large=VerifyAndFreeze(largeRequest); accepted.push_back(Summarize(largeRequest,large));

        if(initial.DecisionIdentity()==steady.DecisionIdentity()) throw std::runtime_error("different dynamic decisions aliased");
        if(recovery.NextHistory().Descriptor().deviceEpoch!=*epoch2) throw std::runtime_error("Recovery seed history is not epoch-bound");
        if(steady.IndirectWorkCount().Value()!=0u) throw std::runtime_error("steady retain invocation dispatched work");
        ExpectItemGeneration(initial,1u,0u);
        ExpectItemGeneration(initial,2u,dyn::InvalidItemGenerationV1);
        ExpectItemGeneration(steady,1u,0u);
        ExpectItemGeneration(modified,3u,1u);
        ExpectItemGeneration(mixed,3u,1u);
        ExpectItemGeneration(mixed,4u,0u);
        ExpectItemGeneration(mixed,5u,1u);
        ExpectItemGeneration(mixed,1u,dyn::InvalidItemGenerationV1);
        ExpectItemGeneration(emptyResult,3u,dyn::InvalidItemGenerationV1);
        ExpectItemGeneration(reactivate,2u,0u);
        ExpectItemGeneration(recovery,3u,0u);

        std::vector<std::uint8_t> observed;
        observed.reserve(27u);
        {
            auto result=dyn::BuildExactIndexSetV1(dyn::UniverseCount(0u),std::array<std::uint32_t,0>{});
            if(result.error!=dyn::ExactIndexSetErrorV1::ZeroUniverse) throw std::runtime_error("zero universe accepted");
            observed.push_back(static_cast<std::uint8_t>(0x80u|static_cast<std::uint8_t>(result.error)));
        }
        {
            std::array<std::uint32_t,1> values{12u}; auto result=dyn::BuildExactIndexSetV1(universe,values);
            if(result.error!=dyn::ExactIndexSetErrorV1::IndexOutOfRange) throw std::runtime_error("out-of-range member accepted");
            observed.push_back(static_cast<std::uint8_t>(0x80u|static_cast<std::uint8_t>(result.error)));
        }
        {
            std::array<std::uint32_t,2> values{1u,1u}; auto result=dyn::BuildExactIndexSetV1(universe,values);
            if(result.error!=dyn::ExactIndexSetErrorV1::DuplicateIndex) throw std::runtime_error("duplicate member accepted");
            observed.push_back(static_cast<std::uint8_t>(0x80u|static_cast<std::uint8_t>(result.error)));
        }

        auto baselinePlan=dyn::DynamicInvocationPlannerV1::Plan(mixedRequest); if(!baselinePlan.Planned()) throw std::runtime_error("baseline plan");
        auto baseline=*baselinePlan.proposal;

        {auto request=mixedRequest;request.identity=c::InvocationIdentity::FromDigest({});ExpectError(request,baseline,dyn::DynamicVerificationErrorV1::RequestIdentityMismatch,observed);}
        {auto request=dyn::MakeDynamicInvocationRequestV1(composition,semantic,c::TimelineOrdinal(0u),*epoch1,universe,dyn::InvocationModeV1::InitialSeed,Set(universe,{1u}),Set(universe,{1u}));ExpectError(request,baseline,dyn::DynamicVerificationErrorV1::SeedModifiedSetNotEmpty,observed);}
        {auto request=dyn::MakeDynamicInvocationRequestV1(composition,semantic,c::TimelineOrdinal(0u),*epoch1,universe,dyn::InvocationModeV1::InitialSeed,Set(universe,{1u,3u,5u}),empty,initial.NextHistory());ExpectError(request,baseline,dyn::DynamicVerificationErrorV1::InitialHistoryPresent,observed);}
        {auto request=dyn::MakeDynamicInvocationRequestV1(composition,semantic,c::TimelineOrdinal(1u),*epoch1,universe,dyn::InvocationModeV1::ContinueHistory,Set(universe,{1u,3u,5u}),empty);ExpectError(request,baseline,dyn::DynamicVerificationErrorV1::ContinueHistoryMissing,observed);}
        {auto request=dyn::MakeDynamicInvocationRequestV1(composition,semantic,c::TimelineOrdinal(0u),*epoch2,universe,dyn::InvocationModeV1::RecoverySeed,Set(universe,{1u}),empty,initial.NextHistory());ExpectError(request,baseline,dyn::DynamicVerificationErrorV1::RecoveryHistoryPresent,observed);}
        {auto request=dyn::MakeDynamicInvocationRequestV1(secondComposition,semantic,c::TimelineOrdinal(1u),*epoch1,universe,dyn::InvocationModeV1::ContinueHistory,Set(universe,{1u,3u,5u}),empty,initial.NextHistory());ExpectError(request,baseline,dyn::DynamicVerificationErrorV1::HistoryCompositionMismatch,observed);}
        {auto request=dyn::MakeDynamicInvocationRequestV1(composition,secondSemantic,c::TimelineOrdinal(1u),*epoch1,universe,dyn::InvocationModeV1::ContinueHistory,Set(universe,{1u,3u,5u}),empty,initial.NextHistory());ExpectError(request,baseline,dyn::DynamicVerificationErrorV1::HistorySemanticMismatch,observed);}
        {auto request=dyn::MakeDynamicInvocationRequestV1(composition,semantic,c::TimelineOrdinal(1u),*epoch2,universe,dyn::InvocationModeV1::ContinueHistory,Set(universe,{1u,3u,5u}),empty,initial.NextHistory());ExpectError(request,baseline,dyn::DynamicVerificationErrorV1::HistoryEpochMismatch,observed);}
        {const dyn::UniverseCount otherUniverse(13u);auto request=dyn::MakeDynamicInvocationRequestV1(composition,semantic,c::TimelineOrdinal(1u),*epoch1,otherUniverse,dyn::InvocationModeV1::ContinueHistory,Set(otherUniverse,{1u,3u,5u}),Set(otherUniverse,{}),initial.NextHistory());ExpectError(request,baseline,dyn::DynamicVerificationErrorV1::HistoryUniverseMismatch,observed);}
        {auto request=dyn::MakeDynamicInvocationRequestV1(composition,semantic,c::TimelineOrdinal(2u),*epoch1,universe,dyn::InvocationModeV1::ContinueHistory,Set(universe,{1u,3u,5u}),empty,initial.NextHistory());ExpectError(request,baseline,dyn::DynamicVerificationErrorV1::TimelineNotSuccessor,observed);}
        {auto request=dyn::MakeDynamicInvocationRequestV1(composition,semantic,c::TimelineOrdinal(1u),*epoch1,universe,dyn::InvocationModeV1::ContinueHistory,Set(universe,{1u,3u,5u}),Set(universe,{7u}),initial.NextHistory());ExpectError(request,baseline,dyn::DynamicVerificationErrorV1::ModifiedNotActive,observed);}
        {auto request=dyn::MakeDynamicInvocationRequestV1(composition,semantic,c::TimelineOrdinal(1u),*epoch1,universe,dyn::InvocationModeV1::ContinueHistory,Set(universe,{1u,3u,5u,7u}),Set(universe,{7u}),initial.NextHistory());ExpectError(request,baseline,dyn::DynamicVerificationErrorV1::ModifiedNotSurvivor,observed);}

        {auto proposal=baseline;proposal.decision.requestIdentity=c::InvocationIdentity::FromDigest({});ExpectError(mixedRequest,proposal,dyn::DynamicVerificationErrorV1::ProposalRequestIdentityMismatch,observed);}
        {auto proposal=baseline;proposal.decision.previousActiveSet=empty;ExpectError(mixedRequest,proposal,dyn::DynamicVerificationErrorV1::PreviousActiveSetMismatch,observed);}
        {auto proposal=baseline;proposal.decision.activationSet=Set(universe,{2u});ExpectError(mixedRequest,proposal,dyn::DynamicVerificationErrorV1::ActivationSetMismatch,observed);}
        {auto proposal=baseline;proposal.decision.deactivationSet=empty;ExpectError(mixedRequest,proposal,dyn::DynamicVerificationErrorV1::DeactivationSetMismatch,observed);}
        {auto proposal=baseline;proposal.decision.updateSet=empty;ExpectError(mixedRequest,proposal,dyn::DynamicVerificationErrorV1::UpdateSetMismatch,observed);}
        {auto proposal=baseline;proposal.decision.retainSet=empty;ExpectError(mixedRequest,proposal,dyn::DynamicVerificationErrorV1::RetainSetMismatch,observed);}
        {auto proposal=baseline;proposal.decision.transitionSet=empty;ExpectError(mixedRequest,proposal,dyn::DynamicVerificationErrorV1::TransitionSetMismatch,observed);}
        {auto proposal=baseline;proposal.decision.itemGenerations[3u]+=1u;ExpectError(mixedRequest,proposal,dyn::DynamicVerificationErrorV1::GenerationVectorMismatch,observed);}
        {auto proposal=baseline;proposal.decision.transitionRecords.front().action=(proposal.decision.transitionRecords.front().action==dyn::TransitionActionV1::Update?dyn::TransitionActionV1::Clear:dyn::TransitionActionV1::Update);ExpectError(mixedRequest,proposal,dyn::DynamicVerificationErrorV1::TransitionRecordMismatch,observed);}
        {auto proposal=baseline;proposal.decision.indirectWorkCount=c::TransitionCount(proposal.decision.indirectWorkCount.Value()+1u);ExpectError(mixedRequest,proposal,dyn::DynamicVerificationErrorV1::IndirectWorkCountMismatch,observed);}
        {auto proposal=baseline;proposal.decision.dynamicWriteSetIdentity=dyn::DynamicWriteSetIdentity::FromDigest({});ExpectError(mixedRequest,proposal,dyn::DynamicVerificationErrorV1::DynamicWriteSetMismatch,observed);}
        {auto proposal=baseline;proposal.decision.identity=dyn::DynamicDecisionIdentity::FromDigest({});ExpectError(mixedRequest,proposal,dyn::DynamicVerificationErrorV1::DecisionIdentityMismatch,observed);}

        if(accepted.size()!=8u || observed.size()!=27u) throw std::runtime_error("evidence case count mismatch");

        std::vector<std::byte> evidence;
        for(const char ch:std::string("L4V2R4V1")) evidence.push_back(static_cast<std::byte>(static_cast<unsigned char>(ch)));
        AppendU32(evidence,static_cast<std::uint32_t>(accepted.size()));
        AppendU32(evidence,static_cast<std::uint32_t>(observed.size()));
        for(const auto& summary:accepted)
        {
            AppendU8(evidence,summary.mode);AppendU32(evidence,summary.active);AppendU32(evidence,summary.update);
            AppendU32(evidence,summary.clear);AppendU32(evidence,summary.retain);AppendU32(evidence,summary.transition);
            AppendU32(evidence,summary.indirect);AppendU64(evidence,summary.historyGeneration);
        }
        for(const auto code:observed) AppendU8(evidence,code);
        Emit(emitPath,evidence);

        std::cout<<"Level 4 v2 R4 exact dynamic invocation, history, sparse membership and delta authority self-test passed.\n";
        std::cout<<"Accepted scenarios: 8. Rejections: 27.\n";
        std::cout<<"Retained members excluded from exact write authority; indirect quantity equals transition count.\n";
        return 0;
    }
    catch(const std::exception& error)
    {
        std::cerr<<"Level 4 v2 R4 failure: "<<error.what()<<'\n';
        return 1;
    }
}
