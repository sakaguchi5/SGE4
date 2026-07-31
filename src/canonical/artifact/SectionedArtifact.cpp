#include "./SectionedArtifact.h"

#include "../base/BinaryIO.h"

#include <algorithm>
#include <limits>

namespace sge4
{
namespace
{
using sge4::base::BinaryReader;
using sge4::base::BinaryWriter;

template<class T>
[[nodiscard]] sge4::base::Expected<T, Error> Fail(std::string stage, std::string message)
{
    return sge4::base::Failure<T, Error>({std::move(stage), std::move(message)});
}

[[nodiscard]] bool IsPowerOfTwo(std::uint32_t value) noexcept
{
    return value != 0 && (value & (value - 1u)) == 0;
}

[[nodiscard]] Digest256 ComputeSemanticDigest(std::span<const SectionInput> sections)
{
    BinaryWriter writer;
    const auto executionSectionCount = static_cast<std::uint32_t>(std::count_if(
        sections.begin(), sections.end(), [](const SectionInput& section)
        {
            return (section.flags & static_cast<std::uint16_t>(SectionFlags::ExecutionAffecting)) != 0;
        }));
    writer.WriteU32(executionSectionCount);
    for (const auto& section : sections)
    {
        if ((section.flags & static_cast<std::uint16_t>(SectionFlags::ExecutionAffecting)) == 0)
            continue;
        writer.WriteU32(section.kind);
        writer.WriteU16(section.schemaVersion);
        writer.WriteU16(section.flags);
        writer.WriteU64(section.bytes.size());
        const auto digest = sge4::base::Sha256(section.bytes);
        writer.WriteBytes(digest);
    }
    return sge4::base::Sha256(writer.Bytes());
}

[[nodiscard]] Digest256 ComputeFileDigest(std::span<const std::byte> bytes)
{
    std::vector<std::byte> canonical(bytes.begin(), bytes.end());
    if (canonical.size() >= ArtifactFileDigestOffset + 32)
        std::fill(canonical.begin() + static_cast<std::ptrdiff_t>(ArtifactFileDigestOffset),
            canonical.begin() + static_cast<std::ptrdiff_t>(ArtifactFileDigestOffset + 32), std::byte{0});
    return sge4::base::Sha256(canonical);
}
}

const SectionView* SectionedArtifact::FindSection(std::uint32_t kind) const noexcept
{
    const auto it = std::lower_bound(sections_.begin(), sections_.end(), kind,
        [](const SectionView& section, std::uint32_t value) { return section.kind < value; });
    return it != sections_.end() && it->kind == kind ? &*it : nullptr;
}

Digest256 ComputeDomainDigest(
    std::string_view domain,
    std::uint32_t schemaVersion,
    std::span<const std::byte> payload)
{
    BinaryWriter writer;
    writer.WriteCountU32(domain.size());
    writer.WriteBytes(std::as_bytes(std::span(domain.data(), domain.size())));
    writer.WriteU32(schemaVersion);
    writer.WriteU64(payload.size());
    writer.WriteBytes(payload);
    return sge4::base::Sha256(writer.Bytes());
}

sge4::base::Expected<std::vector<std::byte>, Error> WriteSectionedArtifact(
    const std::array<std::byte, 8>& magic,
    std::uint16_t formatMajor,
    std::uint16_t formatMinor,
    std::vector<SectionInput> sections)
{
    if (formatMajor == 0)
        return Fail<std::vector<std::byte>>("UnifiedWriter", "入力または内部状態が無効であるか、契約条件を満たしていません。");
    std::sort(sections.begin(), sections.end(), [](const auto& a, const auto& b) { return a.kind < b.kind; });
    for (std::size_t index = 0; index < sections.size(); ++index)
    {
        if (sections[index].kind == 0)
            return Fail<std::vector<std::byte>>("UnifiedWriter", "Sectionが検証または実行の契約に違反しています。");
        if (index != 0 && sections[index - 1].kind == sections[index].kind)
            return Fail<std::vector<std::byte>>("UnifiedWriter", "Sectionが検証または実行の契約に違反しています。");
        if (!IsPowerOfTwo(sections[index].alignment))
            return Fail<std::vector<std::byte>>("UnifiedWriter", "Sectionが検証または実行の契約に違反しています。");
    }

    const auto semanticDigest = ComputeSemanticDigest(sections);
    BinaryWriter writer;
    writer.WriteBytes(magic);
    writer.WriteU16(formatMajor);
    writer.WriteU16(formatMinor);
    writer.WriteU32(ArtifactHeaderBytes);
    writer.WriteU32(ArtifactSectionDescriptorBytes);
    writer.WriteCountU32(sections.size());
    writer.WriteU32(0);
    writer.WriteU32(0);
    writer.WriteU64(0);
    writer.WriteU64(ArtifactHeaderBytes);
    writer.WriteBytes(semanticDigest);
    writer.WriteZeroes(32);
    writer.WriteZeroes(16);
    if (writer.Size() != ArtifactHeaderBytes)
        return Fail<std::vector<std::byte>>("UnifiedWriter", "HeaderがCanonicalな順序または識別子規則に違反しています。");

    const auto descriptorStart = writer.Size();
    writer.WriteZeroes(sections.size() * ArtifactSectionDescriptorBytes);

    struct Encoded final
    {
        SectionInput input;
        std::uint64_t offset = 0;
        Digest256 digest{};
    };
    std::vector<Encoded> encoded;
    encoded.reserve(sections.size());
    for (auto& section : sections)
    {
        writer.Align(section.alignment);
        Encoded entry;
        entry.offset = writer.Size();
        entry.digest = sge4::base::Sha256(section.bytes);
        entry.input = std::move(section);
        writer.WriteBytes(entry.input.bytes);
        encoded.push_back(std::move(entry));
    }

    for (std::size_t index = 0; index < encoded.size(); ++index)
    {
        const auto offset = descriptorStart + index * ArtifactSectionDescriptorBytes;
        BinaryWriter descriptor;
        descriptor.WriteU32(encoded[index].input.kind);
        descriptor.WriteU16(encoded[index].input.schemaVersion);
        descriptor.WriteU16(encoded[index].input.flags);
        descriptor.WriteU32(encoded[index].input.alignment);
        descriptor.WriteU32(0);
        descriptor.WriteU64(encoded[index].offset);
        descriptor.WriteU64(encoded[index].input.bytes.size());
        descriptor.WriteBytes(encoded[index].digest);
        if (descriptor.Size() != ArtifactSectionDescriptorBytes)
            return Fail<std::vector<std::byte>>("UnifiedWriter", "入力または内部状態がCanonicalな順序または識別子規則に違反しています。");
        writer.PatchBytes(offset, descriptor.Bytes());
    }

    writer.PatchU64(32, writer.Size());
    auto result = std::move(writer).Take();
    const auto fileDigest = ComputeFileDigest(result);
    std::copy(fileDigest.begin(), fileDigest.end(),
        result.begin() + static_cast<std::ptrdiff_t>(ArtifactFileDigestOffset));
    return sge4::base::Success<std::vector<std::byte>, Error>(std::move(result));
}

sge4::base::Expected<SectionedArtifact, Error> ReadSectionedArtifact(
    std::span<const std::byte> bytes,
    const std::array<std::byte, 8>& expectedMagic,
    std::uint16_t expectedMajor)
{
    if (bytes.size() < ArtifactHeaderBytes)
        return Fail<SectionedArtifact>("UnifiedReader", "Fileが検証または実行の契約に違反しています。");
    BinaryReader reader(bytes);
    auto magic = reader.ReadBytes(8);
    if (!magic || !std::equal(magic.value().begin(), magic.value().end(), expectedMagic.begin()))
        return Fail<SectionedArtifact>("UnifiedReader", "入力または内部状態が検証または実行の契約に違反しています。");
    auto major = reader.ReadU16(); auto minor = reader.ReadU16();
    auto headerBytes = reader.ReadU32(); auto descriptorBytes = reader.ReadU32();
    auto sectionCount = reader.ReadU32(); auto flags = reader.ReadU32(); auto reserved = reader.ReadU32();
    auto fileBytes = reader.ReadU64(); auto tableOffset = reader.ReadU64();
    auto semanticDigestBytes = reader.ReadBytes(32); auto fileDigestBytes = reader.ReadBytes(32); auto reservedTail = reader.ReadBytes(16);
    if (!major || !minor || !headerBytes || !descriptorBytes || !sectionCount || !flags || !reserved ||
        !fileBytes || !tableOffset || !semanticDigestBytes || !fileDigestBytes || !reservedTail)
        return Fail<SectionedArtifact>("UnifiedReader", "Headerが検証または実行の契約に違反しています。");
    if (major.value() != expectedMajor)
        return Fail<SectionedArtifact>("UnifiedReader", "入力または内部状態に未対応または禁止された値が含まれています。");
    if (headerBytes.value() != ArtifactHeaderBytes || descriptorBytes.value() != ArtifactSectionDescriptorBytes)
        return Fail<SectionedArtifact>("UnifiedReader", "HeaderがCanonicalな順序または識別子規則に違反しています。");
    if (fileBytes.value() != bytes.size() || tableOffset.value() != ArtifactHeaderBytes)
        return Fail<SectionedArtifact>("UnifiedReader", "SectionがCanonicalな順序または識別子規則に違反しています。");
    if (flags.value() != 0 || reserved.value() != 0 ||
        std::any_of(reservedTail.value().begin(), reservedTail.value().end(), [](std::byte b) { return b != std::byte{0}; }))
        return Fail<SectionedArtifact>("UnifiedReader", "Headerが検証または実行の契約に違反しています。");
    if (sectionCount.value() > (std::numeric_limits<std::size_t>::max() - ArtifactHeaderBytes) / ArtifactSectionDescriptorBytes)
        return Fail<SectionedArtifact>("UnifiedReader", "Sectionが検証または実行の契約に違反しています。");
    const auto tableBytes = static_cast<std::size_t>(sectionCount.value()) * ArtifactSectionDescriptorBytes;
    if (ArtifactHeaderBytes + tableBytes > bytes.size())
        return Fail<SectionedArtifact>("UnifiedReader", "Sectionが検証または実行の契約に違反しています。");

    SectionedArtifact artifact;
    artifact.formatMajor_ = major.value();
    artifact.formatMinor_ = minor.value();
    std::copy(semanticDigestBytes.value().begin(), semanticDigestBytes.value().end(), artifact.semanticDigest_.begin());
    std::copy(fileDigestBytes.value().begin(), fileDigestBytes.value().end(), artifact.fileDigest_.begin());
    if (ComputeFileDigest(bytes) != artifact.fileDigest_)
        return Fail<SectionedArtifact>("UnifiedReader", "DigestがCanonicalな順序または識別子規則に違反しています。");
    artifact.storage_.assign(bytes.begin(), bytes.end());

    std::uint32_t previousKind = 0;
    std::uint64_t previousEnd = ArtifactHeaderBytes + tableBytes;
    std::vector<SectionInput> semanticSections;
    for (std::uint32_t index = 0; index < sectionCount.value(); ++index)
    {
        const auto descriptorOffset = ArtifactHeaderBytes + static_cast<std::size_t>(index) * ArtifactSectionDescriptorBytes;
        BinaryReader d(std::span<const std::byte>(artifact.storage_).subspan(descriptorOffset, ArtifactSectionDescriptorBytes));
        auto kind = d.ReadU32(); auto schema = d.ReadU16(); auto sectionFlags = d.ReadU16();
        auto alignment = d.ReadU32(); auto sectionReserved = d.ReadU32();
        auto offset = d.ReadU64(); auto size = d.ReadU64(); auto digestBytes = d.ReadBytes(32);
        if (!kind || !schema || !sectionFlags || !alignment || !sectionReserved || !offset || !size || !digestBytes)
            return Fail<SectionedArtifact>("UnifiedReader", "Sectionが検証または実行の契約に違反しています。");
        if (kind.value() == 0 || kind.value() <= previousKind)
            return Fail<SectionedArtifact>("UnifiedReader", "Sectionが検証または実行の契約に違反しています。");
        if (!IsPowerOfTwo(alignment.value()) || sectionReserved.value() != 0 || offset.value() % alignment.value() != 0)
            return Fail<SectionedArtifact>("UnifiedReader", "Sectionが検証または実行の契約に違反しています。");
        if (offset.value() < previousEnd ||
            offset.value() > artifact.storage_.size() ||
            size.value() > artifact.storage_.size() - offset.value())
            return Fail<SectionedArtifact>("UnifiedReader", "Sectionが検証または実行の契約に違反しています。");
        const auto padding = std::span<const std::byte>(artifact.storage_).subspan(
            static_cast<std::size_t>(previousEnd),
            static_cast<std::size_t>(offset.value() - previousEnd));
        if (std::ranges::any_of(padding, [](std::byte value) { return value != std::byte{0}; }))
            return Fail<SectionedArtifact>("UnifiedReader", "Section paddingがCanonicalではありません。");
        Digest256 digest{};
        std::copy(digestBytes.value().begin(), digestBytes.value().end(), digest.begin());
        const auto sectionSpan = std::span<const std::byte>(artifact.storage_).subspan(
            static_cast<std::size_t>(offset.value()), static_cast<std::size_t>(size.value()));
        if (sge4::base::Sha256(sectionSpan) != digest)
            return Fail<SectionedArtifact>("UnifiedReader", "SectionがCanonicalな順序または識別子規則に違反しています。");
        artifact.sections_.push_back({kind.value(), schema.value(), sectionFlags.value(), alignment.value(),
            offset.value(), size.value(), digest, sectionSpan});
        if ((sectionFlags.value() & static_cast<std::uint16_t>(SectionFlags::ExecutionAffecting)) != 0)
        {
            SectionInput input;
            input.kind = kind.value(); input.schemaVersion = schema.value(); input.flags = sectionFlags.value();
            input.alignment = alignment.value(); input.bytes.assign(sectionSpan.begin(), sectionSpan.end());
            semanticSections.push_back(std::move(input));
        }
        previousKind = kind.value();
        previousEnd = offset.value() + size.value();
    }
    if (ComputeSemanticDigest(semanticSections) != artifact.semanticDigest_)
        return Fail<SectionedArtifact>("UnifiedReader", "DigestがCanonicalな順序または識別子規則に違反しています。");
    return sge4::base::Success<SectionedArtifact, Error>(std::move(artifact));
}

std::vector<std::byte> DigestBytes(const Digest256& digest)
{
    return {digest.begin(), digest.end()};
}

bool DigestEqual(std::span<const std::byte> bytes, const Digest256& digest) noexcept
{
    return bytes.size() == digest.size() && std::equal(bytes.begin(), bytes.end(), digest.begin());
}
}
