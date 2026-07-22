#pragma once

#include "../00_Foundation/Sha256.h"
#include "../134_TemporalStateSemantic/TemporalStateSemantic.h"
#include "../135_Spiral5TemporalContracts/Spiral5TemporalContracts.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace sge4_5::spiral5::candidate
{
enum class TemporalLoweringKindV1 : std::uint32_t
{
    GlobalMotorHistoryReuse = 2
};

enum class HistoryGenerationPolicyV1 : std::uint32_t
{
    SourceGenerationLastUpdateWins = 1
};

enum class HoldExecutionPolicyV1 : std::uint32_t
{
    ZeroHierarchyDispatchRetainHistory = 1
};

struct RawTemporalLoweringCandidateV1 final
{
    base::Digest256 semanticIdentity{};
    base::Digest256 scheduleIdentity{};
    base::Digest256 targetProfileIdentity{};
    base::Digest256 observationContractIdentity{};
    base::Digest256 historyResourceContractIdentity{};
    base::Digest256 retainedCompletionContractIdentity{};
    base::Digest256 deviceEpochPolicyIdentity{};
    base::Digest256 spiral4IndirectArtifactIdentity{};
    base::Digest256 argumentProducerProgramIdentity{};
    base::Digest256 hierarchyProgramIdentity{};
    base::Digest256 consumerProgramIdentity{};
    base::Digest256 proposedArtifactIdentity{};
    base::Digest256 proposedHistoryResourceIdentity{};
    base::Digest256 invocationAuthorityIdentity{};

    TemporalLoweringKindV1 kind = TemporalLoweringKindV1::GlobalMotorHistoryReuse;
    contract::TemporalExtensionStrategyV1 extensionStrategy =
        contract::TemporalExtensionStrategyV1::VersionedSidecarTemporalExtension;
    contract::TemporalHistoryRoleV1 historyRole =
        contract::TemporalHistoryRoleV1::GlobalMotorHistory;
    contract::RetainedHistoryCompletionV1 completionPolicy =
        contract::RetainedHistoryCompletionV1::ConsumerCompletionRetainsHistory;
    HistoryGenerationPolicyV1 generationPolicy =
        HistoryGenerationPolicyV1::SourceGenerationLastUpdateWins;
    HoldExecutionPolicyV1 holdPolicy =
        HoldExecutionPolicyV1::ZeroHierarchyDispatchRetainHistory;

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

    base::Digest256 rawCandidateIdentity{};
};

[[nodiscard]] base::Digest256 ProposedTemporalTargetProfileIdentityV1();
[[nodiscard]] base::Digest256 ProposedTemporalObservationContractIdentityV1();
[[nodiscard]] base::Digest256 ProposedGlobalMotorHistoryResourceContractIdentityV1();
[[nodiscard]] base::Digest256 ProposedRetainedHistoryCompletionContractIdentityV1();
[[nodiscard]] base::Digest256 ProposedTemporalDeviceEpochPolicyIdentityV1();
[[nodiscard]] base::Digest256 ProposedGlobalMotorHistoryResourceIdentityV1(
    const contract::FrozenGlobalMotorHistoryArtifactV1& artifact);
[[nodiscard]] base::Digest256 TemporalInvocationAuthorityIdentityV1(
    const contract::FrozenGlobalMotorHistoryArtifactV1& artifact,
    const semantic::UpdateScheduleV1& schedule);

[[nodiscard]] std::vector<std::byte> SerializeRawTemporalLoweringCandidateV1(
    const RawTemporalLoweringCandidateV1& value);
[[nodiscard]] base::Digest256 RawTemporalLoweringCandidateIdentityV1(
    const RawTemporalLoweringCandidateV1& value);
}
