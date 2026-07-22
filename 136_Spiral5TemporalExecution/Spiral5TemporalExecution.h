#pragma once

#include "../00_Foundation/Result.h"
#include "../00_Foundation/Sha256.h"
#include "../134_TemporalStateSemantic/TemporalStateSemantic.h"
#include "../135_Spiral5TemporalContracts/Spiral5TemporalContracts.h"
#include "../139_TemporalLoweringVerifier/TemporalLoweringVerifier.h"
#include "../149_TemporalCandidateFamilyVerifier/TemporalCandidateFamilyVerifier.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace sge4_5::spiral5::execution
{
struct TemporalExecutionErrorV1 final
{
    std::string stage;
    std::string message;
    long hresult = 0;
};

struct TemporalScheduleExecutionInputV1 final
{
    semantic::UpdateScheduleV1 schedule;
    contract::FrozenGlobalMotorHistoryArtifactV1 artifact;
};

struct TemporalInvocationObservationV1 final
{
    std::uint32_t invocationIndex = 0;
    semantic::UpdateEventV1 event = semantic::UpdateEventV1::Hold;
    std::uint32_t sourceGeneration = 0;
    std::uint32_t expectedHierarchyDispatchX = 0;
    std::uint32_t observedHierarchyDispatchX = 0;
    std::uint32_t consumerDispatchX = 0;
    std::uint32_t historyReadGeneration = semantic::InvalidGenerationV1;
    std::uint32_t historyWriteGeneration = semantic::InvalidGenerationV1;
    std::uint64_t retainedCompletionOrdinal = 0;
    std::uint64_t nativeFenceValue = 0;
    bool historyValid = false;
    bool holdHistoryByteStable = false;
    bool outputMatchesReference = false;
    bool outputMatchesLatestUpdate = false;
    std::uint32_t mismatchCount = 0;
    std::uint32_t nonFiniteCount = 0;
    std::uint32_t homogeneousCoordinateMismatchCount = 0;
    float maxAbsoluteError = 0.0f;
    base::Digest256 historyDigest{};
    base::Digest256 outputDigest{};
};

struct TemporalScheduleObservationV1 final
{
    std::string scheduleId;
    base::Digest256 scheduleIdentity{};
    base::Digest256 artifactIdentity{};
    std::vector<TemporalInvocationObservationV1> invocations;
};

struct GlobalMotorHistoryArchitectureResultV1 final
{
    std::string adapterDescription;
    base::Digest256 semanticIdentity{};
    base::Digest256 argumentProducerShaderDigest{};
    base::Digest256 hierarchyShaderDigest{};
    base::Digest256 consumerShaderDigest{};
    std::vector<TemporalScheduleObservationV1> schedules;
};

[[nodiscard]] base::Result<GlobalMotorHistoryArchitectureResultV1, TemporalExecutionErrorV1>
RunGlobalMotorHistoryArchitecturesOnWarpV1(
    const semantic::TemporalStateSemanticV1& semantic,
    std::span<const TemporalScheduleExecutionInputV1> inputs);

[[nodiscard]] std::vector<std::byte> SerializeGlobalMotorHistoryArchitectureEvidenceV1(
    const GlobalMotorHistoryArchitectureResultV1& result);

struct TemporalHistoryResourceBindingInputV1 final
{
    std::uint64_t deviceEpoch = 0;
    base::Digest256 actualHistoryResourceIdentity{};
    contract::TemporalHistoryRoleV1 historyRole =
        contract::TemporalHistoryRoleV1::GlobalMotorHistory;
    std::uint32_t historyDepth = 0;
    std::uint32_t historyBytes = 0;
};

class FrozenVerifiedGlobalMotorHistoryExecutionV1 final
{
public:
    FrozenVerifiedGlobalMotorHistoryExecutionV1() = delete;
    FrozenVerifiedGlobalMotorHistoryExecutionV1(
        const FrozenVerifiedGlobalMotorHistoryExecutionV1&) = default;
    FrozenVerifiedGlobalMotorHistoryExecutionV1& operator=(
        const FrozenVerifiedGlobalMotorHistoryExecutionV1&) = default;

    [[nodiscard]] const verification::VerifiedTemporalLoweringV1& Verified() const noexcept
    {
        return verified_;
    }
    [[nodiscard]] const contract::FrozenGlobalMotorHistoryArtifactV1& Artifact() const noexcept
    {
        return artifact_;
    }
    [[nodiscard]] const TemporalHistoryResourceBindingInputV1& ResourceBinding() const noexcept
    {
        return resourceBinding_;
    }
    [[nodiscard]] const base::Digest256& BindingIdentity() const noexcept
    {
        return bindingIdentity_;
    }

private:
    FrozenVerifiedGlobalMotorHistoryExecutionV1(
        verification::VerifiedTemporalLoweringV1 verified,
        contract::FrozenGlobalMotorHistoryArtifactV1 artifact,
        TemporalHistoryResourceBindingInputV1 resourceBinding,
        base::Digest256 bindingIdentity);

    verification::VerifiedTemporalLoweringV1 verified_;
    contract::FrozenGlobalMotorHistoryArtifactV1 artifact_;
    TemporalHistoryResourceBindingInputV1 resourceBinding_{};
    base::Digest256 bindingIdentity_{};

    friend base::Result<FrozenVerifiedGlobalMotorHistoryExecutionV1, std::string>
    FreezeVerifiedGlobalMotorHistoryExecutionV1(
        const semantic::TemporalStateSemanticV1&,
        const semantic::UpdateScheduleV1&,
        const verification::TemporalVerificationContextV1&,
        const verification::VerifiedTemporalLoweringV1&,
        TemporalHistoryResourceBindingInputV1);
};

struct VerifiedGlobalMotorHistoryRunResultV1 final
{
    GlobalMotorHistoryArchitectureResultV1 architecture;
    base::Digest256 verifiedBindingIdentity{};
    base::Digest256 actualExecutionIdentity{};
};

[[nodiscard]] TemporalHistoryResourceBindingInputV1
BuildCanonicalGlobalMotorHistoryResourceBindingInputV1(
    const contract::FrozenGlobalMotorHistoryArtifactV1& artifact,
    std::uint64_t deviceEpoch);

[[nodiscard]] base::Result<FrozenVerifiedGlobalMotorHistoryExecutionV1, std::string>
FreezeVerifiedGlobalMotorHistoryExecutionV1(
    const semantic::TemporalStateSemanticV1& semantic,
    const semantic::UpdateScheduleV1& schedule,
    const verification::TemporalVerificationContextV1& context,
    const verification::VerifiedTemporalLoweringV1& verified,
    TemporalHistoryResourceBindingInputV1 resourceBinding);

[[nodiscard]] base::Result<void, std::string>
ValidateFrozenVerifiedGlobalMotorHistoryExecutionV1(
    const FrozenVerifiedGlobalMotorHistoryExecutionV1& frozen,
    const semantic::TemporalStateSemanticV1& semantic,
    const semantic::UpdateScheduleV1& schedule,
    const verification::TemporalVerificationContextV1& context,
    std::uint64_t expectedDeviceEpoch);

[[nodiscard]] base::Result<VerifiedGlobalMotorHistoryRunResultV1, TemporalExecutionErrorV1>
RunVerifiedGlobalMotorHistoryOnWarpV1(
    const FrozenVerifiedGlobalMotorHistoryExecutionV1& frozen,
    const semantic::TemporalStateSemanticV1& semantic,
    const semantic::UpdateScheduleV1& schedule,
    const verification::TemporalVerificationContextV1& context,
    std::uint64_t expectedDeviceEpoch);

[[nodiscard]] std::vector<std::byte>
SerializeFrozenVerifiedGlobalMotorHistoryExecutionV1(
    const FrozenVerifiedGlobalMotorHistoryExecutionV1& value);


struct TemporalCandidateResourceBindingInputV1 final
{
    std::uint64_t deviceEpoch = 0;
    base::Digest256 actualHistoryResourceIdentity{};
    family_candidate::TemporalFamilyHistoryRoleV1 historyRole =
        family_candidate::TemporalFamilyHistoryRoleV1::None;
    std::uint32_t historyDepth = 0;
    std::uint32_t historyBytes = 0;
};

class FrozenVerifiedTemporalCandidateV1 final
{
public:
    FrozenVerifiedTemporalCandidateV1() = delete;
    FrozenVerifiedTemporalCandidateV1(const FrozenVerifiedTemporalCandidateV1&) = default;
    FrozenVerifiedTemporalCandidateV1& operator=(const FrozenVerifiedTemporalCandidateV1&) = default;
    [[nodiscard]] const family_verification::VerifiedTemporalCandidateV1& Verified() const noexcept { return verified_; }
    [[nodiscard]] const TemporalCandidateResourceBindingInputV1& ResourceBinding() const noexcept { return resourceBinding_; }
    [[nodiscard]] const base::Digest256& BindingIdentity() const noexcept { return bindingIdentity_; }
private:
    FrozenVerifiedTemporalCandidateV1(
        family_verification::VerifiedTemporalCandidateV1 verified,
        TemporalCandidateResourceBindingInputV1 resourceBinding,
        base::Digest256 bindingIdentity);
    family_verification::VerifiedTemporalCandidateV1 verified_;
    TemporalCandidateResourceBindingInputV1 resourceBinding_{};
    base::Digest256 bindingIdentity_{};
    friend base::Result<FrozenVerifiedTemporalCandidateV1, std::string>
    FreezeVerifiedTemporalCandidateV1(
        const semantic::TemporalStateSemanticV1&,
        const semantic::UpdateScheduleV1&,
        const family_verification::TemporalCandidateFamilyVerificationContextV1&,
        const family_verification::VerifiedTemporalCandidateV1&,
        TemporalCandidateResourceBindingInputV1);
};

struct TemporalFamilyInvocationObservationV1 final
{
    std::uint32_t invocationIndex = 0;
    semantic::UpdateEventV1 event = semantic::UpdateEventV1::Hold;
    std::uint32_t sourceGeneration = 0;
    std::uint32_t expectedHierarchyDispatchX = 0;
    std::uint32_t observedHierarchyDispatchX = 0;
    std::uint32_t expectedConsumerDispatchX = 0;
    std::uint32_t observedConsumerDispatchX = 0;
    std::uint64_t retainedCompletionOrdinal = 0;
    std::uint64_t nativeFenceValue = 0;
    bool retainedHistoryByteStable = false;
    bool outputMatchesReference = false;
    bool outputMatchesLatestUpdate = false;
    std::uint32_t mismatchCount = 0;
    std::uint32_t nonFiniteCount = 0;
    std::uint32_t homogeneousCoordinateMismatchCount = 0;
    float maxAbsoluteError = 0.0f;
    base::Digest256 retainedHistoryDigest{};
    base::Digest256 outputDigest{};
};

struct TemporalCandidateObservationV1 final
{
    family_candidate::TemporalCandidateKindV1 kind =
        family_candidate::TemporalCandidateKindV1::EveryInvocationRecompute;
    base::Digest256 verifiedBindingIdentity{};
    base::Digest256 actualExecutionIdentity{};
    std::vector<TemporalFamilyInvocationObservationV1> invocations;
};

struct TemporalCandidateFamilyArchitectureResultV1 final
{
    std::string adapterDescription;
    std::string scheduleId;
    base::Digest256 semanticIdentity{};
    base::Digest256 scheduleIdentity{};
    base::Digest256 argumentProducerShaderDigest{};
    base::Digest256 hierarchyShaderDigest{};
    base::Digest256 consumerShaderDigest{};
    bool pairwiseOutputEquivalent = false;
    std::vector<TemporalCandidateObservationV1> candidates;
};

[[nodiscard]] TemporalCandidateResourceBindingInputV1
BuildCanonicalTemporalCandidateResourceBindingInputV1(
    const family_verification::VerifiedTemporalCandidateV1& verified,
    std::uint64_t deviceEpoch);
[[nodiscard]] base::Result<FrozenVerifiedTemporalCandidateV1, std::string>
FreezeVerifiedTemporalCandidateV1(
    const semantic::TemporalStateSemanticV1& semantic,
    const semantic::UpdateScheduleV1& schedule,
    const family_verification::TemporalCandidateFamilyVerificationContextV1& context,
    const family_verification::VerifiedTemporalCandidateV1& verified,
    TemporalCandidateResourceBindingInputV1 binding);
[[nodiscard]] base::Result<void, std::string>
ValidateFrozenVerifiedTemporalCandidateV1(
    const FrozenVerifiedTemporalCandidateV1& frozen,
    const semantic::TemporalStateSemanticV1& semantic,
    const semantic::UpdateScheduleV1& schedule,
    const family_verification::TemporalCandidateFamilyVerificationContextV1& context,
    std::uint64_t expectedDeviceEpoch);
[[nodiscard]] base::Result<TemporalCandidateFamilyArchitectureResultV1, TemporalExecutionErrorV1>
RunVerifiedTemporalCandidateFamilyOnWarpV1(
    const semantic::TemporalStateSemanticV1& semantic,
    const semantic::UpdateScheduleV1& schedule,
    const family_verification::TemporalCandidateFamilyVerificationContextV1& context,
    std::span<const FrozenVerifiedTemporalCandidateV1> candidates,
    std::uint64_t expectedDeviceEpoch);
[[nodiscard]] std::vector<std::byte>
SerializeFrozenVerifiedTemporalCandidateV1(const FrozenVerifiedTemporalCandidateV1& value);
[[nodiscard]] std::vector<std::byte>
SerializeTemporalCandidateFamilyArchitectureEvidenceV1(
    const TemporalCandidateFamilyArchitectureResultV1& value);

}
