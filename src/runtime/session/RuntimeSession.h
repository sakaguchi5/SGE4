#pragma once

#include "../../composition/toolchain/CompositionToolchain.h"
#include "../../dynamic/artifact/DynamicInvocationPackage.h"
#include "../core/package/PackageRuntime.h"

#include <cstddef>
#include <optional>
#include <vector>

namespace sge4::runtime
{
struct DynamicPlanningContext final
{
    canonical::DeviceEpoch deviceEpoch;
    dynamic::InvocationModeV1 requiredMode;
    std::optional<dynamic::VerifiedHistoryStateV1> previousHistory;
};

struct PreparedDynamicBindingV1 final
{
    bool enabled = false;
    composition::LeafPackageId leaf;
    std::uint32_t slot = package::InvalidIndex;
    std::vector<std::byte> denseSlotBytes;
};

// Generalization 6 prepares all route shadows as one atomic candidate.  Native
// submission receives only enabled bindings, but successful submission commits
// every prepared shadow together with History.
struct PreparedDynamicExecutionV1 final
{
    std::vector<PreparedDynamicBindingV1> bindings;
    std::vector<composition::LeafPackageId> enabledLeaves;
    bool hasIndirectDispatch = false;
    composition::LeafPackageId indirectLeaf;
    std::uint32_t indirectComputeCommand = package::InvalidIndex;
    std::uint32_t indirectWorkCount = 0;
    std::uint32_t indirectThreadGroupCountX = 0;
    std::uint32_t indirectThreadGroupCountY = 1;
    std::uint32_t indirectThreadGroupCountZ = 1;
    std::uint32_t verifiedTransitionCount = 0;
    std::uint32_t appliedTransitionCount = 0;
    std::uint32_t conditionalRegionCount = 0;
};

class Session final
{
public:
    Session(Session&&) noexcept = default;
    Session& operator=(Session&&) noexcept = default;
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    [[nodiscard]] static base::Expected<Session, Error> Create(
        composition::FrozenCompositionPackage package,
        std::uint64_t deviceEpoch);

    [[nodiscard]] const composition::FrozenCompositionPackage& Package() const noexcept { return package_; }
    [[nodiscard]] std::uint64_t DeviceEpoch() const noexcept { return deviceEpoch_.value(); }
    [[nodiscard]] DeviceRuntimeState State() const noexcept { return state_; }
    [[nodiscard]] bool ExternalStateBound() const noexcept { return externalStateBound_; }
    [[nodiscard]] bool RequiresRecoverySeed() const noexcept { return recoverySeedRequired_; }
    [[nodiscard]] const canonical::RepresentationHandleV1& RepresentationHandle() const noexcept { return representationHandle_; }
    [[nodiscard]] const canonical::HistoryHandleV1& HistoryHandle() const noexcept { return historyHandle_; }
    [[nodiscard]] std::optional<canonical::HistoryValidityIdentity> AcceptedHistoryIdentity() const noexcept;

    [[nodiscard]] DynamicPlanningContext PlanningContext() const;
    [[nodiscard]] base::Expected<void, Error> ValidateForSubmission(
        const dynamic::FrozenDynamicInvocationPackage& invocation) const;
    [[nodiscard]] base::Expected<PreparedDynamicExecutionV1, Error> PrepareDynamicExecution(
        const dynamic::FrozenDynamicInvocationPackage& invocation) const;
    void CommitSubmission(
        const dynamic::FrozenDynamicInvocationPackage& invocation,
        PreparedDynamicExecutionV1 prepared);

    void ApplyRecoveryState(
        std::uint64_t newDeviceEpoch,
        DeviceRuntimeState state,
        bool rematerialized);
    [[nodiscard]] base::Expected<void, Error> AcknowledgeExternalRebind();

    [[nodiscard]] bool ValidateHandle(
        const canonical::RepresentationHandleV1& handle) const noexcept;
    [[nodiscard]] bool ValidateHandle(
        const canonical::HistoryHandleV1& handle) const noexcept;

private:
    Session(
        composition::FrozenCompositionPackage package,
        canonical::DeviceEpoch deviceEpoch,
        canonical::RepresentationHandleV1 representationHandle,
        canonical::HistoryHandleV1 historyHandle,
        std::vector<std::vector<std::byte>> dynamicExecutionShadows)
        : package_(std::move(package)), deviceEpoch_(deviceEpoch),
          representationHandle_(std::move(representationHandle)),
          historyHandle_(std::move(historyHandle)),
          dynamicExecutionShadows_(std::move(dynamicExecutionShadows)) {}

    [[nodiscard]] dynamic::InvocationModeV1 RequiredMode() const noexcept;
    void RebuildHandles();
    void ResetDynamicExecutionShadows();

    composition::FrozenCompositionPackage package_;
    canonical::DeviceEpoch deviceEpoch_;
    std::optional<dynamic::VerifiedHistoryStateV1> history_;
    canonical::RepresentationHandleV1 representationHandle_;
    canonical::HistoryHandleV1 historyHandle_;
    std::vector<std::vector<std::byte>> dynamicExecutionShadows_;
    std::uint64_t runtimeGeneration_ = 1;
    DeviceRuntimeState state_ = DeviceRuntimeState::Active;
    bool externalStateBound_ = true;
    bool recoverySeedRequired_ = false;
};
}
