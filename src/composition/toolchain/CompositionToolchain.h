#pragma once

#include "../../canonical/artifact/SectionedArtifact.h"
#include "../../canonical/base/SchemaValidation.h"
#include "../artifact/CompositionCertificate.h"
#include "../artifact/VerifiedCompositionArtifact.h"
#include "../artifact/abi2/FrozenCompositionAbi2.h"
#include "../model/CompositionContract.h"
#include "../model/DynamicExecutionContract.h"
#include "../verifier/CompositionVerifier.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace sge4::composition
{
namespace core = ::sge4::canonical;

class FrozenCompositionPackage final
{
public:
    FrozenCompositionPackage(FrozenCompositionPackage&&) noexcept = default;
    FrozenCompositionPackage& operator=(FrozenCompositionPackage&&) noexcept = default;
    FrozenCompositionPackage(const FrozenCompositionPackage&) = delete;
    FrozenCompositionPackage& operator=(const FrozenCompositionPackage&) = delete;

    [[nodiscard]] std::span<const std::byte> FileBytes() const noexcept { return outerBytes_; }
    [[nodiscard]] const artifact::VerifiedFrozenComposition& VerifiedComposition() const noexcept { return verifiedComposition_; }
    [[nodiscard]] const CompositionCertificate& Certificate() const noexcept { return certificate_; }
    [[nodiscard]] const core::SemanticIdentity& DynamicSemanticIdentity() const noexcept { return dynamicSemanticIdentity_; }
    [[nodiscard]] const DynamicContractV1& DynamicContract() const noexcept { return dynamicContract_; }
    [[nodiscard]] const Digest256& CompositionCoreDigest() const noexcept { return compositionCoreDigest_; }
    [[nodiscard]] const Digest256& SemanticDigest() const noexcept { return semanticDigest_; }
    [[nodiscard]] const Digest256& FileDigest() const noexcept { return fileDigest_; }

private:
    friend base::Expected<FrozenCompositionPackage, Error> ReadFrozenCompositionPackage(std::vector<std::byte>);

    FrozenCompositionPackage(
        std::vector<std::byte> outerBytes,
        artifact::VerifiedFrozenComposition verifiedComposition,
        CompositionCertificate certificate,
        core::SemanticIdentity dynamicSemanticIdentity,
        DynamicContractV1 dynamicContract,
        Digest256 compositionCoreDigest,
        Digest256 semanticDigest,
        Digest256 fileDigest)
        : outerBytes_(std::move(outerBytes)), verifiedComposition_(std::move(verifiedComposition)),
          certificate_(std::move(certificate)), dynamicSemanticIdentity_(std::move(dynamicSemanticIdentity)),
          dynamicContract_(dynamicContract), compositionCoreDigest_(compositionCoreDigest),
          semanticDigest_(semanticDigest), fileDigest_(fileDigest) {}

    std::vector<std::byte> outerBytes_;
    artifact::VerifiedFrozenComposition verifiedComposition_;
    CompositionCertificate certificate_;
    core::SemanticIdentity dynamicSemanticIdentity_;
    DynamicContractV1 dynamicContract_;
    Digest256 compositionCoreDigest_{};
    Digest256 semanticDigest_{};
    Digest256 fileDigest_{};
};

[[nodiscard]] base::Expected<FrozenCompositionPackage, Error> BuildFrozenCompositionPackage(
    ContractBuildInput input,
    DynamicContractV1 dynamicContract);

// Plannerを再実行せず、独立Verifierを通過済みのContract／PlanからABI 2.1をFreezeする。
[[nodiscard]] base::Expected<FrozenCompositionPackage, Error> FreezeVerifiedCompositionPackage(
    const ValidatedCompositionContract& contract,
    const verification::VerifiedCompositionPlan& verified,
    DynamicContractV1 dynamicContract);

[[nodiscard]] base::Expected<FrozenCompositionPackage, Error> ReadFrozenCompositionPackage(
    std::span<const std::byte> bytes);
[[nodiscard]] base::Expected<FrozenCompositionPackage, Error> ReadFrozenCompositionPackage(
    std::vector<std::byte> bytes);
}
