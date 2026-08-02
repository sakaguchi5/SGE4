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

[[nodiscard]] std::vector<std::byte> BuildPortableTextureLeafPackage(
    bool consumer,
    std::uint32_t width = 4,
    std::uint32_t height = 4,
    bool uavFlow = false)
{
    d3d::D3D12PackageDescription description;
    description.profile.shaderModelMajor = 5;
    description.profile.shaderModelMinor = 1;
    description.profile.rootSignatureMajor = 1;
    description.profile.rootSignatureMinor = 0;
    description.profile.framesInFlight = 1;
    description.profile.directQueueCount = 1;
    description.profile.surfaceImageCount = 0;
    description.profile.rtvDescriptorCount = consumer ? 1u : (uavFlow ? 0u : 1u);
    description.profile.shaderDescriptorCount = consumer || uavFlow ? 1u : 0u;

    const std::uint32_t resourceCount = consumer ? 2u : 1u;
    for (std::uint32_t index = 0; index < resourceCount; ++index)
    {
        const bool read = consumer && index == 0;
        const bool unorderedWrite = uavFlow && !consumer;
        const auto textureFormat = (uavFlow && (!consumer || index == 0))
            ? d3d::Format::R32G32B32A32Float
            : d3d::Format::B8G8R8A8Unorm;
        d3d::ResourceArtifact resource;
        resource.id = d3d::ResourceId{index};
        resource.resourceKind = d3d::ResourceKind::Texture2D;
        resource.origin = d3d::ResourceOrigin::External;
        resource.rebuildPolicy = d3d::RebuildPolicy::RequireExternalRebind;
        resource.extentMode = d3d::ExtentMode::Fixed;
        resource.physicalInstanceCount = 1;
        resource.format = textureFormat;
        resource.width = width;
        resource.height = height;
        resource.depthOrArraySize = 1;
        resource.mipLevels = 1;
        resource.sampleCount = 1;
        resource.planeCount = 1;
        resource.firstView = index;
        resource.viewCount = 1;
        resource.initialState = {
            d3d::StateClass::Explicit, 0,
            read ? std::to_underlying(d3d::ExplicitStateBits::PixelShaderRead)
                 : unorderedWrite ? std::to_underlying(d3d::ExplicitStateBits::UnorderedWrite)
                                  : std::to_underlying(d3d::ExplicitStateBits::RenderTarget)};
        description.resources.push_back(resource);

        d3d::ResourceViewArtifact view;
        view.id = d3d::ViewId{index};
        view.resource = d3d::ResourceId{index};
        view.viewClass = read ? d3d::ViewClass::ShaderResource :
            unorderedWrite ? d3d::ViewClass::UnorderedAccess : d3d::ViewClass::RenderTarget;
        view.format = textureFormat;
        view.firstMip = 0;
        view.mipCount = 1;
        view.firstArrayLayer = 0;
        view.arrayLayerCount = 1;
        view.firstPlane = 0;
        view.planeCount = 1;
        view.descriptorHeapClass = read || unorderedWrite ? 2u : 1u;
        view.descriptorIndex = read ? 0u : 0u;
        description.views.push_back(view);

        d3d::ExternalResourceSlotArtifact slot;
        slot.id = d3d::ExternalSlotId{index};
        slot.resource = d3d::ResourceId{index};
        slot.requiredKind = d3d::ResourceKind::Texture2D;
        slot.requiredFormat = textureFormat;
        slot.minimumBytes = 0;
        slot.requiredIncomingState = resource.initialState;
        slot.guaranteedOutgoingState = resource.initialState;
        description.externalSlots.push_back(slot);
    }

    description.operationStreams.push_back({d3d::OperationStreamKind::Load, 0, 1, 0});
    const std::uint32_t frameOperationCount = resourceCount * 3u + 1u;
    description.operationStreams.push_back({d3d::OperationStreamKind::Frame, 1, frameOperationCount, 0});
    description.operations.push_back(MakeOperation(d3d::D3D12OperationCode::CreateDescriptorHeaps));
    for (std::uint32_t index = 0; index < resourceCount; ++index)
    {
        description.operations.push_back(MakeOperation(
            d3d::D3D12OperationCode::AcquireExternal, {},
            d3d::Encode(d3d::AcquireExternalPayload{d3d::ExternalSlotId{index}})));
        description.operations.push_back(MakeOperation(
            d3d::D3D12OperationCode::WaitExternal, d3d::QueueId{0},
            d3d::Encode(d3d::WaitExternalPayload{d3d::ExternalSlotId{index}})));
    }
    description.operations.push_back(MakeOperation(
        d3d::D3D12OperationCode::SignalQueue, d3d::QueueId{0},
        d3d::Encode(d3d::SignalQueuePayload{d3d::SignalPointId{0}})));
    for (std::uint32_t index = 0; index < resourceCount; ++index)
        description.operations.push_back(MakeOperation(
            d3d::D3D12OperationCode::ReleaseExternal, {},
            d3d::Encode(d3d::ReleaseExternalPayload{
                d3d::ExternalSlotId{index}, d3d::SignalPointId{0}})));
    description.provenance = {std::byte{3}};

    auto bytes = d3d::BuildFrozenPackage(description);
    Require(static_cast<bool>(bytes), "Portable Texture Leaf Packageの生成に失敗しました。");
    auto package = package::PackageReader::Read(bytes.value());
    Require(static_cast<bool>(package), "Portable Texture Leaf Packageの読込に失敗しました。");
    auto decoded = d3d::D3D12PackageView::Decode(package.value());
    if (!decoded)
        throw std::runtime_error("Portable Texture Leaf PackageのSchema検証に失敗しました: " +
            decoded.error().message + " code=" + std::to_string(static_cast<int>(decoded.error().code)) +
            " record=" + std::to_string(decoded.error().recordIndex) +
            " operation=" + std::to_string(decoded.error().operationIndex));
    Require(decoded.value().ExternalSlots().size() == resourceCount,
        "Portable Texture Leaf PackageのEndpoint数が一致しません。");
    return std::move(bytes).value();
}

