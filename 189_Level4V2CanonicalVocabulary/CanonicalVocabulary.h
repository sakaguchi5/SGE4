#pragma once

#include <array>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

namespace sge4_5::v2::canonical
{
using Digest256V1 = std::array<std::byte, 32>;

template<class Tag, std::unsigned_integral Rep = std::uint32_t>
class StrongScalarV1 final
{
public:
    using Representation = Rep;

    explicit constexpr StrongScalarV1(Rep value) noexcept : value_(value) {}

    [[nodiscard]] constexpr Rep Value() const noexcept { return value_; }
    auto operator<=>(const StrongScalarV1&) const = default;

private:
    Rep value_{};
};

struct SemanticVersionTagV1;
struct HistoryGenerationTagV1;
struct ActiveCountTagV1;
struct TransitionCountTagV1;
struct ReuseCountTagV1;
struct UpdateIntervalTagV1;
struct BatchSizeTagV1;
struct AffectedBlockCountTagV1;
struct TimelineOrdinalTagV1;

using SemanticVersion = StrongScalarV1<SemanticVersionTagV1>;
using HistoryGeneration = StrongScalarV1<HistoryGenerationTagV1, std::uint64_t>;
using ActiveCount = StrongScalarV1<ActiveCountTagV1>;
using TransitionCount = StrongScalarV1<TransitionCountTagV1>;
using ReuseCount = StrongScalarV1<ReuseCountTagV1>;
using UpdateInterval = StrongScalarV1<UpdateIntervalTagV1>;
using BatchSize = StrongScalarV1<BatchSizeTagV1>;
using AffectedBlockCount = StrongScalarV1<AffectedBlockCountTagV1>;
using TimelineOrdinal = StrongScalarV1<TimelineOrdinalTagV1, std::uint64_t>;

class DeviceEpoch final
{
public:
    [[nodiscard]] static std::optional<DeviceEpoch> TryCreate(std::uint64_t value) noexcept;

    [[nodiscard]] constexpr std::uint64_t Value() const noexcept { return value_; }
    auto operator<=>(const DeviceEpoch&) const = default;

private:
    explicit constexpr DeviceEpoch(std::uint64_t value) noexcept : value_(value) {}
    std::uint64_t value_;
};

template<class Tag>
class CanonicalIdentityV1 final
{
public:
    [[nodiscard]] static constexpr CanonicalIdentityV1 FromDigest(Digest256V1 digest) noexcept
    {
        return CanonicalIdentityV1(std::move(digest));
    }

    [[nodiscard]] constexpr const Digest256V1& Digest() const noexcept { return digest_; }
    auto operator<=>(const CanonicalIdentityV1&) const = default;

private:
    explicit constexpr CanonicalIdentityV1(Digest256V1 digest) noexcept : digest_(std::move(digest)) {}
    Digest256V1 digest_{};
};

struct SemanticIdentityTagV1;
struct InvocationIdentityTagV1;
struct CandidateIdentityTagV1;
struct VerifiedPlanIdentityTagV1;
struct FrozenArtifactIdentityTagV1;
struct ResourceContractIdentityTagV1;
struct ResourceInstanceIdentityTagV1;
struct WriteSetIdentityTagV1;
struct HistoryValidityIdentityTagV1;
struct TargetProfileIdentityTagV1;
struct ObservationContractIdentityTagV1;

using SemanticIdentity = CanonicalIdentityV1<SemanticIdentityTagV1>;
using InvocationIdentity = CanonicalIdentityV1<InvocationIdentityTagV1>;
using CandidateIdentity = CanonicalIdentityV1<CandidateIdentityTagV1>;
using VerifiedPlanIdentity = CanonicalIdentityV1<VerifiedPlanIdentityTagV1>;
using FrozenArtifactIdentity = CanonicalIdentityV1<FrozenArtifactIdentityTagV1>;
using ResourceContractIdentity = CanonicalIdentityV1<ResourceContractIdentityTagV1>;
using ResourceInstanceIdentity = CanonicalIdentityV1<ResourceInstanceIdentityTagV1>;
using WriteSetIdentity = CanonicalIdentityV1<WriteSetIdentityTagV1>;
using HistoryValidityIdentity = CanonicalIdentityV1<HistoryValidityIdentityTagV1>;
using TargetProfileIdentity = CanonicalIdentityV1<TargetProfileIdentityTagV1>;
using ObservationContractIdentity = CanonicalIdentityV1<ObservationContractIdentityTagV1>;

struct CanonicalIdentityMaterialV1 final
{
    std::string_view domain;
    std::uint32_t schemaVersion;
    std::span<const std::byte> canonicalPayload;
};

[[nodiscard]] Digest256V1 ComputeCanonicalDigestV1(const CanonicalIdentityMaterialV1& material);

template<class Identity>
[[nodiscard]] Identity MakeCanonicalIdentityV1(const CanonicalIdentityMaterialV1& material)
{
    return Identity::FromDigest(ComputeCanonicalDigestV1(material));
}

struct SemanticDescriptorV1 final
{
    SemanticIdentity identity;
    SemanticVersion version;
};

struct DynamicExecutionDimensionsV1 final
{
    std::optional<ActiveCount> activeCount;
    std::optional<TransitionCount> transitionCount;
    std::optional<ReuseCount> reuseCount;
    std::optional<UpdateInterval> updateInterval;
    std::optional<BatchSize> batchSize;
    std::optional<AffectedBlockCount> affectedBlockCount;
};

struct InvocationDescriptorV1 final
{
    InvocationIdentity identity;
    SemanticIdentity semanticIdentity;
    TimelineOrdinal timelineOrdinal;
    DeviceEpoch deviceEpoch;
    DynamicExecutionDimensionsV1 dimensions;
};

struct RawCandidateProposalV1 final
{
    CandidateIdentity identity;
    SemanticIdentity semanticIdentity;
    InvocationIdentity invocationIdentity;
    TargetProfileIdentity targetProfileIdentity;
    ResourceContractIdentity resourceContractIdentity;
    WriteSetIdentity proposedWriteSetIdentity;
};

class VerifiedPlanConstructionAccessV1;

class OpaqueVerifiedPlanV1 final
{
public:
    OpaqueVerifiedPlanV1(const OpaqueVerifiedPlanV1&) = default;
    OpaqueVerifiedPlanV1& operator=(const OpaqueVerifiedPlanV1&) = default;

