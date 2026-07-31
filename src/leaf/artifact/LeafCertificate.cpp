#include "LeafCertificate.h"

#include "../../canonical/base/BinaryIO.h"
#include "../model/plan/ExecutionPlanModel.h"

namespace sge4::leaf
{
namespace
{
using base::BinaryWriter;

template<class Identity>
[[nodiscard]] Identity MakeIdentity(std::string_view domain, std::span<const std::byte> bytes)
{
    return Identity::FromDigest(ComputeDomainDigest(domain, 1, bytes));
}

[[nodiscard]] std::vector<std::byte> TargetPayload(const package::d3d12_v13::D3D12PackageView& view)
{
    const auto& profile = view.Profile();
    BinaryWriter writer;
    writer.WriteU32(profile.minimumFeatureLevel);
    writer.WriteU16(profile.shaderModelMajor);
    writer.WriteU16(profile.shaderModelMinor);
    writer.WriteU16(profile.rootSignatureMajor);
    writer.WriteU16(profile.rootSignatureMinor);
    writer.WriteU16(static_cast<std::uint16_t>(profile.barrierModel));
    writer.WriteU16(static_cast<std::uint16_t>(profile.shaderBinaryFormat));
    writer.WriteU64(profile.requiredFeatureBits0);
    writer.WriteU64(profile.requiredFeatureBits1);
    return std::move(writer).Take();
}

[[nodiscard]] std::vector<std::byte> ResourcePayload(const package::d3d12_v13::D3D12PackageView& view)
{
    BinaryWriter writer;
    writer.WriteCountU32(view.ExternalSlots().size());
    for (const auto& slot : view.ExternalSlots())
    {
        writer.WriteU32(slot.id.value);
        writer.WriteU32(slot.resource.value);
        writer.WriteU16(static_cast<std::uint16_t>(slot.requiredKind));
        writer.WriteU16(0);
        writer.WriteU32(static_cast<std::uint32_t>(slot.requiredFormat));
        writer.WriteU64(slot.minimumBytes);
        writer.WriteU16(static_cast<std::uint16_t>(slot.requiredIncomingState.stateClass));
        writer.WriteU16(slot.requiredIncomingState.reserved);
        writer.WriteU32(slot.requiredIncomingState.explicitBits);
        writer.WriteU16(static_cast<std::uint16_t>(slot.guaranteedOutgoingState.stateClass));
        writer.WriteU16(slot.guaranteedOutgoingState.reserved);
        writer.WriteU32(slot.guaranteedOutgoingState.explicitBits);
        writer.WriteU32(static_cast<std::uint32_t>(slot.synchronizationContract));
        writer.WriteU32(slot.flags);
    }
    writer.WriteCountU32(view.DynamicSlots().size());
    for (const auto& slot : view.DynamicSlots())
    {
        writer.WriteU32(slot.id.value);
        writer.WriteU32(slot.destinationResource.value);
        writer.WriteU64(slot.destinationOffset);
        writer.WriteU64(slot.requiredBytes);
        writer.WriteU32(slot.requiredAlignment);
        writer.WriteU32(slot.flags);
    }
    writer.WriteCountU32(view.SurfaceSlots().size());
    for (const auto& slot : view.SurfaceSlots())
    {
        writer.WriteU32(slot.id.value);
        writer.WriteU32(slot.imageResource.value);
        writer.WriteU32(static_cast<std::uint32_t>(slot.requiredFormat));
        writer.WriteU16(static_cast<std::uint16_t>(slot.acquiredState.stateClass));
        writer.WriteU16(slot.acquiredState.reserved);
        writer.WriteU32(slot.acquiredState.explicitBits);
        writer.WriteU16(static_cast<std::uint16_t>(slot.presentedState.stateClass));
        writer.WriteU16(slot.presentedState.reserved);
        writer.WriteU32(slot.presentedState.explicitBits);
        writer.WriteU32(slot.flags);
    }
    return std::move(writer).Take();
}

[[nodiscard]] std::vector<std::byte> WriteSetPayload(const package::d3d12_v13::D3D12PackageView& view)
{
    using namespace package::d3d12_v13;
    BinaryWriter writer;
    std::vector<std::uint32_t> writable;
    for (const auto& slot : view.ExternalSlots())
    {
        const auto& outgoing = slot.guaranteedOutgoingState;
        const bool explicitWrite = outgoing.stateClass == StateClass::Explicit &&
            (outgoing.explicitBits & (static_cast<std::uint32_t>(ExplicitStateBits::UnorderedWrite) |
                static_cast<std::uint32_t>(ExplicitStateBits::CopyDestination) |
                static_cast<std::uint32_t>(ExplicitStateBits::RenderTarget))) != 0;
        if (explicitWrite || outgoing != slot.requiredIncomingState)
            writable.push_back(slot.id.value);
    }
    writer.WriteCountU32(writable.size());
    for (const auto slot : writable) writer.WriteU32(slot);
    return std::move(writer).Take();
}

[[nodiscard]] std::vector<std::byte> OperationPayload(const package::d3d12_v13::D3D12PackageView& view)
{
    BinaryWriter writer;
    writer.WriteCountU32(view.FrameOperations().size());
    for (const auto& operation : view.FrameOperations())
    {
        writer.WriteU32(static_cast<std::uint32_t>(operation.opcode));
        writer.WriteU16(operation.operationVersion);
        writer.WriteU16(operation.flags);
        writer.WriteU32(operation.queue.value);
    }
    return std::move(writer).Take();
}
}

base::Expected<LeafCertificate, Error> BuildLeafCertificate(
    const package::FrozenExecutablePackage& frozen,
    const package::d3d12_v13::D3D12PackageView& view)
{
    const auto* provenance = frozen.FindSection(package::SectionKind::Provenance);
    if (provenance == nullptr)
        return base::Failure<LeafCertificate, Error>(
            {"LeafCertificate", "Sectionが検証または実行の契約に違反しています。"});
    auto verifierCertificate = planning::DecodeVerificationCertificate(provenance->bytes);
    if (!verifierCertificate)
        return base::Failure<LeafCertificate, Error>(
            {"LeafCertificate", verifierCertificate.error()});

    auto targetPayload = TargetPayload(view);
    auto resourcePayload = ResourcePayload(view);
    auto writePayload = WriteSetPayload(view);
    auto operationPayload = OperationPayload(view);

    LeafCertificate certificate;
    BinaryWriter semantic;
    semantic.WriteBytes(verifierCertificate.value().obligationDigest);
    certificate.semanticIdentity = MakeIdentity<canonical::SemanticIdentity>(
        "sge4.leaf.semantic-obligation", semantic.Bytes());
    certificate.executionIdentity = canonical::PackageExecutionIdentity::FromDigest(
        frozen.Header().executionDigest);
    certificate.verifiedPlanIdentity = canonical::VerifiedPlanIdentity::FromDigest(
        verifierCertificate.value().planIdentity);
    certificate.targetProfileIdentity = MakeIdentity<canonical::TargetProfileIdentity>(
        "sge4.leaf.target", targetPayload);
    certificate.resourceContractIdentity = MakeIdentity<canonical::ResourceContractIdentity>(
        "sge4.leaf.resources", resourcePayload);
    certificate.writeSetIdentity = MakeIdentity<canonical::WriteSetIdentity>(
        "sge4.leaf.write-set", writePayload);
    certificate.operationSequenceIdentity = MakeIdentity<canonical::OperationSequenceIdentity>(
        "sge4.leaf.operations", operationPayload);
    certificate.sealIdentity = canonical::VerificationSealIdentity::FromDigest(
        verifierCertificate.value().seal);

    BinaryWriter artifact;
    artifact.WriteBytes(frozen.Header().fileDigest);
    artifact.WriteBytes(certificate.verifiedPlanIdentity.Digest());
    artifact.WriteBytes(certificate.sealIdentity.Digest());
    artifact.WriteBytes(certificate.targetProfileIdentity.Digest());
    artifact.WriteBytes(certificate.resourceContractIdentity.Digest());
    artifact.WriteBytes(certificate.writeSetIdentity.Digest());
    artifact.WriteBytes(certificate.operationSequenceIdentity.Digest());
    certificate.artifactIdentity = MakeIdentity<canonical::FrozenArtifactIdentity>(
        "sge4.leaf.frozen-artifact", artifact.Bytes());
    return base::Success<LeafCertificate, Error>(std::move(certificate));
}
}
