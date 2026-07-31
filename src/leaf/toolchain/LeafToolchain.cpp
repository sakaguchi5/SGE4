#include "LeafToolchain.h"

#include "../artifact/package/PackageReader.h"
#include "../../backends/d3d12/artifact/D3D12Encoding.h"
#include "compiler/LeafCompiler.h"

namespace sge4::leaf
{
namespace
{
template<class T>
[[nodiscard]] base::Expected<T, Error> Fail(std::string stage, std::string message)
{
    return base::Failure<T, Error>({std::move(stage), std::move(message)});
}
}

base::Expected<FrozenLeafPackage, Error> ReadFrozenLeaf(std::span<const std::byte> bytes)
{
    return ReadFrozenLeaf(std::vector<std::byte>(bytes.begin(), bytes.end()));
}

base::Expected<FrozenLeafPackage, Error> ReadFrozenLeaf(std::vector<std::byte> bytes)
{
    auto parsed = package::PackageReader::Read(bytes);
    if (!parsed)
        return Fail<FrozenLeafPackage>("LeafReader", parsed.error().message);
    auto view = package::d3d12_v13::D3D12PackageView::Decode(parsed.value());
    if (!view)
        return Fail<FrozenLeafPackage>("LeafReader", view.error().message);
    auto certificate = BuildLeafCertificate(parsed.value(), view.value());
    if (!certificate)
        return Fail<FrozenLeafPackage>(certificate.error().stage, certificate.error().message);

    std::vector<EndpointDescriptor> endpoints;
    endpoints.reserve(view.value().ExternalSlots().size());
    for (const auto& slot : view.value().ExternalSlots())
    {
        endpoints.push_back({slot.id.value, slot.requiredKind, slot.requiredFormat, slot.minimumBytes,
            slot.requiredIncomingState, slot.guaranteedOutgoingState,
            (slot.flags & static_cast<std::uint32_t>(package::d3d12_v13::ExternalSlotFlags::Required)) != 0});
    }
    const auto header = parsed.value().Header();
    return base::Success<FrozenLeafPackage, Error>(FrozenLeafPackage(
        std::move(bytes), header.executionDigest, header.fileDigest, header.targetProfileDigest,
        header.targetKind, header.targetSchemaVersion, header.minimumRuntimeVersion,
        std::move(endpoints), static_cast<std::uint32_t>(view.value().SurfaceSlots().size()),
        std::move(certificate).value()));
}

base::Expected<FrozenLeafPackage, Error> CompileFrozenLeaf(
    const semantic::SemanticGraph& graph,
    const target::D3D12TargetProfile& targetProfile)
{
    auto compiled = compiler::CompileCanonical(graph, targetProfile);
    if (!compiled)
        return Fail<FrozenLeafPackage>(compiled.error().stage, compiled.error().message);
    return ReadFrozenLeaf(std::move(compiled).value().packageBytes);
}
}
