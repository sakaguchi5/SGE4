#pragma once

#include "../00_Foundation/Result.h"
#include "../14_D3D12Backend/D3D12Backend.h"
#include "../19_PackageLinker/FrozenComposition.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace sge4::composition::runtime_v1
{
struct CompositionRuntimeError final { std::string stage; std::string message; };
struct SharedResourceInitialData final { SharedResourceId resource; std::vector<std::byte> bytes; };
struct CompositionLoadInput final { std::vector<SharedResourceInitialData> initialResources; runtime::ISurfaceHost* surface = nullptr; };
struct LeafDynamicData final { LeafPackageId leaf; std::uint32_t slot = package::InvalidIndex; std::vector<std::byte> bytes; };
struct CompositionFrameInvocation final { std::uint64_t frameNumber = 0; std::vector<LeafDynamicData> dynamicData; };
struct CompositionSubmission final { std::uint64_t deviceEpoch = 0; std::vector<runtime::FrameSubmission> leaves; };

class LoadedComposition final
{
public:
    LoadedComposition(LoadedComposition&&) noexcept = default;
    LoadedComposition& operator=(LoadedComposition&&) noexcept = default;
    LoadedComposition(const LoadedComposition&) = delete;
    LoadedComposition& operator=(const LoadedComposition&) = delete;
    [[nodiscard]] std::uint64_t DeviceEpoch() const noexcept { return domain_->DeviceEpoch(); }
    [[nodiscard]] std::size_t LeafCount() const noexcept { return instances_.size(); }
    [[nodiscard]] const FrozenComposition& Artifact() const noexcept { return artifact_; }
    [[nodiscard]] std::shared_ptr<runtime::IExternalResource> SharedResource(SharedResourceId id) const;
    [[nodiscard]] std::shared_ptr<runtime::ICompletionToken> AvailableAfter(SharedResourceId id) const;
private:
    friend base::Result<LoadedComposition, CompositionRuntimeError> LoadComposition(FrozenComposition,d3d12::D3D12Backend&,CompositionLoadInput);
    friend base::Result<CompositionSubmission, CompositionRuntimeError> SubmitComposition(LoadedComposition&,const CompositionFrameInvocation&);
    friend base::Result<d3d12::ExternalBufferReadback, CompositionRuntimeError> ReadCompositionBuffer(LoadedComposition&,SharedResourceId);
    friend base::Result<runtime::DeviceRecoveryReport, CompositionRuntimeError> RecoverComposition(LoadedComposition&,runtime::DeviceRecoveryMode);
    LoadedComposition(FrozenComposition artifact,d3d12::D3D12Backend& backend,CompositionLoadInput input)
        : artifact_(std::move(artifact)),backend_(&backend),surface_(input.surface) {}
    base::Result<void,CompositionRuntimeError> Materialize();
    FrozenComposition artifact_;
    d3d12::D3D12Backend* backend_ = nullptr;
    runtime::ISurfaceHost* surface_ = nullptr;
    std::unique_ptr<runtime::IPackageDeviceDomain> domain_;
    std::vector<std::shared_ptr<const package::FrozenExecutablePackage>> leafPackages_;
    std::vector<std::unique_ptr<runtime::IPackageInstance>> instances_;
    std::vector<d3d12::ExternalBufferBinding> resources_;
    std::vector<std::shared_ptr<runtime::ICompletionToken>> endpointTokens_;
    std::vector<std::vector<std::byte>> initialData_;
    std::vector<package::d3d12_v13::ResourceState> resourceStates_;
};

[[nodiscard]] base::Result<LoadedComposition, CompositionRuntimeError> LoadComposition(
    FrozenComposition artifact,d3d12::D3D12Backend& backend,CompositionLoadInput input = {});
[[nodiscard]] base::Result<CompositionSubmission, CompositionRuntimeError> SubmitComposition(
    LoadedComposition& loaded,const CompositionFrameInvocation& invocation);
[[nodiscard]] base::Result<d3d12::ExternalBufferReadback, CompositionRuntimeError> ReadCompositionBuffer(
    LoadedComposition& loaded,SharedResourceId resource);
[[nodiscard]] base::Result<runtime::DeviceRecoveryReport, CompositionRuntimeError> RecoverComposition(
    LoadedComposition& loaded,runtime::DeviceRecoveryMode mode);
}
