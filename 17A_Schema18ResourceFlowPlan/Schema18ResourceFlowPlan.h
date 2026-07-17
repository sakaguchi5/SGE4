#pragma once

#include "../00_Foundation/Result.h"
#include "../00_Foundation/Sha256.h"
#include "../16A_Schema18CompositionContract/Schema18CompositionContract.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace sge4::composition::schema18::planning
{
using schema18::CompositionEndpointId;
using schema18::LeafPackageId;
using schema18::PackageCompositionContract;
using schema18::ResourceFlowId;
using schema18::StableKey;

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
    package::d3d12_v18::ResourceKind kind = package::d3d12_v18::ResourceKind::Buffer;
    package::d3d12_v18::Format format = package::d3d12_v18::Format::Unknown;
    std::uint64_t sizeBytes = 0;
};

struct LeafScheduleEntry final
{
    std::uint32_t ordinal = package::InvalidIndex;
    LeafPackageId leaf;
};

struct EndpointBindingPlan final
{
    CompositionEndpointId endpoint;
    ResourceFlowId resource;
    LeafPackageId leaf;
    package::d3d12_v18::ExternalSlotId externalSlot;
    package::d3d12_v18::EndpointAccess access = package::d3d12_v18::EndpointAccess::ReadOnly;
};

struct StateHandoffPlan final
{
    ResourceFlowId resource;
    CompositionEndpointId producer;
    CompositionEndpointId consumer;
    LeafPackageId producerLeaf;
    LeafPackageId consumerLeaf;
    package::d3d12_v18::ResourceState producerOutgoingState;
    package::d3d12_v18::ResourceState consumerIncomingState;
};

struct SignalPointPlan final
{
    std::uint32_t id = package::InvalidIndex;
    ResourceFlowId resource;
    CompositionEndpointId producer;
    LeafPackageId producerLeaf;
    std::uint32_t producerScheduleOrdinal = package::InvalidIndex;
};

struct WaitPointPlan final
{
    std::uint32_t id = package::InvalidIndex;
    std::uint32_t signal = package::InvalidIndex;
    ResourceFlowId resource;
    CompositionEndpointId consumer;
    LeafPackageId consumerLeaf;
    std::uint32_t consumerScheduleOrdinal = package::InvalidIndex;
};

struct RecoveryPlan final
{
    std::uint32_t schemaVersion = 1;
    std::vector<LeafPackageId> recreateLeaves;
    std::vector<ResourceFlowId> recreateResources;
    bool resetTemporalState = true;
    bool requireExternalRebind = true;
};

struct RawResourceFlowPlan final
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

[[nodiscard]] base::Result<RawResourceFlowPlan, PlanError>
ProposeResourceFlowPlan(const PackageCompositionContract& contract);

// This only checks the canonical data shape and self-identity of a raw proposal.
// Authority against a Composition Contract belongs exclusively to F5.
[[nodiscard]] base::Result<void, PlanError>
ValidateRawResourceFlowPlanShape(const RawResourceFlowPlan& plan);

[[nodiscard]] std::vector<std::byte>
SerializeRawResourceFlowPlan(const RawResourceFlowPlan& plan);

[[nodiscard]] base::Result<RawResourceFlowPlan, PlanError>
DeserializeRawResourceFlowPlan(std::span<const std::byte> bytes);

[[nodiscard]] base::Digest256
ComputeRawResourceFlowPlanIdentity(const RawResourceFlowPlan& plan);
}
