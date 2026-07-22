#pragma once

#include "../00_Foundation/Result.h"
#include "../00_Foundation/Sha256.h"
#include "../154_SparseWorkSetSemantic/SparseWorkSetSemantic.h"
#include "../160_SparseCandidateFamily/SparseCandidateFamily.h"
#include "../162_SparseCandidateFamilyVerifier/SparseCandidateFamilyVerifier.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace sge4_5::spiral6::family_execution
{
struct SparseFamilyExecutionErrorV1 final
{
    std::string stage;
    std::string message;
    long nativeCode = 0;
};

struct SparseFamilyResourceBindingInputV1 final
{
    std::uint64_t deviceEpoch = 0;
    base::Digest256 actualRepresentationResourceIdentity{};
    family_candidate::SparseFamilyRepresentationRoleV1 representationRole =
        family_candidate::SparseFamilyRepresentationRoleV1::DenseMembershipMask4096;
    std::uint32_t fixedCapacityBytes = 0;
    std::uint32_t payloadStrideBytes = 0;
};

class FrozenVerifiedSparseFamilyCandidateV1 final
{
public:
    FrozenVerifiedSparseFamilyCandidateV1() = delete;
    FrozenVerifiedSparseFamilyCandidateV1(const FrozenVerifiedSparseFamilyCandidateV1&) = default;
    FrozenVerifiedSparseFamilyCandidateV1& operator=(const FrozenVerifiedSparseFamilyCandidateV1&) = default;
    [[nodiscard]] const family_verification::VerifiedSparseFamilyCandidateV1& Verified() const noexcept { return verified_; }
    [[nodiscard]] const family_candidate::FrozenSparseFamilyArtifactV1& Artifact() const noexcept { return artifact_; }
    [[nodiscard]] const SparseFamilyResourceBindingInputV1& ResourceBinding() const noexcept { return binding_; }
    [[nodiscard]] const base::Digest256& BindingIdentity() const noexcept { return bindingIdentity_; }
private:
    FrozenVerifiedSparseFamilyCandidateV1(
        family_verification::VerifiedSparseFamilyCandidateV1 verified,
        family_candidate::FrozenSparseFamilyArtifactV1 artifact,
        SparseFamilyResourceBindingInputV1 binding,
        base::Digest256 bindingIdentity);
    family_verification::VerifiedSparseFamilyCandidateV1 verified_;
    family_candidate::FrozenSparseFamilyArtifactV1 artifact_;
    SparseFamilyResourceBindingInputV1 binding_{};
    base::Digest256 bindingIdentity_{};
    friend base::Result<FrozenVerifiedSparseFamilyCandidateV1, std::string> FreezeVerifiedSparseFamilyCandidateV1(
        const semantic::SparsePointTransformSemanticV1&,
        const semantic::ExactSparseWorkSetV1&,
        const family_verification::SparseFamilyVerificationContextV1&,
        const family_verification::VerifiedSparseFamilyCandidateV1&,
        const family_candidate::FrozenSparseFamilyArtifactV1&,
        const SparseFamilyResourceBindingInputV1&);
};

struct SparseFamilyCandidateObservationV1 final
{
    family_candidate::SparseFamilyCandidateKindV1 kind = family_candidate::SparseFamilyCandidateKindV1::DenseMembershipMaskPredicate;
    std::uint32_t activeCount = 0;
    std::uint32_t activeBlockCount = 0;
    std::uint32_t expectedDispatchGroupsX = 0;
    std::uint32_t observedDispatchGroupsX = 0;
    std::uint32_t activeWriteCount = 0;
    bool activeReferenceQualified = false;
    bool inactiveSentinelByteStable = false;
    bool activeHomogeneousWQualified = false;
    bool finiteQualified = false;
    double maxAbsoluteError = 0.0;
    base::Digest256 artifactIdentity{};
    base::Digest256 bindingIdentity{};
    base::Digest256 outputDigest{};
};

struct SparseCandidateFamilyCaseResultV1 final
{
    std::string adapterDescription;
    semantic::SparsePatternKindV1 pattern = semantic::SparsePatternKindV1::PrefixControl;
    base::Digest256 semanticIdentity{};
    base::Digest256 sparseSetIdentity{};
    std::vector<SparseFamilyCandidateObservationV1> candidates;
    bool pairwiseOutputByteIdentical = false;
};

[[nodiscard]] SparseFamilyResourceBindingInputV1 BuildCanonicalSparseFamilyResourceBindingInputV1(
    const family_verification::VerifiedSparseFamilyCandidateV1& verified,
    std::uint64_t deviceEpoch);
[[nodiscard]] base::Result<FrozenVerifiedSparseFamilyCandidateV1, std::string> FreezeVerifiedSparseFamilyCandidateV1(
    const semantic::SparsePointTransformSemanticV1& semantic,
    const semantic::ExactSparseWorkSetV1& set,
    const family_verification::SparseFamilyVerificationContextV1& context,
    const family_verification::VerifiedSparseFamilyCandidateV1& verified,
    const family_candidate::FrozenSparseFamilyArtifactV1& artifact,
    const SparseFamilyResourceBindingInputV1& binding);
[[nodiscard]] base::Result<void, std::string> ValidateFrozenVerifiedSparseFamilyCandidateV1(
    const FrozenVerifiedSparseFamilyCandidateV1& frozen,
    const semantic::SparsePointTransformSemanticV1& semantic,
    const semantic::ExactSparseWorkSetV1& set,
    const family_verification::SparseFamilyVerificationContextV1& context,
    std::uint64_t currentDeviceEpoch);
[[nodiscard]] base::Result<SparseCandidateFamilyCaseResultV1, SparseFamilyExecutionErrorV1>
RunVerifiedSparseCandidateFamilyOnWarpV1(
    const semantic::SparsePointTransformSemanticV1& semantic,
    semantic::SparsePatternKindV1 pattern,
    const semantic::ExactSparseWorkSetV1& set,
    const family_verification::SparseFamilyVerificationContextV1& context,
    const std::vector<FrozenVerifiedSparseFamilyCandidateV1>& frozen,
    std::uint64_t currentDeviceEpoch);

[[nodiscard]] std::vector<std::byte> SerializeFrozenVerifiedSparseFamilyCandidateV1(
    const FrozenVerifiedSparseFamilyCandidateV1& value);
[[nodiscard]] std::vector<std::byte> SerializeSparseCandidateFamilyCaseResultV1(
    const SparseCandidateFamilyCaseResultV1& value);

enum class SparseCandidateFamilyRuntimeStateV1 : std::uint8_t
{
    Active = 0,
    AwaitingAdapter = 1
};

enum class SparseCandidateFamilyRecoveryModeV1 : std::uint8_t
{
    ControlledRebuild = 0,
    ForceRemovalForTest = 1,
    RetryAdapterReacquisition = 2
};

struct SparseSetCandidateAuthorityV1 final
{
    semantic::SparsePatternKindV1 pattern;
    semantic::ExactSparseWorkSetV1 set;
    std::vector<family_verification::VerifiedSparseFamilyCandidateV1> candidates;

    SparseSetCandidateAuthorityV1(
        semantic::SparsePatternKindV1 patternValue,
        semantic::ExactSparseWorkSetV1 setValue,
        std::vector<family_verification::VerifiedSparseFamilyCandidateV1> candidateValues)
        : pattern(patternValue), set(std::move(setValue)), candidates(std::move(candidateValues)) {}
};

struct SparseCandidateFamilyRuntimeErrorV1 final
{
    std::string stage;
    std::string message;
    long hresult = 0;
};

struct SparseRuntimeEpochHandleV1 final
{
    std::uint64_t deviceEpoch = 0;
    base::Digest256 domainIdentity{};
    base::Digest256 candidateBindingSetIdentity{};
};

struct SparseRuntimeSetBindingV1 final
{
    std::uint64_t deviceEpoch = 0;
    semantic::SparsePatternKindV1 pattern = semantic::SparsePatternKindV1::PrefixControl;
    base::Digest256 sparseSetIdentity{};
    base::Digest256 bindingIdentity{};
};

struct SparseRepresentationSetHandleV1 final
{
    std::uint64_t deviceEpoch = 0;
    semantic::SparsePatternKindV1 pattern = semantic::SparsePatternKindV1::PrefixControl;
    base::Digest256 sparseSetIdentity{};
    base::Digest256 representationSetIdentity{};
};

struct SparseRuntimeSubmissionTokenV1 final
{
    std::uint64_t deviceEpoch = 0;
    std::uint64_t submissionOrdinal = 0;
    base::Digest256 tokenIdentity{};
};

struct SparseRuntimeSubmissionV1 final
{
    SparseRuntimeSubmissionTokenV1 completion;
    SparseRepresentationSetHandleV1 representations;
    bool representationResourcesMaterialized = false;
    bool indirectArgumentsMaterialized = false;
    bool completionStateMaterialized = false;
    bool readbackStateMaterialized = false;
    SparseCandidateFamilyCaseResultV1 architecture;
};

struct SparseCandidateFamilyRecoveryReportV1 final
{
    SparseCandidateFamilyRecoveryModeV1 mode = SparseCandidateFamilyRecoveryModeV1::ControlledRebuild;
    SparseCandidateFamilyRuntimeStateV1 stateBefore = SparseCandidateFamilyRuntimeStateV1::Active;
    SparseCandidateFamilyRuntimeStateV1 stateAfter = SparseCandidateFamilyRuntimeStateV1::Active;
    std::uint64_t previousDeviceEpoch = 0;
    std::uint64_t newDeviceEpoch = 0;
    long removalReason = 0;
    std::uint32_t removedAdapterLuidLow = 0;
    std::int32_t removedAdapterLuidHigh = 0;
    bool forcedRemoval = false;
    bool adapterReacquired = false;
    bool nativeDeviceRematerialized = false;
    bool allRuntimeObjectsReleased = false;
    bool executionContextRematerialized = false;
    bool representationResourcesInvalidated = false;
    bool indirectArgumentsInvalidated = false;
    bool completionStateInvalidated = false;
    bool readbackStateInvalidated = false;
    bool sparseRebindRequired = false;
    bool explicitRepresentationRebuildRequired = false;
    bool frozenAuthorityPreserved = false;
};

class LoadedSparseCandidateFamilyRuntimeV1 final
{
public:
    LoadedSparseCandidateFamilyRuntimeV1(LoadedSparseCandidateFamilyRuntimeV1&&) noexcept;
    LoadedSparseCandidateFamilyRuntimeV1& operator=(LoadedSparseCandidateFamilyRuntimeV1&&) noexcept;
    ~LoadedSparseCandidateFamilyRuntimeV1();
    LoadedSparseCandidateFamilyRuntimeV1(const LoadedSparseCandidateFamilyRuntimeV1&) = delete;
    LoadedSparseCandidateFamilyRuntimeV1& operator=(const LoadedSparseCandidateFamilyRuntimeV1&) = delete;

    [[nodiscard]] SparseCandidateFamilyRuntimeStateV1 State() const noexcept;
    [[nodiscard]] std::uint64_t DeviceEpoch() const noexcept;
    [[nodiscard]] const base::Digest256& DomainIdentity() const noexcept;
    [[nodiscard]] const base::Digest256& CandidateBindingSetIdentity() const noexcept;
    [[nodiscard]] const std::string& AdapterDescription() const noexcept;
    [[nodiscard]] std::size_t AuthorityCount() const noexcept;
    [[nodiscard]] bool SparseSetBound() const noexcept;
    [[nodiscard]] bool RepresentationsBuilt() const noexcept;

private:
    struct Impl;
    explicit LoadedSparseCandidateFamilyRuntimeV1(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;

    friend base::Result<LoadedSparseCandidateFamilyRuntimeV1, SparseCandidateFamilyRuntimeErrorV1>
    LoadSparseCandidateFamilyRuntimeOnWarpV1(
        const semantic::SparsePointTransformSemanticV1&,
        const family_verification::SparseFamilyVerificationContextV1&,
        std::span<const SparseSetCandidateAuthorityV1>);
    friend SparseRuntimeEpochHandleV1 CaptureSparseRuntimeEpochHandleV1(
        const LoadedSparseCandidateFamilyRuntimeV1&);
    friend base::Result<SparseRuntimeSetBindingV1, SparseCandidateFamilyRuntimeErrorV1>
    BindSparseWorkSetV1(
        LoadedSparseCandidateFamilyRuntimeV1&,
        const SparseRuntimeEpochHandleV1&,
        semantic::SparsePatternKindV1,
        const base::Digest256&);
    friend base::Result<SparseRepresentationSetHandleV1, SparseCandidateFamilyRuntimeErrorV1>
    RebuildSparseRepresentationsV1(
        LoadedSparseCandidateFamilyRuntimeV1&,
        const SparseRuntimeEpochHandleV1&,
        const SparseRuntimeSetBindingV1&);
    friend base::Result<SparseRuntimeSubmissionV1, SparseCandidateFamilyRuntimeErrorV1>
    SubmitSparseWorkSetV1(
        LoadedSparseCandidateFamilyRuntimeV1&,
        const SparseRuntimeEpochHandleV1&,
        const SparseRuntimeSetBindingV1&,
        const SparseRepresentationSetHandleV1&);
    friend base::Result<void, SparseCandidateFamilyRuntimeErrorV1>
    ValidateSparseRuntimeEpochHandleV1(
        const LoadedSparseCandidateFamilyRuntimeV1&,
        const SparseRuntimeEpochHandleV1&);
    friend base::Result<void, SparseCandidateFamilyRuntimeErrorV1>
    ValidateSparseRuntimeSubmissionTokenV1(
        const LoadedSparseCandidateFamilyRuntimeV1&,
        const SparseRuntimeSubmissionTokenV1&);
    friend base::Result<void, SparseCandidateFamilyRuntimeErrorV1>
    ValidateSparseRepresentationSetHandleV1(
        const LoadedSparseCandidateFamilyRuntimeV1&,
        const SparseRepresentationSetHandleV1&);
    friend base::Result<SparseCandidateFamilyRecoveryReportV1, SparseCandidateFamilyRuntimeErrorV1>
    RecoverSparseCandidateFamilyRuntimeV1(
        LoadedSparseCandidateFamilyRuntimeV1&,
        SparseCandidateFamilyRecoveryModeV1);
    friend std::vector<std::byte> SerializeSparseCandidateFamilyRuntimeAuthorityV1(
        const LoadedSparseCandidateFamilyRuntimeV1&);
};

[[nodiscard]] base::Result<LoadedSparseCandidateFamilyRuntimeV1, SparseCandidateFamilyRuntimeErrorV1>
LoadSparseCandidateFamilyRuntimeOnWarpV1(
    const semantic::SparsePointTransformSemanticV1& semantic,
    const family_verification::SparseFamilyVerificationContextV1& context,
    std::span<const SparseSetCandidateAuthorityV1> authorities);
[[nodiscard]] SparseRuntimeEpochHandleV1 CaptureSparseRuntimeEpochHandleV1(
    const LoadedSparseCandidateFamilyRuntimeV1& runtime);
[[nodiscard]] base::Result<SparseRuntimeSetBindingV1, SparseCandidateFamilyRuntimeErrorV1>
BindSparseWorkSetV1(
    LoadedSparseCandidateFamilyRuntimeV1& runtime,
    const SparseRuntimeEpochHandleV1& handle,
    semantic::SparsePatternKindV1 pattern,
    const base::Digest256& sparseSetIdentity);
[[nodiscard]] base::Result<SparseRepresentationSetHandleV1, SparseCandidateFamilyRuntimeErrorV1>
RebuildSparseRepresentationsV1(
    LoadedSparseCandidateFamilyRuntimeV1& runtime,
    const SparseRuntimeEpochHandleV1& handle,
    const SparseRuntimeSetBindingV1& binding);
[[nodiscard]] base::Result<SparseRuntimeSubmissionV1, SparseCandidateFamilyRuntimeErrorV1>
SubmitSparseWorkSetV1(
    LoadedSparseCandidateFamilyRuntimeV1& runtime,
    const SparseRuntimeEpochHandleV1& handle,
    const SparseRuntimeSetBindingV1& binding,
    const SparseRepresentationSetHandleV1& representations);
[[nodiscard]] base::Result<void, SparseCandidateFamilyRuntimeErrorV1>
ValidateSparseRuntimeEpochHandleV1(
    const LoadedSparseCandidateFamilyRuntimeV1& runtime,
    const SparseRuntimeEpochHandleV1& handle);
[[nodiscard]] base::Result<void, SparseCandidateFamilyRuntimeErrorV1>
ValidateSparseRuntimeSubmissionTokenV1(
    const LoadedSparseCandidateFamilyRuntimeV1& runtime,
    const SparseRuntimeSubmissionTokenV1& token);
[[nodiscard]] base::Result<void, SparseCandidateFamilyRuntimeErrorV1>
ValidateSparseRepresentationSetHandleV1(
    const LoadedSparseCandidateFamilyRuntimeV1& runtime,
    const SparseRepresentationSetHandleV1& representations);
[[nodiscard]] base::Result<SparseCandidateFamilyRecoveryReportV1, SparseCandidateFamilyRuntimeErrorV1>
RecoverSparseCandidateFamilyRuntimeV1(
    LoadedSparseCandidateFamilyRuntimeV1& runtime,
    SparseCandidateFamilyRecoveryModeV1 mode);
[[nodiscard]] std::vector<std::byte> SerializeSparseCandidateFamilyRuntimeAuthorityV1(
    const LoadedSparseCandidateFamilyRuntimeV1& runtime);

}
