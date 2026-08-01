#include "FrozenCompositionAbi1Migration.h"

#include "../../../canonical/artifact/SectionedArtifact.h"
#include "../../../canonical/base/BinaryIO.h"
#include "../../artifact/CompositionCertificate.h"
#include "../../artifact/VerifiedCompositionArtifact.h"
#include "./container/FrozenCompositionReader.h"
#include "./container/FrozenCompositionWriter.h"
#include "../../planner/CompositionPlanner.h"

#include <algorithm>
#include <array>
#include <utility>

namespace sge4::composition::migration::abi1
{
namespace
{
using base::BinaryReader;
using base::BinaryWriter;

inline constexpr std::array<std::byte, 8> Magic = {
    std::byte{'S'}, std::byte{'G'}, std::byte{'E'}, std::byte{'4'},
    std::byte{'U'}, std::byte{'N'}, std::byte{'I'}, std::byte{0}};
inline constexpr std::uint16_t FormatMajor = 1;
inline constexpr std::uint16_t FormatMinor = 1;

enum class SectionKind : std::uint32_t
{
    Manifest = 1,
    CompleteComposition = 2,
    AuthorityLedger = 3,
    DynamicContract = 4
};

constexpr auto RequiredExecution =
    static_cast<std::uint16_t>(SectionFlags::Required) |
    static_cast<std::uint16_t>(SectionFlags::ExecutionAffecting);

template<class T>
[[nodiscard]] base::Expected<T, Error> Fail(std::string stage, std::string message)
{
    return base::Failure<T, Error>({std::move(stage), std::move(message)});
}

[[nodiscard]] canonical::SemanticIdentity BuildDynamicSemanticIdentity(
    const artifact::VerifiedFrozenComposition& complete,
    const CompositionCertificate& certificate,
    DynamicContractV1 dynamicContract)
{
    BinaryWriter payload;
    payload.WriteBytes(complete.CoreDigest());
    payload.WriteBytes(certificate.artifactIdentity.Digest());
    payload.WriteU32(1);
    payload.WriteU32(dynamicContract.universeCount);
    return canonical::SemanticIdentity::FromDigest(
        ComputeDomainDigest("sge4.composition.dynamic-semantic", 1, payload.Bytes()));
}

[[nodiscard]] std::vector<std::byte> BuildManifest(
    const artifact::VerifiedFrozenComposition& complete,
    const CompositionCertificate& certificate,
    canonical::SemanticIdentity semanticIdentity,
    DynamicContractV1 dynamicContract)
{
    BinaryWriter writer;
    writer.WriteU32(1);
    writer.WriteU32(dynamicContract.universeCount);
    writer.WriteU32(certificate.leafCount);
    writer.WriteU32(certificate.flowCount);
    writer.WriteBytes(complete.CoreDigest());
    writer.WriteBytes(certificate.artifactIdentity.Digest());
    writer.WriteBytes(semanticIdentity.Digest());
    writer.WriteBytes(complete.FileDigest());
    return std::move(writer).Take();
}

[[nodiscard]] std::vector<std::byte> BuildAuthorityLedger(
    const CompositionCertificate& certificate)
{
    BinaryWriter writer;
    writer.WriteU32(1);
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
    canonical::SemanticIdentity semanticIdentity)
{
    BinaryWriter writer;
    writer.WriteU32(1);
    writer.WriteU32(dynamicContract.universeCount);
    writer.WriteBytes(semanticIdentity.Digest());
    return std::move(writer).Take();
}

[[nodiscard]] base::Expected<Digest256, Error> ReadDigest(BinaryReader& reader)
{
    auto bytes = reader.ReadBytes(32);
    if (!bytes) return Fail<Digest256>("abi1/migration", bytes.error());
    Digest256 result{};
    std::copy(bytes.value().begin(), bytes.value().end(), result.begin());
    return base::Success<Digest256, Error>(result);
}

[[nodiscard]] base::Expected<std::vector<std::byte>, Error>
FreezeVerifiedCompositionAbi1(
    const ValidatedCompositionContract& validated,
    const verification::VerifiedCompositionPlan& verified)
{
    auto authority = verification::ValidateVerifiedPlan(validated, verified);
    if (!authority)
        return Fail<std::vector<std::byte>>(authority.error().stage, authority.error().message);

    FrozenCompositionBuildInput input;
    input.leaves.reserve(validated.Leaves().size());
    for (const auto& leaf : validated.Leaves())
        input.leaves.push_back({leaf.stableKey, leaf.packageBytes});

    const auto& contract = validated.Contract();
    if (contract.presenterLeaf.IsValid())
    {
        if (contract.presenterLeaf.value >= validated.Leaves().size())
            return Fail<std::vector<std::byte>>(
                "abi1/freeze/presenter", "Presenter Leaf参照が範囲外です。");
        input.hasPresenter = true;
        input.presenterStableKey =
            validated.Leaves()[contract.presenterLeaf.value].stableKey;
    }
    input.contractBytes = SerializeCompositionContract(contract);
    input.verifiedDecisionBytes = planning::SerializeRawCompositionPlan(verified.Plan());
    input.verificationCertificateBytes =
        verification::SerializeVerificationCertificate(verified.Certificate());

    auto written = FrozenCompositionWriter::Write(std::move(input));
    if (!written)
        return Fail<std::vector<std::byte>>(
            "abi1/freeze/container", written.error().message);
    return base::Success<std::vector<std::byte>, Error>(std::move(written).value());
}

[[nodiscard]] base::Expected<artifact::VerifiedFrozenComposition, Error>
ReadVerifiedCompositionAbi1(std::span<const std::byte> bytes)
{
    auto legacyResult = FrozenCompositionReader::Read(bytes);
    if (!legacyResult)
        return Fail<artifact::VerifiedFrozenComposition>(
            "abi1/read/container", legacyResult.error().message);
    auto legacy = std::move(legacyResult).value();

    std::vector<CanonicalLeafPackage> leaves;
    leaves.reserve(legacy.Leaves().size());
    for (const auto& leaf : legacy.Leaves())
    {
        CanonicalLeafPackage canonical;
        canonical.stableKey = leaf.record.stableKey;
        canonical.packageBytes.assign(
            leaf.packageBytes.begin(), leaf.packageBytes.end());
        leaves.push_back(std::move(canonical));
    }

    auto verified = artifact::CreateVerifiedFrozenComposition(
        std::vector<std::byte>(bytes.begin(), bytes.end()),
        legacy.Header().semanticDigest, legacy.Header().fileDigest,
        legacy.Header().fileDigest, legacy.Header().formatMajor,
        std::vector<std::byte>(
            legacy.ContractBytes().begin(), legacy.ContractBytes().end()),
        std::move(leaves),
        std::vector<std::byte>(legacy.VerifiedDecisionBytes().begin(),
            legacy.VerifiedDecisionBytes().end()),
        std::vector<std::byte>(legacy.VerificationCertificateBytes().begin(),
            legacy.VerificationCertificateBytes().end()));
    if (!verified)
        return Fail<artifact::VerifiedFrozenComposition>(
            verified.error().stage, verified.error().message);
    return base::Success<artifact::VerifiedFrozenComposition, Error>(
        std::move(verified).value());
}
}

base::Expected<std::vector<std::byte>, Error>
BuildFrozenCompositionPackageAbi1ForMigration(
    ContractBuildInput input,
    DynamicContractV1 dynamicContract)
{
    if (dynamicContract.schemaVersion != 3 || dynamicContract.universeCount == 0 ||
        dynamicContract.executionMode != DynamicExecutionModeV1::AuthorityOnly ||
        dynamicContract.targetLeaf.IsValid() ||
        dynamicContract.targetDynamicSlot != package::InvalidIndex ||
        dynamicContract.memberBytes != 0 || !dynamicContract.conditionalRegions.empty())
        return Fail<std::vector<std::byte>>(
            "abi1/build", "ABI 1移行Corpusはauthority-only Dynamic Contractだけを受理します。");

    auto contract = BuildCompositionContract(std::move(input));
    if (!contract)
        return Fail<std::vector<std::byte>>(contract.error().stage, contract.error().message);
    if (std::ranges::any_of(contract.value().Contract().resources, [](const auto& resource) {
            return resource.kind != package::d3d12_v13::ResourceKind::Buffer;
        }))
        return Fail<std::vector<std::byte>>(
            "abi1/build", "ABI 1移行CorpusはBuffer Flowだけを表現します。");
    auto proposal = planning::ProposeCompositionPlan(contract.value());
    if (!proposal)
        return Fail<std::vector<std::byte>>(proposal.error().stage, proposal.error().message);
    auto verified = verification::VerifyAndSeal(contract.value(), proposal.value());
    if (!verified)
        return Fail<std::vector<std::byte>>(verified.error().stage, verified.error().message);

    auto completeBytes = FreezeVerifiedCompositionAbi1(contract.value(), verified.value());
    if (!completeBytes)
        return Fail<std::vector<std::byte>>(completeBytes.error().stage, completeBytes.error().message);
    auto complete = ReadVerifiedCompositionAbi1(completeBytes.value());
    if (!complete)
        return Fail<std::vector<std::byte>>(complete.error().stage, complete.error().message);

    auto certificate = BuildCompositionCertificate(complete.value());
    certificate.schemaVersion = 1;
    const auto semanticIdentity = BuildDynamicSemanticIdentity(
        complete.value(), certificate, dynamicContract);

    std::vector<SectionInput> sections;
    sections.push_back({std::to_underlying(SectionKind::Manifest), 1,
        RequiredExecution, 8,
        BuildManifest(complete.value(), certificate, semanticIdentity, dynamicContract)});
    sections.push_back({std::to_underlying(SectionKind::CompleteComposition), 1,
        RequiredExecution, 8, std::move(completeBytes).value()});
    sections.push_back({std::to_underlying(SectionKind::AuthorityLedger), 2,
        RequiredExecution, 8, BuildAuthorityLedger(certificate)});
    sections.push_back({std::to_underlying(SectionKind::DynamicContract), 1,
        RequiredExecution, 8, BuildDynamicContractBytes(dynamicContract, semanticIdentity)});

    auto outer = WriteSectionedArtifact(Magic, FormatMajor, FormatMinor, std::move(sections));
    if (!outer)
        return Fail<std::vector<std::byte>>(outer.error().stage, outer.error().message);
    return base::Success<std::vector<std::byte>, Error>(std::move(outer).value());
}

base::Expected<FrozenCompositionPackage, Error>
MigrateFrozenCompositionPackageAbi1ToAbi2(std::span<const std::byte> bytes)
{
    auto outer = ReadSectionedArtifact(bytes, Magic, FormatMajor);
    if (!outer)
        return Fail<FrozenCompositionPackage>(outer.error().stage, outer.error().message);
    if (outer.value().FormatMinor() != FormatMinor || outer.value().Sections().size() != 4)
        return Fail<FrozenCompositionPackage>(
            "abi1/migration", "SGE4UNI 1.1ではありません。");

    constexpr std::array expectedKinds = {
        SectionKind::Manifest, SectionKind::CompleteComposition,
        SectionKind::AuthorityLedger, SectionKind::DynamicContract};
    constexpr std::array<std::uint16_t, 4> expectedSchemas = {1, 1, 2, 1};
    for (std::size_t index = 0; index < expectedKinds.size(); ++index)
    {
        const auto& section = outer.value().Sections()[index];
        if (section.kind != std::to_underlying(expectedKinds[index]) ||
            section.schemaVersion != expectedSchemas[index] ||
            section.flags != RequiredExecution || section.alignment != 8)
            return Fail<FrozenCompositionPackage>(
                "abi1/migration",
                "ABI 1 Sectionの順序、Schema、FlagsまたはAlignmentが不正です。");
    }

    const auto* manifestSection = outer.value().FindSection(
        std::to_underlying(SectionKind::Manifest));
    const auto* completeSection = outer.value().FindSection(
        std::to_underlying(SectionKind::CompleteComposition));
    const auto* authoritySection = outer.value().FindSection(
        std::to_underlying(SectionKind::AuthorityLedger));
    const auto* dynamicSection = outer.value().FindSection(
        std::to_underlying(SectionKind::DynamicContract));
    if (!manifestSection || !completeSection || !authoritySection || !dynamicSection)
        return Fail<FrozenCompositionPackage>(
            "abi1/migration", "ABI 1の必須Sectionがありません。");

    auto complete = ReadVerifiedCompositionAbi1(completeSection->bytes);
    if (!complete)
        return Fail<FrozenCompositionPackage>(complete.error().stage, complete.error().message);
    auto certificate = BuildCompositionCertificate(complete.value());
    certificate.schemaVersion = 1;

    BinaryReader manifest(manifestSection->bytes);
    auto manifestSchema = manifest.ReadU32();
    auto universe = manifest.ReadU32();
    auto leafCount = manifest.ReadU32();
    auto flowCount = manifest.ReadU32();
    auto innerSemantic = ReadDigest(manifest);
    auto artifactIdentity = ReadDigest(manifest);
    auto dynamicIdentity = ReadDigest(manifest);
    auto innerFileDigest = ReadDigest(manifest);
    if (!manifestSchema || !universe || !leafCount || !flowCount || !innerSemantic ||
        !artifactIdentity || !dynamicIdentity || !innerFileDigest || manifest.Remaining() != 0 ||
        manifestSchema.value() != 1 || universe.value() == 0 ||
        leafCount.value() != certificate.leafCount || flowCount.value() != certificate.flowCount ||
        innerSemantic.value() != complete.value().CoreDigest() ||
        innerFileDigest.value() != complete.value().FileDigest() ||
        artifactIdentity.value() != certificate.artifactIdentity.Digest())
        return Fail<FrozenCompositionPackage>(
            "abi1/migration", "ABI 1 Manifestが検証済みCompositionと一致しません。");

    BinaryReader ledger(authoritySection->bytes);
    auto ledgerSchema = ledger.ReadU32();
    auto frozen = ReadDigest(ledger);
    auto contract = ReadDigest(ledger);
    auto plan = ReadDigest(ledger);
    auto seal = ReadDigest(ledger);
    auto schedule = ReadDigest(ledger);
    auto recovery = ReadDigest(ledger);
    auto ledgerLeafCount = ledger.ReadU32();
    auto ledgerFlowCount = ledger.ReadU32();
    if (!ledgerSchema || !frozen || !contract || !plan || !seal || !schedule || !recovery ||
        !ledgerLeafCount || !ledgerFlowCount || ledger.Remaining() != 0 ||
        ledgerSchema.value() != 1 ||
        frozen.value() != certificate.artifactIdentity.Digest() ||
        contract.value() != certificate.contractIdentity.Digest() ||
        plan.value() != certificate.planIdentity.Digest() ||
        seal.value() != certificate.sealIdentity.Digest() ||
        schedule.value() != certificate.scheduleIdentity.Digest() ||
        recovery.value() != certificate.recoverySetIdentity.Digest() ||
        ledgerLeafCount.value() != certificate.leafCount ||
        ledgerFlowCount.value() != certificate.flowCount)
        return Fail<FrozenCompositionPackage>(
            "abi1/migration", "ABI 1 Authority Ledgerが一致しません。");

    BinaryReader dynamic(dynamicSection->bytes);
    auto dynamicSchema = dynamic.ReadU32();
    auto dynamicUniverse = dynamic.ReadU32();
    auto encodedDynamicIdentity = ReadDigest(dynamic);
    auto dynamicContract = MakeAuthorityOnlyDynamicContractV1(universe.value());
    const auto derivedDynamicIdentity = BuildDynamicSemanticIdentity(
        complete.value(), certificate, dynamicContract);
    if (!dynamicSchema || !dynamicUniverse || !encodedDynamicIdentity ||
        dynamic.Remaining() != 0 || dynamicSchema.value() != 1 ||
        dynamicUniverse.value() != universe.value() ||
        encodedDynamicIdentity.value() != derivedDynamicIdentity.Digest() ||
        dynamicIdentity.value() != derivedDynamicIdentity.Digest())
        return Fail<FrozenCompositionPackage>(
            "abi1/migration", "ABI 1 Dynamic Contractが一致しません。");

    return FreezeVerifiedCompositionPackage(
        complete.value().ValidatedContract(), complete.value().VerifiedPlan(), dynamicContract);
}
}
