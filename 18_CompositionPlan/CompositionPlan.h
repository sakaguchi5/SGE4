#pragma once

#include "../00_Foundation/Result.h"
#include "../00_Foundation/Sha256.h"
#include "../17_CompositionContract/CompositionContract.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace sge4::composition::canonical::planning
{
struct PlanError final
{
    std::string stage;
    std::string message;
};

enum class AllocationOwnership : std::uint16_t
{
    CompositionOwned = 1,
    ExternalInput = 2,
    ExternalOutput = 3
};

struct ResourceAllocationPlan final
{
    ResourceFlowId resource;
    AllocationOwnership ownership = AllocationOwnership::CompositionOwned;
    package::d3d12_v13::ResourceKind kind = package::d3d12_v13::ResourceKind::Buffer;
    package::d3d12_v13::Format format = package::d3d12_v13::Format::Unknown;
    std::uint64_t sizeBytes = 0;
};

struct LeafScheduleEntry final
{
    std::uint32_t ordinal = InvalidIndex;
    LeafPackageId leaf;
};

struct EndpointBindingPlan final
{
    CompositionEndpointId endpoint;
    ResourceFlowId resource;
    LeafPackageId leaf;
    std::uint32_t externalSlot = InvalidIndex;
    EndpointAccess access = EndpointAccess::ReadOnly;
};

struct StateHandoffPlan final
{
    ResourceFlowId resource;
    CompositionEndpointId producer;
    CompositionEndpointId consumer;
    LeafPackageId producerLeaf;
    LeafPackageId consumerLeaf;
    package::d3d12_v13::ResourceState producerOutgoingState;
    package::d3d12_v13::ResourceState consumerIncomingState;
};

struct SignalPointPlan final
{
    std::uint32_t id = InvalidIndex;
    ResourceFlowId resource;
    CompositionEndpointId producer;
    LeafPackageId producerLeaf;
    std::uint32_t producerScheduleOrdinal = InvalidIndex;
};

struct WaitPointPlan final
{
    std::uint32_t id = InvalidIndex;
    std::uint32_t signal = InvalidIndex;
    ResourceFlowId resource;
    CompositionEndpointId consumer;
    LeafPackageId consumerLeaf;
    std::uint32_t consumerScheduleOrdinal = InvalidIndex;
};

struct RecoveryPlan final
{
    std::uint32_t schemaVersion = 1;
    std::vector<LeafPackageId> recreateLeaves;
    std::vector<ResourceFlowId> recreateResources;
    bool resetTemporalState = true;
    bool requireExternalRebind = true;
};

struct RawCompositionPlan final
{
    base::Digest256 contractIdentity{};
    std::vector<ResourceAllocationPlan> allocations;
    std::vector<LeafScheduleEntry> schedule;
    std::vector<EndpointBindingPlan> bindings;
    std::vector<StateHandoffPlan> handoffs;
    std::vector<SignalPointPlan> signals;
    std::vector<WaitPointPlan> waits;
    RecoveryPlan recovery;
    base::Digest256 identity{};
};

[[nodiscard]] base::Result<RawCompositionPlan, PlanError>
ProposeCompositionPlan(const ValidatedCompositionContract& validatedContract);

// Shape and self-identity only. This does not grant execution or freeze authority.
[[nodiscard]] base::Result<void, PlanError>
ValidateRawCompositionPlanShape(const RawCompositionPlan& plan);

[[nodiscard]] std::vector<std::byte>
SerializeRawCompositionPlan(const RawCompositionPlan& plan);

[[nodiscard]] base::Result<RawCompositionPlan, PlanError>
DeserializeRawCompositionPlan(std::span<const std::byte> bytes);

[[nodiscard]] base::Digest256
ComputeRawCompositionPlanIdentity(const RawCompositionPlan& plan);
}