    [[nodiscard]] constexpr const VerifiedPlanIdentity& Identity() const noexcept { return identity_; }

private:
    friend class VerifiedPlanConstructionAccessV1;
    explicit constexpr OpaqueVerifiedPlanV1(VerifiedPlanIdentity identity) noexcept
        : identity_(std::move(identity)) {}

    VerifiedPlanIdentity identity_;
};

struct FrozenArtifactDescriptorV1 final
{
    FrozenArtifactIdentity identity;
    VerifiedPlanIdentity verifiedPlanIdentity;
    TargetProfileIdentity targetProfileIdentity;
    ResourceContractIdentity resourceContractIdentity;
};

enum class HistoryStateV1 : std::uint8_t
{
    Invalid = 0,
    Valid = 1
};

struct HistoryValidityDescriptorV1 final
{
    HistoryValidityIdentity identity;
    SemanticIdentity semanticIdentity;
    InvocationIdentity sourceInvocationIdentity;
    HistoryGeneration generation;
    DeviceEpoch deviceEpoch;
    HistoryStateV1 state;
};

template<class Tag>
class EpochBoundHandleV1 final
{
public:
    constexpr EpochBoundHandleV1(ResourceInstanceIdentity resourceInstanceIdentity, DeviceEpoch deviceEpoch) noexcept
        : resourceInstanceIdentity_(std::move(resourceInstanceIdentity)), deviceEpoch_(deviceEpoch) {}

    [[nodiscard]] constexpr const ResourceInstanceIdentity& ResourceInstance() const noexcept
    {
        return resourceInstanceIdentity_;
    }

    [[nodiscard]] constexpr DeviceEpoch Epoch() const noexcept { return deviceEpoch_; }
    auto operator<=>(const EpochBoundHandleV1&) const = default;

private:
    ResourceInstanceIdentity resourceInstanceIdentity_;
    DeviceEpoch deviceEpoch_;
};

struct RepresentationHandleTagV1;
struct HistoryHandleTagV1;
struct CompletionHandleTagV1;
struct SubmissionHandleTagV1;

using RepresentationHandleV1 = EpochBoundHandleV1<RepresentationHandleTagV1>;
using HistoryHandleV1 = EpochBoundHandleV1<HistoryHandleTagV1>;
using CompletionHandleV1 = EpochBoundHandleV1<CompletionHandleTagV1>;
using SubmissionHandleV1 = EpochBoundHandleV1<SubmissionHandleTagV1>;

inline constexpr std::uint32_t CanonicalVocabularySchemaVersionV1 = 1u;
}
