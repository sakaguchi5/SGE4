#include "FrozenCompositionReader.h"

#include "../00_Foundation/BinaryIO.h"
#include "../00_Foundation/CheckedMath.h"
#include "../00_Foundation/Sha256.h"
#include "../09_FrozenPackageCore/PackageReader.h"
#include "FrozenCompositionDigest.h"

#include <algorithm>
#include <cstring>
#include <memory>

namespace sge4_5::composition
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
bool Pull(base::Result<T, std::string>& result, T& out, FrozenCompositionError& error)
{
    if (!result)
    {
        error = Error(FrozenCompositionErrorCode::InvalidHeader, result.Error());
        return false;
    }
    out = result.Value();
    return true;
}

bool PullBytes(
    base::Result<std::span<const std::byte>, std::string>& result,
    std::span<const std::byte>& out,
    FrozenCompositionError& error)
{
    if (!result)
    {
        error = Error(FrozenCompositionErrorCode::InvalidHeader, result.Error());
        return false;
    }
    out = result.Value();
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

base::Result<FrozenCompositionHeader, FrozenCompositionError> ReadHeader(std::span<const std::byte> bytes)
{
    if (bytes.size() < FrozenCompositionHeaderBytes)
    {
        return base::Result<FrozenCompositionHeader, FrozenCompositionError>::Failure(Error(
            FrozenCompositionErrorCode::FileSizeMismatch,
            "file is smaller than the fixed Frozen Composition header"));
    }
    if (!std::equal(FrozenCompositionMagic.begin(), FrozenCompositionMagic.end(), bytes.begin()))
    {
        return base::Result<FrozenCompositionHeader, FrozenCompositionError>::Failure(Error(
            FrozenCompositionErrorCode::BadMagic,
            "frozen composition magic does not match SGE4CMP"));
    }

    base::BinaryReader reader(bytes.subspan(8, FrozenCompositionHeaderBytes - 8));
    FrozenCompositionHeader header;
    FrozenCompositionError readError;
    auto u16 = reader.ReadU16(); if (!Pull(u16, header.formatMajor, readError)) return base::Result<FrozenCompositionHeader, FrozenCompositionError>::Failure(readError);
    u16 = reader.ReadU16(); if (!Pull(u16, header.formatMinor, readError)) return base::Result<FrozenCompositionHeader, FrozenCompositionError>::Failure(readError);
    u16 = reader.ReadU16(); if (!Pull(u16, header.headerBytes, readError)) return base::Result<FrozenCompositionHeader, FrozenCompositionError>::Failure(readError);
    u16 = reader.ReadU16(); if (!Pull(u16, header.sectionDescriptorBytes, readError)) return base::Result<FrozenCompositionHeader, FrozenCompositionError>::Failure(readError);
    auto u32 = reader.ReadU32(); if (!Pull(u32, header.flags, readError)) return base::Result<FrozenCompositionHeader, FrozenCompositionError>::Failure(readError);
    u32 = reader.ReadU32(); if (!Pull(u32, header.endianTag, readError)) return base::Result<FrozenCompositionHeader, FrozenCompositionError>::Failure(readError);
    u32 = reader.ReadU32(); if (!Pull(u32, header.digestAlgorithm, readError)) return base::Result<FrozenCompositionHeader, FrozenCompositionError>::Failure(readError);
    std::uint32_t reserved0 = 1; u32 = reader.ReadU32(); if (!Pull(u32, reserved0, readError)) return base::Result<FrozenCompositionHeader, FrozenCompositionError>::Failure(readError);
    auto u64 = reader.ReadU64(); if (!Pull(u64, header.fileBytes, readError)) return base::Result<FrozenCompositionHeader, FrozenCompositionError>::Failure(readError);
    u64 = reader.ReadU64(); if (!Pull(u64, header.sectionTableOffset, readError)) return base::Result<FrozenCompositionHeader, FrozenCompositionError>::Failure(readError);
    u32 = reader.ReadU32(); if (!Pull(u32, header.sectionCount, readError)) return base::Result<FrozenCompositionHeader, FrozenCompositionError>::Failure(readError);
    std::uint32_t reserved1 = 1; u32 = reader.ReadU32(); if (!Pull(u32, reserved1, readError)) return base::Result<FrozenCompositionHeader, FrozenCompositionError>::Failure(readError);
    if (!ReadDigest(reader, header.semanticDigest, readError)) return base::Result<FrozenCompositionHeader, FrozenCompositionError>::Failure(readError);
    if (!ReadDigest(reader, header.decisionDigest, readError)) return base::Result<FrozenCompositionHeader, FrozenCompositionError>::Failure(readError);
    if (!ReadDigest(reader, header.certificateDigest, readError)) return base::Result<FrozenCompositionHeader, FrozenCompositionError>::Failure(readError);
    if (!ReadDigest(reader, header.fileDigest, readError)) return base::Result<FrozenCompositionHeader, FrozenCompositionError>::Failure(readError);
    auto reserved = reader.ReadBytes(8);
    std::span<const std::byte> reservedBytes;
    if (!PullBytes(reserved, reservedBytes, readError)) return base::Result<FrozenCompositionHeader, FrozenCompositionError>::Failure(readError);

    if (header.formatMajor != FrozenCompositionFormatMajor ||
        header.formatMinor > FrozenCompositionFormatMinor ||
        header.headerBytes != FrozenCompositionHeaderBytes ||
        header.sectionDescriptorBytes != FrozenCompositionSectionDescriptorBytes)
    {
        return base::Result<FrozenCompositionHeader, FrozenCompositionError>::Failure(Error(
            FrozenCompositionErrorCode::UnsupportedContainerVersion,
            "unsupported Frozen Composition container version or fixed record size"));
    }
    if (header.endianTag != FrozenCompositionEndianTag)
    {
        return base::Result<FrozenCompositionHeader, FrozenCompositionError>::Failure(Error(
            FrozenCompositionErrorCode::WrongEndianness,
            "frozen composition is not little endian"));
    }
    if (header.digestAlgorithm != FrozenCompositionDigestAlgorithmSha256 ||
        header.flags != 0 || reserved0 != 0 || reserved1 != 0 || !AllZero(reservedBytes))
    {
        return base::Result<FrozenCompositionHeader, FrozenCompositionError>::Failure(Error(
            FrozenCompositionErrorCode::InvalidHeader,
            "header contains unsupported flags, digest algorithm, or non-zero reserved bytes"));
    }
    if (header.fileBytes != bytes.size())
    {
        return base::Result<FrozenCompositionHeader, FrozenCompositionError>::Failure(Error(
            FrozenCompositionErrorCode::FileSizeMismatch,
            "header fileBytes does not match the actual file size"));
    }
    if (header.sectionTableOffset != FrozenCompositionHeaderBytes || header.sectionCount != 6)
    {
        return base::Result<FrozenCompositionHeader, FrozenCompositionError>::Failure(Error(
            FrozenCompositionErrorCode::InvalidSectionTable,
            "R1 requires exactly six canonical sections immediately after the header"));
    }
    return base::Result<FrozenCompositionHeader, FrozenCompositionError>::Success(header);
}

base::Result<std::vector<FrozenCompositionSectionDescriptor>, FrozenCompositionError> ReadDescriptors(
    std::span<const std::byte> bytes,
    const FrozenCompositionHeader& header)
{
    const std::uint64_t tableBytes =
        static_cast<std::uint64_t>(header.sectionCount) * FrozenCompositionSectionDescriptorBytes;
    std::uint64_t tableEnd = 0;
    if (!base::CheckedAdd(header.sectionTableOffset, tableBytes, tableEnd) || tableEnd > bytes.size())
    {
        return base::Result<std::vector<FrozenCompositionSectionDescriptor>, FrozenCompositionError>::Failure(Error(
            FrozenCompositionErrorCode::InvalidSectionTable,
            "section table is out of bounds"));
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
        auto u32 = reader.ReadU32(); if (!Pull(u32, kind, readError)) return base::Result<std::vector<FrozenCompositionSectionDescriptor>, FrozenCompositionError>::Failure(readError);
        descriptor.kind = static_cast<FrozenCompositionSectionKind>(kind);
        auto u16 = reader.ReadU16(); if (!Pull(u16, descriptor.schemaVersion, readError)) return base::Result<std::vector<FrozenCompositionSectionDescriptor>, FrozenCompositionError>::Failure(readError);
        u16 = reader.ReadU16(); if (!Pull(u16, descriptor.descriptorBytes, readError)) return base::Result<std::vector<FrozenCompositionSectionDescriptor>, FrozenCompositionError>::Failure(readError);
        u32 = reader.ReadU32(); if (!Pull(u32, descriptor.flags, readError)) return base::Result<std::vector<FrozenCompositionSectionDescriptor>, FrozenCompositionError>::Failure(readError);
        u32 = reader.ReadU32(); if (!Pull(u32, descriptor.alignment, readError)) return base::Result<std::vector<FrozenCompositionSectionDescriptor>, FrozenCompositionError>::Failure(readError);
        auto u64 = reader.ReadU64(); if (!Pull(u64, descriptor.fileOffset, readError)) return base::Result<std::vector<FrozenCompositionSectionDescriptor>, FrozenCompositionError>::Failure(readError);
        u64 = reader.ReadU64(); if (!Pull(u64, descriptor.storedBytes, readError)) return base::Result<std::vector<FrozenCompositionSectionDescriptor>, FrozenCompositionError>::Failure(readError);
        u64 = reader.ReadU64(); if (!Pull(u64, descriptor.logicalBytes, readError)) return base::Result<std::vector<FrozenCompositionSectionDescriptor>, FrozenCompositionError>::Failure(readError);
        u32 = reader.ReadU32(); if (!Pull(u32, descriptor.elementCount, readError)) return base::Result<std::vector<FrozenCompositionSectionDescriptor>, FrozenCompositionError>::Failure(readError);
        u32 = reader.ReadU32(); if (!Pull(u32, descriptor.elementStride, readError)) return base::Result<std::vector<FrozenCompositionSectionDescriptor>, FrozenCompositionError>::Failure(readError);
        if (!ReadDigest(reader, descriptor.sectionDigest, readError)) return base::Result<std::vector<FrozenCompositionSectionDescriptor>, FrozenCompositionError>::Failure(readError);

        const auto expectedKind = static_cast<FrozenCompositionSectionKind>(index + 1);
        if (descriptor.kind != expectedKind)
        {
            return base::Result<std::vector<FrozenCompositionSectionDescriptor>, FrozenCompositionError>::Failure(Error(
                FrozenCompositionErrorCode::InvalidSectionTable,
                "sections must be unique and in the fixed canonical order",
                descriptor.kind));
        }
        if (descriptor.schemaVersion != 1 ||
            descriptor.descriptorBytes != FrozenCompositionSectionDescriptorBytes ||
            descriptor.flags != RequiredExecutionFlags() ||
            descriptor.alignment != SectionAlignment ||
            descriptor.logicalBytes != descriptor.storedBytes)
        {
            return base::Result<std::vector<FrozenCompositionSectionDescriptor>, FrozenCompositionError>::Failure(Error(
                FrozenCompositionErrorCode::UnsupportedContainerVersion,
                "section descriptor differs from the R1 canonical contract",
                descriptor.kind));
        }
        if (descriptor.elementCount != 0 &&
            (descriptor.elementStride == 0 ||
             static_cast<std::uint64_t>(descriptor.elementCount) * descriptor.elementStride != descriptor.logicalBytes))
        {
            return base::Result<std::vector<FrozenCompositionSectionDescriptor>, FrozenCompositionError>::Failure(Error(
                FrozenCompositionErrorCode::InvalidRecordStride,
                "table record dimensions are inconsistent",
                descriptor.kind));
        }

        const auto alignedOffset = base::AlignUp(expectedOffset, SectionAlignment);
        if (descriptor.fileOffset != alignedOffset)
        {
            return base::Result<std::vector<FrozenCompositionSectionDescriptor>, FrozenCompositionError>::Failure(Error(
                FrozenCompositionErrorCode::NonCanonicalEncoding,
                "section offset is valid only in the unique canonical layout",
                descriptor.kind));
        }
        if (alignedOffset > expectedOffset && !AllZero(bytes.subspan(
                static_cast<std::size_t>(expectedOffset),
                static_cast<std::size_t>(alignedOffset - expectedOffset))))
        {
            return base::Result<std::vector<FrozenCompositionSectionDescriptor>, FrozenCompositionError>::Failure(Error(
                FrozenCompositionErrorCode::NonCanonicalEncoding,
                "section alignment padding must be zero",
                descriptor.kind));
        }
        expectedOffset = alignedOffset;
        std::uint64_t sectionEnd = 0;
        if (!base::CheckedAdd(descriptor.fileOffset, descriptor.storedBytes, sectionEnd) || sectionEnd > bytes.size())
        {
            return base::Result<std::vector<FrozenCompositionSectionDescriptor>, FrozenCompositionError>::Failure(Error(
                FrozenCompositionErrorCode::SectionOutOfBounds,
                "section lies outside the frozen composition",
                descriptor.kind));
        }
        expectedOffset = sectionEnd;
        descriptors.push_back(descriptor);
    }
    if (expectedOffset != bytes.size())
    {
        return base::Result<std::vector<FrozenCompositionSectionDescriptor>, FrozenCompositionError>::Failure(Error(
            FrozenCompositionErrorCode::NonCanonicalEncoding,
            "trailing bytes are not permitted after the final canonical section"));
    }
    return base::Result<std::vector<FrozenCompositionSectionDescriptor>, FrozenCompositionError>::Success(std::move(descriptors));
}

base::Result<FrozenCompositionManifest, FrozenCompositionError> ReadManifest(
    const FrozenCompositionSectionView& section)
{
    if (section.descriptor.elementCount != 1 ||
        section.descriptor.elementStride != FrozenCompositionManifestBytes ||
        section.bytes.size() != FrozenCompositionManifestBytes)
    {
        return base::Result<FrozenCompositionManifest, FrozenCompositionError>::Failure(Error(
            FrozenCompositionErrorCode::InvalidManifest,
            "manifest must be one fixed 64-byte record",
            FrozenCompositionSectionKind::Manifest));
    }

    base::BinaryReader reader(section.bytes);
    FrozenCompositionManifest manifest;
    FrozenCompositionError readError;
    auto u32 = reader.ReadU32(); if (!Pull(u32, manifest.leafCount, readError)) return base::Result<FrozenCompositionManifest, FrozenCompositionError>::Failure(readError);
    u32 = reader.ReadU32(); if (!Pull(u32, manifest.presenterLeafId, readError)) return base::Result<FrozenCompositionManifest, FrozenCompositionError>::Failure(readError);
    u32 = reader.ReadU32(); if (!Pull(u32, manifest.flags, readError)) return base::Result<FrozenCompositionManifest, FrozenCompositionError>::Failure(readError);
    std::uint32_t reserved0 = 1; u32 = reader.ReadU32(); if (!Pull(u32, reserved0, readError)) return base::Result<FrozenCompositionManifest, FrozenCompositionError>::Failure(readError);
    auto u64 = reader.ReadU64(); if (!Pull(u64, manifest.contractBytes, readError)) return base::Result<FrozenCompositionManifest, FrozenCompositionError>::Failure(readError);
    u64 = reader.ReadU64(); if (!Pull(u64, manifest.verifiedDecisionBytes, readError)) return base::Result<FrozenCompositionManifest, FrozenCompositionError>::Failure(readError);
    u64 = reader.ReadU64(); if (!Pull(u64, manifest.certificateBytes, readError)) return base::Result<FrozenCompositionManifest, FrozenCompositionError>::Failure(readError);
    std::uint64_t reserved1 = 1; u64 = reader.ReadU64(); if (!Pull(u64, reserved1, readError)) return base::Result<FrozenCompositionManifest, FrozenCompositionError>::Failure(readError);
    auto reserved = reader.ReadBytes(16);
    std::span<const std::byte> reservedBytes;
    if (!PullBytes(reserved, reservedBytes, readError)) return base::Result<FrozenCompositionManifest, FrozenCompositionError>::Failure(readError);

    if (manifest.leafCount < 2 || manifest.flags != 0 || reserved0 != 0 || reserved1 != 0 ||
        !AllZero(reservedBytes) || manifest.contractBytes == 0 ||
        manifest.verifiedDecisionBytes == 0 || manifest.certificateBytes == 0)
    {
        return base::Result<FrozenCompositionManifest, FrozenCompositionError>::Failure(Error(
            FrozenCompositionErrorCode::InvalidManifest,
            "manifest contains invalid counts, flags, lengths, or reserved bytes",
            FrozenCompositionSectionKind::Manifest));
    }
    if (manifest.presenterLeafId != FrozenCompositionInvalidIndex &&
        manifest.presenterLeafId >= manifest.leafCount)
    {
        return base::Result<FrozenCompositionManifest, FrozenCompositionError>::Failure(Error(
            FrozenCompositionErrorCode::InvalidManifest,
            "presenter Leaf id is outside the embedded Leaf table",
            FrozenCompositionSectionKind::Manifest));
    }
    return base::Result<FrozenCompositionManifest, FrozenCompositionError>::Success(manifest);
}

base::Result<std::vector<EmbeddedLeafRecord>, FrozenCompositionError> ReadLeafRecords(
    const FrozenCompositionSectionView& table,
    const FrozenCompositionSectionView& leafBytes,
    const FrozenCompositionManifest& manifest)
{
    if (table.descriptor.elementCount != manifest.leafCount ||
        table.descriptor.elementStride != EmbeddedLeafRecordBytes ||
        table.bytes.size() != static_cast<std::uint64_t>(manifest.leafCount) * EmbeddedLeafRecordBytes)
    {
        return base::Result<std::vector<EmbeddedLeafRecord>, FrozenCompositionError>::Failure(Error(
            FrozenCompositionErrorCode::InvalidLeafTable,
            "Leaf table dimensions do not match the manifest",
            FrozenCompositionSectionKind::LeafTable));
    }
    if (leafBytes.descriptor.elementCount != 0 || leafBytes.descriptor.elementStride != 0)
    {
        return base::Result<std::vector<EmbeddedLeafRecord>, FrozenCompositionError>::Failure(Error(
            FrozenCompositionErrorCode::InvalidLeafTable,
            "Leaf byte storage must be an opaque byte section",
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
        auto u32 = reader.ReadU32(); if (!Pull(u32, record.leafId, readError)) return base::Result<std::vector<EmbeddedLeafRecord>, FrozenCompositionError>::Failure(readError);
        u32 = reader.ReadU32(); if (!Pull(u32, record.flags, readError)) return base::Result<std::vector<EmbeddedLeafRecord>, FrozenCompositionError>::Failure(readError);
        if (!ReadDigest(reader, record.stableKey, readError)) return base::Result<std::vector<EmbeddedLeafRecord>, FrozenCompositionError>::Failure(readError);
        auto u64 = reader.ReadU64(); if (!Pull(u64, record.byteOffset, readError)) return base::Result<std::vector<EmbeddedLeafRecord>, FrozenCompositionError>::Failure(readError);
        u64 = reader.ReadU64(); if (!Pull(u64, record.byteSize, readError)) return base::Result<std::vector<EmbeddedLeafRecord>, FrozenCompositionError>::Failure(readError);
        if (!ReadDigest(reader, record.executionDigest, readError)) return base::Result<std::vector<EmbeddedLeafRecord>, FrozenCompositionError>::Failure(readError);
        if (!ReadDigest(reader, record.fileDigest, readError)) return base::Result<std::vector<EmbeddedLeafRecord>, FrozenCompositionError>::Failure(readError);
        std::uint64_t reserved = 1; u64 = reader.ReadU64(); if (!Pull(u64, reserved, readError)) return base::Result<std::vector<EmbeddedLeafRecord>, FrozenCompositionError>::Failure(readError);

        if (record.leafId != index || record.flags != 0 || reserved != 0 ||
            IsZeroDigest(record.stableKey) || record.byteSize == 0)
        {
            return base::Result<std::vector<EmbeddedLeafRecord>, FrozenCompositionError>::Failure(Error(
                FrozenCompositionErrorCode::InvalidLeafTable,
                "Leaf record contains a non-canonical id, key, flags, size, or reserved field",
                FrozenCompositionSectionKind::LeafTable,
                index));
        }
        if (!records.empty() && !DigestLess(records.back().stableKey, record.stableKey))
        {
            return base::Result<std::vector<EmbeddedLeafRecord>, FrozenCompositionError>::Failure(Error(
                records.back().stableKey == record.stableKey ?
                    FrozenCompositionErrorCode::DuplicateStableKey :
                    FrozenCompositionErrorCode::NonCanonicalEncoding,
                "Leaf stable keys must be strictly increasing",
                FrozenCompositionSectionKind::LeafTable,
                index));
        }

        const auto alignedLeafOffset = base::AlignUp(expectedLeafOffset, SectionAlignment);
        if (alignedLeafOffset > expectedLeafOffset && !AllZero(leafBytes.bytes.subspan(
                static_cast<std::size_t>(expectedLeafOffset),
                static_cast<std::size_t>(alignedLeafOffset - expectedLeafOffset))))
        {
            return base::Result<std::vector<EmbeddedLeafRecord>, FrozenCompositionError>::Failure(Error(
                FrozenCompositionErrorCode::NonCanonicalEncoding,
                "embedded Leaf alignment padding must be zero",
                FrozenCompositionSectionKind::LeafBytes,
                index));
        }
        expectedLeafOffset = alignedLeafOffset;
        if (record.byteOffset != expectedLeafOffset)
        {
            return base::Result<std::vector<EmbeddedLeafRecord>, FrozenCompositionError>::Failure(Error(
                FrozenCompositionErrorCode::NonCanonicalEncoding,
                "embedded Leaf bytes are not in the unique canonical layout",
                FrozenCompositionSectionKind::LeafBytes,
                index));
        }
        std::uint64_t leafEnd = 0;
        if (!base::CheckedAdd(record.byteOffset, record.byteSize, leafEnd) || leafEnd > leafBytes.bytes.size())
        {
            return base::Result<std::vector<EmbeddedLeafRecord>, FrozenCompositionError>::Failure(Error(
                FrozenCompositionErrorCode::SectionOutOfBounds,
                "embedded Leaf bytes are outside the Leaf byte section",
                FrozenCompositionSectionKind::LeafBytes,
                index));
        }
        const auto packageSpan = leafBytes.bytes.subspan(
            static_cast<std::size_t>(record.byteOffset),
            static_cast<std::size_t>(record.byteSize));
        const auto package = package::PackageReader::Read(packageSpan);
        if (!package)
        {
            return base::Result<std::vector<EmbeddedLeafRecord>, FrozenCompositionError>::Failure(Error(
                FrozenCompositionErrorCode::InvalidEmbeddedLeaf,
                "embedded Leaf package validation failed: " + package.Error().message,
                FrozenCompositionSectionKind::LeafBytes,
                index));
        }
        const auto& packageHeader = package.Value().Header();
        if (packageHeader.targetKind != package::TargetKindD3D12 ||
            packageHeader.targetSchemaVersion != EmbeddedSchemaVersion ||
            packageHeader.minimumRuntimeVersion != EmbeddedRuntimeVersion)
        {
            return base::Result<std::vector<EmbeddedLeafRecord>, FrozenCompositionError>::Failure(Error(
                FrozenCompositionErrorCode::UnsupportedEmbeddedLeaf,
                "embedded Leaf is not D3D12 Schema 17 / Runtime 17",
                FrozenCompositionSectionKind::LeafBytes,
                index));
        }
        if (packageHeader.executionDigest != record.executionDigest ||
            packageHeader.fileDigest != record.fileDigest)
        {
            return base::Result<std::vector<EmbeddedLeafRecord>, FrozenCompositionError>::Failure(Error(
                FrozenCompositionErrorCode::DigestMismatch,
                "Leaf record digest does not match the embedded Frozen Package",
                FrozenCompositionSectionKind::LeafTable,
                index));
        }
        expectedLeafOffset = leafEnd;
        records.push_back(record);
    }
    if (expectedLeafOffset != leafBytes.bytes.size())
    {
        return base::Result<std::vector<EmbeddedLeafRecord>, FrozenCompositionError>::Failure(Error(
            FrozenCompositionErrorCode::NonCanonicalEncoding,
            "trailing bytes are not permitted after the final embedded Leaf",
            FrozenCompositionSectionKind::LeafBytes));
    }
    return base::Result<std::vector<EmbeddedLeafRecord>, FrozenCompositionError>::Success(std::move(records));
}
}

base::Result<FrozenComposition, FrozenCompositionError> FrozenCompositionReader::Read(
    std::span<const std::byte> bytes)
{
    return Read(std::vector<std::byte>(bytes.begin(), bytes.end()));
}

base::Result<FrozenComposition, FrozenCompositionError> FrozenCompositionReader::Read(
    std::vector<std::byte> bytes)
{
    const auto headerResult = ReadHeader(bytes);
    if (!headerResult)
        return base::Result<FrozenComposition, FrozenCompositionError>::Failure(headerResult.Error());
    const auto header = headerResult.Value();

    if (ComputeFrozenCompositionFileDigest(bytes) != header.fileDigest)
    {
        return base::Result<FrozenComposition, FrozenCompositionError>::Failure(Error(
            FrozenCompositionErrorCode::DigestMismatch,
            "frozen composition file digest mismatch"));
    }

    const auto descriptorResult = ReadDescriptors(bytes, header);
    if (!descriptorResult)
        return base::Result<FrozenComposition, FrozenCompositionError>::Failure(descriptorResult.Error());
    const auto descriptors = descriptorResult.Value();

    std::vector<FrozenCompositionSectionView> temporaryViews;
    temporaryViews.reserve(descriptors.size());
    for (const auto& descriptor : descriptors)
    {
        const auto sectionBytes = std::span<const std::byte>(bytes).subspan(
            static_cast<std::size_t>(descriptor.fileOffset),
            static_cast<std::size_t>(descriptor.storedBytes));
        if (base::Sha256(sectionBytes) != descriptor.sectionDigest)
        {
            return base::Result<FrozenComposition, FrozenCompositionError>::Failure(Error(
                FrozenCompositionErrorCode::DigestMismatch,
                "section digest mismatch",
                descriptor.kind));
        }
        temporaryViews.push_back({descriptor, sectionBytes});
    }

    const auto manifestResult = ReadManifest(temporaryViews[0]);
    if (!manifestResult)
        return base::Result<FrozenComposition, FrozenCompositionError>::Failure(manifestResult.Error());
    const auto manifest = manifestResult.Value();

    if (temporaryViews[3].bytes.size() != manifest.contractBytes ||
        temporaryViews[4].bytes.size() != manifest.verifiedDecisionBytes ||
        temporaryViews[5].bytes.size() != manifest.certificateBytes ||
        temporaryViews[3].descriptor.elementCount != 0 || temporaryViews[3].descriptor.elementStride != 0 ||
        temporaryViews[4].descriptor.elementCount != 0 || temporaryViews[4].descriptor.elementStride != 0 ||
        temporaryViews[5].descriptor.elementCount != 0 || temporaryViews[5].descriptor.elementStride != 0)
    {
        return base::Result<FrozenComposition, FrozenCompositionError>::Failure(Error(
            FrozenCompositionErrorCode::InvalidManifest,
            "manifest byte lengths do not match the opaque contract, decision, and certificate sections"));
    }

    const auto leafResult = ReadLeafRecords(temporaryViews[1], temporaryViews[2], manifest);
    if (!leafResult)
        return base::Result<FrozenComposition, FrozenCompositionError>::Failure(leafResult.Error());
    const auto records = leafResult.Value();

    if (ComputeSemanticDigestFromViews(temporaryViews) != header.semanticDigest ||
        base::Sha256(temporaryViews[4].bytes) != header.decisionDigest ||
        base::Sha256(temporaryViews[5].bytes) != header.certificateDigest)
    {
        return base::Result<FrozenComposition, FrozenCompositionError>::Failure(Error(
            FrozenCompositionErrorCode::DigestMismatch,
            "semantic, decision, or certificate digest mismatch"));
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
    return base::Result<FrozenComposition, FrozenCompositionError>::Success(std::move(composition));
}
}
