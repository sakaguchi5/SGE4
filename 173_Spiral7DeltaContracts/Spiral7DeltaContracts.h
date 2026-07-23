#pragma once

#include "../00_Foundation/Result.h"
#include "../00_Foundation/Sha256.h"
#include "../155_Spiral6SparseContracts/Spiral6SparseContracts.h"
#include "../172_SparseTemporalDeltaSemantic/SparseTemporalDeltaSemantic.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace sge4_5::spiral7::contract
{
enum class DeltaCandidateKindV1 : std::uint32_t
{
    CompactDeltaIndexHistoryReuse = 2
};

enum class DeltaRepresentationRoleV1 : std::uint32_t
{
    CanonicalSortedIndexActionList = 1
};

enum class DeltaOperationCodeV1 : std::uint32_t
{
    ExecuteCompactDeltaHistory = 0x0007'0201u
};

enum class DeltaOutputHistoryPolicyV1 : std::uint32_t
{
    CopyPreviousHistoryThenWriteExactlyTransitionSet = 1
};

struct DeltaContractErrorV1 final
{
    std::string stage;
    std::string message;
};

class FrozenCompactDeltaArtifactV1 final
{
public:
    FrozenCompactDeltaArtifactV1() = delete;
    FrozenCompactDeltaArtifactV1(const FrozenCompactDeltaArtifactV1&) = default;
    FrozenCompactDeltaArtifactV1& operator=(const FrozenCompactDeltaArtifactV1&) = default;

    [[nodiscard]] DeltaCandidateKindV1 CandidateKind() const noexcept { return candidateKind_; }
    [[nodiscard]] DeltaRepresentationRoleV1 RepresentationRole() const noexcept { return representationRole_; }
    [[nodiscard]] DeltaOperationCodeV1 OperationCode() const noexcept { return operationCode_; }
    [[nodiscard]] DeltaOutputHistoryPolicyV1 OutputHistoryPolicy() const noexcept { return outputHistoryPolicy_; }
    [[nodiscard]] std::uint32_t UniverseCount() const noexcept { return universeCount_; }
    [[nodiscard]] std::uint32_t TransitionCount() const noexcept { return transitionCount_; }
    [[nodiscard]] std::uint32_t UpdateCount() const noexcept { return updateCount_; }
    [[nodiscard]] std::uint32_t ClearCount() const noexcept { return clearCount_; }
    [[nodiscard]] std::uint32_t ThreadsPerGroup() const noexcept { return threadsPerGroup_; }
    [[nodiscard]] std::uint32_t DispatchGroupsX() const noexcept { return dispatchGroupsX_; }
    [[nodiscard]] std::uint32_t RecordStrideBytes() const noexcept { return recordStrideBytes_; }
    [[nodiscard]] std::uint32_t FixedCapacityBytes() const noexcept { return fixedCapacityBytes_; }
    [[nodiscard]] const base::Digest256& SemanticIdentity() const noexcept { return semanticIdentity_; }
    [[nodiscard]] const base::Digest256& InvocationIdentity() const noexcept { return invocationIdentity_; }
    [[nodiscard]] const base::Digest256& PreviousActiveSetIdentity() const noexcept { return previousActiveSetIdentity_; }
    [[nodiscard]] const base::Digest256& ActiveSetIdentity() const noexcept { return activeSetIdentity_; }
    [[nodiscard]] const base::Digest256& ModifiedSetIdentity() const noexcept { return modifiedSetIdentity_; }
    [[nodiscard]] const base::Digest256& TransitionIdentity() const noexcept { return transitionIdentity_; }
    [[nodiscard]] const base::Digest256& Spiral4IndirectIdentity() const noexcept { return spiral4IndirectIdentity_; }
    [[nodiscard]] const base::Digest256& Spiral5TemporalIdentity() const noexcept { return spiral5TemporalIdentity_; }
    [[nodiscard]] const base::Digest256& Spiral6SparseIdentity() const noexcept { return spiral6SparseIdentity_; }
    [[nodiscard]] const base::Digest256& TargetProfileIdentity() const noexcept { return targetProfileIdentity_; }
    [[nodiscard]] const base::Digest256& ConsumerProgramIdentity() const noexcept { return consumerProgramIdentity_; }
    [[nodiscard]] const base::Digest256& ObservationContractIdentity() const noexcept { return observationContractIdentity_; }
    [[nodiscard]] const base::Digest256& ArtifactIdentity() const noexcept { return artifactIdentity_; }
    [[nodiscard]] const std::vector<semantic::DeltaIndexActionRecordV1>& FixedCapacityRecords() const noexcept { return fixedCapacityRecords_; }
    [[nodiscard]] const std::vector<std::byte>& Bytes() const noexcept { return bytes_; }

private:
    friend FrozenCompactDeltaArtifactV1 BuildCompactDeltaArtifactV1(
        const semantic::SparseTemporalDeltaSemanticV1&,
        const semantic::SparseDeltaInvocationV1&);
    friend base::Result<FrozenCompactDeltaArtifactV1, DeltaContractErrorV1>
        ParseCompactDeltaArtifactV1(std::span<const std::byte>);

    FrozenCompactDeltaArtifactV1(
        DeltaCandidateKindV1 candidateKind,
        DeltaRepresentationRoleV1 representationRole,
        DeltaOperationCodeV1 operationCode,
        DeltaOutputHistoryPolicyV1 outputHistoryPolicy,
        std::uint32_t universeCount,
        std::uint32_t transitionCount,
        std::uint32_t updateCount,
        std::uint32_t clearCount,
        std::uint32_t threadsPerGroup,
        std::uint32_t dispatchGroupsX,
        std::uint32_t recordStrideBytes,
        std::uint32_t fixedCapacityBytes,
        base::Digest256 semanticIdentity,
        base::Digest256 invocationIdentity,
        base::Digest256 previousActiveSetIdentity,
        base::Digest256 activeSetIdentity,
        base::Digest256 modifiedSetIdentity,
        base::Digest256 transitionIdentity,
        base::Digest256 spiral4IndirectIdentity,
        base::Digest256 spiral5TemporalIdentity,
        base::Digest256 spiral6SparseIdentity,
        base::Digest256 targetProfileIdentity,
        base::Digest256 consumerProgramIdentity,
        base::Digest256 observationContractIdentity,
        base::Digest256 artifactIdentity,
        std::vector<semantic::DeltaIndexActionRecordV1> fixedCapacityRecords,
        std::vector<std::byte> bytes);

    DeltaCandidateKindV1 candidateKind_ = DeltaCandidateKindV1::CompactDeltaIndexHistoryReuse;
    DeltaRepresentationRoleV1 representationRole_ = DeltaRepresentationRoleV1::CanonicalSortedIndexActionList;
    DeltaOperationCodeV1 operationCode_ = DeltaOperationCodeV1::ExecuteCompactDeltaHistory;
    DeltaOutputHistoryPolicyV1 outputHistoryPolicy_ =
        DeltaOutputHistoryPolicyV1::CopyPreviousHistoryThenWriteExactlyTransitionSet;
    std::uint32_t universeCount_ = 0;
    std::uint32_t transitionCount_ = 0;
    std::uint32_t updateCount_ = 0;
    std::uint32_t clearCount_ = 0;
    std::uint32_t threadsPerGroup_ = 0;
    std::uint32_t dispatchGroupsX_ = 0;
    std::uint32_t recordStrideBytes_ = 0;
    std::uint32_t fixedCapacityBytes_ = 0;
    base::Digest256 semanticIdentity_{};
    base::Digest256 invocationIdentity_{};
    base::Digest256 previousActiveSetIdentity_{};
    base::Digest256 activeSetIdentity_{};
    base::Digest256 modifiedSetIdentity_{};
    base::Digest256 transitionIdentity_{};
    base::Digest256 spiral4IndirectIdentity_{};
    base::Digest256 spiral5TemporalIdentity_{};
    base::Digest256 spiral6SparseIdentity_{};
    base::Digest256 targetProfileIdentity_{};
    base::Digest256 consumerProgramIdentity_{};
    base::Digest256 observationContractIdentity_{};
    base::Digest256 artifactIdentity_{};
    std::vector<semantic::DeltaIndexActionRecordV1> fixedCapacityRecords_;
    std::vector<std::byte> bytes_;
};

[[nodiscard]] base::Digest256 Spiral4IndirectExtensionIdentityV1();
[[nodiscard]] base::Digest256 Spiral5TemporalExtensionIdentityV1();
[[nodiscard]] base::Digest256 Spiral6SparseExtensionIdentityV1();
[[nodiscard]] base::Digest256 DeltaTargetProfileIdentityV1();
[[nodiscard]] base::Digest256 CompactDeltaConsumerProgramIdentityV1();
[[nodiscard]] base::Digest256 DeltaObservationContractIdentityV1();

[[nodiscard]] FrozenCompactDeltaArtifactV1 BuildCompactDeltaArtifactV1(
    const semantic::SparseTemporalDeltaSemanticV1& semanticValue,
    const semantic::SparseDeltaInvocationV1& invocation);
[[nodiscard]] base::Result<FrozenCompactDeltaArtifactV1, DeltaContractErrorV1>
ParseCompactDeltaArtifactV1(std::span<const std::byte> bytes);
[[nodiscard]] base::Result<void, DeltaContractErrorV1>
ValidateCompactDeltaArtifactForExecutionV1(
    const FrozenCompactDeltaArtifactV1& artifact,
    const semantic::SparseTemporalDeltaSemanticV1& semanticValue,
    const semantic::SparseDeltaInvocationV1& invocation);
} // namespace sge4_5::spiral7::contract
