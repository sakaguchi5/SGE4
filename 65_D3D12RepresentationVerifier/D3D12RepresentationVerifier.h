#pragma once

#include "../60_PgaRigidTransformSemantic/PgaRigidTransformSemantic.h"
#include "../63_D3D12RepresentationCandidate/D3D12RepresentationCandidate.h"

#include <string>
#include <vector>

namespace sge4_5::spiral1::representation
{
enum class RepresentationVerificationErrorCodeV1 : std::uint32_t
{
    InvalidTargetProfile,
    InvalidRawStructure,
    SemanticIdentityMismatch,
    TargetProfileMismatch,
    TemplateContractMismatch,
    ProgramBinaryMismatch,
    SchemaMismatch,
    ObservationContractMismatch,
    PointCountMismatch,
    DispatchMismatch,
    ConstantPayloadMismatch,
    CandidateIdentityMismatch,
    VerifiedPlanContextMismatch,
    VerifiedPlanSealMismatch
};

struct RepresentationVerificationErrorV1 final
{
    RepresentationVerificationErrorCodeV1 code = RepresentationVerificationErrorCodeV1::InvalidRawStructure;
    std::string message;
};

class IndependentRepresentationVerifierV1;

class VerifiedRepresentationPlanV1 final
{
public:
    VerifiedRepresentationPlanV1(const VerifiedRepresentationPlanV1&) = default;
    VerifiedRepresentationPlanV1(VerifiedRepresentationPlanV1&&) noexcept = default;
    VerifiedRepresentationPlanV1& operator=(const VerifiedRepresentationPlanV1&) = default;
    VerifiedRepresentationPlanV1& operator=(VerifiedRepresentationPlanV1&&) noexcept = default;

    [[nodiscard]] const RawRepresentationCandidateV1& Candidate() const noexcept { return candidate_; }
    [[nodiscard]] const base::Digest256& VerifierSchemaIdentity() const noexcept { return verifierSchemaIdentity_; }
    [[nodiscard]] const base::Digest256& SealDigest() const noexcept { return sealDigest_; }

private:
    friend class IndependentRepresentationVerifierV1;
    VerifiedRepresentationPlanV1(RawRepresentationCandidateV1 candidate, base::Digest256 schema, base::Digest256 seal)
        : candidate_(std::move(candidate)), verifierSchemaIdentity_(schema), sealDigest_(seal) {}
    RawRepresentationCandidateV1 candidate_;
    base::Digest256 verifierSchemaIdentity_{};
    base::Digest256 sealDigest_{};
};

class IndependentRepresentationVerifierV1 final
{
public:
    [[nodiscard]] static base::Result<VerifiedRepresentationPlanV1, RepresentationVerificationErrorV1> Verify(
        const semantic::ApplyPgaMotorSemanticV1& semanticValue,
        const RepresentationTargetProfileV1& targetProfile,
        const RawRepresentationCandidateV1& candidate);
    [[nodiscard]] static base::Result<void, RepresentationVerificationErrorV1> ValidatePlanContext(
        const VerifiedRepresentationPlanV1& plan,
        const semantic::ApplyPgaMotorSemanticV1& semanticValue,
        const RepresentationTargetProfileV1& targetProfile);
};

[[nodiscard]] base::Digest256 RepresentationVerifierSchemaIdentityV1();
[[nodiscard]] std::vector<std::byte> SerializeVerifiedRepresentationPlanV1(const VerifiedRepresentationPlanV1& value);
}
