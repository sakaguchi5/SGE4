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

[[nodiscard]] base::Expected<std::vector<std::vector<std::byte>>, Error>
BuildInitialShadows(const composition::DynamicContractV1& contract)
{
    if (contract.executionMode == composition::DynamicExecutionModeV1::AuthorityOnly)
    {
        if (contract.canonicalMemberBytes != 0 || !contract.executionRoutes.empty())
            return Fail<std::vector<std::vector<std::byte>>>(
                "RuntimeSession/DynamicExecution",
                "Authority-only execution contractへrouteが混入しています。");
        return base::Success<std::vector<std::vector<std::byte>>, Error>({});
    }
    if (contract.executionMode != composition::DynamicExecutionModeV1::VerifiedDenseSlot ||
        contract.universeCount == 0 || contract.canonicalMemberBytes == 0 ||
        contract.executionRoutes.empty())
        return Fail<std::vector<std::vector<std::byte>>>(
            "RuntimeSession/DynamicExecution",
            "Dynamic execution contractが検証または実行の契約に違反しています。");

    std::vector<std::vector<std::byte>> shadows;
    shadows.reserve(contract.executionRoutes.size());
    for (const auto& route : contract.executionRoutes)
    {
        if (route.routeMemberBytes == 0 ||
            contract.universeCount >
                std::numeric_limits<std::size_t>::max() / route.routeMemberBytes)
            return Fail<std::vector<std::vector<std::byte>>>(
                "RuntimeSession/DynamicExecution",
                "Dynamic execution shadowのbyte数が表現可能範囲を超えています。");
        shadows.emplace_back(
            static_cast<std::size_t>(contract.universeCount) * route.routeMemberBytes,
            std::byte{0});
    }
    return base::Success<std::vector<std::vector<std::byte>>, Error>(std::move(shadows));
}

[[nodiscard]] base::Expected<void, Error> ValidateIndirectDispatch(
    const composition::FrozenCompositionPackage& package,
    const dynamic::FrozenDynamicInvocationPackage& invocation,
    std::span<const composition::LeafPackageId> enabledLeaves)
{
    const auto& contract = package.DynamicContract().indirectDispatch;
    const auto& dispatch = invocation.IndirectDispatch();

    if (dispatch.mode != contract.mode ||
        dispatch.targetLeaf != contract.targetLeaf ||
        dispatch.targetComputeCommand != contract.targetComputeCommand ||
        dispatch.maxWorkCount != contract.maxWorkCount)
        return Fail<void>("RuntimeSession/IndirectDispatch",
            "Frozen Invocationのindirect dispatch routeがComposition契約と一致しません。");
    if (dispatch.identity != invocation.Artifact().IndirectDispatchIdentityValue() ||
        dynamic::ComputeIndirectDispatchIdentityV1(dispatch) != dispatch.identity)
        return Fail<void>("RuntimeSession/IndirectDispatch",
            "Frozen Invocationのindirect dispatch identityがSeal済み成果物と一致しません。");

    if (contract.mode == composition::IndirectExecutionModeV1::None)
    {
        if (dispatch.targetLeaf.value != package::InvalidIndex ||
            dispatch.targetComputeCommand != package::InvalidIndex ||
            dispatch.maxWorkCount != 0 || dispatch.workCount != 0 ||
            dispatch.threadGroupCountX != 0 ||
            dispatch.threadGroupCountY != 1 || dispatch.threadGroupCountZ != 1)
            return Fail<void>("RuntimeSession/IndirectDispatch",
                "Indirect dispatchなしの成果物に実行引数が含まれています。");
        return base::Success<void, Error>();
    }

    if (contract.mode != composition::IndirectExecutionModeV1::VerifiedDispatch ||
        contract.targetLeaf.value >= package.Certificate().leafCount ||
        contract.targetComputeCommand == package::InvalidIndex ||
        contract.maxWorkCount == 0 ||
        dispatch.workCount > contract.maxWorkCount ||
        dispatch.workCount != invocation.Decision().indirectWorkCount.value() ||
        dispatch.threadGroupCountX != dispatch.workCount ||
        dispatch.threadGroupCountY != 1 || dispatch.threadGroupCountZ != 1)
        return Fail<void>("RuntimeSession/IndirectDispatch",
            "Verified indirect dispatch引数がCompositionまたはDynamic Decisionと一致しません。");

    const auto selected = std::binary_search(
        enabledLeaves.begin(), enabledLeaves.end(), contract.targetLeaf,
        [](const auto& left, const auto& right) { return left.value < right.value; });
    if (!selected)
        return Fail<void>("RuntimeSession/IndirectDispatch",
            "Verified indirect dispatchの対象LeafがSeal済みenabled集合に含まれていません。");
    return base::Success<void, Error>();
}

