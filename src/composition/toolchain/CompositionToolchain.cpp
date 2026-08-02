#include "CompositionToolchain.h"
#include "../planner/CompositionPlanner.h"

#include "../../canonical/base/BinaryIO.h"
#include "../../canonical/base/CheckedMath.h"
#include "../../leaf/artifact/package/PackageReader.h"
#include "../../backends/d3d12/artifact/D3D12Encoding.h"
#include "../model/plan/CompositionPlan.h"

#include <algorithm>
#include <limits>
#include <set>

namespace sge4::composition
{
namespace
{
using base::BinaryReader;
using base::BinaryWriter;

constexpr auto RequiredExecution =
    static_cast<std::uint16_t>(SectionFlags::Required) |
    static_cast<std::uint16_t>(SectionFlags::ExecutionAffecting);

template<class T>
[[nodiscard]] base::Expected<T, Error> Fail(std::string stage, std::string message)
{
    return base::Failure<T, Error>({std::move(stage), std::move(message)});
}

[[nodiscard]] bool IsValidPredicate(ConditionalPredicateKindV1 predicate) noexcept
{
    const auto value = std::to_underlying(predicate);
    return value >= std::to_underlying(ConditionalPredicateKindV1::ActiveSetNonEmpty) &&
        value <= std::to_underlying(ConditionalPredicateKindV1::TransitionSetNonEmpty);
}

[[nodiscard]] bool IsStrictlyIncreasingLeafList(
    std::span<const LeafPackageId> leaves,
    std::size_t leafCount) noexcept
{
    std::uint32_t previous = package::InvalidIndex;
    bool hasPrevious = false;
    for (const auto leaf : leaves)
    {
        if (!leaf.IsValid() || leaf.value >= leafCount ||
            (hasPrevious && leaf.value <= previous))
            return false;
        previous = leaf.value;
        hasPrevious = true;
    }
    return true;
}

// Generalization 2 is deliberately non-nested. A conditional producer may feed only
// consumers in the same region and same branch. An unconditional producer may feed
// either branch. This guarantees that every selected branch has all same-frame inputs
// without Runtime rediscovering graph validity.
[[nodiscard]] base::Expected<void, Error> ValidateConditionalRegions(
    const PackageCompositionContract& contract,
    std::span<const ConditionalRegionV1> regions)
{
    if (regions.size() > contract.leaves.size())
        return Fail<void>("CompositionToolchain/ConditionalRegion",
            "Conditional Region数がLeaf数を超えています。");

    // -1 = unconditional, otherwise region * 2 + branch (0=false, 1=true).
    std::vector<std::int64_t> membership(contract.leaves.size(), -1);
    for (std::size_t index = 0; index < regions.size(); ++index)
    {
        const auto& region = regions[index];
        if (!region.id.IsValid() || region.id.value != index ||
            !IsValidPredicate(region.predicate) ||
            (region.trueLeaves.empty() && region.falseLeaves.empty()) ||
            !IsStrictlyIncreasingLeafList(region.trueLeaves, contract.leaves.size()) ||
            !IsStrictlyIncreasingLeafList(region.falseLeaves, contract.leaves.size()))
            return Fail<void>("CompositionToolchain/ConditionalRegion",
                "Conditional RegionがCanonicalなID、predicateまたはLeaf集合に違反しています。");

        for (const auto leaf : region.falseLeaves)
        {
            if (membership[leaf.value] != -1)
                return Fail<void>("CompositionToolchain/ConditionalRegion",
                    "Leafを複数のConditional branchへ所属させることはできません。");
            membership[leaf.value] = static_cast<std::int64_t>(index * 2u);
        }
        for (const auto leaf : region.trueLeaves)
        {
            if (membership[leaf.value] != -1)
                return Fail<void>("CompositionToolchain/ConditionalRegion",
                    "Leafを複数のConditional branchへ所属させることはできません。");
            membership[leaf.value] = static_cast<std::int64_t>(index * 2u + 1u);
        }
    }

    if (contract.presenterLeaf.IsValid() &&
        contract.presenterLeaf.value < membership.size() &&
        membership[contract.presenterLeaf.value] != -1)
        return Fail<void>("CompositionToolchain/ConditionalRegion",
            "Presenter LeafはGeneralization 2でConditionalにできません。");

    for (const auto& resource : contract.resources)
    {
        if (!resource.producer.IsValid())
            continue;
        if (resource.producer.value >= contract.endpoints.size())
            return Fail<void>("CompositionToolchain/ConditionalRegion",
                "Conditional flowのproducer Endpointが範囲外です。");
        const auto producerLeaf = contract.endpoints[resource.producer.value].leaf;
        if (!producerLeaf.IsValid() || producerLeaf.value >= membership.size())
            return Fail<void>("CompositionToolchain/ConditionalRegion",
                "Conditional flowのproducer Leafが範囲外です。");
        const auto producerMembership = membership[producerLeaf.value];
        if (resource.lifetime == ResourceFlowLifetime::TemporalHistory &&
            producerMembership != -1)
            return Fail<void>("CompositionToolchain/TemporalBuffer",
                "Temporal BufferのCurrent writerは初期版でunconditional Leafに限定されます。");

        for (const auto consumerEndpoint : resource.consumers)
        {
            if (!consumerEndpoint.IsValid() ||
                consumerEndpoint.value >= contract.endpoints.size())
                return Fail<void>("CompositionToolchain/ConditionalRegion",
                    "Conditional flowのconsumer Endpointが範囲外です。");
            const auto consumerLeaf = contract.endpoints[consumerEndpoint.value].leaf;
            if (!consumerLeaf.IsValid() || consumerLeaf.value >= membership.size())
                return Fail<void>("CompositionToolchain/ConditionalRegion",
                    "Conditional flowのconsumer Leafが範囲外です。");
            const auto consumerMembership = membership[consumerLeaf.value];
            if (resource.lifetime == ResourceFlowLifetime::TemporalHistory &&
                consumerMembership != -1)
                return Fail<void>("CompositionToolchain/TemporalBuffer",
                    "Temporal BufferのPrevious readerは初期版でunconditional Leafに限定されます。");

            if (producerMembership != -1 && consumerMembership == -1)
                return Fail<void>("CompositionToolchain/ConditionalRegion",
                    "Unconditional LeafはConditional producerへ依存できません。");
            if (producerMembership != -1 && consumerMembership != -1 &&
                producerMembership != consumerMembership)
                return Fail<void>("CompositionToolchain/ConditionalRegion",
                    "Conditional branchを跨ぐResource Flowは許可されません。");
        }
    }
    return base::Success<void, Error>();
}


void WriteDynamicRoutes(
    BinaryWriter& writer,
    std::span<const DynamicExecutionRouteV1> routes)
{
    writer.WriteCountU32(routes.size());
    for (const auto& route : routes)
    {
        writer.WriteU32(route.targetLeaf.value);
        writer.WriteU32(route.targetDynamicSlot);
        writer.WriteU32(route.sourceByteOffset);
        writer.WriteU32(route.routeMemberBytes);
    }
}

[[nodiscard]] base::Expected<std::vector<DynamicExecutionRouteV1>, Error>
ReadDynamicRoutes(BinaryReader& reader, std::uint32_t leafCount)
{
    auto routeCount = reader.ReadU32();
    if (!routeCount || static_cast<std::uint64_t>(routeCount.value()) >
        static_cast<std::uint64_t>(leafCount) * 64u)
        return Fail<std::vector<DynamicExecutionRouteV1>>(
            "CompositionReader/DynamicContract", "Dynamic route数が無効です。");

    std::vector<DynamicExecutionRouteV1> routes;
    routes.reserve(routeCount.value());
    for (std::uint32_t index = 0; index < routeCount.value(); ++index)
    {
        auto leaf = reader.ReadU32();
        auto slot = reader.ReadU32();
        auto sourceOffset = reader.ReadU32();
        auto memberBytes = reader.ReadU32();
        if (!leaf || !slot || !sourceOffset || !memberBytes)
            return Fail<std::vector<DynamicExecutionRouteV1>>(
                "CompositionReader/DynamicContract", "Dynamic route headerが無効です。");
        routes.push_back({LeafPackageId{leaf.value()}, slot.value(),
            sourceOffset.value(), memberBytes.value()});
    }
    return base::Success<std::vector<DynamicExecutionRouteV1>, Error>(std::move(routes));
}

void WriteConditionalRegions(
    BinaryWriter& writer,
    std::span<const ConditionalRegionV1> regions)
{
    writer.WriteCountU32(regions.size());
    for (const auto& region : regions)
    {
        writer.WriteU32(region.id.value);
        writer.WriteU32(std::to_underlying(region.predicate));
        writer.WriteCountU32(region.trueLeaves.size());
        writer.WriteCountU32(region.falseLeaves.size());
        for (const auto leaf : region.trueLeaves) writer.WriteU32(leaf.value);
        for (const auto leaf : region.falseLeaves) writer.WriteU32(leaf.value);
    }
}

[[nodiscard]] base::Expected<std::vector<ConditionalRegionV1>, Error>
ReadConditionalRegions(BinaryReader& reader, std::uint32_t leafCount)
{
    auto regionCount = reader.ReadU32();
    if (!regionCount || regionCount.value() > leafCount)
        return Fail<std::vector<ConditionalRegionV1>>(
            "CompositionReader/DynamicContract", "Conditional Region数が無効です。");

    std::vector<ConditionalRegionV1> regions;
    regions.reserve(regionCount.value());
    for (std::uint32_t index = 0; index < regionCount.value(); ++index)
    {
        auto id = reader.ReadU32();
        auto predicate = reader.ReadU32();
        auto trueCount = reader.ReadU32();
        auto falseCount = reader.ReadU32();
        if (!id || !predicate || !trueCount || !falseCount ||
            trueCount.value() > leafCount || falseCount.value() > leafCount ||
            static_cast<std::uint64_t>(trueCount.value()) + falseCount.value() > leafCount)
            return Fail<std::vector<ConditionalRegionV1>>(
                "CompositionReader/DynamicContract", "Conditional Region headerが無効です。");

        ConditionalRegionV1 region;
        region.id = ConditionalRegionId{id.value()};
        region.predicate = static_cast<ConditionalPredicateKindV1>(predicate.value());
        region.trueLeaves.reserve(trueCount.value());
        region.falseLeaves.reserve(falseCount.value());
        for (std::uint32_t leafIndex = 0; leafIndex < trueCount.value(); ++leafIndex)
        {
            auto leaf = reader.ReadU32();
            if (!leaf)
                return Fail<std::vector<ConditionalRegionV1>>(
                    "CompositionReader/DynamicContract", leaf.error());
            region.trueLeaves.push_back(LeafPackageId{leaf.value()});
        }
        for (std::uint32_t leafIndex = 0; leafIndex < falseCount.value(); ++leafIndex)
        {
            auto leaf = reader.ReadU32();
            if (!leaf)
                return Fail<std::vector<ConditionalRegionV1>>(
                    "CompositionReader/DynamicContract", leaf.error());
            region.falseLeaves.push_back(LeafPackageId{leaf.value()});
        }
        regions.push_back(std::move(region));
    }
    return base::Success<std::vector<ConditionalRegionV1>, Error>(std::move(regions));
}

[[nodiscard]] core::SemanticIdentity BuildDynamicSemanticIdentity(
    const Digest256& compositionCoreDigest,
    const CompositionCertificate& certificate,
    const DynamicContractV1& dynamicContract)
{
    BinaryWriter payload;
    payload.WriteBytes(compositionCoreDigest);
    payload.WriteBytes(certificate.artifactIdentity.Digest());
    payload.WriteU32(dynamicContract.schemaVersion);
    payload.WriteU32(dynamicContract.universeCount);
    payload.WriteU32(std::to_underlying(dynamicContract.executionMode));
    payload.WriteU32(dynamicContract.canonicalMemberBytes);
    WriteDynamicRoutes(payload, dynamicContract.executionRoutes);
    WriteConditionalRegions(payload, dynamicContract.conditionalRegions);
    payload.WriteU32(std::to_underlying(dynamicContract.indirectDispatch.mode));
    payload.WriteU32(dynamicContract.indirectDispatch.targetLeaf.value);
    payload.WriteU32(dynamicContract.indirectDispatch.targetComputeCommand);
    payload.WriteU32(dynamicContract.indirectDispatch.maxWorkCount);
    payload.WriteU32(std::to_underlying(dynamicContract.indirectDispatch.compactWorklistMode));
    payload.WriteU32(dynamicContract.indirectDispatch.targetIndexListDynamicSlot);
    return core::SemanticIdentity::FromDigest(
        ComputeDomainDigest("sge4.composition.dynamic-semantic", 6, payload.Bytes()));
}

[[nodiscard]] std::vector<std::byte> BuildAuthorityLedger(
    const Digest256& compositionCoreDigest,
    const CompositionCertificate& certificate)
{
    BinaryWriter writer;
    writer.WriteU32(certificate.schemaVersion);
    writer.WriteBytes(compositionCoreDigest);
    writer.WriteBytes(certificate.artifactIdentity.Digest());
    writer.WriteBytes(certificate.contractIdentity.Digest());
    writer.WriteBytes(certificate.planIdentity.Digest());
    writer.WriteBytes(certificate.sealIdentity.Digest());
    writer.WriteBytes(certificate.scheduleIdentity.Digest());
    writer.WriteBytes(certificate.recoverySetIdentity.Digest());
    writer.WriteU32(certificate.leafCount);
    writer.WriteU32(certificate.flowCount);
    return std::move(writer).Take();
}

[[nodiscard]] std::vector<std::byte> BuildDynamicContractBytes(
    const DynamicContractV1& dynamicContract,
    core::SemanticIdentity semanticIdentity,
    FrozenCompositionIdentity compositionIdentity)
{
    BinaryWriter writer;
    writer.WriteU32(dynamicContract.schemaVersion);
    writer.WriteU32(dynamicContract.universeCount);
    writer.WriteU32(std::to_underlying(dynamicContract.executionMode));
    writer.WriteU32(dynamicContract.canonicalMemberBytes);
    WriteDynamicRoutes(writer, dynamicContract.executionRoutes);
    WriteConditionalRegions(writer, dynamicContract.conditionalRegions);
    writer.WriteU32(std::to_underlying(dynamicContract.indirectDispatch.mode));
    writer.WriteU32(dynamicContract.indirectDispatch.targetLeaf.value);
    writer.WriteU32(dynamicContract.indirectDispatch.targetComputeCommand);
    writer.WriteU32(dynamicContract.indirectDispatch.maxWorkCount);
    writer.WriteU32(std::to_underlying(dynamicContract.indirectDispatch.compactWorklistMode));
    writer.WriteU32(dynamicContract.indirectDispatch.targetIndexListDynamicSlot);
    writer.WriteBytes(compositionIdentity.Digest());
    writer.WriteBytes(semanticIdentity.Digest());
    return std::move(writer).Take();
}

[[nodiscard]] base::Expected<Digest256, Error> ReadDigest(
    BinaryReader& reader,
    std::string_view stage)
{
    auto bytes = reader.ReadBytes(32);
    if (!bytes) return Fail<Digest256>(std::string(stage), bytes.error());
    Digest256 digest{};
    std::copy(bytes.value().begin(), bytes.value().end(), digest.begin());
    return base::Success<Digest256, Error>(digest);
}

[[nodiscard]] bool SameIdentity(
    const Digest256& left,
    const auto& right) noexcept
{
    return left == right.Digest();
}

[[nodiscard]] base::Expected<void, Error> ValidateDynamicContract(
    const ValidatedCompositionContract& validated,
    const DynamicContractV1& dynamicContract)
{
    const auto& contract = validated.Contract();
    if (dynamicContract.schemaVersion != 6 || dynamicContract.universeCount == 0)
        return Fail<void>(
            "CompositionToolchain", "Dynamic Contractが検証または実行の契約に違反しています。");

    auto conditionalValid = ValidateConditionalRegions(
        contract, dynamicContract.conditionalRegions);
    if (!conditionalValid)
        return conditionalValid;

    if (dynamicContract.executionMode == DynamicExecutionModeV1::AuthorityOnly)
    {
        if (dynamicContract.canonicalMemberBytes != 0 ||
            !dynamicContract.executionRoutes.empty())
            return Fail<void>(
                "CompositionToolchain", "Authority-only Dynamic Contractに実行routeが混入しています。");
    }
    else if (dynamicContract.executionMode == DynamicExecutionModeV1::VerifiedDenseSlot)
    {
        if (dynamicContract.canonicalMemberBytes == 0 ||
            dynamicContract.executionRoutes.empty())
            return Fail<void>(
                "CompositionToolchain", "Verified Dynamic Execution routeが無効です。");

        std::pair<std::uint32_t, std::uint32_t> previous{};
        bool hasPrevious = false;
        for (const auto& route : dynamicContract.executionRoutes)
        {
            const std::pair key{route.targetLeaf.value, route.targetDynamicSlot};
            if (!route.targetLeaf.IsValid() ||
                route.targetLeaf.value >= validated.Leaves().size() ||
                route.targetDynamicSlot == package::InvalidIndex ||
                route.routeMemberBytes == 0 ||
                route.sourceByteOffset > dynamicContract.canonicalMemberBytes ||
                route.routeMemberBytes >
                    dynamicContract.canonicalMemberBytes - route.sourceByteOffset ||
                (hasPrevious && key <= previous))
                return Fail<void>(
                    "CompositionToolchain/DynamicRoute",
                    "Dynamic routeがCanonicalな対象順序またはsource slice契約に違反しています。");
            previous = key;
            hasPrevious = true;

            const auto& leaf = validated.Leaves()[route.targetLeaf.value];
            auto frozen = package::PackageReader::Read(leaf.packageBytes);
            if (!frozen)
                return Fail<void>("CompositionToolchain/DynamicRoute", frozen.error().message);
            auto view = package::d3d12_v13::D3D12PackageView::Decode(frozen.value());
            if (!view)
                return Fail<void>("CompositionToolchain/DynamicRoute", view.error().message);
            if (route.targetDynamicSlot >= view.value().DynamicSlots().size())
                return Fail<void>(
                    "CompositionToolchain/DynamicRoute", "Dynamic Slot参照が範囲外です。");

            if (dynamicContract.universeCount >
                std::numeric_limits<std::uint64_t>::max() / route.routeMemberBytes)
                return Fail<void>(
                    "CompositionToolchain/DynamicRoute", "Dynamic dense slotのbyte数がoverflowします。");
            const auto requiredBytes =
                static_cast<std::uint64_t>(dynamicContract.universeCount) *
                route.routeMemberBytes;
            const auto& slot = view.value().DynamicSlots()[route.targetDynamicSlot];
            if (slot.requiredBytes != requiredBytes)
                return Fail<void>(
                    "CompositionToolchain/DynamicRoute",
                    "Dynamic SlotのrequiredBytesとroute member universeが一致しません。");
        }
    }
    else
    {
        return Fail<void>(
            "CompositionToolchain", "未対応のDynamic execution modeです。");
    }

    const auto& indirect = dynamicContract.indirectDispatch;
    if (indirect.mode == IndirectExecutionModeV1::None)
    {
        if (indirect.targetLeaf.IsValid() ||
            indirect.targetComputeCommand != package::InvalidIndex ||
            indirect.maxWorkCount != 0 ||
            indirect.compactWorklistMode != CompactWorklistModeV1::None ||
            indirect.targetIndexListDynamicSlot != package::InvalidIndex)
            return Fail<void>("CompositionToolchain/IndirectDispatch",
                "Indirect無効契約へrouteが混入しています。");
        return base::Success<void, Error>();
    }
    if (indirect.mode != IndirectExecutionModeV1::VerifiedDispatch ||
        !indirect.targetLeaf.IsValid() ||
        indirect.targetLeaf.value >= validated.Leaves().size() ||
        indirect.targetComputeCommand == package::InvalidIndex ||
        indirect.maxWorkCount == 0 ||
        indirect.maxWorkCount != dynamicContract.universeCount)
        return Fail<void>("CompositionToolchain/IndirectDispatch",
            "Verified Indirect Dispatch契約が無効です。");

    for (const auto& region : dynamicContract.conditionalRegions)
    {
        const auto inTrue = std::binary_search(
            region.trueLeaves.begin(), region.trueLeaves.end(), indirect.targetLeaf,
            [](const auto& left, const auto& right) { return left.value < right.value; });
        const auto inFalse = std::binary_search(
            region.falseLeaves.begin(), region.falseLeaves.end(), indirect.targetLeaf,
            [](const auto& left, const auto& right) { return left.value < right.value; });
        if (inTrue || inFalse)
            return Fail<void>("CompositionToolchain/IndirectDispatch",
                "Generalization 4のIndirect targetはConditional Leafにできません。");
    }

    const auto& indirectLeaf = validated.Leaves()[indirect.targetLeaf.value];
    auto indirectFrozen = package::PackageReader::Read(indirectLeaf.packageBytes);
    if (!indirectFrozen)
        return Fail<void>("CompositionToolchain/IndirectDispatch", indirectFrozen.error().message);
    auto indirectView = package::d3d12_v13::D3D12PackageView::Decode(indirectFrozen.value());
    if (!indirectView)
        return Fail<void>("CompositionToolchain/IndirectDispatch", indirectView.error().message);
    if (indirect.targetComputeCommand >= indirectView.value().ComputeCommands().size())
        return Fail<void>("CompositionToolchain/IndirectDispatch",
            "Indirect target Compute Commandが範囲外です。");
    const auto& command = indirectView.value().ComputeCommands()[indirect.targetComputeCommand];
    if (command.id.value != indirect.targetComputeCommand ||
        command.threadGroupCountX != indirect.maxWorkCount ||
        command.threadGroupCountY != 1 || command.threadGroupCountZ != 1 ||
        command.flags != 0)
        return Fail<void>("CompositionToolchain/IndirectDispatch",
            "Indirect target Compute Commandが固定上限契約と一致しません。");

    std::uint32_t executeCount = 0;
    for (const auto& operation : indirectView.value().FrameOperations())
    {
        if (operation.opcode != package::d3d12_v13::D3D12OperationCode::ExecuteCompute)
            continue;
        auto payload = package::d3d12_v13::DecodeExecuteCompute(operation.payload);
        if (!payload)
            return Fail<void>("CompositionToolchain/IndirectDispatch", payload.error().message);
        if (payload.value().command.value == indirect.targetComputeCommand)
            ++executeCount;
    }
    if (executeCount != 1)
        return Fail<void>("CompositionToolchain/IndirectDispatch",
            "Indirect target Compute CommandはFrame Operation内で一度だけ実行される必要があります。");

    if (indirect.compactWorklistMode == CompactWorklistModeV1::None)
    {
        if (indirect.targetIndexListDynamicSlot != package::InvalidIndex)
            return Fail<void>("CompositionToolchain/CompactWorklist",
                "Compact worklistなしの契約へDynamic Slotが混入しています。");
        return base::Success<void, Error>();
    }
    if (indirect.compactWorklistMode != CompactWorklistModeV1::VerifiedU32 ||
        dynamicContract.executionMode != DynamicExecutionModeV1::VerifiedDenseSlot ||
        indirect.targetIndexListDynamicSlot == package::InvalidIndex ||
        indirect.targetIndexListDynamicSlot >= indirectView.value().DynamicSlots().size())
        return Fail<void>("CompositionToolchain/CompactWorklist",
            "Verified compact worklist契約が無効です。");

    const auto& worklistSlot =
        indirectView.value().DynamicSlots()[indirect.targetIndexListDynamicSlot];
    const auto requiredWorklistBytes =
        static_cast<std::uint64_t>(indirect.maxWorkCount) * sizeof(std::uint32_t);
    if (worklistSlot.requiredBytes != requiredWorklistBytes)
        return Fail<void>("CompositionToolchain/CompactWorklist",
            "Compact worklist Dynamic SlotのrequiredBytesが固定上限契約と一致しません。");
    for (const auto& route : dynamicContract.executionRoutes)
    {
        if (route.targetLeaf == indirect.targetLeaf &&
            route.targetDynamicSlot == indirect.targetIndexListDynamicSlot)
            return Fail<void>("CompositionToolchain/CompactWorklist",
                "Compact worklist Slotをdense execution routeと共有できません。");
    }
    return base::Success<void, Error>();
}
}

base::Expected<FrozenCompositionPackage, Error> BuildFrozenCompositionPackage(
    ContractBuildInput input,
    DynamicContractV1 dynamicContract)
{
    auto contract = BuildCompositionContract(std::move(input));
    if (!contract)
        return Fail<FrozenCompositionPackage>(contract.error().stage, contract.error().message);
    auto dynamicValid = ValidateDynamicContract(contract.value(), dynamicContract);
    if (!dynamicValid)
        return Fail<FrozenCompositionPackage>(dynamicValid.error().stage, dynamicValid.error().message);
    auto proposal = planning::ProposeCompositionPlan(contract.value());
    if (!proposal)
        return Fail<FrozenCompositionPackage>(proposal.error().stage, proposal.error().message);
    auto verified = verification::VerifyAndSeal(contract.value(), proposal.value());
    if (!verified)
        return Fail<FrozenCompositionPackage>(verified.error().stage, verified.error().message);
    return FreezeVerifiedCompositionPackage(contract.value(), verified.value(), std::move(dynamicContract));
}

base::Expected<FrozenCompositionPackage, Error> FreezeVerifiedCompositionPackage(
    const ValidatedCompositionContract& contract,
    const verification::VerifiedCompositionPlan& verified,
    DynamicContractV1 dynamicContract)
{
    auto dynamicValid = ValidateDynamicContract(contract, dynamicContract);
    if (!dynamicValid)
        return Fail<FrozenCompositionPackage>(dynamicValid.error().stage, dynamicValid.error().message);

    auto coreResult = artifact::BuildFrozenCompositionAbi2Core(contract, verified);
    if (!coreResult)
        return Fail<FrozenCompositionPackage>(
            coreResult.error().stage, coreResult.error().message);
    auto core = std::move(coreResult).value();

    const auto certificate = BuildCompositionCertificate(
        contract, verified, core.coreDigest);
    const auto dynamicIdentity = BuildDynamicSemanticIdentity(
        core.coreDigest, certificate, dynamicContract);

    artifact::FrozenCompositionAbi2Manifest manifest;
    manifest.dynamicUniverseCount = dynamicContract.universeCount;
    manifest.leafCount = core.leafCount;
    manifest.flowCount = core.flowCount;
    manifest.presenterLeafId = core.presenterLeafId;
    manifest.leafBytes = core.leafBytes;
    manifest.contractBytes = core.contractBytes;
    manifest.verifiedDecisionBytes = core.verifiedDecisionBytes;
    manifest.verificationCertificateBytes = core.verificationCertificateBytes;
    manifest.compositionCoreDigest = core.coreDigest;
    manifest.compositionArtifactIdentity = certificate.artifactIdentity.Digest();
    manifest.dynamicSemanticIdentity = dynamicIdentity.Digest();

    std::vector<SectionInput> sections;
    sections.reserve(artifact::FrozenCompositionAbi2SectionKinds.size());
    sections.push_back({
        std::to_underlying(artifact::FrozenCompositionAbi2SectionKind::Manifest),
        artifact::FrozenCompositionAbi2ManifestSchema,
        RequiredExecution, artifact::FrozenCompositionAbi2Alignment,
        artifact::SerializeFrozenCompositionAbi2Manifest(manifest)});
    for (auto& section : core.sections) sections.push_back(std::move(section));
    sections.push_back({
        std::to_underlying(artifact::FrozenCompositionAbi2SectionKind::AuthorityLedger),
        2, RequiredExecution, artifact::FrozenCompositionAbi2Alignment,
        BuildAuthorityLedger(core.coreDigest, certificate)});
    sections.push_back({
        std::to_underlying(artifact::FrozenCompositionAbi2SectionKind::DynamicContract),
        artifact::FrozenCompositionAbi2DynamicContractSchema, RequiredExecution,
        artifact::FrozenCompositionAbi2Alignment,
        BuildDynamicContractBytes(dynamicContract, dynamicIdentity, certificate.artifactIdentity)});

    auto outer = WriteSectionedArtifact(
        artifact::FrozenCompositionAbi2Magic,
        artifact::FrozenCompositionAbi2FormatMajor,
        artifact::FrozenCompositionAbi2FormatMinor,
        std::move(sections));
    if (!outer)
        return Fail<FrozenCompositionPackage>(outer.error().stage, outer.error().message);
    return ReadFrozenCompositionPackage(std::move(outer).value());
}

base::Expected<FrozenCompositionPackage, Error> ReadFrozenCompositionPackage(
    std::span<const std::byte> bytes)
{
    return ReadFrozenCompositionPackage(std::vector<std::byte>(bytes.begin(), bytes.end()));
}

base::Expected<FrozenCompositionPackage, Error> ReadFrozenCompositionPackage(
    std::vector<std::byte> bytes)
{
    auto outer = ReadSectionedArtifact(
        bytes, artifact::FrozenCompositionAbi2Magic,
        artifact::FrozenCompositionAbi2FormatMajor);
    if (!outer)
        return Fail<FrozenCompositionPackage>(outer.error().stage, outer.error().message);
    if (outer.value().FormatMinor() != artifact::FrozenCompositionAbi2FormatMinor)
        return Fail<FrozenCompositionPackage>(
            "CompositionReader", "SGE4UNIのMinor versionが未対応です。");

    auto verifiedResult = artifact::ReadVerifiedFrozenComposition(bytes);
    if (!verifiedResult)
        return Fail<FrozenCompositionPackage>(
            verifiedResult.error().stage, verifiedResult.error().message);
    auto verified = std::move(verifiedResult).value();

    const auto* manifestSection = outer.value().FindSection(
        std::to_underlying(artifact::FrozenCompositionAbi2SectionKind::Manifest));
    const auto* authoritySection = outer.value().FindSection(
        std::to_underlying(artifact::FrozenCompositionAbi2SectionKind::AuthorityLedger));
    const auto* dynamicSection = outer.value().FindSection(
        std::to_underlying(artifact::FrozenCompositionAbi2SectionKind::DynamicContract));
    if (!manifestSection || !authoritySection || !dynamicSection)
        return Fail<FrozenCompositionPackage>(
            "CompositionReader", "SGE4UNI 2.8の必須Sectionがありません。");

    auto manifestResult = artifact::DeserializeFrozenCompositionAbi2Manifest(
        manifestSection->bytes);
    if (!manifestResult)
        return Fail<FrozenCompositionPackage>(
            manifestResult.error().stage, manifestResult.error().message);
    const auto manifest = manifestResult.value();

    const auto certificate = BuildCompositionCertificate(verified);
    if (manifest.compositionCoreDigest != verified.CoreDigest() ||
        !SameIdentity(manifest.compositionArtifactIdentity, certificate.artifactIdentity) ||
        manifest.leafCount != certificate.leafCount ||
        manifest.flowCount != certificate.flowCount)
        return Fail<FrozenCompositionPackage>(
            "CompositionReader", "Manifestと検証済みComposition authorityが一致しません。");

    BinaryReader ledger(authoritySection->bytes);
    auto ledgerSchema = ledger.ReadU32();
    auto coreDigest = ReadDigest(ledger, "CompositionReader/AuthorityLedger");
    auto artifactIdentity = ReadDigest(ledger, "CompositionReader/AuthorityLedger");
    auto contractIdentity = ReadDigest(ledger, "CompositionReader/AuthorityLedger");
    auto planIdentity = ReadDigest(ledger, "CompositionReader/AuthorityLedger");
    auto sealIdentity = ReadDigest(ledger, "CompositionReader/AuthorityLedger");
    auto scheduleIdentity = ReadDigest(ledger, "CompositionReader/AuthorityLedger");
    auto recoveryIdentity = ReadDigest(ledger, "CompositionReader/AuthorityLedger");
    auto leafCount = ledger.ReadU32();
    auto flowCount = ledger.ReadU32();
    if (!ledgerSchema || !coreDigest || !artifactIdentity || !contractIdentity ||
        !planIdentity || !sealIdentity || !scheduleIdentity || !recoveryIdentity ||
        !leafCount || !flowCount || ledger.Remaining() != 0 ||
        ledgerSchema.value() != certificate.schemaVersion ||
        coreDigest.value() != verified.CoreDigest() ||
        !SameIdentity(artifactIdentity.value(), certificate.artifactIdentity) ||
        !SameIdentity(contractIdentity.value(), certificate.contractIdentity) ||
        !SameIdentity(planIdentity.value(), certificate.planIdentity) ||
        !SameIdentity(sealIdentity.value(), certificate.sealIdentity) ||
        !SameIdentity(scheduleIdentity.value(), certificate.scheduleIdentity) ||
        !SameIdentity(recoveryIdentity.value(), certificate.recoverySetIdentity) ||
        leafCount.value() != certificate.leafCount ||
        flowCount.value() != certificate.flowCount)
        return Fail<FrozenCompositionPackage>(
            "CompositionReader", "Authority Ledgerが検証済みCompositionと一致しません。");

    BinaryReader dynamic(dynamicSection->bytes);
    auto dynamicSchema = dynamic.ReadU32();
    auto dynamicUniverse = dynamic.ReadU32();
    auto dynamicExecutionMode = dynamic.ReadU32();
    auto dynamicCanonicalMemberBytes = dynamic.ReadU32();
    if (!dynamicSchema || !dynamicUniverse || !dynamicExecutionMode ||
        !dynamicCanonicalMemberBytes || dynamicSchema.value() != 6 ||
        dynamicUniverse.value() == 0 || dynamicExecutionMode.value() >
            std::to_underlying(DynamicExecutionModeV1::VerifiedDenseSlot))
        return Fail<FrozenCompositionPackage>(
            "CompositionReader", "Dynamic Contract headerが無効です。");

    auto executionRoutes = ReadDynamicRoutes(dynamic, certificate.leafCount);
    if (!executionRoutes)
        return Fail<FrozenCompositionPackage>(
            executionRoutes.error().stage, executionRoutes.error().message);
    auto conditionalRegions = ReadConditionalRegions(dynamic, certificate.leafCount);
    if (!conditionalRegions)
        return Fail<FrozenCompositionPackage>(
            conditionalRegions.error().stage, conditionalRegions.error().message);
    auto indirectMode = dynamic.ReadU32();
    auto indirectTargetLeaf = dynamic.ReadU32();
    auto indirectTargetCommand = dynamic.ReadU32();
    auto indirectMaxWorkCount = dynamic.ReadU32();
    auto compactWorklistMode = dynamic.ReadU32();
    auto targetIndexListDynamicSlot = dynamic.ReadU32();
    if (!indirectMode || !indirectTargetLeaf || !indirectTargetCommand ||
        !indirectMaxWorkCount || !compactWorklistMode ||
        !targetIndexListDynamicSlot || indirectMode.value() >
            std::to_underlying(IndirectExecutionModeV1::VerifiedDispatch) ||
        compactWorklistMode.value() >
            std::to_underlying(CompactWorklistModeV1::VerifiedU32))
        return Fail<FrozenCompositionPackage>(
            "CompositionReader", "Indirect Dispatch Contract headerが無効です。");
    auto dynamicCompositionIdentity = ReadDigest(dynamic, "CompositionReader/DynamicContract");
    auto dynamicSemanticIdentity = ReadDigest(dynamic, "CompositionReader/DynamicContract");
    if (!dynamicCompositionIdentity || !dynamicSemanticIdentity || dynamic.Remaining() != 0 ||
        !SameIdentity(dynamicCompositionIdentity.value(), certificate.artifactIdentity))
        return Fail<FrozenCompositionPackage>(
            "CompositionReader", "Dynamic ContractがComposition identityと一致しません。");

    DynamicContractV1 dynamicContract{
        dynamicSchema.value(), dynamicUniverse.value(),
        static_cast<DynamicExecutionModeV1>(dynamicExecutionMode.value()),
        dynamicCanonicalMemberBytes.value(), std::move(executionRoutes).value(),
        std::move(conditionalRegions).value(),
        {static_cast<IndirectExecutionModeV1>(indirectMode.value()),
            LeafPackageId{indirectTargetLeaf.value()}, indirectTargetCommand.value(),
            indirectMaxWorkCount.value(),
            static_cast<CompactWorklistModeV1>(compactWorklistMode.value()),
            targetIndexListDynamicSlot.value()}};
    auto dynamicValid = ValidateDynamicContract(verified.ValidatedContract(), dynamicContract);
    if (!dynamicValid)
        return Fail<FrozenCompositionPackage>(dynamicValid.error().stage, dynamicValid.error().message);
    const auto derivedDynamicIdentity = BuildDynamicSemanticIdentity(
        verified.CoreDigest(), certificate, dynamicContract);
    if (!SameIdentity(dynamicSemanticIdentity.value(), derivedDynamicIdentity) ||
        manifest.dynamicUniverseCount != dynamicContract.universeCount ||
        manifest.dynamicSemanticIdentity != derivedDynamicIdentity.Digest())
        return Fail<FrozenCompositionPackage>(
            "CompositionReader", "Dynamic Semantic identityがCanonical導出値と一致しません。");

    return base::Success<FrozenCompositionPackage, Error>(FrozenCompositionPackage(
        std::move(bytes), std::move(verified), certificate, derivedDynamicIdentity,
        std::move(dynamicContract), manifest.compositionCoreDigest,
        outer.value().SemanticDigest(), outer.value().FileDigest()));
}
}
