#include "CompositionPlanner.h"

#include <algorithm>
#include <map>
#include <set>

namespace sge4_5::v2::composition
{
namespace
{
template<class Identity>
bool LessIdentity(const Identity& a,const Identity& b){return a.Digest()<b.Digest();}

struct ContractView final
{
    std::map<canonical::Digest256V1,const LeafAuthorityV1*> leaves;
    std::map<canonical::Digest256V1,const EndpointAuthorityV1*> endpoints;
    std::vector<const FlowDeclarationV1*> flows;
    std::optional<PresenterBindingV1> presenter;
};

bool BuildPlannerContractView(const RawCompositionContractV1& contract,ContractView& out)
{
    if(contract.recoveryScope!=RecoveryScopeV1::WholeComposition || contract.leaves.empty()) return false;
    if(contract.identity!=ComputeCompositionContractIdentityV1(contract)) return false;
    std::optional<canonical::TargetProfileIdentity> target;
    for(const auto& leaf:contract.leaves)
    {
        if(leaf.Identity()!=ComputeLeafIdentityV1(leaf) || !out.leaves.emplace(leaf.Identity().Digest(),&leaf).second) return false;
        if(!target) target=leaf.TargetProfileIdentityValue(); else if(*target!=leaf.TargetProfileIdentityValue()) return false;
    }
    for(const auto& endpoint:contract.endpoints)
    {
        if(endpoint.Identity()!=ComputeEndpointIdentityV1(endpoint) || endpoint.ResourceKind()!=ResourceKindV1::Buffer || endpoint.MinimumBytes()==0u) return false;
        auto leaf=out.leaves.find(endpoint.LeafIdentityValue().Digest()); if(leaf==out.leaves.end()) return false;
        if(endpoint.ResourceContractIdentityValue()!=leaf->second->ResourceContractIdentityValue()) return false;
        if(!out.endpoints.emplace(endpoint.Identity().Digest(),&endpoint).second) return false;
    }
    std::set<canonical::Digest256V1> flowIds;
    std::map<canonical::Digest256V1,std::uint32_t> bindingCounts;
    std::uint32_t presenterCount=0;
    for(const auto& flow:contract.flows)
    {
        if(flow.identity!=ComputeFlowIdentityV1(flow) || !flowIds.insert(flow.identity.Digest()).second || flow.endpoints.empty()) return false;
        std::uint32_t writers=0,readers=0; const EndpointAuthorityV1* writer=nullptr;
        std::set<canonical::Digest256V1> local;
        for(const auto& endpointId:flow.endpoints)
        {
            auto found=out.endpoints.find(endpointId.Digest()); if(found==out.endpoints.end() || !local.insert(endpointId.Digest()).second) return false;
            ++bindingCounts[endpointId.Digest()];
            if(found->second->Access()==EndpointAccessV1::WriteOnly){++writers;writer=found->second;} else ++readers;
            if(found->second->Presenter())
            {
                ++presenterCount;
                out.presenter=PresenterBindingV1{found->second->LeafIdentityValue(),found->second->Identity()};
                if(flow.boundary!=FlowBoundaryV1::CompositionOutput || found->second->Access()!=EndpointAccessV1::WriteOnly) return false;
            }
        }
        if(writers>1u) return false;
        if(flow.boundary==FlowBoundaryV1::Internal && (writers!=1u || readers==0u)) return false;
        if(flow.boundary==FlowBoundaryV1::CompositionInput && (writers!=0u || readers==0u)) return false;
        if(flow.boundary==FlowBoundaryV1::CompositionOutput && (writers!=1u || readers!=0u)) return false;
        (void)writer;
        out.flows.push_back(&flow);
    }
    if(presenterCount>1u) return false;
    for(const auto& [id,endpoint]:out.endpoints)
    {
        const auto count=bindingCounts[id];
        if(count>1u || (endpoint->Required() && count!=1u)) return false;
    }
    std::sort(out.flows.begin(),out.flows.end(),[](const auto* a,const auto* b){return LessIdentity(a->identity,b->identity);});
    return true;
}

std::optional<std::vector<CompositionScheduleEntryV1>> BuildPlannerSchedule(const ContractView& view)
{
    std::map<canonical::Digest256V1,std::set<canonical::Digest256V1>> outgoing;
    std::map<canonical::Digest256V1,std::uint32_t> indegree;
    std::map<canonical::Digest256V1,LeafIdentity> identities;
    for(const auto& [d,leaf]:view.leaves){indegree[d]=0u;identities.emplace(d,leaf->Identity());}
    for(const auto* flow:view.flows)
    {
        if(flow->boundary!=FlowBoundaryV1::Internal) continue;
        const EndpointAuthorityV1* producer=nullptr;
        std::vector<const EndpointAuthorityV1*> readers;
        for(const auto& id:flow->endpoints)
        {
            auto e=view.endpoints.at(id.Digest());
            if(e->Access()==EndpointAccessV1::WriteOnly) producer=e; else readers.push_back(e);
        }
        if(!producer) return std::nullopt;
        for(const auto* reader:readers)
        {
            const auto p=producer->LeafIdentityValue().Digest(); const auto c=reader->LeafIdentityValue().Digest();
            if(p==c) return std::nullopt;
            if(outgoing[p].insert(c).second) ++indegree[c];
        }
    }
    std::set<canonical::Digest256V1> ready;
    for(const auto& [id,count]:indegree) if(count==0u) ready.insert(id);
    std::vector<CompositionScheduleEntryV1> result;
    while(!ready.empty())
    {
        auto current=*ready.begin(); ready.erase(ready.begin());
        result.push_back({ScheduleOrdinal(static_cast<std::uint32_t>(result.size())),identities.at(current)});
        for(const auto& next:outgoing[current]) if(--indegree[next]==0u) ready.insert(next);
    }
    if(result.size()!=view.leaves.size()) return std::nullopt;
    return result;
}

CompositionPlanProposalV1 BuildPlannerProposal(const RawCompositionContractV1& contract,const ContractView& view,const std::vector<CompositionScheduleEntryV1>& schedule)
{
    CompositionPlanProposalV1 p{
        CompositionPlanIdentity::FromDigest({}),contract.identity,{},ScheduleIdentity::FromDigest({}),schedule,
        BindingSetIdentity::FromDigest({}),{},HandoffSetIdentity::FromDigest({}),{},SynchronizationSetIdentity::FromDigest({}),{},view.presenter,
        {RecoverySetIdentity::FromDigest({}),RecoveryScopeV1::WholeComposition,{},{}}};
    std::uint32_t syncOrdinal=0;
    for(const auto* flow:view.flows)
    {
        std::uint64_t size=0; const EndpointAuthorityV1* producer=nullptr; std::vector<const EndpointAuthorityV1*> readers;
        std::vector<const EndpointAuthorityV1*> flowEndpoints;
        for(const auto& id:flow->endpoints) flowEndpoints.push_back(view.endpoints.at(id.Digest()));
        std::sort(flowEndpoints.begin(),flowEndpoints.end(),[](const auto* a,const auto* b){return LessIdentity(a->Identity(),b->Identity());});
        for(const auto* e:flowEndpoints)
        {
            size=std::max(size,e->MinimumBytes()); p.bindings.push_back({e->Identity(),flow->identity});
            if(e->Access()==EndpointAccessV1::WriteOnly) producer=e; else readers.push_back(e);
        }
        const auto ownership=flow->boundary==FlowBoundaryV1::CompositionInput?AllocationOwnershipV1::ExternalInput:AllocationOwnershipV1::CompositionOwned;
        p.allocations.push_back({ComputeAllocationIdentityV1(flow->identity,size,ownership),flow->identity,size,ownership});
        if(flow->boundary==FlowBoundaryV1::Internal && producer)
        {
            std::sort(readers.begin(),readers.end(),[](const auto* a,const auto* b){return LessIdentity(a->Identity(),b->Identity());});
            for(const auto* reader:readers)
            {
                p.handoffs.push_back({flow->identity,producer->Identity(),reader->Identity(),producer->OutgoingState(),reader->IncomingState()});
                if(producer->RequiresSynchronization() || reader->RequiresSynchronization())
                {
                    p.synchronizations.push_back({flow->identity,producer->LeafIdentityValue(),reader->LeafIdentityValue(),SignalOrdinal(syncOrdinal),WaitOrdinal(syncOrdinal)});
                    ++syncOrdinal;
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
}

CompositionPlanningResultV1 CompositionPlannerV1::Plan(const RawCompositionContractV1& contract)
{
    ContractView view;
    if(!BuildPlannerContractView(contract,view)) return {CompositionPlanningErrorV1::InvalidContract,std::nullopt};
    auto schedule=BuildPlannerSchedule(view);
    if(!schedule) return {CompositionPlanningErrorV1::GraphCycle,std::nullopt};
    return {CompositionPlanningErrorV1::None,BuildPlannerProposal(contract,view,*schedule)};
}
}
