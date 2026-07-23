#include "SparseTemporalDeltaSemantic.h"

#include "../00_Foundation/BinaryIO.h"

#include <algorithm>
#include <array>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <utility>

namespace sge4_5::spiral7::semantic
{
namespace
{
constexpr std::uint32_t SemanticMagic = 0x31533753u;   // "S7S1"
constexpr std::uint32_t InvocationMagic = 0x31493753u; // "S7I1"
constexpr std::uint32_t TransitionMagic = 0x31543753u; // "S7T1"

SparseDeltaSemanticErrorV1 Error(std::string stage, std::string message)
{
    return {std::move(stage), std::move(message)};
}

base::Result<ExactSparseWorkSetV1, SparseDeltaSemanticErrorV1> BuildSet(
    std::span<const std::uint32_t> indices,
    std::string stage)
{
    auto result = spiral6::semantic::BuildExactSparseWorkSetV1(indices);
    if (!result)
    {
        return base::Result<ExactSparseWorkSetV1, SparseDeltaSemanticErrorV1>::Failure(
            Error(std::move(stage), result.Error().message));
    }
    return base::Result<ExactSparseWorkSetV1, SparseDeltaSemanticErrorV1>::Success(
        std::move(result).Value());
}

std::vector<std::uint32_t> Difference(
    const std::vector<std::uint32_t>& left,
    const std::vector<std::uint32_t>& right)
{
    std::vector<std::uint32_t> result;
    result.reserve(left.size());
    std::set_difference(left.begin(), left.end(), right.begin(), right.end(),
                        std::back_inserter(result));
    return result;
}

std::vector<std::uint32_t> Intersection(
    const std::vector<std::uint32_t>& left,
    const std::vector<std::uint32_t>& right)
{
    std::vector<std::uint32_t> result;
    result.reserve(std::min(left.size(), right.size()));
    std::set_intersection(left.begin(), left.end(), right.begin(), right.end(),
                          std::back_inserter(result));
    return result;
}

std::vector<std::uint32_t> Union(
    const std::vector<std::uint32_t>& left,
    const std::vector<std::uint32_t>& right)
{
    std::vector<std::uint32_t> result;
    result.reserve(left.size() + right.size());
    std::set_union(left.begin(), left.end(), right.begin(), right.end(),
                   std::back_inserter(result));
    return result;
}

void WriteDigest(base::BinaryWriter& writer, const base::Digest256& digest)
{
    writer.WriteBytes(digest);
}

base::Digest256 BuildInvocationIdentity(
    std::uint32_t invocationIndex,
    bool forceFullActiveRebuild,
    const ExactSparseWorkSetV1& previousActiveSet,
    const ExactSparseWorkSetV1& activeSet,
    const ExactSparseWorkSetV1& modifiedSurvivorSet,
    const ExactSparseWorkSetV1& activationSet,
    const ExactSparseWorkSetV1& deactivationSet,
    const ExactSparseWorkSetV1& updateSet,
    const ExactSparseWorkSetV1& retainSet,
    const ExactSparseWorkSetV1& transitionSet,
    std::span<const std::uint32_t> itemGenerations,
    std::span<const DeltaIndexActionRecordV1> records)
{
    base::BinaryWriter writer;
    writer.WriteU32(InvocationMagic);
    writer.WriteU32(1);
    writer.WriteU32(invocationIndex);
    writer.WriteU32(forceFullActiveRebuild ? 1u : 0u);
    WriteDigest(writer, previousActiveSet.Identity());
    WriteDigest(writer, activeSet.Identity());
    WriteDigest(writer, modifiedSurvivorSet.Identity());
    WriteDigest(writer, activationSet.Identity());
    WriteDigest(writer, deactivationSet.Identity());
    WriteDigest(writer, updateSet.Identity());
    WriteDigest(writer, retainSet.Identity());
    WriteDigest(writer, transitionSet.Identity());
    WriteDigest(writer, DeltaTransitionRecordsIdentityV1(records));
    writer.WriteU32(static_cast<std::uint32_t>(itemGenerations.size()));
    for (const std::uint32_t generation : itemGenerations) writer.WriteU32(generation);
    return base::Sha256(writer.Bytes());
}
} // namespace

SparseDeltaInvocationV1::SparseDeltaInvocationV1(
    std::uint32_t invocationIndex,
    bool forceFullActiveRebuild,
    ExactSparseWorkSetV1 previousActiveSet,
    ExactSparseWorkSetV1 activeSet,
    ExactSparseWorkSetV1 modifiedSurvivorSet,
    ExactSparseWorkSetV1 activationSet,
    ExactSparseWorkSetV1 deactivationSet,
    ExactSparseWorkSetV1 updateSet,
    ExactSparseWorkSetV1 retainSet,
    ExactSparseWorkSetV1 transitionSet,
    std::vector<std::uint32_t> itemGenerations,
    std::vector<DeltaIndexActionRecordV1> transitionRecords,
    base::Digest256 identity)
    : invocationIndex_(invocationIndex),
      forceFullActiveRebuild_(forceFullActiveRebuild),
      previousActiveSet_(std::move(previousActiveSet)),
      activeSet_(std::move(activeSet)),
      modifiedSurvivorSet_(std::move(modifiedSurvivorSet)),
      activationSet_(std::move(activationSet)),
      deactivationSet_(std::move(deactivationSet)),
      updateSet_(std::move(updateSet)),
      retainSet_(std::move(retainSet)),
      transitionSet_(std::move(transitionSet)),
      itemGenerations_(std::move(itemGenerations)),
      transitionRecords_(std::move(transitionRecords)),
      identity_(identity)
{
}

SparseTemporalDeltaSemanticV1 BuildCanonicalSparseTemporalDeltaSemanticV1()
{
    SparseTemporalDeltaSemanticV1 semantic;
    const auto sparse = spiral6::semantic::BuildCanonicalSparsePointTransformSemanticV1();
    semantic.sparseSemanticIdentity = spiral6::semantic::SparsePointTransformSemanticIdentityV1(sparse);
    const auto temporal = spiral5::semantic::BuildCanonicalTemporalStateSemanticV1();
    semantic.temporalSemanticIdentity = spiral5::semantic::TemporalStateSemanticIdentityV1(temporal);
    return semantic;
}

std::vector<std::byte> SerializeSparseTemporalDeltaSemanticV1(
    const SparseTemporalDeltaSemanticV1& value)
{
    const auto canonical = BuildCanonicalSparseTemporalDeltaSemanticV1();
    if (value.conventionVersion != canonical.conventionVersion ||
        value.semanticKind != canonical.semanticKind ||
        value.workUniverseCount != WorkUniverseCountV1 ||
        value.outputCapacity != WorkUniverseCountV1 ||
        value.threadsPerGroup != ThreadsPerGroupV1 ||
        value.timelineLength != TimelineLengthV1 ||
        value.historyDepth != HistoryDepthV1 ||
        value.representationControl != 1 ||
        value.inactiveSentinel != 1 ||
        value.sparseSemanticIdentity != canonical.sparseSemanticIdentity ||
        value.temporalSemanticIdentity != canonical.temporalSemanticIdentity)
    {
        throw std::invalid_argument("SparseTemporalDeltaSemanticV1 is not canonical.");
    }

    base::BinaryWriter writer;
    writer.WriteU32(SemanticMagic);
    writer.WriteU32(value.conventionVersion);
    writer.WriteU32(value.semanticKind);
    writer.WriteU32(value.workUniverseCount);
    writer.WriteU32(value.outputCapacity);
    writer.WriteU32(value.threadsPerGroup);
    writer.WriteU32(value.timelineLength);
    writer.WriteU32(value.historyDepth);
    writer.WriteU32(value.representationControl);
    writer.WriteU32(value.inactiveSentinel);
    WriteDigest(writer, value.sparseSemanticIdentity);
    WriteDigest(writer, value.temporalSemanticIdentity);
    return std::move(writer).Take();
}

base::Digest256 SparseTemporalDeltaSemanticIdentityV1(
    const SparseTemporalDeltaSemanticV1& value)
{
    const auto bytes = SerializeSparseTemporalDeltaSemanticV1(value);
    return base::Sha256(bytes);
}

base::Result<SparseDeltaInvocationV1, SparseDeltaSemanticErrorV1>
BuildSparseDeltaInvocationV1(
    std::uint32_t invocationIndex,
    const ExactSparseWorkSetV1& previousActiveSet,
    const ExactSparseWorkSetV1& activeSet,
    const ExactSparseWorkSetV1& modifiedSurvivorSet,
    std::span<const std::uint32_t> previousItemGenerations,
    bool forceFullActiveRebuild)
{
    if (invocationIndex >= TimelineLengthV1)
    {
        return base::Result<SparseDeltaInvocationV1, SparseDeltaSemanticErrorV1>::Failure(
            Error("delta/invocation", "Invocation index is outside the fixed 128-invocation timeline."));
    }
    if (previousItemGenerations.size() != WorkUniverseCountV1)
    {
        return base::Result<SparseDeltaInvocationV1, SparseDeltaSemanticErrorV1>::Failure(
            Error("delta/generation", "Previous per-item generation vector must contain 4096 entries."));
    }

    const auto survivorIndices = Intersection(previousActiveSet.Indices(), activeSet.Indices());
    if (!std::includes(survivorIndices.begin(), survivorIndices.end(),
                       modifiedSurvivorSet.Indices().begin(), modifiedSurvivorSet.Indices().end()))
    {
        return base::Result<SparseDeltaInvocationV1, SparseDeltaSemanticErrorV1>::Failure(
            Error("delta/modified-subset",
                  "Modified Survivor Set must be a subset of A_t intersect A_(t-1)."));
    }

    const auto activationIndices = Difference(activeSet.Indices(), previousActiveSet.Indices());
    const auto normalDeactivationIndices = Difference(previousActiveSet.Indices(), activeSet.Indices());
    const auto normalRetainIndices = Difference(survivorIndices, modifiedSurvivorSet.Indices());
    const auto normalUpdateIndices = Union(activationIndices, modifiedSurvivorSet.Indices());

    const std::vector<std::uint32_t> deactivationIndices =
        forceFullActiveRebuild ? std::vector<std::uint32_t>{} : normalDeactivationIndices;
    const std::vector<std::uint32_t> retainIndices =
        forceFullActiveRebuild ? std::vector<std::uint32_t>{} : normalRetainIndices;
    const std::vector<std::uint32_t> updateIndices =
        forceFullActiveRebuild ? activeSet.Indices() : normalUpdateIndices;
    const auto transitionIndices = Union(updateIndices, deactivationIndices);

    auto activationSetResult = BuildSet(activationIndices, "delta/activation-set");
    auto deactivationSetResult = BuildSet(deactivationIndices, "delta/deactivation-set");
    auto updateSetResult = BuildSet(updateIndices, "delta/update-set");
    auto retainSetResult = BuildSet(retainIndices, "delta/retain-set");
    auto transitionSetResult = BuildSet(transitionIndices, "delta/transition-set");
    if (!activationSetResult) return base::Result<SparseDeltaInvocationV1, SparseDeltaSemanticErrorV1>::Failure(activationSetResult.Error());
    if (!deactivationSetResult) return base::Result<SparseDeltaInvocationV1, SparseDeltaSemanticErrorV1>::Failure(deactivationSetResult.Error());
    if (!updateSetResult) return base::Result<SparseDeltaInvocationV1, SparseDeltaSemanticErrorV1>::Failure(updateSetResult.Error());
    if (!retainSetResult) return base::Result<SparseDeltaInvocationV1, SparseDeltaSemanticErrorV1>::Failure(retainSetResult.Error());
    if (!transitionSetResult) return base::Result<SparseDeltaInvocationV1, SparseDeltaSemanticErrorV1>::Failure(transitionSetResult.Error());

    std::vector<std::uint32_t> generations(WorkUniverseCountV1, InvalidGenerationV1);
    for (const std::uint32_t index : previousActiveSet.Indices())
    {
        if (previousItemGenerations[index] == InvalidGenerationV1)
        {
            return base::Result<SparseDeltaInvocationV1, SparseDeltaSemanticErrorV1>::Failure(
                Error("delta/generation", "A previous active member has invalid semantic generation."));
        }
    }

    for (const std::uint32_t index : activeSet.Indices())
    {
        if (!previousActiveSet.Contains(index))
        {
            generations[index] = 0;
            continue;
        }

        const std::uint32_t previous = previousItemGenerations[index];
        if (modifiedSurvivorSet.Contains(index))
        {
            if (previous >= InvalidGenerationV1 - 1u)
            {
                return base::Result<SparseDeltaInvocationV1, SparseDeltaSemanticErrorV1>::Failure(
                    Error("delta/generation", "Per-item generation overflow."));
            }
            generations[index] = previous + 1u;
        }
        else
        {
            generations[index] = previous;
        }
    }

    std::vector<DeltaIndexActionRecordV1> records;
    records.reserve(transitionIndices.size());
    std::size_t updateCursor = 0;
    std::size_t clearCursor = 0;
    while (updateCursor < updateIndices.size() || clearCursor < deactivationIndices.size())
    {
        const bool takeUpdate = clearCursor >= deactivationIndices.size() ||
            (updateCursor < updateIndices.size() && updateIndices[updateCursor] < deactivationIndices[clearCursor]);
        if (takeUpdate)
        {
            records.push_back({updateIndices[updateCursor], DeltaActionV1::Update});
            ++updateCursor;
        }
        else
        {
            records.push_back({deactivationIndices[clearCursor], DeltaActionV1::Clear});
            ++clearCursor;
        }
    }

    auto activationSet = std::move(activationSetResult).Value();
    auto deactivationSet = std::move(deactivationSetResult).Value();
    auto updateSet = std::move(updateSetResult).Value();
    auto retainSet = std::move(retainSetResult).Value();
    auto transitionSet = std::move(transitionSetResult).Value();
    const auto identity = BuildInvocationIdentity(
        invocationIndex, forceFullActiveRebuild, previousActiveSet, activeSet,
        modifiedSurvivorSet, activationSet, deactivationSet, updateSet, retainSet,
        transitionSet, generations, records);

    return base::Result<SparseDeltaInvocationV1, SparseDeltaSemanticErrorV1>::Success(
        SparseDeltaInvocationV1(
            invocationIndex,
            forceFullActiveRebuild,
            previousActiveSet,
            activeSet,
            modifiedSurvivorSet,
            std::move(activationSet),
            std::move(deactivationSet),
            std::move(updateSet),
            std::move(retainSet),
            std::move(transitionSet),
            std::move(generations),
            std::move(records),
            identity));
}

std::vector<std::byte> SerializeSparseDeltaInvocationV1(
    const SparseDeltaInvocationV1& value)
{
    base::BinaryWriter writer;
    writer.WriteU32(InvocationMagic);
    writer.WriteU32(1);
    writer.WriteU32(value.InvocationIndex());
    writer.WriteU32(value.ForceFullActiveRebuild() ? 1u : 0u);
    WriteDigest(writer, value.PreviousActiveSet().Identity());
    WriteDigest(writer, value.ActiveSet().Identity());
    WriteDigest(writer, value.ModifiedSurvivorSet().Identity());
    WriteDigest(writer, value.ActivationSet().Identity());
    WriteDigest(writer, value.DeactivationSet().Identity());
    WriteDigest(writer, value.UpdateSet().Identity());
    WriteDigest(writer, value.RetainSet().Identity());
    WriteDigest(writer, value.TransitionSet().Identity());
    WriteDigest(writer, DeltaTransitionRecordsIdentityV1(value.TransitionRecords()));
    writer.WriteU32(static_cast<std::uint32_t>(value.ItemGenerations().size()));
    for (const std::uint32_t generation : value.ItemGenerations()) writer.WriteU32(generation);
    auto bytes = std::move(writer).Take();
    if (base::Sha256(bytes) != value.Identity())
        throw std::invalid_argument("SparseDeltaInvocationV1 identity does not match canonical bytes.");
    return bytes;
}

base::Digest256 SparseDeltaInvocationIdentityV1(const SparseDeltaInvocationV1& value)
{
    const auto bytes = SerializeSparseDeltaInvocationV1(value);
    return base::Sha256(bytes);
}

base::Digest256 DeltaTransitionRecordsIdentityV1(
    std::span<const DeltaIndexActionRecordV1> records)
{
    base::BinaryWriter writer;
    writer.WriteU32(TransitionMagic);
    writer.WriteU32(1);
    writer.WriteU32(static_cast<std::uint32_t>(records.size()));
    std::uint32_t previous = 0;
    bool hasPrevious = false;
    for (const auto& record : records)
    {
        if (record.universeIndex >= WorkUniverseCountV1 ||
            (record.action != DeltaActionV1::Update && record.action != DeltaActionV1::Clear) ||
            (hasPrevious && record.universeIndex <= previous))
        {
            throw std::invalid_argument("Delta transition records are not canonical.");
        }
        writer.WriteU32(record.universeIndex);
        writer.WriteU32(static_cast<std::uint32_t>(record.action));
        previous = record.universeIndex;
        hasPrevious = true;
    }
    return base::Sha256(writer.Bytes());
}

std::vector<spiral5::semantic::PointRecordV1>
BuildPointCorpusForInvocationV1(const SparseDeltaInvocationV1& invocation)
{
    auto points = spiral5::semantic::BuildCanonicalPointCorpusV1();
    if (points.size() != static_cast<std::size_t>(WorkUniverseCountV1) ||
        invocation.ItemGenerations().size() != static_cast<std::size_t>(WorkUniverseCountV1))
        throw std::runtime_error("Canonical point corpus or generation vector has unexpected size.");

    for (const std::uint32_t index : invocation.ActiveSet().Indices())
    {
        const std::uint32_t generation = invocation.ItemGenerations()[index];
        if (generation == InvalidGenerationV1)
            throw std::runtime_error("Active point has invalid generation.");
        const float delta = static_cast<float>(generation) * (1.0f / 1024.0f);
        points[index].x += delta;
        points[index].y -= delta * 0.5f;
        points[index].z += delta * 0.25f;
        points[index].w = 1.0f;
    }
    return points;
}

std::vector<spiral5::semantic::PointRecordV1>
BuildCpuReferenceOutputV1(const SparseDeltaInvocationV1& invocation)
{
    const auto points = BuildPointCorpusForInvocationV1(invocation);
    const auto motors = spiral6::semantic::BuildCanonicalGlobalMotorPaletteV1();
    const auto full = spiral5::semantic::ApplyGlobalMotorsCpuV1(motors, points);
    std::vector<spiral5::semantic::PointRecordV1> output(
        WorkUniverseCountV1,
        spiral5::semantic::PointRecordV1{0.0f, 0.0f, 0.0f, 0.0f});
    for (const std::uint32_t index : invocation.ActiveSet().Indices()) output[index] = full[index];
    return output;
}

std::vector<std::byte> BuildCanonicalInactiveHistoryBytesV1()
{
    return std::vector<std::byte>(
        static_cast<std::size_t>(WorkUniverseCountV1) * sizeof(spiral5::semantic::PointRecordV1),
        std::byte{0});
}

std::vector<std::uint32_t> BuildInitialInvalidGenerationsV1()
{
    return std::vector<std::uint32_t>(WorkUniverseCountV1, InvalidGenerationV1);
}
} // namespace sge4_5::spiral7::semantic
