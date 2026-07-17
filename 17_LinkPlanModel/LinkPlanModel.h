#pragma once

#include "../16_CompositionContract/CompositionContract.h"

#include <cstdint>
#include <vector>

namespace sge4::composition
{
enum class CompositionRecoveryPolicy : std::uint8_t { RebuildWholeDeviceDomain = 1 };

struct SharedResourcePlan final
{
    SharedResourceId resource;
    SharedResourceOwnership owner = SharedResourceOwnership::DeviceDomain;
    std::uint64_t allocationBytes = 0;
};
struct EndpointBindingPlan final { CompositionEndpointId endpoint; SharedResourceId resource; };
struct CrossPackageSignalWait final
{
    CompositionLinkId link;
    CompositionEndpointId signalEndpoint;
    CompositionEndpointId waitEndpoint;
    package::d3d12_v13::ResourceState handoffState;
};
struct PackageScheduleEntry final { std::uint32_t ordinal = 0; LeafPackageId leaf; };
struct CompositionRecoveryPlan final { CompositionRecoveryPolicy policy = CompositionRecoveryPolicy::RebuildWholeDeviceDomain; };

struct LinkPlanIR final
{
    base::Digest256 contractIdentity{};
    std::vector<SharedResourcePlan> resources;
    std::vector<EndpointBindingPlan> bindings;
    std::vector<CrossPackageSignalWait> signalWaits;
    std::vector<PackageScheduleEntry> schedule;
    CompositionRecoveryPlan recovery;
    base::Digest256 identity{};
};

[[nodiscard]] base::Digest256 ComputeLinkPlanIdentity(const PackageCompositionContract& contract, const LinkPlanIR& plan);
}
