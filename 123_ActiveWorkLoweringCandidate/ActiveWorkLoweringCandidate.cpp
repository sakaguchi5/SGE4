#include "ActiveWorkLoweringCandidate.h"

#include "../00_Foundation/BinaryIO.h"

#include <span>
#include <string_view>
#include <utility>

namespace sge4_5::spiral4::candidate
{
namespace
{
base::Digest256 LiteralIdentity(std::string_view value)
{
    return base::Sha256(std::as_bytes(std::span(value.data(), value.size())));
}
}

base::Digest256 ProposedTargetProfileIdentityV1()
{
    return LiteralIdentity(
        "SGE4-5.Spiral4.TargetProfile.D3D12.SingleIndirect.WarpQualified.V1");
}

base::Digest256 ProposedActiveCountContractIdentityV1()
{
    return LiteralIdentity(
        "SGE4-5.Spiral4.ActiveCountContract.Prefix.Nmax4096.V1");
}

base::Digest256 ProposedObservationContractIdentityV1()
{
    return LiteralIdentity(
        "SGE4-5.Spiral4.Observation.ActiveReference.InactiveSentinel.V1");
}

base::Digest256 ProposedCommandSignatureContractIdentityV1()
{
    return LiteralIdentity(
        "SGE4-5.Spiral4.CommandSignature.Dispatch.Stride12.OneCommand.V1");
}

base::Digest256 ProposedProducerProgramTemplateIdentityV1()
{
    return LiteralIdentity(
        "SGE4-5.Spiral4.Program.ArgumentProducer.CeilNfOver64.V1");
}

base::Digest256 ProposedConsumerProgramTemplateIdentityV1()
{
    return LiteralIdentity(
        "SGE4-5.Spiral4.Program.DirectPgaConsumer.ActivePrefix.V1");
}

std::vector<std::byte> SerializeRawActiveWorkLoweringCandidateV1(
    const RawActiveWorkLoweringCandidateV1& value)
{
    base::BinaryWriter writer;
    writer.WriteBytes(LiteralIdentity(
        "SGE4-5.Spiral4.RawActiveWorkLoweringCandidate.V1"));
    writer.WriteBytes(value.semanticIdentity);
    writer.WriteBytes(value.targetProfileIdentity);
    writer.WriteBytes(value.activeCountContractIdentity);
    writer.WriteBytes(value.observationContractIdentity);
    writer.WriteBytes(value.commandSignatureContractIdentity);
    writer.WriteBytes(value.producerProgramTemplateIdentity);
    writer.WriteBytes(value.consumerProgramTemplateIdentity);
    writer.WriteBytes(value.proposedArtifactIdentity);
    writer.WriteU32(static_cast<std::uint32_t>(value.kind));
    writer.WriteU32(static_cast<std::uint32_t>(value.commandKind));
    writer.WriteU32(static_cast<std::uint32_t>(value.operationCode));
    writer.WriteU32(static_cast<std::uint32_t>(value.activeRangePolicy));
    writer.WriteU32(static_cast<std::uint32_t>(value.statePolicy));
    writer.WriteU32(static_cast<std::uint32_t>(value.zeroWorkPolicy));
    writer.WriteU32(value.maxWorkCount);
    writer.WriteU32(value.threadsPerGroup);
    writer.WriteU32(value.workRecordStride);
    writer.WriteU32(value.outputRecordStride);
    writer.WriteU32(value.argumentStrideBytes);
    writer.WriteU32(value.argumentOffsetBytes);
    writer.WriteU32(value.maxCommandCount);
    writer.WriteU32(value.producerDispatchX);
    writer.WriteU32(value.producerDispatchY);
    writer.WriteU32(value.producerDispatchZ);
    return std::move(writer).Take();
}

base::Digest256 RawActiveWorkLoweringCandidateIdentityV1(
    const RawActiveWorkLoweringCandidateV1& value)
{
    const auto bytes = SerializeRawActiveWorkLoweringCandidateV1(value);
    return base::Sha256(bytes);
}
}
