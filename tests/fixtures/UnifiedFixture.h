#pragma once

#include "./RuntimeFixture.h"
#include "../../src/composition/toolchain/CompositionToolchain.h"
#include "../../src/dynamic/artifact/DynamicInvocationPackage.h"
#include "../../src/dynamic/planner/DynamicInvocationPlanner.h"
#include "../../src/dynamic/verifier/DynamicInvocationVerifier.h"

#include <string>
#include <string_view>
#include <utility>

namespace sge4::tests
{
namespace fixture = sge4::qualification::canonical_runtime_fixture;
namespace contract = sge4::composition;

inline sge4::base::Expected<contract::ContractBuildInput, std::string> BuildLinearInput()
{
    auto transform = fixture::BuildTransformLeaf();
    if (!transform)
        return sge4::base::Failure<contract::ContractBuildInput, std::string>(transform.error());
    contract::ContractBuildInput input;
    input.leaves = {
        fixture::TransformDeclaration("unified/linear/first", transform.value()),
        fixture::TransformDeclaration("unified/linear/second", transform.value())};

    contract::ResourceFlowDeclaration source;
    source.stableKey = "unified/linear/input";
    source.boundary = contract::ResourceBoundary::CompositionInput;
    source.consumers = {fixture::Ref("unified/linear/first", std::string(fixture::InputEndpoint))};

    contract::ResourceFlowDeclaration middle;
    middle.stableKey = "unified/linear/middle";
    middle.boundary = contract::ResourceBoundary::Internal;
    middle.producer = fixture::Ref("unified/linear/first", std::string(fixture::OutputEndpoint));
    middle.consumers = {fixture::Ref("unified/linear/second", std::string(fixture::InputEndpoint))};

    contract::ResourceFlowDeclaration output;
    output.stableKey = "unified/linear/output";
    output.boundary = contract::ResourceBoundary::CompositionOutput;
    output.producer = fixture::Ref("unified/linear/second", std::string(fixture::OutputEndpoint));
    input.resources = {std::move(source), std::move(middle), std::move(output)};
    return sge4::base::Success<contract::ContractBuildInput, std::string>(std::move(input));
}

inline sge4::base::Expected<composition::FrozenCompositionPackage, std::string> BuildLinearUnified(
    std::uint32_t universe = 8)
{
    auto input = BuildLinearInput();
    if (!input)
        return sge4::base::Failure<composition::FrozenCompositionPackage, std::string>(input.error());
    auto built = composition::BuildFrozenCompositionPackage(
        std::move(input).value(),
        composition::MakeAuthorityOnlyDynamicContractV1(universe));
    if (!built)
        return sge4::base::Failure<composition::FrozenCompositionPackage, std::string>(
            built.error().stage + "：" + built.error().message);
    return sge4::base::Success<composition::FrozenCompositionPackage, std::string>(
        std::move(built).value());
}

inline sge4::base::Expected<composition::FrozenCompositionPackage, std::string>
BuildVerifiedDynamicUnified(std::uint32_t universe = 4)
{
    constexpr std::string_view ExecutorKey = "unified/dynamic/executor";
    constexpr std::string_view ObserverKey = "unified/dynamic/observer";

    auto leaf = fixture::BuildVerifiedDynamicLeaf(universe);
    if (!leaf)
        return sge4::base::Failure<composition::FrozenCompositionPackage, std::string>(
            leaf.error());
    auto observer = fixture::BuildDynamicObservationLeaf(universe);
    if (!observer)
        return sge4::base::Failure<composition::FrozenCompositionPackage, std::string>(
            observer.error());

    contract::ContractBuildInput input;
    input.leaves = {
        fixture::VerifiedDynamicDeclaration(std::string(ExecutorKey), leaf.value()),
        fixture::DynamicObservationDeclaration(std::string(ObserverKey), observer.value())};

    contract::ResourceFlowDeclaration middle;
    middle.stableKey = "unified/dynamic/materialized";
    middle.boundary = contract::ResourceBoundary::Internal;
    middle.producer = fixture::Ref(
        std::string(ExecutorKey), std::string(fixture::DynamicOutputEndpoint));
    middle.consumers = {fixture::Ref(
        std::string(ObserverKey), std::string(fixture::DynamicObservationInputEndpoint))};

    contract::ResourceFlowDeclaration output;
    output.stableKey = "unified/dynamic/output";
    output.boundary = contract::ResourceBoundary::CompositionOutput;
    output.producer = fixture::Ref(
        std::string(ObserverKey), std::string(fixture::DynamicObservationOutputEndpoint));
    input.resources = {std::move(middle), std::move(output)};

    // Composition canonicalizes Leaf IDs by stable-key order. Resolve the
    // execution target from the same identity rule instead of assuming that
    // authored declaration order survives canonicalization.
    const auto executorStableKey = composition::ComputeStableLeafKey(ExecutorKey);
    const auto observerStableKey = composition::ComputeStableLeafKey(ObserverKey);
    const composition::LeafPackageId executorLeaf{
        executorStableKey < observerStableKey ? 0u : 1u};

    auto built = composition::BuildFrozenCompositionPackage(
        std::move(input), composition::MakeVerifiedDenseSlotDynamicContractV1(
            universe, executorLeaf, 0, 16));
    if (!built)
        return sge4::base::Failure<composition::FrozenCompositionPackage, std::string>(
            built.error().stage + "：" + built.error().message);
    return sge4::base::Success<composition::FrozenCompositionPackage, std::string>(
        std::move(built).value());
}

inline sge4::base::Expected<composition::FrozenCompositionPackage, std::string>
BuildMultiTargetVerifiedDynamicUnified(std::uint32_t universe = 4)
{
    constexpr std::string_view FirstKey = "unified/multi-dynamic/first";
    constexpr std::string_view SecondKey = "unified/multi-dynamic/second";

    auto first = fixture::BuildVerifiedDynamicLeaf(universe);
    if (!first)
        return sge4::base::Failure<composition::FrozenCompositionPackage, std::string>(
            first.error());
    auto second = fixture::BuildVerifiedDynamicLeaf(universe);
    if (!second)
        return sge4::base::Failure<composition::FrozenCompositionPackage, std::string>(
            second.error());

    contract::ContractBuildInput input;
    input.leaves = {
        fixture::VerifiedDynamicDeclaration(std::string(FirstKey), first.value()),
        fixture::VerifiedDynamicDeclaration(std::string(SecondKey), second.value())};

    contract::ResourceFlowDeclaration firstOutput;
    firstOutput.stableKey = "unified/multi-dynamic/output-first";
    firstOutput.boundary = contract::ResourceBoundary::CompositionOutput;
    firstOutput.producer = fixture::Ref(
        std::string(FirstKey), std::string(fixture::DynamicOutputEndpoint));

    contract::ResourceFlowDeclaration secondOutput;
    secondOutput.stableKey = "unified/multi-dynamic/output-second";
    secondOutput.boundary = contract::ResourceBoundary::CompositionOutput;
    secondOutput.producer = fixture::Ref(
        std::string(SecondKey), std::string(fixture::DynamicOutputEndpoint));
    input.resources = {std::move(firstOutput), std::move(secondOutput)};

    const auto firstStable = composition::ComputeStableLeafKey(FirstKey);
    const auto secondStable = composition::ComputeStableLeafKey(SecondKey);
    const composition::LeafPackageId firstLeaf{firstStable < secondStable ? 0u : 1u};
    const composition::LeafPackageId secondLeaf{firstStable < secondStable ? 1u : 0u};

    std::vector<composition::DynamicExecutionRouteV1> routes = {
        composition::MakeDynamicExecutionRouteV1(firstLeaf, 0, 0, 16),
        composition::MakeDynamicExecutionRouteV1(secondLeaf, 0, 16, 16)};
    std::sort(routes.begin(), routes.end(), [](const auto& left, const auto& right) {
        return std::pair{left.targetLeaf.value, left.targetDynamicSlot} <
            std::pair{right.targetLeaf.value, right.targetDynamicSlot};
    });

    auto built = composition::BuildFrozenCompositionPackage(
        std::move(input), composition::MakeVerifiedRoutedSlotsDynamicContractV1(
            universe, 32, std::move(routes)));
    if (!built)
        return sge4::base::Failure<composition::FrozenCompositionPackage, std::string>(
            built.error().stage + "：" + built.error().message);
    return sge4::base::Success<composition::FrozenCompositionPackage, std::string>(
        std::move(built).value());
}

inline sge4::base::Expected<composition::FrozenCompositionPackage, std::string>
BuildConditionalVerifiedDynamicUnified(std::uint32_t universe = 4)
{
    constexpr std::string_view ExecutorKey = "unified/conditional/executor";
    constexpr std::string_view ObserverKey = "unified/conditional/observer";

    auto leaf = fixture::BuildVerifiedDynamicLeaf(universe);
    if (!leaf)
        return sge4::base::Failure<composition::FrozenCompositionPackage, std::string>(leaf.error());
    auto observer = fixture::BuildDynamicObservationLeaf(universe);
    if (!observer)
        return sge4::base::Failure<composition::FrozenCompositionPackage, std::string>(observer.error());

    contract::ContractBuildInput input;
    input.leaves = {
        fixture::VerifiedDynamicDeclaration(std::string(ExecutorKey), leaf.value()),
        fixture::DynamicObservationDeclaration(std::string(ObserverKey), observer.value())};

    contract::ResourceFlowDeclaration middle;
    middle.stableKey = "unified/conditional/materialized";
    middle.boundary = contract::ResourceBoundary::Internal;
    middle.producer = fixture::Ref(
        std::string(ExecutorKey), std::string(fixture::DynamicOutputEndpoint));
    middle.consumers = {fixture::Ref(
        std::string(ObserverKey), std::string(fixture::DynamicObservationInputEndpoint))};

    contract::ResourceFlowDeclaration output;
    output.stableKey = "unified/conditional/output";
    output.boundary = contract::ResourceBoundary::CompositionOutput;
    output.producer = fixture::Ref(
        std::string(ObserverKey), std::string(fixture::DynamicObservationOutputEndpoint));
    input.resources = {std::move(middle), std::move(output)};

    const auto executorStableKey = composition::ComputeStableLeafKey(ExecutorKey);
    const auto observerStableKey = composition::ComputeStableLeafKey(ObserverKey);
    const composition::LeafPackageId executorLeaf{
        executorStableKey < observerStableKey ? 0u : 1u};
    std::vector<composition::LeafPackageId> trueLeaves = {
        composition::LeafPackageId{0}, composition::LeafPackageId{1}};
    std::vector<composition::ConditionalRegionV1> regions;
    regions.push_back(composition::MakeConditionalRegionV1(
        0, composition::ConditionalPredicateKindV1::ActiveSetNonEmpty,
        std::move(trueLeaves)));

    auto built = composition::BuildFrozenCompositionPackage(
        std::move(input), composition::MakeVerifiedDenseSlotDynamicContractV1(
            universe, executorLeaf, 0, 16, std::move(regions)));
    if (!built)
        return sge4::base::Failure<composition::FrozenCompositionPackage, std::string>(
            built.error().stage + "：" + built.error().message);
    return sge4::base::Success<composition::FrozenCompositionPackage, std::string>(
        std::move(built).value());
}

inline sge4::base::Expected<composition::FrozenCompositionPackage, std::string>
BuildVerifiedIndirectUnified(
    std::uint32_t universe = 8,
    std::uint32_t contractMaxWorkCount = 0)
{
    constexpr std::string_view ProducerKey = "unified/indirect/producer";
    constexpr std::string_view ObserverKey = "unified/indirect/observer";

    auto producer = fixture::BuildVerifiedIndirectLeaf(universe);
    if (!producer)
        return sge4::base::Failure<composition::FrozenCompositionPackage, std::string>(
            producer.error());
    auto observer = fixture::BuildDynamicObservationLeaf(universe);
    if (!observer)
        return sge4::base::Failure<composition::FrozenCompositionPackage, std::string>(
            observer.error());

    contract::ContractBuildInput input;
    input.leaves = {
        fixture::VerifiedIndirectDeclaration(std::string(ProducerKey), producer.value()),
        fixture::DynamicObservationDeclaration(std::string(ObserverKey), observer.value())};

    contract::ResourceFlowDeclaration middle;
    middle.stableKey = "unified/indirect/materialized";
    middle.boundary = contract::ResourceBoundary::Internal;
    middle.producer = fixture::Ref(
        std::string(ProducerKey), std::string(fixture::IndirectOutputEndpoint));
    middle.consumers = {fixture::Ref(
        std::string(ObserverKey), std::string(fixture::DynamicObservationInputEndpoint))};

    contract::ResourceFlowDeclaration output;
    output.stableKey = "unified/indirect/output";
    output.boundary = contract::ResourceBoundary::CompositionOutput;
    output.producer = fixture::Ref(
        std::string(ObserverKey), std::string(fixture::DynamicObservationOutputEndpoint));
    input.resources = {std::move(middle), std::move(output)};

    const auto producerStableKey = composition::ComputeStableLeafKey(ProducerKey);
    const auto observerStableKey = composition::ComputeStableLeafKey(ObserverKey);
    const composition::LeafPackageId producerLeaf{
        producerStableKey < observerStableKey ? 0u : 1u};
    const auto indirect = composition::MakeVerifiedIndirectDispatchContractV1(
        producerLeaf, 0, contractMaxWorkCount == 0 ? universe : contractMaxWorkCount);
    auto built = composition::BuildFrozenCompositionPackage(
        std::move(input), composition::MakeAuthorityOnlyDynamicContractV1(
            universe, {}, indirect));
    if (!built)
        return sge4::base::Failure<composition::FrozenCompositionPackage, std::string>(
            built.error().stage + "：" + built.error().message);
    return sge4::base::Success<composition::FrozenCompositionPackage, std::string>(
        std::move(built).value());
}

inline sge4::base::Expected<composition::FrozenCompositionPackage, std::string>
BuildLimitedTexture2DUnified(std::uint32_t width = 4, std::uint32_t height = 4)
{
    constexpr std::string_view ProducerKey = "unified/texture/producer";
    constexpr std::string_view ConsumerKey = "unified/texture/consumer";
    auto producer = fixture::BuildTextureProducerLeaf(width, height);
    if (!producer)
        return sge4::base::Failure<composition::FrozenCompositionPackage, std::string>(
            producer.error());
    auto consumer = fixture::BuildTextureConsumerLeaf(width, height);
    if (!consumer)
        return sge4::base::Failure<composition::FrozenCompositionPackage, std::string>(
            consumer.error());

    contract::ContractBuildInput input;
    input.leaves = {
        fixture::TextureProducerDeclaration(std::string(ProducerKey), producer.value()),
        fixture::TextureConsumerDeclaration(std::string(ConsumerKey), consumer.value())};

    contract::ResourceFlowDeclaration middle;
    middle.stableKey = "unified/texture/intermediate";
    middle.boundary = contract::ResourceBoundary::Internal;
    middle.producer = fixture::Ref(
        std::string(ProducerKey), std::string(fixture::TextureOutputEndpoint));
    middle.consumers = {fixture::Ref(
        std::string(ConsumerKey), std::string(fixture::TextureInputEndpoint))};

    contract::ResourceFlowDeclaration output;
    output.stableKey = "unified/texture/output";
    output.boundary = contract::ResourceBoundary::CompositionOutput;
    output.producer = fixture::Ref(
        std::string(ConsumerKey), std::string(fixture::TextureOutputEndpoint));
    input.resources = {std::move(middle), std::move(output)};

    auto built = composition::BuildFrozenCompositionPackage(
        std::move(input), composition::MakeAuthorityOnlyDynamicContractV1(1));
    if (!built)
        return sge4::base::Failure<composition::FrozenCompositionPackage, std::string>(
            built.error().stage + "：" + built.error().message);
    return sge4::base::Success<composition::FrozenCompositionPackage, std::string>(
        std::move(built).value());
}

inline sge4::base::Expected<composition::FrozenCompositionPackage, std::string>
BuildLimitedTexture2DUavUnified(std::uint32_t width = 4, std::uint32_t height = 4)
{
    constexpr std::string_view ProducerKey = "unified/texture-uav/producer";
    constexpr std::string_view ConsumerKey = "unified/texture-uav/consumer";
    auto producer = fixture::BuildTextureUavProducerLeaf(width, height);
    if (!producer)
        return sge4::base::Failure<composition::FrozenCompositionPackage, std::string>(
            producer.error());
    auto consumer = fixture::BuildTextureFloatConsumerLeaf(width, height);
    if (!consumer)
        return sge4::base::Failure<composition::FrozenCompositionPackage, std::string>(
            consumer.error());

    contract::ContractBuildInput input;
    input.leaves = {
        fixture::TextureUavProducerDeclaration(std::string(ProducerKey), producer.value()),
        fixture::TextureConsumerDeclaration(std::string(ConsumerKey), consumer.value())};

    contract::ResourceFlowDeclaration middle;
    middle.stableKey = "unified/texture-uav/intermediate";
    middle.boundary = contract::ResourceBoundary::Internal;
    middle.producer = fixture::Ref(
        std::string(ProducerKey), std::string(fixture::TextureUavOutputEndpoint));
    middle.consumers = {fixture::Ref(
        std::string(ConsumerKey), std::string(fixture::TextureInputEndpoint))};

    contract::ResourceFlowDeclaration output;
    output.stableKey = "unified/texture-uav/output";
    output.boundary = contract::ResourceBoundary::CompositionOutput;
    output.producer = fixture::Ref(
        std::string(ConsumerKey), std::string(fixture::TextureOutputEndpoint));
    input.resources = {std::move(middle), std::move(output)};

    auto built = composition::BuildFrozenCompositionPackage(
        std::move(input), composition::MakeAuthorityOnlyDynamicContractV1(1));
    if (!built)
        return sge4::base::Failure<composition::FrozenCompositionPackage, std::string>(
            built.error().stage + "：" + built.error().message);
    return sge4::base::Success<composition::FrozenCompositionPackage, std::string>(
        std::move(built).value());
}

inline sge4::base::Expected<dynamic::FrozenDynamicInvocationPackage, std::string>
BuildFrozenInvocation(
    const composition::FrozenCompositionPackage& composition,
    canonical::DeviceEpoch deviceEpoch,
    dynamic::InvocationInputV1 input,
    std::optional<dynamic::VerifiedHistoryStateV1> previousHistory = std::nullopt)
{
    auto request = dynamic::BuildDynamicInvocationRequest(
        composition, deviceEpoch, std::move(input), std::move(previousHistory));
    if (!request)
        return sge4::base::Failure<dynamic::FrozenDynamicInvocationPackage, std::string>(
            request.error().stage + "：" + request.error().message);
    auto proposal = dynamic::DynamicInvocationPlannerV1::Plan(request.value());
    if (!proposal.Planned())
        return sge4::base::Failure<dynamic::FrozenDynamicInvocationPackage, std::string>(
            "Planが検証または実行の契約に違反しています。");
    auto verified = dynamic::DynamicInvocationVerifierV1::Verify(
        request.value(), *proposal.proposal);
    if (!verified.Accepted())
        return sge4::base::Failure<dynamic::FrozenDynamicInvocationPackage, std::string>(
            "入力または内部状態が検証または実行の契約に違反しています。");
    auto frozen = dynamic::FreezeVerifiedInvocation(*verified.verified);
    if (!frozen)
        return sge4::base::Failure<dynamic::FrozenDynamicInvocationPackage, std::string>(
            frozen.error().stage + "：" + frozen.error().message);
    return sge4::base::Success<dynamic::FrozenDynamicInvocationPackage, std::string>(
        std::move(frozen).value());
}

}
