#include "./DynamicInvocationPlanner.h"

#include <algorithm>
#include <iterator>
#include <limits>
#include <stdexcept>

namespace sge4::dynamic
{
namespace
{
std::vector<std::uint32_t> Difference(std::span<const std::uint32_t> left, std::span<const std::uint32_t> right)
{
    std::vector<std::uint32_t> result;
    std::set_difference(left.begin(), left.end(), right.begin(), right.end(), std::back_inserter(result));
    return result;
}

std::vector<std::uint32_t> Intersection(std::span<const std::uint32_t> left, std::span<const std::uint32_t> right)
{
    std::vector<std::uint32_t> result;
    std::set_intersection(left.begin(), left.end(), right.begin(), right.end(), std::back_inserter(result));
    return result;
}

std::vector<std::uint32_t> Union(std::span<const std::uint32_t> left, std::span<const std::uint32_t> right)
{
    std::vector<std::uint32_t> result;
    std::set_union(left.begin(), left.end(), right.begin(), right.end(), std::back_inserter(result));
    return result;
}

ExactIndexSetV1 MakeSet(UniverseCount universe, const std::vector<std::uint32_t>& indices)
{
    auto result = BuildExactIndexSetV1(universe, indices);
    if (!result.Accepted()) throw std::runtime_error("PlanがCanonicalな順序または識別子規則に違反しています。");
    return *result.set;
}

DynamicPlanningResultV1 Failure(DynamicVerificationErrorV1 error)
{
    return {error, std::nullopt};
}

DynamicVerificationErrorV1 ValidateExecutionPayload(
    const DynamicInvocationRequestV1& request,
    std::span<const std::uint32_t> update)
{
    const auto expectedIdentity = ComputeDynamicExecutionPayloadIdentityV1(
        request.executionMode, request.targetLeaf, request.targetDynamicSlot,
        request.memberBytes, request.updatePayloads);
    if (request.executionPayloadIdentity != expectedIdentity)
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

bool IsValidPredicate(composition::ConditionalPredicateKindV1 predicate) noexcept
{
    const auto value = std::to_underlying(predicate);
    return value >= std::to_underlying(composition::ConditionalPredicateKindV1::ActiveSetNonEmpty) &&
        value <= std::to_underlying(composition::ConditionalPredicateKindV1::TransitionSetNonEmpty);
}

bool StrictLeafList(
    std::span<const composition::LeafPackageId> leaves,
    std::uint32_t leafCount) noexcept
{
    std::uint32_t previous = package::InvalidIndex;
    bool hasPrevious = false;
    for (const auto leaf : leaves)
    {
        if (!leaf.IsValid() || leaf.value >= leafCount ||
            (hasPrevious && leaf.value <= previous))
            return false;
        previous = leaf.value;
        hasPrevious = true;
    }
    return true;
}

DynamicVerificationErrorV1 ValidateConditionalContract(
    const DynamicInvocationRequestV1& request)
{
    if (request.compositionLeafCount == 0 ||
        request.conditionalRegions.size() > request.compositionLeafCount)
        return DynamicVerificationErrorV1::ConditionalContractMismatch;
    std::vector<bool> claimed(request.compositionLeafCount, false);
    for (std::size_t index = 0; index < request.conditionalRegions.size(); ++index)
    {
        const auto& region = request.conditionalRegions[index];
        if (!region.id.IsValid() || region.id.value != index ||
            !IsValidPredicate(region.predicate) ||
            (region.trueLeaves.empty() && region.falseLeaves.empty()) ||
            !StrictLeafList(region.trueLeaves, request.compositionLeafCount) ||
            !StrictLeafList(region.falseLeaves, request.compositionLeafCount))
            return DynamicVerificationErrorV1::ConditionalContractMismatch;
        for (const auto leaf : region.falseLeaves)
        {
            if (claimed[leaf.value])
                return DynamicVerificationErrorV1::ConditionalContractMismatch;
            claimed[leaf.value] = true;
        }
        for (const auto leaf : region.trueLeaves)
        {
            if (claimed[leaf.value])
                return DynamicVerificationErrorV1::ConditionalContractMismatch;
            claimed[leaf.value] = true;
        }
    }
    return DynamicVerificationErrorV1::None;
}

bool EvaluatePredicate(
    composition::ConditionalPredicateKindV1 predicate,
    std::span<const std::uint32_t> active,
    std::span<const std::uint32_t> activation,
    std::span<const std::uint32_t> deactivation,
    std::span<const std::uint32_t> update,
    std::span<const std::uint32_t> retain,
    std::span<const std::uint32_t> transition)
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

struct ConditionalExecution final
{
    ConditionalExecutionIdentity identity;
    std::vector<ConditionalRegionSelectionV1> selections;
    std::vector<composition::LeafPackageId> enabledLeaves;
};

ConditionalExecution BuildConditionalExecution(
    const DynamicInvocationRequestV1& request,
    std::span<const std::uint32_t> active,
    std::span<const std::uint32_t> activation,
    std::span<const std::uint32_t> deactivation,
    std::span<const std::uint32_t> update,
    std::span<const std::uint32_t> retain,
    std::span<const std::uint32_t> transition)
{
    std::vector<bool> enabled(request.compositionLeafCount, true);
    std::vector<ConditionalRegionSelectionV1> selections;
    selections.reserve(request.conditionalRegions.size());
    for (const auto& region : request.conditionalRegions)
    {
        for (const auto leaf : region.trueLeaves) enabled[leaf.value] = false;
        for (const auto leaf : region.falseLeaves) enabled[leaf.value] = false;
        const bool value = EvaluatePredicate(
            region.predicate, active, activation, deactivation, update, retain, transition);
        selections.push_back({region.id, value});
        const auto& selected = value ? region.trueLeaves : region.falseLeaves;
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

}

DynamicPlanningResultV1 DynamicInvocationPlannerV1::Plan(const DynamicInvocationRequestV1& request)
{
    if (request.identity != ComputeDynamicInvocationIdentityV1(request))
        return Failure(DynamicVerificationErrorV1::RequestIdentityMismatch);
    if (request.activeSet.Universe() != request.universe)
        return Failure(DynamicVerificationErrorV1::ActiveUniverseMismatch);
    if (request.modifiedSurvivorSet.Universe() != request.universe)
        return Failure(DynamicVerificationErrorV1::ModifiedUniverseMismatch);
    const auto conditionalContractError = ValidateConditionalContract(request);
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
        if (history.Descriptor().state != canonical::HistoryStateV1::Valid)
            return Failure(DynamicVerificationErrorV1::HistoryStateInvalid);
        if (history.CompositionIdentity() != request.compositionIdentity)
            return Failure(DynamicVerificationErrorV1::HistoryCompositionMismatch);
        if (history.Descriptor().semanticIdentity != request.semanticIdentity)
            return Failure(DynamicVerificationErrorV1::HistorySemanticMismatch);
        if (history.Descriptor().deviceEpoch != request.deviceEpoch)
            return Failure(DynamicVerificationErrorV1::HistoryEpochMismatch);
        if (history.Universe() != request.universe)
            return Failure(DynamicVerificationErrorV1::HistoryUniverseMismatch);
        if (history.Descriptor().sourceInvocationIdentity == request.identity ||
            request.timelineOrdinal.value() == 0u)
            return Failure(DynamicVerificationErrorV1::TimelineNotSuccessor);
        const auto expectedTimeline = history.Descriptor().generation.value();
        if (request.timelineOrdinal.value() != expectedTimeline)
            return Failure(DynamicVerificationErrorV1::TimelineNotSuccessor);
        if (history.ItemGenerations().size() != request.universe.value())
            return Failure(DynamicVerificationErrorV1::HistoryGenerationCountMismatch);
        if (history.Descriptor().generation.value() == std::numeric_limits<std::uint64_t>::max())
            return Failure(DynamicVerificationErrorV1::GenerationOverflow);

        previousIndices.assign(history.ActiveSet().Indices().begin(), history.ActiveSet().Indices().end());
        previousGenerations.assign(history.ItemGenerations().begin(), history.ItemGenerations().end());
        nextHistoryGeneration = canonical::HistoryGeneration(history.Descriptor().generation.value() + 1u);
        for (const auto index : previousIndices)
        {
            if (previousGenerations[index] == InvalidItemGenerationV1)
                return Failure(DynamicVerificationErrorV1::PreviousActiveGenerationInvalid);
        }
    }

    const auto activeIndices = request.activeSet.Indices();
    const auto modifiedIndices = request.modifiedSurvivorSet.Indices();
    if (!std::includes(activeIndices.begin(), activeIndices.end(), modifiedIndices.begin(), modifiedIndices.end()))
        return Failure(DynamicVerificationErrorV1::ModifiedNotActive);

    auto previousSet = MakeSet(request.universe, previousIndices);
    const auto survivors = Intersection(previousIndices, activeIndices);
    if (!std::includes(survivors.begin(), survivors.end(), modifiedIndices.begin(), modifiedIndices.end()))
        return Failure(DynamicVerificationErrorV1::ModifiedNotSurvivor);

    std::vector<std::uint32_t> activation;
    std::vector<std::uint32_t> deactivation;
    std::vector<std::uint32_t> update;
    std::vector<std::uint32_t> retain;

    if (request.mode == InvocationModeV1::ContinueHistory)
    {
        activation = Difference(activeIndices, previousIndices);
        deactivation = Difference(previousIndices, activeIndices);
        update = Union(activation, modifiedIndices);
        retain = Difference(survivors, modifiedIndices);
    }
    else
    {
        activation.assign(activeIndices.begin(), activeIndices.end());
        update.assign(activeIndices.begin(), activeIndices.end());
    }
    const auto transition = Union(update, deactivation);

    const auto executionPayloadError = ValidateExecutionPayload(request, update);
    if (executionPayloadError != DynamicVerificationErrorV1::None)
        return Failure(executionPayloadError);

    std::vector<std::uint64_t> generations(request.universe.value(), InvalidItemGenerationV1);
    for (const auto index : activeIndices)
    {
        if (request.mode != InvocationModeV1::ContinueHistory || !std::binary_search(previousIndices.begin(), previousIndices.end(), index))
        {
            generations[index] = 0u;
            continue;
        }
        const auto previous = previousGenerations[index];
        if (std::binary_search(modifiedIndices.begin(), modifiedIndices.end(), index))
        {
            if (previous >= InvalidItemGenerationV1 - 1u)
                return Failure(DynamicVerificationErrorV1::GenerationOverflow);
            generations[index] = previous + 1u;
        }
        else generations[index] = previous;
    }

    std::vector<TransitionRecordV1> records;
    records.reserve(transition.size());
    std::size_t updateCursor = 0u;
    std::size_t clearCursor = 0u;
    while (updateCursor < update.size() || clearCursor < deactivation.size())
    {
        const bool takeUpdate = clearCursor >= deactivation.size() ||
            (updateCursor < update.size() && update[updateCursor] < deactivation[clearCursor]);
        if (takeUpdate)
            records.push_back({MemberIndex(update[updateCursor++]), TransitionActionV1::Update});
        else
            records.push_back({MemberIndex(deactivation[clearCursor++]), TransitionActionV1::Clear});
    }

    auto activationSet = MakeSet(request.universe, activation);
    auto deactivationSet = MakeSet(request.universe, deactivation);
    auto updateSet = MakeSet(request.universe, update);
    auto retainSet = MakeSet(request.universe, retain);
    auto transitionSet = MakeSet(request.universe, transition);
    auto generationIdentity = ComputeGenerationVectorIdentityV1(request.universe, generations);
    auto recordIdentity = ComputeTransitionRecordSetIdentityV1(request.universe, records);
    auto writeSetIdentity = ComputeDynamicWriteSetIdentityV1(transitionSet, recordIdentity);
    auto conditional = BuildConditionalExecution(
        request, activeIndices, activation, deactivation, update, retain, transition);

    DynamicDecisionV1 decision{
        DynamicDecisionIdentity::FromDigest({}), request.identity, std::move(previousSet), std::move(activationSet),
        std::move(deactivationSet), std::move(updateSet), std::move(retainSet), std::move(transitionSet),
        generationIdentity, std::move(generations), recordIdentity, std::move(records),
        canonical::TransitionCount(static_cast<std::uint32_t>(transition.size())), writeSetIdentity,
        std::move(conditional.identity), std::move(conditional.selections),
        std::move(conditional.enabledLeaves), nextHistoryGeneration};
    decision.identity = ComputeDynamicDecisionIdentityV1(decision);
    return {DynamicVerificationErrorV1::None, DynamicPlannerProposalV1{std::move(decision)}};
}
}
