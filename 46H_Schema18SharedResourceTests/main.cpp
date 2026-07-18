#include "../46G_Schema18SharedDeviceDomainTests/RuntimeFixture.h"

#include "../13A_Schema18SharedDeviceDomain/Schema18SharedDeviceDomain.h"
#include "../14A_Schema18SharedResources/Schema18SharedResources.h"
#include "../14_D3D12Backend/D3D12Backend.h"

#include <array>
#include <iostream>

int main()
{
    namespace fixture = sge4::qualification::schema18_runtime_fixture;
    namespace runtime18 = sge4::composition::schema18::runtime_v1;

    auto artifact = fixture::BuildDiamondArtifact();
    if (!artifact)
    {
        std::cerr << artifact.Error() << '\n';
        return 1;
    }
    const auto inputId = fixture::FindResourceFlow(artifact.Value(), "f9/diamond/input");
    const auto fanoutId = fixture::FindResourceFlow(artifact.Value(), "f9/diamond/fanout");
    if (!inputId.IsValid() || !fanoutId.IsValid()) return 2;

    sge4::d3d12::D3D12Backend backend({true, false});
    auto domain = runtime18::MaterializeSharedDeviceDomain(
        artifact.Value(), backend, nullptr);
    if (!domain) return 3;

    const std::array<float, 4> input{1.0f, 2.0f, 3.0f, 4.0f};
    const std::vector<runtime18::SharedResourceInitialData> initial = {
        {inputId, fixture::Bytes(input)}};
    auto table = runtime18::MaterializeSharedResources(domain.Value(), initial);
    if (!table)
    {
        std::cerr << table.Error().stage << ": " << table.Error().message << '\n';
        return 4;
    }
    if (table.Value().ResourceCount() != 5) return 5;
    for (const auto& allocation : artifact.Value().plan.allocations)
    {
        const auto* record = table.Value().Record(allocation.resource);
        if (!record || !record->resource || !record->availableAfter ||
            record->resource->DeviceEpoch() != domain.Value().DeviceEpoch() ||
            record->availableAfter->DeviceEpoch() != domain.Value().DeviceEpoch() ||
            record->sizeBytes != 16)
            return 6;
    }

    const auto sourceOutput = fixture::FindEndpoint(
        artifact.Value(), "f9/diamond/source", fixture::OutputEndpoint);
    const auto leftInput = fixture::FindEndpoint(
        artifact.Value(), "f9/diamond/left", fixture::InputEndpoint);
    const auto rightInput = fixture::FindEndpoint(
        artifact.Value(), "f9/diamond/right", fixture::InputEndpoint);
    if (!sourceOutput.IsValid() || !leftInput.IsValid() || !rightInput.IsValid()) return 7;
    if (table.Value().ResourceForEndpoint(sourceOutput) != fanoutId ||
        table.Value().ResourceForEndpoint(leftInput) != fanoutId ||
        table.Value().ResourceForEndpoint(rightInput) != fanoutId)
        return 8;
    const auto sourceHandle = table.Value().ResourceForEndpointHandle(sourceOutput);
    const auto leftHandle = table.Value().ResourceForEndpointHandle(leftInput);
    const auto rightHandle = table.Value().ResourceForEndpointHandle(rightInput);
    if (!sourceHandle || sourceHandle.get() != leftHandle.get() ||
        sourceHandle.get() != rightHandle.get())
        return 9;

    auto read = runtime18::ReadSharedResource(table.Value(), inputId);
    if (!read || !fixture::Equals(read.Value().bytes, input)) return 10;

    const std::vector<runtime18::SharedResourceInitialData> duplicate = {
        {inputId, fixture::Bytes(input)}, {inputId, fixture::Bytes(input)}};
    auto duplicateRejected = runtime18::MaterializeSharedResources(
        domain.Value(), duplicate);
    if (duplicateRejected || duplicateRejected.Error().stage != "resource/initial-data")
        return 11;
    std::vector<std::byte> oversized(17);
    const std::vector<runtime18::SharedResourceInitialData> tooLarge = {
        {inputId, std::move(oversized)}};
    auto oversizeRejected = runtime18::MaterializeSharedResources(
        domain.Value(), tooLarge);
    if (oversizeRejected || oversizeRejected.Error().stage != "resource/initial-data")
        return 12;

    std::cout << "L4V1-F8 Composition-owned Shared Buffer tests passed\n";
    std::cout << "Shared Buffers: 5; fan-out endpoints share one native resource identity\n";
    std::cout << "Epoch authority: Resource and completion token belong to one DeviceDomain\n";
    std::cout << "WARP initial-data readback: 1, 2, 3, 4\n";
    return 0;
}
