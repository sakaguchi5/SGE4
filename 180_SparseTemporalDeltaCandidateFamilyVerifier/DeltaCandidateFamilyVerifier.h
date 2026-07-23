#pragma once

#include "../00_Foundation/Result.h"
#include "../00_Foundation/Sha256.h"
#include "../172_SparseTemporalDeltaSemantic/SparseTemporalDeltaSemantic.h"
#include "../178_SparseTemporalDeltaCandidateFamily/DeltaCandidateFamily.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace sge4_5::spiral7::family_verification
{
struct DeltaFamilyVerificationContextV1 final
{
    base::Digest256 targetProfileIdentity{};
    base::Digest256 observationContractIdentity{};
    base::Digest256 completionContractIdentity{};
    base::Digest256 deviceEpochPolicyIdentity{};
    bool operator==(const DeltaFamilyVerificationContextV1&) const = default;
};

struct VerifiedDeltaFamilyOperationV1 final
{
    family_candidate::DeltaFamilyCandidateKindV1 kind =
        family_candidate::DeltaFamilyCandidateKindV1::FullActiveDenseRecompute;
    family_candidate::DeltaFamilyRepresentationRoleV1 representationRole =
        family_candidate::DeltaFamilyRepresentationRoleV1::DenseActiveMask4096;
    family_candidate::DeltaFamilyOperationCodeV1 operationCode =
        family_candidate::DeltaFamilyOperationCodeV1::ExecuteFullActiveDenseRecompute;
    family_candidate::DeltaFamilyDispatchPolicyV1 dispatchPolicy =
        family_candidate::DeltaFamilyDispatchPolicyV1::FixedUniverseGroups;
    family_candidate::DeltaFamilyOutputHistoryPolicyV1 outputHistoryPolicy =
        family_candidate::DeltaFamilyOutputHistoryPolicyV1::WriteEntireUniverseFromCurrentActiveMask;
    family_candidate::DeltaFamilyLegalWriteSetV1 legalWriteSet =
        family_candidate::DeltaFamilyLegalWriteSetV1::EntireUniverse;
    family_candidate::DeltaFamilyCompletionPolicyV1 completionPolicy =
        family_candidate::DeltaFamilyCompletionPolicyV1::ExecuteIndirectCompletionAuthorizesCandidateWriteSet;
    family_candidate::DeltaFamilyDeviceEpochPolicyV1 deviceEpochPolicy =
        family_candidate::DeltaFamilyDeviceEpochPolicyV1::FamilyResourcesAndHistoryBoundToDeviceEpoch;

    std::uint32_t invocationIndex = 0;
    std::uint32_t universeCount = 0;
    std::uint32_t activeCount = 0;
    std::uint32_t transitionCount = 0;
    std::uint32_t updateCount = 0;
    std::uint32_t clearCount = 0;
    std::uint32_t retainCount = 0;
    std::uint32_t affectedBlockCount = 0;
    std::uint32_t threadsPerGroup = 0;
    std::uint32_t dispatchGroupsX = 0;
    std::uint32_t payloadStrideBytes = 0;
    std::uint32_t fixedCapacityBytes = 0;
    std::uint32_t historyBytes = 0;
    std::uint32_t outputCapacity = 0;
    std::uint32_t outputStrideBytes = 0;
    std::uint32_t zeroWorkMapsToZeroDispatch = 0;

    base::Digest256 representationResourceContractIdentity{};
    base::Digest256 consumerProgramIdentity{};
    base::Digest256 artifactIdentity{};
    base::Digest256 representationResourceIdentity{};
    base::Digest256 writeSetAuthorityIdentity{};
    base::Digest256 cu2CompactArtifactIdentity{};
    base::Digest256 cu3TransitionAuthorityIdentity{};
    base::Digest256 operationIdentity{};
};

struct VerifiedDeltaFamilyPlanV1 final
{
    base::Digest256 semanticIdentity{};
    base::Digest256 invocationIdentity{};
    base::Digest256 previousActiveSetIdentity{};
    base::Digest256 activeSetIdentity{};
    base::Digest256 modifiedSetIdentity{};
    base::Digest256 transitionSetIdentity{};
    base::Digest256 transitionRecordsIdentity{};
    DeltaFamilyVerificationContextV1 context{};
    VerifiedDeltaFamilyOperationV1 operation{};
    base::Digest256 planIdentity{};
};

class VerifiedDeltaFamilyCandidateV1 final
{
public:
    VerifiedDeltaFamilyCandidateV1() = delete;
    VerifiedDeltaFamilyCandidateV1(const VerifiedDeltaFamilyCandidateV1&) = default;
    VerifiedDeltaFamilyCandidateV1& operator=(const VerifiedDeltaFamilyCandidateV1&) = default;

    [[nodiscard]] const VerifiedDeltaFamilyPlanV1& Plan() const noexcept { return plan_; }
    [[nodiscard]] const base::Digest256& CertificateIdentity() const noexcept
    {
        return certificateIdentity_;
    }

private:
    VerifiedDeltaFamilyCandidateV1(
        VerifiedDeltaFamilyPlanV1 plan,
        base::Digest256 certificateIdentity);

    VerifiedDeltaFamilyPlanV1 plan_;
    base::Digest256 certificateIdentity_{};

    friend base::Result<VerifiedDeltaFamilyCandidateV1, std::string>
    VerifyDeltaFamilyCandidateV1(
        const semantic::SparseTemporalDeltaSemanticV1&,
        const semantic::SparseDeltaInvocationV1&,
        const DeltaFamilyVerificationContextV1&,
        const family_candidate::RawDeltaFamilyCandidateV1&);
};

[[nodiscard]] DeltaFamilyVerificationContextV1
BuildCanonicalDeltaFamilyVerificationContextV1();

[[nodiscard]] base::Result<VerifiedDeltaFamilyCandidateV1, std::string>
VerifyDeltaFamilyCandidateV1(
    const semantic::SparseTemporalDeltaSemanticV1& semantic,
    const semantic::SparseDeltaInvocationV1& invocation,
    const DeltaFamilyVerificationContextV1& context,
    const family_candidate::RawDeltaFamilyCandidateV1& proposal);

[[nodiscard]] base::Result<void, std::string>
ValidateVerifiedDeltaFamilyCandidateV1(
    const VerifiedDeltaFamilyCandidateV1& verified,
    const semantic::SparseTemporalDeltaSemanticV1& semantic,
    const semantic::SparseDeltaInvocationV1& invocation,
    const DeltaFamilyVerificationContextV1& context);

[[nodiscard]] std::vector<std::byte> SerializeVerifiedDeltaFamilyOperationV1(
    const VerifiedDeltaFamilyOperationV1& value);
[[nodiscard]] std::vector<std::byte> SerializeVerifiedDeltaFamilyPlanV1(
    const VerifiedDeltaFamilyPlanV1& value);
[[nodiscard]] std::vector<std::byte> SerializeVerifiedDeltaFamilyCandidateV1(
    const VerifiedDeltaFamilyCandidateV1& value);
} // namespace sge4_5::spiral7::family_verification
