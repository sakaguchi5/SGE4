#pragma once

#include "../189_Level4V2CanonicalVocabulary/CanonicalVocabulary.h"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace sge4_5::v2::frozen
{
class FrozenAuthorityBuilderV1;
}

namespace sge4_5::v2::authority
{
namespace canonical = sge4_5::v2::canonical;

struct OperationOrdinalTagV1;
using OperationOrdinal = canonical::StrongScalarV1<OperationOrdinalTagV1>;

struct AuthorityContextIdentityTagV1;
struct OperationSequenceIdentityTagV1;
struct PlannerProposalIdentityTagV1;
struct VerificationSealIdentityTagV1;

using AuthorityContextIdentity = canonical::CanonicalIdentityV1<AuthorityContextIdentityTagV1>;
using OperationSequenceIdentity = canonical::CanonicalIdentityV1<OperationSequenceIdentityTagV1>;
using PlannerProposalIdentity = canonical::CanonicalIdentityV1<PlannerProposalIdentityTagV1>;
using VerificationSealIdentity = canonical::CanonicalIdentityV1<VerificationSealIdentityTagV1>;

struct SemanticDefinitionV1 final
{
    canonical::SemanticDescriptorV1 descriptor;
    std::vector<std::byte> canonicalPayload;
};

template<class Identity>
struct CanonicalBindingDefinitionV1 final
{
    Identity identity;
    std::uint32_t schemaVersion;
    std::vector<std::byte> canonicalPayload;
};

using TargetProfileDefinitionV1 = CanonicalBindingDefinitionV1<canonical::TargetProfileIdentity>;
using ResourceContractDefinitionV1 = CanonicalBindingDefinitionV1<canonical::ResourceContractIdentity>;
using WriteSetDefinitionV1 = CanonicalBindingDefinitionV1<canonical::WriteSetIdentity>;

struct ExplicitCandidateInputV1 final
{
    std::uint32_t schemaVersion;
    std::vector<std::byte> canonicalPayload;
};

struct AuthorityRequestV1 final
{
    SemanticDefinitionV1 semantic;
    canonical::InvocationDescriptorV1 invocation;
    TargetProfileDefinitionV1 targetProfile;
    ResourceContractDefinitionV1 resourceContract;
    WriteSetDefinitionV1 legalWriteSet;
};

enum class AuthorityOperationKindV1 : std::uint8_t
{
    BindSemantic = 1,
    BindInvocation = 2,
    BindCandidate = 3,
    BindTargetProfile = 4,
    BindResourceContract = 5,
    BindLegalWriteSet = 6
};

struct AuthorityOperationV1 final
{
    OperationOrdinal ordinal;
    AuthorityOperationKindV1 kind;
    canonical::Digest256V1 subjectIdentity;
    auto operator<=>(const AuthorityOperationV1&) const = default;
};

struct PlannerProposalV1 final
{
    PlannerProposalIdentity identity;
    canonical::RawCandidateProposalV1 rawCandidate;
    std::vector<AuthorityOperationV1> operations;
};

enum class VerificationErrorV1 : std::uint8_t
{
    None = 0,
    SemanticDefinitionIdentityMismatch = 1,
    InvocationSemanticBindingMismatch = 2,
    InvocationDefinitionIdentityMismatch = 3,
    TargetProfileDefinitionIdentityMismatch = 4,
    ResourceContractDefinitionIdentityMismatch = 5,
    WriteSetDefinitionIdentityMismatch = 6,
    CandidateSemanticBindingMismatch = 7,
    CandidateInvocationBindingMismatch = 8,
    CandidateTargetBindingMismatch = 9,
    CandidateResourceBindingMismatch = 10,
    CandidateWriteSetBindingMismatch = 11,
    CandidateIdentityMismatch = 12,
    OperationCountMismatch = 13,
    OperationOrdinalMismatch = 14,
    OperationKindMismatch = 15,
    OperationSubjectMismatch = 16,
    PlannerProposalIdentityMismatch = 17
};

class IndependentVerifierV1;

class VerifiedAuthorityV1 final
{
public:
    VerifiedAuthorityV1(const VerifiedAuthorityV1&) = default;
    VerifiedAuthorityV1& operator=(const VerifiedAuthorityV1&) = default;

    [[nodiscard]] const canonical::OpaqueVerifiedPlanV1& Plan() const noexcept { return plan_; }
    [[nodiscard]] const VerificationSealIdentity& SealIdentity() const noexcept { return sealIdentity_; }
    [[nodiscard]] const AuthorityContextIdentity& ContextIdentity() const noexcept { return contextIdentity_; }
    [[nodiscard]] const canonical::CandidateIdentity& CandidateIdentityValue() const noexcept { return candidateIdentity_; }
    [[nodiscard]] const OperationSequenceIdentity& OperationSequenceIdentityValue() const noexcept { return operationSequenceIdentity_; }

private:
    friend class IndependentVerifierV1;
    friend class sge4_5::v2::frozen::FrozenAuthorityBuilderV1;

    VerifiedAuthorityV1(
        canonical::OpaqueVerifiedPlanV1 plan,
        VerificationSealIdentity sealIdentity,
        AuthorityContextIdentity contextIdentity,
        canonical::SemanticIdentity semanticIdentity,
        canonical::InvocationIdentity invocationIdentity,
        canonical::CandidateIdentity candidateIdentity,
        canonical::TargetProfileIdentity targetProfileIdentity,
        canonical::ResourceContractIdentity resourceContractIdentity,
        canonical::WriteSetIdentity writeSetIdentity,
        OperationSequenceIdentity operationSequenceIdentity) noexcept
        : plan_(std::move(plan)),
          sealIdentity_(std::move(sealIdentity)),
          contextIdentity_(std::move(contextIdentity)),
          semanticIdentity_(std::move(semanticIdentity)),
          invocationIdentity_(std::move(invocationIdentity)),
          candidateIdentity_(std::move(candidateIdentity)),
          targetProfileIdentity_(std::move(targetProfileIdentity)),
          resourceContractIdentity_(std::move(resourceContractIdentity)),
          writeSetIdentity_(std::move(writeSetIdentity)),
          operationSequenceIdentity_(std::move(operationSequenceIdentity))
    {
    }

