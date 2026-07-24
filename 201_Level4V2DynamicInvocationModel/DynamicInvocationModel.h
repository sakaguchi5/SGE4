#pragma once

#include "../199_Level4V2FrozenComposition/FrozenComposition.h"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace sge4_5::v2::dynamic
{
namespace canonical = sge4_5::v2::canonical;
namespace composition = sge4_5::v2::composition;
namespace frozen_composition = sge4_5::v2::composition::frozen_composition_detail;

struct UniverseCountTagV1;
struct MemberIndexTagV1;
using UniverseCount = canonical::StrongScalarV1<UniverseCountTagV1>;
using MemberIndex = canonical::StrongScalarV1<MemberIndexTagV1>;

struct IndexSetIdentityTagV1;
struct GenerationVectorIdentityTagV1;
struct TransitionRecordSetIdentityTagV1;
struct DynamicWriteSetIdentityTagV1;
struct DynamicDecisionIdentityTagV1;
struct DynamicSealIdentityTagV1;
struct FrozenDynamicInvocationIdentityTagV1;

using IndexSetIdentity = canonical::CanonicalIdentityV1<IndexSetIdentityTagV1>;
using GenerationVectorIdentity = canonical::CanonicalIdentityV1<GenerationVectorIdentityTagV1>;
using TransitionRecordSetIdentity = canonical::CanonicalIdentityV1<TransitionRecordSetIdentityTagV1>;
using DynamicWriteSetIdentity = canonical::CanonicalIdentityV1<DynamicWriteSetIdentityTagV1>;
using DynamicDecisionIdentity = canonical::CanonicalIdentityV1<DynamicDecisionIdentityTagV1>;
using DynamicSealIdentity = canonical::CanonicalIdentityV1<DynamicSealIdentityTagV1>;
using FrozenDynamicInvocationIdentity = canonical::CanonicalIdentityV1<FrozenDynamicInvocationIdentityTagV1>;

inline constexpr std::uint64_t InvalidItemGenerationV1 = ~std::uint64_t{0};
inline constexpr std::uint32_t DynamicInvocationSchemaVersionV1 = 1u;

struct ExactIndexSetBuildResultV1;

class ExactIndexSetV1 final
{
public:
    ExactIndexSetV1(const ExactIndexSetV1&) = default;
    ExactIndexSetV1& operator=(const ExactIndexSetV1&) = default;

    [[nodiscard]] UniverseCount Universe() const noexcept { return universe_; }
    [[nodiscard]] const IndexSetIdentity& Identity() const noexcept { return identity_; }
    [[nodiscard]] std::span<const std::uint32_t> Indices() const noexcept { return indices_; }
    [[nodiscard]] std::uint32_t Count() const noexcept { return static_cast<std::uint32_t>(indices_.size()); }
    [[nodiscard]] bool Contains(std::uint32_t index) const noexcept;

private:
    friend struct ExactIndexSetBuildResultV1;
    friend ExactIndexSetBuildResultV1 BuildExactIndexSetV1(UniverseCount, std::span<const std::uint32_t>);
    friend IndexSetIdentity ComputeIndexSetIdentityV1(const ExactIndexSetV1&);

    ExactIndexSetV1(UniverseCount universe, IndexSetIdentity identity, std::vector<std::uint32_t> indices) noexcept
        : universe_(universe), identity_(std::move(identity)), indices_(std::move(indices)) {}

    UniverseCount universe_;
    IndexSetIdentity identity_;
    std::vector<std::uint32_t> indices_;
};

enum class ExactIndexSetErrorV1 : std::uint8_t
{
    None = 0,
    ZeroUniverse,
    IndexOutOfRange,
    DuplicateIndex
};

struct ExactIndexSetBuildResultV1 final
{
    ExactIndexSetErrorV1 error = ExactIndexSetErrorV1::None;
    std::optional<ExactIndexSetV1> set;
    [[nodiscard]] bool Accepted() const noexcept { return error == ExactIndexSetErrorV1::None && set.has_value(); }
};

[[nodiscard]] ExactIndexSetBuildResultV1 BuildExactIndexSetV1(
    UniverseCount universe,
    std::span<const std::uint32_t> indices);
[[nodiscard]] IndexSetIdentity ComputeIndexSetIdentityV1(const ExactIndexSetV1& set);

enum class InvocationModeV1 : std::uint8_t
{
    InitialSeed = 1,
    ContinueHistory = 2,
    RecoverySeed = 3
};

enum class TransitionActionV1 : std::uint8_t
{
    Update = 1,
    Clear = 2
};

struct TransitionRecordV1 final
{
    MemberIndex member;
    TransitionActionV1 action;
    auto operator<=>(const TransitionRecordV1&) const = default;
};

class DynamicInvocationVerifierV1;
namespace frozen_dynamic_detail { class FrozenDynamicInvocationBuilderV1; }

class VerifiedHistoryStateV1 final
{
public:
    VerifiedHistoryStateV1(const VerifiedHistoryStateV1&) = default;
    VerifiedHistoryStateV1& operator=(const VerifiedHistoryStateV1&) = default;

    [[nodiscard]] const canonical::HistoryValidityDescriptorV1& Descriptor() const noexcept { return descriptor_; }
    [[nodiscard]] const composition::FrozenCompositionIdentity& CompositionIdentity() const noexcept { return compositionIdentity_; }
    [[nodiscard]] UniverseCount Universe() const noexcept { return universe_; }
    [[nodiscard]] const ExactIndexSetV1& ActiveSet() const noexcept { return activeSet_; }
    [[nodiscard]] const GenerationVectorIdentity& GenerationIdentity() const noexcept { return generationIdentity_; }
    [[nodiscard]] std::span<const std::uint64_t> ItemGenerations() const noexcept { return itemGenerations_; }

private:
    friend class frozen_dynamic_detail::FrozenDynamicInvocationBuilderV1;

    VerifiedHistoryStateV1(
        canonical::HistoryValidityDescriptorV1 descriptor,
        composition::FrozenCompositionIdentity compositionIdentity,
        UniverseCount universe,
        ExactIndexSetV1 activeSet,
        GenerationVectorIdentity generationIdentity,
        std::vector<std::uint64_t> itemGenerations) noexcept
        : descriptor_(std::move(descriptor)), compositionIdentity_(std::move(compositionIdentity)), universe_(universe),
          activeSet_(std::move(activeSet)), generationIdentity_(std::move(generationIdentity)),
          itemGenerations_(std::move(itemGenerations)) {}

    canonical::HistoryValidityDescriptorV1 descriptor_;
    composition::FrozenCompositionIdentity compositionIdentity_;
    UniverseCount universe_;
    ExactIndexSetV1 activeSet_;
    GenerationVectorIdentity generationIdentity_;
    std::vector<std::uint64_t> itemGenerations_;
};

struct DynamicInvocationRequestV1 final
{
    canonical::InvocationIdentity identity;
    composition::FrozenCompositionIdentity compositionIdentity;
    canonical::SemanticIdentity semanticIdentity;
    canonical::TimelineOrdinal timelineOrdinal;
    canonical::DeviceEpoch deviceEpoch;
    UniverseCount universe;
    InvocationModeV1 mode;
    ExactIndexSetV1 activeSet;
    ExactIndexSetV1 modifiedSurvivorSet;
    std::optional<VerifiedHistoryStateV1> previousHistory;
};

[[nodiscard]] DynamicInvocationRequestV1 MakeDynamicInvocationRequestV1(
    const frozen_composition::OpaqueFrozenCompositionV1& composition,
    canonical::SemanticIdentity semanticIdentity,
    canonical::TimelineOrdinal timelineOrdinal,
    canonical::DeviceEpoch deviceEpoch,
    UniverseCount universe,
    InvocationModeV1 mode,
    ExactIndexSetV1 activeSet,
    ExactIndexSetV1 modifiedSurvivorSet,
    std::optional<VerifiedHistoryStateV1> previousHistory = std::nullopt);

[[nodiscard]] canonical::InvocationIdentity ComputeDynamicInvocationIdentityV1(
    const DynamicInvocationRequestV1& request);
[[nodiscard]] GenerationVectorIdentity ComputeGenerationVectorIdentityV1(
    UniverseCount universe,
    std::span<const std::uint64_t> generations);
[[nodiscard]] TransitionRecordSetIdentity ComputeTransitionRecordSetIdentityV1(
    UniverseCount universe,
    std::span<const TransitionRecordV1> records);
[[nodiscard]] DynamicWriteSetIdentity ComputeDynamicWriteSetIdentityV1(
    const ExactIndexSetV1& transitionSet,
    TransitionRecordSetIdentity recordSetIdentity);

struct DynamicDecisionV1 final
{
    DynamicDecisionIdentity identity;
    canonical::InvocationIdentity requestIdentity;
    ExactIndexSetV1 previousActiveSet;
    ExactIndexSetV1 activationSet;
    ExactIndexSetV1 deactivationSet;
    ExactIndexSetV1 updateSet;
    ExactIndexSetV1 retainSet;
    ExactIndexSetV1 transitionSet;
    GenerationVectorIdentity generationVectorIdentity;
    std::vector<std::uint64_t> itemGenerations;
    TransitionRecordSetIdentity transitionRecordSetIdentity;
    std::vector<TransitionRecordV1> transitionRecords;
    canonical::TransitionCount indirectWorkCount;
    DynamicWriteSetIdentity dynamicWriteSetIdentity;
    canonical::HistoryGeneration nextHistoryGeneration;
};

struct DynamicPlannerProposalV1 final
{
    DynamicDecisionV1 decision;
};

[[nodiscard]] DynamicDecisionIdentity ComputeDynamicDecisionIdentityV1(const DynamicDecisionV1& decision);
[[nodiscard]] DynamicSealIdentity ComputeDynamicSealIdentityV1(
    const DynamicInvocationRequestV1& request,
    const DynamicDecisionV1& decision);
[[nodiscard]] canonical::HistoryValidityIdentity ComputeHistoryValidityIdentityV1(
    composition::FrozenCompositionIdentity compositionIdentity,
    canonical::SemanticIdentity semanticIdentity,
    canonical::InvocationIdentity sourceInvocationIdentity,
    canonical::HistoryGeneration generation,
    canonical::DeviceEpoch deviceEpoch,
    const ExactIndexSetV1& activeSet,
    GenerationVectorIdentity generationVectorIdentity);
[[nodiscard]] FrozenDynamicInvocationIdentity ComputeFrozenDynamicInvocationIdentityV1(
    composition::FrozenCompositionIdentity compositionIdentity,
    canonical::InvocationIdentity invocationIdentity,
    DynamicDecisionIdentity decisionIdentity,
    DynamicSealIdentity sealIdentity,
    canonical::HistoryValidityIdentity nextHistoryIdentity,
    DynamicWriteSetIdentity dynamicWriteSetIdentity);

class VerifiedDynamicInvocationV1 final
{
public:
    VerifiedDynamicInvocationV1(const VerifiedDynamicInvocationV1&) = default;
    VerifiedDynamicInvocationV1& operator=(const VerifiedDynamicInvocationV1&) = default;

    [[nodiscard]] const DynamicInvocationRequestV1& Request() const noexcept { return request_; }
    [[nodiscard]] const DynamicDecisionV1& Decision() const noexcept { return decision_; }
    [[nodiscard]] const DynamicSealIdentity& SealIdentity() const noexcept { return sealIdentity_; }

private:
    friend class DynamicInvocationVerifierV1;
    friend class frozen_dynamic_detail::FrozenDynamicInvocationBuilderV1;

    VerifiedDynamicInvocationV1(
        DynamicInvocationRequestV1 request,
        DynamicDecisionV1 decision,
        DynamicSealIdentity sealIdentity) noexcept
        : request_(std::move(request)), decision_(std::move(decision)), sealIdentity_(std::move(sealIdentity)) {}

    DynamicInvocationRequestV1 request_;
    DynamicDecisionV1 decision_;
    DynamicSealIdentity sealIdentity_;
};

enum class DynamicVerificationErrorV1 : std::uint8_t
{
    None = 0,
    RequestIdentityMismatch,
    ActiveUniverseMismatch,
    ModifiedUniverseMismatch,
    SeedModifiedSetNotEmpty,
    InitialHistoryPresent,
    ContinueHistoryMissing,
    RecoveryHistoryPresent,
    HistoryStateInvalid,
    HistoryCompositionMismatch,
    HistorySemanticMismatch,
    HistoryEpochMismatch,
    HistoryUniverseMismatch,
    TimelineNotSuccessor,
    ModifiedNotActive,
    ModifiedNotSurvivor,
    HistoryGenerationCountMismatch,
    PreviousActiveGenerationInvalid,
    GenerationOverflow,
    ProposalRequestIdentityMismatch,
    PreviousActiveSetMismatch,
    ActivationSetMismatch,
    DeactivationSetMismatch,
    UpdateSetMismatch,
    RetainSetMismatch,
    TransitionSetMismatch,
    GenerationVectorMismatch,
    TransitionRecordMismatch,
    IndirectWorkCountMismatch,
    DynamicWriteSetMismatch,
    DecisionIdentityMismatch
};

struct DynamicPlanningResultV1 final
{
    DynamicVerificationErrorV1 error = DynamicVerificationErrorV1::None;
    std::optional<DynamicPlannerProposalV1> proposal;
    [[nodiscard]] bool Planned() const noexcept { return error == DynamicVerificationErrorV1::None && proposal.has_value(); }
};

struct DynamicVerificationResultV1 final
{
    DynamicVerificationErrorV1 error = DynamicVerificationErrorV1::None;
    std::optional<VerifiedDynamicInvocationV1> verified;
    [[nodiscard]] bool Accepted() const noexcept { return error == DynamicVerificationErrorV1::None && verified.has_value(); }
};
}
