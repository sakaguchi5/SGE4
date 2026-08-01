#pragma once

#include "../../../canonical/artifact/SectionedArtifact.h"
#include "../../../canonical/base/Expected.h"
#include "../../../canonical/base/SchemaValidation.h"
#include "../../../canonical/base/Sha256.h"
#include "../../model/CompositionContract.h"
#include "../../verifier/CompositionVerifier.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace sge4::composition::artifact
{
class VerifiedFrozenComposition;

inline constexpr std::array<std::byte, 8> FrozenCompositionAbi2Magic = {
    std::byte{'S'}, std::byte{'G'}, std::byte{'E'}, std::byte{'4'},
    std::byte{'U'}, std::byte{'N'}, std::byte{'I'}, std::byte{0}};
inline constexpr std::uint16_t FrozenCompositionAbi2FormatMajor = 2;
inline constexpr std::uint16_t FrozenCompositionAbi2FormatMinor = 6;
inline constexpr std::uint32_t FrozenCompositionAbi2ManifestSchema = 2;
inline constexpr std::uint32_t FrozenCompositionAbi2CoreSchema = 1;
inline constexpr std::uint32_t FrozenCompositionAbi2LeafRecordBytes = 128;
inline constexpr std::uint32_t FrozenCompositionAbi2Alignment = 8;
inline constexpr std::uint32_t FrozenCompositionAbi2EmbeddedSchemaVersion = 17;
inline constexpr std::uint32_t FrozenCompositionAbi2EmbeddedRuntimeVersion = 17;
inline constexpr std::uint16_t FrozenCompositionAbi2ContractSchema = 2;
inline constexpr std::uint16_t FrozenCompositionAbi2DecisionSchema = 2;
inline constexpr std::uint16_t FrozenCompositionAbi2AuthorityLedgerSchema = 2;
inline constexpr std::uint16_t FrozenCompositionAbi2DynamicContractSchema = 5;

// SGE4CMPを内包せず、Compositionの実行事実を直接Sectionとして所有する。
enum class FrozenCompositionAbi2SectionKind : std::uint32_t
{
    Manifest = 1,
    LeafTable = 2,
    LeafBytes = 3,
    ContractData = 4,
    VerifiedDecisionData = 5,
    VerificationCertificate = 6,
    AuthorityLedger = 7,
    DynamicContract = 8
};

inline constexpr std::array FrozenCompositionAbi2SectionKinds = {
    FrozenCompositionAbi2SectionKind::Manifest,
    FrozenCompositionAbi2SectionKind::LeafTable,
    FrozenCompositionAbi2SectionKind::LeafBytes,
    FrozenCompositionAbi2SectionKind::ContractData,
    FrozenCompositionAbi2SectionKind::VerifiedDecisionData,
    FrozenCompositionAbi2SectionKind::VerificationCertificate,
    FrozenCompositionAbi2SectionKind::AuthorityLedger,
    FrozenCompositionAbi2SectionKind::DynamicContract};
static_assert(base::ValuesAreUnique(FrozenCompositionAbi2SectionKinds,
    [](FrozenCompositionAbi2SectionKind value) { return std::to_underlying(value); }));
static_assert(base::ValuesAreStrictlyIncreasing(FrozenCompositionAbi2SectionKinds,
    [](FrozenCompositionAbi2SectionKind value) { return std::to_underlying(value); }));

inline constexpr std::array<std::uint16_t, FrozenCompositionAbi2SectionKinds.size()>
    FrozenCompositionAbi2SectionSchemas = {
        static_cast<std::uint16_t>(FrozenCompositionAbi2ManifestSchema),
        1, 1, FrozenCompositionAbi2ContractSchema,
        FrozenCompositionAbi2DecisionSchema, 1,
        FrozenCompositionAbi2AuthorityLedgerSchema,
        FrozenCompositionAbi2DynamicContractSchema};

struct FrozenCompositionAbi2Manifest final
{
    std::uint32_t schemaVersion = FrozenCompositionAbi2ManifestSchema;
    std::uint32_t dynamicUniverseCount = 0;
    std::uint32_t leafCount = 0;
    std::uint32_t flowCount = 0;
    std::uint32_t presenterLeafId = InvalidIndex;
    std::uint32_t flags = 0;
    std::uint64_t leafBytes = 0;
    std::uint64_t contractBytes = 0;
    std::uint64_t verifiedDecisionBytes = 0;
    std::uint64_t verificationCertificateBytes = 0;
    base::Digest256 compositionCoreDigest{};
    base::Digest256 compositionArtifactIdentity{};
    base::Digest256 dynamicSemanticIdentity{};
};

struct FrozenCompositionAbi2LeafRecord final
{
    std::uint32_t leafId = InvalidIndex;
    std::uint32_t flags = 0;
    base::Digest256 stableKey{};
    std::uint64_t byteOffset = 0;
    std::uint64_t byteSize = 0;
    base::Digest256 executionDigest{};
    base::Digest256 fileDigest{};
};

struct FrozenCompositionAbi2Core final
{
    std::uint32_t leafCount = 0;
    std::uint32_t flowCount = 0;
    std::uint32_t presenterLeafId = InvalidIndex;
    std::uint64_t leafBytes = 0;
    std::uint64_t contractBytes = 0;
    std::uint64_t verifiedDecisionBytes = 0;
    std::uint64_t verificationCertificateBytes = 0;
    base::Digest256 coreDigest{};
    std::vector<SectionInput> sections;
};

[[nodiscard]] base::Expected<FrozenCompositionAbi2Core, verification::VerificationError>
BuildFrozenCompositionAbi2Core(
    const ValidatedCompositionContract& contract,
    const verification::VerifiedCompositionPlan& verified);

[[nodiscard]] std::vector<std::byte> SerializeFrozenCompositionAbi2Manifest(
    const FrozenCompositionAbi2Manifest& manifest);

[[nodiscard]] base::Expected<FrozenCompositionAbi2Manifest, verification::VerificationError>
DeserializeFrozenCompositionAbi2Manifest(std::span<const std::byte> bytes);

[[nodiscard]] base::Digest256 ComputeFrozenCompositionAbi2CoreDigest(
    std::uint32_t leafCount,
    std::uint32_t flowCount,
    std::uint32_t presenterLeafId,
    std::span<const SectionInput> coreSections);

[[nodiscard]] base::Expected<VerifiedFrozenComposition, verification::VerificationError>
ReadVerifiedFrozenCompositionAbi2(std::vector<std::byte> bytes);
}
