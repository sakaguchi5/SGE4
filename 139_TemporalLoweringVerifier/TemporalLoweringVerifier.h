#pragma once

#include "../00_Foundation/Result.h"
#include "../00_Foundation/Sha256.h"
#include "../134_TemporalStateSemantic/TemporalStateSemantic.h"
#include "../137_TemporalLoweringCandidate/TemporalLoweringCandidate.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace sge4_5::spiral5::verification
{
struct TemporalVerificationContextV1 final
{
    base::Digest256 targetProfileIdentity{};
    base::Digest256 observationContractIdentity{};
    base::Digest256 historyResourceContractIdentity{};
    base::Digest256 retainedCompletionContractIdentity{};
    base::Digest256 deviceEpochPolicyIdentity{};

    bool operator==(const TemporalVerificationContextV1&) const = default;
};

struct VerifiedGlobalMotorHistoryOperationV1 final
{
    candidate::TemporalLoweringKindV1 kind =
        candidate::TemporalLoweringKindV1::GlobalMotorHistoryReuse;
    contract::TemporalExtensionStrategyV1 extensionStrategy =
        contract::TemporalExtensionStrategyV1::VersionedSidecarTemporalExtension;
    contract::TemporalHistoryRoleV1 historyRole =
        contract::TemporalHistoryRoleV1::GlobalMotorHistory;
    contract::RetainedHistoryCompletionV1 completionPolicy =
        contract::RetainedHistoryCompletionV1::ConsumerCompletionRetainsHistory;
    candidate::HistoryGenerationPolicyV1 generationPolicy =
        candidate::HistoryGenerationPolicyV1::SourceGenerationLastUpdateWins;
    candidate::HoldExecutionPolicyV1 holdPolicy =
        candidate::HoldExecutionPolicyV1::ZeroHierarchyDispatchRetainHistory;

    std::uint32_t timelineLength = 0;
    std::uint32_t workCount = 0;
    std::uint32_t threadsPerGroup = 0;
    std::uint32_t historyDepth = 0;
    std::uint32_t historyBytes = 0;
    std::uint32_t localMotorBytesPerGeneration = 0;
    std::uint32_t argumentStrideBytes = 0;
    std::uint32_t maxCommandCount = 0;
    std::uint32_t argumentProducerDispatchX = 0;
    std::uint32_t hierarchyUpdateDispatchX = 0;
    std::uint32_t hierarchyHoldDispatchX = 0;
    std::uint32_t consumerDispatchX = 0;
    std::uint32_t updateCount = 0;
    std::uint32_t holdCount = 0;
    std::uint32_t maximumSourceGeneration = 0;

    base::Digest256 spiral4IndirectArtifactIdentity{};
    base::Digest256 argumentProducerProgramIdentity{};
    base::Digest256 hierarchyProgramIdentity{};
    base::Digest256 consumerProgramIdentity{};
    base::Digest256 artifactIdentity{};
    base::Digest256 historyResourceIdentity{};
    base::Digest256 invocationAuthorityIdentity{};
    base::Digest256 operationIdentity{};
};

struct VerifiedTemporalExecutionPlanV1 final
{
    base::Digest256 semanticIdentity{};
    base::Digest256 scheduleIdentity{};
    TemporalVerificationContextV1 context{};
    VerifiedGlobalMotorHistoryOperationV1 operation{};
    base::Digest256 planIdentity{};
};

class VerifiedTemporalLoweringV1 final
{
public:
    VerifiedTemporalLoweringV1() = delete;
    VerifiedTemporalLoweringV1(const VerifiedTemporalLoweringV1&) = default;
    VerifiedTemporalLoweringV1& operator=(const VerifiedTemporalLoweringV1&) = default;

    [[nodiscard]] const VerifiedTemporalExecutionPlanV1& Plan() const noexcept
    {
        return plan_;
    }
    [[nodiscard]] const base::Digest256& CertificateIdentity() const noexcept
    {
        return certificateIdentity_;
    }

private:
    VerifiedTemporalLoweringV1(
        VerifiedTemporalExecutionPlanV1 plan,
        base::Digest256 certificateIdentity);

    VerifiedTemporalExecutionPlanV1 plan_;
    base::Digest256 certificateIdentity_{};

    friend base::Result<VerifiedTemporalLoweringV1, std::string>
    VerifyTemporalLoweringCandidateV1(
        const semantic::TemporalStateSemanticV1&,
        const semantic::UpdateScheduleV1&,
        const TemporalVerificationContextV1&,
        const candidate::RawTemporalLoweringCandidateV1&);
};

[[nodiscard]] TemporalVerificationContextV1
BuildCanonicalTemporalVerificationContextV1();

[[nodiscard]] base::Result<VerifiedTemporalLoweringV1, std::string>
VerifyTemporalLoweringCandidateV1(
    const semantic::TemporalStateSemanticV1& semantic,
    const semantic::UpdateScheduleV1& schedule,
    const TemporalVerificationContextV1& context,
    const candidate::RawTemporalLoweringCandidateV1& proposal);

[[nodiscard]] base::Result<void, std::string>
ValidateVerifiedTemporalLoweringContextV1(
    const VerifiedTemporalLoweringV1& verified,
    const semantic::TemporalStateSemanticV1& semantic,
    const semantic::UpdateScheduleV1& schedule,
    const TemporalVerificationContextV1& context);

[[nodiscard]] std::vector<std::byte>
SerializeVerifiedGlobalMotorHistoryOperationV1(
    const VerifiedGlobalMotorHistoryOperationV1& value);
[[nodiscard]] std::vector<std::byte>
SerializeVerifiedTemporalExecutionPlanV1(
    const VerifiedTemporalExecutionPlanV1& value);
[[nodiscard]] std::vector<std::byte>
SerializeVerifiedTemporalLoweringV1(
    const VerifiedTemporalLoweringV1& value);
}
