#pragma once

#include "../../canonical/artifact/SectionedArtifact.h"
#include "../../canonical/base/SchemaValidation.h"
#include "../../composition/toolchain/CompositionToolchain.h"
#include "FrozenInvocationHistory.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace sge4::dynamic
{
inline constexpr std::array<std::byte, 8> FrozenInvocationMagic = {
    std::byte{'S'}, std::byte{'G'}, std::byte{'E'}, std::byte{'4'},
    std::byte{'I'}, std::byte{'N'}, std::byte{'V'}, std::byte{0}};
inline constexpr std::uint16_t FrozenInvocationFormatMajor = 1;
inline constexpr std::uint16_t FrozenInvocationFormatMinor = 3;
inline constexpr std::uint32_t FrozenInvocationManifestSchemaVersion = 4;

enum class FrozenInvocationSectionKind : std::uint32_t
{
    Manifest = 1,
    ExactSets = 2,
    TransitionRecords = 3,
    NextHistory = 4,
    ExecutionPayload = 5,
    ConditionalExecution = 6
};

inline constexpr std::array FrozenInvocationSectionKinds = {
    FrozenInvocationSectionKind::Manifest, FrozenInvocationSectionKind::ExactSets,
    FrozenInvocationSectionKind::TransitionRecords, FrozenInvocationSectionKind::NextHistory,
    FrozenInvocationSectionKind::ExecutionPayload,
    FrozenInvocationSectionKind::ConditionalExecution};
static_assert(base::ValuesAreUnique(FrozenInvocationSectionKinds,
    [](FrozenInvocationSectionKind value) { return std::to_underlying(value); }));
static_assert(base::ValuesAreStrictlyIncreasing(FrozenInvocationSectionKinds,
    [](FrozenInvocationSectionKind value) { return std::to_underlying(value); }));

struct MemberUpdateInputV1 final
{
    std::uint32_t member = 0;
    std::vector<std::byte> bytes;
};

struct InvocationInputV1 final
{
    std::uint64_t timelineOrdinal = 0;
    InvocationModeV1 mode = InvocationModeV1::InitialSeed;
    std::vector<std::uint32_t> activeMembers;
    std::vector<std::uint32_t> modifiedSurvivors;
    std::vector<MemberUpdateInputV1> updatePayloads;
};

struct FrozenDynamicExecutionPayloadV1 final
{
    composition::DynamicExecutionModeV1 mode =
        composition::DynamicExecutionModeV1::AuthorityOnly;
    composition::LeafPackageId targetLeaf;
    std::uint32_t targetDynamicSlot = package::InvalidIndex;
    std::uint32_t memberBytes = 0;
    DynamicExecutionPayloadIdentity identity;
    std::vector<MemberUpdatePayloadV1> updates;
};

class FrozenDynamicInvocationPackage final
{
public:
    FrozenDynamicInvocationPackage(const FrozenDynamicInvocationPackage&) = default;
    FrozenDynamicInvocationPackage& operator=(const FrozenDynamicInvocationPackage&) = default;
    FrozenDynamicInvocationPackage(FrozenDynamicInvocationPackage&&) noexcept = default;
    FrozenDynamicInvocationPackage& operator=(FrozenDynamicInvocationPackage&&) noexcept = default;

    [[nodiscard]] std::span<const std::byte> FileBytes() const noexcept { return bytes_; }
    [[nodiscard]] const frozen_dynamic_detail::OpaqueFrozenDynamicInvocationV1& Artifact() const noexcept
    {
        return artifact_;
    }
    [[nodiscard]] const VerifiedHistoryStateV1& NextHistory() const noexcept
    {
        return artifact_.NextHistory();
    }
    [[nodiscard]] const DynamicDecisionV1& Decision() const noexcept { return decision_; }
    [[nodiscard]] const FrozenDynamicExecutionPayloadV1& ExecutionPayload() const noexcept
    {
        return executionPayload_;
    }
    [[nodiscard]] InvocationModeV1 Mode() const noexcept { return artifact_.Mode(); }

private:
    friend base::Expected<FrozenDynamicInvocationPackage, Error> FreezeVerifiedInvocation(
        const VerifiedDynamicInvocationV1&);

    FrozenDynamicInvocationPackage(
        std::vector<std::byte> bytes,
        frozen_dynamic_detail::OpaqueFrozenDynamicInvocationV1 artifact,
        DynamicDecisionV1 decision,
        FrozenDynamicExecutionPayloadV1 executionPayload)
        : bytes_(std::move(bytes)), artifact_(std::move(artifact)),
          decision_(std::move(decision)), executionPayload_(std::move(executionPayload)) {}

    std::vector<std::byte> bytes_;
    frozen_dynamic_detail::OpaqueFrozenDynamicInvocationV1 artifact_;
    DynamicDecisionV1 decision_;
    FrozenDynamicExecutionPayloadV1 executionPayload_;
};

// Converts user-owned exact membership input into a canonical request. This function
// performs no planning and no verification.
[[nodiscard]] base::Expected<DynamicInvocationRequestV1, Error> BuildDynamicInvocationRequest(
    const composition::FrozenCompositionPackage& composition,
    canonical::DeviceEpoch deviceEpoch,
    InvocationInputV1 input,
    std::optional<VerifiedHistoryStateV1> previousHistory = std::nullopt);

// Serializes only an independently verified dynamic decision. Runtime code consumes this
// artifact and never invokes a Planner or Verifier.
[[nodiscard]] base::Expected<FrozenDynamicInvocationPackage, Error> FreezeVerifiedInvocation(
    const VerifiedDynamicInvocationV1& verified);
}
