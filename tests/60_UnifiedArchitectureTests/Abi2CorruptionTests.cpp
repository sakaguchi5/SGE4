#include "Abi2CorruptionTests.h"

#include "../../src/canonical/artifact/SectionedArtifact.h"
#include "../../src/canonical/base/Sha256.h"
#include "../../src/composition/artifact/abi2/FrozenCompositionAbi2.h"
#include "../../src/composition/toolchain/CompositionToolchain.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace sge4::tests
{
namespace
{
namespace artifact = composition::artifact;

void Require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

[[nodiscard]] std::vector<SectionInput> CopySections(const SectionedArtifact& source)
{
    std::vector<SectionInput> sections;
    sections.reserve(source.Sections().size());
    for (const auto& section : source.Sections())
        sections.push_back({section.kind, section.schemaVersion, section.flags,
            section.alignment,
            std::vector<std::byte>(section.bytes.begin(), section.bytes.end())});
    return sections;
}

[[nodiscard]] std::vector<std::byte> Rewrite(std::vector<SectionInput> sections)
{
    auto result = WriteSectionedArtifact(
        artifact::FrozenCompositionAbi2Magic,
        artifact::FrozenCompositionAbi2FormatMajor,
        artifact::FrozenCompositionAbi2FormatMinor,
        std::move(sections));
    Require(static_cast<bool>(result), "ABI 2.7破損Corpusの再符号化に失敗しました。");
    return std::move(result).value();
}

void RequireRejected(std::vector<std::byte> bytes, const char* message)
{
    Require(!composition::ReadFrozenCompositionPackage(std::move(bytes)), message);
}

void PatchU32(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value)
{
    Require(offset <= bytes.size() && sizeof(value) <= bytes.size() - offset,
        "ABI 2.7破損CorpusのU32位置が範囲外です。");
    for (std::size_t index = 0; index < sizeof(value); ++index)
        bytes[offset + index] = static_cast<std::byte>((value >> (index * 8u)) & 0xffu);
}

void PatchU64(std::vector<std::byte>& bytes, std::size_t offset, std::uint64_t value)
{
    Require(offset <= bytes.size() && sizeof(value) <= bytes.size() - offset,
        "ABI 2.7破損CorpusのU64位置が範囲外です。");
    for (std::size_t index = 0; index < sizeof(value); ++index)
        bytes[offset + index] = static_cast<std::byte>((value >> (index * 8u)) & 0xffu);
}

void RefreshFileDigest(std::vector<std::byte>& bytes)
{
    Require(bytes.size() >= ArtifactFileDigestOffset + 32,
        "ABI 2.7破損CorpusのHeaderが短すぎます。");
    std::fill(bytes.begin() + static_cast<std::ptrdiff_t>(ArtifactFileDigestOffset),
        bytes.begin() + static_cast<std::ptrdiff_t>(ArtifactFileDigestOffset + 32),
        std::byte{0});
    const auto digest = base::Sha256(bytes);
    std::copy(digest.begin(), digest.end(),
        bytes.begin() + static_cast<std::ptrdiff_t>(ArtifactFileDigestOffset));
}
}

void VerifyAbi2CorruptionRejection(std::span<const std::byte> validBytes)
{
    auto decoded = ReadSectionedArtifact(validBytes,
        artifact::FrozenCompositionAbi2Magic,
        artifact::FrozenCompositionAbi2FormatMajor);
    Require(static_cast<bool>(decoded), "ABI 2.7破損Corpusの正本を読めません。");

    // 必須Section欠落。
    {
        auto sections = CopySections(decoded.value());
        sections.erase(std::remove_if(sections.begin(), sections.end(), [](const SectionInput& section) {
            return section.kind == std::to_underlying(
                artifact::FrozenCompositionAbi2SectionKind::DynamicContract);
        }), sections.end());
        RequireRejected(Rewrite(std::move(sections)),
            "ABI 2.7 Readerが必須Section欠落を受理しました。");
    }

    // 未知のRequired Section追加。
    {
        auto sections = CopySections(decoded.value());
        sections.push_back({9, 1,
            static_cast<std::uint16_t>(SectionFlags::Required) |
                static_cast<std::uint16_t>(SectionFlags::ExecutionAffecting),
            8, {std::byte{0}}});
        RequireRejected(Rewrite(std::move(sections)),
            "ABI 2.7 Readerが未知のRequired Sectionを受理しました。");
    }

    // Section Schema不一致。
    {
        auto sections = CopySections(decoded.value());
        sections.front().schemaVersion = 3;
        RequireRejected(Rewrite(std::move(sections)),
            "ABI 2.7 ReaderがManifest Schema不一致を受理しました。");
    }

    // Leaf Tableのdense IDを壊し、外側Digestは正しく再計算する。
    {
        auto sections = CopySections(decoded.value());
        auto& table = sections[1].bytes;
        Require(table.size() >= 4, "Leaf Tableが短すぎます。");
        PatchU32(table, 0, 1);
        RequireRejected(Rewrite(std::move(sections)),
            "ABI 2.7 Readerが非dense Leaf IDを受理しました。");
    }

    // Leaf Tableの先頭offsetをCanonical位置からずらす。
    {
        auto sections = CopySections(decoded.value());
        auto& table = sections[1].bytes;
        Require(table.size() >= 48, "Leaf Tableが短すぎます。");
        PatchU64(table, 40, 8);
        RequireRejected(Rewrite(std::move(sections)),
            "ABI 2.7 Readerが非Canonical Leaf offsetを受理しました。");
    }

    // 二つ目のLeaf直前に存在するzero paddingを破壊する。
    {
        auto sections = CopySections(decoded.value());
        auto& table = sections[1].bytes;
        auto& leafBytes = sections[2].bytes;
        Require(table.size() >= 128 + 48, "Leaf Tableが二件未満です。");
        auto readU64 = [](const std::vector<std::byte>& bytes, std::size_t offset) {
            std::uint64_t value = 0;
            for (std::size_t index = 0; index < 8; ++index)
                value |= static_cast<std::uint64_t>(std::to_integer<unsigned char>(bytes[offset + index]))
                    << (index * 8u);
            return value;
        };
        const auto firstOffset = readU64(table, 40);
        const auto firstSize = readU64(table, 48);
        const auto secondOffset = readU64(table, 128 + 40);
        if (firstOffset + firstSize < secondOffset)
        {
            Require(secondOffset <= leafBytes.size(), "Leaf padding位置が範囲外です。");
            leafBytes[static_cast<std::size_t>(firstOffset + firstSize)] = std::byte{0x01};
            RequireRejected(Rewrite(std::move(sections)),
                "ABI 2.7 Readerが非zero Leaf paddingを受理しました。");
        }
    }

    // 埋め込みLeaf Packageを壊し、外側Section／file digestだけを更新する。
    {
        auto sections = CopySections(decoded.value());
        auto& leafBytes = sections[2].bytes;
        Require(!leafBytes.empty(), "Leaf bytesが空です。");
        leafBytes[leafBytes.size() / 2] ^= std::byte{0x01};
        RequireRejected(Rewrite(std::move(sections)),
            "ABI 2.7 Readerが破損したLeaf Packageを受理しました。");
    }

    // Contract bytesを壊す。
    {
        auto sections = CopySections(decoded.value());
        auto& contract = sections[3].bytes;
        Require(!contract.empty(), "Contract Sectionが空です。");
        contract[contract.size() / 2] ^= std::byte{0x01};
        RequireRejected(Rewrite(std::move(sections)),
            "ABI 2.7 Readerが破損したContractを受理しました。");
    }

    // Verified Decision bytesを壊す。
    {
        auto sections = CopySections(decoded.value());
        auto& decision = sections[4].bytes;
        Require(!decision.empty(), "Verified Decision Sectionが空です。");
        decision[decision.size() / 2] ^= std::byte{0x01};
        RequireRejected(Rewrite(std::move(sections)),
            "ABI 2.7 Readerが破損したVerified Decisionを受理しました。");
    }

    // Verification Certificate bytesを壊す。
    {
        auto sections = CopySections(decoded.value());
        auto& certificate = sections[5].bytes;
        Require(!certificate.empty(), "Verification Certificate Sectionが空です。");
        certificate[certificate.size() / 2] ^= std::byte{0x01};
        RequireRejected(Rewrite(std::move(sections)),
            "ABI 2.7 Readerが破損したVerification Certificateを受理しました。");
    }

    // ManifestのComposition Core digestを壊す。
    {
        auto sections = CopySections(decoded.value());
        auto& manifest = sections[0].bytes;
        Require(manifest.size() > 56, "Manifestが短すぎます。");
        manifest[56] ^= std::byte{0x01};
        RequireRejected(Rewrite(std::move(sections)),
            "ABI 2.7 Readerが不一致Composition Core digestを受理しました。");
    }

    // ManifestのFrozen Composition identityを壊す。
    {
        auto sections = CopySections(decoded.value());
        auto& manifest = sections[0].bytes;
        Require(manifest.size() > 88, "Manifestが短すぎます。");
        manifest[88] ^= std::byte{0x01};
        RequireRejected(Rewrite(std::move(sections)),
            "ABI 2.7 Readerが不一致Frozen Composition identityを受理しました。");
    }

    // Authority Ledgerだけを書き換え、Composition Coreは維持する。
    {
        auto sections = CopySections(decoded.value());
        auto& ledger = sections[6].bytes;
        Require(ledger.size() > 40, "Authority Ledgerが短すぎます。");
        ledger[40] ^= std::byte{0x01};
        RequireRejected(Rewrite(std::move(sections)),
            "ABI 2.7 Readerが不一致Authority Ledgerを受理しました。");
    }

    // Dynamic identityを壊す。
    {
        auto sections = CopySections(decoded.value());
        auto& dynamic = sections[7].bytes;
        Require(dynamic.size() > 56, "Dynamic Contractが短すぎます。");
        dynamic[56] ^= std::byte{0x01};
        RequireRejected(Rewrite(std::move(sections)),
            "ABI 2.7 Readerが不一致Dynamic identityを受理しました。");
    }

    // 重複SectionはWriter境界でも拒否する。
    {
        auto sections = CopySections(decoded.value());
        sections.push_back(sections.front());
        auto duplicate = WriteSectionedArtifact(
            artifact::FrozenCompositionAbi2Magic,
            artifact::FrozenCompositionAbi2FormatMajor,
            artifact::FrozenCompositionAbi2FormatMinor,
            std::move(sections));
        Require(!duplicate, "Section Writerが重複Sectionを受理しました。");
    }

    // file digestを再計算した範囲外offset。Readerがsubspan前に拒否することを証明する。
    {
        std::vector<std::byte> bytes(validBytes.begin(), validBytes.end());
        constexpr std::size_t LeafTableDescriptor =
            ArtifactHeaderBytes + ArtifactSectionDescriptorBytes;
        constexpr std::size_t FileOffsetField = 16;
        PatchU64(bytes, LeafTableDescriptor + FileOffsetField,
            std::numeric_limits<std::uint64_t>::max() - 7u);
        RefreshFileDigest(bytes);
        RequireRejected(std::move(bytes),
            "Section Readerが範囲外offsetを受理しました。");
    }

    // descriptor kindを重複させ、file digestだけ正しく更新する。
    {
        std::vector<std::byte> bytes(validBytes.begin(), validBytes.end());
        constexpr std::size_t SecondDescriptor =
            ArtifactHeaderBytes + ArtifactSectionDescriptorBytes;
        PatchU32(bytes, SecondDescriptor, 1);
        RefreshFileDigest(bytes);
        RequireRejected(std::move(bytes),
            "Section Readerが重複kindを受理しました。");
    }
}
}
