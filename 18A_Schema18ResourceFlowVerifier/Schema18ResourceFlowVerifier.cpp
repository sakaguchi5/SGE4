#include "Schema18ResourceFlowVerifier.h"

#include "../00_Foundation/BinaryIO.h"

#include <algorithm>
#include <map>
#include <set>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace sge4::composition::schema18::verification
{
namespace
{
constexpr std::uint32_t VerifierSchemaVersion = 1;
constexpr std::string_view SealDomain =
    "SGE4-L4V1-F5-Schema18-Verified-Resource-Flow-Plan-v1";

VerificationError Error(std::string stage, std::string message)
{
    return {std::move(stage), std::move(message)};
}

template<class T>
base::Result<T, VerificationError> Failure(std::string stage,
                                           std::string message)
{
    return base::Result<T, VerificationError>::Failure(
        Error(std::move(stage), std::move(message)));
}

planning::AllocationOwnership ExpectedOwnership(ResourceBoundary boundary)
{
    switch (boundary)
    {
    case ResourceBoundary::Internal:
        return planning::AllocationOwnership::CompositionOwned;
    case ResourceBoundary::CompositionInput:
        return planning::AllocationOwnership::ExternalInput;
    case ResourceBoundary::CompositionOutput:
        return planning::AllocationOwnership::ExternalOutput;
    default:
        return static_cast<planning::AllocationOwnership>(0);
    }
}

base::Result<std::vector<std::uint32_t>, VerificationError>
DeriveCanonicalSchedule(const PackageCompositionContract& contract)
{
    std::vector<std::set<std::uint32_t>> outgoing(contract.leaves.size());
    std::vector<std::uint32_t> indegree(contract.leaves.size(), 0);
    for (const auto& resource : contract.resources)
    {
        if (resource.boundary != ResourceBoundary::Internal) continue;
        if (!resource.producer.IsValid() ||
            resource.producer.value >= contract.endpoints.size())
            return Failure<std::vector<std::uint32_t>>(
                "verify/schedule", "Internal Resource Flow producer is invalid");
        const auto producerLeaf =
            contract.endpoints[resource.producer.value].leaf.value;
        for (const auto consumer : resource.consumers)
        {
            if (consumer.value >= contract.endpoints.size())
                return Failure<std::vector<std::uint32_t>>(
                    "verify/schedule", "Resource Flow consumer is invalid");
            const auto consumerLeaf =
                contract.endpoints[consumer.value].leaf.value;
            if (producerLeaf == consumerLeaf)
                return Failure<std::vector<std::uint32_t>>(
                    "verify/schedule", "cross-Leaf Resource Flow self-edge is invalid");
            if (outgoing[producerLeaf].insert(consumerLeaf).second)
                ++indegree[consumerLeaf];
        }
    }

    std::set<std::uint32_t> ready;
    for (std::uint32_t leaf = 0; leaf < contract.leaves.size(); ++leaf)
        if (indegree[leaf] == 0) ready.insert(leaf);

    std::vector<std::uint32_t> result;
    while (!ready.empty())
    {
        const auto leaf = *ready.begin();
        ready.erase(ready.begin());
        result.push_back(leaf);
        for (const auto target : outgoing[leaf])
            if (--indegree[target] == 0) ready.insert(target);
    }
    if (result.size() != contract.leaves.size())
        return Failure<std::vector<std::uint32_t>>(
            "verify/cycle", "Composition Contract dependency graph is cyclic");
    return base::Result<std::vector<std::uint32_t>, VerificationError>::Success(
        std::move(result));
}

base::Result<void, VerificationError>
VerifyAuthority(const PackageCompositionContract& contract,
                const planning::RawResourceFlowPlan& raw)
{
    auto contractValidation = ValidateCompositionContract(contract);
    if (!contractValidation)
        return base::Result<void, VerificationError>::Failure(
            Error("verify/contract", contractValidation.Error().stage + ": " +
                                     contractValidation.Error().message));
    auto shape = planning::ValidateRawResourceFlowPlanShape(raw);
    if (!shape)
        return base::Result<void, VerificationError>::Failure(
            Error("verify/raw-shape", shape.Error().stage + ": " +
                                      shape.Error().message));
    if (raw.contractIdentity != contract.identity)
        return base::Result<void, VerificationError>::Failure(
            Error("verify/contract-identity",
                  "raw plan does not target the supplied Composition Contract"));

    if (raw.allocations.size() != contract.resources.size())
        return base::Result<void, VerificationError>::Failure(
            Error("verify/allocation", "allocation cardinality is not authoritative"));
    for (std::uint32_t index = 0; index < contract.resources.size(); ++index)
    {
        const auto& source = contract.resources[index];
        const auto& value = raw.allocations[index];
        if (value.resource != source.id ||
            value.ownership != ExpectedOwnership(source.boundary) ||
            value.kind != source.kind || value.format != source.format ||
            value.sizeBytes != source.sizeBytes)
            return base::Result<void, VerificationError>::Failure(
                Error("verify/allocation",
                      "allocation facts disagree with the authoritative Resource Flow"));
    }

    auto schedule = DeriveCanonicalSchedule(contract);
    if (!schedule)
        return base::Result<void, VerificationError>::Failure(schedule.Error());
    if (raw.schedule.size() != schedule.Value().size())
        return base::Result<void, VerificationError>::Failure(
            Error("verify/schedule", "schedule cardinality is invalid"));
    std::vector<std::uint32_t> ordinalByLeaf(contract.leaves.size(), package::InvalidIndex);
    for (std::uint32_t ordinal = 0; ordinal < schedule.Value().size(); ++ordinal)
    {
        if (raw.schedule[ordinal].ordinal != ordinal ||
            raw.schedule[ordinal].leaf.value != schedule.Value()[ordinal])
            return base::Result<void, VerificationError>::Failure(
                Error("verify/schedule",
                      "Leaf schedule is not the canonical topological order"));
        ordinalByLeaf[schedule.Value()[ordinal]] = ordinal;
    }

    if (raw.bindings.size() != contract.bindings.size())
        return base::Result<void, VerificationError>::Failure(
            Error("verify/binding", "binding cardinality is invalid"));
    for (std::uint32_t index = 0; index < contract.bindings.size(); ++index)
    {
        const auto& source = contract.bindings[index];
        const auto& endpoint = contract.endpoints[source.endpoint.value];
        const auto& value = raw.bindings[index];
        if (value.endpoint != source.endpoint || value.resource != source.resource ||
            value.leaf != endpoint.leaf || value.externalSlot != endpoint.externalSlot ||
            value.access != endpoint.access)
            return base::Result<void, VerificationError>::Failure(
                Error("verify/binding",
                      "endpoint binding disagrees with the authoritative Leaf interface"));
    }

    std::vector<planning::StateHandoffPlan> expectedHandoffs;
    for (const auto& resource : contract.resources)
    {
        if (resource.boundary != ResourceBoundary::Internal) continue;
        const auto& producer = contract.endpoints[resource.producer.value];
        if (producer.synchronization !=
                package::d3d12_v18::ExternalSynchronizationContract::CompletionTokenRequired ||
            producer.frameSemantics != package::d3d12_v18::EndpointFrameSemantics::PerFrame ||
            producer.flags != static_cast<std::uint32_t>(
                package::d3d12_v18::EndpointFlags::Required))
            return base::Result<void, VerificationError>::Failure(
                Error("verify/endpoint-contract",
                      "producer synchronization or frame semantics are unsupported"));
        for (const auto consumerId : resource.consumers)
        {
            const auto& consumer = contract.endpoints[consumerId.value];
            if (consumer.synchronization !=
                    package::d3d12_v18::ExternalSynchronizationContract::CompletionTokenRequired ||
                consumer.frameSemantics != package::d3d12_v18::EndpointFrameSemantics::PerFrame ||
                consumer.flags != static_cast<std::uint32_t>(
                    package::d3d12_v18::EndpointFlags::Required))
                return base::Result<void, VerificationError>::Failure(
                    Error("verify/endpoint-contract",
                          "consumer synchronization or frame semantics are unsupported"));
            expectedHandoffs.push_back({
                resource.id, producer.id, consumer.id,
                producer.leaf, consumer.leaf,
                producer.guaranteedOutgoingState,
                consumer.requiredIncomingState});
        }
    }
    std::sort(expectedHandoffs.begin(), expectedHandoffs.end(),
        [](const auto& left, const auto& right) {
            return std::tie(left.resource.value, left.consumer.value) <
                   std::tie(right.resource.value, right.consumer.value);
        });
    if (raw.handoffs.size() != expectedHandoffs.size())
        return base::Result<void, VerificationError>::Failure(
            Error("verify/handoff", "state-handoff cardinality is invalid"));
    for (std::uint32_t index = 0; index < expectedHandoffs.size(); ++index)
    {
        const auto& expected = expectedHandoffs[index];
        const auto& actual = raw.handoffs[index];
        if (actual.resource != expected.resource ||
            actual.producer != expected.producer ||
            actual.consumer != expected.consumer ||
            actual.producerLeaf != expected.producerLeaf ||
            actual.consumerLeaf != expected.consumerLeaf ||
            actual.producerOutgoingState != expected.producerOutgoingState ||
            actual.consumerIncomingState != expected.consumerIncomingState)
            return base::Result<void, VerificationError>::Failure(
                Error("verify/handoff",
                      "state handoff was not derived from authoritative endpoints"));
    }

    std::vector<planning::SignalPointPlan> expectedSignals;
    std::map<std::uint32_t, std::uint32_t> signalByResource;
    for (const auto& resource : contract.resources)
    {
        if (resource.boundary != ResourceBoundary::Internal) continue;
        const auto& producer = contract.endpoints[resource.producer.value];
        const std::uint32_t id = static_cast<std::uint32_t>(expectedSignals.size());
        signalByResource.emplace(resource.id.value, id);
        expectedSignals.push_back({
            id, resource.id, producer.id, producer.leaf,
            ordinalByLeaf[producer.leaf.value]});
    }
    if (raw.signals.size() != expectedSignals.size())
        return base::Result<void, VerificationError>::Failure(
            Error("verify/signal", "signal cardinality is invalid"));
    for (std::uint32_t index = 0; index < expectedSignals.size(); ++index)
    {
        const auto& expected = expectedSignals[index];
        const auto& actual = raw.signals[index];
        if (actual.id != expected.id || actual.resource != expected.resource ||
            actual.producer != expected.producer ||
            actual.producerLeaf != expected.producerLeaf ||
            actual.producerScheduleOrdinal != expected.producerScheduleOrdinal)
            return base::Result<void, VerificationError>::Failure(
                Error("verify/signal",
                      "signal point was not derived from producer authority"));
    }

    if (raw.waits.size() != expectedHandoffs.size())
        return base::Result<void, VerificationError>::Failure(
            Error("verify/wait", "wait cardinality is invalid"));
    for (std::uint32_t index = 0; index < expectedHandoffs.size(); ++index)
    {
        const auto& handoff = expectedHandoffs[index];
        const auto& wait = raw.waits[index];
        const auto expectedSignal = signalByResource.at(handoff.resource.value);
        if (wait.id != index || wait.signal != expectedSignal ||
            wait.resource != handoff.resource || wait.consumer != handoff.consumer ||
            wait.consumerLeaf != handoff.consumerLeaf ||
            wait.consumerScheduleOrdinal != ordinalByLeaf[handoff.consumerLeaf.value])
            return base::Result<void, VerificationError>::Failure(
                Error("verify/wait",
                      "wait point was not derived from consumer authority"));
    }

    if (raw.recovery.schemaVersion != VerifierSchemaVersion ||
        raw.recovery.recreateLeaves.size() != contract.leaves.size() ||
        !raw.recovery.resetTemporalState ||
        !raw.recovery.requireExternalRebind)
        return base::Result<void, VerificationError>::Failure(
            Error("verify/recovery", "recovery policy is incomplete"));
    for (std::uint32_t index = 0; index < contract.leaves.size(); ++index)
        if (raw.recovery.recreateLeaves[index].value != index)
            return base::Result<void, VerificationError>::Failure(
                Error("verify/recovery", "Leaf recovery identity is not authoritative"));

    std::vector<ResourceFlowId> internalResources;
    for (const auto& resource : contract.resources)
        if (resource.boundary == ResourceBoundary::Internal)
            internalResources.push_back(resource.id);
    if (raw.recovery.recreateResources != internalResources)
        return base::Result<void, VerificationError>::Failure(
            Error("verify/recovery",
                  "composition-owned Resource recovery identity is not authoritative"));

    if (raw.identity != planning::ComputeRawResourceFlowPlanIdentity(raw))
        return base::Result<void, VerificationError>::Failure(
            Error("verify/plan-identity", "raw plan identity is corrupt"));
    return base::Result<void, VerificationError>::Success();
}
}

base::Digest256
ComputeVerifierSeal(const base::Digest256& contractIdentity,
                    const base::Digest256& rawPlanIdentity)
{
    base::BinaryWriter writer;
    writer.WriteU32(VerifierSchemaVersion);
    writer.WriteU32(static_cast<std::uint32_t>(SealDomain.size()));
    writer.WriteBytes(std::as_bytes(
        std::span<const char>(SealDomain.data(), SealDomain.size())));
    writer.WriteBytes(contractIdentity);
    writer.WriteBytes(rawPlanIdentity);
    return base::Sha256(writer.Bytes());
}

base::Result<VerifiedResourceFlowPlan, VerificationError>
VerifyAndSeal(const PackageCompositionContract& contract,
              const planning::RawResourceFlowPlan& rawPlan)
{
    auto validation = VerifyAuthority(contract, rawPlan);
    if (!validation)
        return base::Result<VerifiedResourceFlowPlan, VerificationError>::Failure(
            validation.Error());
    auto sealed = rawPlan;
    const auto seal = ComputeVerifierSeal(contract.identity, rawPlan.identity);
    return base::Result<VerifiedResourceFlowPlan, VerificationError>::Success(
        VerifiedResourceFlowPlan(
            std::move(sealed), seal,
            VerifiedResourceFlowPlan::ConstructionToken{}));
}

base::Result<void, VerificationError>
ValidateVerifiedPlan(const PackageCompositionContract& contract,
                     const VerifiedResourceFlowPlan& verified)
{
    auto validation = VerifyAuthority(contract, verified.Plan());
    if (!validation) return validation;
    const auto expected = ComputeVerifierSeal(
        contract.identity, verified.Plan().identity);
    if (verified.Seal() != expected)
        return base::Result<void, VerificationError>::Failure(
            Error("verify/seal", "Verified Resource Flow Plan seal is invalid"));
    return base::Result<void, VerificationError>::Success();
}
}
