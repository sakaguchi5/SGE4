#include "../fixtures/UnifiedFixture.h"
#include "Abi1GoldenBytes.h"
#include "Abi2CorruptionTests.h"
#include "Abi2PortableSelfTest.h"
#include "../../src/composition/migration/abi1/FrozenCompositionAbi1Migration.h"
#include "../../src/composition/artifact/abi2/FrozenCompositionAbi2.h"
#include "../../src/canonical/artifact/SectionedArtifact.h"

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
            "SGE4UNI 2.1の平坦Section構造が成立していません。");
        Require(flat.value().FindSection(
            std::to_underlying(composition::artifact::FrozenCompositionAbi2SectionKind::LeafTable)) != nullptr &&
            flat.value().FindSection(
            std::to_underlying(composition::artifact::FrozenCompositionAbi2SectionKind::AuthorityLedger)) != nullptr,
            "SGE4UNI 2.1の直接Sectionがありません。");

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
        Require(static_cast<bool>(migrated), "SGE4UNI 1.1から2.1へのMigrationに失敗しました。");
        Require(migrated.value().FileBytes().size() == first.value().FileBytes().size() &&
            std::equal(migrated.value().FileBytes().begin(), migrated.value().FileBytes().end(),
                first.value().FileBytes().begin()),
            "直接生成したABI 2.1とMigration後ABI 2.1がbyte一致しません。");
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
            "SGE4INV 1.2のSection構造が成立していません。");

        dynamic::InvocationInputV1 missingPayload;
        missingPayload.timelineOrdinal = 0;
        missingPayload.mode = dynamic::InvocationModeV1::InitialSeed;
        missingPayload.activeMembers = {1, 3};
        missingPayload.updatePayloads = {
            {1, fixture::Bytes(std::array<float, 4>{1.0f, 2.0f, 3.0f, 4.0f})}};
        Require(!tests::BuildFrozenInvocation(
            verifiedDynamic.value(), *epoch, std::move(missingPayload)),
            "exact update setを満たさないpayloadが受理されました。");

        tests::VerifyAbi2CorruptionRejection(first.value().FileBytes());

        std::cout << "New SGE4統合設計試験に合格しました。\n";
        std::cout << "Frozen Composition ABI：SGE4UNI 2.1\n";
        std::cout << "Frozen Dynamic Invocation ABI：SGE4INV 1.2\n";
        std::cout << "Frozen Leaf成果物数：2\n資源接続数：3\n対象要素数：8\n";
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "New SGE4統合設計試験に失敗しました：" << exception.what() << '\n';
        return 1;
    }
}
