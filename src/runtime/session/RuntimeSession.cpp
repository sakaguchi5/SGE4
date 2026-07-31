#include "RuntimeSession.h"

#include "../../canonical/base/BinaryIO.h"

#include <algorithm>
#include <limits>
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

[[nodiscard]] base::Expected<std::size_t, Error> DenseShadowBytes(
    const composition::DynamicContractV1& contract)
{
    if (contract.executionMode == composition::DynamicExecutionModeV1::AuthorityOnly)
        return base::Success<std::size_t, Error>(0u);
    if (contract.executionMode != composition::DynamicExecutionModeV1::VerifiedDenseSlot ||
        contract.universeCount == 0 || contract.memberBytes == 0 ||
        contract.targetLeaf.value == package::InvalidIndex ||
        contract.targetDynamicSlot == package::InvalidIndex)
        return Fail<std::size_t>("RuntimeSession/DynamicExecution",
            "Dynamic execution contractが検証または実行の契約に違反しています。");
    if (contract.universeCount > std::numeric_limits<std::size_t>::max() / contract.memberBytes)
        return Fail<std::size_t>("RuntimeSession/DynamicExecution",
            "Dynamic execution shadowのbyte数が表現可能範囲を超えています。");
    return base::Success<std::size_t, Error>(
        static_cast<std::size_t>(contract.universeCount) * contract.memberBytes);
}
}

