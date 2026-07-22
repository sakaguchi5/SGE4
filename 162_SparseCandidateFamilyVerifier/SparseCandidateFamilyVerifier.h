#pragma once

#include "../00_Foundation/Result.h"
#include "../00_Foundation/Sha256.h"
#include "../154_SparseWorkSetSemantic/SparseWorkSetSemantic.h"
#include "../160_SparseCandidateFamily/SparseCandidateFamily.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace sge4_5::spiral6::family_verification
{
struct SparseFamilyVerificationContextV1 final
{
    base::Digest256 targetProfileIdentity{};
    base::Digest256 observationContractIdentity{};
    base::Digest256 completionContractIdentity{};
    base::Digest256 deviceEpochPolicyIdentity{};
    base::Digest256 spiral4IndirectArtifactIdentity{};
    bool operator==(const SparseFamilyVerificationContextV1&) const = default;
};

struct VerifiedSparseFamilyOperationV1 final
{
    family_candidate::SparseFamilyCandidateKindV1 kind = family_candidate::SparseFamilyCandidateKindV1::DenseMembershipMaskPredicate;
    family_candidate::SparseFamilyRepresentationRoleV1 representationRole = family_candidate::SparseFamilyRepresentationRoleV1::DenseMembershipMask4096;
    family_candidate::SparseFamilyOperationCodeV1 operationCode = family_candidate::SparseFamilyOperationCodeV1::ExecuteDenseMaskPredicate;
    family_candidate::SparseFamilyDispatchPolicyV1 dispatchPolicy = family_candidate::SparseFamilyDispatchPolicyV1::FixedUniverseGroups;
    family_candidate::SparseFamilyCompletionPolicyV1 completionPolicy = family_candidate::SparseFamilyCompletionPolicyV1::ExecuteIndirectCompletionAuthorizesExactWriteSet;
    family_candidate::SparseFamilyDeviceEpochPolicyV1 deviceEpochPolicy = family_candidate::SparseFamilyDeviceEpochPolicyV1::DerivedRepresentationBoundToDeviceEpoch;
    std::uint32_t universeCount = 0;
    std::uint32_t activeCount = 0;
    std::uint32_t activeBlockCount = 0;
    std::uint32_t threadsPerGroup = 0;
    std::uint32_t dispatchGroupsX = 0;
    std::uint32_t payloadStrideBytes = 0;
    std::uint32_t fixedCapacityBytes = 0;
    std::uint32_t outputCapacity = 0;
    std::uint32_t outputStrideBytes = 0;
    std::uint32_t zeroActiveMapsToZeroDispatch = 0;
    base::Digest256 representationResourceContractIdentity{};
    base::Digest256 consumerProgramIdentity{};
    base::Digest256 artifactIdentity{};
    base::Digest256 representationResourceIdentity{};
    base::Digest256 writeSetAuthorityIdentity{};
    base::Digest256 cu2CompactArtifactIdentity{};
    base::Digest256 operationIdentity{};
};

struct VerifiedSparseFamilyPlanV1 final
{
    base::Digest256 semanticIdentity{};
    base::Digest256 sparseSetIdentity{};
    SparseFamilyVerificationContextV1 context{};
    VerifiedSparseFamilyOperationV1 operation{};
    base::Digest256 planIdentity{};
};

class VerifiedSparseFamilyCandidateV1 final
{
public:
    VerifiedSparseFamilyCandidateV1() = delete;
    VerifiedSparseFamilyCandidateV1(const VerifiedSparseFamilyCandidateV1&) = default;
    VerifiedSparseFamilyCandidateV1& operator=(const VerifiedSparseFamilyCandidateV1&) = default;
    [[nodiscard]] const VerifiedSparseFamilyPlanV1& Plan() const noexcept { return plan_; }
    [[nodiscard]] const base::Digest256& CertificateIdentity() const noexcept { return certificateIdentity_; }
private:
    VerifiedSparseFamilyCandidateV1(VerifiedSparseFamilyPlanV1 plan, base::Digest256 certificateIdentity);
    VerifiedSparseFamilyPlanV1 plan_;
    base::Digest256 certificateIdentity_{};
    friend base::Result<VerifiedSparseFamilyCandidateV1, std::string> VerifySparseFamilyCandidateV1(
        const semantic::SparsePointTransformSemanticV1&,
        const semantic::ExactSparseWorkSetV1&,
        const SparseFamilyVerificationContextV1&,
        const family_candidate::RawSparseFamilyCandidateV1&);
};

[[nodiscard]] SparseFamilyVerificationContextV1 BuildCanonicalSparseFamilyVerificationContextV1();
[[nodiscard]] base::Result<VerifiedSparseFamilyCandidateV1, std::string> VerifySparseFamilyCandidateV1(
    const semantic::SparsePointTransformSemanticV1& semantic,
    const semantic::ExactSparseWorkSetV1& set,
    const SparseFamilyVerificationContextV1& context,
    const family_candidate::RawSparseFamilyCandidateV1& proposal);
[[nodiscard]] base::Result<void, std::string> ValidateVerifiedSparseFamilyCandidateV1(
    const VerifiedSparseFamilyCandidateV1& verified,
    const semantic::SparsePointTransformSemanticV1& semantic,
    const semantic::ExactSparseWorkSetV1& set,
    const SparseFamilyVerificationContextV1& context);

[[nodiscard]] std::vector<std::byte> SerializeVerifiedSparseFamilyOperationV1(
    const VerifiedSparseFamilyOperationV1& value);
[[nodiscard]] std::vector<std::byte> SerializeVerifiedSparseFamilyPlanV1(
    const VerifiedSparseFamilyPlanV1& value);
[[nodiscard]] std::vector<std::byte> SerializeVerifiedSparseFamilyCandidateV1(
    const VerifiedSparseFamilyCandidateV1& value);
}
