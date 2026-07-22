#pragma once

#include "../00_Foundation/Result.h"
#include "../00_Foundation/Sha256.h"
#include "../120_ActiveWorkSemantic/ActiveWorkSemantic.h"
#include "../127_Spiral4CandidateFamilyContracts/CandidateFamilyContracts.h"
#include "../128_ActiveWorkCandidateFamily/ActiveWorkCandidateFamily.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace sge4_5::spiral4::family_verification
{
struct CandidateFamilyVerificationContextV1 final
{
    base::Digest256 targetProfileIdentity{};
    base::Digest256 activeCountContractIdentity{};
    base::Digest256 observationContractIdentity{};
    bool operator==(const CandidateFamilyVerificationContextV1&) const = default;
};

struct VerifiedCandidateFamilyPlanV1 final
{
    base::Digest256 semanticIdentity{};
    CandidateFamilyVerificationContextV1 context{};
    family_contract::CandidateFamilyKindV1 kind =
        family_contract::CandidateFamilyKindV1::FixedMaximumGuarded;
    family_contract::CommandSignaturePolicyV1 commandSignaturePolicy =
        family_contract::CommandSignaturePolicyV1::None;
    family_contract::IssuancePolicyV1 issuancePolicy =
        family_contract::IssuancePolicyV1::FixedDirectDispatch;

    std::uint32_t batchSize = 0;
    std::uint32_t maxBatchSlots = 0;
    std::uint32_t maxWorkCount = 0;
    std::uint32_t threadsPerGroup = 0;
    std::uint32_t directDispatchX = 0;
    std::uint32_t argumentStrideBytes = 0;
    std::uint32_t argumentBufferBytes = 0;
    std::uint32_t maxCommandCount = 0;
    std::uint32_t producerDispatchX = 0;

    base::Digest256 commandSignatureContractIdentity{};
    base::Digest256 producerProgramTemplateIdentity{};
    base::Digest256 consumerProgramTemplateIdentity{};
    base::Digest256 artifactIdentity{};
    base::Digest256 planIdentity{};
};

class VerifiedCandidateFamilyV1 final
{
public:
    VerifiedCandidateFamilyV1() = delete;
    VerifiedCandidateFamilyV1(const VerifiedCandidateFamilyV1&) = default;
    VerifiedCandidateFamilyV1& operator=(const VerifiedCandidateFamilyV1&) = default;

    [[nodiscard]] const VerifiedCandidateFamilyPlanV1& Plan() const noexcept { return plan_; }
    [[nodiscard]] const base::Digest256& CertificateIdentity() const noexcept { return certificateIdentity_; }

private:
    VerifiedCandidateFamilyV1(
        VerifiedCandidateFamilyPlanV1 plan,
        base::Digest256 certificateIdentity);

    VerifiedCandidateFamilyPlanV1 plan_;
    base::Digest256 certificateIdentity_{};

    friend base::Result<VerifiedCandidateFamilyV1, std::string>
    VerifyCandidateFamilyV1(
        const semantic::ActiveWorkSemanticV1&,
        const CandidateFamilyVerificationContextV1&,
        const family_candidate::RawCandidateFamilyV1&);
};

[[nodiscard]] CandidateFamilyVerificationContextV1
BuildCanonicalCandidateFamilyVerificationContextV1();

[[nodiscard]] base::Result<VerifiedCandidateFamilyV1, std::string>
VerifyCandidateFamilyV1(
    const semantic::ActiveWorkSemanticV1& semantic,
    const CandidateFamilyVerificationContextV1& context,
    const family_candidate::RawCandidateFamilyV1& proposal);

[[nodiscard]] base::Result<void, std::string>
ValidateVerifiedCandidateFamilyContextV1(
    const VerifiedCandidateFamilyV1& verified,
    const semantic::ActiveWorkSemanticV1& semantic,
    const CandidateFamilyVerificationContextV1& context);

[[nodiscard]] base::Result<
    std::vector<family_contract::BatchDispatchRecordV1>,
    std::string>
DeriveVerifiedBatchDispatchRecordsV1(
    const VerifiedCandidateFamilyV1& verified,
    semantic::ActiveWorkCountV1 activeCount);

[[nodiscard]] std::vector<std::byte>
SerializeVerifiedCandidateFamilyPlanV1(
    const VerifiedCandidateFamilyPlanV1& value);

[[nodiscard]] std::vector<std::byte>
SerializeVerifiedCandidateFamilyV1(
    const VerifiedCandidateFamilyV1& value);
}
