#pragma once

#include "../00_Foundation/Result.h"
#include "../00_Foundation/Sha256.h"
#include "../172_SparseTemporalDeltaSemantic/SparseTemporalDeltaSemantic.h"
#include "../173_Spiral7DeltaContracts/Spiral7DeltaContracts.h"
#include "../177_SparseTemporalDeltaLoweringVerifier/DeltaLoweringVerifier.h"

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


enum class DeltaBindingRoleV1 : std::uint32_t
{
    CanonicalSortedIndexActionList = 1,
    PreviousHistoryReadOnly = 2,
    OutputHistoryWriteOnly = 3
};

struct DeltaResourceBindingInputV1 final
{
    std::uint64_t deviceEpoch = 0;
    base::Digest256 actualDeltaResourceIdentity{};
    base::Digest256 actualPreviousHistoryResourceIdentity{};
    base::Digest256 actualOutputHistoryResourceIdentity{};
    DeltaBindingRoleV1 deltaRole = DeltaBindingRoleV1::CanonicalSortedIndexActionList;
    DeltaBindingRoleV1 previousHistoryRole = DeltaBindingRoleV1::PreviousHistoryReadOnly;
    DeltaBindingRoleV1 outputHistoryRole = DeltaBindingRoleV1::OutputHistoryWriteOnly;
    std::uint32_t deltaFixedCapacityBytes = 0;
    std::uint32_t deltaRecordStrideBytes = 0;
    std::uint32_t previousHistoryBytes = 0;
    std::uint32_t outputHistoryBytes = 0;
};

class FrozenVerifiedCompactDeltaExecutionV1 final
{
public:
    FrozenVerifiedCompactDeltaExecutionV1() = delete;
    FrozenVerifiedCompactDeltaExecutionV1(
        const FrozenVerifiedCompactDeltaExecutionV1&) = default;
    FrozenVerifiedCompactDeltaExecutionV1& operator=(
        const FrozenVerifiedCompactDeltaExecutionV1&) = default;

    [[nodiscard]] const verification::VerifiedDeltaLoweringV1& Verified() const noexcept
    {
        return verified_;
    }
    [[nodiscard]] const contract::FrozenCompactDeltaArtifactV1& Artifact() const noexcept
    {
        return artifact_;
    }
    [[nodiscard]] const DeltaResourceBindingInputV1& ResourceBinding() const noexcept
    {
        return binding_;
    }
    [[nodiscard]] const base::Digest256& BindingIdentity() const noexcept
    {
        return bindingIdentity_;
    }

private:
    FrozenVerifiedCompactDeltaExecutionV1(
        verification::VerifiedDeltaLoweringV1 verified,
        contract::FrozenCompactDeltaArtifactV1 artifact,
        DeltaResourceBindingInputV1 binding,
        base::Digest256 bindingIdentity);

    verification::VerifiedDeltaLoweringV1 verified_;
    contract::FrozenCompactDeltaArtifactV1 artifact_;
    DeltaResourceBindingInputV1 binding_{};
    base::Digest256 bindingIdentity_{};

    friend base::Result<FrozenVerifiedCompactDeltaExecutionV1, std::string>
    FreezeVerifiedCompactDeltaExecutionV1(
        const verification::VerifiedDeltaLoweringV1&,
        const semantic::SparseTemporalDeltaSemanticV1&,
        const semantic::SparseDeltaInvocationV1&,
        const contract::FrozenCompactDeltaArtifactV1&,
        const DeltaResourceBindingInputV1&);
};

[[nodiscard]] base::Result<FrozenVerifiedCompactDeltaExecutionV1, std::string>
FreezeVerifiedCompactDeltaExecutionV1(
    const verification::VerifiedDeltaLoweringV1& verified,
    const semantic::SparseTemporalDeltaSemanticV1& semantic,
    const semantic::SparseDeltaInvocationV1& invocation,
    const contract::FrozenCompactDeltaArtifactV1& artifact,
    const DeltaResourceBindingInputV1& binding);

[[nodiscard]] base::Result<void, std::string>
ValidateFrozenCompactDeltaExecutionEpochV1(
    const FrozenVerifiedCompactDeltaExecutionV1& frozen,
    std::uint64_t currentDeviceEpoch);

[[nodiscard]] base::Result<CompactDeltaArchitectureReportV1, DeltaExecutionErrorV1>
ExecuteVerifiedCompactDeltaHistoryV1(
    const semantic::SparseTemporalDeltaSemanticV1& semantic,
    const semantic::SparseDeltaInvocationV1& invocation,
    const FrozenVerifiedCompactDeltaExecutionV1& frozen,
    const std::vector<std::byte>& previousHistoryBytes,
    std::uint64_t currentDeviceEpoch);

[[nodiscard]] std::vector<std::byte> SerializeFrozenVerifiedCompactDeltaExecutionV1(
    const FrozenVerifiedCompactDeltaExecutionV1& value);
} // namespace sge4_5::spiral7::execution
