#pragma once

#include "../00_Foundation/Result.h"
#include "../00_Foundation/Sha256.h"
#include "../121_Spiral4Contracts/Spiral4Contracts.h"
#include "../154_SparseWorkSetSemantic/SparseWorkSetSemantic.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace sge4_5::spiral6::contract
{
enum class SparseCandidateKindV1 : std::uint32_t
{
    CompactIndexListDispatch = 2
};

enum class SparseRepresentationRoleV1 : std::uint32_t
{
    CompactSortedUniverseIndexList = 1
};

enum class SparseOperationCodeV1 : std::uint32_t
{
    ExecuteCompactIndexList = 0x0006'0201u
};

enum class SparseOutputInitializationV1 : std::uint32_t
{
    CanonicalFloat4Zero = 1
};

enum class SparseWriteSetPolicyV1 : std::uint32_t
{
    ExactlyVerifiedSparseSet = 1
};

struct SparseContractErrorV1 final
{
    std::string stage;
    std::string message;
};

class FrozenCompactIndexListArtifactV1 final
{
public:
    FrozenCompactIndexListArtifactV1() = delete;
    FrozenCompactIndexListArtifactV1(const FrozenCompactIndexListArtifactV1&) = default;
    FrozenCompactIndexListArtifactV1& operator=(const FrozenCompactIndexListArtifactV1&) = default;

    [[nodiscard]] SparseCandidateKindV1 CandidateKind() const noexcept { return candidateKind_; }
    [[nodiscard]] SparseRepresentationRoleV1 RepresentationRole() const noexcept { return representationRole_; }
    [[nodiscard]] SparseOperationCodeV1 OperationCode() const noexcept { return operationCode_; }
    [[nodiscard]] std::uint32_t UniverseCount() const noexcept { return universeCount_; }
    [[nodiscard]] std::uint32_t ActiveCount() const noexcept { return activeCount_; }
    [[nodiscard]] std::uint32_t ThreadsPerGroup() const noexcept { return threadsPerGroup_; }
    [[nodiscard]] std::uint32_t DispatchGroupsX() const noexcept { return dispatchGroupsX_; }
    [[nodiscard]] std::uint32_t IndexStrideBytes() const noexcept { return indexStrideBytes_; }
    [[nodiscard]] std::uint32_t FixedCapacityBytes() const noexcept { return fixedCapacityBytes_; }
    [[nodiscard]] SparseOutputInitializationV1 OutputInitialization() const noexcept { return outputInitialization_; }
    [[nodiscard]] SparseWriteSetPolicyV1 WriteSetPolicy() const noexcept { return writeSetPolicy_; }
    [[nodiscard]] const base::Digest256& SemanticIdentity() const noexcept { return semanticIdentity_; }
    [[nodiscard]] const base::Digest256& SparseSetIdentity() const noexcept { return sparseSetIdentity_; }
    [[nodiscard]] const base::Digest256& Spiral4IndirectIdentity() const noexcept { return spiral4IndirectIdentity_; }
    [[nodiscard]] const base::Digest256& TargetProfileIdentity() const noexcept { return targetProfileIdentity_; }
    [[nodiscard]] const base::Digest256& ConsumerProgramIdentity() const noexcept { return consumerProgramIdentity_; }
    [[nodiscard]] const base::Digest256& ObservationContractIdentity() const noexcept { return observationContractIdentity_; }
    [[nodiscard]] const base::Digest256& ArtifactIdentity() const noexcept { return artifactIdentity_; }
    [[nodiscard]] const std::vector<std::uint32_t>& FixedCapacityIndices() const noexcept { return fixedCapacityIndices_; }
    [[nodiscard]] const std::vector<std::byte>& Bytes() const noexcept { return bytes_; }

private:
    friend FrozenCompactIndexListArtifactV1 BuildCompactIndexListArtifactV1(
        const semantic::SparsePointTransformSemanticV1&,
        const semantic::ExactSparseWorkSetV1&);
    friend base::Result<FrozenCompactIndexListArtifactV1, SparseContractErrorV1>
        ParseCompactIndexListArtifactV1(std::span<const std::byte>);

    FrozenCompactIndexListArtifactV1(
        SparseCandidateKindV1 candidateKind,
        SparseRepresentationRoleV1 representationRole,
        SparseOperationCodeV1 operationCode,
        std::uint32_t universeCount,
        std::uint32_t activeCount,
        std::uint32_t threadsPerGroup,
        std::uint32_t dispatchGroupsX,
        std::uint32_t indexStrideBytes,
        std::uint32_t fixedCapacityBytes,
        SparseOutputInitializationV1 outputInitialization,
        SparseWriteSetPolicyV1 writeSetPolicy,
        base::Digest256 semanticIdentity,
        base::Digest256 sparseSetIdentity,
        base::Digest256 spiral4IndirectIdentity,
        base::Digest256 targetProfileIdentity,
        base::Digest256 consumerProgramIdentity,
        base::Digest256 observationContractIdentity,
        base::Digest256 artifactIdentity,
        std::vector<std::uint32_t> fixedCapacityIndices,
        std::vector<std::byte> bytes);

    SparseCandidateKindV1 candidateKind_ = SparseCandidateKindV1::CompactIndexListDispatch;
    SparseRepresentationRoleV1 representationRole_ = SparseRepresentationRoleV1::CompactSortedUniverseIndexList;
    SparseOperationCodeV1 operationCode_ = SparseOperationCodeV1::ExecuteCompactIndexList;
    std::uint32_t universeCount_ = 0;
    std::uint32_t activeCount_ = 0;
    std::uint32_t threadsPerGroup_ = 0;
    std::uint32_t dispatchGroupsX_ = 0;
    std::uint32_t indexStrideBytes_ = 0;
    std::uint32_t fixedCapacityBytes_ = 0;
    SparseOutputInitializationV1 outputInitialization_ = SparseOutputInitializationV1::CanonicalFloat4Zero;
    SparseWriteSetPolicyV1 writeSetPolicy_ = SparseWriteSetPolicyV1::ExactlyVerifiedSparseSet;
    base::Digest256 semanticIdentity_{};
    base::Digest256 sparseSetIdentity_{};
    base::Digest256 spiral4IndirectIdentity_{};
    base::Digest256 targetProfileIdentity_{};
    base::Digest256 consumerProgramIdentity_{};
    base::Digest256 observationContractIdentity_{};
    base::Digest256 artifactIdentity_{};
    std::vector<std::uint32_t> fixedCapacityIndices_;
    std::vector<std::byte> bytes_;
};

[[nodiscard]] base::Digest256 SparseTargetProfileIdentityV1();
[[nodiscard]] base::Digest256 CompactIndexConsumerProgramIdentityV1();
[[nodiscard]] base::Digest256 SparseObservationContractIdentityV1();
[[nodiscard]] base::Digest256 Spiral4DispatchContractIdentityV1();

[[nodiscard]] FrozenCompactIndexListArtifactV1 BuildCompactIndexListArtifactV1(
    const semantic::SparsePointTransformSemanticV1& semanticValue,
    const semantic::ExactSparseWorkSetV1& set);
[[nodiscard]] base::Result<FrozenCompactIndexListArtifactV1, SparseContractErrorV1>
ParseCompactIndexListArtifactV1(std::span<const std::byte> bytes);
[[nodiscard]] base::Result<void, SparseContractErrorV1>
ValidateCompactIndexListArtifactForExecutionV1(
    const FrozenCompactIndexListArtifactV1& artifact,
    const semantic::SparsePointTransformSemanticV1& semanticValue,
    const semantic::ExactSparseWorkSetV1& set);
}