[[nodiscard]] base::Expected<std::vector<composition::LeafPackageId>, Error>
ValidateConditionalExecution(
    const composition::FrozenCompositionPackage& package,
    const dynamic::DynamicDecisionV1& decision)
{
    const auto& regions = package.DynamicContract().conditionalRegions;
    const auto leafCount = package.Certificate().leafCount;
    if (decision.conditionalSelections.size() != regions.size())
        return Fail<std::vector<composition::LeafPackageId>>(
            "RuntimeSession/ConditionalRegion",
            "Frozen InvocationのConditional selection数がComposition契約と一致しません。");

    std::vector<bool> enabled(leafCount, true);
    for (std::size_t index = 0; index < regions.size(); ++index)
    {
        const auto& region = regions[index];
        const auto& selection = decision.conditionalSelections[index];
        if (selection.region != region.id)
            return Fail<std::vector<composition::LeafPackageId>>(
                "RuntimeSession/ConditionalRegion",
                "Conditional Region identityがComposition契約と一致しません。");
        for (const auto leaf : region.trueLeaves)
        {
            if (leaf.value >= leafCount)
                return Fail<std::vector<composition::LeafPackageId>>(
                    "RuntimeSession/ConditionalRegion", "Conditional leafが範囲外です。");
            enabled[leaf.value] = false;
        }
        for (const auto leaf : region.falseLeaves)
        {
            if (leaf.value >= leafCount)
                return Fail<std::vector<composition::LeafPackageId>>(
                    "RuntimeSession/ConditionalRegion", "Conditional leafが範囲外です。");
            enabled[leaf.value] = false;
        }
        const auto& selected = selection.predicateValue ? region.trueLeaves : region.falseLeaves;
        for (const auto leaf : selected) enabled[leaf.value] = true;
    }

    std::vector<composition::LeafPackageId> expected;
    expected.reserve(leafCount);
    for (std::uint32_t leaf = 0; leaf < leafCount; ++leaf)
        if (enabled[leaf]) expected.push_back(composition::LeafPackageId{leaf});

    if (expected != decision.enabledLeaves)
        return Fail<std::vector<composition::LeafPackageId>>(
            "RuntimeSession/ConditionalRegion",
            "Seal済みenabled Leaf集合がComposition契約の選択結果と一致しません。");
    const auto identity = dynamic::ComputeConditionalExecutionIdentityV1(
        leafCount, decision.conditionalSelections, expected);
    if (identity != decision.conditionalExecutionIdentity)
        return Fail<std::vector<composition::LeafPackageId>>(
            "RuntimeSession/ConditionalRegion",
            "Conditional execution identityがSeal済みDecisionと一致しません。");
    return base::Success<std::vector<composition::LeafPackageId>, Error>(std::move(expected));
}
}

