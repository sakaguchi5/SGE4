#include "LinkPlanModel.h"

#include "../00_Foundation/BinaryIO.h"

namespace sge4::composition
{
namespace
{
void State(base::BinaryWriter& writer, const package::d3d12_v13::ResourceState& value)
{
    writer.WriteU16(static_cast<std::uint16_t>(value.stateClass)); writer.WriteU16(value.reserved); writer.WriteU32(value.explicitBits);
}
}
base::Digest256 ComputeLinkPlanIdentity(const PackageCompositionContract& contract, const LinkPlanIR& plan)
{
    base::BinaryWriter writer;
    writer.WriteU32(1); writer.WriteBytes(contract.identity); writer.WriteBytes(plan.contractIdentity);
    writer.WriteU32(static_cast<std::uint32_t>(plan.resources.size()));
    writer.WriteU32(static_cast<std::uint32_t>(plan.bindings.size()));
    writer.WriteU32(static_cast<std::uint32_t>(plan.signalWaits.size()));
    writer.WriteU32(static_cast<std::uint32_t>(plan.schedule.size()));
    writer.WriteU8(static_cast<std::uint8_t>(plan.recovery.policy)); writer.WriteZeroes(3);
    for (const auto& value : plan.resources) { writer.WriteU32(value.resource.value); writer.WriteU8(static_cast<std::uint8_t>(value.owner)); writer.WriteZeroes(3); writer.WriteU64(value.allocationBytes); }
    for (const auto& value : plan.bindings) { writer.WriteU32(value.endpoint.value); writer.WriteU32(value.resource.value); }
    for (const auto& value : plan.signalWaits) { writer.WriteU32(value.link.value); writer.WriteU32(value.signalEndpoint.value); writer.WriteU32(value.waitEndpoint.value); State(writer, value.handoffState); }
    for (const auto& value : plan.schedule) { writer.WriteU32(value.ordinal); writer.WriteU32(value.leaf.value); }
    return base::Sha256(writer.Bytes());
}
}
