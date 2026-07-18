#include "FrozenCompositionWriter.h"

#include "../00_Foundation/BinaryIO.h"
#include "../00_Foundation/CheckedMath.h"
#include "../00_Foundation/Sha256.h"
#include "../09_FrozenPackageCore/PackageReader.h"
#include "FrozenCompositionDigest.h"

#include <algorithm>
#include <exception>
#include <limits>

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

[[nodiscard]] bool IsZeroDigest(const base::Digest256& digest) noexcept
{
    return std::all_of(digest.begin(), digest.end(), [](std::byte value) { return value == std::byte{0}; });
}

[[nodiscard]] bool DigestLess(const base::Digest256& left, const base::Digest256& right) noexcept
{
    return std::lexicographical_compare(left.begin(), left.end(), right.begin(), right.end());
}

void WriteManifest(base::BinaryWriter& writer, const FrozenCompositionManifest& manifest)
{
    writer.WriteU32(manifest.leafCount);
    writer.WriteU32(manifest.presenterLeafId);
    writer.WriteU32(manifest.flags);
    writer.WriteU32(0);
    writer.WriteU64(manifest.contractBytes);
    writer.WriteU64(manifest.verifiedDecisionBytes);
    writer.WriteU64(manifest.certificateBytes);
    writer.WriteU64(0);
    writer.WriteZeroes(16);
}

void WriteLeafRecord(base::BinaryWriter& writer, const EmbeddedLeafRecord& record)
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

void WriteHeader(base::BinaryWriter& writer, const FrozenCompositionHeader& header)
{
    writer.WriteBytes(FrozenCompositionMagic);
    writer.WriteU16(header.formatMajor);
    writer.WriteU16(header.formatMinor);
    writer.WriteU16(header.headerBytes);
    writer.WriteU16(header.sectionDescriptorBytes);
    writer.WriteU32(header.flags);
    writer.WriteU32(header.endianTag);
    writer.WriteU32(header.digestAlgorithm);
    writer.WriteU32(0);
    writer.WriteU64(header.fileBytes);
    writer.WriteU64(header.sectionTableOffset);
    writer.WriteU32(header.sectionCount);
    writer.WriteU32(0);
    writer.WriteBytes(header.semanticDigest);
    writer.WriteBytes(header.decisionDigest);
    writer.WriteBytes(header.certificateDigest);
    writer.WriteBytes(header.fileDigest);
    writer.WriteZeroes(8);
}

void WriteDescriptor(base::BinaryWriter& writer, const FrozenCompositionSectionDescriptor& descriptor)
{
    writer.WriteU32(static_cast<std::uint32_t>(descriptor.kind));
    writer.WriteU16(descriptor.schemaVersion);
    writer.WriteU16(descriptor.descriptorBytes);
    writer.WriteU32(descriptor.flags);
    writer.WriteU32(descriptor.alignment);
    writer.WriteU64(descriptor.fileOffset);
    writer.WriteU64(descriptor.storedBytes);
    writer.WriteU64(descriptor.logicalBytes);
    writer.WriteU32(descriptor.elementCount);
    writer.WriteU32(descriptor.elementStride);
    writer.WriteBytes(descriptor.sectionDigest);
}
}

