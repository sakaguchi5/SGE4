#pragma once

#include "../00_Foundation/Result.h"
#include "../00_Foundation/Sha256.h"
#include "../00_Foundation/StrongId.h"
#include "../10A_D3D12CompositionSchema18/D3D12CompositionSchema18.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace sge4::composition::schema18
{
template<class Tag> using Id32 = base::Id32<Tag>;
struct LeafPackageTag;
struct EndpointTag;
struct ResourceFlowTag;
using LeafPackageId = Id32<LeafPackageTag>;
using CompositionEndpointId = Id32<EndpointTag>;
using ResourceFlowId = Id32<ResourceFlowTag>;
using StableKey = base::Digest256;

enum class ResourceBoundary : std::uint16_t
{
    Internal = 1,
    CompositionInput = 2,
    CompositionOutput = 3
};

struct ContractError final
{
    std::string stage;
    std::string message;
};

struct LeafPackageContract final
{
    LeafPackageId id;
    StableKey stableKey{};
    std::uint32_t targetKind = 0;
    std::uint32_t schemaVersion = 0;
    std::uint32_t minimumRuntimeVersion = 0;
    base::Digest256 executionDigest{};
    base::Digest256 fileDigest{};
    base::Digest256 targetProfileDigest{};
    std::uint32_t endpointBegin = 0;
    std::uint32_t endpointCount = 0;
    std::uint32_t surfaceSlotCount = 0;
    std::vector<std::byte> packageBytes;
};

struct CompositionEndpointContract final
{
    CompositionEndpointId id;
    LeafPackageId leaf;
    std::uint32_t localEndpoint = package::InvalidIndex;
    package::d3d12_v18::StableEndpointKey stableKey{};
    package::d3d12_v18::ExternalSlotId externalSlot;
    package::d3d12_v18::ResourceId resource;
    package::d3d12_v18::ResourceKind kind = package::d3d12_v18::ResourceKind::Buffer;
    package::d3d12_v18::EndpointAccess access = package::d3d12_v18::EndpointAccess::ReadOnly;
    package::d3d12_v18::Format format = package::d3d12_v18::Format::Unknown;
    std::uint64_t minimumBytes = 0;
    package::d3d12_v18::ResourceState requiredIncomingState;
    package::d3d12_v18::ResourceState guaranteedOutgoingState;
    package::d3d12_v18::ExternalSynchronizationContract synchronization =
        package::d3d12_v18::ExternalSynchronizationContract::CompletionTokenRequired;
    package::d3d12_v18::EndpointFrameSemantics frameSemantics =
        package::d3d12_v18::EndpointFrameSemantics::PerFrame;
    std::uint32_t flags = static_cast<std::uint32_t>(package::d3d12_v18::EndpointFlags::Required);
};

struct ResourceFlowContract final
{
    ResourceFlowId id;
    StableKey stableKey{};
    ResourceBoundary boundary = ResourceBoundary::Internal;
    package::d3d12_v18::ResourceKind kind = package::d3d12_v18::ResourceKind::Buffer;
    package::d3d12_v18::Format format = package::d3d12_v18::Format::Unknown;
    std::uint64_t sizeBytes = 0;
    CompositionEndpointId producer;
    std::vector<CompositionEndpointId> consumers;
};

struct EndpointBindingContract final
{
    CompositionEndpointId endpoint;
    ResourceFlowId resource;
};

struct PackageCompositionContract final
{
    std::vector<LeafPackageContract> leaves;
    std::vector<CompositionEndpointContract> endpoints;
    std::vector<ResourceFlowContract> resources;
    std::vector<EndpointBindingContract> bindings;
    LeafPackageId presenterLeaf;
    base::Digest256 identity{};
};

struct LeafPackageDeclaration final
{
    std::string stableKey;
    std::vector<std::byte> packageBytes;
};

struct EndpointReferenceDeclaration final
{
    std::string leafKey;
    std::string endpointKey;
};

struct ResourceFlowDeclaration final
{
    std::string stableKey;
    ResourceBoundary boundary = ResourceBoundary::Internal;
    std::optional<EndpointReferenceDeclaration> producer;
    std::vector<EndpointReferenceDeclaration> consumers;
};

struct ContractBuildInput final
{
    std::vector<LeafPackageDeclaration> leaves;
    std::vector<ResourceFlowDeclaration> resources;
};

[[nodiscard]] StableKey ComputeStableLeafKey(std::string_view authorKey);
[[nodiscard]] StableKey ComputeStableResourceKey(std::string_view authorKey);
[[nodiscard]] base::Result<PackageCompositionContract, ContractError>
BuildSchema18CompositionContract(ContractBuildInput input);
[[nodiscard]] base::Result<void, ContractError>
ValidateCompositionContract(const PackageCompositionContract& contract);
[[nodiscard]] std::vector<std::byte>
SerializeCompositionContract(const PackageCompositionContract& contract);
[[nodiscard]] base::Digest256
ComputeContractIdentity(const PackageCompositionContract& contract);
}
