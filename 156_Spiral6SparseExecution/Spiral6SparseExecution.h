#pragma once

#include "../00_Foundation/Result.h"
#include "../00_Foundation/Sha256.h"
#include "../154_SparseWorkSetSemantic/SparseWorkSetSemantic.h"
#include "../155_Spiral6SparseContracts/Spiral6SparseContracts.h"

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
}
