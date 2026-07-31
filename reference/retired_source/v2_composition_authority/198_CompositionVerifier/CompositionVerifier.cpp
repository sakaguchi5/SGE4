#include "CompositionVerifier.h"

#include <algorithm>
#include <map>
#include <set>

namespace sge4_5::v2::composition
{
namespace
{
template<class Identity>
bool LessIdentity(const Identity& a,const Identity& b){return a.Digest()<b.Digest();}

struct VerifiedView final
{
    std::map<canonical::Digest256V1,const LeafAuthorityV1*> leaves;
    std::map<canonical::Digest256V1,const EndpointAuthorityV1*> endpoints;
    std::vector<const FlowDeclarationV1*> flows;
    std::optional<PresenterBindingV1> presenter;
};

CompositionVerificationErrorV1 ValidateContractIndependently(const RawCompositionContractV1& contract,VerifiedView& out)
{
    if(contract.recoveryScope!=RecoveryScopeV1::WholeComposition || contract.leaves.empty()) return CompositionVerificationErrorV1::ContractIdentityMismatch;
    for(const auto& leaf:contract.leaves)
    {
        if(leaf.Identity()!=ComputeLeafIdentityV1(leaf)) return CompositionVerificationErrorV1::LeafIdentityMismatch;
        if(!out.leaves.emplace(leaf.Identity().Digest(),&leaf).second) return CompositionVerificationErrorV1::DuplicateLeaf;
    }
    std::optional<canonical::TargetProfileIdentity> target;
    for(const auto& [id,leaf]:out.leaves)
    {
        if(!target) target=leaf->TargetProfileIdentityValue(); else if(*target!=leaf->TargetProfileIdentityValue()) return CompositionVerificationErrorV1::TargetProfileMismatch;
    }
    for(const auto& endpoint:contract.endpoints)
    {
        if(endpoint.Identity()!=ComputeEndpointIdentityV1(endpoint)) return CompositionVerificationErrorV1::EndpointIdentityMismatch;
        if(endpoint.ResourceKind()!=ResourceKindV1::Buffer || endpoint.MinimumBytes()==0u) return CompositionVerificationErrorV1::UnsupportedResourceKind;
        auto leaf=out.leaves.find(endpoint.LeafIdentityValue().Digest());
        if(leaf==out.leaves.end()) return CompositionVerificationErrorV1::EndpointReferencesUnknownLeaf;
        if(endpoint.ResourceContractIdentityValue()!=leaf->second->ResourceContractIdentityValue()) return CompositionVerificationErrorV1::EndpointResourceContractMismatch;
        if(!out.endpoints.emplace(endpoint.Identity().Digest(),&endpoint).second) return CompositionVerificationErrorV1::DuplicateEndpoint;
    }
    std::set<canonical::Digest256V1> flowIds;
    std::map<canonical::Digest256V1,std::uint32_t> bound;
    std::uint32_t presenterCount=0u;
    for(const auto& flow:contract.flows)
    {
        if(flow.identity!=ComputeFlowIdentityV1(flow)) return CompositionVerificationErrorV1::FlowIdentityMismatch;
        if(!flowIds.insert(flow.identity.Digest()).second) return CompositionVerificationErrorV1::DuplicateFlow;
        if(flow.endpoints.empty()) return CompositionVerificationErrorV1::InvalidFlowShape;
        std::set<canonical::Digest256V1> local;
        std::uint32_t writers=0u,readers=0u;
        for(const auto& endpointId:flow.endpoints)
        {
            auto found=out.endpoints.find(endpointId.Digest());
            if(found==out.endpoints.end()) return CompositionVerificationErrorV1::FlowReferencesUnknownEndpoint;
            if(!local.insert(endpointId.Digest()).second) return CompositionVerificationErrorV1::EndpointBoundMoreThanOnce;
            const auto count=++bound[endpointId.Digest()];
            if(count>1u) return CompositionVerificationErrorV1::EndpointBoundMoreThanOnce;
            if(found->second->Access()==EndpointAccessV1::WriteOnly) ++writers; else ++readers;
            if(found->second->Presenter())
            {
                ++presenterCount;
                out.presenter=PresenterBindingV1{found->second->LeafIdentityValue(),found->second->Identity()};
                if(flow.boundary!=FlowBoundaryV1::CompositionOutput || found->second->Access()!=EndpointAccessV1::WriteOnly) return CompositionVerificationErrorV1::InvalidPresenter;
            }
        }
        if(writers>1u) return CompositionVerificationErrorV1::MultipleWriters;
        if(flow.boundary==FlowBoundaryV1::Internal && (writers!=1u || readers==0u)) return CompositionVerificationErrorV1::InvalidFlowShape;
        if(flow.boundary==FlowBoundaryV1::CompositionInput && (writers!=0u || readers==0u)) return CompositionVerificationErrorV1::InvalidFlowShape;
        if(flow.boundary==FlowBoundaryV1::CompositionOutput && (writers!=1u || readers!=0u)) return CompositionVerificationErrorV1::InvalidFlowShape;
        out.flows.push_back(&flow);
    }
    if(presenterCount>1u) return CompositionVerificationErrorV1::MultiplePresenters;
    for(const auto& [id,endpoint]:out.endpoints)
        if(endpoint->Required() && bound[id]!=1u) return CompositionVerificationErrorV1::RequiredEndpointUnbound;
    if(contract.identity!=ComputeCompositionContractIdentityV1(contract)) return CompositionVerificationErrorV1::ContractIdentityMismatch;
    std::sort(out.flows.begin(),out.flows.end(),[](const auto* a,const auto* b){return LessIdentity(a->identity,b->identity);});
    return CompositionVerificationErrorV1::None;
}

std::optional<std::vector<CompositionScheduleEntryV1>> BuildExpectedScheduleV1(const VerifiedView& view)
{
    std::map<canonical::Digest256V1,std::set<canonical::Digest256V1>> edges;
    std::map<canonical::Digest256V1,std::uint32_t> indegree;
    std::map<canonical::Digest256V1,LeafIdentity> ids;
    for(const auto& [d,leaf]:view.leaves){indegree[d]=0u;ids.emplace(d,leaf->Identity());}
    for(const auto* flow:view.flows)
    {
        if(flow->boundary!=FlowBoundaryV1::Internal) continue;
        const EndpointAuthorityV1* writer=nullptr;
        std::vector<const EndpointAuthorityV1*> readers;
        for(const auto& endpointId:flow->endpoints)
        {
            auto endpoint=view.endpoints.at(endpointId.Digest());
            if(endpoint->Access()==EndpointAccessV1::WriteOnly) writer=endpoint; else readers.push_back(endpoint);
        }
        if(!writer) return std::nullopt;
        for(const auto* reader:readers)
        {
            const auto a=writer->LeafIdentityValue().Digest(); const auto b=reader->LeafIdentityValue().Digest();
            if(a==b) return std::nullopt;
            if(edges[a].insert(b).second) ++indegree[b];
        }
    }
    std::set<canonical::Digest256V1> ready;
    for(const auto& [id,count]:indegree) if(count==0u) ready.insert(id);
    std::vector<CompositionScheduleEntryV1> schedule;
    while(!ready.empty())
    {
        const auto current=*ready.begin(); ready.erase(ready.begin());
        schedule.push_back({ScheduleOrdinal(static_cast<std::uint32_t>(schedule.size())),ids.at(current)});
        for(const auto& next:edges[current]) if(--indegree[next]==0u) ready.insert(next);
    }
    if(schedule.size()!=view.leaves.size()) return std::nullopt;
    return schedule;
}

CompositionPlanProposalV1 BuildExpectedCompositionPlanV1(const RawCompositionContractV1& contract,const VerifiedView& view,const std::vector<CompositionScheduleEntryV1>& schedule)
{
    CompositionPlanProposalV1 p{CompositionPlanIdentity::FromDigest({}),contract.identity,{},ScheduleIdentity::FromDigest({}),schedule,BindingSetIdentity::FromDigest({}),{},HandoffSetIdentity::FromDigest({}),{},SynchronizationSetIdentity::FromDigest({}),{},view.presenter,{RecoverySetIdentity::FromDigest({}),RecoveryScopeV1::WholeComposition,{},{}}};
    std::uint32_t ordinal=0u;
    for(const auto* flow:view.flows)
    {
        std::vector<const EndpointAuthorityV1*> endpoints;
        for(const auto& id:flow->endpoints) endpoints.push_back(view.endpoints.at(id.Digest()));
        std::sort(endpoints.begin(),endpoints.end(),[](const auto* a,const auto* b){return LessIdentity(a->Identity(),b->Identity());});
        std::uint64_t bytes=0u; const EndpointAuthorityV1* writer=nullptr; std::vector<const EndpointAuthorityV1*> readers;
        for(const auto* endpoint:endpoints)
        {
            bytes=std::max(bytes,endpoint->MinimumBytes());
            p.bindings.push_back({endpoint->Identity(),flow->identity});
            if(endpoint->Access()==EndpointAccessV1::WriteOnly) writer=endpoint; else readers.push_back(endpoint);
        }
        const auto ownership=flow->boundary==FlowBoundaryV1::CompositionInput?AllocationOwnershipV1::ExternalInput:AllocationOwnershipV1::CompositionOwned;
        p.allocations.push_back({ComputeAllocationIdentityV1(flow->identity,bytes,ownership),flow->identity,bytes,ownership});
        if(flow->boundary==FlowBoundaryV1::Internal && writer)
        {
            std::sort(readers.begin(),readers.end(),[](const auto* a,const auto* b){return LessIdentity(a->Identity(),b->Identity());});
            for(const auto* reader:readers)
            {
                p.handoffs.push_back({flow->identity,writer->Identity(),reader->Identity(),writer->OutgoingState(),reader->IncomingState()});
                if(writer->RequiresSynchronization() || reader->RequiresSynchronization())
                {
                    p.synchronizations.push_back({flow->identity,writer->LeafIdentityValue(),reader->LeafIdentityValue(),SignalOrdinal(ordinal),WaitOrdinal(ordinal)});
                    ++ordinal;
                }
            }
        }
    }
    std::sort(p.allocations.begin(),p.allocations.end(),[](const auto& a,const auto& b){return LessIdentity(a.flowIdentity,b.flowIdentity);});
    std::sort(p.bindings.begin(),p.bindings.end(),[](const auto& a,const auto& b){return LessIdentity(a.endpointIdentity,b.endpointIdentity);});
    std::sort(p.handoffs.begin(),p.handoffs.end(),[](const auto& a,const auto& b){if(a.flowIdentity!=b.flowIdentity)return LessIdentity(a.flowIdentity,b.flowIdentity);return LessIdentity(a.consumerEndpointIdentity,b.consumerEndpointIdentity);});
    std::sort(p.synchronizations.begin(),p.synchronizations.end(),[](const auto& a,const auto& b){return a.signalOrdinal.Value()<b.signalOrdinal.Value();});
    p.scheduleIdentity=ComputeScheduleIdentityV1(p.schedule);
    p.bindingSetIdentity=ComputeBindingSetIdentityV1(p.bindings);
    p.handoffSetIdentity=ComputeHandoffSetIdentityV1(p.handoffs);
    p.synchronizationSetIdentity=ComputeSynchronizationSetIdentityV1(p.synchronizations);
    for(const auto& [id,leaf]:view.leaves) p.recoverySet.leaves.push_back(leaf->Identity());
    for(const auto* flow:view.flows) p.recoverySet.flows.push_back(flow->identity);
    std::sort(p.recoverySet.leaves.begin(),p.recoverySet.leaves.end(),LessIdentity<LeafIdentity>);
    std::sort(p.recoverySet.flows.begin(),p.recoverySet.flows.end(),LessIdentity<FlowIdentity>);
    p.recoverySet.identity=ComputeRecoverySetIdentityV1(p.recoverySet);
    p.identity=ComputeCompositionPlanIdentityV1(p);
    return p;
}

bool AllocationsEqual(const auto& a,const auto& b){return a==b;}
bool ScheduleEqual(const auto& a,const auto& b){return a==b;}
bool BindingsEqual(const auto& a,const auto& b){return a==b;}
bool HandoffsEqual(const auto& a,const auto& b){return a==b;}
bool SynchronizationsEqual(const auto& a,const auto& b){return a==b;}
bool PresenterEqual(const auto& a,const auto& b){return a==b;}
bool RecoveryEqual(const RecoverySetV1& a,const RecoverySetV1& b){return a.identity==b.identity&&a.scope==b.scope&&a.leaves==b.leaves&&a.flows==b.flows;}
}

CompositionVerificationResultV1 CompositionVerifierV1::Verify(const RawCompositionContractV1& contract,const CompositionPlanProposalV1& proposal)
{
    VerifiedView view;
    const auto contractError=ValidateContractIndependently(contract,view);
    if(contractError!=CompositionVerificationErrorV1::None) return {contractError,std::nullopt};
    auto schedule=BuildExpectedScheduleV1(view);
    if(!schedule) return {CompositionVerificationErrorV1::GraphCycle,std::nullopt};
    const auto expected=BuildExpectedCompositionPlanV1(contract,view,*schedule);
    if(proposal.contractIdentity!=contract.identity) return {CompositionVerificationErrorV1::ContractIdentityMismatch,std::nullopt};
    if(!AllocationsEqual(proposal.allocations,expected.allocations)) return {CompositionVerificationErrorV1::AllocationMismatch,std::nullopt};
    if(proposal.scheduleIdentity!=expected.scheduleIdentity || !ScheduleEqual(proposal.schedule,expected.schedule)) return {CompositionVerificationErrorV1::ScheduleMismatch,std::nullopt};
    if(proposal.bindingSetIdentity!=expected.bindingSetIdentity || !BindingsEqual(proposal.bindings,expected.bindings)) return {CompositionVerificationErrorV1::BindingMismatch,std::nullopt};
    if(proposal.handoffSetIdentity!=expected.handoffSetIdentity || !HandoffsEqual(proposal.handoffs,expected.handoffs)) return {CompositionVerificationErrorV1::HandoffMismatch,std::nullopt};
    if(proposal.synchronizationSetIdentity!=expected.synchronizationSetIdentity || !SynchronizationsEqual(proposal.synchronizations,expected.synchronizations)) return {CompositionVerificationErrorV1::SynchronizationMismatch,std::nullopt};
    if(!PresenterEqual(proposal.presenter,expected.presenter)) return {CompositionVerificationErrorV1::PresenterMismatch,std::nullopt};
    if(!RecoveryEqual(proposal.recoverySet,expected.recoverySet)) return {CompositionVerificationErrorV1::RecoveryMismatch,std::nullopt};
    if(proposal.identity!=expected.identity || proposal.identity!=ComputeCompositionPlanIdentityV1(proposal)) return {CompositionVerificationErrorV1::PlanIdentityMismatch,std::nullopt};
    const auto seal=ComputeCompositionSealIdentityV1(contract.identity,proposal.identity,proposal.scheduleIdentity,proposal.recoverySet.identity);
    return {CompositionVerificationErrorV1::None,VerifiedCompositionV1(contract.identity,proposal.identity,seal,proposal.scheduleIdentity,proposal.recoverySet.identity,static_cast<std::uint32_t>(contract.leaves.size()),static_cast<std::uint32_t>(contract.flows.size()))};
}
}