    canonical::OpaqueVerifiedPlanV1 plan_;
    VerificationSealIdentity sealIdentity_;
    AuthorityContextIdentity contextIdentity_;
    canonical::SemanticIdentity semanticIdentity_;
    canonical::InvocationIdentity invocationIdentity_;
    canonical::CandidateIdentity candidateIdentity_;
    canonical::TargetProfileIdentity targetProfileIdentity_;
    canonical::ResourceContractIdentity resourceContractIdentity_;
    canonical::WriteSetIdentity writeSetIdentity_;
    OperationSequenceIdentity operationSequenceIdentity_;
};

struct VerificationResultV1 final
{
    VerificationErrorV1 error;
    std::optional<VerifiedAuthorityV1> verified;

    [[nodiscard]] bool Accepted() const noexcept
    {
        return error == VerificationErrorV1::None && verified.has_value();
    }
};

[[nodiscard]] SemanticDefinitionV1 MakeSemanticDefinitionV1(
    canonical::SemanticVersion version,
    std::span<const std::byte> canonicalPayload);
[[nodiscard]] canonical::InvocationDescriptorV1 MakeInvocationDescriptorV1(
    canonical::SemanticIdentity semanticIdentity,
    canonical::TimelineOrdinal timelineOrdinal,
    canonical::DeviceEpoch deviceEpoch,
    const canonical::DynamicExecutionDimensionsV1& dimensions);
[[nodiscard]] TargetProfileDefinitionV1 MakeTargetProfileDefinitionV1(
    std::uint32_t schemaVersion,
    std::span<const std::byte> canonicalPayload);
[[nodiscard]] ResourceContractDefinitionV1 MakeResourceContractDefinitionV1(
    std::uint32_t schemaVersion,
    std::span<const std::byte> canonicalPayload);
[[nodiscard]] WriteSetDefinitionV1 MakeWriteSetDefinitionV1(
    std::uint32_t schemaVersion,
    std::span<const std::byte> canonicalPayload);

[[nodiscard]] canonical::SemanticIdentity ComputeSemanticIdentityV1(const SemanticDefinitionV1& definition);
[[nodiscard]] canonical::InvocationIdentity ComputeInvocationIdentityV1(const canonical::InvocationDescriptorV1& invocation);
[[nodiscard]] canonical::TargetProfileIdentity ComputeTargetProfileIdentityV1(const TargetProfileDefinitionV1& definition);
[[nodiscard]] canonical::ResourceContractIdentity ComputeResourceContractIdentityV1(const ResourceContractDefinitionV1& definition);
[[nodiscard]] canonical::WriteSetIdentity ComputeWriteSetIdentityV1(const WriteSetDefinitionV1& definition);
[[nodiscard]] canonical::CandidateIdentity ComputeCandidateIdentityV1(
    const AuthorityRequestV1& request,
    const ExplicitCandidateInputV1& candidateInput);
[[nodiscard]] AuthorityContextIdentity ComputeAuthorityContextIdentityV1(const AuthorityRequestV1& request);
[[nodiscard]] OperationSequenceIdentity ComputeOperationSequenceIdentityV1(std::span<const AuthorityOperationV1> operations);
[[nodiscard]] PlannerProposalIdentity ComputePlannerProposalIdentityV1(
    const canonical::RawCandidateProposalV1& rawCandidate,
    OperationSequenceIdentity operationSequenceIdentity);
[[nodiscard]] canonical::VerifiedPlanIdentity ComputeVerifiedPlanIdentityV1(
    AuthorityContextIdentity contextIdentity,
    canonical::CandidateIdentity candidateIdentity,
    OperationSequenceIdentity operationSequenceIdentity);
[[nodiscard]] VerificationSealIdentity ComputeVerificationSealIdentityV1(
    canonical::VerifiedPlanIdentity verifiedPlanIdentity,
    AuthorityContextIdentity contextIdentity,
    canonical::CandidateIdentity candidateIdentity,
    OperationSequenceIdentity operationSequenceIdentity);
[[nodiscard]] canonical::FrozenArtifactIdentity ComputeFrozenArtifactIdentityV1(
    canonical::VerifiedPlanIdentity verifiedPlanIdentity,
    VerificationSealIdentity sealIdentity,
    canonical::TargetProfileIdentity targetProfileIdentity,
    canonical::ResourceContractIdentity resourceContractIdentity,
    canonical::WriteSetIdentity writeSetIdentity,
    OperationSequenceIdentity operationSequenceIdentity);

inline constexpr std::uint32_t AuthorityModelSchemaVersionV1 = 1u;
inline constexpr std::uint32_t AuthorityOperationCountV1 = 6u;
}

namespace sge4_5::v2::canonical
{
class VerifiedPlanConstructionAccessV1 final
{
private:
    friend class sge4_5::v2::authority::IndependentVerifierV1;

    [[nodiscard]] static OpaqueVerifiedPlanV1 Create(VerifiedPlanIdentity identity) noexcept
    {
        return OpaqueVerifiedPlanV1(std::move(identity));
    }
};
}
