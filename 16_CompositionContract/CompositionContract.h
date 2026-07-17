#pragma once

#include "../00_Foundation/Result.h"
#include "../00_Foundation/Sha256.h"
#include "../00_Foundation/StrongId.h"
#include "../10_D3D12PackageSchema/D3D12Schema.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace sge4::composition
{
template<class Tag> using Id32 = base::Id32<Tag>;
struct LeafPackageTag; struct SharedResourceTag; struct EndpointTag; struct LinkTag;
using LeafPackageId = Id32<LeafPackageTag>;
using SharedResourceId = Id32<SharedResourceTag>;
using CompositionEndpointId = Id32<EndpointTag>;
using CompositionLinkId = Id32<LinkTag>;

enum class EndpointDirection : std::uint8_t { Import = 1, Export = 2, ImportExport = 3 };
enum class SharedResourceBoundary : std::uint8_t { Internal = 1, CompositionInput = 2, CompositionOutput = 3 };
enum class SharedResourceOwnership : std::uint8_t { DeviceDomain = 1 };

struct ContractError final
{
    std::string stage;
    std::string message;
};

struct ExternalSlotContract final
{
    std::uint32_t slot = package::InvalidIndex;
    package::d3d12_v13::ResourceKind kind = package::d3d12_v13::ResourceKind::Buffer;
    package::d3d12_v13::Format format = package::d3d12_v13::Format::Unknown;
    std::uint64_t minimumBytes = 0;
    package::d3d12_v13::ResourceState incomingState;
    package::d3d12_v13::ResourceState outgoingState;
    package::d3d12_v13::ExternalSynchronizationContract synchronization =
        package::d3d12_v13::ExternalSynchronizationContract::CompletionTokenRequired;
};

struct LeafPackageContract final
{
    LeafPackageId id;
    std::uint64_t authorKey = 0;
    std::uint32_t targetKind = 0;
    std::uint32_t schemaVersion = 0;
    std::uint32_t minimumRuntimeVersion = 0;
    base::Digest256 executionDigest{};
    base::Digest256 fileDigest{};
    base::Digest256 targetProfileDigest{};
    std::uint32_t surfaceSlotCount = 0;
    std::vector<ExternalSlotContract> externalSlots;
    std::vector<std::byte> packageBytes;
};

struct CompositionEndpoint final
{
    CompositionEndpointId id;
    LeafPackageId leaf;
    std::uint32_t externalSlot = package::InvalidIndex;
    EndpointDirection direction = EndpointDirection::Import;
    ExternalSlotContract contract;
};

struct SharedResourceContract final
{
    SharedResourceId id;
    std::uint64_t authorKey = 0;
    package::d3d12_v13::ResourceKind kind = package::d3d12_v13::ResourceKind::Buffer;
    package::d3d12_v13::Format format = package::d3d12_v13::Format::Unknown;
    std::uint64_t sizeBytes = 0;
    package::d3d12_v13::ResourceState initialState;
    SharedResourceBoundary boundary = SharedResourceBoundary::Internal;
    SharedResourceOwnership ownership = SharedResourceOwnership::DeviceDomain;
};

struct EndpointBindingContract final
{
    SharedResourceId resource;
    CompositionEndpointId endpoint;
};

struct CompositionLinkContract final
{
    CompositionLinkId id;
    SharedResourceId resource;
    CompositionEndpointId producer;
    CompositionEndpointId consumer;
    package::d3d12_v13::ResourceState handoffState;
};

struct PackageCompositionContract final
{
    std::vector<LeafPackageContract> leaves;
    std::vector<SharedResourceContract> resources;
    std::vector<CompositionEndpoint> endpoints;
    std::vector<EndpointBindingContract> bindings;
    std::vector<CompositionLinkContract> links;
    base::Digest256 identity{};
};

struct LeafPackageDeclaration final { std::uint64_t authorKey = 0; std::vector<std::byte> packageBytes; };
struct SharedResourceDeclaration final
{
    std::uint64_t authorKey = 0;
    std::uint64_t sizeBytes = 0;
    package::d3d12_v13::Format format = package::d3d12_v13::Format::Unknown;
    package::d3d12_v13::ResourceState initialState;
    SharedResourceBoundary boundary = SharedResourceBoundary::Internal;
};
struct EndpointDeclaration final
{
    std::uint64_t leafAuthorKey = 0;
    std::uint32_t externalSlot = package::InvalidIndex;
    EndpointDirection direction = EndpointDirection::Import;
};
struct EndpointBindingDeclaration final
{
    std::uint64_t resourceAuthorKey = 0;
    std::uint64_t leafAuthorKey = 0;
    std::uint32_t externalSlot = package::InvalidIndex;
};
struct LinkDeclaration final
{
    std::uint64_t resourceAuthorKey = 0;
    std::uint64_t producerLeafAuthorKey = 0;
    std::uint32_t producerSlot = package::InvalidIndex;
    std::uint64_t consumerLeafAuthorKey = 0;
    std::uint32_t consumerSlot = package::InvalidIndex;
};
struct ContractBuildInput final
{
    std::vector<LeafPackageDeclaration> leaves;
    std::vector<SharedResourceDeclaration> resources;
    std::vector<EndpointDeclaration> endpoints;
    std::vector<EndpointBindingDeclaration> bindings;
    std::vector<LinkDeclaration> links;
};

[[nodiscard]] base::Digest256 ComputeContractIdentity(const PackageCompositionContract& contract);
[[nodiscard]] base::Result<PackageCompositionContract, ContractError> BuildCompositionContract(ContractBuildInput input);
}
