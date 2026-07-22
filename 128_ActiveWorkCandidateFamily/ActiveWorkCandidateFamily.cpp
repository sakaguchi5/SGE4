#include "ActiveWorkCandidateFamily.h"

#include "../00_Foundation/BinaryIO.h"

#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace sge4_5::spiral4::family_candidate
{
namespace
{
base::Digest256 LiteralIdentity(std::string_view value)
{
    return base::Sha256(std::as_bytes(std::span(value.data(), value.size())));
}
}

base::Digest256 ProposedFamilyTargetProfileIdentityV1()
{
    return LiteralIdentity(
        "SGE4-5.Spiral4.TargetProfile.D3D12.CandidateFamily.WarpQualified.V1");
}

base::Digest256 ProposedFamilyActiveCountContractIdentityV1()
{
    return LiteralIdentity(
        "SGE4-5.Spiral4.ActiveCountContract.Prefix.Nmax4096.V1");
}

base::Digest256 ProposedFamilyObservationContractIdentityV1()
{
    return LiteralIdentity(
        "SGE4-5.Spiral4.Observation.ActiveReference.InactiveSentinel.V1");
}

base::Digest256 ProposedCommandSignatureContractIdentityV1(
    family_contract::CandidateFamilyKindV1 kind,
    std::uint32_t)
{
    using K = family_contract::CandidateFamilyKindV1;
    switch (kind)
    {
    case K::FixedMaximumGuarded:
        return LiteralIdentity(
            "SGE4-5.Spiral4.CommandSignature.None.FixedDirect.V1");
    case K::SingleExecuteIndirectDispatch:
        return LiteralIdentity(
            "SGE4-5.Spiral4.CommandSignature.Dispatch.Stride12.OneCommand.V1");
    case K::BatchedExecuteIndirectDispatch:
        return LiteralIdentity(
            "SGE4-5.Spiral4.CommandSignature.RootConstantDispatch.Stride16.V1");
    default:
        return {};
    }
}

base::Digest256 ProposedProducerProgramTemplateIdentityV1(
    family_contract::CandidateFamilyKindV1 kind,
    std::uint32_t)
{
    using K = family_contract::CandidateFamilyKindV1;
    switch (kind)
    {
    case K::FixedMaximumGuarded:
        return LiteralIdentity(
            "SGE4-5.Spiral4.Program.NoArgumentProducer.FixedMaximum.V1");
    case K::SingleExecuteIndirectDispatch:
        return LiteralIdentity(
            "SGE4-5.Spiral4.Program.ArgumentProducer.CeilNfOver64.V1");
    case K::BatchedExecuteIndirectDispatch:
        return LiteralIdentity(
            "SGE4-5.Spiral4.Program.BatchArgumentProducer.FixedSlots.V1");
    default:
        return {};
    }
}

base::Digest256 ProposedConsumerProgramTemplateIdentityV1()
{
    return LiteralIdentity(
        "SGE4-5.Spiral4.Program.DirectPgaConsumer.ActivePrefix.BaseWork.V1");
}

std::vector<std::byte>
SerializeRawCandidateFamilyV1(const RawCandidateFamilyV1& value)
{
    base::BinaryWriter writer;
    writer.WriteBytes(LiteralIdentity(
        "SGE4-5.Spiral4.RawCandidateFamily.V1"));
    writer.WriteBytes(value.semanticIdentity);
    writer.WriteBytes(value.targetProfileIdentity);
    writer.WriteBytes(value.activeCountContractIdentity);
    writer.WriteBytes(value.observationContractIdentity);
    writer.WriteBytes(value.commandSignatureContractIdentity);
    writer.WriteBytes(value.producerProgramTemplateIdentity);
    writer.WriteBytes(value.consumerProgramTemplateIdentity);
    writer.WriteBytes(value.proposedArtifactIdentity);
    writer.WriteU32(static_cast<std::uint32_t>(value.kind));
    writer.WriteU32(static_cast<std::uint32_t>(value.commandSignaturePolicy));
    writer.WriteU32(static_cast<std::uint32_t>(value.issuancePolicy));
    writer.WriteU32(value.batchSize);
    writer.WriteU32(value.maxBatchSlots);
    writer.WriteU32(value.maxWorkCount);
    writer.WriteU32(value.threadsPerGroup);
    writer.WriteU32(value.directDispatchX);
    writer.WriteU32(value.argumentStrideBytes);
    writer.WriteU32(value.argumentBufferBytes);
    writer.WriteU32(value.maxCommandCount);
    writer.WriteU32(value.producerDispatchX);
    return std::move(writer).Take();
}

base::Digest256 RawCandidateFamilyIdentityV1(
    const RawCandidateFamilyV1& value)
{
    return base::Sha256(SerializeRawCandidateFamilyV1(value));
}
}
