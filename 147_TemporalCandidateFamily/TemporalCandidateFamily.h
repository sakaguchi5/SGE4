#pragma once

#include "../00_Foundation/Sha256.h"
#include "../134_TemporalStateSemantic/TemporalStateSemantic.h"
#include "../135_Spiral5TemporalContracts/Spiral5TemporalContracts.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace sge4_5::spiral5::family_candidate
{
enum class TemporalCandidateKindV1 : std::uint32_t
{
    EveryInvocationRecompute = 1,
    GlobalMotorHistoryReuse = 2,
    FinalOutputHistoryReuse = 3
};

enum class TemporalFamilyHistoryRoleV1 : std::uint32_t
{
    None = 0,
    GlobalMotorHistory = 1,
    FinalOutputHistory = 2
};

enum class TemporalIssuancePolicyV1 : std::uint32_t
{
    DirectEveryInvocation = 1,
    UpdateIndirectHierarchy = 2,
    UpdateIndirectHierarchyAndConsumer = 3
};

enum class TemporalCompletionPolicyV1 : std::uint32_t
{
    EveryInvocationOutputCompletion = 1,
    ConsumerCompletionRetainsGlobalMotor = 2,
    OutputCompletionRetainsFinalOutput = 3
};

enum class TemporalHoldPolicyV1 : std::uint32_t
{
    RecomputeUsingLatestSource = 1,
    ZeroHierarchyReuseGlobalMotor = 2,
    ZeroHierarchyAndConsumerRetainOutput = 3
};

struct RawTemporalCandidateV1 final
{
    base::Digest256 semanticIdentity{};
    base::Digest256 scheduleIdentity{};
    base::Digest256 targetProfileIdentity{};
    base::Digest256 observationContractIdentity{};
    base::Digest256 completionContractIdentity{};
    base::Digest256 deviceEpochPolicyIdentity{};
    base::Digest256 spiral4IndirectArtifactIdentity{};
    base::Digest256 argumentProducerProgramIdentity{};
    base::Digest256 hierarchyProgramIdentity{};
    base::Digest256 consumerProgramIdentity{};
    base::Digest256 proposedArtifactIdentity{};
    base::Digest256 proposedHistoryResourceIdentity{};
    base::Digest256 invocationAuthorityIdentity{};

    TemporalCandidateKindV1 kind = TemporalCandidateKindV1::EveryInvocationRecompute;
    TemporalFamilyHistoryRoleV1 historyRole = TemporalFamilyHistoryRoleV1::None;
    TemporalIssuancePolicyV1 issuancePolicy = TemporalIssuancePolicyV1::DirectEveryInvocation;
    TemporalCompletionPolicyV1 completionPolicy = TemporalCompletionPolicyV1::EveryInvocationOutputCompletion;
    TemporalHoldPolicyV1 holdPolicy = TemporalHoldPolicyV1::RecomputeUsingLatestSource;

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

    base::Digest256 rawCandidateIdentity{};
};

[[nodiscard]] base::Digest256 TemporalFamilyTargetProfileIdentityV1();
[[nodiscard]] base::Digest256 TemporalFamilyObservationContractIdentityV1();
[[nodiscard]] base::Digest256 TemporalFamilyDeviceEpochPolicyIdentityV1();
[[nodiscard]] base::Digest256 TemporalFamilyCompletionContractIdentityV1(TemporalCandidateKindV1 kind);
[[nodiscard]] base::Digest256 TemporalFamilyArgumentProducerProgramIdentityV1();
[[nodiscard]] base::Digest256 TemporalFamilyHierarchyProgramIdentityV1();
[[nodiscard]] base::Digest256 TemporalFamilyConsumerProgramIdentityV1();
[[nodiscard]] base::Digest256 TemporalFamilyArtifactIdentityV1(
    const semantic::TemporalStateSemanticV1& semantic,
    const semantic::UpdateScheduleV1& schedule,
    TemporalCandidateKindV1 kind);
[[nodiscard]] base::Digest256 TemporalFamilyHistoryResourceIdentityV1(
    const semantic::TemporalStateSemanticV1& semantic,
    const semantic::UpdateScheduleV1& schedule,
    TemporalCandidateKindV1 kind);
[[nodiscard]] base::Digest256 TemporalFamilyInvocationAuthorityIdentityV1(
    const semantic::TemporalStateSemanticV1& semantic,
    const semantic::UpdateScheduleV1& schedule,
    TemporalCandidateKindV1 kind);

[[nodiscard]] std::vector<std::byte> SerializeRawTemporalCandidateV1(
    const RawTemporalCandidateV1& value);
[[nodiscard]] base::Digest256 RawTemporalCandidateIdentityV1(
    const RawTemporalCandidateV1& value);
}
