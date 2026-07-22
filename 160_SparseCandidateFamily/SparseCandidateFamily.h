#pragma once

#include "../00_Foundation/Result.h"
#include "../00_Foundation/Sha256.h"
#include "../154_SparseWorkSetSemantic/SparseWorkSetSemantic.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace sge4_5::spiral6::family_candidate
{
inline constexpr std::uint32_t DenseMaskCapacityBytesV1 = 512;
inline constexpr std::uint32_t CompactIndexCapacityBytesV1 = 16384;
inline constexpr std::uint32_t ActiveBlockRecordStrideBytesV1 = 16;
inline constexpr std::uint32_t ActiveBlockCapacityV1 = 64;
inline constexpr std::uint32_t ActiveBlockCapacityBytesV1 =
    ActiveBlockRecordStrideBytesV1 * ActiveBlockCapacityV1;
inline constexpr std::uint32_t UnusedBlockSentinelV1 = 0xFFFF'FFFFu;

enum class SparseFamilyCandidateKindV1 : std::uint32_t
{
    DenseMembershipMaskPredicate = 1,
    CompactIndexListDispatch = 2,
    ActiveBlockListLocalMask = 3
};

enum class SparseFamilyRepresentationRoleV1 : std::uint32_t
{
    DenseMembershipMask4096 = 1,
    CompactSortedUniverseIndexList = 2,
    ActiveBlockListWithLocal64BitMask = 3
};

enum class SparseFamilyOperationCodeV1 : std::uint32_t
{
    ExecuteDenseMaskPredicate = 0x0006'0401u,
    ExecuteCompactIndexList = 0x0006'0402u,
    ExecuteActiveBlockLocalMask = 0x0006'0403u
};

enum class SparseFamilyDispatchPolicyV1 : std::uint32_t
{
    FixedUniverseGroups = 1,
    CeilCardinalityGroups = 2,
    OneGroupPerActiveBlock = 3
};

enum class SparseFamilyCompletionPolicyV1 : std::uint32_t
{
    ExecuteIndirectCompletionAuthorizesExactWriteSet = 1
};

enum class SparseFamilyDeviceEpochPolicyV1 : std::uint32_t
{
    DerivedRepresentationBoundToDeviceEpoch = 1
};

struct ActiveBlockRecordV1 final
{
    std::uint32_t blockIndex = UnusedBlockSentinelV1;
    std::uint32_t maskLow = UnusedBlockSentinelV1;
    std::uint32_t maskHigh = UnusedBlockSentinelV1;
    std::uint32_t activeCount = UnusedBlockSentinelV1;
};
static_assert(sizeof(ActiveBlockRecordV1) == ActiveBlockRecordStrideBytesV1);

class FrozenSparseFamilyArtifactV1 final
{
public:
    FrozenSparseFamilyArtifactV1() = delete;
    FrozenSparseFamilyArtifactV1(const FrozenSparseFamilyArtifactV1&) = default;
    FrozenSparseFamilyArtifactV1& operator=(const FrozenSparseFamilyArtifactV1&) = default;

    [[nodiscard]] SparseFamilyCandidateKindV1 Kind() const noexcept { return kind_; }
    [[nodiscard]] SparseFamilyRepresentationRoleV1 Role() const noexcept { return role_; }
    [[nodiscard]] SparseFamilyOperationCodeV1 OperationCode() const noexcept { return operationCode_; }
    [[nodiscard]] SparseFamilyDispatchPolicyV1 DispatchPolicy() const noexcept { return dispatchPolicy_; }
    [[nodiscard]] std::uint32_t ActiveCount() const noexcept { return activeCount_; }
    [[nodiscard]] std::uint32_t ActiveBlockCount() const noexcept { return activeBlockCount_; }
    [[nodiscard]] std::uint32_t DispatchGroupsX() const noexcept { return dispatchGroupsX_; }
    [[nodiscard]] std::uint32_t PayloadStrideBytes() const noexcept { return payloadStrideBytes_; }
    [[nodiscard]] std::uint32_t FixedCapacityBytes() const noexcept { return fixedCapacityBytes_; }
    [[nodiscard]] const base::Digest256& SemanticIdentity() const noexcept { return semanticIdentity_; }
    [[nodiscard]] const base::Digest256& SparseSetIdentity() const noexcept { return sparseSetIdentity_; }
    [[nodiscard]] const base::Digest256& ConsumerProgramIdentity() const noexcept { return consumerProgramIdentity_; }
    [[nodiscard]] const base::Digest256& ObservationContractIdentity() const noexcept { return observationContractIdentity_; }
    [[nodiscard]] const base::Digest256& Cu2CompactArtifactIdentity() const noexcept { return cu2CompactArtifactIdentity_; }
    [[nodiscard]] const base::Digest256& ArtifactIdentity() const noexcept { return artifactIdentity_; }
    [[nodiscard]] const std::vector<std::byte>& PayloadBytes() const noexcept { return payloadBytes_; }
    [[nodiscard]] const std::vector<std::byte>& Bytes() const noexcept { return bytes_; }

private:
    friend FrozenSparseFamilyArtifactV1 BuildSparseFamilyArtifactV1(
        const semantic::SparsePointTransformSemanticV1&,
        const semantic::ExactSparseWorkSetV1&,
        SparseFamilyCandidateKindV1);

    FrozenSparseFamilyArtifactV1(
        SparseFamilyCandidateKindV1 kind,
        SparseFamilyRepresentationRoleV1 role,
        SparseFamilyOperationCodeV1 operationCode,
        SparseFamilyDispatchPolicyV1 dispatchPolicy,
        std::uint32_t activeCount,
        std::uint32_t activeBlockCount,
        std::uint32_t dispatchGroupsX,
        std::uint32_t payloadStrideBytes,
        std::uint32_t fixedCapacityBytes,
        base::Digest256 semanticIdentity,
        base::Digest256 sparseSetIdentity,
        base::Digest256 consumerProgramIdentity,
        base::Digest256 observationContractIdentity,
        base::Digest256 cu2CompactArtifactIdentity,
        base::Digest256 artifactIdentity,
        std::vector<std::byte> payloadBytes,
        std::vector<std::byte> bytes);

    SparseFamilyCandidateKindV1 kind_{};
    SparseFamilyRepresentationRoleV1 role_{};
    SparseFamilyOperationCodeV1 operationCode_{};
    SparseFamilyDispatchPolicyV1 dispatchPolicy_{};
    std::uint32_t activeCount_ = 0;
    std::uint32_t activeBlockCount_ = 0;
    std::uint32_t dispatchGroupsX_ = 0;
    std::uint32_t payloadStrideBytes_ = 0;
    std::uint32_t fixedCapacityBytes_ = 0;
    base::Digest256 semanticIdentity_{};
    base::Digest256 sparseSetIdentity_{};
    base::Digest256 consumerProgramIdentity_{};
    base::Digest256 observationContractIdentity_{};
    base::Digest256 cu2CompactArtifactIdentity_{};
    base::Digest256 artifactIdentity_{};
    std::vector<std::byte> payloadBytes_;
    std::vector<std::byte> bytes_;
};

struct RawSparseFamilyCandidateV1 final
{
    base::Digest256 semanticIdentity{};
    base::Digest256 sparseSetIdentity{};
    base::Digest256 targetProfileIdentity{};
    base::Digest256 observationContractIdentity{};
    base::Digest256 representationResourceContractIdentity{};
    base::Digest256 completionContractIdentity{};
    base::Digest256 deviceEpochPolicyIdentity{};
    base::Digest256 spiral4IndirectArtifactIdentity{};
    base::Digest256 consumerProgramIdentity{};
    base::Digest256 proposedArtifactIdentity{};
    base::Digest256 proposedRepresentationResourceIdentity{};
    base::Digest256 writeSetAuthorityIdentity{};
    base::Digest256 cu2CompactArtifactIdentity{};

    SparseFamilyCandidateKindV1 kind = SparseFamilyCandidateKindV1::DenseMembershipMaskPredicate;
    SparseFamilyRepresentationRoleV1 representationRole = SparseFamilyRepresentationRoleV1::DenseMembershipMask4096;
    SparseFamilyOperationCodeV1 operationCode = SparseFamilyOperationCodeV1::ExecuteDenseMaskPredicate;
    SparseFamilyDispatchPolicyV1 dispatchPolicy = SparseFamilyDispatchPolicyV1::FixedUniverseGroups;
    SparseFamilyCompletionPolicyV1 completionPolicy = SparseFamilyCompletionPolicyV1::ExecuteIndirectCompletionAuthorizesExactWriteSet;
    SparseFamilyDeviceEpochPolicyV1 deviceEpochPolicy = SparseFamilyDeviceEpochPolicyV1::DerivedRepresentationBoundToDeviceEpoch;

    std::uint32_t universeCount = 0;
    std::uint32_t activeCount = 0;
    std::uint32_t activeBlockCount = 0;
    std::uint32_t threadsPerGroup = 0;
    std::uint32_t dispatchGroupsX = 0;
    std::uint32_t payloadStrideBytes = 0;
    std::uint32_t fixedCapacityBytes = 0;
    std::uint32_t outputCapacity = 0;
    std::uint32_t outputStrideBytes = 0;
    std::uint32_t zeroActiveMapsToZeroDispatch = 0;

    base::Digest256 rawCandidateIdentity{};
};

[[nodiscard]] const char* SparseFamilyCandidateNameV1(SparseFamilyCandidateKindV1 value) noexcept;
[[nodiscard]] base::Digest256 SparseFamilyTargetProfileIdentityV1();
[[nodiscard]] base::Digest256 SparseFamilyObservationContractIdentityV1();
[[nodiscard]] base::Digest256 SparseFamilyCompletionContractIdentityV1();
[[nodiscard]] base::Digest256 SparseFamilyDeviceEpochPolicyIdentityV1();
[[nodiscard]] base::Digest256 SparseFamilyConsumerProgramIdentityV1();
[[nodiscard]] base::Digest256 SparseFamilyRepresentationResourceContractIdentityV1(
    SparseFamilyCandidateKindV1 kind);
[[nodiscard]] base::Digest256 SparseFamilyRepresentationResourceIdentityV1(
    const FrozenSparseFamilyArtifactV1& artifact);
[[nodiscard]] base::Digest256 SparseFamilyWriteSetAuthorityIdentityV1(
    const semantic::SparsePointTransformSemanticV1& semantic,
    const semantic::ExactSparseWorkSetV1& set,
    const FrozenSparseFamilyArtifactV1& artifact);

[[nodiscard]] FrozenSparseFamilyArtifactV1 BuildSparseFamilyArtifactV1(
    const semantic::SparsePointTransformSemanticV1& semantic,
    const semantic::ExactSparseWorkSetV1& set,
    SparseFamilyCandidateKindV1 kind);
[[nodiscard]] base::Result<void, std::string> ValidateSparseFamilyArtifactV1(
    const FrozenSparseFamilyArtifactV1& artifact,
    const semantic::SparsePointTransformSemanticV1& semantic,
    const semantic::ExactSparseWorkSetV1& set);

[[nodiscard]] std::vector<std::byte> SerializeRawSparseFamilyCandidateV1(
    const RawSparseFamilyCandidateV1& value);
[[nodiscard]] base::Digest256 RawSparseFamilyCandidateIdentityV1(
    const RawSparseFamilyCandidateV1& value);
}
