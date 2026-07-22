#pragma once

#include "../00_Foundation/Result.h"
#include "../00_Foundation/Sha256.h"
#include "../154_SparseWorkSetSemantic/SparseWorkSetSemantic.h"
#include "../155_Spiral6SparseContracts/Spiral6SparseContracts.h"
#include "../159_SparseLoweringVerifier/SparseLoweringVerifier.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace sge4_5::spiral6::execution
{
struct SparseExecutionErrorV1 final
{
    std::string stage;
    std::string message;
    long nativeCode = 0;
};

struct CompactIndexArchitectureCaseV1 final
{
    semantic::SparsePatternKindV1 pattern = semantic::SparsePatternKindV1::PrefixControl;
    semantic::ExactSparseWorkSetV1 set;
    contract::FrozenCompactIndexListArtifactV1 artifact;

    CompactIndexArchitectureCaseV1(
        semantic::SparsePatternKindV1 patternValue,
        semantic::ExactSparseWorkSetV1 setValue,
        contract::FrozenCompactIndexListArtifactV1 artifactValue)
        : pattern(patternValue), set(std::move(setValue)), artifact(std::move(artifactValue)) {}
};

struct CompactIndexCaseObservationV1 final
{
    semantic::SparsePatternKindV1 pattern = semantic::SparsePatternKindV1::PrefixControl;
    std::uint32_t activeCount = 0;
    std::uint32_t expectedDispatchGroupsX = 0;
    std::uint32_t observedDispatchGroupsX = 0;
    std::uint32_t activeWriteCount = 0;
    bool activeReferenceQualified = false;
    bool inactiveSentinelByteStable = false;
    bool activeHomogeneousWQualified = false;
    bool finiteQualified = false;
    double maxAbsoluteError = 0.0;
    base::Digest256 sparseSetIdentity{};
    base::Digest256 artifactIdentity{};
    base::Digest256 outputDigest{};
};

struct CompactIndexArchitectureReportV1 final
{
    std::string adapterDescription;
    base::Digest256 semanticIdentity{};
    std::vector<CompactIndexCaseObservationV1> cases;
};

[[nodiscard]] base::Result<CompactIndexArchitectureReportV1, SparseExecutionErrorV1>
ExecuteCompactIndexListArchitectureV1(
    const semantic::SparsePointTransformSemanticV1& semanticValue,
    const std::vector<CompactIndexArchitectureCaseV1>& cases);

[[nodiscard]] std::vector<std::byte> SerializeCompactIndexArchitectureReportV1(
    const CompactIndexArchitectureReportV1& report);

struct SparseIndexResourceBindingInputV1 final
{
    std::uint64_t deviceEpoch = 0;
    base::Digest256 actualIndexResourceIdentity{};
    contract::SparseRepresentationRoleV1 representationRole =
        contract::SparseRepresentationRoleV1::CompactSortedUniverseIndexList;
    std::uint32_t fixedCapacityBytes = 0;
    std::uint32_t indexStrideBytes = 0;
};

class FrozenVerifiedCompactIndexExecutionV1 final
{
public:
    FrozenVerifiedCompactIndexExecutionV1() = delete;
    FrozenVerifiedCompactIndexExecutionV1(
        const FrozenVerifiedCompactIndexExecutionV1&) = default;
    FrozenVerifiedCompactIndexExecutionV1& operator=(
        const FrozenVerifiedCompactIndexExecutionV1&) = default;

    [[nodiscard]] const verification::VerifiedSparseLoweringV1& Verified() const noexcept
    {
        return verified_;
    }
    [[nodiscard]] const contract::FrozenCompactIndexListArtifactV1& Artifact() const noexcept
    {
        return artifact_;
    }
    [[nodiscard]] const SparseIndexResourceBindingInputV1& ResourceBinding() const noexcept
    {
        return binding_;
    }
    [[nodiscard]] const base::Digest256& BindingIdentity() const noexcept
    {
        return bindingIdentity_;
    }

private:
    FrozenVerifiedCompactIndexExecutionV1(
        verification::VerifiedSparseLoweringV1 verified,
        contract::FrozenCompactIndexListArtifactV1 artifact,
        SparseIndexResourceBindingInputV1 binding,
        base::Digest256 bindingIdentity);

    verification::VerifiedSparseLoweringV1 verified_;
    contract::FrozenCompactIndexListArtifactV1 artifact_;
    SparseIndexResourceBindingInputV1 binding_{};
    base::Digest256 bindingIdentity_{};

    friend base::Result<FrozenVerifiedCompactIndexExecutionV1, std::string>
    FreezeVerifiedCompactIndexExecutionV1(
        const verification::VerifiedSparseLoweringV1&,
        const semantic::SparsePointTransformSemanticV1&,
        const semantic::ExactSparseWorkSetV1&,
        const contract::FrozenCompactIndexListArtifactV1&,
        const SparseIndexResourceBindingInputV1&);
};

struct VerifiedCompactIndexExecutionReportV1 final
{
    std::uint64_t deviceEpoch = 0;
    base::Digest256 bindingIdentity{};
    CompactIndexArchitectureReportV1 architecture;
};

[[nodiscard]] base::Result<FrozenVerifiedCompactIndexExecutionV1, std::string>
FreezeVerifiedCompactIndexExecutionV1(
    const verification::VerifiedSparseLoweringV1& verified,
    const semantic::SparsePointTransformSemanticV1& semantic,
    const semantic::ExactSparseWorkSetV1& set,
    const contract::FrozenCompactIndexListArtifactV1& artifact,
    const SparseIndexResourceBindingInputV1& binding);

[[nodiscard]] base::Result<void, std::string>
ValidateFrozenCompactIndexExecutionEpochV1(
    const FrozenVerifiedCompactIndexExecutionV1& frozen,
    std::uint64_t currentDeviceEpoch);

[[nodiscard]] base::Result<VerifiedCompactIndexExecutionReportV1, SparseExecutionErrorV1>
ExecuteVerifiedCompactIndexListV1(
    const semantic::SparsePointTransformSemanticV1& semantic,
    const semantic::ExactSparseWorkSetV1& set,
    const FrozenVerifiedCompactIndexExecutionV1& frozen,
    std::uint64_t currentDeviceEpoch);

[[nodiscard]] std::vector<std::byte> SerializeFrozenVerifiedCompactIndexExecutionV1(
    const FrozenVerifiedCompactIndexExecutionV1& value);
[[nodiscard]] std::vector<std::byte> SerializeVerifiedCompactIndexExecutionReportV1(
    const VerifiedCompactIndexExecutionReportV1& value);
}
