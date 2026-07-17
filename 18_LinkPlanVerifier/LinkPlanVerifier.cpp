#include "LinkPlanVerifier.h"

#include "../09_FrozenPackageCore/PackageDigest.h"
#include "../09_FrozenPackageCore/PackageReader.h"
#include "../10_D3D12PackageSchema/D3D12Encoding.h"

#include <algorithm>
#include <map>
#include <queue>
#include <set>
#include <tuple>

namespace sge4::composition
{
namespace
{
LinkVerificationError Error(std::string stage, std::string message) { return {std::move(stage), std::move(message)}; }
bool AllowsImport(EndpointDirection value) { return value == EndpointDirection::Import || value == EndpointDirection::ImportExport; }
bool AllowsExport(EndpointDirection value) { return value == EndpointDirection::Export || value == EndpointDirection::ImportExport; }
bool DeviceCompatible(const package::d3d12_v13::TargetProfile& left, const package::d3d12_v13::TargetProfile& right)
{
    return left.minimumFeatureLevel == right.minimumFeatureLevel && left.shaderModelMajor == right.shaderModelMajor &&
        left.shaderModelMinor == right.shaderModelMinor && left.rootSignatureMajor == right.rootSignatureMajor &&
        left.rootSignatureMinor == right.rootSignatureMinor && left.barrierModel == right.barrierModel &&
        left.shaderBinaryFormat == right.shaderBinaryFormat && left.framesInFlight == right.framesInFlight &&
        left.directQueueCount == right.directQueueCount && left.computeQueueCount == right.computeQueueCount &&
        left.copyQueueCount == right.copyQueueCount && left.requiredFeatureBits0 == right.requiredFeatureBits0 &&
        left.requiredFeatureBits1 == right.requiredFeatureBits1;
}

base::Result<std::vector<LeafPackageId>, LinkVerificationError> CanonicalSchedule(const PackageCompositionContract& contract)
{
    std::vector<std::set<std::uint32_t>> outgoing(contract.leaves.size());
    std::vector<std::uint32_t> indegree(contract.leaves.size(), 0);
    for (const auto& link : contract.links)
    {
        if (link.producer.value >= contract.endpoints.size() || link.consumer.value >= contract.endpoints.size())
            return base::Result<std::vector<LeafPackageId>, LinkVerificationError>::Failure(Error("verify/graph", "link endpoint is out of range"));
        const auto producer = contract.endpoints[link.producer.value].leaf.value;
        const auto consumer = contract.endpoints[link.consumer.value].leaf.value;
        if (producer >= contract.leaves.size() || consumer >= contract.leaves.size() || producer == consumer)
            return base::Result<std::vector<LeafPackageId>, LinkVerificationError>::Failure(Error("verify/graph", "self-link or unknown Leaf is forbidden"));
        if (outgoing[producer].insert(consumer).second) ++indegree[consumer];
    }
    std::priority_queue<std::uint32_t, std::vector<std::uint32_t>, std::greater<>> ready;
    for (std::uint32_t index = 0; index < indegree.size(); ++index) if (indegree[index] == 0) ready.push(index);
    std::vector<LeafPackageId> result;
    while (!ready.empty())
    {
        const auto leaf = ready.top(); ready.pop(); result.push_back({leaf});
        for (const auto consumer : outgoing[leaf]) if (--indegree[consumer] == 0) ready.push(consumer);
    }
    if (result.size() != contract.leaves.size())
        return base::Result<std::vector<LeafPackageId>, LinkVerificationError>::Failure(Error("verify/graph", "Package graph contains a cycle"));
    return base::Result<std::vector<LeafPackageId>, LinkVerificationError>::Success(std::move(result));
}
}

base::Result<VerifiedLinkPlan, LinkVerificationError> VerifyAndSeal(
    const PackageCompositionContract& contract,
    LinkPlanIR plan)
{
    if (contract.leaves.size() < 2 || contract.identity != ComputeContractIdentity(contract))
        return base::Result<VerifiedLinkPlan, LinkVerificationError>::Failure(Error("verify/contract-identity", "composition contract identity is invalid"));
    if (plan.contractIdentity != contract.identity)
        return base::Result<VerifiedLinkPlan, LinkVerificationError>::Failure(Error("verify/plan-contract", "Link Plan does not name the verified contract"));

    std::uint32_t surfaceOwners = 0;
    const auto target = contract.leaves.front().targetKind;
    const auto schema = contract.leaves.front().schemaVersion;
    const auto runtime = contract.leaves.front().minimumRuntimeVersion;
    package::d3d12_v13::TargetProfile domainProfile{};
    bool hasDomainProfile = false;
    for (std::uint32_t index = 0; index < contract.leaves.size(); ++index)
    {
        const auto& leaf = contract.leaves[index];
        if (leaf.id.value != index || leaf.targetKind != target || leaf.schemaVersion != schema ||
            leaf.minimumRuntimeVersion != runtime)
            return base::Result<VerifiedLinkPlan, LinkVerificationError>::Failure(Error("verify/leaf-compatibility", "Leaf target, schema, runtime, or target profile is incompatible"));
        if (leaf.surfaceSlotCount != 0) ++surfaceOwners;
        auto decoded = package::PackageReader::Read(leaf.packageBytes);
        if (!decoded || decoded.Value().ExecutionDigest() != leaf.executionDigest ||
            decoded.Value().Target() != leaf.targetKind ||
            decoded.Value().Header().targetSchemaVersion != leaf.schemaVersion ||
            decoded.Value().Header().minimumRuntimeVersion != leaf.minimumRuntimeVersion ||
            decoded.Value().Header().targetProfileDigest != leaf.targetProfileDigest ||
            package::ComputeFileDigest(decoded.Value().FileBytes()) != leaf.fileDigest)
            return base::Result<VerifiedLinkPlan, LinkVerificationError>::Failure(Error("verify/leaf-digest", "embedded Leaf bytes do not match their identities"));
        auto view = package::d3d12_v13::D3D12PackageView::Decode(decoded.Value());
        if (!view || view.Value().ExternalSlots().size() != leaf.externalSlots.size() ||
            view.Value().SurfaceSlots().size() != leaf.surfaceSlotCount)
            return base::Result<VerifiedLinkPlan, LinkVerificationError>::Failure(Error("verify/leaf-contract", "embedded Leaf interface cardinality does not match the extracted contract"));
        if (!hasDomainProfile) { domainProfile = view.Value().Profile(); hasDomainProfile = true; }
        else if (!DeviceCompatible(domainProfile, view.Value().Profile()))
            return base::Result<VerifiedLinkPlan, LinkVerificationError>::Failure(Error("verify/leaf-compatibility", "Leaf device-requirement profiles are incompatible"));
        for (std::uint32_t slotIndex = 0; slotIndex < leaf.externalSlots.size(); ++slotIndex)
        {
            const auto& actual = view.Value().ExternalSlots()[slotIndex];
            const auto& extracted = leaf.externalSlots[slotIndex];
            if (actual.id.value != slotIndex || extracted.slot != slotIndex ||
                actual.requiredKind != extracted.kind || actual.requiredFormat != extracted.format ||
                actual.minimumBytes != extracted.minimumBytes ||
                actual.requiredIncomingState != extracted.incomingState ||
                actual.guaranteedOutgoingState != extracted.outgoingState ||
                actual.synchronizationContract != extracted.synchronization)
                return base::Result<VerifiedLinkPlan, LinkVerificationError>::Failure(Error("verify/leaf-contract", "embedded Leaf interface does not match the extracted contract"));
        }
    }
    if (surfaceOwners > 1)
        return base::Result<VerifiedLinkPlan, LinkVerificationError>::Failure(Error("verify/surface", "more than one Leaf owns a presentation surface"));

    if (contract.resources.size() != plan.resources.size() || contract.bindings.size() != plan.bindings.size() ||
        contract.links.size() != plan.signalWaits.size() || contract.leaves.size() != plan.schedule.size())
        return base::Result<VerifiedLinkPlan, LinkVerificationError>::Failure(Error("verify/cardinality", "Link Plan table cardinality does not match the contract"));
    if (plan.recovery.policy != CompositionRecoveryPolicy::RebuildWholeDeviceDomain)
        return base::Result<VerifiedLinkPlan, LinkVerificationError>::Failure(Error("verify/recovery", "Level 4 v1 requires whole-DeviceDomain recovery"));

    std::vector<std::uint32_t> endpointBindingCount(contract.endpoints.size(), 0);
    std::vector<SharedResourceId> endpointResources(contract.endpoints.size());
    for (std::uint32_t index = 0; index < contract.resources.size(); ++index)
    {
        const auto& resource = contract.resources[index];
        const auto& proposed = plan.resources[index];
        if (resource.id.value != index || resource.kind != package::d3d12_v13::ResourceKind::Buffer || resource.sizeBytes == 0 ||
            resource.ownership != SharedResourceOwnership::DeviceDomain || proposed.resource != resource.id ||
            proposed.owner != resource.ownership || proposed.allocationBytes != resource.sizeBytes)
            return base::Result<VerifiedLinkPlan, LinkVerificationError>::Failure(Error("verify/resource", "shared Buffer allocation authority is invalid"));
    }
    for (std::uint32_t index = 0; index < contract.endpoints.size(); ++index)
    {
        const auto& endpoint = contract.endpoints[index];
        if (endpoint.id.value != index || endpoint.leaf.value >= contract.leaves.size() ||
            endpoint.externalSlot >= contract.leaves[endpoint.leaf.value].externalSlots.size() ||
            endpoint.contract.slot != endpoint.externalSlot || endpoint.contract.kind != package::d3d12_v13::ResourceKind::Buffer ||
            endpoint.contract.synchronization != package::d3d12_v13::ExternalSynchronizationContract::CompletionTokenRequired)
            return base::Result<VerifiedLinkPlan, LinkVerificationError>::Failure(Error("verify/endpoint", "endpoint does not reproduce its Leaf external-slot contract"));
        const auto& extracted = contract.leaves[endpoint.leaf.value].externalSlots[endpoint.externalSlot];
        if (endpoint.contract.kind != extracted.kind || endpoint.contract.format != extracted.format ||
            endpoint.contract.minimumBytes != extracted.minimumBytes || endpoint.contract.incomingState != extracted.incomingState ||
            endpoint.contract.outgoingState != extracted.outgoingState)
            return base::Result<VerifiedLinkPlan, LinkVerificationError>::Failure(Error("verify/endpoint", "endpoint contract was mutated after extraction"));
    }
    for (std::uint32_t index = 0; index < contract.bindings.size(); ++index)
    {
        const auto& binding = contract.bindings[index]; const auto& proposed = plan.bindings[index];
        if (binding.endpoint.value >= contract.endpoints.size() || binding.resource.value >= contract.resources.size() ||
            proposed.endpoint != binding.endpoint || proposed.resource != binding.resource ||
            ++endpointBindingCount[binding.endpoint.value] != 1)
            return base::Result<VerifiedLinkPlan, LinkVerificationError>::Failure(Error("verify/binding", "endpoint binding authority is invalid"));
        endpointResources[binding.endpoint.value] = binding.resource;
        const auto& endpoint = contract.endpoints[binding.endpoint.value]; const auto& resource = contract.resources[binding.resource.value];
        if (endpoint.contract.kind != resource.kind || endpoint.contract.format != resource.format || resource.sizeBytes < endpoint.contract.minimumBytes)
            return base::Result<VerifiedLinkPlan, LinkVerificationError>::Failure(Error("verify/binding-shape", "shared resource is incompatible with its endpoint"));
    }
    if (std::ranges::any_of(endpointBindingCount, [](auto count) { return count != 1; }))
        return base::Result<VerifiedLinkPlan, LinkVerificationError>::Failure(Error("verify/binding", "every endpoint must be bound exactly once"));

    std::vector<std::set<std::uint32_t>> producers(contract.resources.size());
    std::vector<std::set<std::uint32_t>> consumers(contract.resources.size());
    for (const auto& binding : contract.bindings)
    {
        const auto& endpoint = contract.endpoints[binding.endpoint.value];
        if (AllowsExport(endpoint.direction)) producers[binding.resource.value].insert(endpoint.id.value);
        if (AllowsImport(endpoint.direction)) consumers[binding.resource.value].insert(endpoint.id.value);
    }
    std::set<std::tuple<std::uint32_t, std::uint32_t, std::uint32_t>> uniqueLinks;
    for (std::uint32_t index = 0; index < contract.links.size(); ++index)
    {
        const auto& link = contract.links[index]; const auto& proposed = plan.signalWaits[index];
        if (link.id.value != index || link.resource.value >= contract.resources.size() || link.producer.value >= contract.endpoints.size() ||
            link.consumer.value >= contract.endpoints.size() || endpointResources[link.producer.value] != link.resource ||
            endpointResources[link.consumer.value] != link.resource || proposed.link != link.id ||
            proposed.signalEndpoint != link.producer || proposed.waitEndpoint != link.consumer || proposed.handoffState != link.handoffState ||
            !uniqueLinks.emplace(link.resource.value, link.producer.value, link.consumer.value).second)
            return base::Result<VerifiedLinkPlan, LinkVerificationError>::Failure(Error("verify/signal-wait", "cross-Package signal/wait authority is invalid"));
        const auto& producer = contract.endpoints[link.producer.value]; const auto& consumer = contract.endpoints[link.consumer.value];
        if (!AllowsExport(producer.direction) || !AllowsImport(consumer.direction) || producer.leaf == consumer.leaf ||
            producer.contract.outgoingState != link.handoffState || consumer.contract.incomingState != link.handoffState)
            return base::Result<VerifiedLinkPlan, LinkVerificationError>::Failure(Error("verify/state-handoff", "producer, consumer, or state handoff is incompatible"));
        if (!producers[link.resource.value].contains(link.producer.value) || !consumers[link.resource.value].contains(link.consumer.value))
            return base::Result<VerifiedLinkPlan, LinkVerificationError>::Failure(Error("verify/signal-wait", "link does not use the declared resource producer and consumer"));
    }
    for (std::uint32_t index = 0; index < contract.resources.size(); ++index)
    {
        const auto boundary = contract.resources[index].boundary;
        if (producers[index].size() > 1 ||
            (boundary == SharedResourceBoundary::CompositionInput && (!producers[index].empty() || consumers[index].empty())) ||
            (boundary != SharedResourceBoundary::CompositionInput && producers[index].size() != 1) ||
            (boundary == SharedResourceBoundary::Internal && consumers[index].empty()) ||
            (boundary == SharedResourceBoundary::CompositionOutput && !consumers[index].empty()))
            return base::Result<VerifiedLinkPlan, LinkVerificationError>::Failure(Error("verify/writer", "shared resource producer/consumer ownership is invalid"));
        if (!producers[index].empty())
        {
            const auto producer = *producers[index].begin();
            if (contract.resources[index].initialState != contract.endpoints[producer].contract.incomingState)
                return base::Result<VerifiedLinkPlan, LinkVerificationError>::Failure(Error("verify/initial-state", "shared resource initial state does not match its producer acquisition"));
        }
        else
            for (const auto consumer : consumers[index])
                if (contract.resources[index].initialState != contract.endpoints[consumer].contract.incomingState)
                    return base::Result<VerifiedLinkPlan, LinkVerificationError>::Failure(Error("verify/initial-state", "composition input state does not match its consumer acquisition"));
    }

    auto canonical = CanonicalSchedule(contract);
    if (!canonical) return base::Result<VerifiedLinkPlan, LinkVerificationError>::Failure(canonical.Error());
    for (std::uint32_t index = 0; index < plan.schedule.size(); ++index)
        if (plan.schedule[index].ordinal != index || plan.schedule[index].leaf != canonical.Value()[index])
            return base::Result<VerifiedLinkPlan, LinkVerificationError>::Failure(Error("verify/schedule", "Package schedule is not the canonical topological order"));

    if (plan.identity != ComputeLinkPlanIdentity(contract, plan))
        return base::Result<VerifiedLinkPlan, LinkVerificationError>::Failure(Error("verify/plan-identity", "Link Plan identity is corrupt"));
    return base::Result<VerifiedLinkPlan, LinkVerificationError>::Success(VerifiedLinkPlan(std::move(plan)));
}
}
