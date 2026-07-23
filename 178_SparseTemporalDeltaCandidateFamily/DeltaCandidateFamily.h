#pragma once

#include "../00_Foundation/Result.h"
#include "../00_Foundation/Sha256.h"
#include "../172_SparseTemporalDeltaSemantic/SparseTemporalDeltaSemantic.h"
#include "../173_Spiral7DeltaContracts/Spiral7DeltaContracts.h"
#include "../175_SparseTemporalDeltaLoweringCandidate/DeltaLoweringCandidate.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace sge4_5::spiral7::family_candidate
{
inline constexpr std::uint32_t DenseActiveMaskCapacityBytesV1 = 512;
inline constexpr std::uint32_t CompactDeltaCapacityBytesV1 = semantic::DeltaRecordCapacityBytesV1;
inline constexpr std::uint32_t AffectedBlockRecordStrideBytesV1 = 24;
inline constexpr std::uint32_t AffectedBlockCapacityV1 = 64;
inline constexpr std::uint32_t AffectedBlockCapacityBytesV1 =
    AffectedBlockRecordStrideBytesV1 * AffectedBlockCapacityV1;
inline constexpr std::uint32_t UnusedBlockSentinelV1 = 0xFFFF'FFFFu;

// A/B/C names and numeric order are frozen by the Spiral 7 CU1 contract.
enum class DeltaFamilyCandidateKindV1 : std::uint32_t
{
    FullActiveDenseRecompute = 1,
    CompactDeltaIndexHistoryReuse = 2,
    AffectedBlockDeltaHistoryReuse = 3
};

enum class DeltaFamilyRepresentationRoleV1 : std::uint32_t
{
    DenseActiveMask4096 = 1,
    CanonicalSortedIndexActionList = 2,
    AffectedBlockUpdateClearMasks = 3
};

enum class DeltaFamilyOperationCodeV1 : std::uint32_t
{
    ExecuteFullActiveDenseRecompute = 0x0007'0401u,
    ExecuteCompactDeltaHistory = 0x0007'0402u,
    ExecuteAffectedBlockDeltaHistory = 0x0007'0403u
};

enum class DeltaFamilyDispatchPolicyV1 : std::uint32_t
{
    FixedUniverseGroups = 1,
    CeilTransitionGroups = 2,
    OneGroupPerAffectedBlock = 3
};

enum class DeltaFamilyOutputHistoryPolicyV1 : std::uint32_t
{
    WriteEntireUniverseFromCurrentActiveMask = 1,
    CopyPreviousHistoryThenWriteExactlyTransitionSet = 2
};

enum class DeltaFamilyLegalWriteSetV1 : std::uint32_t
{
    EntireUniverse = 1,
    ExactlyTransitionSet = 2
};

enum class DeltaFamilyCompletionPolicyV1 : std::uint32_t
{
    ExecuteIndirectCompletionAuthorizesCandidateWriteSet = 1
};

enum class DeltaFamilyDeviceEpochPolicyV1 : std::uint32_t
{
    FamilyResourcesAndHistoryBoundToDeviceEpoch = 1
};

struct AffectedBlockDeltaRecordV1 final
{
    std::uint32_t blockIndex = UnusedBlockSentinelV1;
    std::uint32_t reserved = 0;
    std::uint64_t updateMask = 0;
    std::uint64_t clearMask = 0;
};
static_assert(sizeof(AffectedBlockDeltaRecordV1) == AffectedBlockRecordStrideBytesV1);

class FrozenDeltaFamilyArtifactV1 final
{
public:
    FrozenDeltaFamilyArtifactV1() = delete;
    FrozenDeltaFamilyArtifactV1(const FrozenDeltaFamilyArtifactV1&) = default;
    FrozenDeltaFamilyArtifactV1& operator=(const FrozenDeltaFamilyArtifactV1&) = default;

    [[nodiscard]] DeltaFamilyCandidateKindV1 Kind() const noexcept { return kind_; }
    [[nodiscard]] DeltaFamilyRepresentationRoleV1 Role() const noexcept { return role_; }
    [[nodiscard]] DeltaFamilyOperationCodeV1 OperationCode() const noexcept { return operationCode_; }
    [[nodiscard]] DeltaFamilyDispatchPolicyV1 DispatchPolicy() const noexcept { return dispatchPolicy_; }
    [[nodiscard]] DeltaFamilyOutputHistoryPolicyV1 OutputHistoryPolicy() const noexcept { return outputHistoryPolicy_; }
    [[nodiscard]] DeltaFamilyLegalWriteSetV1 LegalWriteSet() const noexcept { return legalWriteSet_; }
    [[nodiscard]] std::uint32_t ActiveCount() const noexcept { return activeCount_; }
    [[nodiscard]] std::uint32_t TransitionCount() const noexcept { return transitionCount_; }
    [[nodiscard]] std::uint32_t UpdateCount() const noexcept { return updateCount_; }
    [[nodiscard]] std::uint32_t ClearCount() const noexcept { return clearCount_; }
    [[nodiscard]] std::uint32_t RetainCount() const noexcept { return retainCount_; }
    [[nodiscard]] std::uint32_t AffectedBlockCount() const noexcept { return affectedBlockCount_; }
    [[nodiscard]] std::uint32_t DispatchGroupsX() const noexcept { return dispatchGroupsX_; }
    [[nodiscard]] std::uint32_t PayloadStrideBytes() const noexcept { return payloadStrideBytes_; }
    [[nodiscard]] std::uint32_t FixedCapacityBytes() const noexcept { return fixedCapacityBytes_; }
    [[nodiscard]] const base::Digest256& SemanticIdentity() const noexcept { return semanticIdentity_; }
    [[nodiscard]] const base::Digest256& InvocationIdentity() const noexcept { return invocationIdentity_; }
    [[nodiscard]] const base::Digest256& PreviousActiveSetIdentity() const noexcept { return previousActiveSetIdentity_; }
    [[nodiscard]] const base::Digest256& ActiveSetIdentity() const noexcept { return activeSetIdentity_; }
    [[nodiscard]] const base::Digest256& ModifiedSetIdentity() const noexcept { return modifiedSetIdentity_; }
    [[nodiscard]] const base::Digest256& TransitionSetIdentity() const noexcept { return transitionSetIdentity_; }
    [[nodiscard]] const base::Digest256& TransitionRecordsIdentity() const noexcept { return transitionRecordsIdentity_; }
    [[nodiscard]] const base::Digest256& Cu2CompactArtifactIdentity() const noexcept { return cu2CompactArtifactIdentity_; }
    [[nodiscard]] const base::Digest256& Cu3TransitionAuthorityIdentity() const noexcept { return cu3TransitionAuthorityIdentity_; }
    [[nodiscard]] const base::Digest256& ConsumerProgramIdentity() const noexcept { return consumerProgramIdentity_; }
    [[nodiscard]] const base::Digest256& ObservationContractIdentity() const noexcept { return observationContractIdentity_; }
    [[nodiscard]] const base::Digest256& ArtifactIdentity() const noexcept { return artifactIdentity_; }
    [[nodiscard]] const std::vector<std::byte>& PayloadBytes() const noexcept { return payloadBytes_; }
    [[nodiscard]] const std::vector<std::byte>& Bytes() const noexcept { return bytes_; }

private:
    friend FrozenDeltaFamilyArtifactV1 BuildDeltaFamilyArtifactV1(
        const semantic::SparseTemporalDeltaSemanticV1&,
        const semantic::SparseDeltaInvocationV1&,
        DeltaFamilyCandidateKindV1);

    FrozenDeltaFamilyArtifactV1(
        DeltaFamilyCandidateKindV1 kind,
        DeltaFamilyRepresentationRoleV1 role,
        DeltaFamilyOperationCodeV1 operationCode,
        DeltaFamilyDispatchPolicyV1 dispatchPolicy,
        DeltaFamilyOutputHistoryPolicyV1 outputHistoryPolicy,
        DeltaFamilyLegalWriteSetV1 legalWriteSet,
        std::uint32_t activeCount,
        std::uint32_t transitionCount,
        std::uint32_t updateCount,
        std::uint32_t clearCount,
        std::uint32_t retainCount,
        std::uint32_t affectedBlockCount,
        std::uint32_t dispatchGroupsX,
        std::uint32_t payloadStrideBytes,
        std::uint32_t fixedCapacityBytes,
        base::Digest256 semanticIdentity,
        base::Digest256 invocationIdentity,
        base::Digest256 previousActiveSetIdentity,
        base::Digest256 activeSetIdentity,
        base::Digest256 modifiedSetIdentity,
        base::Digest256 transitionSetIdentity,
        base::Digest256 transitionRecordsIdentity,
        base::Digest256 cu2CompactArtifactIdentity,
        base::Digest256 cu3TransitionAuthorityIdentity,
        base::Digest256 consumerProgramIdentity,
        base::Digest256 observationContractIdentity,
        base::Digest256 artifactIdentity,
        std::vector<std::byte> payloadBytes,
        std::vector<std::byte> bytes);

    DeltaFamilyCandidateKindV1 kind_{};
    DeltaFamilyRepresentationRoleV1 role_{};
    DeltaFamilyOperationCodeV1 operationCode_{};
    DeltaFamilyDispatchPolicyV1 dispatchPolicy_{};
    DeltaFamilyOutputHistoryPolicyV1 outputHistoryPolicy_{};
    DeltaFamilyLegalWriteSetV1 legalWriteSet_{};
    std::uint32_t activeCount_ = 0;
    std::uint32_t transitionCount_ = 0;
    std::uint32_t updateCount_ = 0;
    std::uint32_t clearCount_ = 0;
    std::uint32_t retainCount_ = 0;
    std::uint32_t affectedBlockCount_ = 0;
    std::uint32_t dispatchGroupsX_ = 0;
    std::uint32_t payloadStrideBytes_ = 0;
    std::uint32_t fixedCapacityBytes_ = 0;
    base::Digest256 semanticIdentity_{};
    base::Digest256 invocationIdentity_{};
    base::Digest256 previousActiveSetIdentity_{};
    base::Digest256 activeSetIdentity_{};
    base::Digest256 modifiedSetIdentity_{};
    base::Digest256 transitionSetIdentity_{};
    base::Digest256 transitionRecordsIdentity_{};
    base::Digest256 cu2CompactArtifactIdentity_{};
    base::Digest256 cu3TransitionAuthorityIdentity_{};
    base::Digest256 consumerProgramIdentity_{};
    base::Digest256 observationContractIdentity_{};
    base::Digest256 artifactIdentity_{};
    std::vector<std::byte> payloadBytes_;
    std::vector<std::byte> bytes_;
};

struct RawDeltaFamilyCandidateV1 final
{
    base::Digest256 semanticIdentity{};
    base::Digest256 invocationIdentity{};
    base::Digest256 previousActiveSetIdentity{};
    base::Digest256 activeSetIdentity{};
    base::Digest256 modifiedSetIdentity{};
    base::Digest256 transitionSetIdentity{};
    base::Digest256 transitionRecordsIdentity{};
    base::Digest256 targetProfileIdentity{};
    base::Digest256 observationContractIdentity{};
    base::Digest256 representationResourceContractIdentity{};
    base::Digest256 completionContractIdentity{};
    base::Digest256 deviceEpochPolicyIdentity{};
    base::Digest256 consumerProgramIdentity{};
    base::Digest256 proposedArtifactIdentity{};
    base::Digest256 proposedRepresentationResourceIdentity{};
    base::Digest256 writeSetAuthorityIdentity{};
    base::Digest256 cu2CompactArtifactIdentity{};
    base::Digest256 cu3TransitionAuthorityIdentity{};

    DeltaFamilyCandidateKindV1 kind = DeltaFamilyCandidateKindV1::FullActiveDenseRecompute;
    DeltaFamilyRepresentationRoleV1 representationRole = DeltaFamilyRepresentationRoleV1::DenseActiveMask4096;
    DeltaFamilyOperationCodeV1 operationCode = DeltaFamilyOperationCodeV1::ExecuteFullActiveDenseRecompute;
    DeltaFamilyDispatchPolicyV1 dispatchPolicy = DeltaFamilyDispatchPolicyV1::FixedUniverseGroups;
    DeltaFamilyOutputHistoryPolicyV1 outputHistoryPolicy = DeltaFamilyOutputHistoryPolicyV1::WriteEntireUniverseFromCurrentActiveMask;
    DeltaFamilyLegalWriteSetV1 legalWriteSet = DeltaFamilyLegalWriteSetV1::EntireUniverse;
    DeltaFamilyCompletionPolicyV1 completionPolicy = DeltaFamilyCompletionPolicyV1::ExecuteIndirectCompletionAuthorizesCandidateWriteSet;
    DeltaFamilyDeviceEpochPolicyV1 deviceEpochPolicy = DeltaFamilyDeviceEpochPolicyV1::FamilyResourcesAndHistoryBoundToDeviceEpoch;

    std::uint32_t invocationIndex = 0;
    std::uint32_t universeCount = 0;
    std::uint32_t activeCount = 0;
    std::uint32_t transitionCount = 0;
    std::uint32_t updateCount = 0;
    std::uint32_t clearCount = 0;
    std::uint32_t retainCount = 0;
    std::uint32_t affectedBlockCount = 0;
    std::uint32_t threadsPerGroup = 0;
    std::uint32_t dispatchGroupsX = 0;
    std::uint32_t payloadStrideBytes = 0;
    std::uint32_t fixedCapacityBytes = 0;
    std::uint32_t historyBytes = 0;
    std::uint32_t outputCapacity = 0;
    std::uint32_t outputStrideBytes = 0;
    std::uint32_t zeroWorkMapsToZeroDispatch = 0;

    base::Digest256 rawCandidateIdentity{};
};

[[nodiscard]] const char* DeltaFamilyCandidateNameV1(DeltaFamilyCandidateKindV1 value) noexcept;
[[nodiscard]] base::Digest256 DeltaFamilyTargetProfileIdentityV1();
[[nodiscard]] base::Digest256 DeltaFamilyObservationContractIdentityV1();
[[nodiscard]] base::Digest256 DeltaFamilyCompletionContractIdentityV1();
[[nodiscard]] base::Digest256 DeltaFamilyDeviceEpochPolicyIdentityV1();
[[nodiscard]] base::Digest256 DeltaFamilyConsumerProgramIdentityV1(DeltaFamilyCandidateKindV1 kind);
[[nodiscard]] base::Digest256 DeltaFamilyRepresentationResourceContractIdentityV1(DeltaFamilyCandidateKindV1 kind);
[[nodiscard]] base::Digest256 DeltaFamilyRepresentationResourceIdentityV1(const FrozenDeltaFamilyArtifactV1& artifact);
[[nodiscard]] base::Digest256 DeltaFamilyWriteSetAuthorityIdentityV1(
    const semantic::SparseTemporalDeltaSemanticV1& semantic,
    const semantic::SparseDeltaInvocationV1& invocation,
    const FrozenDeltaFamilyArtifactV1& artifact);

[[nodiscard]] FrozenDeltaFamilyArtifactV1 BuildDeltaFamilyArtifactV1(
    const semantic::SparseTemporalDeltaSemanticV1& semantic,
    const semantic::SparseDeltaInvocationV1& invocation,
    DeltaFamilyCandidateKindV1 kind);
[[nodiscard]] base::Result<void, std::string> ValidateDeltaFamilyArtifactV1(
    const FrozenDeltaFamilyArtifactV1& artifact,
    const semantic::SparseTemporalDeltaSemanticV1& semantic,
    const semantic::SparseDeltaInvocationV1& invocation);

[[nodiscard]] std::vector<std::byte> SerializeRawDeltaFamilyCandidateV1(
    const RawDeltaFamilyCandidateV1& value);
[[nodiscard]] base::Digest256 RawDeltaFamilyCandidateIdentityV1(
    const RawDeltaFamilyCandidateV1& value);
} // namespace sge4_5::spiral7::family_candidate
