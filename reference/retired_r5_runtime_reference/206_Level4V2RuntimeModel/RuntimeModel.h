#pragma once

#include "../204_Level4V2FrozenInvocationHistory/FrozenInvocationHistory.h"

#include <algorithm>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace sge4_5::v2::runtime_recovery
{
namespace canonical = sge4_5::v2::canonical;
namespace composition = sge4_5::v2::composition;
namespace frozen_composition = sge4_5::v2::composition::frozen_composition_detail;
namespace dynamic = sge4_5::v2::dynamic;
namespace frozen_dynamic = sge4_5::v2::dynamic::frozen_dynamic_detail;

struct AdapterIdentityTagV1;
struct RuntimeInstanceIdentityTagV1;
struct ExternalBindingSetIdentityTagV1;
struct NativeMaterializationIdentityTagV1;
struct RuntimeSubmissionIdentityTagV1;
struct RecoveryOperationIdentityTagV1;
using AdapterIdentity = canonical::CanonicalIdentityV1<AdapterIdentityTagV1>;
using RuntimeInstanceIdentity = canonical::CanonicalIdentityV1<RuntimeInstanceIdentityTagV1>;
using ExternalBindingSetIdentity = canonical::CanonicalIdentityV1<ExternalBindingSetIdentityTagV1>;
using NativeMaterializationIdentity = canonical::CanonicalIdentityV1<NativeMaterializationIdentityTagV1>;
using RuntimeSubmissionIdentity = canonical::CanonicalIdentityV1<RuntimeSubmissionIdentityTagV1>;
using RecoveryOperationIdentity = canonical::CanonicalIdentityV1<RecoveryOperationIdentityTagV1>;

struct ExternalSlotOrdinalTagV1;
using ExternalSlotOrdinal = canonical::StrongScalarV1<ExternalSlotOrdinalTagV1>;

inline constexpr std::uint32_t RuntimeRecoverySchemaVersionV1 = 1u;

enum class RuntimeStateV1 : std::uint8_t
{
    Uninitialized = 0,
    ActiveNeedsExternalRebind = 1,
    Active = 2,
    Lost = 3,
    AwaitingAdapter = 4
};

enum class RecoveryModeV1 : std::uint8_t
{
    ControlledRebuild = 1,
    ForceRemovalForQualification = 2,
    RetryAdapterReacquisition = 3
};

enum class RuntimeErrorV1 : std::uint8_t
{
    None = 0,
    AdapterNotEnumerated,
    AdapterExcluded,
    AdapterAcquireFailed,
    MaterializationFailed,
    RuntimeNotActive,
    ExternalRebindRequired,
    ExternalBindingCompositionMismatch,
    ExternalBindingEpochMismatch,
    ExternalBindingDuplicateSlot,
    ExternalRebindFailed,
    InvocationCompositionMismatch,
    InvocationEpochMismatch,
    RecoverySeedRequired,
    SubmissionFailed,
    StaleHandle,
    StaleHistory,
    RemovalFailed,
    NoEligibleAdapter,
    RetryNotAwaitingAdapter
};

struct AdapterDescriptorV1 final
{
    AdapterIdentity identity;
    bool software = false;
    auto operator<=>(const AdapterDescriptorV1&) const = default;
};

struct ExternalBindingV1 final
{
    ExternalSlotOrdinal slot;
    canonical::ResourceInstanceIdentity resourceIdentity;
    canonical::Digest256V1 stateDigest;
    auto operator<=>(const ExternalBindingV1&) const = default;
};

struct ExternalBindingSetV1 final
{
    ExternalBindingSetIdentity identity;
    composition::FrozenCompositionIdentity compositionIdentity;
    canonical::DeviceEpoch deviceEpoch;
    std::vector<ExternalBindingV1> bindings;
};

struct NativeMaterializationRequestV1 final
{
    composition::FrozenCompositionIdentity compositionIdentity;
    AdapterIdentity adapterIdentity;
    canonical::DeviceEpoch deviceEpoch;
    std::uint32_t leafCount = 0;
    std::uint32_t flowCount = 0;
};

struct NativeSubmissionRequestV1 final
{
    dynamic::FrozenDynamicInvocationIdentity invocationIdentity;
    composition::FrozenCompositionIdentity compositionIdentity;
    dynamic::DynamicWriteSetIdentity writeSetIdentity;
    canonical::TransitionCount indirectWorkCount;
    canonical::DeviceEpoch deviceEpoch;
};

class IRuntimeBackendV1
{
public:
    virtual ~IRuntimeBackendV1() = default;
    [[nodiscard]] virtual std::vector<AdapterDescriptorV1> EnumerateAdapters() const = 0;
    [[nodiscard]] virtual bool AcquireAdapter(const AdapterIdentity& adapter) = 0;
    virtual void ReleaseAllRuntimeObjects() noexcept = 0;
    [[nodiscard]] virtual bool Materialize(const NativeMaterializationRequestV1& request) = 0;
    [[nodiscard]] virtual bool RebindExternalState(const ExternalBindingSetV1& bindings) = 0;
    [[nodiscard]] virtual bool Submit(const NativeSubmissionRequestV1& request) = 0;
    [[nodiscard]] virtual bool ForceRemoveCurrentAdapter() = 0;
};

[[nodiscard]] AdapterIdentity ComputeAdapterIdentityV1(std::span<const std::byte> canonicalDescriptor);
[[nodiscard]] RuntimeInstanceIdentity ComputeRuntimeInstanceIdentityV1(
    composition::FrozenCompositionIdentity compositionIdentity,
    AdapterIdentity adapterIdentity,
    canonical::DeviceEpoch deviceEpoch);
[[nodiscard]] ExternalBindingSetIdentity ComputeExternalBindingSetIdentityV1(const ExternalBindingSetV1& bindings);
[[nodiscard]] NativeMaterializationIdentity ComputeNativeMaterializationIdentityV1(
    RuntimeInstanceIdentity runtimeIdentity,
    composition::FrozenCompositionIdentity compositionIdentity,
    AdapterIdentity adapterIdentity,
    canonical::DeviceEpoch deviceEpoch,
    ExternalBindingSetIdentity externalBindingsIdentity);
[[nodiscard]] RuntimeSubmissionIdentity ComputeRuntimeSubmissionIdentityV1(
    RuntimeInstanceIdentity runtimeIdentity,
    dynamic::FrozenDynamicInvocationIdentity invocationIdentity,
    dynamic::DynamicWriteSetIdentity writeSetIdentity,
    canonical::TransitionCount indirectWorkCount,
    canonical::DeviceEpoch deviceEpoch);
[[nodiscard]] RecoveryOperationIdentity ComputeRecoveryOperationIdentityV1(
    RuntimeInstanceIdentity previousRuntimeIdentity,
    RecoveryModeV1 mode,
    AdapterIdentity previousAdapter,
    std::optional<AdapterIdentity> newAdapter,
    std::uint64_t previousEpoch,
    std::uint64_t newEpoch,
    RuntimeStateV1 finalState);
[[nodiscard]] ExternalBindingSetV1 MakeExternalBindingSetV1(
    composition::FrozenCompositionIdentity compositionIdentity,
    canonical::DeviceEpoch deviceEpoch,
    std::vector<ExternalBindingV1> bindings);

[[nodiscard]] bool ContainsAdapterIdentityV1(
    std::span<const AdapterDescriptorV1> adapters,
    const AdapterIdentity& identity) noexcept;
[[nodiscard]] bool ContainsExcludedAdapterV1(
    std::span<const AdapterIdentity> excluded,
    const AdapterIdentity& identity) noexcept;
}
