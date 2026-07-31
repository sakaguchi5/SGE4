#include "FrozenCompositionAbi2.h"

#include "../VerifiedCompositionArtifact.h"
#include "../../../canonical/base/BinaryIO.h"
#include "../../../canonical/base/CheckedMath.h"
#include "../../../leaf/artifact/package/PackageReader.h"
#include "../../model/plan/CompositionPlan.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace sge4::composition::artifact
{
namespace
{
using base::BinaryReader;
using base::BinaryWriter;
using VerificationError = verification::VerificationError;

constexpr auto RequiredExecution =
    static_cast<std::uint16_t>(SectionFlags::Required) |
    static_cast<std::uint16_t>(SectionFlags::ExecutionAffecting);

template<class T>
[[nodiscard]] base::Expected<T, VerificationError> Fail(
    std::string stage,
    std::string message)
{
    return base::Failure<T, VerificationError>({std::move(stage), std::move(message)});
}

[[nodiscard]] bool IsZeroDigest(const base::Digest256& digest) noexcept
{
    return std::all_of(digest.begin(), digest.end(), [](std::byte value) {
        return value == std::byte{0};
    });
}

[[nodiscard]] bool DigestLess(
    const base::Digest256& left,
    const base::Digest256& right) noexcept
{
    return std::lexicographical_compare(
        left.begin(), left.end(), right.begin(), right.end());
}

void WriteLeafRecord(BinaryWriter& writer, const FrozenCompositionAbi2LeafRecord& record)
{
    writer.WriteU32(record.leafId);
    writer.WriteU32(record.flags);
    writer.WriteBytes(record.stableKey);
    writer.WriteU64(record.byteOffset);
    writer.WriteU64(record.byteSize);
    writer.WriteBytes(record.executionDigest);
    writer.WriteBytes(record.fileDigest);
    writer.WriteU64(0);
}

[[nodiscard]] base::Expected<base::Digest256, VerificationError> ReadDigest(
    BinaryReader& reader,
    std::string_view stage)
{
    auto bytes = reader.ReadBytes(32);
    if (!bytes)
        return Fail<base::Digest256>(std::string(stage), bytes.error());
    base::Digest256 digest{};
    std::copy(bytes.value().begin(), bytes.value().end(), digest.begin());
    return base::Success<base::Digest256, VerificationError>(digest);
}

[[nodiscard]] base::Expected<std::vector<CanonicalLeafPackage>, VerificationError>
ReadLeaves(
    const SectionView& table,
    const SectionView& bytes,
    std::uint32_t expectedCount,
    std::uint64_t expectedByteCount)
{
    if (table.schemaVersion != 1 || bytes.schemaVersion != 1 ||
        table.flags != RequiredExecution || bytes.flags != RequiredExecution ||
        table.alignment != FrozenCompositionAbi2Alignment ||
        bytes.alignment != FrozenCompositionAbi2Alignment ||
        expectedCount > std::numeric_limits<std::size_t>::max() / FrozenCompositionAbi2LeafRecordBytes ||
        table.bytes.size() != static_cast<std::size_t>(expectedCount) * FrozenCompositionAbi2LeafRecordBytes ||
        bytes.bytes.size() != expectedByteCount)
        return Fail<std::vector<CanonicalLeafPackage>>(
            "abi2/leaf-table", "Leaf Tableが検証または実行の契約に違反しています。");

    BinaryReader reader(table.bytes);
    std::vector<CanonicalLeafPackage> leaves;
    leaves.reserve(expectedCount);
    base::Digest256 previousStableKey{};
    bool hasPrevious = false;
    std::uint64_t expectedOffset = 0;

    for (std::uint32_t index = 0; index < expectedCount; ++index)
    {
        auto leafId = reader.ReadU32();
        auto flags = reader.ReadU32();
        auto stableKey = ReadDigest(reader, "abi2/leaf-table");
        auto byteOffset = reader.ReadU64();
        auto byteSize = reader.ReadU64();
        auto executionDigest = ReadDigest(reader, "abi2/leaf-table");
        auto fileDigest = ReadDigest(reader, "abi2/leaf-table");
        auto reserved = reader.ReadU64();
        if (!leafId || !flags || !stableKey || !byteOffset || !byteSize ||
            !executionDigest || !fileDigest || !reserved)
            return Fail<std::vector<CanonicalLeafPackage>>(
                "abi2/leaf-table", "Leaf Tableの読み込みに失敗しました。");
        if (leafId.value() != index || flags.value() != 0 || reserved.value() != 0 ||
            IsZeroDigest(stableKey.value()) || byteSize.value() == 0)
            return Fail<std::vector<CanonicalLeafPackage>>(
                "abi2/leaf-table", "Leaf TableがCanonicalな順序または識別子規則に違反しています。");
        if (hasPrevious && !DigestLess(previousStableKey, stableKey.value()))
            return Fail<std::vector<CanonicalLeafPackage>>(
                "abi2/leaf-table", "Leaf stable keyが重複または非Canonicalです。");

        if (expectedOffset > std::numeric_limits<std::uint64_t>::max() -
                (FrozenCompositionAbi2Alignment - 1u))
            return Fail<std::vector<CanonicalLeafPackage>>(
                "abi2/leaf-bytes", "Leaf bytesのAlignment計算がoverflowしました。");
        const auto alignedOffset = base::AlignUp(
            expectedOffset, FrozenCompositionAbi2Alignment);
        if (alignedOffset > bytes.bytes.size() ||
            !std::ranges::all_of(
                bytes.bytes.subspan(
                    static_cast<std::size_t>(expectedOffset),
                    static_cast<std::size_t>(alignedOffset - expectedOffset)),
                [](std::byte value) { return value == std::byte{0}; }) ||
            byteOffset.value() != alignedOffset ||
            byteOffset.value() > bytes.bytes.size() ||
            byteSize.value() > bytes.bytes.size() - byteOffset.value())
            return Fail<std::vector<CanonicalLeafPackage>>(
                "abi2/leaf-bytes", "Leaf bytesの範囲またはpaddingが無効です。");
        expectedOffset = alignedOffset;
        const auto packageBytes = bytes.bytes.subspan(
            static_cast<std::size_t>(byteOffset.value()),
            static_cast<std::size_t>(byteSize.value()));
        auto packageResult = package::PackageReader::Read(packageBytes);
        if (!packageResult)
            return Fail<std::vector<CanonicalLeafPackage>>(
                "abi2/leaf-package", packageResult.error().message);
        const auto& header = packageResult.value().Header();
        if (header.targetKind != package::TargetKindD3D12 ||
            header.targetSchemaVersion != FrozenCompositionAbi2EmbeddedSchemaVersion ||
            header.minimumRuntimeVersion != FrozenCompositionAbi2EmbeddedRuntimeVersion ||
            header.executionDigest != executionDigest.value() ||
            header.fileDigest != fileDigest.value())
            return Fail<std::vector<CanonicalLeafPackage>>(
                "abi2/leaf-package", "埋め込みLeaf Packageの契約またはDigestが一致しません。");

        CanonicalLeafPackage leaf;
        leaf.stableKey = stableKey.value();
        leaf.packageBytes.assign(packageBytes.begin(), packageBytes.end());
        leaves.push_back(std::move(leaf));
        previousStableKey = stableKey.value();
        hasPrevious = true;
        expectedOffset = byteOffset.value() + byteSize.value();
    }
    if (reader.Remaining() != 0 || expectedOffset != bytes.bytes.size())
        return Fail<std::vector<CanonicalLeafPackage>>(
            "abi2/leaf-bytes", "Leaf bytes末尾またはTable record数がCanonicalではありません。");
    return base::Success<std::vector<CanonicalLeafPackage>, VerificationError>(
        std::move(leaves));
}

[[nodiscard]] bool IsCoreSection(FrozenCompositionAbi2SectionKind kind) noexcept
{
    return kind >= FrozenCompositionAbi2SectionKind::LeafTable &&
        kind <= FrozenCompositionAbi2SectionKind::VerificationCertificate;
}
}

base::Digest256 ComputeFrozenCompositionAbi2CoreDigest(
    std::uint32_t leafCount,
    std::uint32_t flowCount,
    std::uint32_t presenterLeafId,
    std::span<const SectionInput> coreSections)
{
    BinaryWriter writer;
    writer.WriteU32(FrozenCompositionAbi2CoreSchema);
    writer.WriteU32(leafCount);
    writer.WriteU32(flowCount);
    writer.WriteU32(presenterLeafId);
    writer.WriteCountU32(coreSections.size());
    for (const auto& section : coreSections)
    {
        writer.WriteU32(section.kind);
        writer.WriteU16(section.schemaVersion);
        writer.WriteU16(section.flags);
        writer.WriteU32(section.alignment);
        writer.WriteU32(0);
        writer.WriteU64(section.bytes.size());
        writer.WriteBytes(base::Sha256(section.bytes));
    }
    return ComputeDomainDigest(
        "sge4.composition.abi2.core", FrozenCompositionAbi2CoreSchema, writer.Bytes());
}

base::Expected<FrozenCompositionAbi2Core, verification::VerificationError>
BuildFrozenCompositionAbi2Core(
    const ValidatedCompositionContract& validated,
    const verification::VerifiedCompositionPlan& verified)
{
    auto authority = verification::ValidateVerifiedPlan(validated, verified);
    if (!authority)
        return base::Failure<FrozenCompositionAbi2Core, VerificationError>(authority.error());

    const auto& contract = validated.Contract();
    if (validated.Leaves().size() < 2 ||
        validated.Leaves().size() > std::numeric_limits<std::uint32_t>::max() ||
        contract.resources.size() > std::numeric_limits<std::uint32_t>::max())
        return Fail<FrozenCompositionAbi2Core>(
            "abi2/build", "CompositionのLeafまたはFlow数がABI 2.1の範囲外です。");

    BinaryWriter leafBytes;
    BinaryWriter leafTable;
    base::Digest256 previousStableKey{};
    bool hasPrevious = false;
    for (std::size_t index = 0; index < validated.Leaves().size(); ++index)
    {
        const auto& leaf = validated.Leaves()[index];
        if (IsZeroDigest(leaf.stableKey) ||
            (hasPrevious && !DigestLess(previousStableKey, leaf.stableKey)))
            return Fail<FrozenCompositionAbi2Core>(
                "abi2/build/leaf-order", "Leaf stable keyが重複または非Canonicalです。");
        auto packageResult = package::PackageReader::Read(leaf.packageBytes);
        if (!packageResult)
            return Fail<FrozenCompositionAbi2Core>(
                "abi2/build/leaf-package", packageResult.error().message);
        const auto& header = packageResult.value().Header();
        if (header.targetKind != package::TargetKindD3D12 ||
            header.targetSchemaVersion != FrozenCompositionAbi2EmbeddedSchemaVersion ||
            header.minimumRuntimeVersion != FrozenCompositionAbi2EmbeddedRuntimeVersion)
            return Fail<FrozenCompositionAbi2Core>(
                "abi2/build/leaf-package", "Leaf Package SchemaまたはRuntime versionが未対応です。");

        leafBytes.Align(FrozenCompositionAbi2Alignment);
        FrozenCompositionAbi2LeafRecord record;
        record.leafId = static_cast<std::uint32_t>(index);
        record.stableKey = leaf.stableKey;
        record.byteOffset = leafBytes.Size();
        record.byteSize = leaf.packageBytes.size();
        record.executionDigest = header.executionDigest;
        record.fileDigest = header.fileDigest;
        WriteLeafRecord(leafTable, record);
        leafBytes.WriteBytes(leaf.packageBytes);
        previousStableKey = leaf.stableKey;
        hasPrevious = true;
    }

    FrozenCompositionAbi2Core core;
    core.leafCount = static_cast<std::uint32_t>(validated.Leaves().size());
    core.flowCount = static_cast<std::uint32_t>(contract.resources.size());
    core.presenterLeafId = contract.presenterLeaf.value;
    core.leafBytes = leafBytes.Size();

    auto contractBytes = SerializeCompositionContract(contract);
    auto decisionBytes = planning::SerializeRawCompositionPlan(verified.Plan());
    auto certificateBytes = verification::SerializeVerificationCertificate(
        verified.Certificate());
    core.contractBytes = contractBytes.size();
    core.verifiedDecisionBytes = decisionBytes.size();
    core.verificationCertificateBytes = certificateBytes.size();

    core.sections.push_back({
        std::to_underlying(FrozenCompositionAbi2SectionKind::LeafTable), 1,
        RequiredExecution, FrozenCompositionAbi2Alignment, std::move(leafTable).Take()});
    core.sections.push_back({
        std::to_underlying(FrozenCompositionAbi2SectionKind::LeafBytes), 1,
        RequiredExecution, FrozenCompositionAbi2Alignment, std::move(leafBytes).Take()});
    core.sections.push_back({
        std::to_underlying(FrozenCompositionAbi2SectionKind::ContractData), 1,
        RequiredExecution, FrozenCompositionAbi2Alignment, std::move(contractBytes)});
    core.sections.push_back({
        std::to_underlying(FrozenCompositionAbi2SectionKind::VerifiedDecisionData), 1,
        RequiredExecution, FrozenCompositionAbi2Alignment, std::move(decisionBytes)});
    core.sections.push_back({
        std::to_underlying(FrozenCompositionAbi2SectionKind::VerificationCertificate), 1,
        RequiredExecution, FrozenCompositionAbi2Alignment, std::move(certificateBytes)});
    core.coreDigest = ComputeFrozenCompositionAbi2CoreDigest(
        core.leafCount, core.flowCount, core.presenterLeafId, core.sections);
    return base::Success<FrozenCompositionAbi2Core, VerificationError>(std::move(core));
}

std::vector<std::byte> SerializeFrozenCompositionAbi2Manifest(
    const FrozenCompositionAbi2Manifest& manifest)
{
    BinaryWriter writer;
    writer.WriteU32(manifest.schemaVersion);
    writer.WriteU32(manifest.dynamicUniverseCount);
    writer.WriteU32(manifest.leafCount);
    writer.WriteU32(manifest.flowCount);
    writer.WriteU32(manifest.presenterLeafId);
    writer.WriteU32(manifest.flags);
    writer.WriteU64(manifest.leafBytes);
    writer.WriteU64(manifest.contractBytes);
    writer.WriteU64(manifest.verifiedDecisionBytes);
    writer.WriteU64(manifest.verificationCertificateBytes);
    writer.WriteBytes(manifest.compositionCoreDigest);
    writer.WriteBytes(manifest.compositionArtifactIdentity);
    writer.WriteBytes(manifest.dynamicSemanticIdentity);
    return std::move(writer).Take();
}

base::Expected<FrozenCompositionAbi2Manifest, verification::VerificationError>
DeserializeFrozenCompositionAbi2Manifest(std::span<const std::byte> bytes)
{
    BinaryReader reader(bytes);
    FrozenCompositionAbi2Manifest manifest;
    auto schema = reader.ReadU32();
    auto universe = reader.ReadU32();
    auto leafCount = reader.ReadU32();
    auto flowCount = reader.ReadU32();
    auto presenter = reader.ReadU32();
    auto flags = reader.ReadU32();
    auto leafBytes = reader.ReadU64();
    auto contractBytes = reader.ReadU64();
    auto decisionBytes = reader.ReadU64();
    auto certificateBytes = reader.ReadU64();
    auto coreDigest = ReadDigest(reader, "abi2/manifest");
    auto artifactIdentity = ReadDigest(reader, "abi2/manifest");
    auto dynamicIdentity = ReadDigest(reader, "abi2/manifest");
    if (!schema || !universe || !leafCount || !flowCount || !presenter || !flags ||
        !leafBytes || !contractBytes || !decisionBytes || !certificateBytes ||
        !coreDigest || !artifactIdentity || !dynamicIdentity || reader.Remaining() != 0)
        return Fail<FrozenCompositionAbi2Manifest>(
            "abi2/manifest", "Manifestの読み込みに失敗しました。");
    manifest.schemaVersion = schema.value();
    manifest.dynamicUniverseCount = universe.value();
    manifest.leafCount = leafCount.value();
    manifest.flowCount = flowCount.value();
    manifest.presenterLeafId = presenter.value();
    manifest.flags = flags.value();
    manifest.leafBytes = leafBytes.value();
    manifest.contractBytes = contractBytes.value();
    manifest.verifiedDecisionBytes = decisionBytes.value();
    manifest.verificationCertificateBytes = certificateBytes.value();
    manifest.compositionCoreDigest = coreDigest.value();
    manifest.compositionArtifactIdentity = artifactIdentity.value();
    manifest.dynamicSemanticIdentity = dynamicIdentity.value();
    if (manifest.schemaVersion != FrozenCompositionAbi2ManifestSchema ||
        manifest.dynamicUniverseCount == 0 || manifest.leafCount < 2 ||
        manifest.flowCount == 0 ||
        (manifest.presenterLeafId != InvalidIndex &&
            manifest.presenterLeafId >= manifest.leafCount) ||
        manifest.leafBytes == 0 || manifest.contractBytes == 0 ||
        manifest.verifiedDecisionBytes == 0 ||
        manifest.verificationCertificateBytes == 0 ||
        manifest.flags != 0 || IsZeroDigest(manifest.compositionCoreDigest) ||
        IsZeroDigest(manifest.compositionArtifactIdentity) ||
        IsZeroDigest(manifest.dynamicSemanticIdentity))
        return Fail<FrozenCompositionAbi2Manifest>(
            "abi2/manifest", "ManifestがABI 2.1契約に違反しています。");
    return base::Success<FrozenCompositionAbi2Manifest, VerificationError>(manifest);
}

base::Expected<VerifiedFrozenComposition, verification::VerificationError>
ReadVerifiedFrozenCompositionAbi2(std::vector<std::byte> bytes)
{
    auto outer = ReadSectionedArtifact(
        bytes, FrozenCompositionAbi2Magic, FrozenCompositionAbi2FormatMajor);
    if (!outer)
        return Fail<VerifiedFrozenComposition>(outer.error().stage, outer.error().message);
    if (outer.value().FormatMinor() != FrozenCompositionAbi2FormatMinor ||
        outer.value().Sections().size() != FrozenCompositionAbi2SectionKinds.size())
        return Fail<VerifiedFrozenComposition>(
            "abi2/header", "SGE4UNI 2.1のVersionまたはSection数が一致しません。");

    for (std::size_t index = 0; index < FrozenCompositionAbi2SectionKinds.size(); ++index)
    {
        const auto& section = outer.value().Sections()[index];
        if (section.kind != std::to_underlying(FrozenCompositionAbi2SectionKinds[index]) ||
            section.schemaVersion != FrozenCompositionAbi2SectionSchemas[index] ||
            section.flags != RequiredExecution ||
            section.alignment != FrozenCompositionAbi2Alignment)
            return Fail<VerifiedFrozenComposition>(
                "abi2/section-table",
                "Sectionの順序、Schema、FlagsまたはAlignmentがABI 2.1契約に違反しています。");
    }

    const auto* manifestSection = outer.value().FindSection(
        std::to_underlying(FrozenCompositionAbi2SectionKind::Manifest));
    const auto* leafTable = outer.value().FindSection(
        std::to_underlying(FrozenCompositionAbi2SectionKind::LeafTable));
    const auto* leafBytes = outer.value().FindSection(
        std::to_underlying(FrozenCompositionAbi2SectionKind::LeafBytes));
    const auto* contractSection = outer.value().FindSection(
        std::to_underlying(FrozenCompositionAbi2SectionKind::ContractData));
    const auto* decisionSection = outer.value().FindSection(
        std::to_underlying(FrozenCompositionAbi2SectionKind::VerifiedDecisionData));
    const auto* certificateSection = outer.value().FindSection(
        std::to_underlying(FrozenCompositionAbi2SectionKind::VerificationCertificate));
    if (!manifestSection || !leafTable || !leafBytes || !contractSection ||
        !decisionSection || !certificateSection)
        return Fail<VerifiedFrozenComposition>(
            "abi2/sections", "必須Composition Sectionがありません。");

    auto manifestResult = DeserializeFrozenCompositionAbi2Manifest(manifestSection->bytes);
    if (!manifestResult)
        return base::Failure<VerifiedFrozenComposition, VerificationError>(manifestResult.error());
    const auto manifest = manifestResult.value();
    if (leafBytes->bytes.size() != manifest.leafBytes ||
        contractSection->bytes.size() != manifest.contractBytes ||
        decisionSection->bytes.size() != manifest.verifiedDecisionBytes ||
        certificateSection->bytes.size() != manifest.verificationCertificateBytes)
        return Fail<VerifiedFrozenComposition>(
            "abi2/manifest", "ManifestのSection byte数が実体と一致しません。");

    auto leaves = ReadLeaves(
        *leafTable, *leafBytes, manifest.leafCount, manifest.leafBytes);
    if (!leaves)
        return base::Failure<VerifiedFrozenComposition, VerificationError>(leaves.error());

    std::vector<SectionInput> coreSections;
    for (const auto& section : outer.value().Sections())
    {
        const auto kind = static_cast<FrozenCompositionAbi2SectionKind>(section.kind);
        if (!IsCoreSection(kind)) continue;
        coreSections.push_back({section.kind, section.schemaVersion, section.flags,
            section.alignment, std::vector<std::byte>(section.bytes.begin(), section.bytes.end())});
    }
    if (coreSections.size() != 5)
        return Fail<VerifiedFrozenComposition>(
            "abi2/core-sections", "Composition Core Section数が一致しません。");
    const auto coreDigest = ComputeFrozenCompositionAbi2CoreDigest(
        manifest.leafCount, manifest.flowCount, manifest.presenterLeafId, coreSections);
    if (coreDigest != manifest.compositionCoreDigest)
        return Fail<VerifiedFrozenComposition>(
            "abi2/core-digest", "Composition Core Digestが一致しません。");

    auto verified = CreateVerifiedFrozenComposition(
        std::move(bytes), coreDigest, coreDigest, outer.value().FileDigest(),
        FrozenCompositionAbi2FormatMajor,
        std::vector<std::byte>(contractSection->bytes.begin(), contractSection->bytes.end()),
        std::move(leaves).value(),
        std::vector<std::byte>(decisionSection->bytes.begin(), decisionSection->bytes.end()),
        std::vector<std::byte>(certificateSection->bytes.begin(), certificateSection->bytes.end()));
    if (!verified)
        return verified;
    const auto& contract = verified.value().ValidatedContract().Contract();
    if (contract.leaves.size() != manifest.leafCount ||
        contract.resources.size() != manifest.flowCount ||
        contract.presenterLeaf.value != manifest.presenterLeafId)
        return Fail<VerifiedFrozenComposition>(
            "abi2/manifest-authority", "Manifestの件数またはPresenterがContractと一致しません。");
    return verified;
}
}
