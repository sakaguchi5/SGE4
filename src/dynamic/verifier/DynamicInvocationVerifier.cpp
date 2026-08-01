#include "./DynamicInvocationVerifier.h"

#include <algorithm>
#include <iterator>
#include <limits>
#include <stdexcept>

namespace sge4::dynamic
{
namespace
{
std::vector<std::uint32_t> ExpectedDifference(const std::vector<std::uint32_t>& left, const std::vector<std::uint32_t>& right)
{
    std::vector<std::uint32_t> result;
    std::set_difference(left.begin(), left.end(), right.begin(), right.end(), std::back_inserter(result));
    return result;
}

std::vector<std::uint32_t> ExpectedIntersection(const std::vector<std::uint32_t>& left, const std::vector<std::uint32_t>& right)
{
    std::vector<std::uint32_t> result;
    std::set_intersection(left.begin(), left.end(), right.begin(), right.end(), std::back_inserter(result));
    return result;
}

std::vector<std::uint32_t> ExpectedUnion(const std::vector<std::uint32_t>& left, const std::vector<std::uint32_t>& right)
{
    std::vector<std::uint32_t> result;
    std::set_union(left.begin(), left.end(), right.begin(), right.end(), std::back_inserter(result));
    return result;
}

ExactIndexSetV1 ExpectedSet(UniverseCount universe, const std::vector<std::uint32_t>& indices)
{
    auto result = BuildExactIndexSetV1(universe, indices);
    if (!result.Accepted()) throw std::runtime_error("入力または内部状態がCanonicalな順序または識別子規則に違反しています。");
    return *result.set;
}

DynamicVerificationResultV1 Failure(DynamicVerificationErrorV1 error)
{
    return {error, std::nullopt};
}

bool SameRecords(std::span<const TransitionRecordV1> left, std::span<const TransitionRecordV1> right)
{
    return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin(), right.end());
}

DynamicVerificationErrorV1 VerifyExecutionPayload(
    const DynamicInvocationRequestV1& request,
    std::span<const std::uint32_t> update)
{
    if (request.executionPayloadIdentity != ComputeDynamicExecutionPayloadIdentityV1(
        request.executionMode, request.targetLeaf, request.targetDynamicSlot,
        request.memberBytes, request.updatePayloads))
        return DynamicVerificationErrorV1::ExecutionPayloadIdentityMismatch;

    if (request.executionMode == composition::DynamicExecutionModeV1::AuthorityOnly)
    {
        if (request.targetLeaf.IsValid() ||
            request.targetDynamicSlot != package::InvalidIndex ||
            request.memberBytes != 0 || !request.updatePayloads.empty())
            return DynamicVerificationErrorV1::ExecutionContractMismatch;
        return DynamicVerificationErrorV1::None;
    }

    if (request.executionMode != composition::DynamicExecutionModeV1::VerifiedDenseSlot ||
        !request.targetLeaf.IsValid() ||
        request.targetDynamicSlot == package::InvalidIndex || request.memberBytes == 0)
        return DynamicVerificationErrorV1::ExecutionContractMismatch;
    if (request.updatePayloads.size() != update.size())
        return DynamicVerificationErrorV1::ExecutionPayloadSetMismatch;
    for (std::size_t index = 0; index < request.updatePayloads.size(); ++index)
    {
        const auto& payload = request.updatePayloads[index];
        if (payload.member.value() >= request.universe.value())
            return DynamicVerificationErrorV1::ExecutionPayloadMemberOutOfRange;
        if (index > 0 && request.updatePayloads[index - 1].member.value() >= payload.member.value())
            return DynamicVerificationErrorV1::ExecutionPayloadDuplicateMember;
        if (payload.bytes.size() != request.memberBytes)
            return DynamicVerificationErrorV1::ExecutionPayloadSizeMismatch;
        if (payload.member.value() != update[index])
            return DynamicVerificationErrorV1::ExecutionPayloadSetMismatch;
    }
    return DynamicVerificationErrorV1::None;
}

bool IsSupportedPredicate(composition::ConditionalPredicateKindV1 predicate) noexcept
{
    switch (predicate)
    {
    case composition::ConditionalPredicateKindV1::ActiveSetNonEmpty:
    case composition::ConditionalPredicateKindV1::ActivationSetNonEmpty:
    case composition::ConditionalPredicateKindV1::DeactivationSetNonEmpty:
    case composition::ConditionalPredicateKindV1::UpdateSetNonEmpty:
    case composition::ConditionalPredicateKindV1::RetainSetNonEmpty:
    case composition::ConditionalPredicateKindV1::TransitionSetNonEmpty:
        return true;
    }
    return false;
}

bool CanonicalLeafSequence(
    std::span<const composition::LeafPackageId> leaves,
    std::uint32_t leafCount) noexcept
{
    for (std::size_t index = 0; index < leaves.size(); ++index)
    {
        if (!leaves[index].IsValid() || leaves[index].value >= leafCount ||
            (index > 0 && leaves[index - 1].value >= leaves[index].value))
            return false;
    }
    return true;
}

DynamicVerificationErrorV1 VerifyConditionalContractShape(
    const DynamicInvocationRequestV1& request)
{
    if (request.compositionLeafCount == 0 ||
        request.conditionalRegions.size() > request.compositionLeafCount)
        return DynamicVerificationErrorV1::ConditionalContractMismatch;
    std::vector<std::uint8_t> ownership(request.compositionLeafCount, 0u);
    for (std::size_t index = 0; index < request.conditionalRegions.size(); ++index)
    {
        const auto& region = request.conditionalRegions[index];
        if (!region.id.IsValid() || region.id.value != index ||
            !IsSupportedPredicate(region.predicate) ||
            (region.trueLeaves.empty() && region.falseLeaves.empty()) ||
            !CanonicalLeafSequence(region.trueLeaves, request.compositionLeafCount) ||
            !CanonicalLeafSequence(region.falseLeaves, request.compositionLeafCount))
            return DynamicVerificationErrorV1::ConditionalContractMismatch;
        for (const auto leaf : region.trueLeaves)
        {
            if (ownership[leaf.value] != 0u)
                return DynamicVerificationErrorV1::ConditionalContractMismatch;
            ownership[leaf.value] = 1u;
        }
        for (const auto leaf : region.falseLeaves)
        {
            if (ownership[leaf.value] != 0u)
                return DynamicVerificationErrorV1::ConditionalContractMismatch;
            ownership[leaf.value] = 2u;
        }
    }
    return DynamicVerificationErrorV1::None;
}

bool ExpectedPredicateValue(
    composition::ConditionalPredicateKindV1 predicate,
    const std::vector<std::uint32_t>& active,
    const std::vector<std::uint32_t>& activation,
    const std::vector<std::uint32_t>& deactivation,
    const std::vector<std::uint32_t>& update,
    const std::vector<std::uint32_t>& retain,
    const std::vector<std::uint32_t>& transition)
{
    switch (predicate)
    {
    case composition::ConditionalPredicateKindV1::ActiveSetNonEmpty:
        return !active.empty();
    case composition::ConditionalPredicateKindV1::ActivationSetNonEmpty:
        return !activation.empty();
    case composition::ConditionalPredicateKindV1::DeactivationSetNonEmpty:
        return !deactivation.empty();
    case composition::ConditionalPredicateKindV1::UpdateSetNonEmpty:
        return !update.empty();
    case composition::ConditionalPredicateKindV1::RetainSetNonEmpty:
        return !retain.empty();
    case composition::ConditionalPredicateKindV1::TransitionSetNonEmpty:
        return !transition.empty();
    }
    throw std::runtime_error("Conditional predicateが検証または実行の契約に違反しています。");
}

struct ExpectedConditionalExecution final
{
    ConditionalExecutionIdentity identity;
    std::vector<ConditionalRegionSelectionV1> selections;
    std::vector<composition::LeafPackageId> enabledLeaves;
};

ExpectedConditionalExecution DeriveConditionalExecution(
    const DynamicInvocationRequestV1& request,
    const std::vector<std::uint32_t>& active,
    const std::vector<std::uint32_t>& activation,
    const std::vector<std::uint32_t>& deactivation,
    const std::vector<std::uint32_t>& update,
    const std::vector<std::uint32_t>& retain,
    const std::vector<std::uint32_t>& transition)
{
    std::vector<bool> enabled(request.compositionLeafCount, true);
    std::vector<ConditionalRegionSelectionV1> selections;
    selections.reserve(request.conditionalRegions.size());
    for (const auto& region : request.conditionalRegions)
    {
        for (const auto leaf : region.trueLeaves) enabled[leaf.value] = false;
        for (const auto leaf : region.falseLeaves) enabled[leaf.value] = false;
        const bool predicateValue = ExpectedPredicateValue(
            region.predicate, active, activation, deactivation, update, retain, transition);
        selections.push_back({region.id, predicateValue});
        const auto& selected = predicateValue ? region.trueLeaves : region.falseLeaves;
        for (const auto leaf : selected) enabled[leaf.value] = true;
    }
    std::vector<composition::LeafPackageId> leaves;
    for (std::uint32_t leaf = 0; leaf < request.compositionLeafCount; ++leaf)
    {
        if (enabled[leaf]) leaves.push_back(composition::LeafPackageId{leaf});
    }
    auto identity = ComputeConditionalExecutionIdentityV1(
        request.compositionLeafCount, selections, leaves);
    return {std::move(identity), std::move(selections), std::move(leaves)};
}

bool SameSelections(
    std::span<const ConditionalRegionSelectionV1> left,
    std::span<const ConditionalRegionSelectionV1> right)
{
    return left.size() == right.size() &&
        std::equal(left.begin(), left.end(), right.begin(), right.end());
}

bool SameLeaves(
    std::span<const composition::LeafPackageId> left,
    std::span<const composition::LeafPackageId> right)
{
    return left.size() == right.size() &&
        std::equal(left.begin(), left.end(), right.begin(), right.end(),
            [](const auto a, const auto b) { return a.value == b.value; });
}

}

