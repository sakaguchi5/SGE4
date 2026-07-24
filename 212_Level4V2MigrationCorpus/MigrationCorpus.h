#pragma once

#include "../209_Level4V2RuntimeRecovery/RuntimeRecovery.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sge4_5::v2::migration
{
namespace canonical = sge4_5::v2::canonical;

struct MigrationCertificateIdentityTagV1;
using MigrationCertificateIdentity = canonical::CanonicalIdentityV1<MigrationCertificateIdentityTagV1>;

inline constexpr std::uint32_t MigrationCorpusSchemaVersionV1 = 1u;
inline constexpr std::uint32_t RequiredSourceLineageCountV1 = 16u;
inline constexpr std::uint32_t RequiredInvariantCountV1 = 40u;

// Correctness and observational performance remain distinct evidence lanes.
enum class EvidenceLaneV1 : std::uint8_t
{
    Correctness = 1,
    ObservationalPerformance = 2
};

enum class MigrationOutcomeV1 : std::uint8_t
{
    Reproduced = 1,
    VersionedReplacement = 2,
    ExplicitlyNotCarriedWithOwnerReason = 3
};

enum class BindingKindV1 : std::uint8_t
{
    V2Authority = 1,
    RetainedObservationalEvidence = 2
};

struct SourceLineageV1 final
{
    std::string role;
    canonical::Digest256V1 sourceSha256;
};

struct EvidenceRecordV1 final
{
    std::string id;
    EvidenceLaneV1 lane = EvidenceLaneV1::Correctness;
    canonical::Digest256V1 evidenceSha256;
};

struct InvariantOutcomeV1 final
{
    std::uint32_t ordinal = 0;
    MigrationOutcomeV1 outcome = MigrationOutcomeV1::Reproduced;
    std::vector<std::string> evidenceIds;
};

struct MigrationCaseV1 final
{
    std::string id;
    EvidenceLaneV1 lane = EvidenceLaneV1::Correctness;
    std::vector<std::string> sourceRoles;
    std::vector<std::uint32_t> invariantOrdinals;
    BindingKindV1 bindingKind = BindingKindV1::V2Authority;
    canonical::Digest256V1 boundIdentity;
    std::vector<std::string> evidenceIds;
};

struct MigrationCorpusInputV1 final
{
    std::vector<SourceLineageV1> sources;
    std::vector<EvidenceRecordV1> evidence;
    std::vector<InvariantOutcomeV1> invariants;
    std::vector<MigrationCaseV1> cases;
    bool runtimeCandidatePolicyAuthorized = false;
    bool referenceProjectsDeleted = false;
};

enum class MigrationCorpusErrorV1 : std::uint8_t
{
    None = 0,
    SourceCountMismatch,
    DuplicateSourceRole,
    InvalidSourceDigest,
    DuplicateEvidenceId,
    InvalidEvidenceDigest,
    DuplicateInvariant,
    IncompleteInvariantSet,
    NonTerminalOutcome,
    InvariantEvidenceMissing,
    DuplicateCaseId,
    UnknownSourceReference,
    UnknownEvidenceReference,
    UnknownInvariantReference,
    InvalidBoundIdentity,
    BindingKindMismatch,
    SourceCoverageIncomplete,
    InvariantCoverageIncomplete,
    EvidenceLaneMismatch,
    RuntimePolicyLeak,
    ReferenceDeletionBeforeDecision
};

struct MigrationCorpusBuildResultV1;

class OpaqueMigrationCertificateV1 final
{
public:
    OpaqueMigrationCertificateV1(const OpaqueMigrationCertificateV1&) = default;
    OpaqueMigrationCertificateV1& operator=(const OpaqueMigrationCertificateV1&) = default;

    [[nodiscard]] const MigrationCertificateIdentity& Identity() const noexcept { return identity_; }
    [[nodiscard]] std::uint32_t SourceCount() const noexcept { return sourceCount_; }
    [[nodiscard]] std::uint32_t InvariantCount() const noexcept { return invariantCount_; }
    [[nodiscard]] std::uint32_t CaseCount() const noexcept { return caseCount_; }
    [[nodiscard]] std::uint32_t CorrectnessCaseCount() const noexcept { return correctnessCaseCount_; }
    [[nodiscard]] std::uint32_t ObservationalCaseCount() const noexcept { return observationalCaseCount_; }

private:
    friend struct MigrationCorpusBuildResultV1;
    friend MigrationCorpusBuildResultV1 BuildMigrationCertificateV1(const MigrationCorpusInputV1&);

    OpaqueMigrationCertificateV1(
        MigrationCertificateIdentity identity,
        std::uint32_t sourceCount,
        std::uint32_t invariantCount,
        std::uint32_t caseCount,
        std::uint32_t correctnessCaseCount,
        std::uint32_t observationalCaseCount) noexcept
        : identity_(std::move(identity)), sourceCount_(sourceCount), invariantCount_(invariantCount),
          caseCount_(caseCount), correctnessCaseCount_(correctnessCaseCount),
          observationalCaseCount_(observationalCaseCount) {}

    MigrationCertificateIdentity identity_;
    std::uint32_t sourceCount_ = 0;
    std::uint32_t invariantCount_ = 0;
    std::uint32_t caseCount_ = 0;
    std::uint32_t correctnessCaseCount_ = 0;
    std::uint32_t observationalCaseCount_ = 0;
};

struct MigrationCorpusBuildResultV1 final
{
    MigrationCorpusErrorV1 error = MigrationCorpusErrorV1::None;
    std::optional<OpaqueMigrationCertificateV1> certificate;
    [[nodiscard]] bool Accepted() const noexcept
    {
        return error == MigrationCorpusErrorV1::None && certificate.has_value();
    }
};

[[nodiscard]] MigrationCorpusBuildResultV1 BuildMigrationCertificateV1(
    const MigrationCorpusInputV1& input);

[[nodiscard]] canonical::Digest256V1 ParseSha256HexV1(std::string_view hex);
[[nodiscard]] bool IsZeroDigestV1(const canonical::Digest256V1& digest) noexcept;
}
