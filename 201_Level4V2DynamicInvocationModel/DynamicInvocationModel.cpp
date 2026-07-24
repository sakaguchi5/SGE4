#include "DynamicInvocationModel.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace sge4_5::v2::dynamic
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
    if (universe.Value() == 0u)
        return {ExactIndexSetErrorV1::ZeroUniverse, std::nullopt};

    std::vector<std::uint32_t> canonicalIndices(indices.begin(), indices.end());
    for (const auto index : canonicalIndices)
    {
        if (index >= universe.Value())
            return {ExactIndexSetErrorV1::IndexOutOfRange, std::nullopt};
    }
    std::sort(canonicalIndices.begin(), canonicalIndices.end());
    if (std::adjacent_find(canonicalIndices.begin(), canonicalIndices.end()) != canonicalIndices.end())
        return {ExactIndexSetErrorV1::DuplicateIndex, std::nullopt};

    std::vector<std::byte> payload;
    AppendU32(payload, universe.Value());
    AppendU64(payload, static_cast<std::uint64_t>(canonicalIndices.size()));
    for (const auto index : canonicalIndices) AppendU32(payload, index);
    auto identity = MakeIdentity<IndexSetIdentity>("SGE.V2.Dynamic.ExactIndexSet.V1", payload);
    return {ExactIndexSetErrorV1::None, ExactIndexSetV1(universe, std::move(identity), std::move(canonicalIndices))};
}

IndexSetIdentity ComputeIndexSetIdentityV1(const ExactIndexSetV1& set)
{
    std::vector<std::byte> payload;
    AppendU32(payload, set.universe_.Value());
    AppendU64(payload, static_cast<std::uint64_t>(set.indices_.size()));
    for (const auto index : set.indices_) AppendU32(payload, index);
    return MakeIdentity<IndexSetIdentity>("SGE.V2.Dynamic.ExactIndexSet.V1", payload);
}

DynamicInvocationRequestV1 MakeDynamicInvocationRequestV1(
    const frozen_composition::OpaqueFrozenCompositionV1& composition,
    canonical::SemanticIdentity semanticIdentity,
    canonical::TimelineOrdinal timelineOrdinal,
    canonical::DeviceEpoch deviceEpoch,
    UniverseCount universe,
    InvocationModeV1 mode,
    ExactIndexSetV1 activeSet,
    ExactIndexSetV1 modifiedSurvivorSet,
    std::optional<VerifiedHistoryStateV1> previousHistory)
{
    if (activeSet.Universe() != universe || modifiedSurvivorSet.Universe() != universe)
        throw std::invalid_argument("Dynamic invocation sets do not match the explicit universe.");

    DynamicInvocationRequestV1 request{
        canonical::InvocationIdentity::FromDigest({}),
        composition.Identity(),
        std::move(semanticIdentity),
        timelineOrdinal,
        deviceEpoch,
        universe,
        mode,
        std::move(activeSet),
        std::move(modifiedSurvivorSet),
        std::move(previousHistory)};
    request.identity = ComputeDynamicInvocationIdentityV1(request);
    return request;
}

canonical::InvocationIdentity ComputeDynamicInvocationIdentityV1(
    const DynamicInvocationRequestV1& request)
{
    std::vector<std::byte> payload;
    AppendDigest(payload, request.compositionIdentity.Digest());
    AppendDigest(payload, request.semanticIdentity.Digest());
    AppendU64(payload, request.timelineOrdinal.Value());
    AppendU64(payload, request.deviceEpoch.Value());
    AppendU32(payload, request.universe.Value());
    AppendU8(payload, static_cast<std::uint8_t>(request.mode));
    AppendDigest(payload, request.activeSet.Identity().Digest());
    AppendDigest(payload, request.modifiedSurvivorSet.Identity().Digest());
    AppendU8(payload, request.previousHistory.has_value() ? 1u : 0u);
    if (request.previousHistory.has_value())
        AppendDigest(payload, request.previousHistory->Descriptor().identity.Digest());
    return MakeIdentity<canonical::InvocationIdentity>("SGE.V2.Dynamic.InvocationRequest.V1", payload);
}

GenerationVectorIdentity ComputeGenerationVectorIdentityV1(
    UniverseCount universe,
    std::span<const std::uint64_t> generations)
{
    std::vector<std::byte> payload;
    AppendU32(payload, universe.Value());
    AppendU64(payload, static_cast<std::uint64_t>(generations.size()));
    for (const auto generation : generations) AppendU64(payload, generation);
    return MakeIdentity<GenerationVectorIdentity>("SGE.V2.Dynamic.GenerationVector.V1", payload);
}

TransitionRecordSetIdentity ComputeTransitionRecordSetIdentityV1(
    UniverseCount universe,
    std::span<const TransitionRecordV1> records)
{
    std::vector<std::byte> payload;
    AppendU32(payload, universe.Value());
    AppendU64(payload, static_cast<std::uint64_t>(records.size()));
    for (const auto& record : records)
    {
        AppendU32(payload, record.member.Value());
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
    AppendU32(payload, decision.indirectWorkCount.Value());
    AppendDigest(payload, decision.dynamicWriteSetIdentity.Digest());
    AppendU64(payload, decision.nextHistoryGeneration.Value());
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
    AppendU64(payload, generation.Value());
    AppendU64(payload, deviceEpoch.Value());
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
    DynamicWriteSetIdentity dynamicWriteSetIdentity)
{
    std::vector<std::byte> payload;
    AppendDigest(payload, compositionIdentity.Digest());
    AppendDigest(payload, invocationIdentity.Digest());
    AppendDigest(payload, decisionIdentity.Digest());
    AppendDigest(payload, sealIdentity.Digest());
    AppendDigest(payload, nextHistoryIdentity.Digest());
    AppendDigest(payload, dynamicWriteSetIdentity.Digest());
    return MakeIdentity<FrozenDynamicInvocationIdentity>("SGE.V2.Dynamic.FrozenInvocation.V1", payload);
}
}
