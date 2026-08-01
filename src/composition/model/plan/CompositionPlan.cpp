#include "./CompositionPlan.h"

#include "../../../canonical/base/BinaryIO.h"

#include <algorithm>
#include <map>
#include <set>
#include <tuple>
#include <utility>

namespace sge4::composition::planning
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
base::Expected<T, PlanError> Failure(std::string stage, std::string message)
{
    return base::Failure<T, PlanError>(
        Error(std::move(stage), std::move(message)));
}

void WriteDigest(base::BinaryWriter& writer, const base::Digest256& value)
{
    writer.WriteBytes(value);
}

void WriteState(base::BinaryWriter& writer,
                const package::d3d12_v13::ResourceState& value)
{
    writer.WriteU16(static_cast<std::uint16_t>(value.stateClass));
    writer.WriteU16(value.reserved);
    writer.WriteU32(value.explicitBits);
}

base::Expected<base::Digest256, PlanError> ReadDigest(base::BinaryReader& reader)
{
    auto bytes = reader.ReadBytes(base::Digest256{}.size());
    if (!bytes) return Failure<base::Digest256>("plan/read", bytes.error());
    base::Digest256 result{};
    std::copy(bytes.value().begin(), bytes.value().end(), result.begin());
    return base::Success<base::Digest256, PlanError>(result);
}

base::Expected<package::d3d12_v13::ResourceState, PlanError>
ReadState(base::BinaryReader& reader)
{
    auto stateClass = reader.ReadU16();
    auto reserved = reader.ReadU16();
    auto bits = reader.ReadU32();
    if (!stateClass || !reserved || !bits)
        return Failure<package::d3d12_v13::ResourceState>(
            "plan/read", "検証または実行の契約に違反しています。");
    package::d3d12_v13::ResourceState state;
    state.stateClass = static_cast<package::d3d12_v13::StateClass>(stateClass.value());
    state.reserved = reserved.value();
    state.explicitBits = bits.value();
    return base::Success<package::d3d12_v13::ResourceState, PlanError>(state);
}

void WriteTexture2DShape(base::BinaryWriter& writer, const Texture2DFlowShape& value)
{
    writer.WriteU32(value.width);
    writer.WriteU32(value.height);
    writer.WriteU32(value.rowBytes);
    writer.WriteU16(value.mipLevels);
    writer.WriteU16(value.arrayLayers);
    writer.WriteU16(value.sampleCount);
    writer.WriteU16(value.planeCount);
}

base::Expected<Texture2DFlowShape, PlanError> ReadTexture2DShape(base::BinaryReader& reader)
{
    auto width = reader.ReadU32();
    auto height = reader.ReadU32();
    auto rowBytes = reader.ReadU32();
    auto mipLevels = reader.ReadU16();
    auto arrayLayers = reader.ReadU16();
    auto sampleCount = reader.ReadU16();
    auto planeCount = reader.ReadU16();
    if (!width || !height || !rowBytes || !mipLevels || !arrayLayers || !sampleCount || !planeCount)
        return Failure<Texture2DFlowShape>("plan/read", "Textureが検証または実行の契約に違反しています。");
    Texture2DFlowShape result;
    result.width = width.value(); result.height = height.value(); result.rowBytes = rowBytes.value();
    result.mipLevels = mipLevels.value(); result.arrayLayers = arrayLayers.value();
    result.sampleCount = sampleCount.value(); result.planeCount = planeCount.value();
    return base::Success<Texture2DFlowShape, PlanError>(result);
}

bool ValidAllocationShape(const ResourceAllocationPlan& value) noexcept
{
    if (value.kind == package::d3d12_v13::ResourceKind::Buffer)
        return value.sizeBytes > 0 && value.texture2D == Texture2DFlowShape{} &&
            value.format == package::d3d12_v13::Format::Unknown;
    if (value.kind == package::d3d12_v13::ResourceKind::Texture2D)
    {
        const std::uint32_t bytesPerPixel =
            value.format == package::d3d12_v13::Format::B8G8R8A8Unorm ? 4u :
            value.format == package::d3d12_v13::Format::R32G32B32A32Float ? 16u : 0u;
        return bytesPerPixel != 0 &&
            value.texture2D.width > 0 && value.texture2D.height > 0 &&
            static_cast<std::uint64_t>(value.texture2D.rowBytes) ==
                static_cast<std::uint64_t>(value.texture2D.width) * bytesPerPixel &&
            value.texture2D.mipLevels == 1 && value.texture2D.arrayLayers == 1 &&
            value.texture2D.sampleCount == 1 && value.texture2D.planeCount == 1 &&
            value.sizeBytes == static_cast<std::uint64_t>(value.texture2D.rowBytes) * value.texture2D.height;
    }
    return false;
}

