#include "RuntimeSession.h"

#include "../../canonical/base/BinaryIO.h"

#include <stdexcept>
#include <string_view>
#include <utility>

namespace sge4::runtime
{
namespace
{
template<class T>
[[nodiscard]] base::Expected<T, Error> Fail(std::string stage, std::string message)
{
    return base::Failure<T, Error>({std::move(stage), std::move(message)});
}

[[nodiscard]] canonical::ResourceInstanceIdentity MakeResourceIdentity(
    const composition::FrozenCompositionPackage& package,
    canonical::DeviceEpoch epoch,
    std::uint64_t generation,
    std::string_view domain)
{
    base::BinaryWriter writer;
    writer.WriteBytes(package.Certificate().artifactIdentity.Digest());
    writer.WriteU64(epoch.value());
    writer.WriteU64(generation);
    return canonical::ResourceInstanceIdentity::FromDigest(
        ComputeDomainDigest(domain, 1, writer.Bytes()));
}

[[nodiscard]] canonical::RepresentationHandleV1 MakeRepresentationHandle(
    const composition::FrozenCompositionPackage& package,
    canonical::DeviceEpoch epoch,
    std::uint64_t generation)
{
    return canonical::RepresentationHandleV1(
        MakeResourceIdentity(package, epoch, generation, "sge4.runtime.representation"), epoch);
}

[[nodiscard]] canonical::HistoryHandleV1 MakeHistoryHandle(
    const composition::FrozenCompositionPackage& package,
    canonical::DeviceEpoch epoch,
    std::uint64_t generation)
{
    return canonical::HistoryHandleV1(
        MakeResourceIdentity(package, epoch, generation, "sge4.runtime.history"), epoch);
}
}

base::Expected<Session, Error> Session::Create(
    composition::FrozenCompositionPackage package,
    std::uint64_t deviceEpoch)
{
    const auto epoch = canonical::DeviceEpoch::TryCreate(deviceEpoch);
    if (!epoch)
        return Fail<Session>("RuntimeSession", "Deviceが検証または実行の契約に違反しています。");
    auto representationHandle = MakeRepresentationHandle(package, *epoch, 1);
    auto historyHandle = MakeHistoryHandle(package, *epoch, 1);
    return base::Success<Session, Error>(Session(
        std::move(package), *epoch,
        std::move(representationHandle), std::move(historyHandle)));
}

std::optional<canonical::HistoryValidityIdentity> Session::AcceptedHistoryIdentity() const noexcept
{
    return history_ ? std::optional(history_->Descriptor().identity) : std::nullopt;
}

dynamic::InvocationModeV1 Session::RequiredMode() const noexcept
{
    if (recoverySeedRequired_)
        return dynamic::InvocationModeV1::RecoverySeed;
    return history_ ? dynamic::InvocationModeV1::ContinueHistory
                    : dynamic::InvocationModeV1::InitialSeed;
}

DynamicPlanningContext Session::PlanningContext() const
{
    DynamicPlanningContext context{deviceEpoch_, RequiredMode(), std::nullopt};
    if (context.requiredMode == dynamic::InvocationModeV1::ContinueHistory)
        context.previousHistory = history_;
    return context;
}

base::Expected<void, Error> Session::ValidateForSubmission(
    const dynamic::FrozenDynamicInvocationPackage& invocation) const
{
    if (state_ != DeviceRuntimeState::Active)
        return Fail<void>("RuntimeSession", "Runtimeが検証または実行の契約に違反しています。");
    if (!externalStateBound_)
        return Fail<void>("RuntimeSession", "Stateの状態または世代が実行契約と一致しません。");
    if (invocation.Mode() != RequiredMode())
        return Fail<void>("RuntimeSession", "InvocationがCanonicalな順序または識別子規則に違反しています。");
    if (invocation.Artifact().CompositionIdentity() != package_.Certificate().artifactIdentity)
        return Fail<void>("RuntimeSession", "Compositionが検証または実行の契約に違反しています。");
    if (invocation.NextHistory().Descriptor().deviceEpoch != deviceEpoch_)
        return Fail<void>("RuntimeSession", "Invocationが検証または実行の契約に違反しています。");
    if (invocation.NextHistory().Universe().value() != package_.DynamicContract().universeCount)
        return Fail<void>("RuntimeSession", "CompositionがCanonicalな順序または識別子規則に違反しています。");

    if (history_)
    {
        if (invocation.Mode() != dynamic::InvocationModeV1::ContinueHistory)
            return Fail<void>("RuntimeSession", "Historyが検証または実行の契約に違反しています。");
        if (!invocation.Artifact().PreviousHistoryIdentity().has_value() ||
            *invocation.Artifact().PreviousHistoryIdentity() != history_->Descriptor().identity)
            return Fail<void>("RuntimeSession", "Invocationが検証または実行の契約に違反しています。");
        if (invocation.NextHistory().Descriptor().generation.value() !=
            history_->Descriptor().generation.value() + 1)
            return Fail<void>("RuntimeSession", "Invocationが検証または実行の契約に違反しています。");
    }
    else
    {
        if (invocation.Artifact().PreviousHistoryIdentity().has_value())
            return Fail<void>("RuntimeSession", "Invocationが検証または実行の契約に違反しています。");
        if (invocation.NextHistory().Descriptor().generation.value() != 1)
            return Fail<void>("RuntimeSession", "Invocationが検証または実行の契約に違反しています。");
    }
    return base::Success<void, Error>();
}

void Session::CommitSubmission(const dynamic::FrozenDynamicInvocationPackage& invocation)
{
    history_ = invocation.NextHistory();
    recoverySeedRequired_ = false;
}

void Session::RebuildHandles()
{
    representationHandle_ = MakeRepresentationHandle(package_, deviceEpoch_, runtimeGeneration_);
    historyHandle_ = MakeHistoryHandle(package_, deviceEpoch_, runtimeGeneration_);
}

void Session::ApplyRecoveryState(
    std::uint64_t newDeviceEpoch,
    DeviceRuntimeState state,
    bool rematerialized)
{
    const auto epoch = canonical::DeviceEpoch::TryCreate(newDeviceEpoch);
    if (!epoch)
        throw std::invalid_argument("Runtimeが検証または実行の契約に違反しています。");
    deviceEpoch_ = *epoch;
    state_ = state;
    ++runtimeGeneration_;
    history_.reset();
    externalStateBound_ = false;
    recoverySeedRequired_ = rematerialized || state == DeviceRuntimeState::AwaitingAdapter;
    RebuildHandles();
}

base::Expected<void, Error> Session::AcknowledgeExternalRebind()
{
    if (state_ != DeviceRuntimeState::Active)
        return Fail<void>("RuntimeSession", "Adapterが検証または実行の契約に違反しています。");
    if (!recoverySeedRequired_)
        return Fail<void>("RuntimeSession", "入力または内部状態が検証または実行の契約に違反しています。");
    externalStateBound_ = true;
    return base::Success<void, Error>();
}

bool Session::ValidateHandle(const canonical::RepresentationHandleV1& handle) const noexcept
{
    return handle.Epoch() == deviceEpoch_ &&
        handle.ResourceInstance() == representationHandle_.ResourceInstance();
}

bool Session::ValidateHandle(const canonical::HistoryHandleV1& handle) const noexcept
{
    return handle.Epoch() == deviceEpoch_ &&
        handle.ResourceInstance() == historyHandle_.ResourceInstance();
}
}
