#pragma once

#include "../../canonical/artifact/SectionedArtifact.h"
#include "../model/semantic/SemanticModel.h"
#include "../../backends/d3d12/compiler/target/TargetModel.h"
#include "../artifact/package/FrozenExecutablePackage.h"
#include "../../backends/d3d12/artifact/D3D12Schema.h"
#include "../artifact/LeafCertificate.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace sge4::leaf
{
struct EndpointDescriptor final
{
    std::uint32_t externalSlot = package::InvalidIndex;
    package::d3d12_v13::ResourceKind kind = package::d3d12_v13::ResourceKind::Buffer;
    package::d3d12_v13::Format format = package::d3d12_v13::Format::Unknown;
    std::uint64_t minimumBytes = 0;
    package::d3d12_v13::ResourceState requiredIncomingState;
    package::d3d12_v13::ResourceState guaranteedOutgoingState;
    bool required = true;
};

class FrozenLeafPackage final
{
public:
    FrozenLeafPackage(const FrozenLeafPackage&) = default;
    FrozenLeafPackage& operator=(const FrozenLeafPackage&) = default;
    FrozenLeafPackage(FrozenLeafPackage&&) noexcept = default;
    FrozenLeafPackage& operator=(FrozenLeafPackage&&) noexcept = default;

    [[nodiscard]] std::span<const std::byte> FileBytes() const noexcept { return bytes_; }
    [[nodiscard]] const Digest256& ExecutionDigest() const noexcept { return executionDigest_; }
    [[nodiscard]] const Digest256& FileDigest() const noexcept { return fileDigest_; }
    [[nodiscard]] const Digest256& TargetProfileDigest() const noexcept { return targetProfileDigest_; }
    [[nodiscard]] std::uint32_t TargetKind() const noexcept { return targetKind_; }
    [[nodiscard]] std::uint32_t TargetSchemaVersion() const noexcept { return targetSchemaVersion_; }
    [[nodiscard]] std::uint32_t MinimumRuntimeVersion() const noexcept { return minimumRuntimeVersion_; }
    [[nodiscard]] std::span<const EndpointDescriptor> Endpoints() const noexcept { return endpoints_; }
    [[nodiscard]] std::uint32_t SurfaceSlotCount() const noexcept { return surfaceSlotCount_; }
    [[nodiscard]] const LeafCertificate& Certificate() const noexcept { return certificate_; }

private:
    friend base::Expected<FrozenLeafPackage, Error> ReadFrozenLeaf(std::vector<std::byte>);

    FrozenLeafPackage(
        std::vector<std::byte> bytes,
        Digest256 executionDigest,
        Digest256 fileDigest,
        Digest256 targetProfileDigest,
        std::uint32_t targetKind,
        std::uint32_t targetSchemaVersion,
        std::uint32_t minimumRuntimeVersion,
        std::vector<EndpointDescriptor> endpoints,
        std::uint32_t surfaceSlotCount,
        LeafCertificate certificate)
        : bytes_(std::move(bytes)), executionDigest_(executionDigest), fileDigest_(fileDigest),
          targetProfileDigest_(targetProfileDigest), targetKind_(targetKind),
          targetSchemaVersion_(targetSchemaVersion), minimumRuntimeVersion_(minimumRuntimeVersion),
          endpoints_(std::move(endpoints)), surfaceSlotCount_(surfaceSlotCount), certificate_(std::move(certificate)) {}

    std::vector<std::byte> bytes_;
    Digest256 executionDigest_{};
    Digest256 fileDigest_{};
    Digest256 targetProfileDigest_{};
    std::uint32_t targetKind_ = 0;
    std::uint32_t targetSchemaVersion_ = 0;
    std::uint32_t minimumRuntimeVersion_ = 0;
    std::vector<EndpointDescriptor> endpoints_;
    std::uint32_t surfaceSlotCount_ = 0;
    LeafCertificate certificate_;
};

[[nodiscard]] base::Expected<FrozenLeafPackage, Error> ReadFrozenLeaf(
    std::span<const std::byte> bytes);
[[nodiscard]] base::Expected<FrozenLeafPackage, Error> ReadFrozenLeaf(
    std::vector<std::byte> bytes);

[[nodiscard]] base::Expected<FrozenLeafPackage, Error> CompileFrozenLeaf(
    const semantic::SemanticGraph& graph,
    const target::D3D12TargetProfile& targetProfile);
}
