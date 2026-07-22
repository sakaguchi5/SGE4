#include "Spiral4VerifiedExecution.h"

#include "../00_Foundation/BinaryIO.h"

#include <span>
#include <string_view>
#include <utility>

namespace sge4_5::spiral4::verified_execution
{
namespace
{
base::Digest256 LiteralIdentity(std::string_view value)
{
    return base::Sha256(std::as_bytes(std::span(value.data(), value.size())));
}

base::Digest256 BuildBindingIdentity(
    const verification::VerifiedActiveWorkLoweringV1& verified,
    const contract::FrozenSingleIndirectArtifactV1& artifact)
{
    base::BinaryWriter writer;
    writer.WriteBytes(LiteralIdentity(
        "SGE4-5.Spiral4.FrozenVerifiedSingleIndirectBinding.V1"));
    writer.WriteBytes(verified.Plan().planIdentity);
    writer.WriteBytes(verified.CertificateIdentity());
    writer.WriteBytes(verified.Plan().operation.operationIdentity);
    writer.WriteBytes(
        verified.Plan().operation.producerProgramTemplateIdentity);
    writer.WriteBytes(
        verified.Plan().operation.consumerProgramTemplateIdentity);
    writer.WriteBytes(artifact.ArtifactIdentity());
    writer.WriteBytes(artifact.SemanticIdentity());
    return base::Sha256(std::move(writer).Take());
}

base::Result<void, std::string> ValidateFrozen(
    const FrozenVerifiedSingleIndirectExecutionV1& frozen,
    const semantic::ActiveWorkSemanticV1& semantic,
    const verification::ActiveWorkVerificationContextV1& context)
{
    auto verified = verification::
        ValidateVerifiedActiveWorkLoweringContextV1(
            frozen.Verified(),
            semantic,
            context);
    if (!verified)
    {
        return verified;
    }

    auto artifactValidation =
        contract::ValidateSingleIndirectArtifactForExecutionV1(
            frozen.Artifact(),
            semantic);
    if (!artifactValidation)
    {
        return base::Result<void, std::string>::Failure(
            artifactValidation.Error().stage + ": " +
            artifactValidation.Error().message);
    }

    const auto& operation = frozen.Verified().Plan().operation;
    if (operation.artifactIdentity !=
            frozen.Artifact().ArtifactIdentity() ||
        operation.maxWorkCount !=
            frozen.Artifact().MaxWorkCount() ||
        operation.threadsPerGroup !=
            frozen.Artifact().ThreadsPerGroup() ||
        operation.argumentStrideBytes !=
            frozen.Artifact().ArgumentStrideBytes() ||
        operation.argumentOffsetBytes !=
            frozen.Artifact().ArgumentOffsetBytes() ||
        operation.maxCommandCount !=
            frozen.Artifact().MaxCommandCount() ||
        operation.producerDispatchX !=
            frozen.Artifact().ProducerDispatchX() ||
        operation.producerDispatchY !=
            frozen.Artifact().ProducerDispatchY() ||
        operation.producerDispatchZ !=
            frozen.Artifact().ProducerDispatchZ() ||
        operation.commandKind !=
            frozen.Artifact().CommandKind() ||
        operation.operationCode !=
            frozen.Artifact().OperationCode())
    {
        return base::Result<void, std::string>::Failure(
            "verified plan is not bound to the actual indirect artifact");
    }

    const auto expectedBinding =
        BuildBindingIdentity(frozen.Verified(), frozen.Artifact());
    if (expectedBinding != frozen.BindingIdentity())
    {
        return base::Result<void, std::string>::Failure(
            "frozen verified execution binding identity mismatch");
    }

    return base::Result<void, std::string>::Success();
}
}

FrozenVerifiedSingleIndirectExecutionV1::
FrozenVerifiedSingleIndirectExecutionV1(
    verification::VerifiedActiveWorkLoweringV1 verified,
    contract::FrozenSingleIndirectArtifactV1 artifact,
    base::Digest256 bindingIdentity)
    : verified_(std::move(verified)),
      artifact_(std::move(artifact)),
      bindingIdentity_(bindingIdentity)
{
}

base::Result<FrozenVerifiedSingleIndirectExecutionV1, std::string>
FreezeVerifiedSingleIndirectExecutionV1(
    const semantic::ActiveWorkSemanticV1& semantic,
    const verification::ActiveWorkVerificationContextV1& context,
    const verification::VerifiedActiveWorkLoweringV1& verified)
{
    auto verifiedContext =
        verification::ValidateVerifiedActiveWorkLoweringContextV1(
            verified,
            semantic,
            context);
    if (!verifiedContext)
    {
        return base::Result<
            FrozenVerifiedSingleIndirectExecutionV1,
            std::string>::Failure(verifiedContext.Error());
    }

    auto artifact =
        contract::BuildCu2SingleIndirectArtifactV1(semantic);
    const auto binding =
        BuildBindingIdentity(verified, artifact);

    FrozenVerifiedSingleIndirectExecutionV1 frozen(
        verified,
        std::move(artifact),
        binding);

    auto validation = ValidateFrozen(frozen, semantic, context);
    if (!validation)
    {
        return base::Result<
            FrozenVerifiedSingleIndirectExecutionV1,
            std::string>::Failure(validation.Error());
    }

    return base::Result<
        FrozenVerifiedSingleIndirectExecutionV1,
        std::string>::Success(std::move(frozen));
}

base::Result<
    VerifiedSingleIndirectRunResultV1,
    execution::IndirectExecutionErrorV1>
RunVerifiedSingleIndirectOnWarpV1(
    const FrozenVerifiedSingleIndirectExecutionV1& frozen,
    const semantic::ActiveWorkSemanticV1& semantic,
    const verification::ActiveWorkVerificationContextV1& context,
    std::span<const std::uint32_t> activeCounts)
{
    const auto validation =
        ValidateFrozen(frozen, semantic, context);
    if (!validation)
    {
        return base::Result<
            VerifiedSingleIndirectRunResultV1,
            execution::IndirectExecutionErrorV1>::Failure(
                execution::IndirectExecutionErrorV1{
                    "authority/context",
                    validation.Error(),
                    0});
    }

    auto run = execution::RunSingleIndirectArchitectureOnWarpV1(
        semantic,
        frozen.Artifact(),
        activeCounts);
    if (!run)
    {
        return base::Result<
            VerifiedSingleIndirectRunResultV1,
            execution::IndirectExecutionErrorV1>::Failure(
                run.Error());
    }

    VerifiedSingleIndirectRunResultV1 result;
    result.architecture = std::move(run.Value());
    result.verifiedBindingIdentity = frozen.BindingIdentity();

    base::BinaryWriter writer;
    writer.WriteBytes(LiteralIdentity(
        "SGE4-5.Spiral4.ActualVerifiedSingleIndirectExecution.V1"));
    writer.WriteBytes(result.verifiedBindingIdentity);
    writer.WriteBytes(frozen.Artifact().ArtifactIdentity());
    writer.WriteBytes(
        result.architecture.producerShaderBytecodeDigest);
    writer.WriteBytes(
        result.architecture.consumerShaderBytecodeDigest);
    result.actualExecutionIdentity =
        base::Sha256(std::move(writer).Take());

    return base::Result<
        VerifiedSingleIndirectRunResultV1,
        execution::IndirectExecutionErrorV1>::Success(
            std::move(result));
}

std::vector<std::byte>
SerializeFrozenVerifiedSingleIndirectExecutionV1(
    const FrozenVerifiedSingleIndirectExecutionV1& value)
{
    base::BinaryWriter writer;
    writer.WriteBytes(LiteralIdentity(
        "SGE4-5.Spiral4.FrozenVerifiedSingleIndirectExecution.V1"));

    const auto verifiedBytes =
        verification::SerializeVerifiedActiveWorkLoweringV1(
            value.verified_);
    writer.WriteU32(
        static_cast<std::uint32_t>(verifiedBytes.size()));
    writer.WriteBytes(verifiedBytes);

    writer.WriteU32(
        static_cast<std::uint32_t>(
            value.artifact_.Bytes().size()));
    writer.WriteBytes(value.artifact_.Bytes());
    writer.WriteBytes(value.bindingIdentity_);
    return std::move(writer).Take();
}
}
