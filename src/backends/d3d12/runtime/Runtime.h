#pragma once

#include "../executor/Executor.h"
#include "../../../composition/toolchain/CompositionToolchain.h"
#include "../../../dynamic/artifact/DynamicInvocationPackage.h"
#include "../../../runtime/session/RuntimeSession.h"

#include <memory>
#include <span>
#include <vector>

namespace sge4::d3d12
{
struct InitialResourceData final
{
    composition::ResourceFlowId resource;
    std::vector<std::byte> bytes;
};

struct LoadInput final
{
    std::vector<InitialResourceData> initialResources;
    runtime::ISurfaceHost* surface = nullptr;
};

struct LeafDynamicData final
{
    composition::LeafPackageId leaf;
    std::uint32_t slot = package::InvalidIndex;
    std::vector<std::byte> bytes;
};

struct FrameInput final
{
    std::uint64_t frameNumber = 0;
    std::vector<LeafDynamicData> leafDynamicData;
};

struct Submission final
{
    dynamic::FrozenDynamicInvocationPackage dynamicInvocation;
    std::uint64_t deviceEpoch = 0;
    std::uint32_t submittedLeafCount = 0;
};

struct RecoveryReport final
{
    Digest256 identity{};
    std::uint64_t previousEpoch = 0;
    std::uint64_t newEpoch = 0;
    bool allHandlesInvalidated = false;
    bool externalRebindRequired = false;
    bool recoverySeedRequired = false;
    bool awaitingAdapter = false;
    bool adapterReacquired = false;
};

struct BufferReadback final
{
    std::vector<std::byte> bytes;
};

class LoadedComposition final
{
public:
    LoadedComposition(LoadedComposition&&) noexcept;
    LoadedComposition& operator=(LoadedComposition&&) noexcept;
    LoadedComposition(const LoadedComposition&) = delete;
    LoadedComposition& operator=(const LoadedComposition&) = delete;
    ~LoadedComposition();

    [[nodiscard]] std::uint64_t DeviceEpoch() const noexcept;
    [[nodiscard]] runtime::DeviceRuntimeState State() const noexcept;
    [[nodiscard]] bool ExternalStateBound() const noexcept;
    [[nodiscard]] bool RequiresRecoverySeed() const noexcept;
    [[nodiscard]] const composition::FrozenCompositionPackage& Package() const noexcept;
    [[nodiscard]] const canonical::RepresentationHandleV1& RepresentationHandle() const noexcept;
    [[nodiscard]] const canonical::HistoryHandleV1& HistoryHandle() const noexcept;
    [[nodiscard]] std::optional<canonical::HistoryValidityIdentity> AcceptedHistoryIdentity() const noexcept;
    [[nodiscard]] runtime::DynamicPlanningContext PlanningContext() const;

private:
    struct Impl;
    explicit LoadedComposition(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_;

    friend base::Expected<LoadedComposition, Error> LoadComposition(
        std::span<const std::byte>, Executor&, LoadInput);
    friend base::Expected<Submission, Error> Submit(
        LoadedComposition&, dynamic::FrozenDynamicInvocationPackage, FrameInput);
    friend base::Expected<RecoveryReport, Error> Recover(
        LoadedComposition&, runtime::DeviceRecoveryMode);
    friend base::Expected<void, Error> AcknowledgeExternalRebind(LoadedComposition&);
    friend base::Expected<BufferReadback, Error> ReadBuffer(
        LoadedComposition&, composition::ResourceFlowId);
    friend bool ValidateHandleEpoch(
        const LoadedComposition&, const canonical::RepresentationHandleV1&) noexcept;
    friend bool ValidateHandleEpoch(
        const LoadedComposition&, const canonical::HistoryHandleV1&) noexcept;
};

[[nodiscard]] base::Expected<LoadedComposition, Error> LoadComposition(
    std::span<const std::byte> packageBytes,
    Executor& executor,
    LoadInput input = {});

[[nodiscard]] base::Expected<Submission, Error> Submit(
    LoadedComposition& loaded,
    dynamic::FrozenDynamicInvocationPackage invocation,
    FrameInput frame = {});

[[nodiscard]] base::Expected<RecoveryReport, Error> Recover(
    LoadedComposition& loaded,
    runtime::DeviceRecoveryMode mode);

[[nodiscard]] base::Expected<void, Error> AcknowledgeExternalRebind(
    LoadedComposition& loaded);

[[nodiscard]] base::Expected<BufferReadback, Error> ReadBuffer(
    LoadedComposition& loaded,
    composition::ResourceFlowId resource);

[[nodiscard]] bool ValidateHandleEpoch(
    const LoadedComposition& loaded,
    const canonical::RepresentationHandleV1& handle) noexcept;
[[nodiscard]] bool ValidateHandleEpoch(
    const LoadedComposition& loaded,
    const canonical::HistoryHandleV1& handle) noexcept;
}
