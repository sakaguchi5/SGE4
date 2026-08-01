#include "./DynamicInvocationModel.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace sge4::dynamic
{
namespace
{
void AppendU8(std::vector<std::byte>& bytes, std::uint8_t value)
{
    bytes.push_back(static_cast<std::byte>(value));
}

void AppendU32(std::vector<std::byte>& bytes, std::uint32_t value)
{
    for (std::uint32_t index = 0; index < sizeof(value); ++index)
        bytes.push_back(static_cast<std::byte>((value >> (index * 8u)) & 0xffu));
}

void AppendU64(std::vector<std::byte>& bytes, std::uint64_t value)
{
    for (std::uint32_t index = 0; index < sizeof(value); ++index)
        bytes.push_back(static_cast<std::byte>((value >> (index * 8u)) & 0xffu));
}

void AppendDigest(std::vector<std::byte>& bytes, const canonical::Digest256V1& digest)
{
    bytes.insert(bytes.end(), digest.begin(), digest.end());
}

template<class Identity>
Identity MakeIdentity(std::string_view domain, std::span<const std::byte> payload)
{
    return canonical::MakeCanonicalIdentityV1<Identity>({domain, DynamicInvocationSchemaVersionV1, payload});
}
}

bool ExactIndexSetV1::Contains(std::uint32_t index) const noexcept
{
    return std::binary_search(indices_.begin(), indices_.end(), index);
}

ExactIndexSetBuildResultV1 BuildExactIndexSetV1(
    UniverseCount universe,
    std::span<const std::uint32_t> indices)
{
    if (universe.value() == 0u)
        return {ExactIndexSetErrorV1::ZeroUniverse, std::nullopt};

    std::vector<std::uint32_t> canonicalIndices(indices.begin(), indices.end());
    for (const auto index : canonicalIndices)
    {
        if (index >= universe.value())
            return {ExactIndexSetErrorV1::IndexOutOfRange, std::nullopt};
    }
    std::sort(canonicalIndices.begin(), canonicalIndices.end());
    if (std::adjacent_find(canonicalIndices.begin(), canonicalIndices.end()) != canonicalIndices.end())
        return {ExactIndexSetErrorV1::DuplicateIndex, std::nullopt};

    std::vector<std::byte> payload;
    AppendU32(payload, universe.value());
    AppendU64(payload, static_cast<std::uint64_t>(canonicalIndices.size()));
    for (const auto index : canonicalIndices) AppendU32(payload, index);
    auto identity = MakeIdentity<IndexSetIdentity>("SGE.V2.Dynamic.ExactIndexSet.V1", payload);
    return {ExactIndexSetErrorV1::None, ExactIndexSetV1(universe, std::move(identity), std::move(canonicalIndices))};
}

IndexSetIdentity ComputeIndexSetIdentityV1(const ExactIndexSetV1& set)
{
    std::vector<std::byte> payload;
    AppendU32(payload, set.universe_.value());
    AppendU64(payload, static_cast<std::uint64_t>(set.indices_.size()));
    for (const auto index : set.indices_) AppendU32(payload, index);
    return MakeIdentity<IndexSetIdentity>("SGE.V2.Dynamic.ExactIndexSet.V1", payload);
}

DynamicInvocationRequestV1 MakeDynamicInvocationRequestV1(
    composition::FrozenCompositionIdentity compositionIdentity,
    canonical::SemanticIdentity semanticIdentity,
    canonical::TimelineOrdinal timelineOrdinal,
    canonical::DeviceEpoch deviceEpoch,
    UniverseCount universe,
    InvocationModeV1 mode,
    ExactIndexSetV1 activeSet,
    ExactIndexSetV1 modifiedSurvivorSet,
    composition::DynamicExecutionModeV1 executionMode,
    std::uint32_t canonicalMemberBytes,
    std::vector<composition::DynamicExecutionRouteV1> executionRoutes,
    std::uint32_t compositionLeafCount,
    std::vector<composition::ConditionalRegionV1> conditionalRegions,
    composition::VerifiedIndirectDispatchContractV1 indirectDispatchContract,
    std::vector<MemberUpdatePayloadV1> updatePayloads,
    std::optional<VerifiedHistoryStateV1> previousHistory)
{
    if (activeSet.Universe() != universe || modifiedSurvivorSet.Universe() != universe)
        throw std::invalid_argument("Invocationが検証または実行の契約に違反しています。");

    DynamicInvocationRequestV1 request{
        canonical::InvocationIdentity::FromDigest({}),
        std::move(compositionIdentity),
        std::move(semanticIdentity),
        timelineOrdinal,
        deviceEpoch,
        universe,
        mode,
        std::move(activeSet),
        std::move(modifiedSurvivorSet),
        executionMode,
        canonicalMemberBytes,
        std::move(executionRoutes),
        compositionLeafCount,
        std::move(conditionalRegions),
        indirectDispatchContract,
        DynamicExecutionPayloadIdentity::FromDigest({}),
        std::move(updatePayloads),
        std::move(previousHistory)};
    request.executionPayloadIdentity = ComputeDynamicExecutionPayloadIdentityV1(
        request.executionMode, request.canonicalMemberBytes,
        request.executionRoutes, request.updatePayloads);
    request.identity = ComputeDynamicInvocationIdentityV1(request);
    return request;
}

canonical::InvocationIdentity ComputeDynamicInvocationIdentityV1(
    const DynamicInvocationRequestV1& request)
{
    std::vector<std::byte> payload;
    AppendDigest(payload, request.compositionIdentity.Digest());
    AppendDigest(payload, request.semanticIdentity.Digest());
    AppendU64(payload, request.timelineOrdinal.value());
    AppendU64(payload, request.deviceEpoch.value());
    AppendU32(payload, request.universe.value());
    AppendU8(payload, static_cast<std::uint8_t>(request.mode));
    AppendDigest(payload, request.activeSet.Identity().Digest());
    AppendDigest(payload, request.modifiedSurvivorSet.Identity().Digest());
    AppendU32(payload, std::to_underlying(request.executionMode));
    AppendU32(payload, request.canonicalMemberBytes);
    AppendU64(payload, static_cast<std::uint64_t>(request.executionRoutes.size()));
    for (const auto& route : request.executionRoutes)
    {
        AppendU32(payload, route.targetLeaf.value);
        AppendU32(payload, route.targetDynamicSlot);
        AppendU32(payload, route.sourceByteOffset);
        AppendU32(payload, route.routeMemberBytes);
    }
    AppendU32(payload, request.compositionLeafCount);
    AppendU64(payload, static_cast<std::uint64_t>(request.conditionalRegions.size()));
    for (const auto& region : request.conditionalRegions)
    {
        AppendU32(payload, region.id.value);
        AppendU32(payload, std::to_underlying(region.predicate));
        AppendU64(payload, static_cast<std::uint64_t>(region.trueLeaves.size()));
        for (const auto leaf : region.trueLeaves) AppendU32(payload, leaf.value);
        AppendU64(payload, static_cast<std::uint64_t>(region.falseLeaves.size()));
        for (const auto leaf : region.falseLeaves) AppendU32(payload, leaf.value);
    }
    AppendU32(payload, std::to_underlying(request.indirectDispatchContract.mode));
    AppendU32(payload, request.indirectDispatchContract.targetLeaf.value);
    AppendU32(payload, request.indirectDispatchContract.targetComputeCommand);
    AppendU32(payload, request.indirectDispatchContract.maxWorkCount);
    AppendDigest(payload, request.executionPayloadIdentity.Digest());
    AppendU8(payload, request.previousHistory.has_value() ? 1u : 0u);
    if (request.previousHistory.has_value())
        AppendDigest(payload, request.previousHistory->Descriptor().identity.Digest());
    return MakeIdentity<canonical::InvocationIdentity>("SGE.V2.Dynamic.InvocationRequest.V2", payload);
}

GenerationVectorIdentity ComputeGenerationVectorIdentityV1(
    UniverseCount universe,
    std::span<const std::uint64_t> generations)
{
    std::vector<std::byte> payload;
    AppendU32(payload, universe.value());
    AppendU64(payload, static_cast<std::uint64_t>(generations.size()));
    for (const auto generation : generations) AppendU64(payload, generation);
    return MakeIdentity<GenerationVectorIdentity>("SGE.V2.Dynamic.GenerationVector.V1", payload);
}

TransitionRecordSetIdentity ComputeTransitionRecordSetIdentityV1(
    UniverseCount universe,
    std::span<const TransitionRecordV1> records)
{
    std::vector<std::byte> payload;
    AppendU32(payload, universe.value());
    AppendU64(payload, static_cast<std::uint64_t>(records.size()));
    for (const auto& record : records)
    {
        AppendU32(payload, record.member.value());
        AppendU8(payload, static_cast<std::uint8_t>(record.action));
    }
    return MakeIdentity<TransitionRecordSetIdentity>("SGE.V2.Dynamic.TransitionRecords.V1", payload);
}

DynamicWriteSetIdentity ComputeDynamicWriteSetIdentityV1(
    const ExactIndexSetV1& transitionSet,
    TransitionRecordSetIdentity recordSetIdentity)
{
    std::vector<std::byte> payload;
    AppendDigest(payload, transitionSet.Identity().Digest());
    AppendDigest(payload, recordSetIdentity.Digest());
    AppendU32(payload, transitionSet.Count());
    return MakeIdentity<DynamicWriteSetIdentity>("SGE.V2.Dynamic.ExactWriteSet.V1", payload);
}


DynamicExecutionPayloadIdentity ComputeDynamicExecutionPayloadIdentityV1(
    composition::DynamicExecutionModeV1 executionMode,
    std::uint32_t canonicalMemberBytes,
    std::span<const composition::DynamicExecutionRouteV1> executionRoutes,
    std::span<const MemberUpdatePayloadV1> updatePayloads)
{
    std::vector<std::byte> payload;
    AppendU32(payload, std::to_underlying(executionMode));
    AppendU32(payload, canonicalMemberBytes);
    AppendU64(payload, static_cast<std::uint64_t>(executionRoutes.size()));
    for (const auto& route : executionRoutes)
    {
        AppendU32(payload, route.targetLeaf.value);
        AppendU32(payload, route.targetDynamicSlot);
        AppendU32(payload, route.sourceByteOffset);
        AppendU32(payload, route.routeMemberBytes);
    }
    AppendU64(payload, static_cast<std::uint64_t>(updatePayloads.size()));
    for (const auto& update : updatePayloads)
    {
        AppendU32(payload, update.member.value());
        AppendU64(payload, static_cast<std::uint64_t>(update.bytes.size()));
        payload.insert(payload.end(), update.bytes.begin(), update.bytes.end());
    }
    return MakeIdentity<DynamicExecutionPayloadIdentity>(
        "SGE.V2.Dynamic.ExecutionPayload.V2", payload);
}

IndirectDispatchIdentity ComputeIndirectDispatchIdentityV1(
    const VerifiedIndirectDispatchV1& dispatch)
{
    std::vector<std::byte> payload;
    AppendU32(payload, std::to_underlying(dispatch.mode));
    AppendU32(payload, dispatch.targetLeaf.value);
    AppendU32(payload, dispatch.targetComputeCommand);
    AppendU32(payload, dispatch.maxWorkCount);
    AppendU32(payload, dispatch.workCount);
    AppendU32(payload, dispatch.threadGroupCountX);
    AppendU32(payload, dispatch.threadGroupCountY);
    AppendU32(payload, dispatch.threadGroupCountZ);
    return MakeIdentity<IndirectDispatchIdentity>(
        "SGE.V2.Dynamic.IndirectDispatch.V1", payload);
}

ConditionalExecutionIdentity ComputeConditionalExecutionIdentityV1(
    std::uint32_t compositionLeafCount,
    std::span<const ConditionalRegionSelectionV1> selections,
    std::span<const composition::LeafPackageId> enabledLeaves)
{
    std::vector<std::byte> payload;
    AppendU32(payload, compositionLeafCount);
    AppendU64(payload, static_cast<std::uint64_t>(selections.size()));
    for (const auto& selection : selections)
    {
        AppendU32(payload, selection.region.value);
        AppendU8(payload, selection.predicateValue ? 1u : 0u);
    }
    AppendU64(payload, static_cast<std::uint64_t>(enabledLeaves.size()));
    for (const auto leaf : enabledLeaves) AppendU32(payload, leaf.value);
    return MakeIdentity<ConditionalExecutionIdentity>(
        "SGE.V2.Dynamic.ConditionalExecution.V1", payload);
}

DynamicDecisionIdentity ComputeDynamicDecisionIdentityV1(const DynamicDecisionV1& decision)
{
    std::vector<std::byte> payload;
    AppendDigest(payload, decision.requestIdentity.Digest());
    AppendDigest(payload, decision.previousActiveSet.Identity().Digest());
    AppendDigest(payload, decision.activationSet.Identity().Digest());
    AppendDigest(payload, decision.deactivationSet.Identity().Digest());
    AppendDigest(payload, decision.updateSet.Identity().Digest());
    AppendDigest(payload, decision.retainSet.Identity().Digest());
    AppendDigest(payload, decision.transitionSet.Identity().Digest());
    AppendDigest(payload, decision.generationVectorIdentity.Digest());
    AppendDigest(payload, decision.transitionRecordSetIdentity.Digest());
    AppendU32(payload, decision.indirectWorkCount.value());
    AppendDigest(payload, decision.dynamicWriteSetIdentity.Digest());
    AppendDigest(payload, decision.indirectDispatch.identity.Digest());
    AppendDigest(payload, decision.conditionalExecutionIdentity.Digest());
    AppendU64(payload, decision.nextHistoryGeneration.value());
    return MakeIdentity<DynamicDecisionIdentity>("SGE.V2.Dynamic.Decision.V1", payload);
}

DynamicSealIdentity ComputeDynamicSealIdentityV1(
    const DynamicInvocationRequestV1& request,
    const DynamicDecisionV1& decision)
{
    std::vector<std::byte> payload;
    AppendDigest(payload, request.compositionIdentity.Digest());
    AppendDigest(payload, request.semanticIdentity.Digest());
    AppendDigest(payload, request.identity.Digest());
    AppendDigest(payload, decision.identity.Digest());
    AppendDigest(payload, decision.dynamicWriteSetIdentity.Digest());
    AppendDigest(payload, request.executionPayloadIdentity.Digest());
    AppendDigest(payload, decision.indirectDispatch.identity.Digest());
    AppendDigest(payload, decision.conditionalExecutionIdentity.Digest());
    return MakeIdentity<DynamicSealIdentity>("SGE.V2.Dynamic.VerificationSeal.V1", payload);
}

canonical::HistoryValidityIdentity ComputeHistoryValidityIdentityV1(
    composition::FrozenCompositionIdentity compositionIdentity,
    canonical::SemanticIdentity semanticIdentity,
    canonical::InvocationIdentity sourceInvocationIdentity,
    canonical::HistoryGeneration generation,
    canonical::DeviceEpoch deviceEpoch,
    const ExactIndexSetV1& activeSet,
    GenerationVectorIdentity generationVectorIdentity)
{
    std::vector<std::byte> payload;
    AppendDigest(payload, compositionIdentity.Digest());
    AppendDigest(payload, semanticIdentity.Digest());
    AppendDigest(payload, sourceInvocationIdentity.Digest());
    AppendU64(payload, generation.value());
    AppendU64(payload, deviceEpoch.value());
    AppendDigest(payload, activeSet.Identity().Digest());
    AppendDigest(payload, generationVectorIdentity.Digest());
    AppendU8(payload, static_cast<std::uint8_t>(canonical::HistoryStateV1::Valid));
    return MakeIdentity<canonical::HistoryValidityIdentity>("SGE.V2.Dynamic.HistoryValidity.V1", payload);
}

FrozenDynamicInvocationIdentity ComputeFrozenDynamicInvocationIdentityV1(
    composition::FrozenCompositionIdentity compositionIdentity,
    canonical::InvocationIdentity invocationIdentity,
    DynamicDecisionIdentity decisionIdentity,
    DynamicSealIdentity sealIdentity,
    canonical::HistoryValidityIdentity nextHistoryIdentity,
    DynamicWriteSetIdentity dynamicWriteSetIdentity,
    DynamicExecutionPayloadIdentity executionPayloadIdentity,
    IndirectDispatchIdentity indirectDispatchIdentity)
{
    std::vector<std::byte> payload;
    AppendDigest(payload, compositionIdentity.Digest());
    AppendDigest(payload, invocationIdentity.Digest());
    AppendDigest(payload, decisionIdentity.Digest());
    AppendDigest(payload, sealIdentity.Digest());
    AppendDigest(payload, nextHistoryIdentity.Digest());
    AppendDigest(payload, dynamicWriteSetIdentity.Digest());
    AppendDigest(payload, executionPayloadIdentity.Digest());
    AppendDigest(payload, indirectDispatchIdentity.Digest());
    return MakeIdentity<FrozenDynamicInvocationIdentity>("SGE.V2.Dynamic.FrozenInvocation.V1", payload);
}
}
