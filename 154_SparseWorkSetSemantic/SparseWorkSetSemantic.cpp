#include "SparseWorkSetSemantic.h"

#include "../00_Foundation/BinaryIO.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>

namespace sge4_5::spiral6::semantic
{
namespace
{
constexpr std::uint32_t SemanticMagic = 0x31533653u; // "S6S1"
constexpr std::uint32_t SetMagic = 0x31573653u;      // "S6W1"

SparseSemanticErrorV1 Error(std::string stage, std::string message)
{
    return {std::move(stage), std::move(message)};
}

std::vector<std::uint32_t> BuildPatternIndices(
    SparsePatternKindV1 pattern,
    std::uint32_t count)
{
    std::vector<std::uint32_t> indices;
    indices.reserve(count);
    if (count == 0) return indices;

    switch (pattern)
    {
    case SparsePatternKindV1::PrefixControl:
        for (std::uint32_t index = 0; index < count; ++index)
            indices.push_back(index);
        break;

    case SparsePatternKindV1::UniformStride:
        for (std::uint32_t j = 0; j < count; ++j)
        {
            const auto numerator = static_cast<std::uint64_t>(j) * WorkUniverseCountV1;
            indices.push_back(static_cast<std::uint32_t>(numerator / count));
        }
        break;

    case SparsePatternKindV1::BlockClusteredPermutation:
    {
        const std::uint32_t fullBlocks = count / ThreadsPerGroupV1;
        const std::uint32_t remainder = count % ThreadsPerGroupV1;
        for (std::uint32_t j = 0; j < fullBlocks; ++j)
        {
            const std::uint32_t block = (17u * j + 5u) % 64u;
            for (std::uint32_t local = 0; local < ThreadsPerGroupV1; ++local)
                indices.push_back(block * ThreadsPerGroupV1 + local);
        }
        if (remainder != 0)
        {
            const std::uint32_t block = (17u * fullBlocks + 5u) % 64u;
            for (std::uint32_t local = 0; local < remainder; ++local)
                indices.push_back(block * ThreadsPerGroupV1 + local);
        }
        std::sort(indices.begin(), indices.end());
        break;
    }

    case SparsePatternKindV1::HashScatterPermutation:
        for (std::uint32_t j = 0; j < count; ++j)
            indices.push_back((4051u * j + 17u) % WorkUniverseCountV1);
        std::sort(indices.begin(), indices.end());
        break;

    default:
        throw std::invalid_argument("Unknown SparsePatternKindV1.");
    }
    return indices;
}
}

WorkUniverseIndexV1::WorkUniverseIndexV1(std::uint32_t value)
    : value_(value)
{
    if (value >= WorkUniverseCountV1)
        throw std::invalid_argument("WorkUniverseIndexV1 is outside the 4096-record universe.");
}

SparseCardinalityV1::SparseCardinalityV1(std::uint32_t value)
    : value_(value)
{
    if (value > WorkUniverseCountV1)
        throw std::invalid_argument("SparseCardinalityV1 exceeds the 4096-record universe.");
}

bool ExactSparseWorkSetV1::Contains(std::uint32_t index) const noexcept
{
    return std::binary_search(indices_.begin(), indices_.end(), index);
}

SparsePointTransformSemanticV1 BuildCanonicalSparsePointTransformSemanticV1()
{
    SparsePointTransformSemanticV1 semantic;
    const auto points = spiral5::semantic::BuildCanonicalPointCorpusV1();
    const auto pointBytes = spiral5::semantic::SerializePointRecordsV1(points);
    semantic.pointCorpusIdentity = base::Sha256(pointBytes);
    const auto motors = BuildCanonicalGlobalMotorPaletteV1();
    const auto motorBytes = spiral5::semantic::SerializeMotorRecordsV1(motors);
    semantic.motorPaletteIdentity = base::Sha256(motorBytes);
    return semantic;
}

std::vector<std::byte> SerializeSparsePointTransformSemanticV1(
    const SparsePointTransformSemanticV1& value)
{
    const auto canonical = BuildCanonicalSparsePointTransformSemanticV1();
    if (value.conventionVersion != canonical.conventionVersion ||
        value.semanticKind != canonical.semanticKind ||
        value.workUniverseCount != WorkUniverseCountV1 ||
        value.outputCapacity != WorkUniverseCountV1 ||
        value.threadsPerGroup != ThreadsPerGroupV1 ||
        value.boneCount != BoneCountV1 ||
        value.representationControl != 1 ||
        value.timelineLength != 1 ||
        value.historyDepth != 0 ||
        value.inactiveSentinel != 1 ||
        value.pointCorpusIdentity != canonical.pointCorpusIdentity ||
        value.motorPaletteIdentity != canonical.motorPaletteIdentity)
    {
        throw std::invalid_argument("SparsePointTransformSemanticV1 is not canonical.");
    }

    base::BinaryWriter writer;
    writer.WriteU32(SemanticMagic);
    writer.WriteU32(value.conventionVersion);
    writer.WriteU32(value.semanticKind);
    writer.WriteU32(value.workUniverseCount);
    writer.WriteU32(value.outputCapacity);
    writer.WriteU32(value.threadsPerGroup);
    writer.WriteU32(value.boneCount);
    writer.WriteU32(value.representationControl);
    writer.WriteU32(value.timelineLength);
    writer.WriteU32(value.historyDepth);
    writer.WriteU32(value.inactiveSentinel);
    writer.WriteBytes(value.pointCorpusIdentity);
    writer.WriteBytes(value.motorPaletteIdentity);
    return std::move(writer).Take();
}

base::Digest256 SparsePointTransformSemanticIdentityV1(
    const SparsePointTransformSemanticV1& value)
{
    const auto bytes = SerializeSparsePointTransformSemanticV1(value);
    return base::Sha256(bytes);
}

base::Result<ExactSparseWorkSetV1, SparseSemanticErrorV1>
BuildExactSparseWorkSetV1(std::span<const std::uint32_t> strictlyIncreasingIndices)
{
    if (strictlyIncreasingIndices.size() > WorkUniverseCountV1)
    {
        return base::Result<ExactSparseWorkSetV1, SparseSemanticErrorV1>::Failure(
            Error("sparse-set/cardinality", "Sparse set contains more than 4096 indices."));
    }

    std::uint32_t previous = 0;
    bool hasPrevious = false;
    for (std::uint32_t index : strictlyIncreasingIndices)
    {
        if (index >= WorkUniverseCountV1)
        {
            return base::Result<ExactSparseWorkSetV1, SparseSemanticErrorV1>::Failure(
                Error("sparse-set/range", "Sparse set contains an out-of-range universe index."));
        }
        if (hasPrevious && index <= previous)
        {
            return base::Result<ExactSparseWorkSetV1, SparseSemanticErrorV1>::Failure(
                Error("sparse-set/order", "Sparse set indices are not strictly increasing and unique."));
        }
        previous = index;
        hasPrevious = true;
    }

    std::vector<std::uint32_t> owned(strictlyIncreasingIndices.begin(), strictlyIncreasingIndices.end());
    base::BinaryWriter writer;
    writer.WriteU32(SetMagic);
    writer.WriteU32(1);
    writer.WriteU32(WorkUniverseCountV1);
    writer.WriteU32(static_cast<std::uint32_t>(owned.size()));
    for (std::uint32_t index : owned) writer.WriteU32(index);
    const auto identity = base::Sha256(writer.Bytes());
    const auto cardinality = SparseCardinalityV1(static_cast<std::uint32_t>(owned.size()));
    return base::Result<ExactSparseWorkSetV1, SparseSemanticErrorV1>::Success(
        ExactSparseWorkSetV1(cardinality, std::move(owned), identity));
}

base::Result<ExactSparseWorkSetV1, SparseSemanticErrorV1>
BuildCanonicalSparseWorkSetV1(SparsePatternKindV1 pattern, SparseCardinalityV1 cardinality)
{
    try
    {
        auto indices = BuildPatternIndices(pattern, cardinality.Value());
        return BuildExactSparseWorkSetV1(indices);
    }
    catch (const std::exception& error)
    {
        return base::Result<ExactSparseWorkSetV1, SparseSemanticErrorV1>::Failure(
            Error("sparse-set/pattern", error.what()));
    }
}

std::vector<std::byte> SerializeExactSparseWorkSetV1(const ExactSparseWorkSetV1& value)
{
    base::BinaryWriter writer;
    writer.WriteU32(SetMagic);
    writer.WriteU32(1);
    writer.WriteU32(WorkUniverseCountV1);
    writer.WriteU32(value.Cardinality().Value());
    for (std::uint32_t index : value.Indices()) writer.WriteU32(index);
    auto bytes = std::move(writer).Take();
    if (base::Sha256(bytes) != value.Identity())
        throw std::invalid_argument("ExactSparseWorkSetV1 identity does not match its canonical bytes.");
    return bytes;
}

base::Digest256 ExactSparseWorkSetIdentityV1(const ExactSparseWorkSetV1& value)
{
    const auto bytes = SerializeExactSparseWorkSetV1(value);
    return base::Sha256(bytes);
}

const std::array<std::uint32_t, 21>& QualificationCardinalitiesV1() noexcept
{
    static constexpr std::array<std::uint32_t, 21> values{
        0u,1u,2u,31u,32u,63u,64u,65u,127u,128u,129u,
        255u,256u,257u,511u,512u,1024u,2048u,3072u,4095u,4096u};
    return values;
}

const std::array<SparsePatternKindV1, 4>& SparsePatternCorpusV1() noexcept
{
    static constexpr std::array<SparsePatternKindV1, 4> values{
        SparsePatternKindV1::PrefixControl,
        SparsePatternKindV1::UniformStride,
        SparsePatternKindV1::BlockClusteredPermutation,
        SparsePatternKindV1::HashScatterPermutation};
    return values;
}

const char* SparsePatternNameV1(SparsePatternKindV1 value) noexcept
{
    switch (value)
    {
    case SparsePatternKindV1::PrefixControl: return "PrefixControl";
    case SparsePatternKindV1::UniformStride: return "UniformStride";
    case SparsePatternKindV1::BlockClusteredPermutation: return "BlockClusteredPermutation";
    case SparsePatternKindV1::HashScatterPermutation: return "HashScatterPermutation";
    }
    return "Unknown";
}

std::array<spiral5::semantic::PgaMotorRecordV1, BoneCountV1>
BuildCanonicalGlobalMotorPaletteV1()
{
    const auto local = spiral5::semantic::BuildCanonicalLocalMotorGenerationV1(0);
    return spiral5::semantic::ComposeCanonicalGlobalMotorsCpuV1(local);
}

std::vector<spiral5::semantic::PointRecordV1>
BuildSparseCpuReferenceOutputV1(const ExactSparseWorkSetV1& set)
{
    const auto points = spiral5::semantic::BuildCanonicalPointCorpusV1();
    const auto motors = BuildCanonicalGlobalMotorPaletteV1();
    const auto full = spiral5::semantic::ApplyGlobalMotorsCpuV1(motors, points);
    std::vector<spiral5::semantic::PointRecordV1> output(WorkUniverseCountV1);
    for (std::uint32_t index : set.Indices()) output[index] = full[index];
    return output;
}

std::vector<std::byte> BuildCanonicalInactiveOutputBytesV1()
{
    return std::vector<std::byte>(
        static_cast<std::size_t>(WorkUniverseCountV1) * OutputRecordStrideV1,
        std::byte{0});
}
}
