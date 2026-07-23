#pragma once

#include "../00_Foundation/Result.h"
#include "../00_Foundation/Sha256.h"
#include "../172_SparseTemporalDeltaSemantic/SparseTemporalDeltaSemantic.h"
#include "../173_Spiral7DeltaContracts/Spiral7DeltaContracts.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace sge4_5::spiral7::execution
{
struct DeltaExecutionErrorV1 final
{
    std::string stage;
    std::string message;
    long nativeCode = 0;
};

struct CompactDeltaArchitectureCaseV1 final
{
    std::string id;
    semantic::SparseDeltaInvocationV1 invocation;
    contract::FrozenCompactDeltaArtifactV1 artifact;
    std::vector<std::byte> previousHistoryBytes;

    CompactDeltaArchitectureCaseV1(
        std::string idValue,
        semantic::SparseDeltaInvocationV1 invocationValue,
        contract::FrozenCompactDeltaArtifactV1 artifactValue,
        std::vector<std::byte> previousHistoryBytesValue)
        : id(std::move(idValue)),
          invocation(std::move(invocationValue)),
          artifact(std::move(artifactValue)),
          previousHistoryBytes(std::move(previousHistoryBytesValue)) {}
};

struct CompactDeltaCaseObservationV1 final
{
    std::string id;
    std::uint32_t invocationIndex = 0;
    std::uint32_t previousActiveCount = 0;
    std::uint32_t activeCount = 0;
    std::uint32_t updateCount = 0;
    std::uint32_t clearCount = 0;
    std::uint32_t retainCount = 0;
    std::uint32_t transitionCount = 0;
    std::uint32_t expectedDispatchGroupsX = 0;
    std::uint32_t observedDispatchGroupsX = 0;
    bool activeReferenceQualified = false;
    bool retainedHistoryByteStable = false;
    bool inactiveSentinelByteStable = false;
    bool updateAndClearQualified = false;
    bool activeHomogeneousWQualified = false;
    bool finiteQualified = false;
    double maxAbsoluteError = 0.0;
    base::Digest256 invocationIdentity{};
    base::Digest256 artifactIdentity{};
    base::Digest256 previousHistoryDigest{};
    base::Digest256 outputDigest{};
};

struct CompactDeltaArchitectureReportV1 final
{
    std::string adapterDescription;
    base::Digest256 semanticIdentity{};
    std::vector<CompactDeltaCaseObservationV1> cases;
};

[[nodiscard]] base::Result<CompactDeltaArchitectureReportV1, DeltaExecutionErrorV1>
ExecuteCompactDeltaArchitectureV1(
    const semantic::SparseTemporalDeltaSemanticV1& semanticValue,
    const std::vector<CompactDeltaArchitectureCaseV1>& cases);

[[nodiscard]] std::vector<std::byte> SerializeCompactDeltaArchitectureReportV1(
    const CompactDeltaArchitectureReportV1& report);
} // namespace sge4_5::spiral7::execution