base::Result<std::vector<std::byte>, FrozenCompositionError> FrozenCompositionWriter::Write(
    FrozenCompositionBuildInput input)
{
    try
    {
        if (input.leaves.size() < 2 || input.leaves.size() > std::numeric_limits<std::uint32_t>::max())
        {
            return base::Result<std::vector<std::byte>, FrozenCompositionError>::Failure(Error(
                FrozenCompositionErrorCode::InvalidBuildInput,
                "a canonical Level 4 v1 composition requires at least two embedded Leaves"));
        }
        if (input.contractBytes.empty() || input.verifiedDecisionBytes.empty() ||
            input.verificationCertificateBytes.empty())
        {
            return base::Result<std::vector<std::byte>, FrozenCompositionError>::Failure(Error(
                FrozenCompositionErrorCode::InvalidBuildInput,
                "contract, verified decisions, and verification certificate must all be non-empty"));
        }

        struct ValidatedLeaf final
        {
            EmbeddedLeafInput input;
            base::Digest256 executionDigest{};
            base::Digest256 fileDigest{};
        };

        std::vector<ValidatedLeaf> leaves;
        leaves.reserve(input.leaves.size());
        for (std::size_t index = 0; index < input.leaves.size(); ++index)
        {
            auto& leaf = input.leaves[index];
            if (IsZeroDigest(leaf.stableKey))
            {
                return base::Result<std::vector<std::byte>, FrozenCompositionError>::Failure(Error(
                    FrozenCompositionErrorCode::InvalidStableKey,
                    "embedded Leaf stable key must not be zero",
                    FrozenCompositionSectionKind::LeafTable,
                    static_cast<std::uint32_t>(index)));
            }
            const auto package = package::PackageReader::Read(std::span<const std::byte>(leaf.packageBytes));
            if (!package)
            {
                return base::Result<std::vector<std::byte>, FrozenCompositionError>::Failure(Error(
                    FrozenCompositionErrorCode::InvalidEmbeddedLeaf,
                    "embedded Leaf is not a valid Frozen Executable Package: " + package.Error().message,
                    FrozenCompositionSectionKind::LeafBytes,
                    static_cast<std::uint32_t>(index)));
            }
            const auto& header = package.Value().Header();
            if (header.targetKind != package::TargetKindD3D12 ||
                header.targetSchemaVersion != EmbeddedSchemaVersion ||
                header.minimumRuntimeVersion != EmbeddedRuntimeVersion)
            {
                return base::Result<std::vector<std::byte>, FrozenCompositionError>::Failure(Error(
                    FrozenCompositionErrorCode::UnsupportedEmbeddedLeaf,
                    "embedded Leaf must be D3D12 Schema 17 / Runtime 17",
                    FrozenCompositionSectionKind::LeafBytes,
                    static_cast<std::uint32_t>(index)));
            }
            leaves.push_back(ValidatedLeaf{
                std::move(leaf), header.executionDigest, header.fileDigest});
        }

        std::sort(leaves.begin(), leaves.end(), [](const auto& left, const auto& right) {
            return DigestLess(left.input.stableKey, right.input.stableKey);
        });
        for (std::size_t index = 1; index < leaves.size(); ++index)
        {
            if (leaves[index - 1].input.stableKey == leaves[index].input.stableKey)
            {
                return base::Result<std::vector<std::byte>, FrozenCompositionError>::Failure(Error(
                    FrozenCompositionErrorCode::DuplicateStableKey,
                    "embedded Leaf stable keys must be unique",
                    FrozenCompositionSectionKind::LeafTable,
                    static_cast<std::uint32_t>(index)));
            }
        }

        std::uint32_t presenterLeafId = FrozenCompositionInvalidIndex;
        if (input.hasPresenter)
        {
            if (IsZeroDigest(input.presenterStableKey))
            {
                return base::Result<std::vector<std::byte>, FrozenCompositionError>::Failure(Error(
                    FrozenCompositionErrorCode::PresenterNotFound,
                    "presenter stable key must be explicit"));
            }
            const auto it = std::find_if(leaves.begin(), leaves.end(), [&](const auto& leaf) {
                return leaf.input.stableKey == input.presenterStableKey;
            });
            if (it == leaves.end())
            {
                return base::Result<std::vector<std::byte>, FrozenCompositionError>::Failure(Error(
                    FrozenCompositionErrorCode::PresenterNotFound,
                    "presenter stable key does not identify an embedded Leaf"));
            }
            presenterLeafId = static_cast<std::uint32_t>(std::distance(leaves.begin(), it));
        }

        base::BinaryWriter leafBytesWriter;
        std::vector<EmbeddedLeafRecord> records;
        records.reserve(leaves.size());
        for (std::size_t index = 0; index < leaves.size(); ++index)
        {
            leafBytesWriter.Align(SectionAlignment);
            EmbeddedLeafRecord record;
            record.leafId = static_cast<std::uint32_t>(index);
            record.stableKey = leaves[index].input.stableKey;
            record.byteOffset = leafBytesWriter.Size();
            record.byteSize = leaves[index].input.packageBytes.size();
            record.executionDigest = leaves[index].executionDigest;
            record.fileDigest = leaves[index].fileDigest;
            records.push_back(record);
            leafBytesWriter.WriteBytes(leaves[index].input.packageBytes);
        }

        base::BinaryWriter leafTableWriter;
        for (const auto& record : records) WriteLeafRecord(leafTableWriter, record);

        FrozenCompositionManifest manifest;
        manifest.leafCount = static_cast<std::uint32_t>(records.size());
        manifest.presenterLeafId = presenterLeafId;
        manifest.contractBytes = input.contractBytes.size();
        manifest.verifiedDecisionBytes = input.verifiedDecisionBytes.size();
        manifest.certificateBytes = input.verificationCertificateBytes.size();
        base::BinaryWriter manifestWriter;
        WriteManifest(manifestWriter, manifest);

        std::vector<FrozenCompositionSectionInput> sections;
        sections.push_back(FrozenCompositionSectionInput{
            FrozenCompositionSectionKind::Manifest, 1, RequiredExecutionFlags(), SectionAlignment,
            1, FrozenCompositionManifestBytes, std::move(manifestWriter).Take()});
        sections.push_back(FrozenCompositionSectionInput{
            FrozenCompositionSectionKind::LeafTable, 1, RequiredExecutionFlags(), SectionAlignment,
            static_cast<std::uint32_t>(records.size()), EmbeddedLeafRecordBytes,
            std::move(leafTableWriter).Take()});
        sections.push_back(FrozenCompositionSectionInput{
            FrozenCompositionSectionKind::LeafBytes, 1, RequiredExecutionFlags(), SectionAlignment,
            0, 0, std::move(leafBytesWriter).Take()});
        sections.push_back(FrozenCompositionSectionInput{
            FrozenCompositionSectionKind::ContractData, 1, RequiredExecutionFlags(), SectionAlignment,
            0, 0, std::move(input.contractBytes)});
        sections.push_back(FrozenCompositionSectionInput{
            FrozenCompositionSectionKind::VerifiedDecisionData, 1, RequiredExecutionFlags(), SectionAlignment,
            0, 0, std::move(input.verifiedDecisionBytes)});
        sections.push_back(FrozenCompositionSectionInput{
            FrozenCompositionSectionKind::VerificationCertificate, 1, RequiredExecutionFlags(), SectionAlignment,
            0, 0, std::move(input.verificationCertificateBytes)});

        FrozenCompositionHeader header;
        header.sectionCount = static_cast<std::uint32_t>(sections.size());
        header.semanticDigest = ComputeSemanticDigest(sections);
        header.decisionDigest = base::Sha256(sections[4].bytes);
        header.certificateDigest = base::Sha256(sections[5].bytes);

        std::vector<FrozenCompositionSectionDescriptor> descriptors;
        descriptors.reserve(sections.size());
        std::uint64_t cursor = FrozenCompositionHeaderBytes +
            static_cast<std::uint64_t>(sections.size()) * FrozenCompositionSectionDescriptorBytes;
        for (const auto& section : sections)
        {
            cursor = base::AlignUp(cursor, section.alignment);
            FrozenCompositionSectionDescriptor descriptor;
            descriptor.kind = section.kind;
            descriptor.schemaVersion = section.schemaVersion;
            descriptor.flags = section.flags;
            descriptor.alignment = section.alignment;
            descriptor.fileOffset = cursor;
            descriptor.storedBytes = section.bytes.size();
            descriptor.logicalBytes = section.bytes.size();
            descriptor.elementCount = section.elementCount;
            descriptor.elementStride = section.elementStride;
            descriptor.sectionDigest = base::Sha256(section.bytes);
            descriptors.push_back(descriptor);
            std::uint64_t next = 0;
            if (!base::CheckedAdd(cursor, descriptor.storedBytes, next))
            {
                return base::Result<std::vector<std::byte>, FrozenCompositionError>::Failure(Error(
                    FrozenCompositionErrorCode::SerializationFailure,
                    "frozen composition size overflow",
                    section.kind));
            }
            cursor = next;
        }
        header.fileBytes = cursor;

        base::BinaryWriter writer;
        WriteHeader(writer, header);
        for (const auto& descriptor : descriptors) WriteDescriptor(writer, descriptor);
        for (std::size_t index = 0; index < sections.size(); ++index)
        {
            writer.Align(sections[index].alignment);
            if (writer.Size() != descriptors[index].fileOffset)
            {
                return base::Result<std::vector<std::byte>, FrozenCompositionError>::Failure(Error(
                    FrozenCompositionErrorCode::SerializationFailure,
                    "internal section layout mismatch",
                    sections[index].kind));
            }
            writer.WriteBytes(sections[index].bytes);
        }
        if (writer.Size() != header.fileBytes)
        {
            return base::Result<std::vector<std::byte>, FrozenCompositionError>::Failure(Error(
                FrozenCompositionErrorCode::SerializationFailure,
                "serialized file size does not match the header"));
        }

        auto bytes = std::move(writer).Take();
        header.fileDigest = ComputeFrozenCompositionFileDigest(bytes);
        std::copy(
            header.fileDigest.begin(), header.fileDigest.end(),
            bytes.begin() + static_cast<std::ptrdiff_t>(FrozenCompositionFileDigestOffset));
        return base::Result<std::vector<std::byte>, FrozenCompositionError>::Success(std::move(bytes));
    }
    catch (const std::exception& exception)
    {
        return base::Result<std::vector<std::byte>, FrozenCompositionError>::Failure(Error(
            FrozenCompositionErrorCode::SerializationFailure,
            exception.what()));
    }
}
}
