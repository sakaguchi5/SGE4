#include "DynamicInvocationVerifier.h"

#include <algorithm>
#include <iterator>
#include <limits>
#include <stdexcept>

namespace sge4_5::v2::dynamic
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
    if (!result.Accepted()) throw std::runtime_error("Verifier failed to reconstruct a canonical exact set.");
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
        if (request.timelineOrdinal.Value() != descriptor.generation.Value())
            return Failure(DynamicVerificationErrorV1::TimelineNotSuccessor);
        if (history.ItemGenerations().size() != request.universe.Value())
            return Failure(DynamicVerificationErrorV1::HistoryGenerationCountMismatch);
        if (descriptor.generation.Value() == std::numeric_limits<std::uint64_t>::max())
            return Failure(DynamicVerificationErrorV1::GenerationOverflow);
        const auto recomputedGenerationIdentity = ComputeGenerationVectorIdentityV1(history.Universe(), history.ItemGenerations());
        const auto recomputedHistoryIdentity = ComputeHistoryValidityIdentityV1(
            history.CompositionIdentity(), descriptor.semanticIdentity, descriptor.sourceInvocationIdentity,
            descriptor.generation, descriptor.deviceEpoch, history.ActiveSet(), recomputedGenerationIdentity);
        if (recomputedGenerationIdentity != history.GenerationIdentity() || recomputedHistoryIdentity != descriptor.identity)
            return Failure(DynamicVerificationErrorV1::HistoryStateInvalid);

        previousIndices.assign(history.ActiveSet().Indices().begin(), history.ActiveSet().Indices().end());
        previousGenerations.assign(history.ItemGenerations().begin(), history.ItemGenerations().end());
        nextHistoryGeneration = canonical::HistoryGeneration(descriptor.generation.Value() + 1u);
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

    std::vector<std::uint64_t> generations(request.universe.Value(), InvalidItemGenerationV1);
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
    if (actual.indirectWorkCount.Value() != transition.size())
        return Failure(DynamicVerificationErrorV1::IndirectWorkCountMismatch);
    if (actual.dynamicWriteSetIdentity != expectedWriteSetIdentity)
        return Failure(DynamicVerificationErrorV1::DynamicWriteSetMismatch);

    DynamicDecisionV1 expectedDecision{
        DynamicDecisionIdentity::FromDigest({}), request.identity, std::move(expectedPrevious), std::move(expectedActivation),
        std::move(expectedDeactivation), std::move(expectedUpdate), std::move(expectedRetain), std::move(expectedTransition),
        expectedGenerationIdentity, std::move(generations), expectedRecordIdentity, std::move(records),
        canonical::TransitionCount(static_cast<std::uint32_t>(transition.size())), expectedWriteSetIdentity,
        nextHistoryGeneration};
    expectedDecision.identity = ComputeDynamicDecisionIdentityV1(expectedDecision);
    if (actual.identity != expectedDecision.identity)
        return Failure(DynamicVerificationErrorV1::DecisionIdentityMismatch);

    auto seal = ComputeDynamicSealIdentityV1(request, expectedDecision);
    return {DynamicVerificationErrorV1::None,
        VerifiedDynamicInvocationV1(request, std::move(expectedDecision), std::move(seal))};
}
}
