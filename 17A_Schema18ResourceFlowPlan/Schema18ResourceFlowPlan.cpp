#include "Schema18ResourceFlowPlan.h"

#include "../00_Foundation/BinaryIO.h"

#include <algorithm>
#include <map>
#include <set>
#include <tuple>
#include <utility>

namespace sge4::composition::schema18::planning
{
namespace
{
constexpr std::uint32_t PlanMagic = 0x3450'4753u; // "SGP4" little-endian
constexpr std::uint32_t PlanVersion = 1;

PlanError Error(std::string stage, std::string message)
{
    return {std::move(stage), std::move(message)};
}

template<class T>
base::Result<T, PlanError> Failure(std::string stage, std::string message)
{
    return base::Result<T, PlanError>::Failure(
        Error(std::move(stage), std::move(message)));
}

void WriteDigest(base::BinaryWriter& writer, const base::Digest256& value)
{
    writer.WriteBytes(value);
}

void WriteState(base::BinaryWriter& writer,
                const package::d3d12_v18::ResourceState& value)
{
    writer.WriteU16(static_cast<std::uint16_t>(value.stateClass));
    writer.WriteU16(value.reserved);
    writer.WriteU32(value.explicitBits);
}

base::Result<base::Digest256, PlanError> ReadDigest(base::BinaryReader& reader)
{
    auto bytes = reader.ReadBytes(base::Digest256{}.size());
    if (!bytes) return Failure<base::Digest256>("plan/read", bytes.Error());
    base::Digest256 result{};
    std::copy(bytes.Value().begin(), bytes.Value().end(), result.begin());
    return base::Result<base::Digest256, PlanError>::Success(result);
}

base::Result<package::d3d12_v18::ResourceState, PlanError>
ReadState(base::BinaryReader& reader)
{
    auto stateClass = reader.ReadU16();
    auto reserved = reader.ReadU16();
    auto bits = reader.ReadU32();
    if (!stateClass || !reserved || !bits)
        return Failure<package::d3d12_v18::ResourceState>(
            "plan/read", "truncated ResourceState");
    package::d3d12_v18::ResourceState state;
    state.stateClass = static_cast<package::d3d12_v13::StateClass>(stateClass.Value());
    state.reserved = reserved.Value();
    state.explicitBits = bits.Value();
    return base::Result<package::d3d12_v18::ResourceState, PlanError>::Success(state);
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

std::vector<std::byte> SerializeBody(const RawResourceFlowPlan& plan)
{
    base::BinaryWriter writer;
    writer.WriteU32(PlanMagic);
    writer.WriteU32(PlanVersion);
    WriteDigest(writer, plan.contractIdentity);
    writer.WriteU32(static_cast<std::uint32_t>(plan.allocations.size()));
    writer.WriteU32(static_cast<std::uint32_t>(plan.schedule.size()));
    writer.WriteU32(static_cast<std::uint32_t>(plan.bindings.size()));
    writer.WriteU32(static_cast<std::uint32_t>(plan.handoffs.size()));
    writer.WriteU32(static_cast<std::uint32_t>(plan.signals.size()));
    writer.WriteU32(static_cast<std::uint32_t>(plan.waits.size()));

    for (const auto& allocation : plan.allocations)
    {
        writer.WriteU32(allocation.resource.value);
        writer.WriteU16(static_cast<std::uint16_t>(allocation.ownership));
        writer.WriteU16(static_cast<std::uint16_t>(allocation.kind));
        writer.WriteU32(static_cast<std::uint32_t>(allocation.format));
        writer.WriteU64(allocation.sizeBytes);
    }
    for (const auto& entry : plan.schedule)
    {
        writer.WriteU32(entry.ordinal);
        writer.WriteU32(entry.leaf.value);
    }
    for (const auto& binding : plan.bindings)
    {
        writer.WriteU32(binding.endpoint.value);
        writer.WriteU32(binding.resource.value);
        writer.WriteU32(binding.leaf.value);
        writer.WriteU32(binding.externalSlot.value);
        writer.WriteU16(static_cast<std::uint16_t>(binding.access));
        writer.WriteU16(0);
    }
    for (const auto& handoff : plan.handoffs)
    {
        writer.WriteU32(handoff.resource.value);
        writer.WriteU32(handoff.producer.value);
        writer.WriteU32(handoff.consumer.value);
        writer.WriteU32(handoff.producerLeaf.value);
        writer.WriteU32(handoff.consumerLeaf.value);
        WriteState(writer, handoff.producerOutgoingState);
        WriteState(writer, handoff.consumerIncomingState);
    }
    for (const auto& signal : plan.signals)
    {
        writer.WriteU32(signal.id);
        writer.WriteU32(signal.resource.value);
        writer.WriteU32(signal.producer.value);
        writer.WriteU32(signal.producerLeaf.value);
        writer.WriteU32(signal.producerScheduleOrdinal);
    }
    for (const auto& wait : plan.waits)
    {
        writer.WriteU32(wait.id);
        writer.WriteU32(wait.signal);
        writer.WriteU32(wait.resource.value);
        writer.WriteU32(wait.consumer.value);
        writer.WriteU32(wait.consumerLeaf.value);
        writer.WriteU32(wait.consumerScheduleOrdinal);
    }

    writer.WriteU32(plan.recovery.schemaVersion);
    writer.WriteU32(static_cast<std::uint32_t>(plan.recovery.recreateLeaves.size()));
    writer.WriteU32(static_cast<std::uint32_t>(plan.recovery.recreateResources.size()));
    writer.WriteU8(plan.recovery.resetTemporalState ? 1 : 0);
    writer.WriteU8(plan.recovery.requireExternalRebind ? 1 : 0);
    writer.WriteU16(0);
    for (const auto leaf : plan.recovery.recreateLeaves) writer.WriteU32(leaf.value);
    for (const auto resource : plan.recovery.recreateResources) writer.WriteU32(resource.value);
    return std::move(writer).Take();
}

base::Result<std::vector<std::uint32_t>, PlanError>
DeriveSchedule(const PackageCompositionContract& contract)
{
    const auto leafCount = contract.leaves.size();
    std::vector<std::set<std::uint32_t>> outgoing(leafCount);
    std::vector<std::uint32_t> indegree(leafCount, 0);

    for (const auto& resource : contract.resources)
    {
        if (resource.boundary != ResourceBoundary::Internal) continue;
        if (!resource.producer.IsValid() || resource.producer.value >= contract.endpoints.size())
            return Failure<std::vector<std::uint32_t>>(
                "plan/dependency", "Internal Resource Flow has no valid producer");
        const auto producerLeaf = contract.endpoints[resource.producer.value].leaf.value;
        for (const auto consumer : resource.consumers)
        {
            if (consumer.value >= contract.endpoints.size())
                return Failure<std::vector<std::uint32_t>>(
                    "plan/dependency", "Resource Flow has an invalid consumer");
            const auto consumerLeaf = contract.endpoints[consumer.value].leaf.value;
            if (producerLeaf == consumerLeaf)
                return Failure<std::vector<std::uint32_t>>(
                    "plan/dependency", "cross-Leaf Resource Flow cannot connect one Leaf to itself");
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
        {
            if (--indegree[consumer] == 0) ready.insert(consumer);
        }
    }
    if (schedule.size() != leafCount)
        return Failure<std::vector<std::uint32_t>>(
            "plan/cycle", "Composition Contract contains a cyclic Leaf dependency");
    return base::Result<std::vector<std::uint32_t>, PlanError>::Success(
        std::move(schedule));
}
}

base::Result<RawResourceFlowPlan, PlanError>
ProposeResourceFlowPlan(const PackageCompositionContract& contract)
{
    auto contractValidation = ValidateCompositionContract(contract);
    if (!contractValidation)
        return Failure<RawResourceFlowPlan>(
            "plan/contract", contractValidation.Error().stage + ": " +
            contractValidation.Error().message);

    auto scheduleResult = DeriveSchedule(contract);
    if (!scheduleResult)
        return base::Result<RawResourceFlowPlan, PlanError>::Failure(
            scheduleResult.Error());

    RawResourceFlowPlan plan;
    plan.contractIdentity = contract.identity;

    for (const auto& resource : contract.resources)
    {
        plan.allocations.push_back({
            resource.id,
            OwnershipFor(resource.boundary),
            resource.kind,
            resource.format,
            resource.sizeBytes});
    }

    std::vector<std::uint32_t> ordinalByLeaf(contract.leaves.size(), package::InvalidIndex);
    for (std::uint32_t ordinal = 0; ordinal < scheduleResult.Value().size(); ++ordinal)
    {
        const auto leaf = scheduleResult.Value()[ordinal];
        ordinalByLeaf[leaf] = ordinal;
        plan.schedule.push_back({ordinal, {leaf}});
    }

    for (const auto& binding : contract.bindings)
    {
        const auto& endpoint = contract.endpoints[binding.endpoint.value];
        plan.bindings.push_back({
            binding.endpoint,
            binding.resource,
            endpoint.leaf,
            endpoint.externalSlot,
            endpoint.access});
    }

    for (const auto& resource : contract.resources)
    {
        if (resource.boundary != ResourceBoundary::Internal) continue;
        const auto& producer = contract.endpoints[resource.producer.value];
        for (const auto consumerId : resource.consumers)
        {
            const auto& consumer = contract.endpoints[consumerId.value];
            plan.handoffs.push_back({
                resource.id,
                producer.id,
                consumer.id,
                producer.leaf,
                consumer.leaf,
                producer.guaranteedOutgoingState,
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
        if (resource.boundary != ResourceBoundary::Internal) continue;
        const auto& producer = contract.endpoints[resource.producer.value];
        const std::uint32_t signalId = static_cast<std::uint32_t>(plan.signals.size());
        signalByResource.emplace(resource.id.value, signalId);
        plan.signals.push_back({
            signalId,
            resource.id,
            producer.id,
            producer.leaf,
            ordinalByLeaf[producer.leaf.value]});
    }

    for (const auto& handoff : plan.handoffs)
    {
        plan.waits.push_back({
            static_cast<std::uint32_t>(plan.waits.size()),
            signalByResource.at(handoff.resource.value),
            handoff.resource,
            handoff.consumer,
            handoff.consumerLeaf,
            ordinalByLeaf[handoff.consumerLeaf.value]});
    }

    plan.recovery.schemaVersion = 1;
    for (const auto& leaf : contract.leaves)
        plan.recovery.recreateLeaves.push_back(leaf.id);
    for (const auto& resource : contract.resources)
        if (resource.boundary == ResourceBoundary::Internal)
            plan.recovery.recreateResources.push_back(resource.id);
    plan.recovery.resetTemporalState = true;
    plan.recovery.requireExternalRebind = true;

    plan.identity = ComputeRawResourceFlowPlanIdentity(plan);
    auto shape = ValidateRawResourceFlowPlanShape(plan);
    if (!shape)
        return base::Result<RawResourceFlowPlan, PlanError>::Failure(shape.Error());
    return base::Result<RawResourceFlowPlan, PlanError>::Success(std::move(plan));
}

base::Result<void, PlanError>
ValidateRawResourceFlowPlanShape(const RawResourceFlowPlan& plan)
{
    if (plan.contractIdentity == base::Digest256{})
        return base::Result<void, PlanError>::Failure(
            Error("plan/shape", "raw plan has no Contract identity"));

    for (std::uint32_t index = 0; index < plan.allocations.size(); ++index)
    {
        const auto& value = plan.allocations[index];
        if (value.resource.value != index || value.sizeBytes == 0 ||
            (value.ownership != AllocationOwnership::CompositionOwned &&
             value.ownership != AllocationOwnership::ExternalInput &&
             value.ownership != AllocationOwnership::ExternalOutput))
            return base::Result<void, PlanError>::Failure(
                Error("plan/shape-allocation", "allocations are not dense or valid"));
    }

    std::set<std::uint32_t> leaves;
    for (std::uint32_t index = 0; index < plan.schedule.size(); ++index)
    {
        const auto& entry = plan.schedule[index];
        if (entry.ordinal != index || !entry.leaf.IsValid() ||
            !leaves.insert(entry.leaf.value).second)
            return base::Result<void, PlanError>::Failure(
                Error("plan/shape-schedule", "schedule is not a dense Leaf permutation"));
    }

    for (std::uint32_t index = 0; index < plan.bindings.size(); ++index)
    {
        const auto& binding = plan.bindings[index];
        if (binding.endpoint.value != index || !binding.resource.IsValid() ||
            !binding.leaf.IsValid() || !binding.externalSlot.IsValid() ||
            (binding.access != package::d3d12_v18::EndpointAccess::ReadOnly &&
             binding.access != package::d3d12_v18::EndpointAccess::WriteOnly))
            return base::Result<void, PlanError>::Failure(
                Error("plan/shape-binding", "bindings are not dense or valid"));
    }

    if (!std::is_sorted(plan.handoffs.begin(), plan.handoffs.end(), [](const auto& left, const auto& right) {
            return std::tie(left.resource.value, left.consumer.value) <
                   std::tie(right.resource.value, right.consumer.value);
        }))
        return base::Result<void, PlanError>::Failure(
            Error("plan/shape-handoff", "handoffs are not canonical"));

    for (std::uint32_t index = 0; index < plan.signals.size(); ++index)
        if (plan.signals[index].id != index)
            return base::Result<void, PlanError>::Failure(
                Error("plan/shape-signal", "signal IDs are not dense"));
    for (std::uint32_t index = 0; index < plan.waits.size(); ++index)
        if (plan.waits[index].id != index || plan.waits[index].signal >= plan.signals.size())
            return base::Result<void, PlanError>::Failure(
                Error("plan/shape-wait", "wait IDs or signal references are invalid"));

    if (plan.recovery.schemaVersion != 1 ||
        !plan.recovery.resetTemporalState ||
        !plan.recovery.requireExternalRebind ||
        !std::is_sorted(plan.recovery.recreateLeaves.begin(),
                        plan.recovery.recreateLeaves.end(),
                        [](auto left, auto right) { return left.value < right.value; }) ||
        !std::is_sorted(plan.recovery.recreateResources.begin(),
                        plan.recovery.recreateResources.end(),
                        [](auto left, auto right) { return left.value < right.value; }))
        return base::Result<void, PlanError>::Failure(
            Error("plan/shape-recovery", "recovery record is not canonical"));

    if (plan.identity != ComputeRawResourceFlowPlanIdentity(plan))
        return base::Result<void, PlanError>::Failure(
            Error("plan/shape-identity", "raw plan identity is invalid"));
    return base::Result<void, PlanError>::Success();
}

std::vector<std::byte>
SerializeRawResourceFlowPlan(const RawResourceFlowPlan& plan)
{
    auto bytes = SerializeBody(plan);
    bytes.insert(bytes.end(), plan.identity.begin(), plan.identity.end());
    return bytes;
}

base::Digest256
ComputeRawResourceFlowPlanIdentity(const RawResourceFlowPlan& plan)
{
    const auto body = SerializeBody(plan);
    return base::Sha256(body);
}

base::Result<RawResourceFlowPlan, PlanError>
DeserializeRawResourceFlowPlan(std::span<const std::byte> bytes)
{
    if (bytes.size() < 8 + base::Digest256{}.size() * 2)
        return Failure<RawResourceFlowPlan>("plan/read", "raw plan bytes are too small");
    base::BinaryReader reader(bytes);
    auto magic = reader.ReadU32();
    auto version = reader.ReadU32();
    if (!magic || !version || magic.Value() != PlanMagic || version.Value() != PlanVersion)
        return Failure<RawResourceFlowPlan>("plan/read", "raw plan magic or version is invalid");

    RawResourceFlowPlan plan;
    auto contractIdentity = ReadDigest(reader);
    if (!contractIdentity) return base::Result<RawResourceFlowPlan, PlanError>::Failure(contractIdentity.Error());
    plan.contractIdentity = contractIdentity.Value();

    auto allocationCount = reader.ReadU32();
    auto scheduleCount = reader.ReadU32();
    auto bindingCount = reader.ReadU32();
    auto handoffCount = reader.ReadU32();
    auto signalCount = reader.ReadU32();
    auto waitCount = reader.ReadU32();
    if (!allocationCount || !scheduleCount || !bindingCount || !handoffCount ||
        !signalCount || !waitCount)
        return Failure<RawResourceFlowPlan>("plan/read", "raw plan counts are truncated");

    constexpr std::uint32_t MaximumRecords = 1'000'000;
    if (allocationCount.Value() > MaximumRecords || scheduleCount.Value() > MaximumRecords ||
        bindingCount.Value() > MaximumRecords || handoffCount.Value() > MaximumRecords ||
        signalCount.Value() > MaximumRecords || waitCount.Value() > MaximumRecords)
        return Failure<RawResourceFlowPlan>("plan/read", "raw plan count exceeds the supported limit");

    for (std::uint32_t i = 0; i < allocationCount.Value(); ++i)
    {
        auto resource = reader.ReadU32();
        auto ownership = reader.ReadU16();
        auto kind = reader.ReadU16();
        auto format = reader.ReadU32();
        auto size = reader.ReadU64();
        if (!resource || !ownership || !kind || !format || !size)
            return Failure<RawResourceFlowPlan>("plan/read", "allocation record is truncated");
        plan.allocations.push_back({
            {resource.Value()}, static_cast<AllocationOwnership>(ownership.Value()),
            static_cast<package::d3d12_v18::ResourceKind>(kind.Value()),
            static_cast<package::d3d12_v18::Format>(format.Value()), size.Value()});
    }
    for (std::uint32_t i = 0; i < scheduleCount.Value(); ++i)
    {
        auto ordinal = reader.ReadU32();
        auto leaf = reader.ReadU32();
        if (!ordinal || !leaf)
            return Failure<RawResourceFlowPlan>("plan/read", "schedule record is truncated");
        plan.schedule.push_back({ordinal.Value(), {leaf.Value()}});
    }
    for (std::uint32_t i = 0; i < bindingCount.Value(); ++i)
    {
        auto endpoint = reader.ReadU32();
        auto resource = reader.ReadU32();
        auto leaf = reader.ReadU32();
        auto slot = reader.ReadU32();
        auto access = reader.ReadU16();
        auto reserved = reader.ReadU16();
        if (!endpoint || !resource || !leaf || !slot || !access || !reserved || reserved.Value() != 0)
            return Failure<RawResourceFlowPlan>("plan/read", "binding record is truncated or reserved bits are set");
        plan.bindings.push_back({
            {endpoint.Value()}, {resource.Value()}, {leaf.Value()}, {slot.Value()},
            static_cast<package::d3d12_v18::EndpointAccess>(access.Value())});
    }
    for (std::uint32_t i = 0; i < handoffCount.Value(); ++i)
    {
        auto resource = reader.ReadU32();
        auto producer = reader.ReadU32();
        auto consumer = reader.ReadU32();
        auto producerLeaf = reader.ReadU32();
        auto consumerLeaf = reader.ReadU32();
        auto outgoing = ReadState(reader);
        auto incoming = ReadState(reader);
        if (!resource || !producer || !consumer || !producerLeaf || !consumerLeaf || !outgoing || !incoming)
            return Failure<RawResourceFlowPlan>("plan/read", "handoff record is truncated");
        plan.handoffs.push_back({
            {resource.Value()}, {producer.Value()}, {consumer.Value()},
            {producerLeaf.Value()}, {consumerLeaf.Value()}, outgoing.Value(), incoming.Value()});
    }
    for (std::uint32_t i = 0; i < signalCount.Value(); ++i)
    {
        auto id = reader.ReadU32();
        auto resource = reader.ReadU32();
        auto producer = reader.ReadU32();
        auto leaf = reader.ReadU32();
        auto ordinal = reader.ReadU32();
        if (!id || !resource || !producer || !leaf || !ordinal)
            return Failure<RawResourceFlowPlan>("plan/read", "signal record is truncated");
        plan.signals.push_back({id.Value(), {resource.Value()}, {producer.Value()},
                                {leaf.Value()}, ordinal.Value()});
    }
    for (std::uint32_t i = 0; i < waitCount.Value(); ++i)
    {
        auto id = reader.ReadU32();
        auto signal = reader.ReadU32();
        auto resource = reader.ReadU32();
        auto consumer = reader.ReadU32();
        auto leaf = reader.ReadU32();
        auto ordinal = reader.ReadU32();
        if (!id || !signal || !resource || !consumer || !leaf || !ordinal)
            return Failure<RawResourceFlowPlan>("plan/read", "wait record is truncated");
        plan.waits.push_back({id.Value(), signal.Value(), {resource.Value()},
                              {consumer.Value()}, {leaf.Value()}, ordinal.Value()});
    }

    auto recoveryVersion = reader.ReadU32();
    auto recoveryLeafCount = reader.ReadU32();
    auto recoveryResourceCount = reader.ReadU32();
    auto reset = reader.ReadU8();
    auto rebind = reader.ReadU8();
    auto reserved = reader.ReadU16();
    if (!recoveryVersion || !recoveryLeafCount || !recoveryResourceCount ||
        !reset || !rebind || !reserved || reserved.Value() != 0 ||
        recoveryLeafCount.Value() > MaximumRecords ||
        recoveryResourceCount.Value() > MaximumRecords)
        return Failure<RawResourceFlowPlan>("plan/read", "recovery header is invalid");
    plan.recovery.schemaVersion = recoveryVersion.Value();
    plan.recovery.resetTemporalState = reset.Value() != 0;
    plan.recovery.requireExternalRebind = rebind.Value() != 0;
    for (std::uint32_t i = 0; i < recoveryLeafCount.Value(); ++i)
    {
        auto leaf = reader.ReadU32();
        if (!leaf) return Failure<RawResourceFlowPlan>("plan/read", "recovery Leaf list is truncated");
        plan.recovery.recreateLeaves.push_back({leaf.Value()});
    }
    for (std::uint32_t i = 0; i < recoveryResourceCount.Value(); ++i)
    {
        auto resource = reader.ReadU32();
        if (!resource) return Failure<RawResourceFlowPlan>("plan/read", "recovery Resource list is truncated");
        plan.recovery.recreateResources.push_back({resource.Value()});
    }

    auto identity = ReadDigest(reader);
    if (!identity) return base::Result<RawResourceFlowPlan, PlanError>::Failure(identity.Error());
    plan.identity = identity.Value();
    if (reader.Remaining() != 0)
        return Failure<RawResourceFlowPlan>("plan/read", "raw plan has trailing bytes");

    auto validation = ValidateRawResourceFlowPlanShape(plan);
    if (!validation)
        return base::Result<RawResourceFlowPlan, PlanError>::Failure(validation.Error());
    return base::Result<RawResourceFlowPlan, PlanError>::Success(std::move(plan));
}
}
