#pragma once

#include "../00_Foundation/Sha256.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace sge4_5::composition
{
inline constexpr std::array<std::byte, 8> FrozenCompositionMagic = {
    std::byte{'S'}, std::byte{'G'}, std::byte{'E'}, std::byte{'4'},
    std::byte{'C'}, std::byte{'M'}, std::byte{'P'}, std::byte{0}
};
inline constexpr std::uint16_t FrozenCompositionFormatMajor = 1;
inline constexpr std::uint16_t FrozenCompositionFormatMinor = 0;
inline constexpr std::uint16_t FrozenCompositionHeaderBytes = 192;
inline constexpr std::uint16_t FrozenCompositionSectionDescriptorBytes = 80;
inline constexpr std::uint32_t FrozenCompositionEndianTag = 0x01020304u;
inline constexpr std::uint32_t FrozenCompositionDigestAlgorithmSha256 = 1;
inline constexpr std::uint32_t FrozenCompositionInvalidIndex = 0xffff'ffffu;
inline constexpr std::uint32_t EmbeddedLeafRecordBytes = 128;
inline constexpr std::uint32_t FrozenCompositionManifestBytes = 64;
inline constexpr std::uint32_t EmbeddedSchemaVersion = 17;
inline constexpr std::uint32_t EmbeddedRuntimeVersion = 17;
inline constexpr std::uint32_t SectionAlignment = 8;
inline constexpr std::size_t FrozenCompositionFileDigestOffset = 152;

enum class FrozenCompositionSectionKind : std::uint32_t
{
    Manifest = 1,
    LeafTable = 2,
    LeafBytes = 3,
    ContractData = 4,
    VerifiedDecisionData = 5,
    VerificationCertificate = 6
};

enum class FrozenCompositionSectionFlags : std::uint32_t
{
    Required = 1u << 0,
    ExecutionAffecting = 1u << 1
};

[[nodiscard]] constexpr std::uint32_t RequiredExecutionFlags() noexcept
{
    return static_cast<std::uint32_t>(FrozenCompositionSectionFlags::Required) |
        static_cast<std::uint32_t>(FrozenCompositionSectionFlags::ExecutionAffecting);
}

struct FrozenCompositionHeader final
{
    std::uint16_t formatMajor = FrozenCompositionFormatMajor;
    std::uint16_t formatMinor = FrozenCompositionFormatMinor;
    std::uint16_t headerBytes = FrozenCompositionHeaderBytes;
    std::uint16_t sectionDescriptorBytes = FrozenCompositionSectionDescriptorBytes;
    std::uint32_t flags = 0;
    std::uint32_t endianTag = FrozenCompositionEndianTag;
    std::uint32_t digestAlgorithm = FrozenCompositionDigestAlgorithmSha256;
    std::uint64_t fileBytes = 0;
    std::uint64_t sectionTableOffset = FrozenCompositionHeaderBytes;
    std::uint32_t sectionCount = 0;
    base::Digest256 semanticDigest{};
    base::Digest256 decisionDigest{};
    base::Digest256 certificateDigest{};
    base::Digest256 fileDigest{};
};

struct FrozenCompositionSectionDescriptor final
{
    FrozenCompositionSectionKind kind{};
    std::uint16_t schemaVersion = 1;
    std::uint16_t descriptorBytes = FrozenCompositionSectionDescriptorBytes;
    std::uint32_t flags = RequiredExecutionFlags();
    std::uint32_t alignment = SectionAlignment;
    std::uint64_t fileOffset = 0;
    std::uint64_t storedBytes = 0;
    std::uint64_t logicalBytes = 0;
    std::uint32_t elementCount = 0;
    std::uint32_t elementStride = 0;
    base::Digest256 sectionDigest{};
};

struct FrozenCompositionManifest final
{
    std::uint32_t leafCount = 0;
    std::uint32_t presenterLeafId = FrozenCompositionInvalidIndex;
    std::uint32_t flags = 0;
    std::uint64_t contractBytes = 0;
    std::uint64_t verifiedDecisionBytes = 0;
    std::uint64_t certificateBytes = 0;
};

struct EmbeddedLeafRecord final
{
    std::uint32_t leafId = FrozenCompositionInvalidIndex;
    std::uint32_t flags = 0;
    base::Digest256 stableKey{};
    std::uint64_t byteOffset = 0;
    std::uint64_t byteSize = 0;
    base::Digest256 executionDigest{};
    base::Digest256 fileDigest{};
};

enum class FrozenCompositionErrorCode
{
    InvalidBuildInput,
    InvalidStableKey,
    DuplicateStableKey,
    PresenterNotFound,
    InvalidEmbeddedLeaf,
    UnsupportedEmbeddedLeaf,
    SerializationFailure,
    BadMagic,
    UnsupportedContainerVersion,
    WrongEndianness,
    FileSizeMismatch,
    InvalidHeader,
    InvalidSectionTable,
    DuplicateSection,
    MissingRequiredSection,
    InvalidAlignment,
    SectionOutOfBounds,
    SectionOverlap,
    InvalidRecordStride,
    InvalidManifest,
    InvalidLeafTable,
    NonCanonicalEncoding,
    DigestMismatch
};

struct FrozenCompositionError final
{
    FrozenCompositionErrorCode code = FrozenCompositionErrorCode::InvalidBuildInput;
    FrozenCompositionSectionKind section{};
    std::uint32_t recordIndex = FrozenCompositionInvalidIndex;
    std::string message;
};

struct EmbeddedLeafInput final
{
    base::Digest256 stableKey{};
    std::vector<std::byte> packageBytes;
};

struct FrozenCompositionBuildInput final
{
    std::vector<EmbeddedLeafInput> leaves;
    bool hasPresenter = false;
    base::Digest256 presenterStableKey{};
    std::vector<std::byte> contractBytes;
    std::vector<std::byte> verifiedDecisionBytes;
    std::vector<std::byte> verificationCertificateBytes;
};
}