std::vector<std::byte> SerializeBody(const RawCompositionPlan& plan)
{
    base::BinaryWriter writer;
    writer.WriteU32(PlanMagic);
    writer.WriteU32(PlanVersion);
    WriteDigest(writer, plan.contractIdentity);
    writer.WriteCountU32(plan.allocations.size());
    writer.WriteCountU32(plan.schedule.size());
    writer.WriteCountU32(plan.bindings.size());
    writer.WriteCountU32(plan.handoffs.size());
    writer.WriteCountU32(plan.signals.size());
    writer.WriteCountU32(plan.waits.size());

    for (const auto& allocation : plan.allocations)
    {
        writer.WriteU32(allocation.resource.value);
        writer.WriteU16(static_cast<std::uint16_t>(allocation.ownership));
        writer.WriteU16(static_cast<std::uint16_t>(allocation.kind));
        writer.WriteU32(static_cast<std::uint32_t>(allocation.format));
        writer.WriteU64(allocation.sizeBytes);
        if (allocation.kind == package::d3d12_v13::ResourceKind::Texture2D)
            WriteTexture2DShape(writer, allocation.texture2D);
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
        writer.WriteU32(binding.externalSlot);
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
    writer.WriteCountU32(plan.recovery.recreateLeaves.size());
    writer.WriteCountU32(plan.recovery.recreateResources.size());
    writer.WriteU8(plan.recovery.resetTemporalState ? 1 : 0);
    writer.WriteU8(plan.recovery.requireExternalRebind ? 1 : 0);
    writer.WriteU16(0);
    for (const auto leaf : plan.recovery.recreateLeaves) writer.WriteU32(leaf.value);
    for (const auto resource : plan.recovery.recreateResources) writer.WriteU32(resource.value);
    return std::move(writer).Take();
}

}

base::Expected<void, PlanError>
ValidateRawCompositionPlanShape(const RawCompositionPlan& plan)
{
    if (plan.contractIdentity == base::Digest256{})
        return base::Failure<void, PlanError>(
            Error("plan/shape", "検証または実行の契約に違反しています。"));

    for (std::uint32_t index = 0; index < plan.allocations.size(); ++index)
    {
        const auto& value = plan.allocations[index];
        if (value.resource.value != index || !ValidAllocationShape(value) ||
            (value.ownership != AllocationOwnership::CompositionOwned &&
             value.ownership != AllocationOwnership::ExternalInput &&
             value.ownership != AllocationOwnership::ExternalOutput))
            return base::Failure<void, PlanError>(
                Error("plan/shape-allocation", "検証または実行の契約に違反しています。"));
    }

    std::set<std::uint32_t> leaves;
    for (std::uint32_t index = 0; index < plan.schedule.size(); ++index)
    {
        const auto& entry = plan.schedule[index];
        if (entry.ordinal != index || !entry.leaf.IsValid() ||
            !leaves.insert(entry.leaf.value).second)
            return base::Failure<void, PlanError>(
                Error("plan/shape-schedule", "検証または実行の契約に違反しています。"));
    }

    for (std::uint32_t index = 0; index < plan.bindings.size(); ++index)
    {
        const auto& binding = plan.bindings[index];
        if (binding.endpoint.value != index || !binding.resource.IsValid() ||
            !binding.leaf.IsValid() || binding.externalSlot == InvalidIndex ||
            (binding.access != EndpointAccess::ReadOnly &&
             binding.access != EndpointAccess::WriteOnly))
            return base::Failure<void, PlanError>(
                Error("plan/shape-binding", "検証または実行の契約に違反しています。"));
    }

    if (!std::is_sorted(plan.handoffs.begin(), plan.handoffs.end(), [](const auto& left, const auto& right) {
            return std::tie(left.resource.value, left.consumer.value) <
                   std::tie(right.resource.value, right.consumer.value);
        }))
        return base::Failure<void, PlanError>(
            Error("plan/shape-handoff", "入力または内部状態がCanonicalな順序または識別子規則に違反しています。"));

    for (std::uint32_t index = 0; index < plan.signals.size(); ++index)
        if (plan.signals[index].id != index)
            return base::Failure<void, PlanError>(
                Error("plan/shape-signal", "検証または実行の契約に違反しています。"));
    for (std::uint32_t index = 0; index < plan.waits.size(); ++index)
        if (plan.waits[index].id != index || plan.waits[index].signal >= plan.signals.size())
            return base::Failure<void, PlanError>(
                Error("plan/shape-wait", "Signalが検証または実行の契約に違反しています。"));

    if (plan.recovery.schemaVersion != 1 ||
        !plan.recovery.resetTemporalState ||
        !plan.recovery.requireExternalRebind ||
        !std::is_sorted(plan.recovery.recreateLeaves.begin(),
                        plan.recovery.recreateLeaves.end(),
                        [](auto left, auto right) { return left.value < right.value; }) ||
        !std::is_sorted(plan.recovery.recreateResources.begin(),
                        plan.recovery.recreateResources.end(),
                        [](auto left, auto right) { return left.value < right.value; }))
        return base::Failure<void, PlanError>(
            Error("plan/shape-recovery", "入力または内部状態がCanonicalな順序または識別子規則に違反しています。"));

    if (plan.identity != ComputeRawCompositionPlanIdentity(plan))
        return base::Failure<void, PlanError>(
            Error("plan/shape-identity", "Planが検証または実行の契約に違反しています。"));
    return base::Success<void, PlanError>();
}