DynamicVerificationResultV1 DynamicInvocationVerifierV1::Verify(
    const DynamicInvocationRequestV1& request,
    const DynamicPlannerProposalV1& proposal)
{
    if (request.identity != ComputeDynamicInvocationIdentityV1(request))
        return Failure(DynamicVerificationErrorV1::RequestIdentityMismatch);
    if (request.activeSet.Identity() != ComputeIndexSetIdentityV1(request.activeSet) || request.activeSet.Universe() != request.universe)
        return Failure(DynamicVerificationErrorV1::ActiveUniverseMismatch);
    if (request.modifiedSurvivorSet.Identity() != ComputeIndexSetIdentityV1(request.modifiedSurvivorSet) || request.modifiedSurvivorSet.Universe() != request.universe)
        return Failure(DynamicVerificationErrorV1::ModifiedUniverseMismatch);
    const auto conditionalContractError = VerifyConditionalContractShape(request);
    if (conditionalContractError != DynamicVerificationErrorV1::None)
        return Failure(conditionalContractError);

    if (request.mode != InvocationModeV1::ContinueHistory && request.modifiedSurvivorSet.Count() != 0u)
        return Failure(DynamicVerificationErrorV1::SeedModifiedSetNotEmpty);
    if (request.mode == InvocationModeV1::InitialSeed && request.previousHistory.has_value())
        return Failure(DynamicVerificationErrorV1::InitialHistoryPresent);
    if (request.mode == InvocationModeV1::ContinueHistory && !request.previousHistory.has_value())
        return Failure(DynamicVerificationErrorV1::ContinueHistoryMissing);
    if (request.mode == InvocationModeV1::RecoverySeed && request.previousHistory.has_value())
        return Failure(DynamicVerificationErrorV1::RecoveryHistoryPresent);

    std::vector<std::uint32_t> previousIndices;
    std::vector<std::uint64_t> previousGenerations(request.universe.value(), InvalidItemGenerationV1);
    canonical::HistoryGeneration nextHistoryGeneration(1u);

    if (request.previousHistory.has_value())
    {
        const auto& history = *request.previousHistory;
        const auto& descriptor = history.Descriptor();
        if (descriptor.state != canonical::HistoryStateV1::Valid)
            return Failure(DynamicVerificationErrorV1::HistoryStateInvalid);
        if (history.CompositionIdentity() != request.compositionIdentity)
            return Failure(DynamicVerificationErrorV1::HistoryCompositionMismatch);
        if (descriptor.semanticIdentity != request.semanticIdentity)
            return Failure(DynamicVerificationErrorV1::HistorySemanticMismatch);
        if (descriptor.deviceEpoch != request.deviceEpoch)
            return Failure(DynamicVerificationErrorV1::HistoryEpochMismatch);
        if (history.Universe() != request.universe)
            return Failure(DynamicVerificationErrorV1::HistoryUniverseMismatch);
        if (request.timelineOrdinal.value() != descriptor.generation.value())
            return Failure(DynamicVerificationErrorV1::TimelineNotSuccessor);
        if (history.ItemGenerations().size() != request.universe.value())
            return Failure(DynamicVerificationErrorV1::HistoryGenerationCountMismatch);
        if (descriptor.generation.value() == std::numeric_limits<std::uint64_t>::max())
            return Failure(DynamicVerificationErrorV1::GenerationOverflow);
        const auto recomputedGenerationIdentity = ComputeGenerationVectorIdentityV1(history.Universe(), history.ItemGenerations());
        const auto recomputedHistoryIdentity = ComputeHistoryValidityIdentityV1(
            history.CompositionIdentity(), descriptor.semanticIdentity, descriptor.sourceInvocationIdentity,
            descriptor.generation, descriptor.deviceEpoch, history.ActiveSet(), recomputedGenerationIdentity);
        if (recomputedGenerationIdentity != history.GenerationIdentity() || recomputedHistoryIdentity != descriptor.identity)
            return Failure(DynamicVerificationErrorV1::HistoryStateInvalid);

        previousIndices.assign(history.ActiveSet().Indices().begin(), history.ActiveSet().Indices().end());
        previousGenerations.assign(history.ItemGenerations().begin(), history.ItemGenerations().end());
        nextHistoryGeneration = canonical::HistoryGeneration(descriptor.generation.value() + 1u);
        for (const auto index : previousIndices)
        {
            if (previousGenerations[index] == InvalidItemGenerationV1)
                return Failure(DynamicVerificationErrorV1::PreviousActiveGenerationInvalid);
        }
    }

    std::vector<std::uint32_t> active(request.activeSet.Indices().begin(), request.activeSet.Indices().end());
    std::vector<std::uint32_t> modified(request.modifiedSurvivorSet.Indices().begin(), request.modifiedSurvivorSet.Indices().end());
    if (!std::includes(active.begin(), active.end(), modified.begin(), modified.end()))
        return Failure(DynamicVerificationErrorV1::ModifiedNotActive);

    const auto survivors = ExpectedIntersection(previousIndices, active);
    if (!std::includes(survivors.begin(), survivors.end(), modified.begin(), modified.end()))
        return Failure(DynamicVerificationErrorV1::ModifiedNotSurvivor);

    std::vector<std::uint32_t> activation;
    std::vector<std::uint32_t> deactivation;
    std::vector<std::uint32_t> update;
    std::vector<std::uint32_t> retain;
    if (request.mode == InvocationModeV1::ContinueHistory)
    {
        activation = ExpectedDifference(active, previousIndices);
        deactivation = ExpectedDifference(previousIndices, active);
        update = ExpectedUnion(activation, modified);
        retain = ExpectedDifference(survivors, modified);
    }
    else
    {
        activation = active;
        update = active;
    }
    const auto transition = ExpectedUnion(update, deactivation);

    const auto executionPayloadError = VerifyExecutionPayload(request, update);
    if (executionPayloadError != DynamicVerificationErrorV1::None)
        return Failure(executionPayloadError);

    std::vector<std::uint64_t> generations(request.universe.value(), InvalidItemGenerationV1);
    for (const auto index : active)
    {
        const bool previouslyActive = std::binary_search(previousIndices.begin(), previousIndices.end(), index);
        if (request.mode != InvocationModeV1::ContinueHistory || !previouslyActive)
        {
            generations[index] = 0u;
            continue;
        }
        const auto previous = previousGenerations[index];
        if (std::binary_search(modified.begin(), modified.end(), index))
        {
            if (previous >= InvalidItemGenerationV1 - 1u)
                return Failure(DynamicVerificationErrorV1::GenerationOverflow);
            generations[index] = previous + 1u;
        }
        else generations[index] = previous;
    }

    std::vector<TransitionRecordV1> records;
    records.reserve(transition.size());
    for (const auto member : transition)
    {
        const auto action = std::binary_search(update.begin(), update.end(), member)
            ? TransitionActionV1::Update : TransitionActionV1::Clear;
        records.push_back({MemberIndex(member), action});
    }

    auto expectedPrevious = ExpectedSet(request.universe, previousIndices);
    auto expectedActivation = ExpectedSet(request.universe, activation);
    auto expectedDeactivation = ExpectedSet(request.universe, deactivation);
    auto expectedUpdate = ExpectedSet(request.universe, update);
    auto expectedRetain = ExpectedSet(request.universe, retain);
    auto expectedTransition = ExpectedSet(request.universe, transition);
    auto expectedGenerationIdentity = ComputeGenerationVectorIdentityV1(request.universe, generations);
    auto expectedRecordIdentity = ComputeTransitionRecordSetIdentityV1(request.universe, records);
    auto expectedWriteSetIdentity = ComputeDynamicWriteSetIdentityV1(expectedTransition, expectedRecordIdentity);
    auto expectedConditional = DeriveConditionalExecution(
        request, active, activation, deactivation, update, retain, transition);

    const auto& actual = proposal.decision;
    if (actual.requestIdentity != request.identity)
        return Failure(DynamicVerificationErrorV1::ProposalRequestIdentityMismatch);
    if (actual.previousActiveSet.Identity() != expectedPrevious.Identity())
        return Failure(DynamicVerificationErrorV1::PreviousActiveSetMismatch);
    if (actual.activationSet.Identity() != expectedActivation.Identity())
        return Failure(DynamicVerificationErrorV1::ActivationSetMismatch);
    if (actual.deactivationSet.Identity() != expectedDeactivation.Identity())
        return Failure(DynamicVerificationErrorV1::DeactivationSetMismatch);
    if (actual.updateSet.Identity() != expectedUpdate.Identity())
        return Failure(DynamicVerificationErrorV1::UpdateSetMismatch);
    if (actual.retainSet.Identity() != expectedRetain.Identity())
        return Failure(DynamicVerificationErrorV1::RetainSetMismatch);
    if (actual.transitionSet.Identity() != expectedTransition.Identity())
        return Failure(DynamicVerificationErrorV1::TransitionSetMismatch);
    if (actual.generationVectorIdentity != expectedGenerationIdentity || actual.itemGenerations != generations ||
        actual.nextHistoryGeneration != nextHistoryGeneration)
        return Failure(DynamicVerificationErrorV1::GenerationVectorMismatch);
    if (actual.transitionRecordSetIdentity != expectedRecordIdentity || !SameRecords(actual.transitionRecords, records))
        return Failure(DynamicVerificationErrorV1::TransitionRecordMismatch);
    if (actual.indirectWorkCount.value() != transition.size())
        return Failure(DynamicVerificationErrorV1::IndirectWorkCountMismatch);
    if (actual.dynamicWriteSetIdentity != expectedWriteSetIdentity)
        return Failure(DynamicVerificationErrorV1::DynamicWriteSetMismatch);
    if (!SameSelections(actual.conditionalSelections, expectedConditional.selections))
        return Failure(DynamicVerificationErrorV1::ConditionalSelectionMismatch);
    if (!SameLeaves(actual.enabledLeaves, expectedConditional.enabledLeaves))
        return Failure(DynamicVerificationErrorV1::EnabledLeafSetMismatch);
    if (actual.conditionalExecutionIdentity != expectedConditional.identity)
        return Failure(DynamicVerificationErrorV1::ConditionalExecutionIdentityMismatch);

    DynamicDecisionV1 expectedDecision{
        DynamicDecisionIdentity::FromDigest({}), request.identity, std::move(expectedPrevious), std::move(expectedActivation),
        std::move(expectedDeactivation), std::move(expectedUpdate), std::move(expectedRetain), std::move(expectedTransition),
        expectedGenerationIdentity, std::move(generations), expectedRecordIdentity, std::move(records),
        canonical::TransitionCount(static_cast<std::uint32_t>(transition.size())), expectedWriteSetIdentity,
        std::move(expectedConditional.identity), std::move(expectedConditional.selections),
        std::move(expectedConditional.enabledLeaves), nextHistoryGeneration};
    expectedDecision.identity = ComputeDynamicDecisionIdentityV1(expectedDecision);
    if (actual.identity != expectedDecision.identity)
        return Failure(DynamicVerificationErrorV1::DecisionIdentityMismatch);

    auto seal = ComputeDynamicSealIdentityV1(request, expectedDecision);
    return {DynamicVerificationErrorV1::None,
        VerifiedDynamicInvocationV1(request, std::move(expectedDecision), std::move(seal))};
}
}
