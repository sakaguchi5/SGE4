#pragma once

#include "../00_Foundation/Result.h"
#include "../00_Foundation/Sha256.h"
#include "../120_ActiveWorkSemantic/ActiveWorkSemantic.h"
#include "../121_Spiral4Contracts/Spiral4Contracts.h"
#include "../122_Spiral4IndirectExecution/Spiral4IndirectExecution.h"
#include "../125_ActiveWorkLoweringVerifier/ActiveWorkLoweringVerifier.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace sge4_5::spiral4::verified_execution
{
struct VerifiedSingleIndirectRunResultV1;

class FrozenVerifiedSingleIndirectExecutionV1 final
{
public:
    FrozenVerifiedSingleIndirectExecutionV1() = delete;
    FrozenVerifiedSingleIndirectExecutionV1(
        const FrozenVerifiedSingleIndirectExecutionV1&) = default;
    FrozenVerifiedSingleIndirectExecutionV1& operator=(
        const FrozenVerifiedSingleIndirectExecutionV1&) = default;

    [[nodiscard]] const base::Digest256&
    BindingIdentity() const noexcept
    {
        return bindingIdentity_;
    }

    [[nodiscard]] const base::Digest256&
    PlanIdentity() const noexcept
    {
        return verified_.Plan().planIdentity;
    }

    [[nodiscard]] const base::Digest256&
    CertificateIdentity() const noexcept
    {
        return verified_.CertificateIdentity();
    }

    [[nodiscard]] const base::Digest256&
    ArtifactIdentity() const noexcept
    {
        return artifact_.ArtifactIdentity();
    }

    [[nodiscard]] const verification::VerifiedActiveWorkLoweringV1&
    Verified() const noexcept
    {
        return verified_;
    }

    [[nodiscard]] const contract::FrozenSingleIndirectArtifactV1&
    Artifact() const noexcept
    {
        return artifact_;
    }

private:
    FrozenVerifiedSingleIndirectExecutionV1(
        verification::VerifiedActiveWorkLoweringV1 verified,
        contract::FrozenSingleIndirectArtifactV1 artifact,
        base::Digest256 bindingIdentity);

    verification::VerifiedActiveWorkLoweringV1 verified_;
    contract::FrozenSingleIndirectArtifactV1 artifact_;
    base::Digest256 bindingIdentity_{};

    friend base::Result<
        FrozenVerifiedSingleIndirectExecutionV1,
        std::string>
    FreezeVerifiedSingleIndirectExecutionV1(
        const semantic::ActiveWorkSemanticV1&,
        const verification::ActiveWorkVerificationContextV1&,
        const verification::VerifiedActiveWorkLoweringV1&);

    friend base::Result<
        VerifiedSingleIndirectRunResultV1,
        execution::IndirectExecutionErrorV1>
    RunVerifiedSingleIndirectOnWarpV1(
        const FrozenVerifiedSingleIndirectExecutionV1&,
        const semantic::ActiveWorkSemanticV1&,
        const verification::ActiveWorkVerificationContextV1&,
        std::span<const std::uint32_t>);

    friend std::vector<std::byte>
    SerializeFrozenVerifiedSingleIndirectExecutionV1(
        const FrozenVerifiedSingleIndirectExecutionV1&);
};

struct VerifiedSingleIndirectRunResultV1 final
{
    execution::SingleIndirectArchitectureResultV1 architecture;
    base::Digest256 verifiedBindingIdentity{};
    base::Digest256 actualExecutionIdentity{};
};

[[nodiscard]] base::Result<
    FrozenVerifiedSingleIndirectExecutionV1,
    std::string>
FreezeVerifiedSingleIndirectExecutionV1(
    const semantic::ActiveWorkSemanticV1& semantic,
    const verification::ActiveWorkVerificationContextV1& context,
    const verification::VerifiedActiveWorkLoweringV1& verified);

[[nodiscard]] base::Result<
    VerifiedSingleIndirectRunResultV1,
    execution::IndirectExecutionErrorV1>
RunVerifiedSingleIndirectOnWarpV1(
    const FrozenVerifiedSingleIndirectExecutionV1& frozen,
    const semantic::ActiveWorkSemanticV1& semantic,
    const verification::ActiveWorkVerificationContextV1& context,
    std::span<const std::uint32_t> activeCounts);

[[nodiscard]] std::vector<std::byte>
SerializeFrozenVerifiedSingleIndirectExecutionV1(
    const FrozenVerifiedSingleIndirectExecutionV1& value);
}