std::vector<std::byte>
SerializeRawCompositionPlan(const RawCompositionPlan& plan)
{
    auto bytes = SerializeBody(plan);
    bytes.insert(bytes.end(), plan.identity.begin(), plan.identity.end());
    return bytes;
}

base::Digest256
ComputeRawCompositionPlanIdentity(const RawCompositionPlan& plan)
{
    const auto body = SerializeBody(plan);
    return base::Sha256(body);
}

base::Expected<RawCompositionPlan, PlanError>
DeserializeRawCompositionPlan(std::span<const std::byte> bytes)
{
    if (bytes.size() < 8 + base::Digest256{}.size() * 2)
        return Failure<RawCompositionPlan>("plan/read", "検証または実行の契約に違反しています。");
    base::BinaryReader reader(bytes);
    auto magic = reader.ReadU32();
    auto version = reader.ReadU32();
    if (!magic || !version || magic.value() != PlanMagic || version.value() != PlanVersion)
        return Failure<RawCompositionPlan>("plan/read", "Planが検証または実行の契約に違反しています。");

    RawCompositionPlan plan;
    auto contractIdentity = ReadDigest(reader);
    if (!contractIdentity) return base::Failure<RawCompositionPlan, PlanError>(contractIdentity.error());
    plan.contractIdentity = contractIdentity.value();

    auto allocationCount = reader.ReadU32();
    auto scheduleCount = reader.ReadU32();
    auto bindingCount = reader.ReadU32();
    auto handoffCount = reader.ReadU32();
    auto signalCount = reader.ReadU32();
    auto waitCount = reader.ReadU32();
    if (!allocationCount || !scheduleCount || !bindingCount || !handoffCount ||
        !signalCount || !waitCount)
        return Failure<RawCompositionPlan>("plan/read", "Planが検証または実行の契約に違反しています。");

    constexpr std::uint32_t MaximumRecords = 1'000'000;
    if (allocationCount.value() > MaximumRecords || scheduleCount.value() > MaximumRecords ||
        bindingCount.value() > MaximumRecords || handoffCount.value() > MaximumRecords ||
        signalCount.value() > MaximumRecords || waitCount.value() > MaximumRecords)
        return Failure<RawCompositionPlan>("plan/read", "Planが検証または実行の契約に違反しています。");

    for (std::uint32_t i = 0; i < allocationCount.value(); ++i)
    {
        auto resource = reader.ReadU32();
        auto ownership = reader.ReadU16();
        auto kind = reader.ReadU16();
        auto format = reader.ReadU32();
        auto size = reader.ReadU64();
        base::Expected<Texture2DFlowShape, PlanError> texture2D =
            base::Success<Texture2DFlowShape, PlanError>({});
        if (kind && static_cast<package::d3d12_v13::ResourceKind>(kind.value()) ==
                package::d3d12_v13::ResourceKind::Texture2D)
            texture2D = ReadTexture2DShape(reader);
        if (!resource || !ownership || !kind || !format || !size || !texture2D)
            return Failure<RawCompositionPlan>("plan/read", "Allocationが検証または実行の契約に違反しています。");
        plan.allocations.push_back({
            {resource.value()}, static_cast<AllocationOwnership>(ownership.value()),
            static_cast<package::d3d12_v13::ResourceKind>(kind.value()),
            static_cast<package::d3d12_v13::Format>(format.value()), size.value(),
            texture2D.value()});
    }
    for (std::uint32_t i = 0; i < scheduleCount.value(); ++i)
    {
        auto ordinal = reader.ReadU32();
        auto leaf = reader.ReadU32();
        if (!ordinal || !leaf)
            return Failure<RawCompositionPlan>("plan/read", "Scheduleが検証または実行の契約に違反しています。");
        plan.schedule.push_back({ordinal.value(), {leaf.value()}});
    }
    for (std::uint32_t i = 0; i < bindingCount.value(); ++i)
    {
        auto endpoint = reader.ReadU32();
        auto resource = reader.ReadU32();
        auto leaf = reader.ReadU32();
        auto slot = reader.ReadU32();
        auto access = reader.ReadU16();
        auto reserved = reader.ReadU16();
        if (!endpoint || !resource || !leaf || !slot || !access || !reserved || reserved.value() != 0)
            return Failure<RawCompositionPlan>("plan/read", "検証または実行の契約に違反しています。");
        plan.bindings.push_back({
            {endpoint.value()}, {resource.value()}, {leaf.value()}, slot.value(),
            static_cast<EndpointAccess>(access.value())});
    }
    for (std::uint32_t i = 0; i < handoffCount.value(); ++i)
    {
        auto resource = reader.ReadU32();
        auto producer = reader.ReadU32();
        auto consumer = reader.ReadU32();
        auto producerLeaf = reader.ReadU32();
        auto consumerLeaf = reader.ReadU32();
        auto outgoing = ReadState(reader);
        auto incoming = ReadState(reader);
        if (!resource || !producer || !consumer || !producerLeaf || !consumerLeaf || !outgoing || !incoming)
            return Failure<RawCompositionPlan>("plan/read", "入力または内部状態が検証または実行の契約に違反しています。");
        plan.handoffs.push_back({
            {resource.value()}, {producer.value()}, {consumer.value()},
            {producerLeaf.value()}, {consumerLeaf.value()}, outgoing.value(), incoming.value()});
    }
    for (std::uint32_t i = 0; i < signalCount.value(); ++i)
    {
        auto id = reader.ReadU32();
        auto resource = reader.ReadU32();
        auto producer = reader.ReadU32();
        auto leaf = reader.ReadU32();
        auto ordinal = reader.ReadU32();
        if (!id || !resource || !producer || !leaf || !ordinal)
            return Failure<RawCompositionPlan>("plan/read", "Signalが検証または実行の契約に違反しています。");
        plan.signals.push_back({id.value(), {resource.value()}, {producer.value()},
                                {leaf.value()}, ordinal.value()});
    }
    for (std::uint32_t i = 0; i < waitCount.value(); ++i)
    {
        auto id = reader.ReadU32();
        auto signal = reader.ReadU32();
        auto resource = reader.ReadU32();
        auto consumer = reader.ReadU32();
        auto leaf = reader.ReadU32();
        auto ordinal = reader.ReadU32();
        if (!id || !signal || !resource || !consumer || !leaf || !ordinal)
            return Failure<RawCompositionPlan>("plan/read", "Waitが検証または実行の契約に違反しています。");
        plan.waits.push_back({id.value(), signal.value(), {resource.value()},
                              {consumer.value()}, {leaf.value()}, ordinal.value()});
    }

    auto recoveryVersion = reader.ReadU32();
    auto recoveryLeafCount = reader.ReadU32();
    auto recoveryResourceCount = reader.ReadU32();
    auto reset = reader.ReadU8();
    auto rebind = reader.ReadU8();
    auto reserved = reader.ReadU16();
    if (!recoveryVersion || !recoveryLeafCount || !recoveryResourceCount ||
        !reset || !rebind || !reserved || reserved.value() != 0 ||
        recoveryLeafCount.value() > MaximumRecords ||
        recoveryResourceCount.value() > MaximumRecords)
        return Failure<RawCompositionPlan>("plan/read", "Headerが検証または実行の契約に違反しています。");
    plan.recovery.schemaVersion = recoveryVersion.value();
    plan.recovery.resetTemporalState = reset.value() != 0;
    plan.recovery.requireExternalRebind = rebind.value() != 0;
    for (std::uint32_t i = 0; i < recoveryLeafCount.value(); ++i)
    {
        auto leaf = reader.ReadU32();
        if (!leaf) return Failure<RawCompositionPlan>("plan/read", "Leafが検証または実行の契約に違反しています。");
        plan.recovery.recreateLeaves.push_back({leaf.value()});
    }
    for (std::uint32_t i = 0; i < recoveryResourceCount.value(); ++i)
    {
        auto resource = reader.ReadU32();
        if (!resource) return Failure<RawCompositionPlan>("plan/read", "Resourceが検証または実行の契約に違反しています。");
        plan.recovery.recreateResources.push_back({resource.value()});
    }

    auto identity = ReadDigest(reader);
    if (!identity) return base::Failure<RawCompositionPlan, PlanError>(identity.error());
    plan.identity = identity.value();
    if (reader.Remaining() != 0)
        return Failure<RawCompositionPlan>("plan/read", "Planが検証または実行の契約に違反しています。");

    auto validation = ValidateRawCompositionPlanShape(plan);
    if (!validation)
        return base::Failure<RawCompositionPlan, PlanError>(validation.error());
    return base::Success<RawCompositionPlan, PlanError>(std::move(plan));
}
}
