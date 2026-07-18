#pragma once

#include "../00_Foundation/Result.h"
#include "../13A_Schema18SharedDeviceDomain/Schema18SharedDeviceDomain.h"
#include "../14_D3D12Backend/D3D12Backend.h"
#include "../17A_Schema18ResourceFlowPlan/Schema18ResourceFlowPlan.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace sge4::composition::schema18::runtime_v1
{
struct SharedResourceError final
{
    std::string stage;
    std::string message;
};

struct SharedResourceInitialData final
{
    ResourceFlowId resource;
    std::vector<std::byte> bytes;
};

struct SharedResourceRecord final
{
    ResourceFlowId id;
    planning::AllocationOwnership ownership =
        planning::AllocationOwnership::CompositionOwned;
    std::uint64_t sizeBytes = 0;
    package::d3d12_v18::ResourceState currentState;
    std::shared_ptr<runtime::IExternalResource> resource;
    std::shared_ptr<runtime::ICompletionToken> availableAfter;
};

class SharedResourceTable final
{
public:
    SharedResourceTable(SharedResourceTable&&) noexcept = default;
    SharedResourceTable& operator=(SharedResourceTable&&) noexcept = default;
    SharedResourceTable(const SharedResourceTable&) = delete;
    SharedResourceTable& operator=(const SharedResourceTable&) = delete;

    [[nodiscard]] std::size_t ResourceCount() const noexcept { return records_.size(); }
    [[nodiscard]] const SharedResourceRecord* Record(ResourceFlowId resource) const noexcept;
    [[nodiscard]] SharedResourceRecord* Record(ResourceFlowId resource) noexcept;
    [[nodiscard]] ResourceFlowId ResourceForEndpoint(CompositionEndpointId endpoint) const noexcept;
    [[nodiscard]] std::shared_ptr<runtime::IExternalResource>
    ResourceForEndpointHandle(CompositionEndpointId endpoint) const;
    [[nodiscard]] std::shared_ptr<runtime::ICompletionToken>
    AvailableAfter(ResourceFlowId resource) const;
    [[nodiscard]] package::d3d12_v18::ResourceState
    CurrentState(ResourceFlowId resource) const noexcept;

    [[nodiscard]] base::Result<void, SharedResourceError> PrepareForEndpoint(
        CompositionEndpointId endpoint);
    [[nodiscard]] base::Result<void, SharedResourceError> UpdateAfterRelease(
        CompositionEndpointId endpoint,
        std::shared_ptr<runtime::ICompletionToken> token);
    [[nodiscard]] base::Result<void, SharedResourceError> UpdateAfterObservation(
        ResourceFlowId resource,
        std::shared_ptr<runtime::ICompletionToken> token);

private:
    friend base::Result<SharedResourceTable, SharedResourceError>
    MaterializeSharedResources(
        SharedDeviceDomain&,
        std::span<const SharedResourceInitialData>);
    friend base::Result<d3d12::ExternalBufferReadback, SharedResourceError>
    ReadSharedResource(SharedResourceTable&, ResourceFlowId);

    explicit SharedResourceTable(SharedDeviceDomain& domain) : domain_(&domain) {}

    SharedDeviceDomain* domain_ = nullptr;
    std::vector<SharedResourceRecord> records_;
    std::vector<ResourceFlowId> endpointResources_;
};

[[nodiscard]] base::Result<SharedResourceTable, SharedResourceError>
MaterializeSharedResources(
    SharedDeviceDomain& domain,
    std::span<const SharedResourceInitialData> initialData = {});

[[nodiscard]] base::Result<d3d12::ExternalBufferReadback, SharedResourceError>
ReadSharedResource(SharedResourceTable& table, ResourceFlowId resource);
}
