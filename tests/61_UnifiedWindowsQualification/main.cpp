#include "../fixtures/UnifiedFixture.h"
#include "../../src/backends/d3d12/runtime/Runtime.h"

#include <array>
#include <cstring>
#include <cmath>
#include <span>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace
{
namespace tests = sge4::tests;
namespace fixture = sge4::qualification::canonical_runtime_fixture;

void Require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

[[nodiscard]] sge4::dynamic::MemberUpdateInputV1 UpdatePayload(
    std::uint32_t member,
    const std::array<float, 4>& value)
{
    return {member, fixture::Bytes(value)};
}

[[nodiscard]] bool EqualsDense(
    std::span<const std::byte> bytes,
    const std::array<std::array<float, 4>, 4>& expected,
    float tolerance = 0.0001f)
{
    if (bytes.size() != sizeof(expected)) return false;
    std::array<std::array<float, 4>, 4> actual{};
    std::memcpy(actual.data(), bytes.data(), bytes.size());
    for (std::size_t member = 0; member < actual.size(); ++member)
        for (std::size_t component = 0; component < actual[member].size(); ++component)
            if (std::abs(actual[member][component] - expected[member][component]) > tolerance)
                return false;
    return true;
}

[[nodiscard]] bool EqualsIndirect(
    std::span<const std::byte> bytes,
    std::uint32_t universe,
    std::uint32_t executedWorkCount,
    float tolerance = 0.0001f)
{
    if (bytes.size() != static_cast<std::size_t>(universe) * sizeof(std::array<float, 4>))
        return false;
    for (std::uint32_t member = 0; member < universe; ++member)
    {
        std::array<float, 4> actual{};
        std::memcpy(actual.data(), bytes.data() + static_cast<std::size_t>(member) * sizeof(actual), sizeof(actual));
        const std::array<float, 4> expected = member < executedWorkCount
            ? std::array<float, 4>{float(member + 1u), float(member + 11u), float(member + 21u), 1.0f}
            : std::array<float, 4>{};
        for (std::size_t component = 0; component < actual.size(); ++component)
            if (std::abs(actual[component] - expected[component]) > tolerance) return false;
    }
    return true;
}

[[nodiscard]] bool EqualsTexture(
    const sge4::d3d12::Texture2DReadback& readback,
    std::uint32_t width,
    std::uint32_t height)
{
    if (readback.width != width || readback.height != height ||
        readback.rowBytes != width * 4u ||
        readback.format != sge4::package::d3d12_v13::Format::B8G8R8A8Unorm ||
        readback.bytes.size() != static_cast<std::size_t>(width) * height * 4u)
        return false;
    for (std::size_t offset = 0; offset < readback.bytes.size(); offset += 4)
    {
        if (readback.bytes[offset + 0] != std::byte{192} ||
            readback.bytes[offset + 1] != std::byte{128} ||
            readback.bytes[offset + 2] != std::byte{64} ||
            readback.bytes[offset + 3] != std::byte{255})
            return false;
    }
    return true;
}

void VerifyIndirectWorkQualification()
{
    constexpr std::uint32_t Universe = 8;
    auto package = tests::BuildVerifiedIndirectUnified(Universe);
    if (!package)
        throw std::runtime_error(
            "Verified Indirect Compositionの生成に失敗しました：" + package.error());
    const auto outputId = fixture::FindResourceFlow(
        package.value().FileBytes(), "unified/indirect/output");
    Require(outputId.IsValid(), "Verified Indirect outputの解決に失敗しました。");

    sge4::d3d12::Executor backend({true, false, false});
    auto loaded = sge4::d3d12::LoadComposition(package.value().FileBytes(), backend);
    Require(static_cast<bool>(loaded), "Verified Indirect CompositionのLoadに失敗しました。");

    auto planning = loaded.value().PlanningContext();
    sge4::dynamic::InvocationInputV1 zeroSeed;
    zeroSeed.timelineOrdinal = 0;
    zeroSeed.mode = planning.requiredMode;
    auto frozenZero = tests::BuildFrozenInvocation(
        loaded.value().Package(), planning.deviceEpoch, std::move(zeroSeed),
        std::move(planning.previousHistory));
    Require(static_cast<bool>(frozenZero), "zero-work InitialSeedの生成に失敗しました。");
    auto zeroSubmission = sge4::d3d12::Submit(
        loaded.value(), std::move(frozenZero).value(), {0, {}});
    Require(zeroSubmission && zeroSubmission.value().submittedLeafCount == 2 &&
        zeroSubmission.value().verifiedIndirectDispatchCount == 1 &&
        zeroSubmission.value().verifiedIndirectWorkCount == 0,
        "zero-work DispatchIndirectがSeal済み件数で実行されませんでした。");
    auto zeroRead = sge4::d3d12::ReadBuffer(loaded.value(), outputId);
    Require(zeroRead && EqualsIndirect(zeroRead.value().bytes, Universe, 0),
        "zero-work DispatchIndirectでGPU出力が変更されました。");

    planning = loaded.value().PlanningContext();
    sge4::dynamic::InvocationInputV1 threeWork;
    threeWork.timelineOrdinal = 1;
    threeWork.mode = planning.requiredMode;
    threeWork.activeMembers = {0, 2, 7};
    auto frozenThree = tests::BuildFrozenInvocation(
        loaded.value().Package(), planning.deviceEpoch, std::move(threeWork),
        std::move(planning.previousHistory));
    Require(static_cast<bool>(frozenThree), "three-work ContinueHistoryの生成に失敗しました。");
    auto threeSubmission = sge4::d3d12::Submit(
        loaded.value(), std::move(frozenThree).value(), {1, {}});
    if (!threeSubmission)
        throw std::runtime_error(
            "work count 3のD3D12 Submitに失敗しました：" +
            threeSubmission.error().stage + "：" + threeSubmission.error().message);
    Require(threeSubmission.value().verifiedTransitionCount == 3 &&
        threeSubmission.value().verifiedIndirectDispatchCount == 1 &&
        threeSubmission.value().verifiedIndirectWorkCount == 3,
        "exact transition count 3のSubmit監査値がDispatchIndirect契約と一致しませんでした。");
    auto threeRead = sge4::d3d12::ReadBuffer(loaded.value(), outputId);
    Require(threeRead && EqualsIndirect(threeRead.value().bytes, Universe, 3),
        "DispatchIndirect work count 3のGPU観測結果が一致しません。");

    planning = loaded.value().PlanningContext();
    sge4::dynamic::InvocationInputV1 retain;
    retain.timelineOrdinal = 2;
    retain.mode = planning.requiredMode;
    retain.activeMembers = {0, 2, 7};
    auto frozenRetain = tests::BuildFrozenInvocation(
        loaded.value().Package(), planning.deviceEpoch, std::move(retain),
        std::move(planning.previousHistory));
    Require(static_cast<bool>(frozenRetain), "zero-transition Retainの生成に失敗しました。");
    auto retainSubmission = sge4::d3d12::Submit(
        loaded.value(), std::move(frozenRetain).value(), {2, {}});
    Require(retainSubmission && retainSubmission.value().verifiedTransitionCount == 0 &&
        retainSubmission.value().verifiedIndirectWorkCount == 0,
        "Retain-only frameがzero-work DispatchIndirectになりませんでした。");
    auto retainRead = sge4::d3d12::ReadBuffer(loaded.value(), outputId);
    Require(retainRead && EqualsIndirect(retainRead.value().bytes, Universe, 3),
        "zero-work Retain frameで既存GPU出力が変化しました。");

    auto recovery = sge4::d3d12::Recover(
        loaded.value(), sge4::runtime::DeviceRecoveryMode::ControlledRebuild);
    Require(recovery && recovery.value().newEpoch > recovery.value().previousEpoch,
        "Verified Indirect Compositionの制御回復に失敗しました。");
    Require(static_cast<bool>(sge4::d3d12::AcknowledgeExternalRebind(loaded.value())),
        "Verified Indirect Compositionの外部再bind確認に失敗しました。");

    planning = loaded.value().PlanningContext();
    sge4::dynamic::InvocationInputV1 recoverySeed;
    recoverySeed.timelineOrdinal = 3;
    recoverySeed.mode = planning.requiredMode;
    recoverySeed.activeMembers = {0, 1};
    auto frozenRecovery = tests::BuildFrozenInvocation(
        loaded.value().Package(), planning.deviceEpoch, std::move(recoverySeed),
        std::move(planning.previousHistory));
    Require(static_cast<bool>(frozenRecovery), "Verified Indirect RecoverySeedの生成に失敗しました。");
    auto resumed = sge4::d3d12::Submit(
        loaded.value(), std::move(frozenRecovery).value(), {3, {}});
    Require(resumed && resumed.value().verifiedIndirectWorkCount == 2,
        "RecoverySeedのwork countがDispatchIndirectへ再構築されませんでした。");
    auto recoveryRead = sge4::d3d12::ReadBuffer(loaded.value(), outputId);
    Require(recoveryRead && EqualsIndirect(recoveryRead.value().bytes, Universe, 2),
        "Recovery後のVerified indirect GPU観測結果が一致しません。");
}

void VerifyLimitedTexture2DFlowQualification()
{
    constexpr std::uint32_t Width = 4;
    constexpr std::uint32_t Height = 4;
    auto package = tests::BuildLimitedTexture2DUnified(Width, Height);
    if (!package)
        throw std::runtime_error(
            "限定Texture2D Flow Compositionの生成に失敗しました：" + package.error());
    const auto outputId = fixture::FindResourceFlow(
        package.value().FileBytes(), "unified/texture/output");
    Require(outputId.IsValid(), "限定Texture2D Flow outputの解決に失敗しました。");

    sge4::d3d12::Executor backend({true, false, false});
    auto loaded = sge4::d3d12::LoadComposition(package.value().FileBytes(), backend);
    Require(static_cast<bool>(loaded), "限定Texture2D Flow CompositionのLoadに失敗しました。");

    auto planning = loaded.value().PlanningContext();
    sge4::dynamic::InvocationInputV1 seed;
    seed.timelineOrdinal = 0;
    seed.mode = planning.requiredMode;
    seed.activeMembers = {0};
    auto frozenSeed = tests::BuildFrozenInvocation(
        loaded.value().Package(), planning.deviceEpoch, std::move(seed),
        std::move(planning.previousHistory));
    Require(static_cast<bool>(frozenSeed), "限定Texture2D Flow InitialSeedの生成に失敗しました。");
    auto submitted = sge4::d3d12::Submit(
        loaded.value(), std::move(frozenSeed).value(), {0, {}});
    Require(submitted && submitted.value().submittedLeafCount == 2,
        "限定Texture2D Flowの2 Leaf実行に失敗しました。");
    auto readback = sge4::d3d12::ReadTexture2D(loaded.value(), outputId);
    Require(readback && EqualsTexture(readback.value(), Width, Height),
        "限定Texture2D Flowのpacked GPU readbackが一致しません。");

    auto recovery = sge4::d3d12::Recover(
        loaded.value(), sge4::runtime::DeviceRecoveryMode::ControlledRebuild);
    Require(recovery && recovery.value().newEpoch > recovery.value().previousEpoch,
        "限定Texture2D Flow Compositionの制御回復に失敗しました。");
    Require(static_cast<bool>(sge4::d3d12::AcknowledgeExternalRebind(loaded.value())),
        "限定Texture2D Flow Compositionの外部再bind確認に失敗しました。");

    planning = loaded.value().PlanningContext();
    sge4::dynamic::InvocationInputV1 recoverySeed;
    recoverySeed.timelineOrdinal = 1;
    recoverySeed.mode = planning.requiredMode;
    recoverySeed.activeMembers = {0};
    auto frozenRecovery = tests::BuildFrozenInvocation(
        loaded.value().Package(), planning.deviceEpoch, std::move(recoverySeed),
        std::move(planning.previousHistory));
    Require(static_cast<bool>(frozenRecovery),
        "限定Texture2D Flow RecoverySeedの生成に失敗しました。");
    auto resumed = sge4::d3d12::Submit(
        loaded.value(), std::move(frozenRecovery).value(), {1, {}});
    Require(resumed && resumed.value().submittedLeafCount == 2,
        "限定Texture2D Flow RecoverySeed後の実行に失敗しました。");
    auto reread = sge4::d3d12::ReadTexture2D(loaded.value(), outputId);
    Require(reread && EqualsTexture(reread.value(), Width, Height),
        "限定Texture2D Flow Recovery後のGPU readbackが一致しません。");
}

void VerifyConditionalRegionQualification()
{
    constexpr std::uint32_t Universe = 4;
    const std::array<float, 4> valueOne{31, 32, 33, 34};
    const std::array<float, 4> valueThree{41, 42, 43, 44};
    const std::array<float, 4> zero{};

    auto package = tests::BuildConditionalVerifiedDynamicUnified(Universe);
    if (!package)
        throw std::runtime_error(
            "Conditional Region Compositionの生成に失敗しました：" + package.error());
    const auto outputId = fixture::FindResourceFlow(
        package.value().FileBytes(), "unified/conditional/output");
    Require(outputId.IsValid(), "Conditional Region outputの解決に失敗しました。");

    sge4::d3d12::Executor backend({true, false, false});
    auto loaded = sge4::d3d12::LoadComposition(package.value().FileBytes(), backend);
    Require(static_cast<bool>(loaded), "Conditional Region CompositionのLoadに失敗しました。");

    auto planning = loaded.value().PlanningContext();
    sge4::dynamic::InvocationInputV1 seed;
    seed.timelineOrdinal = 0;
    seed.mode = planning.requiredMode;
    seed.activeMembers = {1};
    seed.updatePayloads = {UpdatePayload(1, valueOne)};
    auto frozenSeed = tests::BuildFrozenInvocation(
        loaded.value().Package(), planning.deviceEpoch, std::move(seed),
        std::move(planning.previousHistory));
    Require(static_cast<bool>(frozenSeed), "Conditional true seedの生成に失敗しました。");
    auto submittedSeed = sge4::d3d12::Submit(
        loaded.value(), std::move(frozenSeed).value(), {0, {}});
    Require(submittedSeed && submittedSeed.value().submittedLeafCount == 2 &&
        submittedSeed.value().verifiedConditionalRegionCount == 1,
        "Conditional true branchが実行されませんでした。");
    const std::array seedExpected{zero, valueOne, zero, zero};
    auto seedRead = sge4::d3d12::ReadBuffer(loaded.value(), outputId);
    Require(seedRead && EqualsDense(seedRead.value().bytes, seedExpected),
        "Conditional true branchのGPU観測結果が一致しません。");

    planning = loaded.value().PlanningContext();
    sge4::dynamic::InvocationInputV1 disable;
    disable.timelineOrdinal = 1;
    disable.mode = planning.requiredMode;
    auto frozenDisable = tests::BuildFrozenInvocation(
        loaded.value().Package(), planning.deviceEpoch, std::move(disable),
        std::move(planning.previousHistory));
    Require(static_cast<bool>(frozenDisable), "Conditional false Invocationの生成に失敗しました。");
    auto submittedDisable = sge4::d3d12::Submit(
        loaded.value(), std::move(frozenDisable).value(), {1, {}});
    Require(submittedDisable && submittedDisable.value().submittedLeafCount == 0 &&
        submittedDisable.value().verifiedTransitionCount == 1 &&
        submittedDisable.value().verifiedConditionalRegionCount == 1,
        "Conditional false branchがzero-Leaf submissionになりませんでした。");
    auto retainedRead = sge4::d3d12::ReadBuffer(loaded.value(), outputId);
    Require(retainedRead && EqualsDense(retainedRead.value().bytes, seedExpected),
        "未選択RegionのComposition outputが直前の受理状態を保持しませんでした。");

    planning = loaded.value().PlanningContext();
    sge4::dynamic::InvocationInputV1 reenable;
    reenable.timelineOrdinal = 2;
    reenable.mode = planning.requiredMode;
    reenable.activeMembers = {3};
    reenable.updatePayloads = {UpdatePayload(3, valueThree)};
    auto frozenReenable = tests::BuildFrozenInvocation(
        loaded.value().Package(), planning.deviceEpoch, std::move(reenable),
        std::move(planning.previousHistory));
    Require(static_cast<bool>(frozenReenable), "Conditional re-enable Invocationの生成に失敗しました。");
    auto submittedReenable = sge4::d3d12::Submit(
        loaded.value(), std::move(frozenReenable).value(), {2, {}});
    Require(submittedReenable && submittedReenable.value().submittedLeafCount == 2,
        "Conditional Regionの再有効化に失敗しました。");
    const std::array reenabledExpected{zero, zero, zero, valueThree};
    auto reenabledRead = sge4::d3d12::ReadBuffer(loaded.value(), outputId);
    Require(reenabledRead && EqualsDense(reenabledRead.value().bytes, reenabledExpected),
        "再有効化時にCommit済みDynamic shadowがGPUへ反映されませんでした。");

    auto recovery = sge4::d3d12::Recover(
        loaded.value(), sge4::runtime::DeviceRecoveryMode::ControlledRebuild);
    Require(recovery && recovery.value().newEpoch > recovery.value().previousEpoch,
        "Conditional Region Compositionの制御回復に失敗しました。");
    Require(static_cast<bool>(sge4::d3d12::AcknowledgeExternalRebind(loaded.value())),
        "Conditional Region Compositionの外部再bind確認に失敗しました。");
    planning = loaded.value().PlanningContext();
    sge4::dynamic::InvocationInputV1 recoverySeed;
    recoverySeed.timelineOrdinal = 3;
    recoverySeed.mode = planning.requiredMode;
    recoverySeed.activeMembers = {3};
    recoverySeed.updatePayloads = {UpdatePayload(3, valueThree)};
    auto frozenRecovery = tests::BuildFrozenInvocation(
        loaded.value().Package(), planning.deviceEpoch, std::move(recoverySeed),
        std::move(planning.previousHistory));
    Require(static_cast<bool>(frozenRecovery), "Conditional RecoverySeedの生成に失敗しました。");
    auto resumed = sge4::d3d12::Submit(
        loaded.value(), std::move(frozenRecovery).value(), {3, {}});
    Require(resumed && resumed.value().submittedLeafCount == 2 &&
        resumed.value().verifiedConditionalRegionCount == 1,
        "RecoverySeed後にConditional true branchを再構築できませんでした。");
    auto recoveryRead = sge4::d3d12::ReadBuffer(loaded.value(), outputId);
    Require(recoveryRead && EqualsDense(recoveryRead.value().bytes, reenabledExpected),
        "Conditional RecoverySeed後のGPU観測結果が一致しません。");
}

void VerifyDynamicExecutionQualification()
{
    constexpr std::uint32_t Universe = 4;
    const std::array<float, 4> firstOne{1, 2, 3, 4};
    const std::array<float, 4> firstThree{5, 6, 7, 8};
    const std::array<float, 4> secondOne{11, 12, 13, 14};
    const std::array<float, 4> secondTwo{21, 22, 23, 24};
    const std::array<float, 4> zero{};

    auto package = tests::BuildVerifiedDynamicUnified(Universe);
    if (!package)
        throw std::runtime_error(
            "Verified Dynamic Compositionの生成に失敗しました：" +
            package.error());
    const auto outputId = fixture::FindResourceFlow(
        package.value().FileBytes(), "unified/dynamic/output");
    Require(outputId.IsValid(),
        "Verified Dynamic outputの解決に失敗しました。");

    sge4::d3d12::Executor backend({true, false, false});
    auto loaded = sge4::d3d12::LoadComposition(
        package.value().FileBytes(), backend);
    Require(static_cast<bool>(loaded),
        "Verified Dynamic CompositionのLoadに失敗しました。");

    auto planning = loaded.value().PlanningContext();
    sge4::dynamic::InvocationInputV1 seed;
    seed.timelineOrdinal = 0;
    seed.mode = planning.requiredMode;
    seed.activeMembers = {1, 3};
    seed.updatePayloads = {
        UpdatePayload(1, firstOne), UpdatePayload(3, firstThree)};
    auto frozenSeed = tests::BuildFrozenInvocation(
        loaded.value().Package(), planning.deviceEpoch, std::move(seed),
        std::move(planning.previousHistory));
    Require(static_cast<bool>(frozenSeed),
        "Verified Dynamic InitialSeedの生成に失敗しました。");
    auto submittedSeed = sge4::d3d12::Submit(
        loaded.value(), std::move(frozenSeed).value(), {0, {}});
    Require(submittedSeed && submittedSeed.value().verifiedTransitionCount == 2 &&
        submittedSeed.value().verifiedDynamicByteCount == Universe * 16,
        "Verified Dynamic InitialSeedが実実行へ接続されませんでした。");
    auto firstRead = sge4::d3d12::ReadBuffer(loaded.value(), outputId);
    const std::array firstExpected{zero, firstOne, zero, firstThree};
    Require(firstRead && EqualsDense(firstRead.value().bytes, firstExpected),
        "Verified Dynamic InitialSeedのGPU観測結果が一致しません。");

    planning = loaded.value().PlanningContext();
    sge4::dynamic::InvocationInputV1 next;
    next.timelineOrdinal = 1;
    next.mode = planning.requiredMode;
    next.activeMembers = {1, 2};
    next.modifiedSurvivors = {1};
    next.updatePayloads = {
        UpdatePayload(1, secondOne), UpdatePayload(2, secondTwo)};
    auto frozenNext = tests::BuildFrozenInvocation(
        loaded.value().Package(), planning.deviceEpoch, std::move(next),
        std::move(planning.previousHistory));
    Require(static_cast<bool>(frozenNext),
        "Verified Dynamic ContinueHistoryの生成に失敗しました。");
    auto submittedNext = sge4::d3d12::Submit(
        loaded.value(), std::move(frozenNext).value(), {1, {}});
    Require(submittedNext && submittedNext.value().verifiedTransitionCount == 3,
        "Update／Activation／Clearがexact transition countへ反映されませんでした。");
    auto secondRead = sge4::d3d12::ReadBuffer(loaded.value(), outputId);
    const std::array secondExpected{zero, secondOne, secondTwo, zero};
    Require(secondRead && EqualsDense(secondRead.value().bytes, secondExpected),
        "Update／Retain／ClearのGPU観測結果が一致しません。");

    planning = loaded.value().PlanningContext();
    sge4::dynamic::InvocationInputV1 retain;
    retain.timelineOrdinal = 2;
    retain.mode = planning.requiredMode;
    retain.activeMembers = {1, 2};
    auto frozenRetain = tests::BuildFrozenInvocation(
        loaded.value().Package(), planning.deviceEpoch, std::move(retain),
        std::move(planning.previousHistory));
    Require(static_cast<bool>(frozenRetain),
        "Verified Dynamic Retain Invocationの生成に失敗しました。");
    auto submittedRetain = sge4::d3d12::Submit(
        loaded.value(), std::move(frozenRetain).value(), {2, {}});
    Require(submittedRetain && submittedRetain.value().verifiedTransitionCount == 0,
        "Retain-only frameのexact zero transitionが保存されませんでした。");
    auto retainedRead = sge4::d3d12::ReadBuffer(loaded.value(), outputId);
    Require(retainedRead && EqualsDense(retainedRead.value().bytes, secondExpected),
        "Retain-only frameでGPU shadowが変化しました。");

    // The verified route owns this slot. A caller cannot inject competing bytes.
    planning = loaded.value().PlanningContext();
    sge4::dynamic::InvocationInputV1 collision;
    collision.timelineOrdinal = 3;
    collision.mode = planning.requiredMode;
    collision.activeMembers = {1, 2};
    collision.modifiedSurvivors = {1};
    collision.updatePayloads = {UpdatePayload(1, secondOne)};
    auto frozenCollision = tests::BuildFrozenInvocation(
        loaded.value().Package(), planning.deviceEpoch, std::move(collision),
        planning.previousHistory);
    Require(static_cast<bool>(frozenCollision),
        "Slot collision negative testのInvocation生成に失敗しました。");
    sge4::d3d12::FrameInput competingFrame;
    competingFrame.frameNumber = 3;
    competingFrame.leafDynamicData.push_back({
        package.value().DynamicContract().targetLeaf,
        package.value().DynamicContract().targetDynamicSlot,
        std::vector<std::byte>(Universe * 16, std::byte{0})});
    Require(!sge4::d3d12::Submit(
        loaded.value(), std::move(frozenCollision).value(), std::move(competingFrame)),
        "Verified Dynamic SlotへのCaller上書きが受理されました。");

    // Missing exact update payload must be rejected before freezing.
    sge4::dynamic::InvocationInputV1 missing;
    missing.timelineOrdinal = 3;
    missing.mode = planning.requiredMode;
    missing.activeMembers = {1, 2};
    missing.modifiedSurvivors = {1};
    Require(!tests::BuildFrozenInvocation(
        loaded.value().Package(), planning.deviceEpoch, std::move(missing),
        planning.previousHistory),
        "exact update payloadを欠くInvocationが受理されました。");

    auto recovery = sge4::d3d12::Recover(
        loaded.value(), sge4::runtime::DeviceRecoveryMode::ControlledRebuild);
    Require(recovery && recovery.value().newEpoch > recovery.value().previousEpoch,
        "Verified Dynamic Compositionの制御回復に失敗しました。");
    Require(static_cast<bool>(sge4::d3d12::AcknowledgeExternalRebind(loaded.value())),
        "Verified Dynamic Compositionの外部再bind確認に失敗しました。");

    planning = loaded.value().PlanningContext();
    sge4::dynamic::InvocationInputV1 recoverySeed;
    recoverySeed.timelineOrdinal = 3;
    recoverySeed.mode = planning.requiredMode;
    recoverySeed.activeMembers = {1, 2};
    recoverySeed.updatePayloads = {
        UpdatePayload(1, secondOne), UpdatePayload(2, secondTwo)};
    auto frozenRecoverySeed = tests::BuildFrozenInvocation(
        loaded.value().Package(), planning.deviceEpoch, std::move(recoverySeed),
        std::move(planning.previousHistory));
    Require(static_cast<bool>(frozenRecoverySeed),
        "Verified Dynamic RecoverySeedの生成に失敗しました。");
    auto resumed = sge4::d3d12::Submit(
        loaded.value(), std::move(frozenRecoverySeed).value(), {3, {}});
    Require(resumed && resumed.value().verifiedTransitionCount == 2,
        "RecoverySeedがActive全要素を再構築しませんでした。");
    auto recoveryRead = sge4::d3d12::ReadBuffer(loaded.value(), outputId);
    Require(recoveryRead && EqualsDense(recoveryRead.value().bytes, secondExpected),
        "RecoverySeed後のGPU観測結果が一致しません。");
}
}