base::Expected<Session, Error> Session::Create(
    composition::FrozenCompositionPackage package,
    std::uint64_t deviceEpoch)
{
    const auto epoch = canonical::DeviceEpoch::TryCreate(deviceEpoch);
    if (!epoch)
        return Fail<Session>("RuntimeSession", "Deviceが検証または実行の契約に違反しています。");
    auto shadowBytes = DenseShadowBytes(package.DynamicContract());
    if (!shadowBytes)
        return Fail<Session>(shadowBytes.error().stage, shadowBytes.error().message);
    auto representationHandle = MakeRepresentationHandle(package, *epoch, 1);
    auto historyHandle = MakeHistoryHandle(package, *epoch, 1);
    return base::Success<Session, Error>(Session(
        std::move(package), *epoch,
        std::move(representationHandle), std::move(historyHandle),
        std::vector<std::byte>(shadowBytes.value(), std::byte{0})));
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

    const auto& contract = package_.DynamicContract();
    const auto& payload = invocation.ExecutionPayload();
    if (payload.mode != contract.executionMode || payload.targetLeaf != contract.targetLeaf ||
        payload.targetDynamicSlot != contract.targetDynamicSlot ||
        payload.memberBytes != contract.memberBytes)
        return Fail<void>("RuntimeSession/DynamicExecution",
            "Frozen Invocationのexecution routeがComposition契約と一致しません。");
    if (payload.identity != invocation.Artifact().ExecutionPayloadIdentity())
        return Fail<void>("RuntimeSession/DynamicExecution",
            "Frozen Invocationのexecution payload identityが一致しません。");
    const auto computedPayloadIdentity = dynamic::ComputeDynamicExecutionPayloadIdentityV1(
        payload.mode, payload.targetLeaf, payload.targetDynamicSlot,
        payload.memberBytes, payload.updates);
    if (computedPayloadIdentity != payload.identity)
        return Fail<void>("RuntimeSession/DynamicExecution",
            "Frozen Invocationのexecution payloadがSealされたidentityと一致しません。");
    if (invocation.Decision().indirectWorkCount.value() !=
        invocation.Decision().transitionRecords.size())
        return Fail<void>("RuntimeSession/DynamicExecution",
            "Verified transition countがtransition record数と一致しません。");

    if (contract.executionMode == composition::DynamicExecutionModeV1::AuthorityOnly)
    {
        if (!payload.updates.empty())
            return Fail<void>("RuntimeSession/DynamicExecution",
                "Authority-only Invocationへexecution payloadを適用できません。");
    }
    else if (contract.executionMode == composition::DynamicExecutionModeV1::VerifiedDenseSlot)
    {
        auto expectedShadowBytes = DenseShadowBytes(contract);
        if (!expectedShadowBytes || dynamicExecutionShadow_.size() != expectedShadowBytes.value())
            return Fail<void>("RuntimeSession/DynamicExecution",
                "Dynamic execution shadowがComposition契約と一致しません。");
    }
    else
    {
        return Fail<void>("RuntimeSession/DynamicExecution",
            "未対応のDynamic execution modeです。");
    }

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

base::Expected<PreparedDynamicExecutionV1, Error> Session::PrepareDynamicExecution(
    const dynamic::FrozenDynamicInvocationPackage& invocation) const
{
    const auto& contract = package_.DynamicContract();
    if (contract.executionMode == composition::DynamicExecutionModeV1::AuthorityOnly)
        return base::Success<PreparedDynamicExecutionV1, Error>({});
    if (contract.executionMode != composition::DynamicExecutionModeV1::VerifiedDenseSlot)
        return Fail<PreparedDynamicExecutionV1>("RuntimeSession/DynamicExecution",
            "未対応のDynamic execution modeです。");

    PreparedDynamicExecutionV1 prepared;
    prepared.hasBinding = true;
    prepared.leaf = contract.targetLeaf;
    prepared.slot = contract.targetDynamicSlot;
    prepared.denseSlotBytes = dynamicExecutionShadow_;

    const auto& decision = invocation.Decision();
    const auto& payloads = invocation.ExecutionPayload().updates;
    std::size_t updateCursor = 0;
    std::uint32_t previousMember = package::InvalidIndex;
    bool hasPreviousMember = false;

    for (const auto& record : decision.transitionRecords)
    {
        const auto member = record.member.value();
        if (member >= contract.universeCount ||
            (hasPreviousMember && member <= previousMember))
            return Fail<PreparedDynamicExecutionV1>("RuntimeSession/DynamicExecution",
                "Transition recordがCanonicalなmember順序に違反しています。");
        previousMember = member;
        hasPreviousMember = true;

        const auto offset = static_cast<std::size_t>(member) * contract.memberBytes;
        auto destination = std::span<std::byte>(prepared.denseSlotBytes).subspan(
            offset, contract.memberBytes);
        if (record.action == dynamic::TransitionActionV1::Update)
        {
            if (updateCursor >= payloads.size() ||
                payloads[updateCursor].member.value() != member ||
                payloads[updateCursor].bytes.size() != contract.memberBytes)
                return Fail<PreparedDynamicExecutionV1>("RuntimeSession/DynamicExecution",
                    "Update transitionとexecution payloadが一対一に対応していません。");
            std::copy(payloads[updateCursor].bytes.begin(),
                payloads[updateCursor].bytes.end(), destination.begin());
            ++updateCursor;
        }
        else if (record.action == dynamic::TransitionActionV1::Clear)
        {
            std::fill(destination.begin(), destination.end(), std::byte{0});
        }
        else
        {
            return Fail<PreparedDynamicExecutionV1>("RuntimeSession/DynamicExecution",
                "未対応のDynamic transition actionです。");
        }
        ++prepared.appliedTransitionCount;
    }

    if (updateCursor != payloads.size() ||
        prepared.appliedTransitionCount != decision.indirectWorkCount.value())
        return Fail<PreparedDynamicExecutionV1>("RuntimeSession/DynamicExecution",
            "Verified execution payloadの全要素がexact transitionへ対応していません。");

    return base::Success<PreparedDynamicExecutionV1, Error>(std::move(prepared));
}

void Session::CommitSubmission(
    const dynamic::FrozenDynamicInvocationPackage& invocation,
    PreparedDynamicExecutionV1 prepared)
{
    history_ = invocation.NextHistory();
    recoverySeedRequired_ = false;
    if (prepared.hasBinding)
        dynamicExecutionShadow_ = std::move(prepared.denseSlotBytes);
}

void Session::RebuildHandles()
{
    representationHandle_ = MakeRepresentationHandle(package_, deviceEpoch_, runtimeGeneration_);
    historyHandle_ = MakeHistoryHandle(package_, deviceEpoch_, runtimeGeneration_);
}

void Session::ResetDynamicExecutionShadow()
{
    std::fill(dynamicExecutionShadow_.begin(), dynamicExecutionShadow_.end(), std::byte{0});
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
    ResetDynamicExecutionShadow();
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
