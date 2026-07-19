#pragma once

#include "../00_Foundation/Result.h"
#include "../20_CompositionDeviceDomain/CompositionDeviceDomain.h"
#include "../21_CompositionSharedResources/CompositionSharedResources.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace sge4_5::composition::canonical::runtime_v1
{
class WholeCompositionRecovery;
struct StaticRuntimeError final { std::string stage; std::string message; };
struct StaticCompositionLoadInput final
{
    std::vector<SharedResourceInitialData> initialResources;
    ::sge4_5::runtime::ISurfaceHost* surface = nullptr;
};
struct LeafDynamicData final
{
    LeafPackageId leaf;
    std::uint32_t slot = package::InvalidIndex;
    std::vector<std::byte> bytes;
};
struct StaticCompositionFrameInvocation final
{
    std::uint64_t frameNumber = 0;
    std::vector<LeafDynamicData> dynamicData;
};
struct StaticCompositionSubmission final
{
    std::uint64_t deviceEpoch = 0;
    std::vector<LeafPackageId> executionOrder;
    std::vector<::sge4_5::runtime::FrameSubmission> leaves;
};

class LoadedStaticComposition final
{
public:
    LoadedStaticComposition(LoadedStaticComposition&&) noexcept = default;
    LoadedStaticComposition& operator=(LoadedStaticComposition&&) noexcept = default;
    LoadedStaticComposition(const LoadedStaticComposition&) = delete;
    LoadedStaticComposition& operator=(const LoadedStaticComposition&) = delete;
    [[nodiscard]] std::uint64_t DeviceEpoch() const noexcept { return domain_ ? domain_->DeviceEpoch() : 0; }
    [[nodiscard]] ::sge4_5::runtime::DeviceRuntimeState State() const noexcept { return domain_ ? domain_->State() : ::sge4_5::runtime::DeviceRuntimeState::Lost; }
    [[nodiscard]] std::size_t LeafCount() const noexcept { return domain_ ? domain_->LeafCount() : 0; }
    [[nodiscard]] std::size_t ResourceCount() const noexcept { return resources_ ? resources_->ResourceCount() : 0; }
    [[nodiscard]] const verification::VerifiedFrozenComposition& Artifact() const noexcept { return domain_->Artifact(); }
    [[nodiscard]] std::shared_ptr<::sge4_5::runtime::IExternalResource> SharedResource(ResourceFlowId) const;
    [[nodiscard]] std::shared_ptr<::sge4_5::runtime::ICompletionToken> AvailableAfter(ResourceFlowId) const;
    [[nodiscard]] ResourceFlowId FindResourceFlow(const StableKey&) const noexcept;
private:
    LoadedStaticComposition() = default;
    friend base::Result<LoadedStaticComposition, StaticRuntimeError> LoadStaticComposition(std::span<const std::byte>, d3d12::D3D12Backend&, StaticCompositionLoadInput);
    friend base::Result<StaticCompositionSubmission, StaticRuntimeError> SubmitStaticComposition(LoadedStaticComposition&, const StaticCompositionFrameInvocation&);
    friend base::Result<d3d12::ExternalBufferReadback, StaticRuntimeError> ReadStaticCompositionBuffer(LoadedStaticComposition&, ResourceFlowId);
    friend class WholeCompositionRecovery;
    std::unique_ptr<SharedDeviceDomain> domain_;
    std::unique_ptr<SharedResourceTable> resources_;
    std::vector<std::shared_ptr<::sge4_5::runtime::ICompletionToken>> endpointTokens_;
    std::vector<SharedResourceInitialData> recoveryInitialResources_;
};

[[nodiscard]] base::Result<LoadedStaticComposition, StaticRuntimeError> LoadStaticComposition(
    std::span<const std::byte>, d3d12::D3D12Backend&, StaticCompositionLoadInput input = {});
[[nodiscard]] base::Result<StaticCompositionSubmission, StaticRuntimeError> SubmitStaticComposition(
    LoadedStaticComposition&, const StaticCompositionFrameInvocation&);
[[nodiscard]] base::Result<d3d12::ExternalBufferReadback, StaticRuntimeError> ReadStaticCompositionBuffer(
    LoadedStaticComposition&, ResourceFlowId);
[[nodiscard]] base::Result<void, StaticRuntimeError> ValidateStaticCompositionHandleEpoch(
    const LoadedStaticComposition&,
    const std::shared_ptr<::sge4_5::runtime::IExternalResource>&,
    const std::shared_ptr<::sge4_5::runtime::ICompletionToken>&);
}
