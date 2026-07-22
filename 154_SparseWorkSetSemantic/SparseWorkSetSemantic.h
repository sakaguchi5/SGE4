#pragma once

#include "../00_Foundation/Result.h"
#include "../00_Foundation/Sha256.h"
#include "../134_TemporalStateSemantic/TemporalStateSemantic.h"

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace sge4_5::spiral6::semantic
{
inline constexpr std::uint32_t WorkUniverseCountV1 = 4096;
inline constexpr std::uint32_t ThreadsPerGroupV1 = 64;
inline constexpr std::uint32_t BoneCountV1 = 8;
inline constexpr std::uint32_t OutputRecordStrideV1 = 16;
inline constexpr std::uint32_t CompactIndexStrideV1 = 4;
inline constexpr std::uint32_t CompactIndexCapacityBytesV1 = 16384;
inline constexpr std::uint32_t InvalidUniverseIndexV1 = 0xFFFF'FFFFu;

enum class SparsePatternKindV1 : std::uint32_t
{
    PrefixControl = 1,
    UniformStride = 2,
    BlockClusteredPermutation = 3,
    HashScatterPermutation = 4
};

class WorkUniverseIndexV1 final
{
public:
    explicit WorkUniverseIndexV1(std::uint32_t value);
    [[nodiscard]] std::uint32_t Value() const noexcept { return value_; }
    auto operator<=>(const WorkUniverseIndexV1&) const = default;
private:
    std::uint32_t value_ = 0;
};

class SparseCardinalityV1 final
{
public:
    explicit SparseCardinalityV1(std::uint32_t value);
    [[nodiscard]] std::uint32_t Value() const noexcept { return value_; }
    auto operator<=>(const SparseCardinalityV1&) const = default;
private:
    std::uint32_t value_ = 0;
};

struct SparseSemanticErrorV1 final
{
    std::string stage;
    std::string message;
};

class ExactSparseWorkSetV1 final
{
public:
    ExactSparseWorkSetV1() = delete;
    ExactSparseWorkSetV1(const ExactSparseWorkSetV1&) = default;
    ExactSparseWorkSetV1& operator=(const ExactSparseWorkSetV1&) = default;

    [[nodiscard]] SparseCardinalityV1 Cardinality() const noexcept { return cardinality_; }
    [[nodiscard]] const std::vector<std::uint32_t>& Indices() const noexcept { return indices_; }
    [[nodiscard]] const base::Digest256& Identity() const noexcept { return identity_; }
    [[nodiscard]] bool Contains(std::uint32_t index) const noexcept;

private:
    friend base::Result<ExactSparseWorkSetV1, SparseSemanticErrorV1>
        BuildExactSparseWorkSetV1(std::span<const std::uint32_t>);

    ExactSparseWorkSetV1(
        SparseCardinalityV1 cardinality,
        std::vector<std::uint32_t> indices,
        base::Digest256 identity)
        : cardinality_(cardinality), indices_(std::move(indices)), identity_(identity) {}

    SparseCardinalityV1 cardinality_{0};
    std::vector<std::uint32_t> indices_;
    base::Digest256 identity_{};
};

struct SparsePointTransformSemanticV1 final
{
    std::uint32_t conventionVersion = 1;
    std::uint32_t semanticKind = 1; // ExactSparsePointTransformWorkSetV1
    std::uint32_t workUniverseCount = WorkUniverseCountV1;
    std::uint32_t outputCapacity = WorkUniverseCountV1;
    std::uint32_t threadsPerGroup = ThreadsPerGroupV1;
    std::uint32_t boneCount = BoneCountV1;
    std::uint32_t representationControl = 1; // DirectPgaThroughConsumerV1
    std::uint32_t timelineLength = 1;
    std::uint32_t historyDepth = 0;
    std::uint32_t inactiveSentinel = 1; // Float4Zero
    base::Digest256 pointCorpusIdentity{};
    base::Digest256 motorPaletteIdentity{};
};

[[nodiscard]] SparsePointTransformSemanticV1 BuildCanonicalSparsePointTransformSemanticV1();
[[nodiscard]] std::vector<std::byte> SerializeSparsePointTransformSemanticV1(
    const SparsePointTransformSemanticV1& value);
[[nodiscard]] base::Digest256 SparsePointTransformSemanticIdentityV1(
    const SparsePointTransformSemanticV1& value);

[[nodiscard]] base::Result<ExactSparseWorkSetV1, SparseSemanticErrorV1>
BuildExactSparseWorkSetV1(std::span<const std::uint32_t> strictlyIncreasingIndices);
[[nodiscard]] base::Result<ExactSparseWorkSetV1, SparseSemanticErrorV1>
BuildCanonicalSparseWorkSetV1(SparsePatternKindV1 pattern, SparseCardinalityV1 cardinality);
[[nodiscard]] std::vector<std::byte> SerializeExactSparseWorkSetV1(
    const ExactSparseWorkSetV1& value);
[[nodiscard]] base::Digest256 ExactSparseWorkSetIdentityV1(
    const ExactSparseWorkSetV1& value);

[[nodiscard]] const std::array<std::uint32_t, 21>& QualificationCardinalitiesV1() noexcept;
[[nodiscard]] const std::array<SparsePatternKindV1, 4>& SparsePatternCorpusV1() noexcept;
[[nodiscard]] const char* SparsePatternNameV1(SparsePatternKindV1 value) noexcept;

[[nodiscard]] std::array<spiral5::semantic::PgaMotorRecordV1, BoneCountV1>
BuildCanonicalGlobalMotorPaletteV1();
[[nodiscard]] std::vector<spiral5::semantic::PointRecordV1>
BuildSparseCpuReferenceOutputV1(const ExactSparseWorkSetV1& set);
[[nodiscard]] std::vector<std::byte> BuildCanonicalInactiveOutputBytesV1();
}
