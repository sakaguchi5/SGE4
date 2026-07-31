#include "Abi2PortableSelfTest.h"
#include "Abi2CorruptionTests.h"

#include "../../src/backends/d3d12/artifact/D3D12Encoding.h"
#include "../../src/canonical/artifact/SectionedArtifact.h"
#include "../../src/composition/artifact/abi2/FrozenCompositionAbi2.h"
#include "../../src/composition/migration/abi1/FrozenCompositionAbi1Migration.h"
#include "../../src/composition/toolchain/CompositionToolchain.h"
#include "../../src/leaf/artifact/package/PackageReader.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace sge4::tests
{
namespace
{
namespace d3d = package::d3d12_v13;
namespace artifact = composition::artifact;

void Require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

[[nodiscard]] d3d::OperationArtifact MakeOperation(
    d3d::D3D12OperationCode code,
    d3d::QueueId queue = {},
    std::vector<std::byte> payload = {})
{
    d3d::OperationArtifact operation;
    operation.opcode = code;
    operation.operationVersion = d3d::OperationVersion(code);
    operation.queue = queue;
    operation.payload = std::move(payload);
    return operation;
}

[[nodiscard]] std::vector<std::byte> BuildPortableLeafPackage()
{
    d3d::D3D12PackageDescription description;
    description.profile.shaderModelMajor = 5;
    description.profile.shaderModelMinor = 1;
    description.profile.rootSignatureMajor = 1;
    description.profile.rootSignatureMinor = 0;
    description.profile.framesInFlight = 1;
    description.profile.directQueueCount = 1;
    description.profile.computeQueueCount = 0;
    description.profile.copyQueueCount = 0;
    description.profile.surfaceImageCount = 0;
    description.profile.shaderDescriptorCount = 2;

    for (std::uint32_t index = 0; index < 2; ++index)
    {
        d3d::ResourceArtifact resource;
        resource.id = d3d::ResourceId{index};
        resource.origin = d3d::ResourceOrigin::External;
        resource.rebuildPolicy = d3d::RebuildPolicy::RequireExternalRebind;
        resource.sizeBytes = 16;
        resource.firstView = index;
        resource.viewCount = 1;
        description.resources.push_back(resource);

        d3d::ResourceViewArtifact view;
        view.id = d3d::ViewId{index};
        view.resource = d3d::ResourceId{index};
        view.viewClass = index == 0
            ? d3d::ViewClass::ShaderResource
            : d3d::ViewClass::UnorderedAccess;
        view.byteSize = 16;
        view.strideBytes = 16;
        view.descriptorHeapClass = 2;
        view.descriptorIndex = index;
        description.views.push_back(view);

        d3d::ExternalResourceSlotArtifact slot;
        slot.id = d3d::ExternalSlotId{index};
        slot.resource = d3d::ResourceId{index};
        slot.minimumBytes = 16;
        slot.requiredIncomingState = {
            d3d::StateClass::Explicit,
            0,
            index == 0
                ? std::to_underlying(d3d::ExplicitStateBits::NonPixelShaderRead)
                : std::to_underlying(d3d::ExplicitStateBits::UnorderedWrite)};
        slot.guaranteedOutgoingState = slot.requiredIncomingState;
        description.externalSlots.push_back(slot);
    }

    description.operationStreams.push_back({d3d::OperationStreamKind::Load, 0, 1, 0});
    description.operationStreams.push_back({d3d::OperationStreamKind::Frame, 1, 7, 0});
    description.operations.push_back(MakeOperation(
        d3d::D3D12OperationCode::CreateDescriptorHeaps));
    description.operations.push_back(MakeOperation(
        d3d::D3D12OperationCode::AcquireExternal, {},
        d3d::Encode(d3d::AcquireExternalPayload{d3d::ExternalSlotId{0}})));
    description.operations.push_back(MakeOperation(
        d3d::D3D12OperationCode::WaitExternal, d3d::QueueId{0},
        d3d::Encode(d3d::WaitExternalPayload{d3d::ExternalSlotId{0}})));
    description.operations.push_back(MakeOperation(
        d3d::D3D12OperationCode::AcquireExternal, {},
        d3d::Encode(d3d::AcquireExternalPayload{d3d::ExternalSlotId{1}})));
    description.operations.push_back(MakeOperation(
        d3d::D3D12OperationCode::WaitExternal, d3d::QueueId{0},
        d3d::Encode(d3d::WaitExternalPayload{d3d::ExternalSlotId{1}})));
    description.operations.push_back(MakeOperation(
        d3d::D3D12OperationCode::SignalQueue, d3d::QueueId{0},
        d3d::Encode(d3d::SignalQueuePayload{d3d::SignalPointId{0}})));
    description.operations.push_back(MakeOperation(
        d3d::D3D12OperationCode::ReleaseExternal, {},
        d3d::Encode(d3d::ReleaseExternalPayload{
            d3d::ExternalSlotId{0}, d3d::SignalPointId{0}})));
    description.operations.push_back(MakeOperation(
        d3d::D3D12OperationCode::ReleaseExternal, {},
        d3d::Encode(d3d::ReleaseExternalPayload{
            d3d::ExternalSlotId{1}, d3d::SignalPointId{0}})));
    description.provenance = {std::byte{1}};

    auto bytes = d3d::BuildFrozenPackage(description);
    Require(static_cast<bool>(bytes), "Portable Leaf Packageの生成に失敗しました。");

    auto package = package::PackageReader::Read(bytes.value());
    Require(static_cast<bool>(package), "Portable Leaf Packageの読込に失敗しました。");
    auto decoded = d3d::D3D12PackageView::Decode(package.value());
    Require(static_cast<bool>(decoded), "Portable Leaf PackageのD3D12 Schema検証に失敗しました。");
    Require(decoded.value().ExternalSlots().size() == 2,
        "Portable Leaf PackageのEndpoint数が一致しません。");
    return std::move(bytes).value();
}

[[nodiscard]] composition::LeafPackageDeclaration MakeLeaf(
    std::string stableKey,
    const std::vector<std::byte>& packageBytes)
{
    composition::LeafPackageDeclaration leaf;
    leaf.stableKey = std::move(stableKey);
    leaf.packageBytes = packageBytes;
    leaf.endpoints = {
        {0, "portable/input"},
        {1, "portable/output"}};
    return leaf;
}

[[nodiscard]] composition::EndpointReferenceDeclaration Reference(
    std::string leaf,
    std::string endpoint)
{
    return {std::move(leaf), std::move(endpoint)};
}

[[nodiscard]] composition::ContractBuildInput BuildPortableCompositionInput(
    const std::vector<std::byte>& packageBytes)
{
    composition::ContractBuildInput input;
    input.leaves = {
        MakeLeaf("portable/first", packageBytes),
        MakeLeaf("portable/second", packageBytes)};

    composition::ResourceFlowDeclaration source;
    source.stableKey = "portable/flow/input";
    source.boundary = composition::ResourceBoundary::CompositionInput;
    source.consumers = {Reference("portable/first", "portable/input")};

    composition::ResourceFlowDeclaration middle;
    middle.stableKey = "portable/flow/middle";
    middle.boundary = composition::ResourceBoundary::Internal;
    middle.producer = Reference("portable/first", "portable/output");
    middle.consumers = {Reference("portable/second", "portable/input")};

    composition::ResourceFlowDeclaration output;
    output.stableKey = "portable/flow/output";
    output.boundary = composition::ResourceBoundary::CompositionOutput;
    output.producer = Reference("portable/second", "portable/output");

    input.resources = {std::move(source), std::move(middle), std::move(output)};
    return input;
}
}

void VerifyAbi2PortableRoundTrip()
{
    const auto leafBytes = BuildPortableLeafPackage();

    auto first = composition::BuildFrozenCompositionPackage(
        BuildPortableCompositionInput(leafBytes), {1, 8});
    auto second = composition::BuildFrozenCompositionPackage(
        BuildPortableCompositionInput(leafBytes), {1, 8});
    Require(static_cast<bool>(first) && static_cast<bool>(second),
        "Portable SGE4UNI 2.0の生成に失敗しました。");
    Require(first.value().FileBytes().size() == second.value().FileBytes().size() &&
        std::equal(first.value().FileBytes().begin(), first.value().FileBytes().end(),
            second.value().FileBytes().begin()),
        "Portable SGE4UNI 2.0がbyte決定的ではありません。");
    Require(first.value().FileBytes().size() == 9304,
        "Portable SGE4UNI 2.0のGolden byte数が変化しました。");
    Require(base::ToHex(base::Sha256(first.value().FileBytes())) ==
        "753c82dfc62c65cf56d09bccf81b9b081b2e9c9a04c81ddce2d0b79f36b77223",
        "Portable SGE4UNI 2.0のGolden digestが変化しました。");

    auto outer = ReadSectionedArtifact(
        first.value().FileBytes(), artifact::FrozenCompositionAbi2Magic,
        artifact::FrozenCompositionAbi2FormatMajor);
    Require(static_cast<bool>(outer) &&
        outer.value().FormatMinor() == artifact::FrozenCompositionAbi2FormatMinor &&
        outer.value().Sections().size() == artifact::FrozenCompositionAbi2SectionKinds.size(),
        "Portable SGE4UNI 2.0の平坦Section構造が一致しません。");

    const auto leaves = first.value().VerifiedComposition().ValidatedContract().Leaves();
    Require(leaves.size() == 2, "Portable CompositionのLeaf数が一致しません。");
    for (const auto& leaf : leaves)
        Require(leaf.packageBytes == leafBytes,
            "ABI 2.0でSchema 17 Leaf Package bytesが保存されませんでした。");

    auto roundTrip = composition::ReadFrozenCompositionPackage(first.value().FileBytes());
    Require(static_cast<bool>(roundTrip), "Portable SGE4UNI 2.0のRound-tripに失敗しました。");
    Require(roundTrip.value().FileDigest() == first.value().FileDigest() &&
        roundTrip.value().CompositionCoreDigest() == first.value().CompositionCoreDigest() &&
        roundTrip.value().SemanticDigest() == first.value().SemanticDigest(),
        "Portable SGE4UNI 2.0のDigestがRound-tripで変化しました。");

    auto legacyBytes = composition::migration::abi1::BuildFrozenCompositionPackageAbi1ForMigration(
        BuildPortableCompositionInput(leafBytes), {1, 8});
    Require(static_cast<bool>(legacyBytes), "Portable SGE4UNI 1.1 Corpusの生成に失敗しました。");
    Require(!composition::ReadFrozenCompositionPackage(legacyBytes.value()),
        "Production ReaderがPortable SGE4UNI 1.1を受理しました。");

    auto migrated = composition::migration::abi1::MigrateFrozenCompositionPackageAbi1ToAbi2(
        legacyBytes.value());
    Require(static_cast<bool>(migrated), "Portable ABI 1.1から2.0へのMigrationに失敗しました。");
    Require(migrated.value().FileBytes().size() == first.value().FileBytes().size() &&
        std::equal(migrated.value().FileBytes().begin(), migrated.value().FileBytes().end(),
            first.value().FileBytes().begin()),
        "直接生成とMigration後のPortable SGE4UNI 2.0がbyte一致しません。");
    Require(migrated.value().Certificate().contractIdentity == first.value().Certificate().contractIdentity &&
        migrated.value().Certificate().planIdentity == first.value().Certificate().planIdentity &&
        migrated.value().Certificate().sealIdentity == first.value().Certificate().sealIdentity &&
        migrated.value().Certificate().scheduleIdentity == first.value().Certificate().scheduleIdentity &&
        migrated.value().Certificate().recoverySetIdentity == first.value().Certificate().recoverySetIdentity,
        "ABI 1.1から2.0へのMigrationで権威Identityが保存されませんでした。");

    VerifyAbi2CorruptionRejection(first.value().FileBytes());
}
}
