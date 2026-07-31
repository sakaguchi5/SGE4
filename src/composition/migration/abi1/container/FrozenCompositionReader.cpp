#include "./FrozenCompositionReader.h"

#include "../../../../canonical/base/BinaryIO.h"
#include "../../../../canonical/base/CheckedMath.h"
#include "../../../../canonical/base/Sha256.h"
#include "../../../../leaf/artifact/package/PackageReader.h"
#include "./FrozenCompositionDigest.h"

#include <algorithm>
#include <cstring>
#include <memory>

namespace sge4::composition
{
namespace
{
FrozenCompositionError Error(
    FrozenCompositionErrorCode code,
    std::string message,
    FrozenCompositionSectionKind section = {},
    std::uint32_t recordIndex = FrozenCompositionInvalidIndex)
{
    return {code, section, recordIndex, std::move(message)};
}

template<class T>
bool Pull(base::Expected<T, std::string>& result, T& out, FrozenCompositionError& error)
{
    if (!result)
    {
        error = Error(FrozenCompositionErrorCode::InvalidHeader, result.error());
        return false;
    }
    out = result.value();
    return true;
}

bool PullBytes(
    base::Expected<std::span<const std::byte>, std::string>& result,
    std::span<const std::byte>& out,
    FrozenCompositionError& error)
{
    if (!result)
    {
        error = Error(FrozenCompositionErrorCode::InvalidHeader, result.error());
        return false;
    }
    out = result.value();
    return true;
}

[[nodiscard]] bool AllZero(std::span<const std::byte> bytes) noexcept
{
    return std::all_of(bytes.begin(), bytes.end(), [](std::byte value) { return value == std::byte{0}; });
}

[[nodiscard]] bool IsZeroDigest(const base::Digest256& digest) noexcept
{
    return AllZero(digest);
}

[[nodiscard]] bool DigestLess(const base::Digest256& left, const base::Digest256& right) noexcept
{
    return std::lexicographical_compare(left.begin(), left.end(), right.begin(), right.end());
}

bool ReadDigest(base::BinaryReader& reader, base::Digest256& digest, FrozenCompositionError& error)
{
    auto raw = reader.ReadBytes(digest.size());
    std::span<const std::byte> bytes;
    if (!PullBytes(raw, bytes, error)) return false;
    std::copy(bytes.begin(), bytes.end(), digest.begin());
    return true;
}

base::Expected<FrozenCompositionHeader, FrozenCompositionError> ReadHeader(std::span<const std::byte> bytes)
{
    if (bytes.size() < FrozenCompositionHeaderBytes)
    {
        return base::Failure<FrozenCompositionHeader, FrozenCompositionError>(Error(
            FrozenCompositionErrorCode::FileSizeMismatch,
            "検証または実行の契約に違反しています。"));
    }
    if (!std::equal(FrozenCompositionMagic.begin(), FrozenCompositionMagic.end(), bytes.begin()))
    {
        return base::Failure<FrozenCompositionHeader, FrozenCompositionError>(Error(
            FrozenCompositionErrorCode::BadMagic,
            "Compositionが検証または実行の契約に違反しています。"));
    }

    base::BinaryReader reader(bytes.subspan(8, FrozenCompositionHeaderBytes - 8));
    FrozenCompositionHeader header;
    FrozenCompositionError readError;
    auto u16 = reader.ReadU16(); if (!Pull(u16, header.formatMajor, readError)) return base::Failure<FrozenCompositionHeader, FrozenCompositionError>(readError);
    u16 = reader.ReadU16(); if (!Pull(u16, header.formatMinor, readError)) return base::Failure<FrozenCompositionHeader, FrozenCompositionError>(readError);
    u16 = reader.ReadU16(); if (!Pull(u16, header.headerBytes, readError)) return base::Failure<FrozenCompositionHeader, FrozenCompositionError>(readError);
    u16 = reader.ReadU16(); if (!Pull(u16, header.sectionDescriptorBytes, readError)) return base::Failure<FrozenCompositionHeader, FrozenCompositionError>(readError);
    auto u32 = reader.ReadU32(); if (!Pull(u32, header.flags, readError)) return base::Failure<FrozenCompositionHeader, FrozenCompositionError>(readError);
    u32 = reader.ReadU32(); if (!Pull(u32, header.endianTag, readError)) return base::Failure<FrozenCompositionHeader, FrozenCompositionError>(readError);
    u32 = reader.ReadU32(); if (!Pull(u32, header.digestAlgorithm, readError)) return base::Failure<FrozenCompositionHeader, FrozenCompositionError>(readError);
    std::uint32_t reserved0 = 1; u32 = reader.ReadU32(); if (!Pull(u32, reserved0, readError)) return base::Failure<FrozenCompositionHeader, FrozenCompositionError>(readError);
    auto u64 = reader.ReadU64(); if (!Pull(u64, header.fileBytes, readError)) return base::Failure<FrozenCompositionHeader, FrozenCompositionError>(readError);
    u64 = reader.ReadU64(); if (!Pull(u64, header.sectionTableOffset, readError)) return base::Failure<FrozenCompositionHeader, FrozenCompositionError>(readError);
    u32 = reader.ReadU32(); if (!Pull(u32, header.sectionCount, readError)) return base::Failure<FrozenCompositionHeader, FrozenCompositionError>(readError);
    std::uint32_t reserved1 = 1; u32 = reader.ReadU32(); if (!Pull(u32, reserved1, readError)) return base::Failure<FrozenCompositionHeader, FrozenCompositionError>(readError);
    if (!ReadDigest(reader, header.semanticDigest, readError)) return base::Failure<FrozenCompositionHeader, FrozenCompositionError>(readError);
    if (!ReadDigest(reader, header.decisionDigest, readError)) return base::Failure<FrozenCompositionHeader, FrozenCompositionError>(readError);
    if (!ReadDigest(reader, header.certificateDigest, readError)) return base::Failure<FrozenCompositionHeader, FrozenCompositionError>(readError);
    if (!ReadDigest(reader, header.fileDigest, readError)) return base::Failure<FrozenCompositionHeader, FrozenCompositionError>(readError);
    auto reserved = reader.ReadBytes(8);
    std::span<const std::byte> reservedBytes;
    if (!PullBytes(reserved, reservedBytes, readError)) return base::Failure<FrozenCompositionHeader, FrozenCompositionError>(readError);

    if (header.formatMajor != FrozenCompositionFormatMajor ||
        header.formatMinor > FrozenCompositionFormatMinor ||
        header.headerBytes != FrozenCompositionHeaderBytes ||
        header.sectionDescriptorBytes != FrozenCompositionSectionDescriptorBytes)
    {
        return base::Failure<FrozenCompositionHeader, FrozenCompositionError>(Error(
            FrozenCompositionErrorCode::UnsupportedContainerVersion,
            "Compositionが検証または実行の契約に違反しています。"));
    }
    if (header.endianTag != FrozenCompositionEndianTag)
    {
        return base::Failure<FrozenCompositionHeader, FrozenCompositionError>(Error(
            FrozenCompositionErrorCode::WrongEndianness,
            "検証または実行の契約に違反しています。"));
    }
    if (header.digestAlgorithm != FrozenCompositionDigestAlgorithmSha256 ||
        header.flags != 0 || reserved0 != 0 || reserved1 != 0 || !AllZero(reservedBytes))
    {
        return base::Failure<FrozenCompositionHeader, FrozenCompositionError>(Error(
            FrozenCompositionErrorCode::InvalidHeader,
            "Digestが検証または実行の契約に違反しています。"));
    }
    if (header.fileBytes != bytes.size())
    {
        return base::Failure<FrozenCompositionHeader, FrozenCompositionError>(Error(
            FrozenCompositionErrorCode::FileSizeMismatch,
            "Fileが検証または実行の契約に違反しています。"));
    }
    if (header.sectionTableOffset != FrozenCompositionHeaderBytes || header.sectionCount != 6)
    {
        return base::Failure<FrozenCompositionHeader, FrozenCompositionError>(Error(
            FrozenCompositionErrorCode::InvalidSectionTable,
            "SectionがCanonicalな順序または識別子規則に違反しています。"));
    }
    return base::Success<FrozenCompositionHeader, FrozenCompositionError>(header);
}

base::Expected<std::vector<FrozenCompositionSectionDescriptor>, FrozenCompositionError> ReadDescriptors(
    std::span<const std::byte> bytes,
    const FrozenCompositionHeader& header)
{
    const std::uint64_t tableBytes =
        static_cast<std::uint64_t>(header.sectionCount) * FrozenCompositionSectionDescriptorBytes;
    std::uint64_t tableEnd = 0;
    if (!base::CheckedAdd(header.sectionTableOffset, tableBytes, tableEnd) || tableEnd > bytes.size())
    {
        return base::Failure<std::vector<FrozenCompositionSectionDescriptor>, FrozenCompositionError>(Error(
            FrozenCompositionErrorCode::InvalidSectionTable,
            "検証または実行の契約に違反しています。"));
    }

    base::BinaryReader reader(bytes.subspan(
        static_cast<std::size_t>(header.sectionTableOffset),
        static_cast<std::size_t>(tableBytes)));
    std::vector<FrozenCompositionSectionDescriptor> descriptors;
    descriptors.reserve(header.sectionCount);
    std::uint64_t expectedOffset = tableEnd;
    for (std::uint32_t index = 0; index < header.sectionCount; ++index)
    {
        FrozenCompositionSectionDescriptor descriptor;
        FrozenCompositionError readError;
        std::uint32_t kind = 0;
        auto u32 = reader.ReadU32(); if (!Pull(u32, kind, readError)) return base::Failure<std::vector<FrozenCompositionSectionDescriptor>, FrozenCompositionError>(readError);
        descriptor.kind = static_cast<FrozenCompositionSectionKind>(kind);
        auto u16 = reader.ReadU16(); if (!Pull(u16, descriptor.schemaVersion, readError)) return base::Failure<std::vector<FrozenCompositionSectionDescriptor>, FrozenCompositionError>(readError);
        u16 = reader.ReadU16(); if (!Pull(u16, descriptor.descriptorBytes, readError)) return base::Failure<std::vector<FrozenCompositionSectionDescriptor>, FrozenCompositionError>(readError);
        u32 = reader.ReadU32(); if (!Pull(u32, descriptor.flags, readError)) return base::Failure<std::vector<FrozenCompositionSectionDescriptor>, FrozenCompositionError>(readError);
        u32 = reader.ReadU32(); if (!Pull(u32, descriptor.alignment, readError)) return base::Failure<std::vector<FrozenCompositionSectionDescriptor>, FrozenCompositionError>(readError);
        auto u64 = reader.ReadU64(); if (!Pull(u64, descriptor.fileOffset, readError)) return base::Failure<std::vector<FrozenCompositionSectionDescriptor>, FrozenCompositionError>(readError);
        u64 = reader.ReadU64(); if (!Pull(u64, descriptor.storedBytes, readError)) return base::Failure<std::vector<FrozenCompositionSectionDescriptor>, FrozenCompositionError>(readError);
        u64 = reader.ReadU64(); if (!Pull(u64, descriptor.logicalBytes, readError)) return base::Failure<std::vector<FrozenCompositionSectionDescriptor>, FrozenCompositionError>(readError);
        u32 = reader.ReadU32(); if (!Pull(u32, descriptor.elementCount, readError)) return base::Failure<std::vector<FrozenCompositionSectionDescriptor>, FrozenCompositionError>(readError);
        u32 = reader.ReadU32(); if (!Pull(u32, descriptor.elementStride, readError)) return base::Failure<std::vector<FrozenCompositionSectionDescriptor>, FrozenCompositionError>(readError);
        if (!ReadDigest(reader, descriptor.sectionDigest, readError)) return base::Failure<std::vector<FrozenCompositionSectionDescriptor>, FrozenCompositionError>(readError);

        const auto expectedKind = static_cast<FrozenCompositionSectionKind>(index + 1);
        if (descriptor.kind != expectedKind)
        {
            return base::Failure<std::vector<FrozenCompositionSectionDescriptor>, FrozenCompositionError>(Error(
                FrozenCompositionErrorCode::InvalidSectionTable,
                "SectionがCanonicalな順序または識別子規則に違反しています。",
                descriptor.kind));
        }
        if (descriptor.schemaVersion != 1 ||
            descriptor.descriptorBytes != FrozenCompositionSectionDescriptorBytes ||
            descriptor.flags != RequiredExecutionFlags() ||
            descriptor.alignment != SectionAlignment ||
            descriptor.logicalBytes != descriptor.storedBytes)
        {
            return base::Failure<std::vector<FrozenCompositionSectionDescriptor>, FrozenCompositionError>(Error(
                FrozenCompositionErrorCode::UnsupportedContainerVersion,
                "検証または実行の契約に違反しています。",
                descriptor.kind));
        }
        if (descriptor.elementCount != 0 &&
            (descriptor.elementStride == 0 ||
             static_cast<std::uint64_t>(descriptor.elementCount) * descriptor.elementStride != descriptor.logicalBytes))
        {
            return base::Failure<std::vector<FrozenCompositionSectionDescriptor>, FrozenCompositionError>(Error(
                FrozenCompositionErrorCode::InvalidRecordStride,
                "検証または実行の契約に違反しています。",
                descriptor.kind));
        }

        const auto alignedOffset = base::AlignUp(expectedOffset, SectionAlignment);
        if (descriptor.fileOffset != alignedOffset)
        {
            return base::Failure<std::vector<FrozenCompositionSectionDescriptor>, FrozenCompositionError>(Error(
                FrozenCompositionErrorCode::NonCanonicalEncoding,
                "検証または実行の契約に違反しています。",
                descriptor.kind));
        }
        if (alignedOffset > expectedOffset && !AllZero(bytes.subspan(
                static_cast<std::size_t>(expectedOffset),
                static_cast<std::size_t>(alignedOffset - expectedOffset))))
        {
            return base::Failure<std::vector<FrozenCompositionSectionDescriptor>, FrozenCompositionError>(Error(
                FrozenCompositionErrorCode::NonCanonicalEncoding,
                "Sectionが検証または実行の契約に違反しています。",
                descriptor.kind));
        }
        expectedOffset = alignedOffset;
        std::uint64_t sectionEnd = 0;
        if (!base::CheckedAdd(descriptor.fileOffset, descriptor.storedBytes, sectionEnd) || sectionEnd > bytes.size())
        {
            return base::Failure<std::vector<FrozenCompositionSectionDescriptor>, FrozenCompositionError>(Error(
                FrozenCompositionErrorCode::SectionOutOfBounds,
                "Compositionが検証または実行の契約に違反しています。",
                descriptor.kind));
        }
        expectedOffset = sectionEnd;
        descriptors.push_back(descriptor);
    }
    if (expectedOffset != bytes.size())
    {
        return base::Failure<std::vector<FrozenCompositionSectionDescriptor>, FrozenCompositionError>(Error(
            FrozenCompositionErrorCode::NonCanonicalEncoding,
            "SectionがCanonicalな順序または識別子規則に違反しています。"));
    }
    return base::Success<std::vector<FrozenCompositionSectionDescriptor>, FrozenCompositionError>(std::move(descriptors));
}

base::Expected<FrozenCompositionManifest, FrozenCompositionError> ReadManifest(
    const FrozenCompositionSectionView& section)
{
    if (section.descriptor.elementCount != 1 ||
        section.descriptor.elementStride != FrozenCompositionManifestBytes ||
        section.bytes.size() != FrozenCompositionManifestBytes)
    {
        return base::Failure<FrozenCompositionManifest, FrozenCompositionError>(Error(
            FrozenCompositionErrorCode::InvalidManifest,
            "Manifestが検証または実行の契約に違反しています。",
            FrozenCompositionSectionKind::Manifest));
    }

    base::BinaryReader reader(section.bytes);
    FrozenCompositionManifest manifest;
    FrozenCompositionError readError;
    auto u32 = reader.ReadU32(); if (!Pull(u32, manifest.leafCount, readError)) return base::Failure<FrozenCompositionManifest, FrozenCompositionError>(readError);
    u32 = reader.ReadU32(); if (!Pull(u32, manifest.presenterLeafId, readError)) return base::Failure<FrozenCompositionManifest, FrozenCompositionError>(readError);
    u32 = reader.ReadU32(); if (!Pull(u32, manifest.flags, readError)) return base::Failure<FrozenCompositionManifest, FrozenCompositionError>(readError);
    std::uint32_t reserved0 = 1; u32 = reader.ReadU32(); if (!Pull(u32, reserved0, readError)) return base::Failure<FrozenCompositionManifest, FrozenCompositionError>(readError);
    auto u64 = reader.ReadU64(); if (!Pull(u64, manifest.contractBytes, readError)) return base::Failure<FrozenCompositionManifest, FrozenCompositionError>(readError);
    u64 = reader.ReadU64(); if (!Pull(u64, manifest.verifiedDecisionBytes, readError)) return base::Failure<FrozenCompositionManifest, FrozenCompositionError>(readError);
    u64 = reader.ReadU64(); if (!Pull(u64, manifest.certificateBytes, readError)) return base::Failure<FrozenCompositionManifest, FrozenCompositionError>(readError);
    std::uint64_t reserved1 = 1; u64 = reader.ReadU64(); if (!Pull(u64, reserved1, readError)) return base::Failure<FrozenCompositionManifest, FrozenCompositionError>(readError);
    auto reserved = reader.ReadBytes(16);
    std::span<const std::byte> reservedBytes;
    if (!PullBytes(reserved, reservedBytes, readError)) return base::Failure<FrozenCompositionManifest, FrozenCompositionError>(readError);

    if (manifest.leafCount < 2 || manifest.flags != 0 || reserved0 != 0 || reserved1 != 0 ||
        !AllZero(reservedBytes) || manifest.contractBytes == 0 ||
        manifest.verifiedDecisionBytes == 0 || manifest.certificateBytes == 0)
    {
        return base::Failure<FrozenCompositionManifest, FrozenCompositionError>(Error(
            FrozenCompositionErrorCode::InvalidManifest,
            "Manifestが検証または実行の契約に違反しています。",
            FrozenCompositionSectionKind::Manifest));
    }
    if (manifest.presenterLeafId != FrozenCompositionInvalidIndex &&
        manifest.presenterLeafId >= manifest.leafCount)
    {
        return base::Failure<FrozenCompositionManifest, FrozenCompositionError>(Error(
            FrozenCompositionErrorCode::InvalidManifest,
            "Leafが検証または実行の契約に違反しています。",
            FrozenCompositionSectionKind::Manifest));
    }
    return base::Success<FrozenCompositionManifest, FrozenCompositionError>(manifest);
}

base::Expected<std::vector<EmbeddedLeafRecord>, FrozenCompositionError> ReadLeafRecords(
    const FrozenCompositionSectionView& table,
    const FrozenCompositionSectionView& leafBytes,
    const FrozenCompositionManifest& manifest)
{
    if (table.descriptor.elementCount != manifest.leafCount ||
        table.descriptor.elementStride != EmbeddedLeafRecordBytes ||
        table.bytes.size() != static_cast<std::uint64_t>(manifest.leafCount) * EmbeddedLeafRecordBytes)
    {
        return base::Failure<std::vector<EmbeddedLeafRecord>, FrozenCompositionError>(Error(
            FrozenCompositionErrorCode::InvalidLeafTable,
            "Manifestが検証または実行の契約に違反しています。",
            FrozenCompositionSectionKind::LeafTable));
    }
    if (leafBytes.descriptor.elementCount != 0 || leafBytes.descriptor.elementStride != 0)
    {
        return base::Failure<std::vector<EmbeddedLeafRecord>, FrozenCompositionError>(Error(
            FrozenCompositionErrorCode::InvalidLeafTable,
            "Sectionが検証または実行の契約に違反しています。",
            FrozenCompositionSectionKind::LeafBytes));
    }

    base::BinaryReader reader(table.bytes);
    std::vector<EmbeddedLeafRecord> records;
    records.reserve(manifest.leafCount);
    std::uint64_t expectedLeafOffset = 0;
    for (std::uint32_t index = 0; index < manifest.leafCount; ++index)
    {
        EmbeddedLeafRecord record;
        FrozenCompositionError readError;
        auto u32 = reader.ReadU32(); if (!Pull(u32, record.leafId, readError)) return base::Failure<std::vector<EmbeddedLeafRecord>, FrozenCompositionError>(readError);
        u32 = reader.ReadU32(); if (!Pull(u32, record.flags, readError)) return base::Failure<std::vector<EmbeddedLeafRecord>, FrozenCompositionError>(readError);
        if (!ReadDigest(reader, record.stableKey, readError)) return base::Failure<std::vector<EmbeddedLeafRecord>, FrozenCompositionError>(readError);
        auto u64 = reader.ReadU64(); if (!Pull(u64, record.byteOffset, readError)) return base::Failure<std::vector<EmbeddedLeafRecord>, FrozenCompositionError>(readError);
        u64 = reader.ReadU64(); if (!Pull(u64, record.byteSize, readError)) return base::Failure<std::vector<EmbeddedLeafRecord>, FrozenCompositionError>(readError);
        if (!ReadDigest(reader, record.executionDigest, readError)) return base::Failure<std::vector<EmbeddedLeafRecord>, FrozenCompositionError>(readError);
        if (!ReadDigest(reader, record.fileDigest, readError)) return base::Failure<std::vector<EmbeddedLeafRecord>, FrozenCompositionError>(readError);
        std::uint64_t reserved = 1; u64 = reader.ReadU64(); if (!Pull(u64, reserved, readError)) return base::Failure<std::vector<EmbeddedLeafRecord>, FrozenCompositionError>(readError);

        if (record.leafId != index || record.flags != 0 || reserved != 0 ||
            IsZeroDigest(record.stableKey) || record.byteSize == 0)
        {
            return base::Failure<std::vector<EmbeddedLeafRecord>, FrozenCompositionError>(Error(
                FrozenCompositionErrorCode::InvalidLeafTable,
                "LeafがCanonicalな順序または識別子規則に違反しています。",
                FrozenCompositionSectionKind::LeafTable,
                index));
        }
        if (!records.empty() && !DigestLess(records.back().stableKey, record.stableKey))
        {
            return base::Failure<std::vector<EmbeddedLeafRecord>, FrozenCompositionError>(Error(
                records.back().stableKey == record.stableKey ?
                    FrozenCompositionErrorCode::DuplicateStableKey :
                    FrozenCompositionErrorCode::NonCanonicalEncoding,
                "Leafが検証または実行の契約に違反しています。",
                FrozenCompositionSectionKind::LeafTable,
                index));
        }

        const auto alignedLeafOffset = base::AlignUp(expectedLeafOffset, SectionAlignment);
        if (alignedLeafOffset > expectedLeafOffset && !AllZero(leafBytes.bytes.subspan(
                static_cast<std::size_t>(expectedLeafOffset),
                static_cast<std::size_t>(alignedLeafOffset - expectedLeafOffset))))
        {
            return base::Failure<std::vector<EmbeddedLeafRecord>, FrozenCompositionError>(Error(
                FrozenCompositionErrorCode::NonCanonicalEncoding,
                "Leafが検証または実行の契約に違反しています。",
                FrozenCompositionSectionKind::LeafBytes,
                index));
        }
        expectedLeafOffset = alignedLeafOffset;
        if (record.byteOffset != expectedLeafOffset)
        {
            return base::Failure<std::vector<EmbeddedLeafRecord>, FrozenCompositionError>(Error(
                FrozenCompositionErrorCode::NonCanonicalEncoding,
                "検証または実行の契約に違反しています。",
                FrozenCompositionSectionKind::LeafBytes,
                index));
        }
        std::uint64_t leafEnd = 0;
        if (!base::CheckedAdd(record.byteOffset, record.byteSize, leafEnd) || leafEnd > leafBytes.bytes.size())
        {
            return base::Failure<std::vector<EmbeddedLeafRecord>, FrozenCompositionError>(Error(
                FrozenCompositionErrorCode::SectionOutOfBounds,
                "検証または実行の契約に違反しています。",
                FrozenCompositionSectionKind::LeafBytes,
                index));
        }
        const auto packageSpan = leafBytes.bytes.subspan(
            static_cast<std::size_t>(record.byteOffset),
            static_cast<std::size_t>(record.byteSize));
        const auto package = package::PackageReader::Read(packageSpan);
        if (!package)
        {
            return base::Failure<std::vector<EmbeddedLeafRecord>, FrozenCompositionError>(Error(
                FrozenCompositionErrorCode::InvalidEmbeddedLeaf,
                "Packageが検証または実行の契約に違反しています。" + package.error().message,
                FrozenCompositionSectionKind::LeafBytes,
                index));
        }
        const auto& packageHeader = package.value().Header();
        if (packageHeader.targetKind != package::TargetKindD3D12 ||
            packageHeader.targetSchemaVersion != EmbeddedSchemaVersion ||
            packageHeader.minimumRuntimeVersion != EmbeddedRuntimeVersion)
        {
            return base::Failure<std::vector<EmbeddedLeafRecord>, FrozenCompositionError>(Error(
                FrozenCompositionErrorCode::UnsupportedEmbeddedLeaf,
                "検証または実行の契約に違反しています。",
                FrozenCompositionSectionKind::LeafBytes,
                index));
        }
        if (packageHeader.executionDigest != record.executionDigest ||
            packageHeader.fileDigest != record.fileDigest)
        {
            return base::Failure<std::vector<EmbeddedLeafRecord>, FrozenCompositionError>(Error(
                FrozenCompositionErrorCode::DigestMismatch,
                "Packageが検証または実行の契約に違反しています。",
                FrozenCompositionSectionKind::LeafTable,
                index));
        }
        expectedLeafOffset = leafEnd;
        records.push_back(record);
    }
    if (expectedLeafOffset != leafBytes.bytes.size())
    {
        return base::Failure<std::vector<EmbeddedLeafRecord>, FrozenCompositionError>(Error(
            FrozenCompositionErrorCode::NonCanonicalEncoding,
            "Leafが検証または実行の契約に違反しています。",
            FrozenCompositionSectionKind::LeafBytes));
    }
    return base::Success<std::vector<EmbeddedLeafRecord>, FrozenCompositionError>(std::move(records));
}
}

base::Expected<FrozenComposition, FrozenCompositionError> FrozenCompositionReader::Read(
    std::span<const std::byte> bytes)
{
    return Read(std::vector<std::byte>(bytes.begin(), bytes.end()));
}

base::Expected<FrozenComposition, FrozenCompositionError> FrozenCompositionReader::Read(
    std::vector<std::byte> bytes)
{
    const auto headerResult = ReadHeader(bytes);
    if (!headerResult)
        return base::Failure<FrozenComposition, FrozenCompositionError>(headerResult.error());
    const auto header = headerResult.value();

    if (ComputeFrozenCompositionFileDigest(bytes) != header.fileDigest)
    {
        return base::Failure<FrozenComposition, FrozenCompositionError>(Error(
            FrozenCompositionErrorCode::DigestMismatch,
            "Compositionが検証または実行の契約に違反しています。"));
    }

    const auto descriptorResult = ReadDescriptors(bytes, header);
    if (!descriptorResult)
        return base::Failure<FrozenComposition, FrozenCompositionError>(descriptorResult.error());
    const auto descriptors = descriptorResult.value();

    std::vector<FrozenCompositionSectionView> temporaryViews;
    temporaryViews.reserve(descriptors.size());
    for (const auto& descriptor : descriptors)
    {
        const auto sectionBytes = std::span<const std::byte>(bytes).subspan(
            static_cast<std::size_t>(descriptor.fileOffset),
            static_cast<std::size_t>(descriptor.storedBytes));
        if (base::Sha256(sectionBytes) != descriptor.sectionDigest)
        {
            return base::Failure<FrozenComposition, FrozenCompositionError>(Error(
                FrozenCompositionErrorCode::DigestMismatch,
                "Sectionが検証または実行の契約に違反しています。",
                descriptor.kind));
        }
        temporaryViews.push_back({descriptor, sectionBytes});
    }

    const auto manifestResult = ReadManifest(temporaryViews[0]);
    if (!manifestResult)
        return base::Failure<FrozenComposition, FrozenCompositionError>(manifestResult.error());
    const auto manifest = manifestResult.value();

    if (temporaryViews[3].bytes.size() != manifest.contractBytes ||
        temporaryViews[4].bytes.size() != manifest.verifiedDecisionBytes ||
        temporaryViews[5].bytes.size() != manifest.certificateBytes ||
        temporaryViews[3].descriptor.elementCount != 0 || temporaryViews[3].descriptor.elementStride != 0 ||
        temporaryViews[4].descriptor.elementCount != 0 || temporaryViews[4].descriptor.elementStride != 0 ||
        temporaryViews[5].descriptor.elementCount != 0 || temporaryViews[5].descriptor.elementStride != 0)
    {
        return base::Failure<FrozenComposition, FrozenCompositionError>(Error(
            FrozenCompositionErrorCode::InvalidManifest,
            "Certificateが検証または実行の契約に違反しています。"));
    }

    const auto leafResult = ReadLeafRecords(temporaryViews[1], temporaryViews[2], manifest);
    if (!leafResult)
        return base::Failure<FrozenComposition, FrozenCompositionError>(leafResult.error());
    const auto records = leafResult.value();

    if (ComputeSemanticDigestFromViews(temporaryViews) != header.semanticDigest ||
        base::Sha256(temporaryViews[4].bytes) != header.decisionDigest ||
        base::Sha256(temporaryViews[5].bytes) != header.certificateDigest)
    {
        return base::Failure<FrozenComposition, FrozenCompositionError>(Error(
            FrozenCompositionErrorCode::DigestMismatch,
            "Certificateが検証または実行の契約に違反しています。"));
    }

    auto storage = std::make_shared<const std::vector<std::byte>>(std::move(bytes));
    FrozenComposition composition;
    composition.storage_ = storage;
    composition.header_ = header;
    composition.manifest_ = manifest;
    for (const auto& descriptor : descriptors)
    {
        const auto sectionBytes = std::span<const std::byte>(*storage).subspan(
            static_cast<std::size_t>(descriptor.fileOffset),
            static_cast<std::size_t>(descriptor.storedBytes));
        composition.sections_.push_back({descriptor, sectionBytes});
    }
    const auto& leafStorage = composition.sections_[2].bytes;
    for (const auto& record : records)
    {
        composition.leaves_.push_back({
            record,
            leafStorage.subspan(
                static_cast<std::size_t>(record.byteOffset),
                static_cast<std::size_t>(record.byteSize))});
    }
    composition.contractBytes_ = composition.sections_[3].bytes;
    composition.verifiedDecisionBytes_ = composition.sections_[4].bytes;
    composition.certificateBytes_ = composition.sections_[5].bytes;
    return base::Success<FrozenComposition, FrozenCompositionError>(std::move(composition));
}
}
