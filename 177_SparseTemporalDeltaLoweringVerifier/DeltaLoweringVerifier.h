#pragma once

#include "../00_Foundation/Result.h"
#include "../00_Foundation/Sha256.h"
#include "../172_SparseTemporalDeltaSemantic/SparseTemporalDeltaSemantic.h"
#include "../175_SparseTemporalDeltaLoweringCandidate/DeltaLoweringCandidate.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace sge4_5::spiral7::verification
{
struct DeltaVerificationContextV1 final
{
    base::Digest256 targetProfileIdentity{};
    base::Digest256 observationContractIdentity{};
    base::Digest256 deltaResourceContractIdentity{};
    base::Digest256 previousHistoryResourceContractIdentity{};
    base::Digest256 outputHistoryResourceContractIdentity{};
    base::Digest256 completionContractIdentity{};
    base::Digest256 deviceEpochPolicyIdentity{};

    bool operator==(const DeltaVerificationContextV1&) const = default;
};

struct VerifiedCompactDeltaOperationV1 final
{
    candidate::DeltaLoweringKindV1 kind =
        candidate::DeltaLoweringKindV1::CompactDeltaIndexHistoryReuse;
    contract::DeltaRepresentationRoleV1 representationRole =
        contract::DeltaRepresentationRoleV1::CanonicalSortedIndexActionList;
    contract::DeltaOperationCodeV1 operationCode =
        contract::DeltaOperationCodeV1::ExecuteCompactDeltaHistory;
    contract::DeltaOutputHistoryPolicyV1 outputHistoryPolicy =
        contract::DeltaOutputHistoryPolicyV1::CopyPreviousHistoryThenWriteExactlyTransitionSet;
    candidate::DeltaCompletionPolicyV1 completionPolicy =
        candidate::DeltaCompletionPolicyV1::ExecuteIndirectCompletionAuthorizesExactTransitionSet;
    candidate::DeltaDeviceEpochPolicyV1 deviceEpochPolicy =
        candidate::DeltaDeviceEpochPolicyV1::HistoryAndDerivedResourcesBoundToDeviceEpoch;

    std::uint32_t invocationIndex = 0;
    std::uint32_t universeCount = 0;
    std::uint32_t transitionCount = 0;
    std::uint32_t updateCount = 0;
    std::uint32_t clearCount = 0;
    std::uint32_t retainCount = 0;
    std::uint32_t threadsPerGroup = 0;
    std::uint32_t dispatchGroupsX = 0;
    std::uint32_t recordStrideBytes = 0;
    std::uint32_t fixedCapacityBytes = 0;
    std::uint32_t historyBytes = 0;
    std::uint32_t outputCapacity = 0;
    std::uint32_t outputStrideBytes = 0;
    std::uint32_t zeroTransitionMapsToZeroDispatch = 0;

    base::Digest256 spiral4IndirectIdentity{};
    base::Digest256 spiral5TemporalIdentity{};
    base::Digest256 spiral6SparseIdentity{};
    base::Digest256 consumerProgramIdentity{};
    base::Digest256 artifactIdentity{};
    base::Digest256 deltaResourceIdentity{};
    base::Digest256 previousHistoryResourceIdentity{};
    base::Digest256 outputHistoryResourceIdentity{};
    base::Digest256 transitionAuthorityIdentity{};
    base::Digest256 operationIdentity{};
};

struct VerifiedDeltaExecutionPlanV1 final
{
    base::Digest256 semanticIdentity{};
    base::Digest256 invocationIdentity{};
    base::Digest256 previousActiveSetIdentity{};
    base::Digest256 activeSetIdentity{};
    base::Digest256 modifiedSetIdentity{};
    base::Digest256 transitionSetIdentity{};
    base::Digest256 transitionRecordsIdentity{};
    DeltaVerificationContextV1 context{};
    VerifiedCompactDeltaOperationV1 operation{};
    base::Digest256 planIdentity{};
};

class VerifiedDeltaLoweringV1 final
{
public:
    VerifiedDeltaLoweringV1() = delete;
    VerifiedDeltaLoweringV1(const VerifiedDeltaLoweringV1&) = default;
    VerifiedDeltaLoweringV1& operator=(const VerifiedDeltaLoweringV1&) = default;

    [[nodiscard]] const VerifiedDeltaExecutionPlanV1& Plan() const noexcept { return plan_; }
    [[nodiscard]] const base::Digest256& CertificateIdentity() const noexcept
    {
        return certificateIdentity_;
    }

private:
    VerifiedDeltaLoweringV1(
        VerifiedDeltaExecutionPlanV1 plan,
        base::Digest256 certificateIdentity);

    VerifiedDeltaExecutionPlanV1 plan_;
    base::Digest256 certificateIdentity_{};

    friend base::Result<VerifiedDeltaLoweringV1, std::string>
    VerifyDeltaLoweringCandidateV1(
        const semantic::SparseTemporalDeltaSemanticV1&,
        const semantic::SparseDeltaInvocationV1&,
        const DeltaVerificationContextV1&,
        const candidate::RawDeltaLoweringCandidateV1&);
};

[[nodiscard]] DeltaVerificationContextV1 BuildCanonicalDeltaVerificationContextV1();

[[nodiscard]] base::Result<VerifiedDeltaLoweringV1, std::string>
VerifyDeltaLoweringCandidateV1(
    const semantic::SparseTemporalDeltaSemanticV1& semantic,
    const semantic::SparseDeltaInvocationV1& invocation,
    const DeltaVerificationContextV1& context,
    const candidate::RawDeltaLoweringCandidateV1& proposal);

[[nodiscard]] base::Result<void, std::string>
ValidateVerifiedDeltaLoweringContextV1(
    const VerifiedDeltaLoweringV1& verified,
    const semantic::SparseTemporalDeltaSemanticV1& semantic,
    const semantic::SparseDeltaInvocationV1& invocation,
    const DeltaVerificationContextV1& context);

[[nodiscard]] std::vector<std::byte> SerializeVerifiedCompactDeltaOperationV1(
    const VerifiedCompactDeltaOperationV1& value);
[[nodiscard]] std::vector<std::byte> SerializeVerifiedDeltaExecutionPlanV1(
    const VerifiedDeltaExecutionPlanV1& value);
[[nodiscard]] std::vector<std::byte> SerializeVerifiedDeltaLoweringV1(
    const VerifiedDeltaLoweringV1& value);
} // namespace sge4_5::spiral7::verification
