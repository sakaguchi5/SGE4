#pragma once

#include "../00_Foundation/Result.h"
#include "../00_Foundation/Sha256.h"
#include "../134_TemporalStateSemantic/TemporalStateSemantic.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace sge4_5::spiral5::contract
{
enum class TemporalCandidateKindV1 : std::uint32_t
{
    GlobalMotorHistoryReuse = 2
};

enum class TemporalHistoryRoleV1 : std::uint32_t
{
    GlobalMotorHistory = 1
};

enum class RetainedHistoryCompletionV1 : std::uint32_t
{
    ConsumerCompletionRetainsHistory = 1
};

enum class TemporalExtensionStrategyV1 : std::uint32_t
{
    VersionedSidecarTemporalExtension = 1
};

struct TemporalContractErrorV1 final
{
    std::string stage;
    std::string message;
};

struct TemporalInvocationArtifactV1 final
{
    std::uint32_t invocationIndex = 0;
    semantic::UpdateEventV1 event = semantic::UpdateEventV1::Hold;
    std::uint32_t sourceGeneration = 0;
    std::uint32_t hierarchyDispatchX = 0;
    std::uint32_t consumerDispatchX = 0;
    std::uint32_t historyReadGeneration = semantic::InvalidGenerationV1;
    std::uint32_t historyWriteGeneration = semantic::InvalidGenerationV1;
    std::uint64_t retainedCompletionOrdinal = 0;
};

class FrozenGlobalMotorHistoryArtifactV1 final
{
public:
    FrozenGlobalMotorHistoryArtifactV1() = delete;
    FrozenGlobalMotorHistoryArtifactV1(const FrozenGlobalMotorHistoryArtifactV1&) = default;
    FrozenGlobalMotorHistoryArtifactV1& operator=(const FrozenGlobalMotorHistoryArtifactV1&) = default;

    [[nodiscard]] TemporalExtensionStrategyV1 ExtensionStrategy() const noexcept { return extensionStrategy_; }
    [[nodiscard]] TemporalCandidateKindV1 CandidateKind() const noexcept { return candidateKind_; }
    [[nodiscard]] TemporalHistoryRoleV1 HistoryRole() const noexcept { return historyRole_; }
    [[nodiscard]] RetainedHistoryCompletionV1 CompletionPolicy() const noexcept { return completionPolicy_; }
    [[nodiscard]] std::uint32_t TimelineLength() const noexcept { return timelineLength_; }
    [[nodiscard]] std::uint32_t WorkCount() const noexcept { return workCount_; }
    [[nodiscard]] std::uint32_t ThreadsPerGroup() const noexcept { return threadsPerGroup_; }
    [[nodiscard]] std::uint32_t HistoryDepth() const noexcept { return historyDepth_; }
    [[nodiscard]] std::uint32_t HistoryBytes() const noexcept { return historyBytes_; }
    [[nodiscard]] std::uint32_t LocalMotorBytesPerGeneration() const noexcept { return localMotorBytesPerGeneration_; }
    [[nodiscard]] std::uint32_t ArgumentStrideBytes() const noexcept { return argumentStrideBytes_; }
    [[nodiscard]] std::uint32_t MaxCommandCount() const noexcept { return maxCommandCount_; }
    [[nodiscard]] std::uint32_t ArgumentProducerDispatchX() const noexcept { return argumentProducerDispatchX_; }
    [[nodiscard]] std::uint32_t HierarchyUpdateDispatchX() const noexcept { return hierarchyUpdateDispatchX_; }
    [[nodiscard]] std::uint32_t HierarchyHoldDispatchX() const noexcept { return hierarchyHoldDispatchX_; }
    [[nodiscard]] std::uint32_t ConsumerDispatchX() const noexcept { return consumerDispatchX_; }
    [[nodiscard]] const base::Digest256& SemanticIdentity() const noexcept { return semanticIdentity_; }
    [[nodiscard]] const base::Digest256& ScheduleIdentity() const noexcept { return scheduleIdentity_; }
    [[nodiscard]] const base::Digest256& Spiral4IndirectArtifactIdentity() const noexcept { return spiral4IndirectArtifactIdentity_; }
    [[nodiscard]] const base::Digest256& ArgumentProducerProgramIdentity() const noexcept { return argumentProducerProgramIdentity_; }
    [[nodiscard]] const base::Digest256& HierarchyProgramIdentity() const noexcept { return hierarchyProgramIdentity_; }
    [[nodiscard]] const base::Digest256& ConsumerProgramIdentity() const noexcept { return consumerProgramIdentity_; }
    [[nodiscard]] const base::Digest256& ArtifactIdentity() const noexcept { return artifactIdentity_; }
    [[nodiscard]] const std::vector<std::byte>& Bytes() const noexcept { return bytes_; }

private:
    friend FrozenGlobalMotorHistoryArtifactV1 BuildCu2GlobalMotorHistoryArtifactV1(
        const semantic::TemporalStateSemanticV1&,
        const semantic::UpdateScheduleV1&);
    friend base::Result<FrozenGlobalMotorHistoryArtifactV1, TemporalContractErrorV1>
        ParseGlobalMotorHistoryArtifactV1(std::span<const std::byte>);

    FrozenGlobalMotorHistoryArtifactV1(
        TemporalExtensionStrategyV1 extensionStrategy,
        TemporalCandidateKindV1 candidateKind,
        TemporalHistoryRoleV1 historyRole,
        RetainedHistoryCompletionV1 completionPolicy,
        std::uint32_t timelineLength,
        std::uint32_t workCount,
        std::uint32_t threadsPerGroup,
        std::uint32_t historyDepth,
        std::uint32_t historyBytes,
        std::uint32_t localMotorBytesPerGeneration,
        std::uint32_t argumentStrideBytes,
        std::uint32_t maxCommandCount,
        std::uint32_t argumentProducerDispatchX,
        std::uint32_t hierarchyUpdateDispatchX,
        std::uint32_t hierarchyHoldDispatchX,
        std::uint32_t consumerDispatchX,
        base::Digest256 semanticIdentity,
        base::Digest256 scheduleIdentity,
        base::Digest256 spiral4IndirectArtifactIdentity,
        base::Digest256 argumentProducerProgramIdentity,
        base::Digest256 hierarchyProgramIdentity,
        base::Digest256 consumerProgramIdentity,
        base::Digest256 artifactIdentity,
        std::vector<std::byte> bytes);

    TemporalExtensionStrategyV1 extensionStrategy_{};
    TemporalCandidateKindV1 candidateKind_{};
    TemporalHistoryRoleV1 historyRole_{};
    RetainedHistoryCompletionV1 completionPolicy_{};
    std::uint32_t timelineLength_ = 0;
    std::uint32_t workCount_ = 0;
    std::uint32_t threadsPerGroup_ = 0;
    std::uint32_t historyDepth_ = 0;
    std::uint32_t historyBytes_ = 0;
    std::uint32_t localMotorBytesPerGeneration_ = 0;
    std::uint32_t argumentStrideBytes_ = 0;
    std::uint32_t maxCommandCount_ = 0;
    std::uint32_t argumentProducerDispatchX_ = 0;
    std::uint32_t hierarchyUpdateDispatchX_ = 0;
    std::uint32_t hierarchyHoldDispatchX_ = 0;
    std::uint32_t consumerDispatchX_ = 0;
    base::Digest256 semanticIdentity_{};
    base::Digest256 scheduleIdentity_{};
    base::Digest256 spiral4IndirectArtifactIdentity_{};
    base::Digest256 argumentProducerProgramIdentity_{};
    base::Digest256 hierarchyProgramIdentity_{};
    base::Digest256 consumerProgramIdentity_{};
    base::Digest256 artifactIdentity_{};
    std::vector<std::byte> bytes_;
};

[[nodiscard]] FrozenGlobalMotorHistoryArtifactV1 BuildCu2GlobalMotorHistoryArtifactV1(
    const semantic::TemporalStateSemanticV1& semantic,
    const semantic::UpdateScheduleV1& schedule);
[[nodiscard]] base::Result<FrozenGlobalMotorHistoryArtifactV1, TemporalContractErrorV1>
ParseGlobalMotorHistoryArtifactV1(std::span<const std::byte> bytes);
[[nodiscard]] base::Result<void, TemporalContractErrorV1>
ValidateGlobalMotorHistoryArtifactForExecutionV1(
    const FrozenGlobalMotorHistoryArtifactV1& artifact,
    const semantic::TemporalStateSemanticV1& semantic,
    const semantic::UpdateScheduleV1& schedule);
[[nodiscard]] std::vector<TemporalInvocationArtifactV1>
BuildTemporalInvocationArtifactsV1(
    const FrozenGlobalMotorHistoryArtifactV1& artifact,
    const semantic::UpdateScheduleV1& schedule);
[[nodiscard]] std::vector<std::byte> SerializeTemporalInvocationArtifactsV1(
    std::span<const TemporalInvocationArtifactV1> invocations);
}
