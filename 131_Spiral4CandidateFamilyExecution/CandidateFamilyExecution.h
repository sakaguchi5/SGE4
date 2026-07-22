#pragma once

#include "../00_Foundation/Result.h"
#include "../00_Foundation/Sha256.h"
#include "../120_ActiveWorkSemantic/ActiveWorkSemantic.h"
#include "../127_Spiral4CandidateFamilyContracts/CandidateFamilyContracts.h"
#include "../130_ActiveWorkCandidateFamilyVerifier/ActiveWorkCandidateFamilyVerifier.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

struct ID3D12Device;

namespace sge4_5::spiral4::family_execution
{
struct CandidateFamilyExecutionErrorV1 final
{
    std::string stage;
    std::string message;
    long hresult = 0;
};

class FrozenVerifiedCandidateFamilyV1 final
{
public:
    FrozenVerifiedCandidateFamilyV1() = delete;
    FrozenVerifiedCandidateFamilyV1(
        const FrozenVerifiedCandidateFamilyV1&) = default;
    FrozenVerifiedCandidateFamilyV1& operator=(
        const FrozenVerifiedCandidateFamilyV1&) = default;

    [[nodiscard]] const family_verification::VerifiedCandidateFamilyV1&
    Verified() const noexcept { return verified_; }

    [[nodiscard]] const family_contract::FrozenCandidateFamilyArtifactV1&
    Artifact() const noexcept { return artifact_; }

    [[nodiscard]] const base::Digest256&
    BindingIdentity() const noexcept { return bindingIdentity_; }

private:
    FrozenVerifiedCandidateFamilyV1(
        family_verification::VerifiedCandidateFamilyV1 verified,
        family_contract::FrozenCandidateFamilyArtifactV1 artifact,
        base::Digest256 bindingIdentity);

    family_verification::VerifiedCandidateFamilyV1 verified_;
    family_contract::FrozenCandidateFamilyArtifactV1 artifact_;
    base::Digest256 bindingIdentity_{};

    friend base::Result<FrozenVerifiedCandidateFamilyV1, std::string>
    FreezeVerifiedCandidateFamilyV1(
        const semantic::ActiveWorkSemanticV1&,
        const family_verification::CandidateFamilyVerificationContextV1&,
        const family_verification::VerifiedCandidateFamilyV1&);
};

struct CandidateFamilyCaseResultV1 final
{
    std::uint32_t activeWorkCount = 0;
    std::uint32_t issuedThreadGroupCount = 0;
    std::uint32_t activeMismatchCount = 0;
    std::uint32_t firstActiveMismatch = 0xFFFF'FFFFu;
    bool inactiveTailPreserved = false;
    std::uint32_t nonFiniteCount = 0;
    std::uint32_t homogeneousCoordinateMismatchCount = 0;
    std::uint32_t duplicateWorkCount = 0;
    std::uint32_t missingWorkCount = 0;
    std::uint32_t reorderedWorkCount = 0;
    std::uint32_t outOfRangeWorkCount = 0;
    float maxAbsoluteError = 0.0f;
    float maxRelativeError = 0.0f;
    std::uint32_t maxUlpError = 0;
    std::vector<family_contract::BatchDispatchRecordV1> observedBatchRecords;
    base::Digest256 outputDigest{};
};

struct CandidateFamilyRunResultV1 final
{
    family_contract::CandidateFamilyKindV1 kind =
        family_contract::CandidateFamilyKindV1::FixedMaximumGuarded;
    std::uint32_t batchSize = 0;
    std::string adapterDescription;
    base::Digest256 producerShaderBytecodeDigest{};
    base::Digest256 consumerShaderBytecodeDigest{};
    base::Digest256 verifiedBindingIdentity{};
    base::Digest256 actualExecutionIdentity{};
    std::vector<CandidateFamilyCaseResultV1> cases;
};

[[nodiscard]] base::Result<
    FrozenVerifiedCandidateFamilyV1,
    std::string>
FreezeVerifiedCandidateFamilyV1(
    const semantic::ActiveWorkSemanticV1& semantic,
    const family_verification::CandidateFamilyVerificationContextV1& context,
    const family_verification::VerifiedCandidateFamilyV1& verified);

[[nodiscard]] base::Result<
    std::vector<CandidateFamilyRunResultV1>,
    CandidateFamilyExecutionErrorV1>
RunVerifiedCandidateFamilyCorpusOnDeviceV1(
    ID3D12Device* device,
    std::string_view adapterDescription,
    const semantic::ActiveWorkSemanticV1& semantic,
    const family_verification::CandidateFamilyVerificationContextV1& context,
    std::span<const FrozenVerifiedCandidateFamilyV1> candidates,
    std::span<const std::uint32_t> activeCounts);

[[nodiscard]] base::Result<
    std::vector<CandidateFamilyRunResultV1>,
    CandidateFamilyExecutionErrorV1>
RunVerifiedCandidateFamilyCorpusOnWarpV1(
    const semantic::ActiveWorkSemanticV1& semantic,
    const family_verification::CandidateFamilyVerificationContextV1& context,
    std::span<const FrozenVerifiedCandidateFamilyV1> candidates,
    std::span<const std::uint32_t> activeCounts);

[[nodiscard]] std::vector<std::byte>
SerializeFrozenVerifiedCandidateFamilyV1(
    const FrozenVerifiedCandidateFamilyV1& value);
}