int main(int argc, char** argv)
{
    try
    {
        const bool actualRemoval = argc > 1 && std::string_view(argv[1]) == "--actual-removal";
        VerifyIndirectWorkQualification();
        VerifyLimitedTexture2DFlowQualification();
        VerifyConditionalRegionQualification();
        VerifyDynamicExecutionQualification();
        auto package = tests::BuildLinearUnified();
        Require(static_cast<bool>(package), "Compositionが検証または実行の契約に違反しています。");
        const auto inputId = fixture::FindResourceFlow(
            package.value().FileBytes(), "unified/linear/input");
        const auto outputId = fixture::FindResourceFlow(
            package.value().FileBytes(), "unified/linear/output");
        Require(inputId.IsValid() && outputId.IsValid(),
            "Compositionが検証または実行の契約に違反しています。");

        const std::array<float, 4> input{1.0f, 2.0f, 3.0f, 4.0f};
        const std::array<float, 4> expected{3.0f, 4.0f, 5.0f, 6.0f};
        sge4::d3d12::Executor backend({true, false, false});
        sge4::d3d12::LoadInput loadInput;
        loadInput.initialResources = {{inputId, fixture::Bytes(input)}};
        auto loaded = sge4::d3d12::LoadComposition(
            package.value().FileBytes(), backend, std::move(loadInput));
        Require(static_cast<bool>(loaded), "Compositionが検証または実行の契約に違反しています。");

        const auto oldRepresentation = loaded.value().RepresentationHandle();
        const auto oldHistory = loaded.value().HistoryHandle();
        auto planning = loaded.value().PlanningContext();
        sge4::dynamic::InvocationInputV1 firstInput;
        firstInput.timelineOrdinal = 0;
        firstInput.mode = planning.requiredMode;
        firstInput.activeMembers = {0, 1};
        auto firstFrozen = tests::BuildFrozenInvocation(
            loaded.value().Package(), planning.deviceEpoch, std::move(firstInput),
            std::move(planning.previousHistory));
        Require(static_cast<bool>(firstFrozen), "入力または内部状態の処理または検証に失敗しました。");
        sge4::d3d12::FrameInput firstFrame;
        firstFrame.frameNumber = 0;
        auto submitted = sge4::d3d12::Submit(
            loaded.value(), std::move(firstFrozen).value(), std::move(firstFrame));
        Require(static_cast<bool>(submitted), "入力または内部状態の処理または検証に失敗しました。");
        auto read = sge4::d3d12::ReadBuffer(loaded.value(), outputId);
        Require(read && fixture::Equals(read.value().bytes, expected),
            "入力または内部状態がCanonicalな順序または識別子規則に違反しています。");

        // A ContinueHistory invocation verified against a different generation-one
        // history must be rejected before native submission, even when composition,
        // epoch, mode, and next generation otherwise look valid.
        sge4::dynamic::InvocationInputV1 alternateSeedInput;
        alternateSeedInput.timelineOrdinal = 0;
        alternateSeedInput.mode = sge4::dynamic::InvocationModeV1::InitialSeed;
        alternateSeedInput.activeMembers = {1};
        auto alternateSeed = tests::BuildFrozenInvocation(
            loaded.value().Package(), planning.deviceEpoch, std::move(alternateSeedInput));
        Require(static_cast<bool>(alternateSeed), "入力または内部状態の処理または検証に失敗しました。");
        sge4::dynamic::InvocationInputV1 mismatchedContinueInput;
        mismatchedContinueInput.timelineOrdinal = 1;
        mismatchedContinueInput.mode = sge4::dynamic::InvocationModeV1::ContinueHistory;
        mismatchedContinueInput.activeMembers = {1};
        auto mismatchedContinue = tests::BuildFrozenInvocation(
            loaded.value().Package(), planning.deviceEpoch,
            std::move(mismatchedContinueInput), alternateSeed.value().NextHistory());
        Require(static_cast<bool>(mismatchedContinue), "InvocationがCanonicalな順序または識別子規則に違反しています。");
        sge4::d3d12::FrameInput rejectedFrame;
        rejectedFrame.frameNumber = 1;
        auto rejected = sge4::d3d12::Submit(
            loaded.value(), std::move(mismatchedContinue).value(), std::move(rejectedFrame));
        Require(!rejected, "Invocationが検証または実行の契約に違反しています。");

        const auto mode = actualRemoval
            ? sge4::runtime::DeviceRecoveryMode::ForceRemovalForTest
            : sge4::runtime::DeviceRecoveryMode::ControlledRebuild;
        auto recovery = sge4::d3d12::Recover(loaded.value(), mode);
        Require(static_cast<bool>(recovery), "Compositionが検証または実行の契約に違反しています。");
        Require(!sge4::d3d12::ValidateHandleEpoch(loaded.value(), oldRepresentation) &&
            !sge4::d3d12::ValidateHandleEpoch(loaded.value(), oldHistory),
            "入力または内部状態の状態または世代が実行契約と一致しません。");
        if (actualRemoval)
        {
            Require(recovery.value().awaitingAdapter &&
                loaded.value().State() == sge4::runtime::DeviceRuntimeState::AwaitingAdapter,
                "Adapterが検証または実行の契約に違反しています。");
            Require(recovery.value().newEpoch == recovery.value().previousEpoch,
                "Adapterが検証または実行の契約に違反しています。");
            auto retry = sge4::d3d12::Recover(
                loaded.value(), sge4::runtime::DeviceRecoveryMode::RetryAdapterReacquisition);
            Require(static_cast<bool>(retry), "Adapterが検証または実行の契約に違反しています。");
            Require(retry.value().awaitingAdapter && !retry.value().adapterReacquired,
                "Adapterが検証または実行の契約に違反しています。");
            std::cout << "New SGE4統合実Device削除資格試験に合格しました。\n";
            std::cout << "Adapter待機状態を維持したDevice世代：" << loaded.value().DeviceEpoch() << '\n';
            return 0;
        }

        Require(recovery.value().newEpoch > recovery.value().previousEpoch,
            "Epochの状態または世代が実行契約と一致しません。");
        Require(!loaded.value().ExternalStateBound() && loaded.value().RequiresRecoverySeed(),
            "入力または内部状態が検証または実行の契約に違反しています。");
        Require(static_cast<bool>(sge4::d3d12::AcknowledgeExternalRebind(loaded.value())),
            "入力または内部状態の処理または検証に失敗しました。");

        planning = loaded.value().PlanningContext();
        sge4::dynamic::InvocationInputV1 seedInput;
        seedInput.timelineOrdinal = 1;
        seedInput.mode = planning.requiredMode;
        seedInput.activeMembers = {0, 1};
        auto seedFrozen = tests::BuildFrozenInvocation(
            loaded.value().Package(), planning.deviceEpoch, std::move(seedInput),
            std::move(planning.previousHistory));
        Require(static_cast<bool>(seedFrozen), "入力または内部状態の処理または検証に失敗しました。");
        sge4::d3d12::FrameInput seedFrame;
        seedFrame.frameNumber = 1;
        auto resumed = sge4::d3d12::Submit(
            loaded.value(), std::move(seedFrozen).value(), std::move(seedFrame));
        Require(static_cast<bool>(resumed), "入力または内部状態の処理または検証に失敗しました。");
        auto reread = sge4::d3d12::ReadBuffer(loaded.value(), outputId);
        Require(reread && fixture::Equals(reread.value().bytes, expected),
            "入力または内部状態がCanonicalな順序または識別子規則に違反しています。");

        std::cout << "New SGE4統合Windows実行資格試験に合格しました。\n";
        std::cout << "回復方式：制御された全体再構築\n";
        std::cout << "Device世代：" << recovery.value().previousEpoch << " -> "
                  << recovery.value().newEpoch << '\n';
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "New SGE4統合Windows実行資格試験に失敗しました："
                  << exception.what() << '\n';
        return 1;
    }
}
