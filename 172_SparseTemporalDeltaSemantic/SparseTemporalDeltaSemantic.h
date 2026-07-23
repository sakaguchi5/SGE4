#pragma once

#include "../00_Foundation/Result.h"
#include "../00_Foundation/Sha256.h"
#include "../134_TemporalStateSemantic/TemporalStateSemantic.h"
#include "../154_SparseWorkSetSemantic/SparseWorkSetSemantic.h"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace sge4_5::spiral7::semantic
{
inline constexpr std::uint32_t WorkUniverseCountV1 = spiral6::semantic::WorkUniverseCountV1;
inline constexpr std::uint32_t ThreadsPerGroupV1 = spiral6::semantic::ThreadsPerGroupV1;
inline constexpr std::uint32_t TimelineLengthV1 = spiral5::semantic::TimelineLengthV1;
inline constexpr std::uint32_t HistoryDepthV1 = 1;
inline constexpr std::uint32_t InvalidGenerationV1 = 0xFFFF'FFFFu;
inline constexpr std::uint32_t InvalidUniverseIndexV1 = 0xFFFF'FFFFu;
inline constexpr std::uint32_t DeltaRecordStrideBytesV1 = 8;
inline constexpr std::uint32_t DeltaRecordCapacityBytesV1 =
    WorkUniverseCountV1 * DeltaRecordStrideBytesV1;

using ExactSparseWorkSetV1 = spiral6::semantic::ExactSparseWorkSetV1;
using SparsePatternKindV1 = spiral6::semantic::SparsePatternKindV1;
using SparseCardinalityV1 = spiral6::semantic::SparseCardinalityV1;

struct SparseTemporalDeltaSemanticV1 final
{
    std::uint32_t conventionVersion = 1;
    std::uint32_t semanticKind = 1; // VersionedSparseDeltaPointTransformTimelineV1
    std::uint32_t workUniverseCount = WorkUniverseCountV1;
    std::uint32_t outputCapacity = WorkUniverseCountV1;
    std::uint32_t threadsPerGroup = ThreadsPerGroupV1;
    std::uint32_t timelineLength = TimelineLengthV1;
    std::uint32_t historyDepth = HistoryDepthV1;
    std::uint32_t representationControl = 1; // DirectPgaThroughConsumerV1
    std::uint32_t inactiveSentinel = 1; // Float4Zero
    base::Digest256 sparseSemanticIdentity{};
    base::Digest256 temporalSemanticIdentity{};
};

enum class DeltaActionV1 : std::uint32_t
{
    Unused = 0,
    Update = 1,
    Clear = 2
};

struct DeltaIndexActionRecordV1 final
{
    std::uint32_t universeIndex = InvalidUniverseIndexV1;
    DeltaActionV1 action = DeltaActionV1::Unused;
    auto operator<=>(const DeltaIndexActionRecordV1&) const = default;
};
static_assert(sizeof(DeltaIndexActionRecordV1) == DeltaRecordStrideBytesV1);

struct SparseDeltaSemanticErrorV1 final
{
    std::string stage;
    std::string message;
};

class SparseDeltaInvocationV1 final
{
public:
    SparseDeltaInvocationV1() = delete;
    SparseDeltaInvocationV1(const SparseDeltaInvocationV1&) = default;
    SparseDeltaInvocationV1& operator=(const SparseDeltaInvocationV1&) = default;

    [[nodiscard]] std::uint32_t InvocationIndex() const noexcept { return invocationIndex_; }
    [[nodiscard]] bool ForceFullActiveRebuild() const noexcept { return forceFullActiveRebuild_; }
    [[nodiscard]] const ExactSparseWorkSetV1& PreviousActiveSet() const noexcept { return previousActiveSet_; }
    [[nodiscard]] const ExactSparseWorkSetV1& ActiveSet() const noexcept { return activeSet_; }
    [[nodiscard]] const ExactSparseWorkSetV1& ModifiedSurvivorSet() const noexcept { return modifiedSurvivorSet_; }
    [[nodiscard]] const ExactSparseWorkSetV1& ActivationSet() const noexcept { return activationSet_; }
    [[nodiscard]] const ExactSparseWorkSetV1& DeactivationSet() const noexcept { return deactivationSet_; }
    [[nodiscard]] const ExactSparseWorkSetV1& UpdateSet() const noexcept { return updateSet_; }
    [[nodiscard]] const ExactSparseWorkSetV1& RetainSet() const noexcept { return retainSet_; }
    [[nodiscard]] const ExactSparseWorkSetV1& TransitionSet() const noexcept { return transitionSet_; }
    [[nodiscard]] const std::vector<std::uint32_t>& ItemGenerations() const noexcept { return itemGenerations_; }
    [[nodiscard]] const std::vector<DeltaIndexActionRecordV1>& TransitionRecords() const noexcept { return transitionRecords_; }
    [[nodiscard]] const base::Digest256& Identity() const noexcept { return identity_; }

private:
    friend base::Result<SparseDeltaInvocationV1, SparseDeltaSemanticErrorV1>
        BuildSparseDeltaInvocationV1(
            std::uint32_t,
            const ExactSparseWorkSetV1&,
            const ExactSparseWorkSetV1&,
            const ExactSparseWorkSetV1&,
            std::span<const std::uint32_t>,
            bool);

    SparseDeltaInvocationV1(
        std::uint32_t invocationIndex,
        bool forceFullActiveRebuild,
        ExactSparseWorkSetV1 previousActiveSet,
        ExactSparseWorkSetV1 activeSet,
        ExactSparseWorkSetV1 modifiedSurvivorSet,
        ExactSparseWorkSetV1 activationSet,
        ExactSparseWorkSetV1 deactivationSet,
        ExactSparseWorkSetV1 updateSet,
        ExactSparseWorkSetV1 retainSet,
        ExactSparseWorkSetV1 transitionSet,
        std::vector<std::uint32_t> itemGenerations,
        std::vector<DeltaIndexActionRecordV1> transitionRecords,
        base::Digest256 identity);

    std::uint32_t invocationIndex_ = 0;
    bool forceFullActiveRebuild_ = false;
    ExactSparseWorkSetV1 previousActiveSet_;
    ExactSparseWorkSetV1 activeSet_;
    ExactSparseWorkSetV1 modifiedSurvivorSet_;
    ExactSparseWorkSetV1 activationSet_;
    ExactSparseWorkSetV1 deactivationSet_;
    ExactSparseWorkSetV1 updateSet_;
    ExactSparseWorkSetV1 retainSet_;
    ExactSparseWorkSetV1 transitionSet_;
    std::vector<std::uint32_t> itemGenerations_;
    std::vector<DeltaIndexActionRecordV1> transitionRecords_;
    base::Digest256 identity_{};
};

[[nodiscard]] SparseTemporalDeltaSemanticV1 BuildCanonicalSparseTemporalDeltaSemanticV1();
[[nodiscard]] std::vector<std::byte> SerializeSparseTemporalDeltaSemanticV1(
    const SparseTemporalDeltaSemanticV1& value);
[[nodiscard]] base::Digest256 SparseTemporalDeltaSemanticIdentityV1(
    const SparseTemporalDeltaSemanticV1& value);

[[nodiscard]] base::Result<SparseDeltaInvocationV1, SparseDeltaSemanticErrorV1>
BuildSparseDeltaInvocationV1(
    std::uint32_t invocationIndex,
    const ExactSparseWorkSetV1& previousActiveSet,
    const ExactSparseWorkSetV1& activeSet,
    const ExactSparseWorkSetV1& modifiedSurvivorSet,
    std::span<const std::uint32_t> previousItemGenerations,
    bool forceFullActiveRebuild = false);

[[nodiscard]] std::vector<std::byte> SerializeSparseDeltaInvocationV1(
    const SparseDeltaInvocationV1& value);
[[nodiscard]] base::Digest256 SparseDeltaInvocationIdentityV1(
    const SparseDeltaInvocationV1& value);
[[nodiscard]] base::Digest256 DeltaTransitionRecordsIdentityV1(
    std::span<const DeltaIndexActionRecordV1> records);

[[nodiscard]] std::vector<spiral5::semantic::PointRecordV1>
BuildPointCorpusForInvocationV1(const SparseDeltaInvocationV1& invocation);
[[nodiscard]] std::vector<spiral5::semantic::PointRecordV1>
BuildCpuReferenceOutputV1(const SparseDeltaInvocationV1& invocation);
[[nodiscard]] std::vector<std::byte> BuildCanonicalInactiveHistoryBytesV1();
[[nodiscard]] std::vector<std::uint32_t> BuildInitialInvalidGenerationsV1();
} // namespace sge4_5::spiral7::semantic
