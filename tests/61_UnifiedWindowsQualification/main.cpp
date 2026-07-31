#include "../fixtures/UnifiedFixture.h"
#include "../../src/backends/d3d12/runtime/Runtime.h"

#include <array>
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
}

int main(int argc, char** argv)
{
    try
    {
        const bool actualRemoval = argc > 1 && std::string_view(argv[1]) == "--actual-removal";
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