[[nodiscard]] composition::LeafPackageDeclaration MakeTextureProducerLeaf(
    std::string stableKey,
    const std::vector<std::byte>& packageBytes)
{
    composition::LeafPackageDeclaration leaf;
    leaf.stableKey = std::move(stableKey);
    leaf.packageBytes = packageBytes;
    leaf.endpoints = {{0, "portable/texture/output"}};
    return leaf;
}

[[nodiscard]] composition::LeafPackageDeclaration MakeTextureConsumerLeaf(
    std::string stableKey,
    const std::vector<std::byte>& packageBytes)
{
    composition::LeafPackageDeclaration leaf;
    leaf.stableKey = std::move(stableKey);
    leaf.packageBytes = packageBytes;
    leaf.endpoints = {
        {0, "portable/texture/input"},
        {1, "portable/texture/output"}};
    return leaf;
}

[[nodiscard]] composition::ContractBuildInput BuildPortableTextureCompositionInput(
    const std::vector<std::byte>& producerBytes,
    const std::vector<std::byte>& consumerBytes)
{
    composition::ContractBuildInput input;
    input.leaves = {
        MakeTextureProducerLeaf("portable/texture/producer", producerBytes),
        MakeTextureConsumerLeaf("portable/texture/consumer", consumerBytes)};

    composition::ResourceFlowDeclaration middle;
    middle.stableKey = "portable/texture/flow/middle";
    middle.boundary = composition::ResourceBoundary::Internal;
    middle.producer = {"portable/texture/producer", "portable/texture/output"};
    middle.consumers = {{"portable/texture/consumer", "portable/texture/input"}};

    composition::ResourceFlowDeclaration output;
    output.stableKey = "portable/texture/flow/output";
    output.boundary = composition::ResourceBoundary::CompositionOutput;
    output.producer = {"portable/texture/consumer", "portable/texture/output"};
    input.resources = {std::move(middle), std::move(output)};
    return input;
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

[[nodiscard]] composition::ContractBuildInput BuildPortableTemporalCompositionInput(
    const std::vector<std::byte>& packageBytes)
{
    auto input = BuildPortableCompositionInput(packageBytes);
    auto& temporal = input.resources.at(1);
    temporal.lifetime = composition::ResourceFlowLifetime::TemporalHistory;
    temporal.historyDepth = 1;
    return input;
}
}

