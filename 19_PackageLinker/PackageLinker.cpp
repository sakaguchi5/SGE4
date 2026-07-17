#include "PackageLinker.h"

#include <queue>
#include <set>

namespace sge4::composition
{
base::Result<LinkPlanIR, LinkerError> ProposeLinkPlan(const PackageCompositionContract& contract)
{
    LinkPlanIR plan;
    plan.contractIdentity = contract.identity;
    for (const auto& resource : contract.resources)
        plan.resources.push_back({resource.id, resource.ownership, resource.sizeBytes});
    for (const auto& binding : contract.bindings)
        plan.bindings.push_back({binding.endpoint, binding.resource});
    for (const auto& link : contract.links)
        plan.signalWaits.push_back({link.id, link.producer, link.consumer, link.handoffState});

    std::vector<std::set<std::uint32_t>> outgoing(contract.leaves.size());
    std::vector<std::uint32_t> indegree(contract.leaves.size(), 0);
    for (const auto& link : contract.links)
    {
        if (link.producer.value >= contract.endpoints.size() || link.consumer.value >= contract.endpoints.size())
            return base::Result<LinkPlanIR, LinkerError>::Failure({"link/graph", "contract link endpoint is out of range"});
        const auto producer = contract.endpoints[link.producer.value].leaf.value;
        const auto consumer = contract.endpoints[link.consumer.value].leaf.value;
        if (producer >= contract.leaves.size() || consumer >= contract.leaves.size())
            return base::Result<LinkPlanIR, LinkerError>::Failure({"link/graph", "contract link Leaf is out of range"});
        if (outgoing[producer].insert(consumer).second) ++indegree[consumer];
    }
    std::priority_queue<std::uint32_t, std::vector<std::uint32_t>, std::greater<>> ready;
    for (std::uint32_t index = 0; index < indegree.size(); ++index) if (indegree[index] == 0) ready.push(index);
    while (!ready.empty())
    {
        const auto leaf = ready.top(); ready.pop();
        plan.schedule.push_back({static_cast<std::uint32_t>(plan.schedule.size()), {leaf}});
        for (const auto consumer : outgoing[leaf]) if (--indegree[consumer] == 0) ready.push(consumer);
    }
    if (plan.schedule.size() != contract.leaves.size())
        return base::Result<LinkPlanIR, LinkerError>::Failure({"link/graph", "contract Package graph is cyclic"});
    plan.identity = ComputeLinkPlanIdentity(contract, plan);
    return base::Result<LinkPlanIR, LinkerError>::Success(std::move(plan));
}
}
