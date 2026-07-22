#pragma once

#include "../00_Foundation/Result.h"
#include "../00_Foundation/Sha256.h"
#include "../134_TemporalStateSemantic/TemporalStateSemantic.h"
#include "../135_Spiral5TemporalContracts/Spiral5TemporalContracts.h"

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
}
