#pragma once

#include "../00_Foundation/Result.h"
#include "../00_Foundation/Sha256.h"
#include "../134_TemporalStateSemantic/TemporalStateSemantic.h"
#include "../147_TemporalCandidateFamily/TemporalCandidateFamily.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace sge4_5::spiral5::family_verification
{
struct TemporalCandidateFamilyVerificationContextV1 final
{
    base::Digest256 targetProfileIdentity{};
    base::Digest256 observationContractIdentity{};
    base::Digest256 deviceEpochPolicyIdentity{};
    bool operator==(const TemporalCandidateFamilyVerificationContextV1&) const = default;
};

struct VerifiedTemporalCandidateOperationV1 final
{
    family_candidate::TemporalCandidateKindV1 kind{};
    family_candidate::TemporalFamilyHistoryRoleV1 historyRole{};
    family_candidate::TemporalIssuancePolicyV1 issuancePolicy{};
    family_candidate::TemporalCompletionPolicyV1 completionPolicy{};
    family_candidate::TemporalHoldPolicyV1 holdPolicy{};
    std::uint32_t timelineLength = 0;
    std::uint32_t workCount = 0;
    std::uint32_t threadsPerGroup = 0;
    std::uint32_t historyDepth = 0;
    std::uint32_t historyBytes = 0;
    std::uint32_t argumentBufferBytes = 0;
    std::uint32_t argumentStrideBytes = 0;
    std::uint32_t maxCommandCount = 0;
    std::uint32_t argumentProducerDispatchX = 0;
    std::uint32_t hierarchyUpdateDispatchX = 0;
    std::uint32_t hierarchyHoldDispatchX = 0;
    std::uint32_t consumerUpdateDispatchX = 0;
    std::uint32_t consumerHoldDispatchX = 0;
    std::uint32_t updateCount = 0;
    std::uint32_t holdCount = 0;
    std::uint32_t maximumSourceGeneration = 0;
    base::Digest256 completionContractIdentity{};
    base::Digest256 spiral4IndirectArtifactIdentity{};
    base::Digest256 argumentProducerProgramIdentity{};
    base::Digest256 hierarchyProgramIdentity{};
    base::Digest256 consumerProgramIdentity{};
    base::Digest256 artifactIdentity{};
    base::Digest256 historyResourceIdentity{};
    base::Digest256 invocationAuthorityIdentity{};
    base::Digest256 operationIdentity{};
};

struct VerifiedTemporalCandidatePlanV1 final
{
    base::Digest256 semanticIdentity{};
    base::Digest256 scheduleIdentity{};
    TemporalCandidateFamilyVerificationContextV1 context{};
    VerifiedTemporalCandidateOperationV1 operation{};
    base::Digest256 planIdentity{};
};

class VerifiedTemporalCandidateV1 final
{
public:
    VerifiedTemporalCandidateV1() = delete;
    VerifiedTemporalCandidateV1(const VerifiedTemporalCandidateV1&) = default;
    VerifiedTemporalCandidateV1& operator=(const VerifiedTemporalCandidateV1&) = default;
    [[nodiscard]] const VerifiedTemporalCandidatePlanV1& Plan() const noexcept { return plan_; }
    [[nodiscard]] const base::Digest256& CertificateIdentity() const noexcept { return certificateIdentity_; }
private:
    VerifiedTemporalCandidateV1(VerifiedTemporalCandidatePlanV1 plan, base::Digest256 certificateIdentity);
    VerifiedTemporalCandidatePlanV1 plan_;
    base::Digest256 certificateIdentity_{};
    friend base::Result<VerifiedTemporalCandidateV1, std::string> VerifyTemporalCandidateV1(
        const semantic::TemporalStateSemanticV1&,
        const semantic::UpdateScheduleV1&,
        const TemporalCandidateFamilyVerificationContextV1&,
        const family_candidate::RawTemporalCandidateV1&);
};

[[nodiscard]] TemporalCandidateFamilyVerificationContextV1
BuildCanonicalTemporalCandidateFamilyVerificationContextV1();
[[nodiscard]] base::Result<VerifiedTemporalCandidateV1, std::string> VerifyTemporalCandidateV1(
    const semantic::TemporalStateSemanticV1& semantic,
    const semantic::UpdateScheduleV1& schedule,
    const TemporalCandidateFamilyVerificationContextV1& context,
    const family_candidate::RawTemporalCandidateV1& proposal);
[[nodiscard]] base::Result<void, std::string> ValidateVerifiedTemporalCandidateContextV1(
    const VerifiedTemporalCandidateV1& verified,
    const semantic::TemporalStateSemanticV1& semantic,
    const semantic::UpdateScheduleV1& schedule,
    const TemporalCandidateFamilyVerificationContextV1& context);
[[nodiscard]] std::vector<std::byte> SerializeVerifiedTemporalCandidateOperationV1(
    const VerifiedTemporalCandidateOperationV1& value);
[[nodiscard]] std::vector<std::byte> SerializeVerifiedTemporalCandidateV1(
    const VerifiedTemporalCandidateV1& value);
}