void VerifyAbi2PortableRoundTrip()
{
    const auto leafBytes = BuildPortableLeafPackage();

    auto first = composition::BuildFrozenCompositionPackage(
        BuildPortableCompositionInput(leafBytes),
        composition::MakeAuthorityOnlyDynamicContractV1(8));
    auto second = composition::BuildFrozenCompositionPackage(
        BuildPortableCompositionInput(leafBytes),
        composition::MakeAuthorityOnlyDynamicContractV1(8));
    Require(static_cast<bool>(first) && static_cast<bool>(second),
        "Portable SGE4UNI 2.8の生成に失敗しました。");
    Require(first.value().FileBytes().size() == second.value().FileBytes().size() &&
        std::equal(first.value().FileBytes().begin(), first.value().FileBytes().end(),
            second.value().FileBytes().begin()),
        "Portable SGE4UNI 2.8がbyte決定的ではありません。");

    auto outer = ReadSectionedArtifact(
        first.value().FileBytes(), artifact::FrozenCompositionAbi2Magic,
        artifact::FrozenCompositionAbi2FormatMajor);
    Require(static_cast<bool>(outer) &&
        outer.value().FormatMinor() == artifact::FrozenCompositionAbi2FormatMinor &&
        outer.value().Sections().size() == artifact::FrozenCompositionAbi2SectionKinds.size(),
        "Portable SGE4UNI 2.8の平坦Section構造が一致しません。");

    const auto leaves = first.value().VerifiedComposition().ValidatedContract().Leaves();
    Require(leaves.size() == 2, "Portable CompositionのLeaf数が一致しません。");
    for (const auto& leaf : leaves)
        Require(leaf.packageBytes == leafBytes,
            "ABI 2.0でSchema 17 Leaf Package bytesが保存されませんでした。");

    auto roundTrip = composition::ReadFrozenCompositionPackage(first.value().FileBytes());
    Require(static_cast<bool>(roundTrip), "Portable SGE4UNI 2.8のRound-tripに失敗しました。");
    Require(roundTrip.value().FileDigest() == first.value().FileDigest() &&
        roundTrip.value().CompositionCoreDigest() == first.value().CompositionCoreDigest() &&
        roundTrip.value().SemanticDigest() == first.value().SemanticDigest(),
        "Portable SGE4UNI 2.8のDigestがRound-tripで変化しました。");

    auto legacyBytes = composition::migration::abi1::BuildFrozenCompositionPackageAbi1ForMigration(
        BuildPortableCompositionInput(leafBytes),
        composition::MakeAuthorityOnlyDynamicContractV1(8));
    Require(static_cast<bool>(legacyBytes), "Portable SGE4UNI 1.1 Corpusの生成に失敗しました。");
    Require(!composition::ReadFrozenCompositionPackage(legacyBytes.value()),
        "Production ReaderがPortable SGE4UNI 1.1を受理しました。");

    auto migrated = composition::migration::abi1::MigrateFrozenCompositionPackageAbi1ToAbi2(
        legacyBytes.value());
    Require(static_cast<bool>(migrated), "Portable ABI 1.1から2.7へのMigrationに失敗しました。");
    Require(migrated.value().FileBytes().size() == first.value().FileBytes().size() &&
        std::equal(migrated.value().FileBytes().begin(), migrated.value().FileBytes().end(),
            first.value().FileBytes().begin()),
        "直接生成とMigration後のPortable SGE4UNI 2.8がbyte一致しません。");
    Require(migrated.value().Certificate().contractIdentity == first.value().Certificate().contractIdentity &&
        migrated.value().Certificate().planIdentity == first.value().Certificate().planIdentity &&
        migrated.value().Certificate().sealIdentity == first.value().Certificate().sealIdentity &&
        migrated.value().Certificate().scheduleIdentity == first.value().Certificate().scheduleIdentity &&
        migrated.value().Certificate().recoverySetIdentity == first.value().Certificate().recoverySetIdentity,
        "ABI 1.1から2.7へのMigrationで権威Identityが保存されませんでした。");

    std::vector<composition::ConditionalRegionV1> conditionalRegions;
    conditionalRegions.push_back(composition::MakeConditionalRegionV1(
        0, composition::ConditionalPredicateKindV1::ActiveSetNonEmpty,
        {{0}, {1}}));
    auto conditional = composition::BuildFrozenCompositionPackage(
        BuildPortableCompositionInput(leafBytes),
        composition::MakeAuthorityOnlyDynamicContractV1(
            8, std::move(conditionalRegions)));
    Require(static_cast<bool>(conditional),
        "Portable Conditional Region Compositionの生成に失敗しました。");
    auto conditionalRoundTrip = composition::ReadFrozenCompositionPackage(
        conditional.value().FileBytes());
    Require(conditionalRoundTrip &&
        conditionalRoundTrip.value().DynamicContract().conditionalRegions.size() == 1 &&
        conditionalRoundTrip.value().DynamicContract().conditionalRegions[0].trueLeaves ==
            std::vector<composition::LeafPackageId>{{0}, {1}},
        "Portable Conditional Region契約のRound-tripに失敗しました。");

    const auto firstKey = composition::ComputeStableLeafKey("portable/first");
    const auto secondKey = composition::ComputeStableLeafKey("portable/second");
    const composition::LeafPackageId firstLeaf{firstKey < secondKey ? 0u : 1u};
    const composition::LeafPackageId secondLeaf{firstLeaf.value == 0u ? 1u : 0u};
    std::vector<composition::ConditionalRegionV1> crossBranch;
    crossBranch.push_back(composition::MakeConditionalRegionV1(
        0, composition::ConditionalPredicateKindV1::ActiveSetNonEmpty,
        {firstLeaf}, {secondLeaf}));
    Require(!composition::BuildFrozenCompositionPackage(
        BuildPortableCompositionInput(leafBytes),
        composition::MakeAuthorityOnlyDynamicContractV1(8, std::move(crossBranch))),
        "Portable Conditional branchを跨ぐFlowが受理されました。");

    auto temporal = composition::BuildFrozenCompositionPackage(
        BuildPortableTemporalCompositionInput(leafBytes),
        composition::MakeAuthorityOnlyDynamicContractV1(1));
    Require(static_cast<bool>(temporal),
        "Portable Temporal Buffer Compositionの生成に失敗しました。");
    const auto& temporalContract =
        temporal.value().VerifiedComposition().ValidatedContract().Contract();
    const auto temporalResource = std::ranges::find_if(
        temporalContract.resources, [](const auto& resource) {
            return resource.lifetime == composition::ResourceFlowLifetime::TemporalHistory;
        });
    Require(temporalContract.resources.size() == 3 &&
        temporalResource != temporalContract.resources.end() &&
        temporalResource->historyDepth == 1 &&
        temporalResource->boundary == composition::ResourceBoundary::Internal &&
        temporalResource->kind == d3d::ResourceKind::Buffer,
        "Portable Temporal Buffer契約がSGE4UNI 2.8へ固定されませんでした。");
    const auto temporalResourceId = temporalResource->id;
    const auto& temporalPlan =
        temporal.value().VerifiedComposition().VerifiedPlan().Plan();
    Require(temporalPlan.temporalBuffers.size() == 1 &&
        temporalPlan.temporalBuffers[0].resource == temporalResourceId &&
        temporalPlan.temporalBuffers[0].historyDepth == 1 &&
        temporalPlan.temporalBuffers[0].physicalInstanceCount == 2 &&
        temporalPlan.temporalBuffers[0].previousConsumers.size() == 1,
        "Portable Temporal Buffer Planが二世代へ固定されませんでした。");
    Require(std::ranges::none_of(temporalPlan.handoffs, [temporalResourceId](const auto& handoff) {
            return handoff.resource == temporalResourceId;
        }) &&
        std::ranges::none_of(temporalPlan.signals, [temporalResourceId](const auto& signal) {
            return signal.resource == temporalResourceId;
        }) &&
        std::ranges::none_of(temporalPlan.waits, [temporalResourceId](const auto& wait) {
            return wait.resource == temporalResourceId;
        }),
        "Temporal Bufferがsame-frame handoff／signal／waitへ混入しました。");
    auto temporalRoundTrip = composition::ReadFrozenCompositionPackage(
        temporal.value().FileBytes());
    Require(temporalRoundTrip &&
        temporalRoundTrip.value().SemanticDigest() == temporal.value().SemanticDigest() &&
        temporalRoundTrip.value().VerifiedComposition().VerifiedPlan().Plan().temporalBuffers.size() == 1,
        "Portable Temporal Buffer SGE4UNI 2.8のRound-tripに失敗しました。");
    auto invalidTemporal = BuildPortableTemporalCompositionInput(leafBytes);
    invalidTemporal.resources[1].historyDepth = 0;
    Require(!composition::BuildFrozenCompositionPackage(
        std::move(invalidTemporal), composition::MakeAuthorityOnlyDynamicContractV1(1)),
        "history depth 0のTemporal Bufferが受理されました。");

    Require(!composition::migration::abi1::BuildFrozenCompositionPackageAbi1ForMigration(
        BuildPortableTemporalCompositionInput(leafBytes),
        composition::MakeAuthorityOnlyDynamicContractV1(1)),
        "ABI 1.1移行Corpusが未表現のTemporal Buffer Flowを受理しました。");

    std::vector<composition::ConditionalRegionV1> temporalConditionalRegions;
    temporalConditionalRegions.push_back(composition::MakeConditionalRegionV1(
        0, composition::ConditionalPredicateKindV1::ActiveSetNonEmpty, {{0}}, {}));
    Require(!composition::BuildFrozenCompositionPackage(
        BuildPortableTemporalCompositionInput(leafBytes),
        composition::MakeAuthorityOnlyDynamicContractV1(
            1, std::move(temporalConditionalRegions))),
        "Conditional Temporal writer／readerが受理されました。");

    const auto textureProducer = BuildPortableTextureLeafPackage(false);
    const auto textureConsumer = BuildPortableTextureLeafPackage(true);
    auto textureFirst = composition::BuildFrozenCompositionPackage(
        BuildPortableTextureCompositionInput(textureProducer, textureConsumer),
        composition::MakeAuthorityOnlyDynamicContractV1(1));
    auto textureSecond = composition::BuildFrozenCompositionPackage(
        BuildPortableTextureCompositionInput(textureProducer, textureConsumer),
        composition::MakeAuthorityOnlyDynamicContractV1(1));
    Require(textureFirst && textureSecond,
        "Portable限定Texture2D Compositionの生成に失敗しました。");
    Require(textureFirst.value().FileBytes().size() == textureSecond.value().FileBytes().size() &&
        std::equal(textureFirst.value().FileBytes().begin(), textureFirst.value().FileBytes().end(),
            textureSecond.value().FileBytes().begin()),
        "Portable限定Texture2D Compositionがbyte決定的ではありません。");
    const auto& textureContract =
        textureFirst.value().VerifiedComposition().ValidatedContract().Contract();
    Require(textureContract.resources.size() == 2 &&
        std::ranges::all_of(textureContract.resources, [](const auto& resource) {
            return resource.kind == d3d::ResourceKind::Texture2D &&
                resource.format == d3d::Format::B8G8R8A8Unorm &&
                resource.sizeBytes == 0 && resource.texture2D.width == 4 &&
                resource.texture2D.height == 4 && resource.texture2D.rowBytes == 16 &&
                resource.texture2D.mipLevels == 1 && resource.texture2D.arrayLayers == 1 &&
                resource.texture2D.sampleCount == 1 && resource.texture2D.planeCount == 1;
        }),
        "Portable限定Texture2D shapeがContractへ固定されませんでした。");
    const auto& texturePlan = textureFirst.value().VerifiedComposition().VerifiedPlan().Plan();
    Require(texturePlan.allocations.size() == 2 &&
        std::ranges::all_of(texturePlan.allocations, [](const auto& allocation) {
            return allocation.kind == d3d::ResourceKind::Texture2D &&
                allocation.format == d3d::Format::B8G8R8A8Unorm &&
                allocation.sizeBytes == 64 && allocation.texture2D.width == 4 &&
                allocation.texture2D.height == 4;
        }),
        "Portable限定Texture2D allocationがPlanへ固定されませんでした。");
    auto textureRoundTrip = composition::ReadFrozenCompositionPackage(
        textureFirst.value().FileBytes());
    Require(textureRoundTrip &&
        textureRoundTrip.value().SemanticDigest() == textureFirst.value().SemanticDigest(),
        "Portable限定Texture2D SGE4UNI 2.8のRound-tripに失敗しました。");
    const auto textureUavProducer = BuildPortableTextureLeafPackage(false, 4, 4, true);
    const auto textureFloatConsumer = BuildPortableTextureLeafPackage(true, 4, 4, true);
    auto textureUav = composition::BuildFrozenCompositionPackage(
        BuildPortableTextureCompositionInput(textureUavProducer, textureFloatConsumer),
        composition::MakeAuthorityOnlyDynamicContractV1(1));
    Require(static_cast<bool>(textureUav),
        "Portable限定Texture2D UAV Compositionの生成に失敗しました。");
    const auto& textureUavContract =
        textureUav.value().VerifiedComposition().ValidatedContract().Contract();
    Require(std::ranges::any_of(textureUavContract.resources, [](const auto& resource) {
        return resource.kind == d3d::ResourceKind::Texture2D &&
            resource.format == d3d::Format::R32G32B32A32Float &&
            resource.texture2D.rowBytes == 64;
    }), "Portable RGBA32F Texture2D UAV shapeがContractへ固定されませんでした。");
    const auto& textureUavPlan = textureUav.value().VerifiedComposition().VerifiedPlan().Plan();
    Require(std::ranges::any_of(textureUavPlan.handoffs, [](const auto& handoff) {
        return handoff.producerOutgoingState.explicitBits ==
                std::to_underlying(d3d::ExplicitStateBits::UnorderedWrite) &&
            handoff.consumerIncomingState.explicitBits ==
                std::to_underlying(d3d::ExplicitStateBits::PixelShaderRead);
    }), "Portable Texture2D UAVからSRVへのstate handoffがPlanへ固定されませんでした。");
    auto textureUavRoundTrip = composition::ReadFrozenCompositionPackage(
        textureUav.value().FileBytes());
    Require(textureUavRoundTrip &&
        textureUavRoundTrip.value().SemanticDigest() == textureUav.value().SemanticDigest(),
        "Portable限定Texture2D UAV SGE4UNI 2.8のRound-tripに失敗しました。");

    auto temporalTextureInput =
        BuildPortableTextureCompositionInput(textureProducer, textureConsumer);
    auto temporalTexture = std::ranges::find_if(
        temporalTextureInput.resources, [](const auto& resource) {
            return resource.boundary == composition::ResourceBoundary::Internal;
        });
    Require(temporalTexture != temporalTextureInput.resources.end(),
        "Temporal Texture negative corpusのInternal Flowがありません。");
    temporalTexture->lifetime = composition::ResourceFlowLifetime::TemporalHistory;
    temporalTexture->historyDepth = 1;
    Require(!composition::BuildFrozenCompositionPackage(
        std::move(temporalTextureInput),
        composition::MakeAuthorityOnlyDynamicContractV1(1)),
        "Temporal Texture2D Flowが受理されました。");

    Require(!composition::migration::abi1::BuildFrozenCompositionPackageAbi1ForMigration(
        BuildPortableTextureCompositionInput(textureProducer, textureConsumer),
        composition::MakeAuthorityOnlyDynamicContractV1(1)),
        "ABI 1.1移行Corpusが未表現のTexture2D Flowを受理しました。");

    VerifyAbi2CorruptionRejection(first.value().FileBytes());
}
}