base::Expected<Session, Error> Session::Create(
    composition::FrozenCompositionPackage package,
    std::uint64_t deviceEpoch)
{
    const auto epoch = canonical::DeviceEpoch::TryCreate(deviceEpoch);
    if (!epoch)
        return Fail<Session>("RuntimeSession", "Deviceが検証または実行の契約に違反しています。");
    auto shadows = BuildInitialShadows(package.DynamicContract());
    if (!shadows)
        return Fail<Session>(shadows.error().stage, shadows.error().message);
    auto representationHandle = MakeRepresentationHandle(package, *epoch, 1);
    auto historyHandle = MakeHistoryHandle(package, *epoch, 1);
    return base::Success<Session, Error>(Session(
        std::move(package), *epoch,
        std::move(representationHandle), std::move(historyHandle),
        std::move(shadows).value()));
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
    if (payload.mode != contract.executionMode ||
        payload.canonicalMemberBytes != contract.canonicalMemberBytes ||
        payload.routes != contract.executionRoutes)
        return Fail<void>("RuntimeSession/DynamicExecution",
            "Frozen Invocationのexecution routesがComposition契約と一致しません。");
    if (payload.identity != invocation.Artifact().ExecutionPayloadIdentity())
        return Fail<void>("RuntimeSession/DynamicExecution",
            "Frozen Invocationのexecution payload identityが一致しません。");
    const auto computedPayloadIdentity = dynamic::ComputeDynamicExecutionPayloadIdentityV1(
        payload.mode, payload.canonicalMemberBytes, payload.routes, payload.updates);
    if (computedPayloadIdentity != payload.identity)
        return Fail<void>("RuntimeSession/DynamicExecution",
            "Frozen Invocationのexecution payloadがSealされたidentityと一致しません。");
    if (invocation.Decision().indirectWorkCount.value() !=
        invocation.Decision().transitionRecords.size())
        return Fail<void>("RuntimeSession/DynamicExecution",
            "Verified transition countがtransition record数と一致しません。");
    auto enabledLeaves = ValidateConditionalExecution(package_, invocation.Decision());
    if (!enabledLeaves)
        return Fail<void>(enabledLeaves.error().stage, enabledLeaves.error().message);
    auto indirectDispatch = ValidateIndirectDispatch(
        package_, invocation, enabledLeaves.value());
    if (!indirectDispatch)
        return Fail<void>(indirectDispatch.error().stage, indirectDispatch.error().message);

    if (contract.executionMode == composition::DynamicExecutionModeV1::AuthorityOnly)
    {
        if (!payload.updates.empty())
            return Fail<void>("RuntimeSession/DynamicExecution",
                "Authority-only Invocationへexecution payloadを適用できません。");
    }
    else if (contract.executionMode == composition::DynamicExecutionModeV1::VerifiedDenseSlot)
    {
        auto expectedShadows = BuildInitialShadows(contract);
        if (!expectedShadows ||
            dynamicExecutionShadows_.size() != expectedShadows.value().size())
            return Fail<void>("RuntimeSession/DynamicExecution",
                "Dynamic execution shadow集合がComposition契約と一致しません。");
        for (std::size_t index = 0; index < dynamicExecutionShadows_.size(); ++index)
            if (dynamicExecutionShadows_[index].size() != expectedShadows.value()[index].size())
                return Fail<void>("RuntimeSession/DynamicExecution",
                    "Dynamic execution shadow byte数がroute契約と一致しません。");
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
    auto enabledLeaves = ValidateConditionalExecution(package_, invocation.Decision());
    if (!enabledLeaves)
        return Fail<PreparedDynamicExecutionV1>(
            enabledLeaves.error().stage, enabledLeaves.error().message);

    PreparedDynamicExecutionV1 prepared;
    prepared.enabledLeaves = std::move(enabledLeaves).value();
    prepared.conditionalRegionCount = static_cast<std::uint32_t>(
        contract.conditionalRegions.size());
    prepared.verifiedTransitionCount = invocation.Decision().indirectWorkCount.value();

    const auto& indirect = invocation.IndirectDispatch();
    if (indirect.mode == composition::IndirectExecutionModeV1::VerifiedDispatch)
    {
        prepared.hasIndirectDispatch = true;
        prepared.indirectLeaf = indirect.targetLeaf;
        prepared.indirectComputeCommand = indirect.targetComputeCommand;
        prepared.indirectWorkCount = indirect.workCount;
        prepared.indirectThreadGroupCountX = indirect.threadGroupCountX;
        prepared.indirectThreadGroupCountY = indirect.threadGroupCountY;
        prepared.indirectThreadGroupCountZ = indirect.threadGroupCountZ;
    }

    if (contract.executionMode == composition::DynamicExecutionModeV1::AuthorityOnly)
        return base::Success<PreparedDynamicExecutionV1, Error>(std::move(prepared));
    if (contract.executionMode != composition::DynamicExecutionModeV1::VerifiedDenseSlot ||
        dynamicExecutionShadows_.size() != contract.executionRoutes.size())
        return Fail<PreparedDynamicExecutionV1>("RuntimeSession/DynamicExecution",
            "未対応または不整合なDynamic execution route集合です。");

    prepared.bindings.reserve(contract.executionRoutes.size());
    for (std::size_t index = 0; index < contract.executionRoutes.size(); ++index)
    {
        const auto& route = contract.executionRoutes[index];
        PreparedDynamicBindingV1 binding;
        binding.leaf = route.targetLeaf;
        binding.slot = route.targetDynamicSlot;
        binding.denseSlotBytes = dynamicExecutionShadows_[index];
        binding.enabled = std::binary_search(
            prepared.enabledLeaves.begin(), prepared.enabledLeaves.end(), route.targetLeaf,
            [](const auto& left, const auto& right) { return left.value < right.value; });
        prepared.bindings.push_back(std::move(binding));
    }

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

        const dynamic::MemberUpdatePayloadV1* updatePayload = nullptr;
        if (record.action == dynamic::TransitionActionV1::Update)
        {
            if (updateCursor >= payloads.size() ||
                payloads[updateCursor].member.value() != member ||
                payloads[updateCursor].bytes.size() != contract.canonicalMemberBytes)
                return Fail<PreparedDynamicExecutionV1>("RuntimeSession/DynamicExecution",
                    "Update transitionとCanonical execution payloadが一対一に対応していません。");
            updatePayload = &payloads[updateCursor++];
        }
        else if (record.action != dynamic::TransitionActionV1::Clear)
        {
            return Fail<PreparedDynamicExecutionV1>("RuntimeSession/DynamicExecution",
                "未対応のDynamic transition actionです。");
        }

        for (std::size_t routeIndex = 0;
            routeIndex < contract.executionRoutes.size(); ++routeIndex)
        {
            const auto& route = contract.executionRoutes[routeIndex];
            const auto destinationOffset =
                static_cast<std::size_t>(member) * route.routeMemberBytes;
            auto destination = std::span<std::byte>(
                prepared.bindings[routeIndex].denseSlotBytes).subspan(
                    destinationOffset, route.routeMemberBytes);
            if (updatePayload)
            {
                const auto source = std::span<const std::byte>(updatePayload->bytes).subspan(
                    route.sourceByteOffset, route.routeMemberBytes);
                std::copy(source.begin(), source.end(), destination.begin());
            }
            else
            {
                std::fill(destination.begin(), destination.end(), std::byte{0});
            }
        }
        ++prepared.appliedTransitionCount;
    }

    if (updateCursor != payloads.size() ||
        prepared.appliedTransitionCount != decision.indirectWorkCount.value())
        return Fail<PreparedDynamicExecutionV1>("RuntimeSession/DynamicExecution",
            "Verified routed payloadの全要素がexact transitionへ対応していません。");

    return base::Success<PreparedDynamicExecutionV1, Error>(std::move(prepared));
}

void Session::CommitSubmission(
    const dynamic::FrozenDynamicInvocationPackage& invocation,
    PreparedDynamicExecutionV1 prepared)
{
    history_ = invocation.NextHistory();
    recoverySeedRequired_ = false;

    if (package_.DynamicContract().executionMode ==
        composition::DynamicExecutionModeV1::VerifiedDenseSlot)
    {
        dynamicExecutionShadows_.clear();
        dynamicExecutionShadows_.reserve(prepared.bindings.size());
        for (auto& binding : prepared.bindings)
            dynamicExecutionShadows_.push_back(std::move(binding.denseSlotBytes));
    }
}

void Session::RebuildHandles()
{
    representationHandle_ = MakeRepresentationHandle(package_, deviceEpoch_, runtimeGeneration_);
    historyHandle_ = MakeHistoryHandle(package_, deviceEpoch_, runtimeGeneration_);
}

void Session::ResetDynamicExecutionShadows()
{
    for (auto& shadow : dynamicExecutionShadows_)
        std::fill(shadow.begin(), shadow.end(), std::byte{0});
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
    ResetDynamicExecutionShadows();
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
