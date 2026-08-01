#include "../fixtures/UnifiedFixture.h"
#include "Abi1GoldenBytes.h"
#include "Abi2CorruptionTests.h"
#include "Abi2PortableSelfTest.h"
#include "../../src/composition/migration/abi1/FrozenCompositionAbi1Migration.h"
#include "../../src/composition/artifact/abi2/FrozenCompositionAbi2.h"
#include "../../src/canonical/artifact/SectionedArtifact.h"
#include "../../src/runtime/session/RuntimeSession.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace
{
namespace tests = sge4::tests;
namespace composition = sge4::composition;
namespace dynamic = sge4::dynamic;
namespace fixture = sge4::qualification::canonical_runtime_fixture;

void Require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

bool EqualIndices(std::span<const std::uint32_t> actual, std::initializer_list<std::uint32_t> expected)
{
    return std::equal(actual.begin(), actual.end(), expected.begin(), expected.end());
}
}

int main(int argc, char** argv)
{
    try
    {
        tests::VerifyAbi1GoldenBytes();
        tests::VerifyAbi2PortableRoundTrip();

        auto first = tests::BuildLinearUnified();
        auto second = tests::BuildLinearUnified();
        Require(first && second, "Compositionが検証または実行の契約に違反しています。");
        Require(first.value().FileBytes().size() == second.value().FileBytes().size() &&
            std::equal(first.value().FileBytes().begin(), first.value().FileBytes().end(),
                second.value().FileBytes().begin()),
            "Compositionが検証または実行の契約に違反しています。");
        Require(first.value().VerifiedComposition().LeafCount() == 2,
            "Compositionが検証または実行の契約に違反しています。");
        Require(first.value().Certificate().leafCount == 2 && first.value().Certificate().flowCount == 3,
            "CompositionがCanonicalな順序または識別子規則に違反しています。");
        auto flat = sge4::ReadSectionedArtifact(
            first.value().FileBytes(), composition::artifact::FrozenCompositionAbi2Magic,
            composition::artifact::FrozenCompositionAbi2FormatMajor);
        Require(flat &&
            flat.value().FormatMinor() == composition::artifact::FrozenCompositionAbi2FormatMinor &&
            flat.value().Sections().size() ==
                composition::artifact::FrozenCompositionAbi2SectionKinds.size(),
            "SGE4UNI 2.7の平坦Section構造が成立していません。");
        Require(flat.value().FindSection(
            std::to_underlying(composition::artifact::FrozenCompositionAbi2SectionKind::LeafTable)) != nullptr &&
            flat.value().FindSection(
            std::to_underlying(composition::artifact::FrozenCompositionAbi2SectionKind::AuthorityLedger)) != nullptr,
            "SGE4UNI 2.7の直接Sectionがありません。");

        auto legacyInput = tests::BuildLinearInput();
        Require(static_cast<bool>(legacyInput), "ABI 1移行入力の生成に失敗しました。");
        auto legacyBytes = composition::migration::abi1::BuildFrozenCompositionPackageAbi1ForMigration(
            std::move(legacyInput).value(),
            composition::MakeAuthorityOnlyDynamicContractV1(8));
        Require(static_cast<bool>(legacyBytes), "SGE4UNI 1.1移行Corpusの生成に失敗しました。");
        Require(!composition::ReadFrozenCompositionPackage(legacyBytes.value()),
            "Production ReaderがSGE4UNI 1.1を受理しました。");
        auto migrated = composition::migration::abi1::MigrateFrozenCompositionPackageAbi1ToAbi2(
            legacyBytes.value());
        Require(static_cast<bool>(migrated), "SGE4UNI 1.1から2.7へのMigrationに失敗しました。");
        Require(migrated.value().FileBytes().size() == first.value().FileBytes().size() &&
            std::equal(migrated.value().FileBytes().begin(), migrated.value().FileBytes().end(),
                first.value().FileBytes().begin()),
            "直接生成したABI 2.7とMigration後ABI 2.7がbyte一致しません。");
        Require(migrated.value().Certificate().contractIdentity == first.value().Certificate().contractIdentity &&
            migrated.value().Certificate().planIdentity == first.value().Certificate().planIdentity &&
            migrated.value().Certificate().sealIdentity == first.value().Certificate().sealIdentity,
            "MigrationでContract／Plan／Seal identityが保存されませんでした。");
        if (argc > 1)
        {
            std::ofstream output(argv[1], std::ios::binary | std::ios::trunc);
            Require(static_cast<bool>(output), "入力または内部状態の処理または検証に失敗しました。");
            const auto bytes = first.value().FileBytes();
            output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            Require(static_cast<bool>(output), "入力または内部状態の処理または検証に失敗しました。");
        }

        auto roundTrip = composition::ReadFrozenCompositionPackage(first.value().FileBytes());
        Require(static_cast<bool>(roundTrip), "Compositionが検証または実行の契約に違反しています。");
        Require(roundTrip.value().SemanticDigest() == first.value().SemanticDigest(),
            "Compositionが検証または実行の契約に違反しています。");

        const auto epoch = sge4::canonical::DeviceEpoch::TryCreate(1);
        Require(epoch.has_value(), "Epochの状態または世代が実行契約と一致しません。");
        dynamic::InvocationInputV1 initial;
        initial.timelineOrdinal = 0;
        initial.mode = sge4::dynamic::InvocationModeV1::InitialSeed;
        initial.activeMembers = {0, 2, 7};
        auto initialFrozen = tests::BuildFrozenInvocation(first.value(), *epoch, std::move(initial));
        Require(static_cast<bool>(initialFrozen), "Invocationが検証または実行の契約に違反しています。");
        Require(EqualIndices(initialFrozen.value().Decision().activationSet.Indices(), {0, 2, 7}),
            "入力または内部状態がCanonicalな順序または識別子規則に違反しています。");
        Require(initialFrozen.value().Decision().indirectWorkCount.value() == 3,
            "入力または内部状態がCanonicalな順序または識別子規則に違反しています。");
        Require(
            initialFrozen.value().IndirectDispatch().mode ==
                composition::IndirectExecutionModeV1::None &&
            initialFrozen.value().IndirectDispatch().workCount == 0 &&
            initialFrozen.value().IndirectDispatch().threadGroupCountX == 0,
            "Indirect契約を持たないInvocationにDispatch引数が生成されました。");
        Require(!initialFrozen.value().Artifact().PreviousHistoryIdentity().has_value(),
            "Invocationが検証または実行の契約に違反しています。");

        dynamic::InvocationInputV1 next;
        next.timelineOrdinal = 1;
        next.mode = sge4::dynamic::InvocationModeV1::ContinueHistory;
        next.activeMembers = {0, 3, 7};
        next.modifiedSurvivors = {7};
        auto continued = tests::BuildFrozenInvocation(
            first.value(), *epoch, std::move(next), initialFrozen.value().NextHistory());
        Require(static_cast<bool>(continued), "Invocationが検証または実行の契約に違反しています。");
        Require(continued.value().Artifact().PreviousHistoryIdentity().has_value() &&
            *continued.value().Artifact().PreviousHistoryIdentity() ==
                initialFrozen.value().NextHistory().Descriptor().identity,
            "Invocationが検証または実行の契約に違反しています。");
        const auto& decision = continued.value().Decision();
        Require(EqualIndices(decision.activationSet.Indices(), {3}), "入力または内部状態がCanonicalな順序または識別子規則に違反しています。");
        Require(EqualIndices(decision.deactivationSet.Indices(), {2}), "入力または内部状態がCanonicalな順序または識別子規則に違反しています。");
        // R4 defines the exact update set as W_t = N_t union M_t.
        // Member 3 is newly activated (N_t) and member 7 is a modified survivor (M_t).
        Require(EqualIndices(decision.updateSet.Indices(), {3, 7}), "入力または内部状態がCanonicalな順序または識別子規則に違反しています。");
        Require(EqualIndices(decision.retainSet.Indices(), {0}), "入力または内部状態がCanonicalな順序または識別子規則に違反しています。");
        Require(EqualIndices(decision.transitionSet.Indices(), {2, 3, 7}), "入力または内部状態がCanonicalな順序または識別子規則に違反しています。");
        Require(decision.indirectWorkCount.value() == 3, "入力または内部状態がCanonicalな順序または識別子規則に違反しています。");

        auto verifiedDynamic = tests::BuildVerifiedDynamicUnified(4);
        if (!verifiedDynamic)
            throw std::runtime_error(
                "Verified Dynamic Compositionの生成に失敗しました：" +
                verifiedDynamic.error());
        dynamic::InvocationInputV1 verifiedSeed;
        verifiedSeed.timelineOrdinal = 0;
        verifiedSeed.mode = dynamic::InvocationModeV1::InitialSeed;
        verifiedSeed.activeMembers = {1, 3};
        verifiedSeed.updatePayloads = {
            {1, fixture::Bytes(std::array<float, 4>{1.0f, 2.0f, 3.0f, 4.0f})},
            {3, fixture::Bytes(std::array<float, 4>{5.0f, 6.0f, 7.0f, 8.0f})}};
        auto verifiedFrozen = tests::BuildFrozenInvocation(
            verifiedDynamic.value(), *epoch, std::move(verifiedSeed));
        Require(static_cast<bool>(verifiedFrozen),
            "Verified Dynamic Invocationの生成に失敗しました。");
        Require(verifiedFrozen.value().ExecutionPayload().updates.size() == 2 &&
            verifiedFrozen.value().Decision().indirectWorkCount.value() == 2,
            "Verified payloadとexact transition countが一致しません。");
        auto invocationArtifact = sge4::ReadSectionedArtifact(
            verifiedFrozen.value().FileBytes(), dynamic::FrozenInvocationMagic,
            dynamic::FrozenInvocationFormatMajor);
        Require(invocationArtifact &&
            invocationArtifact.value().FormatMinor() == dynamic::FrozenInvocationFormatMinor &&
            invocationArtifact.value().Sections().size() ==
                dynamic::FrozenInvocationSectionKinds.size(),
            "SGE4INV 1.5のSection構造が成立していません。");

        dynamic::InvocationInputV1 missingPayload;
        missingPayload.timelineOrdinal = 0;
        missingPayload.mode = dynamic::InvocationModeV1::InitialSeed;
        missingPayload.activeMembers = {1, 3};
        missingPayload.updatePayloads = {
            {1, fixture::Bytes(std::array<float, 4>{1.0f, 2.0f, 3.0f, 4.0f})}};
        Require(!tests::BuildFrozenInvocation(
            verifiedDynamic.value(), *epoch, std::move(missingPayload)),
            "exact update setを満たさないpayloadが受理されました。");

        auto conditional = tests::BuildConditionalVerifiedDynamicUnified(4);
        if (!conditional)
            throw std::runtime_error(
                "Conditional Region Compositionの生成に失敗しました：" + conditional.error());
        Require(conditional.value().DynamicContract().conditionalRegions.size() == 1,
            "Conditional Region契約がSGE4UNI 2.7へ保存されませんでした。");

        dynamic::InvocationInputV1 conditionalTrue;
        conditionalTrue.timelineOrdinal = 0;
        conditionalTrue.mode = dynamic::InvocationModeV1::InitialSeed;
        conditionalTrue.activeMembers = {1};
        conditionalTrue.updatePayloads = {
            {1, fixture::Bytes(std::array<float, 4>{1.0f, 2.0f, 3.0f, 4.0f})}};
        auto frozenTrue = tests::BuildFrozenInvocation(
            conditional.value(), *epoch, std::move(conditionalTrue));
        Require(frozenTrue && frozenTrue.value().Decision().conditionalSelections.size() == 1 &&
            frozenTrue.value().Decision().conditionalSelections[0].predicateValue &&
            frozenTrue.value().Decision().enabledLeaves ==
                std::vector<composition::LeafPackageId>{{0}, {1}},
            "ActiveSetNonEmptyのTrue branchが独立検証されませんでした。");

        dynamic::InvocationInputV1 conditionalFalse;
        conditionalFalse.timelineOrdinal = 0;
        conditionalFalse.mode = dynamic::InvocationModeV1::InitialSeed;
        auto requestFalse = dynamic::BuildDynamicInvocationRequest(
            conditional.value(), *epoch, std::move(conditionalFalse));
        Require(static_cast<bool>(requestFalse),
            "Conditional false requestの生成に失敗しました。");
        auto proposalFalse = dynamic::DynamicInvocationPlannerV1::Plan(requestFalse.value());
        Require(proposalFalse.Planned() &&
            proposalFalse.proposal->decision.conditionalSelections.size() == 1 &&
            !proposalFalse.proposal->decision.conditionalSelections[0].predicateValue &&
            proposalFalse.proposal->decision.enabledLeaves.empty(),
            "ActiveSetNonEmptyのFalse branchが導出されませんでした。");
        auto verifiedFalse = dynamic::DynamicInvocationVerifierV1::Verify(
            requestFalse.value(), *proposalFalse.proposal);
        Require(verifiedFalse.Accepted(),
            "Conditional false Decisionが独立Verifierに拒否されました。");
        auto tamperedProposal = *proposalFalse.proposal;
        tamperedProposal.decision.enabledLeaves = {{0}, {1}};
        Require(!dynamic::DynamicInvocationVerifierV1::Verify(
            requestFalse.value(), tamperedProposal).Accepted(),
            "改竄されたenabled Leaf集合が独立Verifierに受理されました。");
        auto frozenFalse = dynamic::FreezeVerifiedInvocation(*verifiedFalse.verified);
        Require(frozenFalse && frozenFalse.value().Decision().enabledLeaves.empty(),
            "Conditional false DecisionをSGE4INV 1.5へFreezeできませんでした。");

        // Regression: a zero-Leaf false branch still constitutes a successful
        // verified dynamic submission.  Its Clear/Update effects must be committed
        // to the private dense shadow together with History, even though no Dynamic
        // Slot binding was emitted to the native runtime in that frame.
        auto conditionalRuntimePackage = tests::BuildConditionalVerifiedDynamicUnified(4);
        Require(static_cast<bool>(conditionalRuntimePackage),
            "Conditional shadow commit用Compositionの生成に失敗しました。");
        auto conditionalSession = sge4::runtime::Session::Create(
            std::move(conditionalRuntimePackage).value(), 1);
        Require(static_cast<bool>(conditionalSession),
            "Conditional shadow commit用Runtime Sessionの生成に失敗しました。");

        auto runtimePlanning = conditionalSession.value().PlanningContext();
        dynamic::InvocationInputV1 runtimeSeed;
        runtimeSeed.timelineOrdinal = 0;
        runtimeSeed.mode = runtimePlanning.requiredMode;
        runtimeSeed.activeMembers = {1};
        runtimeSeed.updatePayloads = {
            {1, fixture::Bytes(std::array<float, 4>{1.0f, 2.0f, 3.0f, 4.0f})}};
        auto runtimeSeedFrozen = tests::BuildFrozenInvocation(
            conditionalSession.value().Package(), runtimePlanning.deviceEpoch,
            std::move(runtimeSeed), std::move(runtimePlanning.previousHistory));
        Require(static_cast<bool>(runtimeSeedFrozen),
            "Conditional shadow commit用InitialSeedの生成に失敗しました。");
        Require(static_cast<bool>(conditionalSession.value().ValidateForSubmission(
            runtimeSeedFrozen.value())),
            "Conditional shadow commit用InitialSeedの検証に失敗しました。");
        auto preparedRuntimeSeed = conditionalSession.value().PrepareDynamicExecution(
            runtimeSeedFrozen.value());
        Require(preparedRuntimeSeed && preparedRuntimeSeed.value().bindings.size() == 1 && preparedRuntimeSeed.value().bindings[0].enabled,
            "Conditional true branchのDynamic bindingが準備されませんでした。");
        conditionalSession.value().CommitSubmission(
            runtimeSeedFrozen.value(), std::move(preparedRuntimeSeed).value());

        runtimePlanning = conditionalSession.value().PlanningContext();
        dynamic::InvocationInputV1 runtimeDisable;
        runtimeDisable.timelineOrdinal = 1;
        runtimeDisable.mode = runtimePlanning.requiredMode;
        auto runtimeDisableFrozen = tests::BuildFrozenInvocation(
            conditionalSession.value().Package(), runtimePlanning.deviceEpoch,
            std::move(runtimeDisable), std::move(runtimePlanning.previousHistory));
        Require(static_cast<bool>(runtimeDisableFrozen),
            "Conditional shadow commit用false Invocationの生成に失敗しました。");
        Require(static_cast<bool>(conditionalSession.value().ValidateForSubmission(
            runtimeDisableFrozen.value())),
            "Conditional shadow commit用false Invocationの検証に失敗しました。");
        auto preparedRuntimeDisable = conditionalSession.value().PrepareDynamicExecution(
            runtimeDisableFrozen.value());
        Require(preparedRuntimeDisable && preparedRuntimeDisable.value().bindings.size() == 1 && !preparedRuntimeDisable.value().bindings[0].enabled &&
            preparedRuntimeDisable.value().appliedTransitionCount == 1,
            "zero-Leaf false branchのClear transitionが準備されませんでした。");
        conditionalSession.value().CommitSubmission(
            runtimeDisableFrozen.value(), std::move(preparedRuntimeDisable).value());

        runtimePlanning = conditionalSession.value().PlanningContext();
        dynamic::InvocationInputV1 runtimeReenable;
        runtimeReenable.timelineOrdinal = 2;
        runtimeReenable.mode = runtimePlanning.requiredMode;
        runtimeReenable.activeMembers = {3};
        const std::array<float, 4> runtimeValueThree{31.0f, 32.0f, 33.0f, 34.0f};
        runtimeReenable.updatePayloads = {{3, fixture::Bytes(runtimeValueThree)}};
        auto runtimeReenableFrozen = tests::BuildFrozenInvocation(
            conditionalSession.value().Package(), runtimePlanning.deviceEpoch,
            std::move(runtimeReenable), std::move(runtimePlanning.previousHistory));
        Require(static_cast<bool>(runtimeReenableFrozen),
            "Conditional shadow commit用再有効化Invocationの生成に失敗しました。");
        auto preparedRuntimeReenable = conditionalSession.value().PrepareDynamicExecution(
            runtimeReenableFrozen.value());
        Require(preparedRuntimeReenable && preparedRuntimeReenable.value().bindings.size() == 1 && preparedRuntimeReenable.value().bindings[0].enabled,
            "Conditional shadow commit用再有効化bindingが準備されませんでした。");
        std::vector<std::byte> expectedRuntimeShadow(4u * 16u, std::byte{0});
        const auto valueThreeBytes = fixture::Bytes(runtimeValueThree);
        std::copy(valueThreeBytes.begin(), valueThreeBytes.end(),
            expectedRuntimeShadow.begin() + 3u * 16u);
        Require(preparedRuntimeReenable.value().bindings[0].denseSlotBytes == expectedRuntimeShadow,
            "zero-Leaf false branchでCommitされたClearが再有効化shadowへ反映されませんでした。");

        // Generalization 6: one canonical 32-byte member payload is sliced into
        // two independently owned dense Dynamic Slots.  Both route shadows advance
        // from the same exact transition stream and commit atomically.
        auto multiPackage = tests::BuildMultiTargetVerifiedDynamicUnified(4);
        Require(static_cast<bool>(multiPackage),
            "Multi-target Dynamic Compositionの生成に失敗しました。");
        Require(multiPackage.value().DynamicContract().schemaVersion ==
            composition::artifact::FrozenCompositionAbi2DynamicContractSchema &&
            multiPackage.value().DynamicContract().canonicalMemberBytes == 32 &&
            multiPackage.value().DynamicContract().executionRoutes.size() == 2,
            "Multi-target Dynamic ContractがSGE4UNI 2.7へ固定されませんでした。");
        auto multiSession = sge4::runtime::Session::Create(
            std::move(multiPackage).value(), 1);
        Require(static_cast<bool>(multiSession),
            "Multi-target Runtime Sessionの生成に失敗しました。");

        const auto MakeCanonicalPayload = [](const std::array<float, 4>& first,
                                             const std::array<float, 4>& second) {
            auto firstBytes = fixture::Bytes(first);
            auto secondBytes = fixture::Bytes(second);
            std::vector<std::byte> bytes;
            bytes.reserve(firstBytes.size() + secondBytes.size());
            bytes.insert(bytes.end(), firstBytes.begin(), firstBytes.end());
            bytes.insert(bytes.end(), secondBytes.begin(), secondBytes.end());
            return bytes;
        };
        const std::array<float, 4> multiA1{1, 2, 3, 4};
        const std::array<float, 4> multiB1{11, 12, 13, 14};
        const std::array<float, 4> multiA3{31, 32, 33, 34};
        const std::array<float, 4> multiB3{41, 42, 43, 44};

        auto multiPlanning = multiSession.value().PlanningContext();
        dynamic::InvocationInputV1 invalidMultiPayload;
        invalidMultiPayload.timelineOrdinal = 0;
        invalidMultiPayload.mode = multiPlanning.requiredMode;
        invalidMultiPayload.activeMembers = {0};
        invalidMultiPayload.updatePayloads = {
            {0, std::vector<std::byte>(16, std::byte{0})}};
        Require(!tests::BuildFrozenInvocation(
            multiSession.value().Package(), multiPlanning.deviceEpoch,
            std::move(invalidMultiPayload), multiPlanning.previousHistory),
            "Canonical member byte幅より短いMulti-target payloadが受理されました。");

        dynamic::InvocationInputV1 multiSeed;
        multiSeed.timelineOrdinal = 0;
        multiSeed.mode = multiPlanning.requiredMode;
        multiSeed.activeMembers = {1, 3};
        multiSeed.updatePayloads = {
            {1, MakeCanonicalPayload(multiA1, multiB1)},
            {3, MakeCanonicalPayload(multiA3, multiB3)}};
        auto multiSeedFrozen = tests::BuildFrozenInvocation(
            multiSession.value().Package(), multiPlanning.deviceEpoch,
            std::move(multiSeed), std::move(multiPlanning.previousHistory));
        Require(static_cast<bool>(multiSeedFrozen) &&
            multiSeedFrozen.value().ExecutionPayload().routes.size() == 2 &&
            multiSeedFrozen.value().ExecutionPayload().canonicalMemberBytes == 32,
            "Multi-target payloadをSGE4INV 1.5へFreezeできませんでした。");
        Require(static_cast<bool>(multiSession.value().ValidateForSubmission(
            multiSeedFrozen.value())),
            "Multi-target InitialSeedのRuntime検証に失敗しました。");
        auto multiPrepared = multiSession.value().PrepareDynamicExecution(
            multiSeedFrozen.value());
        Require(multiPrepared && multiPrepared.value().bindings.size() == 2 &&
            multiPrepared.value().appliedTransitionCount == 2,
            "Multi-target InitialSeedのroute shadowが準備されませんでした。");
        for (std::size_t routeIndex = 0; routeIndex < 2; ++routeIndex)
        {
            const auto& route = multiSession.value().Package().DynamicContract().executionRoutes[routeIndex];
            std::vector<std::byte> expected(4u * route.routeMemberBytes, std::byte{0});
            const auto payloadOne = MakeCanonicalPayload(multiA1, multiB1);
            const auto payloadThree = MakeCanonicalPayload(multiA3, multiB3);
            std::copy_n(payloadOne.begin() + route.sourceByteOffset,
                route.routeMemberBytes,
                expected.begin() + route.routeMemberBytes);
            std::copy_n(payloadThree.begin() + route.sourceByteOffset,
                route.routeMemberBytes,
                expected.begin() + 3u * route.routeMemberBytes);
            Require(multiPrepared.value().bindings[routeIndex].denseSlotBytes == expected,
                "Canonical member sliceが対応route shadowへ反映されませんでした。");
        }
        multiSession.value().CommitSubmission(
            multiSeedFrozen.value(), std::move(multiPrepared).value());

        // A malformed route table must be rejected before any Dynamic planning.
        auto invalidMulti = tests::BuildMultiTargetVerifiedDynamicUnified(4);
        Require(static_cast<bool>(invalidMulti),
            "Multi-target negative testの正本生成に失敗しました。");
        auto invalidContract = invalidMulti.value().DynamicContract();
        std::swap(invalidContract.executionRoutes[0], invalidContract.executionRoutes[1]);
        Require(!composition::FreezeVerifiedCompositionPackage(
            invalidMulti.value().VerifiedComposition().ValidatedContract(),
            invalidMulti.value().VerifiedComposition().VerifiedPlan(),
            std::move(invalidContract)),
            "非Canonical順序のDynamic route集合が受理されました。");

        auto duplicateMulti = tests::BuildMultiTargetVerifiedDynamicUnified(4);
        Require(static_cast<bool>(duplicateMulti),
            "Multi-target duplicate negative testの正本生成に失敗しました。");
        auto duplicateContract = duplicateMulti.value().DynamicContract();
        duplicateContract.executionRoutes[1].targetLeaf =
            duplicateContract.executionRoutes[0].targetLeaf;
        duplicateContract.executionRoutes[1].targetDynamicSlot =
            duplicateContract.executionRoutes[0].targetDynamicSlot;
        Require(!composition::FreezeVerifiedCompositionPackage(
            duplicateMulti.value().VerifiedComposition().ValidatedContract(),
            duplicateMulti.value().VerifiedComposition().VerifiedPlan(),
            std::move(duplicateContract)),
            "同一Leaf／Slotを二重所有するDynamic route集合が受理されました。");

        auto outOfRangeMulti = tests::BuildMultiTargetVerifiedDynamicUnified(4);
        Require(static_cast<bool>(outOfRangeMulti),
            "Multi-target slice negative testの正本生成に失敗しました。");
        auto outOfRangeContract = outOfRangeMulti.value().DynamicContract();
        outOfRangeContract.executionRoutes[0].sourceByteOffset = 24;
        Require(!composition::FreezeVerifiedCompositionPackage(
            outOfRangeMulti.value().VerifiedComposition().ValidatedContract(),
            outOfRangeMulti.value().VerifiedComposition().VerifiedPlan(),
            std::move(outOfRangeContract)),
            "Canonical payload範囲外のDynamic route sliceが受理されました。");

        auto overlapInput = tests::BuildLinearInput();
        Require(static_cast<bool>(overlapInput), "Conditional overlap入力の生成に失敗しました。");
        std::vector<composition::ConditionalRegionV1> overlapRegions;
        overlapRegions.push_back(composition::MakeConditionalRegionV1(
            0, composition::ConditionalPredicateKindV1::ActiveSetNonEmpty, {{0}}));
        overlapRegions.push_back(composition::MakeConditionalRegionV1(
            1, composition::ConditionalPredicateKindV1::TransitionSetNonEmpty, {{0}}));
        Require(!composition::BuildFrozenCompositionPackage(
            std::move(overlapInput).value(),
            composition::MakeAuthorityOnlyDynamicContractV1(8, std::move(overlapRegions))),
            "同一Leafを複数Regionへ所属させるContractが受理されました。");

        auto crossBranchInput = tests::BuildLinearInput();
        Require(static_cast<bool>(crossBranchInput), "Conditional branch入力の生成に失敗しました。");
        const auto firstKey = composition::ComputeStableLeafKey("unified/linear/first");
        const auto secondKey = composition::ComputeStableLeafKey("unified/linear/second");
        const composition::LeafPackageId firstLeaf{firstKey < secondKey ? 0u : 1u};
        const composition::LeafPackageId secondLeaf{firstLeaf.value == 0u ? 1u : 0u};
        std::vector<composition::ConditionalRegionV1> crossRegions;
        crossRegions.push_back(composition::MakeConditionalRegionV1(
            0, composition::ConditionalPredicateKindV1::ActiveSetNonEmpty,
            {firstLeaf}, {secondLeaf}));
        Require(!composition::BuildFrozenCompositionPackage(
            std::move(crossBranchInput).value(),
            composition::MakeAuthorityOnlyDynamicContractV1(8, std::move(crossRegions))),
            "異なるConditional branchを跨ぐResource Flowが受理されました。");

        auto indirectPackage = tests::BuildVerifiedIndirectUnified(8);
        if (!indirectPackage)
            throw std::runtime_error(
                "Verified Indirect Compositionの生成に失敗しました：" +
                indirectPackage.error());
        const auto& indirectContract = indirectPackage.value().DynamicContract().indirectDispatch;
        Require(indirectPackage.value().DynamicContract().schemaVersion ==
            composition::artifact::FrozenCompositionAbi2DynamicContractSchema &&
            indirectContract.mode == composition::IndirectExecutionModeV1::VerifiedDispatch &&
            indirectContract.targetLeaf.IsValid() &&
            indirectContract.targetComputeCommand == 0 &&
            indirectContract.maxWorkCount == 8,
            "Verified indirect dispatch契約がSGE4UNI 2.7へ固定されませんでした。");
        Require(!tests::BuildVerifiedIndirectUnified(8, 7),
            "static Compute Commandと異なるmaxWorkCountが受理されました。");

        dynamic::InvocationInputV1 indirectZeroInput;
        indirectZeroInput.timelineOrdinal = 0;
        indirectZeroInput.mode = dynamic::InvocationModeV1::InitialSeed;
        auto indirectZero = tests::BuildFrozenInvocation(
            indirectPackage.value(), *epoch, std::move(indirectZeroInput));
        Require(indirectZero &&
            indirectZero.value().IndirectDispatch().workCount == 0 &&
            indirectZero.value().IndirectDispatch().threadGroupCountX == 0 &&
            indirectZero.value().IndirectDispatch().identity ==
                indirectZero.value().Artifact().IndirectDispatchIdentityValue(),
            "zero-work Verified indirect dispatchがSGE4INV 1.5へSealされませんでした。");

        dynamic::InvocationInputV1 indirectInput;
        indirectInput.timelineOrdinal = 0;
        indirectInput.mode = dynamic::InvocationModeV1::InitialSeed;
        indirectInput.activeMembers = {0, 2, 7};
        auto indirectRequest = dynamic::BuildDynamicInvocationRequest(
            indirectPackage.value(), *epoch, std::move(indirectInput));
        Require(static_cast<bool>(indirectRequest),
            "Verified indirect requestの生成に失敗しました。");
        auto indirectProposal = dynamic::DynamicInvocationPlannerV1::Plan(
            indirectRequest.value());
        Require(indirectProposal.Planned() &&
            indirectProposal.proposal->decision.indirectDispatch.workCount == 3 &&
            indirectProposal.proposal->decision.indirectDispatch.threadGroupCountX == 3,
            "exact transition countがVerified dispatch引数へ導出されませんでした。");
        auto indirectVerified = dynamic::DynamicInvocationVerifierV1::Verify(
            indirectRequest.value(), *indirectProposal.proposal);
        Require(indirectVerified.Accepted(),
            "Verified indirect Decisionが独立Verifierに拒否されました。");
        auto tamperedIndirect = *indirectProposal.proposal;
        tamperedIndirect.decision.indirectDispatch.threadGroupCountX = 4;
        Require(!dynamic::DynamicInvocationVerifierV1::Verify(
            indirectRequest.value(), tamperedIndirect).Accepted(),
            "改竄されたindirect dispatch引数が独立Verifierに受理されました。");
        auto frozenIndirect = dynamic::FreezeVerifiedInvocation(*indirectVerified.verified);
        Require(frozenIndirect &&
            frozenIndirect.value().IndirectDispatch().workCount == 3,
            "Verified indirect DecisionをSGE4INV 1.5へFreezeできませんでした。");
        auto indirectArtifact = sge4::ReadSectionedArtifact(
            frozenIndirect.value().FileBytes(), dynamic::FrozenInvocationMagic,
            dynamic::FrozenInvocationFormatMajor);
        Require(indirectArtifact &&
            indirectArtifact.value().FormatMinor() == dynamic::FrozenInvocationFormatMinor &&
            indirectArtifact.value().Sections().size() == dynamic::FrozenInvocationSectionKinds.size() &&
            indirectArtifact.value().FindSection(
                std::to_underlying(dynamic::FrozenInvocationSectionKind::IndirectDispatch)) != nullptr,
            "SGE4INV 1.5のIndirect Dispatch Sectionが成立していません。");

        auto indirectSession = sge4::runtime::Session::Create(
            std::move(indirectPackage).value(), 1);
        Require(static_cast<bool>(indirectSession) &&
            static_cast<bool>(indirectSession.value().ValidateForSubmission(
                indirectZero.value())),
            "Runtime Sessionがzero-work Verified indirect成果物を受理できませんでした。");
        auto preparedIndirect = indirectSession.value().PrepareDynamicExecution(
            indirectZero.value());
        Require(preparedIndirect && preparedIndirect.value().hasIndirectDispatch &&
            preparedIndirect.value().verifiedTransitionCount == 0 &&
            preparedIndirect.value().appliedTransitionCount == 0 &&
            preparedIndirect.value().indirectWorkCount == 0 &&
            preparedIndirect.value().indirectThreadGroupCountX == 0,
            "RuntimeがSeal済みzero-work dispatchを機械的に準備できませんでした。");

        Require(static_cast<bool>(indirectSession.value().ValidateForSubmission(
                frozenIndirect.value())),
            "Runtime Sessionがwork count 3のVerified indirect成果物を受理できませんでした。");
        auto preparedIndirectThree = indirectSession.value().PrepareDynamicExecution(
            frozenIndirect.value());
        Require(preparedIndirectThree &&
            preparedIndirectThree.value().verifiedTransitionCount == 3 &&
            preparedIndirectThree.value().appliedTransitionCount == 0 &&
            preparedIndirectThree.value().indirectWorkCount == 3 &&
            preparedIndirectThree.value().indirectThreadGroupCountX == 3,
            "AuthorityOnlyのexact transition件数とGPU indirect work件数が分離されませんでした。");

        auto textureFirst = tests::BuildLimitedTexture2DUnified();
        auto textureSecond = tests::BuildLimitedTexture2DUnified();
        if (!textureFirst || !textureSecond)
            throw std::runtime_error(
                "限定Texture2D Compositionの生成に失敗しました：" +
                (textureFirst ? textureSecond.error() : textureFirst.error()));
        Require(textureFirst.value().FileBytes().size() == textureSecond.value().FileBytes().size() &&
            std::equal(textureFirst.value().FileBytes().begin(), textureFirst.value().FileBytes().end(),
                textureSecond.value().FileBytes().begin()),
            "限定Texture2D Compositionがbyte決定的ではありません。");
        const auto& textureContract = textureFirst.value().VerifiedComposition().ValidatedContract().Contract();
        Require(textureContract.resources.size() == 2 &&
            std::ranges::all_of(textureContract.resources, [](const auto& resource) {
                return resource.kind == sge4::package::d3d12_v13::ResourceKind::Texture2D &&
                    resource.format == sge4::package::d3d12_v13::Format::B8G8R8A8Unorm &&
                    resource.sizeBytes == 0 && resource.texture2D.width == 4 &&
                    resource.texture2D.height == 4 && resource.texture2D.rowBytes == 16 &&
                    resource.texture2D.mipLevels == 1 && resource.texture2D.arrayLayers == 1 &&
                    resource.texture2D.sampleCount == 1 && resource.texture2D.planeCount == 1;
            }),
            "限定Texture2D Flow形状がFrozen Contractへ固定されませんでした。");
        Require(std::ranges::all_of(
            textureFirst.value().VerifiedComposition().VerifiedPlan().Plan().allocations,
            [](const auto& allocation) {
                return allocation.kind == sge4::package::d3d12_v13::ResourceKind::Texture2D &&
                    allocation.sizeBytes == 64 && allocation.texture2D.width == 4 &&
                    allocation.texture2D.height == 4;
            }),
            "限定Texture2D allocationがVerified Planへ固定されませんでした。");
        auto textureRoundTrip = composition::ReadFrozenCompositionPackage(
            textureFirst.value().FileBytes());
        Require(static_cast<bool>(textureRoundTrip) &&
            textureRoundTrip.value().SemanticDigest() == textureFirst.value().SemanticDigest(),
            "限定Texture2D FlowのSGE4UNI 2.7 round-tripに失敗しました。");

        auto mismatchProducer = fixture::BuildTextureProducerLeaf(4, 4);
        auto mismatchConsumer = fixture::BuildTextureConsumerLeaf(2, 2);
        Require(mismatchProducer && mismatchConsumer,
            "Texture shape mismatch Fixtureの生成に失敗しました。");
        composition::ContractBuildInput mismatchInput;
        mismatchInput.leaves = {
            fixture::TextureProducerDeclaration("unified/texture/mismatch/producer", mismatchProducer.value()),
            fixture::TextureConsumerDeclaration("unified/texture/mismatch/consumer", mismatchConsumer.value())};
        composition::ResourceFlowDeclaration mismatchMiddle;
        mismatchMiddle.stableKey = "unified/texture/mismatch/intermediate";
        mismatchMiddle.boundary = composition::ResourceBoundary::Internal;
        mismatchMiddle.producer = fixture::Ref(
            "unified/texture/mismatch/producer", std::string(fixture::TextureOutputEndpoint));
        mismatchMiddle.consumers = {fixture::Ref(
            "unified/texture/mismatch/consumer", std::string(fixture::TextureInputEndpoint))};
        composition::ResourceFlowDeclaration mismatchOutput;
        mismatchOutput.stableKey = "unified/texture/mismatch/output";
        mismatchOutput.boundary = composition::ResourceBoundary::CompositionOutput;
        mismatchOutput.producer = fixture::Ref(
            "unified/texture/mismatch/consumer", std::string(fixture::TextureOutputEndpoint));
        mismatchInput.resources = {std::move(mismatchMiddle), std::move(mismatchOutput)};
        Require(!composition::BuildFrozenCompositionPackage(
            std::move(mismatchInput), composition::MakeAuthorityOnlyDynamicContractV1(1)),
            "異なるextentのTexture2D endpointが同一Flowとして受理されました。");

        auto textureUavFirst = tests::BuildLimitedTexture2DUavUnified();
        auto textureUavSecond = tests::BuildLimitedTexture2DUavUnified();
        if (!textureUavFirst || !textureUavSecond)
            throw std::runtime_error(
                "限定Texture2D UAV Compositionの生成に失敗しました：" +
                (textureUavFirst ? textureUavSecond.error() : textureUavFirst.error()));
        Require(textureUavFirst.value().FileBytes().size() == textureUavSecond.value().FileBytes().size() &&
            std::equal(textureUavFirst.value().FileBytes().begin(), textureUavFirst.value().FileBytes().end(),
                textureUavSecond.value().FileBytes().begin()),
            "限定Texture2D UAV Compositionがbyte決定的ではありません。");
        const auto& textureUavContract =
            textureUavFirst.value().VerifiedComposition().ValidatedContract().Contract();
        const auto rgbaFormat = sge4::package::d3d12_v13::Format::R32G32B32A32Float;
        const auto bgraFormat = sge4::package::d3d12_v13::Format::B8G8R8A8Unorm;
        Require(textureUavContract.resources.size() == 2 &&
            std::ranges::count_if(textureUavContract.resources, [=](const auto& resource) {
                return resource.kind == sge4::package::d3d12_v13::ResourceKind::Texture2D &&
                    resource.format == rgbaFormat && resource.texture2D.width == 4 &&
                    resource.texture2D.height == 4 && resource.texture2D.rowBytes == 64;
            }) == 1 &&
            std::ranges::count_if(textureUavContract.resources, [=](const auto& resource) {
                return resource.kind == sge4::package::d3d12_v13::ResourceKind::Texture2D &&
                    resource.format == bgraFormat && resource.texture2D.width == 4 &&
                    resource.texture2D.height == 4 && resource.texture2D.rowBytes == 16;
            }) == 1,
            "限定Texture2D UAV／BGRA output形状がFrozen Contractへ固定されませんでした。");
        const auto unorderedBits = static_cast<std::uint32_t>(
            sge4::package::d3d12_v13::ExplicitStateBits::UnorderedWrite);
        const auto pixelReadBits = static_cast<std::uint32_t>(
            sge4::package::d3d12_v13::ExplicitStateBits::PixelShaderRead);
        const auto uavWriter = std::ranges::find_if(textureUavContract.endpoints, [=](const auto& endpoint) {
            return endpoint.format == rgbaFormat && endpoint.access == composition::EndpointAccess::WriteOnly;
        });
        const auto sampledReader = std::ranges::find_if(textureUavContract.endpoints, [=](const auto& endpoint) {
            return endpoint.format == rgbaFormat && endpoint.access == composition::EndpointAccess::ReadOnly;
        });
        Require(uavWriter != textureUavContract.endpoints.end() &&
            sampledReader != textureUavContract.endpoints.end() &&
            uavWriter->requiredIncomingState.explicitBits == unorderedBits &&
            uavWriter->guaranteedOutgoingState.explicitBits == unorderedBits &&
            sampledReader->requiredIncomingState.explicitBits == pixelReadBits &&
            sampledReader->guaranteedOutgoingState.explicitBits == pixelReadBits,
            "Texture2D UAV write／SRV read state契約がFrozen endpointへ固定されませんでした。");
        const auto& textureUavPlan =
            textureUavFirst.value().VerifiedComposition().VerifiedPlan().Plan();
        Require(std::ranges::any_of(textureUavPlan.handoffs, [=](const auto& handoff) {
            return handoff.producerOutgoingState.explicitBits == unorderedBits &&
                handoff.consumerIncomingState.explicitBits == pixelReadBits;
        }),
            "Texture2D UAVからSRVへのstate handoffがVerified Planへ固定されませんでした。");
        Require(std::ranges::any_of(textureUavPlan.allocations, [=](const auto& allocation) {
            return allocation.format == rgbaFormat && allocation.sizeBytes == 256;
        }),
            "RGBA32F Texture2D UAV allocationがVerified Planへ固定されませんでした。");
        auto textureUavRoundTrip = composition::ReadFrozenCompositionPackage(
            textureUavFirst.value().FileBytes());
        Require(textureUavRoundTrip &&
            textureUavRoundTrip.value().SemanticDigest() == textureUavFirst.value().SemanticDigest(),
            "限定Texture2D UAV FlowのSGE4UNI 2.7 round-tripに失敗しました。");

        auto formatMismatchProducer = fixture::BuildTextureUavProducerLeaf(4, 4);
        auto formatMismatchConsumer = fixture::BuildTextureConsumerLeaf(4, 4);
        Require(formatMismatchProducer && formatMismatchConsumer,
            "Texture UAV format mismatch Fixtureの生成に失敗しました。");
        composition::ContractBuildInput formatMismatchInput;
        formatMismatchInput.leaves = {
            fixture::TextureUavProducerDeclaration(
                "unified/texture-uav/mismatch/producer", formatMismatchProducer.value()),
            fixture::TextureConsumerDeclaration(
                "unified/texture-uav/mismatch/consumer", formatMismatchConsumer.value())};
        composition::ResourceFlowDeclaration formatMismatchMiddle;
        formatMismatchMiddle.stableKey = "unified/texture-uav/mismatch/intermediate";
        formatMismatchMiddle.boundary = composition::ResourceBoundary::Internal;
        formatMismatchMiddle.producer = fixture::Ref(
            "unified/texture-uav/mismatch/producer",
            std::string(fixture::TextureUavOutputEndpoint));
        formatMismatchMiddle.consumers = {fixture::Ref(
            "unified/texture-uav/mismatch/consumer",
            std::string(fixture::TextureInputEndpoint))};
        composition::ResourceFlowDeclaration formatMismatchOutput;
        formatMismatchOutput.stableKey = "unified/texture-uav/mismatch/output";
        formatMismatchOutput.boundary = composition::ResourceBoundary::CompositionOutput;
        formatMismatchOutput.producer = fixture::Ref(
            "unified/texture-uav/mismatch/consumer",
            std::string(fixture::TextureOutputEndpoint));
        formatMismatchInput.resources = {
            std::move(formatMismatchMiddle), std::move(formatMismatchOutput)};
        Require(!composition::BuildFrozenCompositionPackage(
            std::move(formatMismatchInput),
            composition::MakeAuthorityOnlyDynamicContractV1(1)),
            "RGBA32F UAV producerとBGRA8 consumerのformat不一致が受理されました。");

        auto temporalBytes = fixture::BuildTemporalBufferArtifact();
        if (!temporalBytes)
            throw std::runtime_error(
                "Verified Temporal Buffer Compositionの生成に失敗しました：" +
                temporalBytes.error());
        auto temporalArtifact = composition::ReadFrozenCompositionPackage(
            temporalBytes.value());
        Require(static_cast<bool>(temporalArtifact),
            "Verified Temporal Buffer SGE4UNI 2.7の読込みに失敗しました。");
        const auto& temporalContract =
            temporalArtifact.value().VerifiedComposition().ValidatedContract().Contract();
        const auto temporalResource = std::ranges::find_if(
            temporalContract.resources, [](const auto& value) {
                return value.lifetime == composition::ResourceFlowLifetime::TemporalHistory;
            });
        Require(temporalResource != temporalContract.resources.end() &&
            temporalResource->boundary == composition::ResourceBoundary::Internal &&
            temporalResource->kind == sge4::package::d3d12_v13::ResourceKind::Buffer &&
            temporalResource->sizeBytes == 16 && temporalResource->historyDepth == 1 &&
            temporalResource->producer.IsValid() &&
            temporalResource->consumers.size() == 1,
            "Temporal BufferのPrevious／Current契約がFrozen Contractへ固定されませんでした。");
        const auto& temporalPlan =
            temporalArtifact.value().VerifiedComposition().VerifiedPlan().Plan();
        Require(temporalPlan.temporalBuffers.size() == 1 &&
            temporalPlan.temporalBuffers.front().resource == temporalResource->id &&
            temporalPlan.temporalBuffers.front().historyDepth == 1 &&
            temporalPlan.temporalBuffers.front().physicalInstanceCount == 2 &&
            temporalPlan.temporalBuffers.front().previousConsumers == temporalResource->consumers,
            "Temporal Bufferの二世代Planが独立Verifierへ固定されませんでした。");
        Require(temporalPlan.allocations[temporalResource->id.value].lifetime ==
                composition::ResourceFlowLifetime::TemporalHistory &&
            temporalPlan.allocations[temporalResource->id.value].physicalInstanceCount == 2 &&
            std::ranges::none_of(temporalPlan.handoffs,
                [&](const auto& value) { return value.resource == temporalResource->id; }) &&
            std::ranges::none_of(temporalPlan.signals,
                [&](const auto& value) { return value.resource == temporalResource->id; }) &&
            std::ranges::none_of(temporalPlan.waits,
                [&](const auto& value) { return value.resource == temporalResource->id; }),
            "Temporal Bufferがsame-frame handoff／waitへ誤って混入しました。");
        Require(temporalPlan.schedule.size() == 2 &&
            temporalPlan.schedule[0].leaf.value == 0 &&
            temporalPlan.schedule[1].leaf.value == 1,
            "Temporal edgeがsame-frame Schedule依存として扱われました。");

        auto badTemporalProducer = fixture::BuildTemporalProducerLeaf();
        auto badTemporalConsumer = fixture::BuildTransformLeaf();
        Require(badTemporalProducer && badTemporalConsumer,
            "Temporal negative Fixtureの生成に失敗しました。");
        composition::ContractBuildInput badTemporalInput;
        badTemporalInput.leaves = {
            fixture::TemporalProducerDeclaration(
                "l4g7/negative/producer", badTemporalProducer.value()),
            fixture::TransformDeclaration(
                "l4g7/negative/consumer", badTemporalConsumer.value())};
        composition::ResourceFlowDeclaration badHistory;
        badHistory.stableKey = "l4g7/negative/history";
        badHistory.boundary = composition::ResourceBoundary::Internal;
        badHistory.lifetime = composition::ResourceFlowLifetime::TemporalHistory;
        badHistory.historyDepth = 0;
        badHistory.producer = fixture::Ref(
            "l4g7/negative/producer", std::string(fixture::OutputEndpoint));
        badHistory.consumers = {fixture::Ref(
            "l4g7/negative/consumer", std::string(fixture::InputEndpoint))};
        composition::ResourceFlowDeclaration badOutput;
        badOutput.stableKey = "l4g7/negative/output";
        badOutput.boundary = composition::ResourceBoundary::CompositionOutput;
        badOutput.producer = fixture::Ref(
            "l4g7/negative/consumer", std::string(fixture::OutputEndpoint));
        badTemporalInput.resources = {std::move(badHistory), std::move(badOutput)};
        Require(!composition::BuildFrozenCompositionPackage(
            std::move(badTemporalInput),
            composition::MakeAuthorityOnlyDynamicContractV1(1)),
            "history depth 0のTemporal Bufferが受理されました。");

        tests::VerifyAbi2CorruptionRejection(first.value().FileBytes());

        std::cout << "New SGE4統合設計試験に合格しました。\n";
        std::cout << "Frozen Composition ABI：SGE4UNI 2.7\n";
        std::cout << "Frozen Dynamic Invocation ABI：SGE4INV 1.5\n";
        std::cout << "Frozen Leaf成果物数：2\n資源接続数：3\n対象要素数：8\n";
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "New SGE4統合設計試験に失敗しました：" << exception.what() << '\n';
        return 1;
    }
}
