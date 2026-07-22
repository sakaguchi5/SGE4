#pragma once

#include "../00_Foundation/Result.h"
#include "../00_Foundation/Sha256.h"
#include "../154_SparseWorkSetSemantic/SparseWorkSetSemantic.h"
#include "../160_SparseCandidateFamily/SparseCandidateFamily.h"
#include "../162_SparseCandidateFamilyVerifier/SparseCandidateFamilyVerifier.h"

#include <cstddef>
#include <cstdint>
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
}
