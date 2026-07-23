#pragma once

#include "../00_Foundation/Result.h"
#include "../00_Foundation/Sha256.h"
#include "../172_SparseTemporalDeltaSemantic/SparseTemporalDeltaSemantic.h"
#include "../178_SparseTemporalDeltaCandidateFamily/DeltaCandidateFamily.h"
#include "../180_SparseTemporalDeltaCandidateFamilyVerifier/DeltaCandidateFamilyVerifier.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace sge4_5::spiral7::family_execution
{
struct DeltaFamilyExecutionErrorV1 final
{
    std::string stage;
    std::string message;
    long nativeCode = 0;
};

struct VerifiedDeltaFamilyCaseV1 final
{
    std::string id;
    semantic::SparseDeltaInvocationV1 invocation;
    std::vector<family_candidate::FrozenDeltaFamilyArtifactV1> artifacts;
    std::vector<family_verification::VerifiedDeltaFamilyCandidateV1> verifiedCandidates;

    VerifiedDeltaFamilyCaseV1(
        std::string idValue,
        semantic::SparseDeltaInvocationV1 invocationValue,
        std::vector<family_candidate::FrozenDeltaFamilyArtifactV1> artifactsValue,
        std::vector<family_verification::VerifiedDeltaFamilyCandidateV1> verifiedCandidatesValue)
        : id(std::move(idValue)),
          invocation(std::move(invocationValue)),
          artifacts(std::move(artifactsValue)),
          verifiedCandidates(std::move(verifiedCandidatesValue)) {}
};

struct DeltaFamilyCandidateObservationV1 final
{
    family_candidate::DeltaFamilyCandidateKindV1 kind =
        family_candidate::DeltaFamilyCandidateKindV1::FullActiveDenseRecompute;
    std::uint32_t dispatchGroupsX = 0;
    std::uint32_t legalWriteCount = 0;
    std::uint32_t observedWriteCount = 0;
    bool activeReferenceQualified = false;
    bool inactiveSentinelByteStable = false;
    bool retainedHistoryByteStable = false;
    bool exactWriteSetQualified = false;
    bool homogeneousWQualified = false;
    bool finiteQualified = false;
    double maxAbsoluteError = 0.0;
    base::Digest256 artifactIdentity{};
    base::Digest256 certificateIdentity{};
    base::Digest256 outputDigest{};
};

struct DeltaFamilyInvocationObservationV1 final
{
    std::string id;
    std::uint32_t invocationIndex = 0;
    std::uint32_t activeCount = 0;
    std::uint32_t transitionCount = 0;
    std::uint32_t affectedBlockCount = 0;
    bool exactCandidateOrder = false;
    bool pairwiseOutputByteIdentical = false;
    std::vector<DeltaFamilyCandidateObservationV1> candidates;
};

struct DeltaFamilyArchitectureReportV1 final
{
    std::string adapterDescription;
    base::Digest256 semanticIdentity{};
    std::vector<DeltaFamilyInvocationObservationV1> invocations;
};

[[nodiscard]] base::Result<DeltaFamilyArchitectureReportV1, DeltaFamilyExecutionErrorV1>
ExecuteVerifiedDeltaCandidateFamilyV1(
    const semantic::SparseTemporalDeltaSemanticV1& semantic,
    const std::vector<VerifiedDeltaFamilyCaseV1>& cases);

[[nodiscard]] std::vector<std::byte> SerializeDeltaFamilyArchitectureReportV1(
    const DeltaFamilyArchitectureReportV1& report);
} // namespace sge4_5::spiral7::family_execution
