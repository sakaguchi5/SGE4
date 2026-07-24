#include "MigrationCorpus.h"

#include <algorithm>
#include <array>
#include <limits>
#include <set>
#include <stdexcept>
#include <string_view>

namespace sge4_5::v2::migration
{
namespace
{
void AppendU8(std::vector<std::byte>& bytes, std::uint8_t value)
{
    bytes.push_back(static_cast<std::byte>(value));
}

void AppendU32(std::vector<std::byte>& bytes, std::uint32_t value)
{
    for (std::uint32_t index = 0; index < sizeof(value); ++index)
        bytes.push_back(static_cast<std::byte>((value >> (index * 8u)) & 0xffu));
}

void AppendU64(std::vector<std::byte>& bytes, std::uint64_t value)
{
    for (std::uint32_t index = 0; index < sizeof(value); ++index)
        bytes.push_back(static_cast<std::byte>((value >> (index * 8u)) & 0xffu));
}

void AppendDigest(std::vector<std::byte>& bytes, const canonical::Digest256V1& digest)
{
    bytes.insert(bytes.end(), digest.begin(), digest.end());
}

void AppendString(std::vector<std::byte>& bytes, std::string_view value)
{
    AppendU64(bytes, static_cast<std::uint64_t>(value.size()));
    for (const auto character : value)
        bytes.push_back(static_cast<std::byte>(static_cast<unsigned char>(character)));
}

std::uint8_t HexNibble(char value)
{
    if (value >= '0' && value <= '9') return static_cast<std::uint8_t>(value - '0');
    if (value >= 'a' && value <= 'f') return static_cast<std::uint8_t>(10 + value - 'a');
    if (value >= 'A' && value <= 'F') return static_cast<std::uint8_t>(10 + value - 'A');
    throw std::invalid_argument("Invalid SHA-256 hexadecimal character.");
}

template<class T>
bool HasDuplicateBy(const std::vector<T>& values, auto selector)
{
    std::set<std::string> seen;
    for (const auto& value : values)
    {
        const std::string key(selector(value));
        if (!seen.insert(key).second) return true;
    }
    return false;
}

const EvidenceRecordV1* FindEvidence(const MigrationCorpusInputV1& input, const std::string& id)
{
    const auto found = std::find_if(input.evidence.begin(), input.evidence.end(), [&](const auto& value) {
        return value.id == id;
    });
    return found == input.evidence.end() ? nullptr : &*found;
}

bool ContainsSource(const MigrationCorpusInputV1& input, const std::string& role)
{
    return std::any_of(input.sources.begin(), input.sources.end(), [&](const auto& source) {
        return source.role == role;
    });
}

bool ContainsInvariant(const MigrationCorpusInputV1& input, std::uint32_t ordinal)
{
    return std::any_of(input.invariants.begin(), input.invariants.end(), [&](const auto& invariant) {
        return invariant.ordinal == ordinal;
    });
}

MigrationCertificateIdentity ComputeCertificateIdentity(const MigrationCorpusInputV1& input)
{
    auto sources = input.sources;
    auto evidence = input.evidence;
    auto invariants = input.invariants;
    auto cases = input.cases;
    std::sort(sources.begin(), sources.end(), [](const auto& left, const auto& right) { return left.role < right.role; });
    std::sort(evidence.begin(), evidence.end(), [](const auto& left, const auto& right) { return left.id < right.id; });
    std::sort(invariants.begin(), invariants.end(), [](const auto& left, const auto& right) { return left.ordinal < right.ordinal; });
    std::sort(cases.begin(), cases.end(), [](const auto& left, const auto& right) { return left.id < right.id; });

    std::vector<std::byte> payload;
    AppendU32(payload, RequiredSourceLineageCountV1);
    AppendU32(payload, RequiredInvariantCountV1);
    AppendU64(payload, static_cast<std::uint64_t>(sources.size()));
    for (const auto& source : sources)
    {
        AppendString(payload, source.role);
        AppendDigest(payload, source.sourceSha256);
    }
    AppendU64(payload, static_cast<std::uint64_t>(evidence.size()));
    for (const auto& item : evidence)
    {
        AppendString(payload, item.id);
        AppendU8(payload, static_cast<std::uint8_t>(item.lane));
        AppendDigest(payload, item.evidenceSha256);
    }
    AppendU64(payload, static_cast<std::uint64_t>(invariants.size()));
    for (auto invariant : invariants)
    {
        std::sort(invariant.evidenceIds.begin(), invariant.evidenceIds.end());
        AppendU32(payload, invariant.ordinal);
        AppendU8(payload, static_cast<std::uint8_t>(invariant.outcome));
        AppendU64(payload, static_cast<std::uint64_t>(invariant.evidenceIds.size()));
        for (const auto& id : invariant.evidenceIds) AppendString(payload, id);
    }
    AppendU64(payload, static_cast<std::uint64_t>(cases.size()));
    for (auto item : cases)
    {
        std::sort(item.sourceRoles.begin(), item.sourceRoles.end());
        std::sort(item.invariantOrdinals.begin(), item.invariantOrdinals.end());
        std::sort(item.evidenceIds.begin(), item.evidenceIds.end());
        AppendString(payload, item.id);
        AppendU8(payload, static_cast<std::uint8_t>(item.lane));
        AppendU64(payload, static_cast<std::uint64_t>(item.sourceRoles.size()));
        for (const auto& role : item.sourceRoles) AppendString(payload, role);
        AppendU64(payload, static_cast<std::uint64_t>(item.invariantOrdinals.size()));
        for (const auto ordinal : item.invariantOrdinals) AppendU32(payload, ordinal);
        AppendU8(payload, static_cast<std::uint8_t>(item.bindingKind));
        AppendDigest(payload, item.boundIdentity);
        AppendU64(payload, static_cast<std::uint64_t>(item.evidenceIds.size()));
        for (const auto& id : item.evidenceIds) AppendString(payload, id);
    }
    AppendU8(payload, input.runtimeCandidatePolicyAuthorized ? 1u : 0u);
    AppendU8(payload, input.referenceProjectsDeleted ? 1u : 0u);
    return canonical::MakeCanonicalIdentityV1<MigrationCertificateIdentity>(
        {"SGE.V2.Migration.Certificate.V1", MigrationCorpusSchemaVersionV1, payload});
}
}

canonical::Digest256V1 ParseSha256HexV1(std::string_view hex)
{
    if (hex.size() != 64u) throw std::invalid_argument("SHA-256 hexadecimal text must contain 64 characters.");
    canonical::Digest256V1 digest{};
    for (std::size_t index = 0; index < digest.size(); ++index)
    {
        const auto high = HexNibble(hex[index * 2u]);
        const auto low = HexNibble(hex[index * 2u + 1u]);
        digest[index] = static_cast<std::byte>((high << 4u) | low);
    }
    return digest;
}

bool IsZeroDigestV1(const canonical::Digest256V1& digest) noexcept
{
    return std::all_of(digest.begin(), digest.end(), [](const auto value) { return value == std::byte{0}; });
}

MigrationCorpusBuildResultV1 BuildMigrationCertificateV1(const MigrationCorpusInputV1& input)
{
    if (input.sources.size() != RequiredSourceLineageCountV1)
        return {MigrationCorpusErrorV1::SourceCountMismatch, std::nullopt};
    if (HasDuplicateBy(input.sources, [](const auto& value) { return value.role; }))
        return {MigrationCorpusErrorV1::DuplicateSourceRole, std::nullopt};
    if (std::any_of(input.sources.begin(), input.sources.end(), [](const auto& value) {
            return value.role.empty() || IsZeroDigestV1(value.sourceSha256);
        }))
        return {MigrationCorpusErrorV1::InvalidSourceDigest, std::nullopt};
    if (HasDuplicateBy(input.evidence, [](const auto& value) { return value.id; }))
        return {MigrationCorpusErrorV1::DuplicateEvidenceId, std::nullopt};
    if (std::any_of(input.evidence.begin(), input.evidence.end(), [](const auto& value) {
            return value.id.empty() || IsZeroDigestV1(value.evidenceSha256);
        }))
        return {MigrationCorpusErrorV1::InvalidEvidenceDigest, std::nullopt};

    std::set<std::uint32_t> invariantOrdinals;
    for (const auto& invariant : input.invariants)
    {
        if (!invariantOrdinals.insert(invariant.ordinal).second)
            return {MigrationCorpusErrorV1::DuplicateInvariant, std::nullopt};
        if (invariant.outcome != MigrationOutcomeV1::Reproduced &&
            invariant.outcome != MigrationOutcomeV1::VersionedReplacement &&
            invariant.outcome != MigrationOutcomeV1::ExplicitlyNotCarriedWithOwnerReason)
            return {MigrationCorpusErrorV1::NonTerminalOutcome, std::nullopt};
        if (invariant.evidenceIds.empty())
            return {MigrationCorpusErrorV1::InvariantEvidenceMissing, std::nullopt};
    }
    if (input.invariants.size() != RequiredInvariantCountV1)
        return {MigrationCorpusErrorV1::IncompleteInvariantSet, std::nullopt};
    for (std::uint32_t ordinal = 1u; ordinal <= RequiredInvariantCountV1; ++ordinal)
    {
        if (!invariantOrdinals.contains(ordinal))
            return {MigrationCorpusErrorV1::IncompleteInvariantSet, std::nullopt};
    }

    if (HasDuplicateBy(input.cases, [](const auto& value) { return value.id; }))
        return {MigrationCorpusErrorV1::DuplicateCaseId, std::nullopt};

    std::set<std::string> coveredSources;
    std::set<std::uint32_t> coveredInvariants;
    std::uint32_t correctnessCases = 0u;
    std::uint32_t observationalCases = 0u;
    for (const auto& item : input.cases)
    {
        if (item.id.empty() || IsZeroDigestV1(item.boundIdentity))
            return {MigrationCorpusErrorV1::InvalidBoundIdentity, std::nullopt};
        if (item.lane == EvidenceLaneV1::Correctness)
        {
            ++correctnessCases;
            if (item.bindingKind != BindingKindV1::V2Authority)
                return {MigrationCorpusErrorV1::BindingKindMismatch, std::nullopt};
        }
        else
        {
            ++observationalCases;
            if (item.bindingKind != BindingKindV1::RetainedObservationalEvidence)
                return {MigrationCorpusErrorV1::BindingKindMismatch, std::nullopt};
        }

        for (const auto& role : item.sourceRoles)
        {
            if (!ContainsSource(input, role))
                return {MigrationCorpusErrorV1::UnknownSourceReference, std::nullopt};
            coveredSources.insert(role);
        }
        for (const auto ordinal : item.invariantOrdinals)
        {
            if (!ContainsInvariant(input, ordinal))
                return {MigrationCorpusErrorV1::UnknownInvariantReference, std::nullopt};
            coveredInvariants.insert(ordinal);
        }
        for (const auto& id : item.evidenceIds)
        {
            const auto* evidence = FindEvidence(input, id);
            if (evidence == nullptr)
                return {MigrationCorpusErrorV1::UnknownEvidenceReference, std::nullopt};
            if (evidence->lane != item.lane)
                return {MigrationCorpusErrorV1::EvidenceLaneMismatch, std::nullopt};
        }
    }

    for (const auto& invariant : input.invariants)
    {
        for (const auto& id : invariant.evidenceIds)
        {
            if (FindEvidence(input, id) == nullptr)
                return {MigrationCorpusErrorV1::UnknownEvidenceReference, std::nullopt};
        }
    }

    if (coveredSources.size() != input.sources.size())
        return {MigrationCorpusErrorV1::SourceCoverageIncomplete, std::nullopt};
    if (coveredInvariants.size() != RequiredInvariantCountV1)
        return {MigrationCorpusErrorV1::InvariantCoverageIncomplete, std::nullopt};
    if (input.runtimeCandidatePolicyAuthorized)
        return {MigrationCorpusErrorV1::RuntimePolicyLeak, std::nullopt};
    if (input.referenceProjectsDeleted)
        return {MigrationCorpusErrorV1::ReferenceDeletionBeforeDecision, std::nullopt};

    const auto identity = ComputeCertificateIdentity(input);
    return {
        MigrationCorpusErrorV1::None,
        OpaqueMigrationCertificateV1(
            identity,
            static_cast<std::uint32_t>(input.sources.size()),
            static_cast<std::uint32_t>(input.invariants.size()),
            static_cast<std::uint32_t>(input.cases.size()),
            correctnessCases,
            observationalCases)};
}
}
