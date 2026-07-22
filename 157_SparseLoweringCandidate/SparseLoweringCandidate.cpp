#include "SparseLoweringCandidate.h"

#include "../00_Foundation/BinaryIO.h"

#include <span>
#include <string_view>
#include <utility>

namespace sge4_5::spiral6::candidate
{
namespace
{
base::Digest256 LiteralIdentity(std::string_view value)
{
    return base::Sha256(std::as_bytes(std::span(value.data(), value.size())));
}

void WriteDigest(base::BinaryWriter& writer, const base::Digest256& value)
{
    writer.WriteBytes(value);
}
}

base::Digest256 ProposedSparseTargetProfileIdentityV1()
{
    return LiteralIdentity(
        "SGE4-5.Spiral6.TargetProfile.D3D12.CompactIndexList.WarpQualified.V1");
}

base::Digest256 ProposedSparseObservationContractIdentityV1()
{
    return LiteralIdentity(
        "SGE4-5.Spiral6.Observation.ExactWriteSet.ActiveReference.InactiveFloat4Zero.V1");
}

base::Digest256 ProposedCompactIndexResourceContractIdentityV1()
{
    return LiteralIdentity(
        "SGE4-5.Spiral6.Resource.CompactSortedIndexList.Buffer16384.FixedTail.V1");
}

base::Digest256 ProposedSparseCompletionContractIdentityV1()
{
    return LiteralIdentity(
        "SGE4-5.Spiral6.Completion.ExecuteIndirectAuthorizesExactSparseWriteSet.V1");
}

base::Digest256 ProposedSparseDeviceEpochPolicyIdentityV1()
{
    return LiteralIdentity(
        "SGE4-5.Spiral6.DeviceEpoch.SparseDerivedResource.StaleRejected.RebuildRequired.V1");
}

base::Digest256 ProposedCompactIndexResourceIdentityV1(
    const contract::FrozenCompactIndexListArtifactV1& artifact)
{
    base::BinaryWriter writer;
    WriteDigest(writer, LiteralIdentity(
        "SGE4-5.Spiral6.ActualSparseResource.CompactIndexList.V1"));
    WriteDigest(writer, artifact.ArtifactIdentity());
    WriteDigest(writer, artifact.SparseSetIdentity());
    writer.WriteU32(static_cast<std::uint32_t>(artifact.RepresentationRole()));
    writer.WriteU32(artifact.ActiveCount());
    writer.WriteU32(artifact.IndexStrideBytes());
    writer.WriteU32(artifact.FixedCapacityBytes());
    return base::Sha256(writer.Bytes());
}

base::Digest256 SparseWriteSetAuthorityIdentityV1(
    const semantic::SparsePointTransformSemanticV1& semanticValue,
    const semantic::ExactSparseWorkSetV1& set,
    const contract::FrozenCompactIndexListArtifactV1& artifact)
{
    base::BinaryWriter writer;
    WriteDigest(writer, LiteralIdentity(
        "SGE4-5.Spiral6.WriteSetAuthority.ExactSparseSet.V1"));
    WriteDigest(writer, semantic::SparsePointTransformSemanticIdentityV1(semanticValue));
    WriteDigest(writer, set.Identity());
    WriteDigest(writer, artifact.ArtifactIdentity());
    writer.WriteU32(static_cast<std::uint32_t>(artifact.OutputInitialization()));
    writer.WriteU32(static_cast<std::uint32_t>(artifact.WriteSetPolicy()));
    writer.WriteU32(artifact.UniverseCount());
    writer.WriteU32(artifact.ActiveCount());
    return base::Sha256(writer.Bytes());
}

std::vector<std::byte> SerializeRawSparseLoweringCandidateV1(
    const RawSparseLoweringCandidateV1& value)
{
    base::BinaryWriter writer;
    WriteDigest(writer, LiteralIdentity(
        "SGE4-5.Spiral6.RawSparseLoweringCandidate.V1"));
    WriteDigest(writer, value.semanticIdentity);
    WriteDigest(writer, value.sparseSetIdentity);
    WriteDigest(writer, value.targetProfileIdentity);
    WriteDigest(writer, value.observationContractIdentity);
    WriteDigest(writer, value.indexResourceContractIdentity);
    WriteDigest(writer, value.completionContractIdentity);
    WriteDigest(writer, value.deviceEpochPolicyIdentity);
    WriteDigest(writer, value.spiral4IndirectArtifactIdentity);
    WriteDigest(writer, value.consumerProgramIdentity);
    WriteDigest(writer, value.proposedArtifactIdentity);
    WriteDigest(writer, value.proposedIndexResourceIdentity);
    WriteDigest(writer, value.writeSetAuthorityIdentity);
    writer.WriteU32(static_cast<std::uint32_t>(value.kind));
    writer.WriteU32(static_cast<std::uint32_t>(value.representationRole));
    writer.WriteU32(static_cast<std::uint32_t>(value.operationCode));
    writer.WriteU32(static_cast<std::uint32_t>(value.outputInitialization));
    writer.WriteU32(static_cast<std::uint32_t>(value.writeSetPolicy));
    writer.WriteU32(static_cast<std::uint32_t>(value.completionPolicy));
    writer.WriteU32(static_cast<std::uint32_t>(value.deviceEpochPolicy));
    writer.WriteU32(value.universeCount);
    writer.WriteU32(value.activeCount);
    writer.WriteU32(value.threadsPerGroup);
    writer.WriteU32(value.dispatchGroupsX);
    writer.WriteU32(value.indexStrideBytes);
    writer.WriteU32(value.fixedCapacityBytes);
    writer.WriteU32(value.outputCapacity);
    writer.WriteU32(value.outputStrideBytes);
    writer.WriteU32(value.unusedIndexSentinel);
    writer.WriteU32(value.zeroActiveMapsToZeroDispatch);
    return std::move(writer).Take();
}

base::Digest256 RawSparseLoweringCandidateIdentityV1(
    const RawSparseLoweringCandidateV1& value)
{
    return base::Sha256(SerializeRawSparseLoweringCandidateV1(value));
}
}
