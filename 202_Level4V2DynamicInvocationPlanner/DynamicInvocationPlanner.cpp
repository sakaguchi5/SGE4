#include "DynamicInvocationPlanner.h"

#include <algorithm>
#include <iterator>
#include <limits>
#include <stdexcept>

namespace sge4_5::v2::dynamic
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
    if (!result.Accepted()) throw std::runtime_error("Planner failed to construct a canonical exact set.");
    return *result.set;
}

DynamicPlanningResultV1 Failure(DynamicVerificationErrorV1 error)
{
    return {error, std::nullopt};
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

    if (request.mode != InvocationModeV1::ContinueHistory && request.modifiedSurvivorSet.Count() != 0u)
        return Failure(DynamicVerificationErrorV1::SeedModifiedSetNotEmpty);
    if (request.mode == InvocationModeV1::InitialSeed && request.previousHistory.has_value())
        return Failure(DynamicVerificationErrorV1::InitialHistoryPresent);
    if (request.mode == InvocationModeV1::ContinueHistory && !request.previousHistory.has_value())
        return Failure(DynamicVerificationErrorV1::ContinueHistoryMissing);
    if (request.mode == InvocationModeV1::RecoverySeed && request.previousHistory.has_value())
        return Failure(DynamicVerificationErrorV1::RecoveryHistoryPresent);

    std::vector<std::uint32_t> previousIndices;
    std::vector<std::uint64_t> previousGenerations(request.universe.Value(), InvalidItemGenerationV1);
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
            request.timelineOrdinal.Value() == 0u)
            return Failure(DynamicVerificationErrorV1::TimelineNotSuccessor);
        const auto expectedTimeline = history.Descriptor().generation.Value();
        if (request.timelineOrdinal.Value() != expectedTimeline)
            return Failure(DynamicVerificationErrorV1::TimelineNotSuccessor);
        if (history.ItemGenerations().size() != request.universe.Value())
            return Failure(DynamicVerificationErrorV1::HistoryGenerationCountMismatch);
        if (history.Descriptor().generation.Value() == std::numeric_limits<std::uint64_t>::max())
            return Failure(DynamicVerificationErrorV1::GenerationOverflow);

        previousIndices.assign(history.ActiveSet().Indices().begin(), history.ActiveSet().Indices().end());
        previousGenerations.assign(history.ItemGenerations().begin(), history.ItemGenerations().end());
        nextHistoryGeneration = canonical::HistoryGeneration(history.Descriptor().generation.Value() + 1u);
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

    std::vector<std::uint64_t> generations(request.universe.Value(), InvalidItemGenerationV1);
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

    DynamicDecisionV1 decision{
        DynamicDecisionIdentity::FromDigest({}), request.identity, std::move(previousSet), std::move(activationSet),
        std::move(deactivationSet), std::move(updateSet), std::move(retainSet), std::move(transitionSet),
        generationIdentity, std::move(generations), recordIdentity, std::move(records),
        canonical::TransitionCount(static_cast<std::uint32_t>(transition.size())), writeSetIdentity,
        nextHistoryGeneration};
    decision.identity = ComputeDynamicDecisionIdentityV1(decision);
    return {DynamicVerificationErrorV1::None, DynamicPlannerProposalV1{std::move(decision)}};
}
}
