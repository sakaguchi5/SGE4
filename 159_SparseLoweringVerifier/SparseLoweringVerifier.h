#pragma once

#include "../00_Foundation/Result.h"
#include "../00_Foundation/Sha256.h"
#include "../154_SparseWorkSetSemantic/SparseWorkSetSemantic.h"
#include "../157_SparseLoweringCandidate/SparseLoweringCandidate.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace sge4_5::spiral6::verification
{
struct SparseVerificationContextV1 final
{
    base::Digest256 targetProfileIdentity{};
    base::Digest256 observationContractIdentity{};
    base::Digest256 indexResourceContractIdentity{};
    base::Digest256 completionContractIdentity{};
    base::Digest256 deviceEpochPolicyIdentity{};

    bool operator==(const SparseVerificationContextV1&) const = default;
};

struct VerifiedCompactIndexOperationV1 final
{
    candidate::SparseLoweringKindV1 kind =
        candidate::SparseLoweringKindV1::CompactIndexListDispatch;
    contract::SparseRepresentationRoleV1 representationRole =
        contract::SparseRepresentationRoleV1::CompactSortedUniverseIndexList;
    contract::SparseOperationCodeV1 operationCode =
        contract::SparseOperationCodeV1::ExecuteCompactIndexList;
    contract::SparseOutputInitializationV1 outputInitialization =
        contract::SparseOutputInitializationV1::CanonicalFloat4Zero;
    contract::SparseWriteSetPolicyV1 writeSetPolicy =
        contract::SparseWriteSetPolicyV1::ExactlyVerifiedSparseSet;
    candidate::SparseCompletionPolicyV1 completionPolicy =
        candidate::SparseCompletionPolicyV1::ExecuteIndirectCompletionAuthorizesExactWriteSet;
    candidate::SparseDeviceEpochPolicyV1 deviceEpochPolicy =
        candidate::SparseDeviceEpochPolicyV1::DerivedResourceBoundToDeviceEpoch;

    std::uint32_t universeCount = 0;
    std::uint32_t activeCount = 0;
    std::uint32_t threadsPerGroup = 0;
    std::uint32_t dispatchGroupsX = 0;
    std::uint32_t indexStrideBytes = 0;
    std::uint32_t fixedCapacityBytes = 0;
    std::uint32_t outputCapacity = 0;
    std::uint32_t outputStrideBytes = 0;
    std::uint32_t unusedIndexSentinel = 0;
    std::uint32_t zeroActiveMapsToZeroDispatch = 0;

    base::Digest256 spiral4IndirectArtifactIdentity{};
    base::Digest256 consumerProgramIdentity{};
    base::Digest256 artifactIdentity{};
    base::Digest256 indexResourceIdentity{};
    base::Digest256 writeSetAuthorityIdentity{};
    base::Digest256 operationIdentity{};
};

struct VerifiedSparseExecutionPlanV1 final
{
    base::Digest256 semanticIdentity{};
    base::Digest256 sparseSetIdentity{};
    SparseVerificationContextV1 context{};
    VerifiedCompactIndexOperationV1 operation{};
    base::Digest256 planIdentity{};
};

class VerifiedSparseLoweringV1 final
{
public:
    VerifiedSparseLoweringV1() = delete;
    VerifiedSparseLoweringV1(const VerifiedSparseLoweringV1&) = default;
    VerifiedSparseLoweringV1& operator=(const VerifiedSparseLoweringV1&) = default;

    [[nodiscard]] const VerifiedSparseExecutionPlanV1& Plan() const noexcept
    {
        return plan_;
    }
    [[nodiscard]] const base::Digest256& CertificateIdentity() const noexcept
    {
        return certificateIdentity_;
    }

private:
    VerifiedSparseLoweringV1(
        VerifiedSparseExecutionPlanV1 plan,
        base::Digest256 certificateIdentity);

    VerifiedSparseExecutionPlanV1 plan_;
    base::Digest256 certificateIdentity_{};

    friend base::Result<VerifiedSparseLoweringV1, std::string>
    VerifySparseLoweringCandidateV1(
        const semantic::SparsePointTransformSemanticV1&,
        const semantic::ExactSparseWorkSetV1&,
        const SparseVerificationContextV1&,
        const candidate::RawSparseLoweringCandidateV1&);
};

[[nodiscard]] SparseVerificationContextV1
BuildCanonicalSparseVerificationContextV1();

[[nodiscard]] base::Result<VerifiedSparseLoweringV1, std::string>
VerifySparseLoweringCandidateV1(
    const semantic::SparsePointTransformSemanticV1& semantic,
    const semantic::ExactSparseWorkSetV1& set,
    const SparseVerificationContextV1& context,
    const candidate::RawSparseLoweringCandidateV1& proposal);

[[nodiscard]] base::Result<void, std::string>
ValidateVerifiedSparseLoweringContextV1(
    const VerifiedSparseLoweringV1& verified,
    const semantic::SparsePointTransformSemanticV1& semantic,
    const semantic::ExactSparseWorkSetV1& set,
    const SparseVerificationContextV1& context);

[[nodiscard]] std::vector<std::byte> SerializeVerifiedCompactIndexOperationV1(
    const VerifiedCompactIndexOperationV1& value);
[[nodiscard]] std::vector<std::byte> SerializeVerifiedSparseExecutionPlanV1(
    const VerifiedSparseExecutionPlanV1& value);
[[nodiscard]] std::vector<std::byte> SerializeVerifiedSparseLoweringV1(
    const VerifiedSparseLoweringV1& value);
}
