#pragma once

#include "../00_Foundation/Result.h"
#include "../00_Foundation/Sha256.h"
#include "../00_Foundation/StrongId.h"
#include "../10_D3D12PackageSchema/D3D12Schema.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sge4::composition::canonical
{
template<class Tag> using Id32 = base::Id32<Tag>;
struct LeafPackageTag;
struct EndpointTag;
struct ResourceFlowTag;
using LeafPackageId = Id32<LeafPackageTag>;
using CompositionEndpointId = Id32<EndpointTag>;
using ResourceFlowId = Id32<ResourceFlowTag>;
using StableKey = base::Digest256;

inline constexpr std::uint32_t InvalidIndex = base::InvalidIndex;
inline constexpr std::uint32_t CompositionContractVersion = 1;

enum class EndpointAccess : std::uint16_t
{
    ReadOnly = 1,
    WriteOnly = 2
};

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
};

struct CompositionEndpointContract final
{
    CompositionEndpointId id;
    LeafPackageId leaf;
    std::uint32_t localExternalSlot = InvalidIndex;
    StableKey stableKey{};
    package::d3d12_v13::ResourceId resource;
    package::d3d12_v13::ResourceKind kind = package::d3d12_v13::ResourceKind::Buffer;
    EndpointAccess access = EndpointAccess::ReadOnly;
    package::d3d12_v13::Format format = package::d3d12_v13::Format::Unknown;
    std::uint64_t minimumBytes = 0;
    package::d3d12_v13::ResourceState requiredIncomingState;
    package::d3d12_v13::ResourceState guaranteedOutgoingState;
    package::d3d12_v13::ExternalSynchronizationContract synchronization =
        package::d3d12_v13::ExternalSynchronizationContract::CompletionTokenRequired;
    std::uint32_t flags = static_cast<std::uint32_t>(
        package::d3d12_v13::ExternalSlotFlags::Required);
};

struct ResourceFlowContract final
{
    ResourceFlowId id;
    StableKey stableKey{};
    ResourceBoundary boundary = ResourceBoundary::Internal;
    package::d3d12_v13::ResourceKind kind = package::d3d12_v13::ResourceKind::Buffer;
    package::d3d12_v13::Format format = package::d3d12_v13::Format::Unknown;
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

struct LeafEndpointDeclaration final
{
    std::uint32_t externalSlot = InvalidIndex;
    std::string stableKey;
};

struct LeafPackageDeclaration final
{
    std::string stableKey;
    std::vector<std::byte> packageBytes;
    std::vector<LeafEndpointDeclaration> endpoints;
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

struct CanonicalLeafPackage final
{
    StableKey stableKey{};
    std::vector<std::byte> packageBytes;
};

class ValidatedCompositionContract final
{
public:
    ValidatedCompositionContract(const ValidatedCompositionContract&) = default;
    ValidatedCompositionContract(ValidatedCompositionContract&&) noexcept = default;
    ValidatedCompositionContract& operator=(const ValidatedCompositionContract&) = default;
    ValidatedCompositionContract& operator=(ValidatedCompositionContract&&) noexcept = default;

    [[nodiscard]] const PackageCompositionContract& Contract() const noexcept { return contract_; }
    [[nodiscard]] std::span<const CanonicalLeafPackage> Leaves() const noexcept { return leaves_; }

private:
    struct ConstructionToken final {};
    ValidatedCompositionContract(
        PackageCompositionContract contract,
        std::vector<CanonicalLeafPackage> leaves,
        ConstructionToken)
        : contract_(std::move(contract)), leaves_(std::move(leaves)) {}

    PackageCompositionContract contract_;
    std::vector<CanonicalLeafPackage> leaves_;

    friend base::Result<ValidatedCompositionContract, ContractError>
    BuildCompositionContract(ContractBuildInput);
    friend base::Result<ValidatedCompositionContract, ContractError>
    ValidateCompositionContractAgainstLeaves(
        PackageCompositionContract,
        std::vector<CanonicalLeafPackage>);
};

[[nodiscard]] StableKey ComputeStableLeafKey(std::string_view authorKey);
[[nodiscard]] StableKey ComputeStableEndpointKey(std::string_view authorKey);
[[nodiscard]] StableKey ComputeStableResourceKey(std::string_view authorKey);

[[nodiscard]] base::Result<ValidatedCompositionContract, ContractError>
BuildCompositionContract(ContractBuildInput input);

[[nodiscard]] base::Result<ValidatedCompositionContract, ContractError>
ValidateCompositionContractAgainstLeaves(
    PackageCompositionContract contract,
    std::vector<CanonicalLeafPackage> leaves);

[[nodiscard]] base::Result<void, ContractError>
ValidateCompositionContractShape(const PackageCompositionContract& contract);

[[nodiscard]] std::vector<std::byte>
SerializeCompositionContract(const PackageCompositionContract& contract);

[[nodiscard]] base::Result<PackageCompositionContract, ContractError>
DeserializeCompositionContract(std::span<const std::byte> bytes);

[[nodiscard]] base::Digest256
ComputeCompositionContractIdentity(const PackageCompositionContract& contract);
}
