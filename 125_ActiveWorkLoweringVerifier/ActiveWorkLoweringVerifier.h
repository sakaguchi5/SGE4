#pragma once

#include "../00_Foundation/Result.h"
#include "../00_Foundation/Sha256.h"
#include "../120_ActiveWorkSemantic/ActiveWorkSemantic.h"
#include "../123_ActiveWorkLoweringCandidate/ActiveWorkLoweringCandidate.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace sge4_5::spiral4::verification
{
struct ActiveWorkVerificationContextV1 final
{
    base::Digest256 targetProfileIdentity{};
    base::Digest256 activeCountContractIdentity{};
    base::Digest256 observationContractIdentity{};
    base::Digest256 commandSignatureContractIdentity{};

    bool operator==(const ActiveWorkVerificationContextV1&) const = default;
};

struct VerifiedSingleIndirectOperationV1 final
{
    candidate::ActiveWorkLoweringKindV1 kind =
        candidate::ActiveWorkLoweringKindV1::
            SingleExecuteIndirectDispatch;
    contract::IndirectCommandKindV1 commandKind =
        contract::IndirectCommandKindV1::Dispatch;
    contract::IndirectOperationCodeV1 operationCode =
        contract::IndirectOperationCodeV1::ExecuteDispatch;
    candidate::ActiveRangePolicyV1 activeRangePolicy =
        candidate::ActiveRangePolicyV1::PrefixZeroToActiveCount;
    candidate::IndirectStatePolicyV1 statePolicy =
        candidate::IndirectStatePolicyV1::
            UavBarrierThenIndirectArgument;
    candidate::ZeroWorkPolicyV1 zeroWorkPolicy =
        candidate::ZeroWorkPolicyV1::ZeroDispatchGroups;

    std::uint32_t maxWorkCount = 0;
    std::uint32_t threadsPerGroup = 0;
    std::uint32_t workRecordStride = 0;
    std::uint32_t outputRecordStride = 0;
    std::uint32_t argumentStrideBytes = 0;
    std::uint32_t argumentOffsetBytes = 0;
    std::uint32_t maxCommandCount = 0;
    std::uint32_t producerDispatchX = 0;
    std::uint32_t producerDispatchY = 0;
    std::uint32_t producerDispatchZ = 0;

    base::Digest256 producerProgramTemplateIdentity{};
    base::Digest256 consumerProgramTemplateIdentity{};
    base::Digest256 artifactIdentity{};
    base::Digest256 operationIdentity{};
};

struct VerifiedActiveWorkExecutionPlanV1 final
{
    base::Digest256 semanticIdentity{};
    ActiveWorkVerificationContextV1 context{};
    VerifiedSingleIndirectOperationV1 operation{};
    base::Digest256 planIdentity{};
};

class VerifiedActiveWorkLoweringV1 final
{
public:
    VerifiedActiveWorkLoweringV1() = delete;
    VerifiedActiveWorkLoweringV1(
        const VerifiedActiveWorkLoweringV1&) = default;
    VerifiedActiveWorkLoweringV1& operator=(
        const VerifiedActiveWorkLoweringV1&) = default;

    [[nodiscard]] const VerifiedActiveWorkExecutionPlanV1&
    Plan() const noexcept
    {
        return plan_;
    }

    [[nodiscard]] const base::Digest256&
    CertificateIdentity() const noexcept
    {
        return certificateIdentity_;
    }

private:
    VerifiedActiveWorkLoweringV1(
        VerifiedActiveWorkExecutionPlanV1 plan,
        base::Digest256 certificateIdentity);

    VerifiedActiveWorkExecutionPlanV1 plan_;
    base::Digest256 certificateIdentity_{};

    friend base::Result<VerifiedActiveWorkLoweringV1, std::string>
    VerifyActiveWorkLoweringCandidateV1(
        const semantic::ActiveWorkSemanticV1&,
        const ActiveWorkVerificationContextV1&,
        const candidate::RawActiveWorkLoweringCandidateV1&);
};

[[nodiscard]] ActiveWorkVerificationContextV1
BuildCanonicalActiveWorkVerificationContextV1();

[[nodiscard]] base::Result<
    VerifiedActiveWorkLoweringV1,
    std::string>
VerifyActiveWorkLoweringCandidateV1(
    const semantic::ActiveWorkSemanticV1& semantic,
    const ActiveWorkVerificationContextV1& context,
    const candidate::RawActiveWorkLoweringCandidateV1& proposal);

[[nodiscard]] base::Result<void, std::string>
ValidateVerifiedActiveWorkLoweringContextV1(
    const VerifiedActiveWorkLoweringV1& verified,
    const semantic::ActiveWorkSemanticV1& semantic,
    const ActiveWorkVerificationContextV1& context);

[[nodiscard]] std::vector<std::byte>
SerializeVerifiedSingleIndirectOperationV1(
    const VerifiedSingleIndirectOperationV1& value);

[[nodiscard]] std::vector<std::byte>
SerializeVerifiedActiveWorkExecutionPlanV1(
    const VerifiedActiveWorkExecutionPlanV1& value);

[[nodiscard]] std::vector<std::byte>
SerializeVerifiedActiveWorkLoweringV1(
    const VerifiedActiveWorkLoweringV1& value);
}
