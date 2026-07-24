#pragma once

#include "../194_Level4V2FrozenAuthority/FrozenAuthority.h"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace sge4_5::v2::composition
{
namespace canonical = sge4_5::v2::canonical;
namespace authority = sge4_5::v2::authority;
namespace frozen_authority = sge4_5::v2::frozen;

struct LeafIdentityTagV1;
struct EndpointIdentityTagV1;
struct FlowIdentityTagV1;
struct CompositionContractIdentityTagV1;
struct AllocationIdentityTagV1;
struct ScheduleIdentityTagV1;
struct BindingSetIdentityTagV1;
struct HandoffSetIdentityTagV1;
struct SynchronizationSetIdentityTagV1;
struct RecoverySetIdentityTagV1;
struct CompositionPlanIdentityTagV1;
struct CompositionSealIdentityTagV1;
struct FrozenCompositionIdentityTagV1;

using LeafIdentity = canonical::CanonicalIdentityV1<LeafIdentityTagV1>;
using EndpointIdentity = canonical::CanonicalIdentityV1<EndpointIdentityTagV1>;
using FlowIdentity = canonical::CanonicalIdentityV1<FlowIdentityTagV1>;
using CompositionContractIdentity = canonical::CanonicalIdentityV1<CompositionContractIdentityTagV1>;
using AllocationIdentity = canonical::CanonicalIdentityV1<AllocationIdentityTagV1>;
using ScheduleIdentity = canonical::CanonicalIdentityV1<ScheduleIdentityTagV1>;
using BindingSetIdentity = canonical::CanonicalIdentityV1<BindingSetIdentityTagV1>;
using HandoffSetIdentity = canonical::CanonicalIdentityV1<HandoffSetIdentityTagV1>;
using SynchronizationSetIdentity = canonical::CanonicalIdentityV1<SynchronizationSetIdentityTagV1>;
using RecoverySetIdentity = canonical::CanonicalIdentityV1<RecoverySetIdentityTagV1>;
using CompositionPlanIdentity = canonical::CanonicalIdentityV1<CompositionPlanIdentityTagV1>;
using CompositionSealIdentity = canonical::CanonicalIdentityV1<CompositionSealIdentityTagV1>;
using FrozenCompositionIdentity = canonical::CanonicalIdentityV1<FrozenCompositionIdentityTagV1>;

struct ScheduleOrdinalTagV1;
struct SignalOrdinalTagV1;
struct WaitOrdinalTagV1;
using ScheduleOrdinal = canonical::StrongScalarV1<ScheduleOrdinalTagV1>;
using SignalOrdinal = canonical::StrongScalarV1<SignalOrdinalTagV1>;
using WaitOrdinal = canonical::StrongScalarV1<WaitOrdinalTagV1>;

enum class ResourceKindV1 : std::uint8_t
{
    Buffer = 1,
    Unsupported = 255
};

enum class EndpointAccessV1 : std::uint8_t
{
    ReadOnly = 1,
    WriteOnly = 2
};

enum class FlowBoundaryV1 : std::uint8_t
{
    Internal = 1,
    CompositionInput = 2,
    CompositionOutput = 3
};

enum class ResourceStateV1 : std::uint8_t
{
    Common = 1,
    Read = 2,
    Write = 3,
    Present = 4
};

enum class AllocationOwnershipV1 : std::uint8_t
{
    ExternalInput = 1,
    CompositionOwned = 2
};

enum class RecoveryScopeV1 : std::uint8_t
{
    WholeComposition = 1
};

class LeafAuthorityV1 final
{
public:
    LeafAuthorityV1(const LeafAuthorityV1&) = default;
    LeafAuthorityV1& operator=(const LeafAuthorityV1&) = default;

    [[nodiscard]] const LeafIdentity& Identity() const noexcept { return identity_; }
    [[nodiscard]] const canonical::FrozenArtifactIdentity& FrozenArtifactIdentityValue() const noexcept { return frozenArtifactIdentity_; }
    [[nodiscard]] const canonical::VerifiedPlanIdentity& VerifiedPlanIdentityValue() const noexcept { return verifiedPlanIdentity_; }
    [[nodiscard]] const canonical::TargetProfileIdentity& TargetProfileIdentityValue() const noexcept { return targetProfileIdentity_; }
    [[nodiscard]] const canonical::ResourceContractIdentity& ResourceContractIdentityValue() const noexcept { return resourceContractIdentity_; }
    [[nodiscard]] std::uint32_t KeySchemaVersion() const noexcept { return keySchemaVersion_; }
    [[nodiscard]] std::span<const std::byte> CanonicalKey() const noexcept { return canonicalKey_; }

private:
    friend LeafAuthorityV1 MakeLeafAuthorityV1(
        std::uint32_t,
        std::span<const std::byte>,
        const frozen_authority::OpaqueFrozenArtifactV1&);
    friend LeafIdentity ComputeLeafIdentityV1(const LeafAuthorityV1&);

    LeafAuthorityV1(
        LeafIdentity identity,
        canonical::FrozenArtifactIdentity frozenArtifactIdentity,
        canonical::VerifiedPlanIdentity verifiedPlanIdentity,
        canonical::TargetProfileIdentity targetProfileIdentity,
        canonical::ResourceContractIdentity resourceContractIdentity,
        std::uint32_t keySchemaVersion,
        std::vector<std::byte> canonicalKey) noexcept
        : identity_(std::move(identity)),
          frozenArtifactIdentity_(std::move(frozenArtifactIdentity)),
          verifiedPlanIdentity_(std::move(verifiedPlanIdentity)),
          targetProfileIdentity_(std::move(targetProfileIdentity)),
          resourceContractIdentity_(std::move(resourceContractIdentity)),
          keySchemaVersion_(keySchemaVersion),
          canonicalKey_(std::move(canonicalKey))
    {
    }

    LeafIdentity identity_;
    canonical::FrozenArtifactIdentity frozenArtifactIdentity_;
    canonical::VerifiedPlanIdentity verifiedPlanIdentity_;
    canonical::TargetProfileIdentity targetProfileIdentity_;
    canonical::ResourceContractIdentity resourceContractIdentity_;
    std::uint32_t keySchemaVersion_;
    std::vector<std::byte> canonicalKey_;
};

class EndpointAuthorityV1 final
{
public:
    EndpointAuthorityV1(const EndpointAuthorityV1&) = default;
    EndpointAuthorityV1& operator=(const EndpointAuthorityV1&) = default;

    [[nodiscard]] const EndpointIdentity& Identity() const noexcept { return identity_; }
    [[nodiscard]] const LeafIdentity& LeafIdentityValue() const noexcept { return leafIdentity_; }
    [[nodiscard]] const canonical::ResourceContractIdentity& ResourceContractIdentityValue() const noexcept { return resourceContractIdentity_; }
    [[nodiscard]] ResourceKindV1 ResourceKind() const noexcept { return resourceKind_; }
    [[nodiscard]] EndpointAccessV1 Access() const noexcept { return access_; }
    [[nodiscard]] std::uint64_t MinimumBytes() const noexcept { return minimumBytes_; }
    [[nodiscard]] ResourceStateV1 IncomingState() const noexcept { return incomingState_; }
    [[nodiscard]] ResourceStateV1 OutgoingState() const noexcept { return outgoingState_; }
    [[nodiscard]] bool RequiresSynchronization() const noexcept { return requiresSynchronization_; }
    [[nodiscard]] bool Required() const noexcept { return required_; }
    [[nodiscard]] bool Presenter() const noexcept { return presenter_; }
    [[nodiscard]] std::uint32_t KeySchemaVersion() const noexcept { return keySchemaVersion_; }
    [[nodiscard]] std::span<const std::byte> CanonicalKey() const noexcept { return canonicalKey_; }

private:
    friend EndpointAuthorityV1 MakeEndpointAuthorityV1(
        const LeafAuthorityV1&,
        std::uint32_t,
        std::span<const std::byte>,
        ResourceKindV1,
        EndpointAccessV1,
        std::uint64_t,
        ResourceStateV1,
        ResourceStateV1,
        bool,
        bool,
        bool);
    friend EndpointIdentity ComputeEndpointIdentityV1(const EndpointAuthorityV1&);

    EndpointAuthorityV1(
        EndpointIdentity identity,
        LeafIdentity leafIdentity,
        canonical::ResourceContractIdentity resourceContractIdentity,
        ResourceKindV1 resourceKind,
        EndpointAccessV1 access,
        std::uint64_t minimumBytes,
        ResourceStateV1 incomingState,
        ResourceStateV1 outgoingState,
        bool requiresSynchronization,
        bool required,
        bool presenter,
        std::uint32_t keySchemaVersion,
        std::vector<std::byte> canonicalKey) noexcept
        : identity_(std::move(identity)), leafIdentity_(std::move(leafIdentity)),
          resourceContractIdentity_(std::move(resourceContractIdentity)), resourceKind_(resourceKind), access_(access),
          minimumBytes_(minimumBytes), incomingState_(incomingState), outgoingState_(outgoingState),
          requiresSynchronization_(requiresSynchronization), required_(required), presenter_(presenter),
          keySchemaVersion_(keySchemaVersion), canonicalKey_(std::move(canonicalKey))
    {
    }

    EndpointIdentity identity_;
    LeafIdentity leafIdentity_;
    canonical::ResourceContractIdentity resourceContractIdentity_;
    ResourceKindV1 resourceKind_;
    EndpointAccessV1 access_;
    std::uint64_t minimumBytes_;
    ResourceStateV1 incomingState_;
    ResourceStateV1 outgoingState_;
    bool requiresSynchronization_;
    bool required_;
    bool presenter_;
    std::uint32_t keySchemaVersion_;
    std::vector<std::byte> canonicalKey_;
};

struct FlowDeclarationV1 final
{
    FlowIdentity identity;
    std::uint32_t keySchemaVersion;
    std::vector<std::byte> canonicalKey;
    FlowBoundaryV1 boundary;
    std::vector<EndpointIdentity> endpoints;
};

struct RawCompositionContractV1 final
{
    CompositionContractIdentity identity;
    std::vector<LeafAuthorityV1> leaves;
    std::vector<EndpointAuthorityV1> endpoints;
    std::vector<FlowDeclarationV1> flows;
    RecoveryScopeV1 recoveryScope;
};

struct CompositionAllocationV1 final
{
    AllocationIdentity identity;
    FlowIdentity flowIdentity;
    std::uint64_t sizeBytes;
    AllocationOwnershipV1 ownership;
    auto operator<=>(const CompositionAllocationV1&) const = default;
};

struct CompositionScheduleEntryV1 final
{
    ScheduleOrdinal ordinal;
    LeafIdentity leafIdentity;
    auto operator<=>(const CompositionScheduleEntryV1&) const = default;
};

struct EndpointBindingV1 final
{
    EndpointIdentity endpointIdentity;
    FlowIdentity flowIdentity;
    auto operator<=>(const EndpointBindingV1&) const = default;
};

struct StateHandoffV1 final
{
    FlowIdentity flowIdentity;
    EndpointIdentity producerEndpointIdentity;
    EndpointIdentity consumerEndpointIdentity;
    ResourceStateV1 producerOutgoingState;
    ResourceStateV1 consumerIncomingState;
    auto operator<=>(const StateHandoffV1&) const = default;
};

struct SynchronizationEdgeV1 final
{
    FlowIdentity flowIdentity;
    LeafIdentity producerLeafIdentity;
    LeafIdentity consumerLeafIdentity;
    SignalOrdinal signalOrdinal;
    WaitOrdinal waitOrdinal;
    auto operator<=>(const SynchronizationEdgeV1&) const = default;
};

struct PresenterBindingV1 final
{
    LeafIdentity leafIdentity;
    EndpointIdentity endpointIdentity;
    auto operator<=>(const PresenterBindingV1&) const = default;
};

struct RecoverySetV1 final
{
    RecoverySetIdentity identity;
    RecoveryScopeV1 scope;
    std::vector<LeafIdentity> leaves;
    std::vector<FlowIdentity> flows;
};

struct CompositionPlanProposalV1 final
{
    CompositionPlanIdentity identity;
    CompositionContractIdentity contractIdentity;
    std::vector<CompositionAllocationV1> allocations;
    ScheduleIdentity scheduleIdentity;
    std::vector<CompositionScheduleEntryV1> schedule;
    BindingSetIdentity bindingSetIdentity;
    std::vector<EndpointBindingV1> bindings;
    HandoffSetIdentity handoffSetIdentity;
    std::vector<StateHandoffV1> handoffs;
    SynchronizationSetIdentity synchronizationSetIdentity;
    std::vector<SynchronizationEdgeV1> synchronizations;
    std::optional<PresenterBindingV1> presenter;
    RecoverySetV1 recoverySet;
};

class CompositionVerifierV1;
namespace frozen_composition_detail { class FrozenCompositionBuilderV1; }

class VerifiedCompositionV1 final
{
public:
    VerifiedCompositionV1(const VerifiedCompositionV1&) = default;
    VerifiedCompositionV1& operator=(const VerifiedCompositionV1&) = default;

    [[nodiscard]] const CompositionContractIdentity& ContractIdentity() const noexcept { return contractIdentity_; }
    [[nodiscard]] const CompositionPlanIdentity& PlanIdentity() const noexcept { return planIdentity_; }
    [[nodiscard]] const CompositionSealIdentity& SealIdentity() const noexcept { return sealIdentity_; }
    [[nodiscard]] const ScheduleIdentity& ScheduleIdentityValue() const noexcept { return scheduleIdentity_; }
    [[nodiscard]] const RecoverySetIdentity& RecoveryIdentity() const noexcept { return recoveryIdentity_; }
    [[nodiscard]] std::uint32_t LeafCount() const noexcept { return leafCount_; }
    [[nodiscard]] std::uint32_t FlowCount() const noexcept { return flowCount_; }

private:
    friend class CompositionVerifierV1;
    friend class frozen_composition_detail::FrozenCompositionBuilderV1;

    VerifiedCompositionV1(
        CompositionContractIdentity contractIdentity,
        CompositionPlanIdentity planIdentity,
        CompositionSealIdentity sealIdentity,
        ScheduleIdentity scheduleIdentity,
        RecoverySetIdentity recoveryIdentity,
        std::uint32_t leafCount,
        std::uint32_t flowCount) noexcept
        : contractIdentity_(std::move(contractIdentity)), planIdentity_(std::move(planIdentity)),
          sealIdentity_(std::move(sealIdentity)), scheduleIdentity_(std::move(scheduleIdentity)),
          recoveryIdentity_(std::move(recoveryIdentity)), leafCount_(leafCount), flowCount_(flowCount)
    {
    }

    CompositionContractIdentity contractIdentity_;
    CompositionPlanIdentity planIdentity_;
    CompositionSealIdentity sealIdentity_;
    ScheduleIdentity scheduleIdentity_;
    RecoverySetIdentity recoveryIdentity_;
    std::uint32_t leafCount_;
    std::uint32_t flowCount_;
};

enum class CompositionVerificationErrorV1 : std::uint8_t
{
    None = 0,
    LeafIdentityMismatch,
    EndpointIdentityMismatch,
    FlowIdentityMismatch,
    ContractIdentityMismatch,
    DuplicateLeaf,
    DuplicateEndpoint,
    DuplicateFlow,
    EndpointReferencesUnknownLeaf,
    EndpointResourceContractMismatch,
    UnsupportedResourceKind,
    EndpointBoundMoreThanOnce,
    RequiredEndpointUnbound,
    FlowReferencesUnknownEndpoint,
    InvalidFlowShape,
    MultipleWriters,
    TargetProfileMismatch,
    GraphCycle,
    MultiplePresenters,
    InvalidPresenter,
    AllocationMismatch,
    ScheduleMismatch,
    BindingMismatch,
    HandoffMismatch,
    SynchronizationMismatch,
    PresenterMismatch,
    RecoveryMismatch,
    PlanIdentityMismatch
};

struct CompositionVerificationResultV1 final
{
    CompositionVerificationErrorV1 error;
    std::optional<VerifiedCompositionV1> verified;
    [[nodiscard]] bool Accepted() const noexcept
    {
        return error == CompositionVerificationErrorV1::None && verified.has_value();
    }
};

[[nodiscard]] LeafAuthorityV1 MakeLeafAuthorityV1(
    std::uint32_t keySchemaVersion,
    std::span<const std::byte> canonicalKey,
    const frozen_authority::OpaqueFrozenArtifactV1& artifact);
[[nodiscard]] EndpointAuthorityV1 MakeEndpointAuthorityV1(
    const LeafAuthorityV1& leaf,
    std::uint32_t keySchemaVersion,
    std::span<const std::byte> canonicalKey,
    ResourceKindV1 resourceKind,
    EndpointAccessV1 access,
    std::uint64_t minimumBytes,
    ResourceStateV1 incomingState,
    ResourceStateV1 outgoingState,
    bool requiresSynchronization,
    bool required,
    bool presenter);
[[nodiscard]] FlowDeclarationV1 MakeFlowDeclarationV1(
    std::uint32_t keySchemaVersion,
    std::span<const std::byte> canonicalKey,
    FlowBoundaryV1 boundary,
    std::span<const EndpointIdentity> endpoints);
[[nodiscard]] RawCompositionContractV1 MakeRawCompositionContractV1(
    std::vector<LeafAuthorityV1> leaves,
    std::vector<EndpointAuthorityV1> endpoints,
    std::vector<FlowDeclarationV1> flows);

[[nodiscard]] LeafIdentity ComputeLeafIdentityV1(const LeafAuthorityV1& leaf);
[[nodiscard]] EndpointIdentity ComputeEndpointIdentityV1(const EndpointAuthorityV1& endpoint);
[[nodiscard]] FlowIdentity ComputeFlowIdentityV1(const FlowDeclarationV1& flow);
[[nodiscard]] CompositionContractIdentity ComputeCompositionContractIdentityV1(const RawCompositionContractV1& contract);
[[nodiscard]] AllocationIdentity ComputeAllocationIdentityV1(
    FlowIdentity flowIdentity,
    std::uint64_t sizeBytes,
    AllocationOwnershipV1 ownership);
[[nodiscard]] ScheduleIdentity ComputeScheduleIdentityV1(std::span<const CompositionScheduleEntryV1> schedule);
[[nodiscard]] BindingSetIdentity ComputeBindingSetIdentityV1(std::span<const EndpointBindingV1> bindings);
[[nodiscard]] HandoffSetIdentity ComputeHandoffSetIdentityV1(std::span<const StateHandoffV1> handoffs);
[[nodiscard]] SynchronizationSetIdentity ComputeSynchronizationSetIdentityV1(std::span<const SynchronizationEdgeV1> synchronizations);
[[nodiscard]] RecoverySetIdentity ComputeRecoverySetIdentityV1(const RecoverySetV1& recoverySet);
[[nodiscard]] CompositionPlanIdentity ComputeCompositionPlanIdentityV1(const CompositionPlanProposalV1& proposal);
[[nodiscard]] CompositionSealIdentity ComputeCompositionSealIdentityV1(
    CompositionContractIdentity contractIdentity,
    CompositionPlanIdentity planIdentity,
    ScheduleIdentity scheduleIdentity,
    RecoverySetIdentity recoveryIdentity);
[[nodiscard]] FrozenCompositionIdentity ComputeFrozenCompositionIdentityV1(
    const VerifiedCompositionV1& verified);

inline constexpr std::uint32_t CompositionModelSchemaVersionV1 = 1u;
}
