#include "./CompositionPlanner.h"

#include <algorithm>
#include <map>
#include <set>
#include <tuple>
#include <utility>

namespace sge4::composition::planning
{
namespace
{
template<class T>
base::Expected<T, PlanError> Failure(std::string stage, std::string message)
{
    return base::Failure<T, PlanError>({std::move(stage), std::move(message)});
}

AllocationOwnership OwnershipFor(ResourceBoundary boundary)
{
    switch (boundary)
    {
    case ResourceBoundary::Internal: return AllocationOwnership::CompositionOwned;
    case ResourceBoundary::CompositionInput: return AllocationOwnership::ExternalInput;
    case ResourceBoundary::CompositionOutput: return AllocationOwnership::ExternalOutput;
    default: return static_cast<AllocationOwnership>(0);
    }
}

base::Expected<std::vector<std::uint32_t>, PlanError>
DeriveSchedule(const PackageCompositionContract& contract)
{
    const auto leafCount = contract.leaves.size();
    std::vector<std::set<std::uint32_t>> outgoing(leafCount);
    std::vector<std::uint32_t> indegree(leafCount, 0);

    for (const auto& resource : contract.resources)
    {
        if (resource.boundary != ResourceBoundary::Internal ||
            resource.lifetime == ResourceFlowLifetime::TemporalHistory) continue;
        if (!resource.producer.IsValid() || resource.producer.value >= contract.endpoints.size())
            return Failure<std::vector<std::uint32_t>>(
                "plan/dependency", "検証または実行の契約に違反しています。");
        const auto producerLeaf = contract.endpoints[resource.producer.value].leaf.value;
        for (const auto consumer : resource.consumers)
        {
            if (consumer.value >= contract.endpoints.size())
                return Failure<std::vector<std::uint32_t>>(
                    "plan/dependency", "Resource Flowが検証または実行の契約に違反しています。");
            const auto consumerLeaf = contract.endpoints[consumer.value].leaf.value;
            if (producerLeaf == consumerLeaf)
                return Failure<std::vector<std::uint32_t>>(
                    "plan/dependency", "Resource Flowが検証または実行の契約に違反しています。");
            if (outgoing[producerLeaf].insert(consumerLeaf).second)
                ++indegree[consumerLeaf];
        }
    }

    std::set<std::uint32_t> ready;
    for (std::uint32_t leaf = 0; leaf < leafCount; ++leaf)
        if (indegree[leaf] == 0) ready.insert(leaf);

    std::vector<std::uint32_t> schedule;
    schedule.reserve(leafCount);
    while (!ready.empty())
    {
        const auto leaf = *ready.begin();
        ready.erase(ready.begin());
        schedule.push_back(leaf);
        for (const auto consumer : outgoing[leaf])
            if (--indegree[consumer] == 0) ready.insert(consumer);
    }
    if (schedule.size() != leafCount)
        return Failure<std::vector<std::uint32_t>>(
            "plan/cycle", "Compositionが検証または実行の契約に違反しています。");
    return base::Success<std::vector<std::uint32_t>, PlanError>(std::move(schedule));
}
}

base::Expected<RawCompositionPlan, PlanError>
ProposeCompositionPlan(const ValidatedCompositionContract& validatedContract)
{
    const auto& contract = validatedContract.Contract();
    auto contractValidation = ValidateCompositionContractShape(contract);
    if (!contractValidation)
        return Failure<RawCompositionPlan>(
            "plan/contract", contractValidation.error().stage + "：" +
            contractValidation.error().message);

    auto scheduleResult = DeriveSchedule(contract);
    if (!scheduleResult)
        return base::Failure<RawCompositionPlan, PlanError>(scheduleResult.error());

    RawCompositionPlan plan;
    plan.contractIdentity = contract.identity;
    for (const auto& resource : contract.resources)
    {
        const auto storageBytes = resource.kind == package::d3d12_v13::ResourceKind::Texture2D
            ? static_cast<std::uint64_t>(resource.texture2D.rowBytes) * resource.texture2D.height
            : resource.sizeBytes;
        ResourceAllocationPlan allocation;
        allocation.resource = resource.id;
        allocation.ownership = OwnershipFor(resource.boundary);
        allocation.kind = resource.kind;
        allocation.format = resource.format;
        allocation.sizeBytes = storageBytes;
        allocation.texture2D = resource.texture2D;
        allocation.lifetime = resource.lifetime;
        allocation.historyDepth = resource.historyDepth;
        allocation.physicalInstanceCount =
            resource.lifetime == ResourceFlowLifetime::TemporalHistory ? 2u : 1u;
        plan.allocations.push_back(allocation);
    }

    std::vector<std::uint32_t> ordinalByLeaf(contract.leaves.size(), package::InvalidIndex);
    for (std::uint32_t ordinal = 0; ordinal < scheduleResult.value().size(); ++ordinal)
    {
        const auto leaf = scheduleResult.value()[ordinal];
        ordinalByLeaf[leaf] = ordinal;
        plan.schedule.push_back({ordinal, {leaf}});
    }

    for (const auto& binding : contract.bindings)
    {
        const auto& endpoint = contract.endpoints[binding.endpoint.value];
        plan.bindings.push_back({binding.endpoint, binding.resource, endpoint.leaf,
            endpoint.localExternalSlot, endpoint.access});
    }

    for (const auto& resource : contract.resources)
    {
        if (resource.boundary != ResourceBoundary::Internal) continue;
        const auto& producer = contract.endpoints[resource.producer.value];
        if (resource.lifetime == ResourceFlowLifetime::TemporalHistory)
        {
            TemporalBufferPlan temporal;
            temporal.resource = resource.id;
            temporal.currentProducer = producer.id;
            temporal.currentProducerLeaf = producer.leaf;
            temporal.historyDepth = resource.historyDepth;
            temporal.physicalInstanceCount = 2;
            temporal.previousConsumers = resource.consumers;
            plan.temporalBuffers.push_back(std::move(temporal));
            continue;
        }
        for (const auto consumerId : resource.consumers)
        {
            const auto& consumer = contract.endpoints[consumerId.value];
            plan.handoffs.push_back({resource.id, producer.id, consumer.id,
                producer.leaf, consumer.leaf, producer.guaranteedOutgoingState,
                consumer.requiredIncomingState});
        }
    }
    std::sort(plan.handoffs.begin(), plan.handoffs.end(), [](const auto& left, const auto& right) {
        return std::tie(left.resource.value, left.consumer.value) <
               std::tie(right.resource.value, right.consumer.value);
    });

    std::map<std::uint32_t, std::uint32_t> signalByResource;
    for (const auto& resource : contract.resources)
    {
        if (resource.boundary != ResourceBoundary::Internal ||
            resource.lifetime == ResourceFlowLifetime::TemporalHistory) continue;
        const auto& producer = contract.endpoints[resource.producer.value];
        const auto signalId = static_cast<std::uint32_t>(plan.signals.size());
        signalByResource.emplace(resource.id.value, signalId);
        plan.signals.push_back({signalId, resource.id, producer.id, producer.leaf,
            ordinalByLeaf[producer.leaf.value]});
    }

    for (const auto& handoff : plan.handoffs)
        plan.waits.push_back({static_cast<std::uint32_t>(plan.waits.size()),
            signalByResource.at(handoff.resource.value), handoff.resource, handoff.consumer,
            handoff.consumerLeaf, ordinalByLeaf[handoff.consumerLeaf.value]});

    plan.recovery.schemaVersion = 1;
    for (const auto& leaf : contract.leaves) plan.recovery.recreateLeaves.push_back(leaf.id);
    for (const auto& resource : contract.resources)
        if (resource.boundary == ResourceBoundary::Internal)
            plan.recovery.recreateResources.push_back(resource.id);
    plan.recovery.resetTemporalState = true;
    plan.recovery.requireExternalRebind = true;

    plan.identity = ComputeRawCompositionPlanIdentity(plan);
    auto shape = ValidateRawCompositionPlanShape(plan);
    if (!shape) return base::Failure<RawCompositionPlan, PlanError>(shape.error());
    return base::Success<RawCompositionPlan, PlanError>(std::move(plan));
}
}
