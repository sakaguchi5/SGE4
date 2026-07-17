#pragma once

#include "../00_Foundation/Result.h"
#include "../00_Foundation/Sha256.h"
#include "../09_FrozenPackageCore/FrozenExecutablePackage.h"
#include "../10_D3D12PackageSchema/D3D12Schema.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace sge4::package::d3d12_v18
{
inline constexpr std::uint32_t TargetSchemaVersion = 18;
inline constexpr std::uint32_t MinimumRuntimeVersion = 18;
inline constexpr SectionKind CompositionEndpointTableSection =
    SectionKind::D3D12CompositionEndpointTable;
inline constexpr std::uint16_t CompositionEndpointSectionSchemaVersion = 1;
inline constexpr std::uint32_t CompositionEndpointStride = 96;

using StableEndpointKey = base::Digest256;
using ResourceId = d3d12_v13::ResourceId;
using ExternalSlotId = d3d12_v13::ExternalSlotId;
using ResourceKind = d3d12_v13::ResourceKind;
using Format = d3d12_v13::Format;
using ResourceState = d3d12_v13::ResourceState;
using ExternalSynchronizationContract = d3d12_v13::ExternalSynchronizationContract;

enum class EndpointAccess : std::uint16_t
{
    ReadOnly = 1,
    WriteOnly = 2
};

enum class EndpointFrameSemantics : std::uint32_t
{
    PerFrame = 1
};

enum class EndpointFlags : std::uint32_t
{
    Required = 1u << 0
};

struct LeafCompositionEndpointArtifact final
{
    std::uint32_t id = InvalidIndex;
    StableEndpointKey stableKey{};
    ExternalSlotId externalSlot;
    ResourceId resource;
    ResourceKind kind = ResourceKind::Buffer;
    EndpointAccess access = EndpointAccess::ReadOnly;
    Format format = Format::Unknown;
    std::uint64_t minimumBytes = 0;
    ResourceState requiredIncomingState;
    ResourceState guaranteedOutgoingState;
    ExternalSynchronizationContract synchronization =
        ExternalSynchronizationContract::CompletionTokenRequired;
    EndpointFrameSemantics frameSemantics = EndpointFrameSemantics::PerFrame;
    std::uint32_t flags = static_cast<std::uint32_t>(EndpointFlags::Required);
    std::uint32_t reserved = 0;
};

[[nodiscard]] StableEndpointKey ComputeStableEndpointKey(std::string_view authorKey);
[[nodiscard]] bool IsZeroStableEndpointKey(const StableEndpointKey& key) noexcept;

class CompositionLeafView final
{
public:
    [[nodiscard]] static base::Result<CompositionLeafView, PackageError> Decode(
        const FrozenExecutablePackage& package);

    [[nodiscard]] std::span<const LeafCompositionEndpointArtifact> Endpoints() const noexcept
    {
        return endpoints_;
    }

    [[nodiscard]] std::span<const std::byte> Schema17BasePackageBytes() const noexcept
    {
        return schema17BasePackageBytes_;
    }

private:
    std::vector<LeafCompositionEndpointArtifact> endpoints_;
    std::vector<std::byte> schema17BasePackageBytes_;
};
}
