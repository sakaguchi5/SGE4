#include "CompositionToolchain.h"
#include "../planner/CompositionPlanner.h"

#include "../../canonical/base/BinaryIO.h"
#include "../../canonical/base/CheckedMath.h"
#include "../../leaf/artifact/package/PackageReader.h"
#include "../../backends/d3d12/artifact/D3D12Encoding.h"
#include "../model/plan/CompositionPlan.h"

#include <algorithm>
#include <limits>

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

[[nodiscard]] core::SemanticIdentity BuildDynamicSemanticIdentity(
    const Digest256& compositionCoreDigest,
    const CompositionCertificate& certificate,
    DynamicContractV1 dynamicContract)
{
    BinaryWriter payload;
    payload.WriteBytes(compositionCoreDigest);
    payload.WriteBytes(certificate.artifactIdentity.Digest());
    payload.WriteU32(dynamicContract.schemaVersion);
    payload.WriteU32(dynamicContract.universeCount);
    payload.WriteU32(std::to_underlying(dynamicContract.executionMode));
    payload.WriteU32(dynamicContract.targetLeaf.value);
    payload.WriteU32(dynamicContract.targetDynamicSlot);
    payload.WriteU32(dynamicContract.memberBytes);
    return core::SemanticIdentity::FromDigest(
        ComputeDomainDigest("sge4.composition.dynamic-semantic", 3, payload.Bytes()));
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
    DynamicContractV1 dynamicContract,
    core::SemanticIdentity semanticIdentity,
    FrozenCompositionIdentity compositionIdentity)
{
    BinaryWriter writer;
    writer.WriteU32(dynamicContract.schemaVersion);
    writer.WriteU32(dynamicContract.universeCount);
    writer.WriteU32(std::to_underlying(dynamicContract.executionMode));
    writer.WriteU32(dynamicContract.targetLeaf.value);
    writer.WriteU32(dynamicContract.targetDynamicSlot);
    writer.WriteU32(dynamicContract.memberBytes);
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
    const ValidatedCompositionContract& contract,
    DynamicContractV1 dynamicContract)
{
    if (dynamicContract.schemaVersion != 2 || dynamicContract.universeCount == 0)
        return Fail<void>(
            "CompositionToolchain", "Dynamic Contractが検証または実行の契約に違反しています。");

    if (dynamicContract.executionMode == DynamicExecutionModeV1::AuthorityOnly)
    {
        if (dynamicContract.targetLeaf.IsValid() ||
            dynamicContract.targetDynamicSlot != package::InvalidIndex ||
            dynamicContract.memberBytes != 0)
            return Fail<void>(
                "CompositionToolchain", "Authority-only Dynamic Contractに実行routeが混入しています。");
        return base::Success<void, Error>();
    }

    if (dynamicContract.executionMode != DynamicExecutionModeV1::VerifiedDenseSlot ||
        !dynamicContract.targetLeaf.IsValid() ||
        dynamicContract.targetLeaf.value >= contract.Leaves().size() ||
        dynamicContract.targetDynamicSlot == package::InvalidIndex ||
        dynamicContract.memberBytes == 0)
        return Fail<void>(
            "CompositionToolchain", "Verified Dynamic Execution routeが無効です。");

    const auto& leaf = contract.Leaves()[dynamicContract.targetLeaf.value];
    auto frozen = package::PackageReader::Read(leaf.packageBytes);
    if (!frozen)
        return Fail<void>("CompositionToolchain/DynamicRoute", frozen.error().message);
    auto view = package::d3d12_v13::D3D12PackageView::Decode(frozen.value());
    if (!view)
        return Fail<void>("CompositionToolchain/DynamicRoute", view.error().message);
    if (dynamicContract.targetDynamicSlot >= view.value().DynamicSlots().size())
        return Fail<void>(
            "CompositionToolchain/DynamicRoute", "Dynamic Slot参照が範囲外です。");

    if (dynamicContract.universeCount >
        std::numeric_limits<std::uint64_t>::max() / dynamicContract.memberBytes)
        return Fail<void>(
            "CompositionToolchain/DynamicRoute", "Dynamic dense slotのbyte数がoverflowします。");
    const auto requiredBytes = static_cast<std::uint64_t>(dynamicContract.universeCount) *
        dynamicContract.memberBytes;
    const auto& slot = view.value().DynamicSlots()[dynamicContract.targetDynamicSlot];
    if (slot.requiredBytes != requiredBytes)
        return Fail<void>(
            "CompositionToolchain/DynamicRoute", "Dynamic SlotのrequiredBytesとmember universeが一致しません。");
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
    return FreezeVerifiedCompositionPackage(contract.value(), verified.value(), dynamicContract);
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
            "CompositionReader", "SGE4UNI 2.1の必須Sectionがありません。");

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
    auto dynamicTargetLeaf = dynamic.ReadU32();
    auto dynamicTargetSlot = dynamic.ReadU32();
    auto dynamicMemberBytes = dynamic.ReadU32();
    auto dynamicCompositionIdentity = ReadDigest(dynamic, "CompositionReader/DynamicContract");
    auto dynamicSemanticIdentity = ReadDigest(dynamic, "CompositionReader/DynamicContract");
    if (!dynamicSchema || !dynamicUniverse || !dynamicExecutionMode ||
        !dynamicTargetLeaf || !dynamicTargetSlot || !dynamicMemberBytes ||
        !dynamicCompositionIdentity || !dynamicSemanticIdentity || dynamic.Remaining() != 0 ||
        dynamicSchema.value() != 2 || dynamicUniverse.value() == 0 ||
        dynamicExecutionMode.value() > std::to_underlying(DynamicExecutionModeV1::VerifiedDenseSlot) ||
        !SameIdentity(dynamicCompositionIdentity.value(), certificate.artifactIdentity))
        return Fail<FrozenCompositionPackage>(
            "CompositionReader", "Dynamic ContractがComposition identityと一致しません。");

    DynamicContractV1 dynamicContract{
        dynamicSchema.value(), dynamicUniverse.value(),
        static_cast<DynamicExecutionModeV1>(dynamicExecutionMode.value()),
        LeafPackageId{dynamicTargetLeaf.value()}, dynamicTargetSlot.value(),
        dynamicMemberBytes.value()};
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
        dynamicContract, manifest.compositionCoreDigest,
        outer.value().SemanticDigest(), outer.value().FileDigest()));
}
}
