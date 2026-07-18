#pragma once

#include "../00_Foundation/Result.h"
#include "../13A_Schema18SharedDeviceDomain/Schema18SharedDeviceDomain.h"
#include "../14A_Schema18SharedResources/Schema18SharedResources.h"
#include "../19A_Schema18CompositionLinker/Schema18CompositionLinker.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace sge4::composition::schema18::runtime_v1
{
class StaticCompositionRecoveryAccess;
struct StaticRuntimeError final
{
    std::string stage;
    std::string message;
};

struct StaticCompositionLoadInput final
{
    std::vector<SharedResourceInitialData> initialResources;
    runtime::ISurfaceHost* surface = nullptr;
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
    std::vector<runtime::FrameSubmission> leaves;
};

class LoadedStaticComposition final
{
public:
    LoadedStaticComposition(LoadedStaticComposition&&) noexcept = default;
    LoadedStaticComposition& operator=(LoadedStaticComposition&&) noexcept = default;
    LoadedStaticComposition(const LoadedStaticComposition&) = delete;
    LoadedStaticComposition& operator=(const LoadedStaticComposition&) = delete;

    [[nodiscard]] std::uint64_t DeviceEpoch() const noexcept
    {
        return domain_ ? domain_->DeviceEpoch() : 0;
    }
    [[nodiscard]] runtime::DeviceRuntimeState State() const noexcept
    {
        return domain_ ? domain_->State() : runtime::DeviceRuntimeState::Lost;
    }
    [[nodiscard]] std::size_t LeafCount() const noexcept
    {
        return domain_ ? domain_->LeafCount() : 0;
    }
    [[nodiscard]] const linking::FrozenCompositionArtifact& Artifact() const noexcept
    {
        return domain_->Artifact();
    }
    [[nodiscard]] std::shared_ptr<runtime::IExternalResource>
    SharedResource(ResourceFlowId resource) const;
    [[nodiscard]] std::shared_ptr<runtime::ICompletionToken>
    AvailableAfter(ResourceFlowId resource) const;
    [[nodiscard]] ResourceFlowId FindResourceFlow(const StableKey& stableKey) const noexcept;

private:
    LoadedStaticComposition() = default;

    friend base::Result<LoadedStaticComposition, StaticRuntimeError>
    LoadStaticComposition(std::span<const std::byte>,
                          d3d12::D3D12Backend&,
                          StaticCompositionLoadInput);
    friend base::Result<StaticCompositionSubmission, StaticRuntimeError>
    SubmitStaticComposition(LoadedStaticComposition&,
                            const StaticCompositionFrameInvocation&);
    friend base::Result<d3d12::ExternalBufferReadback, StaticRuntimeError>
    ReadStaticCompositionBuffer(LoadedStaticComposition&, ResourceFlowId);
    friend class StaticCompositionRecoveryAccess;

    std::unique_ptr<SharedDeviceDomain> domain_;
    std::unique_ptr<SharedResourceTable> resources_;
    std::vector<std::shared_ptr<runtime::ICompletionToken>> endpointTokens_;
    std::vector<SharedResourceInitialData> recoveryInitialResources_;
};

[[nodiscard]] base::Result<LoadedStaticComposition, StaticRuntimeError>
LoadStaticComposition(
    std::span<const std::byte> frozenCompositionBytes,
    d3d12::D3D12Backend& backend,
    StaticCompositionLoadInput input = {});

[[nodiscard]] base::Result<StaticCompositionSubmission, StaticRuntimeError>
SubmitStaticComposition(
    LoadedStaticComposition& loaded,
    const StaticCompositionFrameInvocation& invocation);

[[nodiscard]] base::Result<d3d12::ExternalBufferReadback, StaticRuntimeError>
ReadStaticCompositionBuffer(
    LoadedStaticComposition& loaded,
    ResourceFlowId resource);
}
