#include "Runtime.h"

#include <algorithm>

#include "composition/CompositionRuntime.h"
#include "recovery/CompositionRecovery.h"
#include "../../../canonical/base/BinaryIO.h"

namespace sge4::d3d12
{
namespace native = runtime_detail;

struct LoadedComposition::Impl final
{
    Impl(runtime::Session sessionValue, native::LoadedStaticComposition nativeValue)
        : session(std::move(sessionValue)), nativeRuntime(std::move(nativeValue)) {}

    runtime::Session session;
    native::LoadedStaticComposition nativeRuntime;
};

namespace
{
template<class T>
[[nodiscard]] base::Expected<T, Error> Fail(std::string stage, std::string message)
{
    return base::Failure<T, Error>({std::move(stage), std::move(message)});
}

[[nodiscard]] Digest256 ComputeRecoveryIdentity(
    const composition::FrozenCompositionPackage& package,
    std::uint64_t previousEpoch,
    std::uint64_t newEpoch,
    runtime::DeviceRecoveryMode mode)
{
    base::BinaryWriter writer;
    writer.WriteBytes(package.Certificate().artifactIdentity.Digest());
    writer.WriteU64(previousEpoch);
    writer.WriteU64(newEpoch);
    writer.WriteU32(static_cast<std::uint32_t>(mode));
    return ComputeDomainDigest("sge4.d3d12.recovery", 1, writer.Bytes());
}
}

LoadedComposition::LoadedComposition(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}
LoadedComposition::LoadedComposition(LoadedComposition&&) noexcept = default;
LoadedComposition& LoadedComposition::operator=(LoadedComposition&&) noexcept = default;
LoadedComposition::~LoadedComposition() = default;

std::uint64_t LoadedComposition::DeviceEpoch() const noexcept { return impl_->session.DeviceEpoch(); }
runtime::DeviceRuntimeState LoadedComposition::State() const noexcept { return impl_->session.State(); }
bool LoadedComposition::ExternalStateBound() const noexcept { return impl_->session.ExternalStateBound(); }
bool LoadedComposition::RequiresRecoverySeed() const noexcept { return impl_->session.RequiresRecoverySeed(); }
const composition::FrozenCompositionPackage& LoadedComposition::Package() const noexcept { return impl_->session.Package(); }
const canonical::RepresentationHandleV1& LoadedComposition::RepresentationHandle() const noexcept { return impl_->session.RepresentationHandle(); }
const canonical::HistoryHandleV1& LoadedComposition::HistoryHandle() const noexcept { return impl_->session.HistoryHandle(); }
std::optional<canonical::HistoryValidityIdentity> LoadedComposition::AcceptedHistoryIdentity() const noexcept { return impl_->session.AcceptedHistoryIdentity(); }
runtime::DynamicPlanningContext LoadedComposition::PlanningContext() const { return impl_->session.PlanningContext(); }

base::Expected<LoadedComposition, Error> LoadComposition(
    std::span<const std::byte> packageBytes,
    Executor& executor,
    LoadInput input)
{
    auto package = composition::ReadFrozenCompositionPackage(packageBytes);
    if (!package)
        return Fail<LoadedComposition>(package.error().stage, package.error().message);

    native::StaticCompositionLoadInput nativeInput;
    nativeInput.surface = input.surface;
    nativeInput.initialResources.reserve(input.initialResources.size());
    for (auto& item : input.initialResources)
        nativeInput.initialResources.push_back({item.resource, std::move(item.bytes)});

    auto nativeRuntime = native::LoadStaticComposition(
        package.value().FileBytes(), executor, std::move(nativeInput));
    if (!nativeRuntime)
        return Fail<LoadedComposition>(nativeRuntime.error().stage, nativeRuntime.error().message);
    auto session = runtime::Session::Create(
        std::move(package).value(), nativeRuntime.value().DeviceEpoch());
    if (!session)
        return Fail<LoadedComposition>(session.error().stage, session.error().message);

    return base::Success<LoadedComposition, Error>(LoadedComposition(
        std::make_unique<LoadedComposition::Impl>(
            std::move(session).value(), std::move(nativeRuntime).value())));
}

base::Expected<Submission, Error> Submit(
    LoadedComposition& loaded,
    dynamic::FrozenDynamicInvocationPackage invocation,
    FrameInput frame)
{
    auto validated = loaded.impl_->session.ValidateForSubmission(invocation);
    if (!validated)
        return Fail<Submission>(validated.error().stage, validated.error().message);

    auto prepared = loaded.impl_->session.PrepareDynamicExecution(invocation);
    if (!prepared)
        return Fail<Submission>(prepared.error().stage, prepared.error().message);

    native::StaticCompositionFrameInvocation nativeInvocation;
    nativeInvocation.frameNumber = frame.frameNumber;
    nativeInvocation.enabledLeaves = prepared.value().enabledLeaves;
    std::size_t enabledRouteCount = 0;
    for (const auto& binding : prepared.value().bindings)
        if (binding.enabled) ++enabledRouteCount;
    const auto enabledWorklistCount =
        prepared.value().hasCompactWorklist &&
        prepared.value().compactWorklistBinding.enabled ? 1u : 0u;
    nativeInvocation.dynamicData.reserve(
        frame.leafDynamicData.size() + enabledRouteCount + enabledWorklistCount);
    for (auto& item : frame.leafDynamicData)
    {
        const auto owned = std::any_of(
            prepared.value().bindings.begin(), prepared.value().bindings.end(),
            [&](const auto& binding) {
                return item.leaf == binding.leaf && item.slot == binding.slot;
            });
        const auto worklistOwned = prepared.value().hasCompactWorklist &&
            item.leaf == prepared.value().compactWorklistBinding.leaf &&
            item.slot == prepared.value().compactWorklistBinding.slot;
        if (owned || worklistOwned)
            return Fail<Submission>("D3D12Runtime/DynamicExecution",
                "Verified Dynamic routeまたはcompact worklistをFrameInputから上書きできません。");
        nativeInvocation.dynamicData.push_back({item.leaf, item.slot, std::move(item.bytes)});
    }
    for (const auto& binding : prepared.value().bindings)
        if (binding.enabled)
            nativeInvocation.dynamicData.push_back({
                binding.leaf, binding.slot, binding.denseSlotBytes});
    if (prepared.value().hasCompactWorklist &&
        prepared.value().compactWorklistBinding.enabled)
        nativeInvocation.dynamicData.push_back({
            prepared.value().compactWorklistBinding.leaf,
            prepared.value().compactWorklistBinding.slot,
            prepared.value().compactWorklistBinding.denseSlotBytes});
    if (prepared.value().hasIndirectDispatch)
        nativeInvocation.indirectDispatches.push_back({
            prepared.value().indirectLeaf,
            prepared.value().indirectComputeCommand,
            prepared.value().indirectWorkCount,
            prepared.value().indirectThreadGroupCountX,
            prepared.value().indirectThreadGroupCountY,
            prepared.value().indirectThreadGroupCountZ});

    auto nativeSubmission = native::SubmitStaticComposition(
        loaded.impl_->nativeRuntime, nativeInvocation);
    if (!nativeSubmission)
        return Fail<Submission>(nativeSubmission.error().stage, nativeSubmission.error().message);
    if (nativeSubmission.value().deviceEpoch != loaded.impl_->session.DeviceEpoch())
        return Fail<Submission>("D3D12Runtime", "Deviceが検証または実行の契約に違反しています。");

    const auto verifiedTransitionCount = prepared.value().verifiedTransitionCount;
    std::uint64_t verifiedDynamicByteCount = 0;
    std::uint32_t verifiedDynamicRouteCount = 0;
    for (const auto& binding : prepared.value().bindings)
    {
        if (!binding.enabled) continue;
        ++verifiedDynamicRouteCount;
        verifiedDynamicByteCount += binding.denseSlotBytes.size();
    }
    const auto verifiedConditionalRegionCount = prepared.value().conditionalRegionCount;
    const auto verifiedIndirectDispatchCount = prepared.value().hasIndirectDispatch ? 1u : 0u;
    const auto verifiedIndirectWorkCount = prepared.value().indirectWorkCount;
    const auto verifiedCompactWorklistBindingCount =
        prepared.value().hasCompactWorklist ? 1u : 0u;
    const auto verifiedCompactWorklistIndexCount =
        prepared.value().compactWorklistCount;
    loaded.impl_->session.CommitSubmission(invocation, std::move(prepared).value());
    Submission result{std::move(invocation), nativeSubmission.value().deviceEpoch,
        static_cast<std::uint32_t>(nativeSubmission.value().leaves.size()),
        verifiedTransitionCount, verifiedDynamicRouteCount,
        verifiedDynamicByteCount, verifiedConditionalRegionCount,
        verifiedIndirectDispatchCount, verifiedIndirectWorkCount,
        verifiedCompactWorklistBindingCount,
        verifiedCompactWorklistIndexCount};
    return base::Success<Submission, Error>(std::move(result));
}

base::Expected<RecoveryReport, Error> Recover(
    LoadedComposition& loaded,
    runtime::DeviceRecoveryMode mode)
{
    const auto previousEpoch = loaded.impl_->nativeRuntime.DeviceEpoch();
    auto nativeReport = native::RecoverStaticComposition(loaded.impl_->nativeRuntime, mode);
    if (!nativeReport)
        return Fail<RecoveryReport>(nativeReport.error().stage, nativeReport.error().message);

    const auto newEpoch = loaded.impl_->nativeRuntime.DeviceEpoch();
    const auto state = loaded.impl_->nativeRuntime.State();
    const bool awaitingAdapter = state == runtime::DeviceRuntimeState::AwaitingAdapter;
    const bool active = state == runtime::DeviceRuntimeState::Active;
    const bool rematerialized = nativeReport.value().allRuntimeObjectsRematerialized;
    if (active && rematerialized && newEpoch <= previousEpoch)
        return Fail<RecoveryReport>("D3D12Recovery", "Deviceが検証または実行の契約に違反しています。");
    if (!active && !awaitingAdapter)
        return Fail<RecoveryReport>("D3D12Recovery", "Runtimeが検証または実行の契約に違反しています。");

    loaded.impl_->session.ApplyRecoveryState(newEpoch, state, active && rematerialized);

    RecoveryReport report;
    report.identity = ComputeRecoveryIdentity(loaded.impl_->session.Package(), previousEpoch, newEpoch, mode);
    report.previousEpoch = previousEpoch;
    report.newEpoch = newEpoch;
    report.allHandlesInvalidated = true;
    report.externalRebindRequired = active && rematerialized;
    report.recoverySeedRequired = active && rematerialized;
    report.awaitingAdapter = awaitingAdapter;
    report.adapterReacquired = nativeReport.value().device.adapterReacquired;
    return base::Success<RecoveryReport, Error>(std::move(report));
}

base::Expected<void, Error> AcknowledgeExternalRebind(LoadedComposition& loaded)
{
    return loaded.impl_->session.AcknowledgeExternalRebind();
}

base::Expected<BufferReadback, Error> ReadBuffer(
    LoadedComposition& loaded,
    composition::ResourceFlowId resource)
{
    auto readback = native::ReadStaticCompositionBuffer(loaded.impl_->nativeRuntime, resource);
    if (!readback)
        return Fail<BufferReadback>(readback.error().stage, readback.error().message);
    BufferReadback result{std::move(readback.value().bytes)};
    return base::Success<BufferReadback, Error>(std::move(result));
}

base::Expected<Texture2DReadback, Error> ReadTexture2D(
    LoadedComposition& loaded,
    composition::ResourceFlowId resource)
{
    auto readback = native::ReadStaticCompositionTexture2D(loaded.impl_->nativeRuntime, resource);
    if (!readback)
        return Fail<Texture2DReadback>(readback.error().stage, readback.error().message);
    Texture2DReadback result;
    result.bytes = std::move(readback.value().bytes);
    result.width = readback.value().width;
    result.height = readback.value().height;
    result.rowBytes = readback.value().rowBytes;
    result.format = readback.value().format;
    return base::Success<Texture2DReadback, Error>(std::move(result));
}

bool ValidateHandleEpoch(
    const LoadedComposition& loaded,
    const canonical::RepresentationHandleV1& handle) noexcept
{
    return loaded.impl_->session.ValidateHandle(handle);
}

bool ValidateHandleEpoch(
    const LoadedComposition& loaded,
    const canonical::HistoryHandleV1& handle) noexcept
{
    return loaded.impl_->session.ValidateHandle(handle);
}
}
