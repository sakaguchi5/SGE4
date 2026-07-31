#include "AuthorityModel.h"

#include <limits>
#include <stdexcept>

namespace sge4_5::v2::authority
{
namespace
{
void AppendU8(std::vector<std::byte>& bytes, std::uint8_t value)
{
    bytes.push_back(static_cast<std::byte>(value));
}

void AppendU32(std::vector<std::byte>& bytes, std::uint32_t value)
{
    for (std::uint32_t index = 0; index < sizeof(value); ++index)
        bytes.push_back(static_cast<std::byte>((value >> (index * 8u)) & 0xffu));
}

void AppendU64(std::vector<std::byte>& bytes, std::uint64_t value)
{
    for (std::uint32_t index = 0; index < sizeof(value); ++index)
        bytes.push_back(static_cast<std::byte>((value >> (index * 8u)) & 0xffu));
}

void AppendDigest(std::vector<std::byte>& bytes, const canonical::Digest256V1& digest)
{
    bytes.insert(bytes.end(), digest.begin(), digest.end());
}

void AppendPayload(std::vector<std::byte>& bytes, std::span<const std::byte> payload)
{
    if constexpr (sizeof(std::size_t) > sizeof(std::uint64_t))
    {
        if (payload.size() > static_cast<std::size_t>(std::numeric_limits<std::uint64_t>::max()))
            throw std::invalid_argument("Canonical authority payload is too large.");
    }
    AppendU64(bytes, static_cast<std::uint64_t>(payload.size()));
    bytes.insert(bytes.end(), payload.begin(), payload.end());
}

template<class Scalar>
void AppendOptionalScalar(std::vector<std::byte>& bytes, const std::optional<Scalar>& value)
{
    AppendU8(bytes, value.has_value() ? 1u : 0u);
    if (value.has_value()) AppendU64(bytes, static_cast<std::uint64_t>(value->Value()));
}

template<class Identity>
Identity MakeIdentity(std::string_view domain, std::uint32_t schemaVersion, std::span<const std::byte> payload)
{
    return canonical::MakeCanonicalIdentityV1<Identity>({domain, schemaVersion, payload});
}

template<class Identity>
CanonicalBindingDefinitionV1<Identity> MakeBindingDefinition(
    std::string_view domain,
    std::uint32_t schemaVersion,
    std::span<const std::byte> canonicalPayload)
{
    std::vector<std::byte> payload(canonicalPayload.begin(), canonicalPayload.end());
    const auto identity = MakeIdentity<Identity>(domain, schemaVersion, payload);
    return {identity, schemaVersion, std::move(payload)};
}

template<class Identity>
Identity ComputeBindingIdentity(
    std::string_view domain,
    const CanonicalBindingDefinitionV1<Identity>& definition)
{
    return MakeIdentity<Identity>(domain, definition.schemaVersion, definition.canonicalPayload);
}
}

SemanticDefinitionV1 MakeSemanticDefinitionV1(
    canonical::SemanticVersion version,
    std::span<const std::byte> canonicalPayload)
{
    std::vector<std::byte> payload(canonicalPayload.begin(), canonicalPayload.end());
    std::vector<std::byte> identityPayload;
    AppendU32(identityPayload, version.Value());
    AppendPayload(identityPayload, payload);
    const auto identity = MakeIdentity<canonical::SemanticIdentity>(
        "SGE.V2.SemanticDefinition.V1", AuthorityModelSchemaVersionV1, identityPayload);
    return {{identity, version}, std::move(payload)};
}

canonical::InvocationDescriptorV1 MakeInvocationDescriptorV1(
    canonical::SemanticIdentity semanticIdentity,
    canonical::TimelineOrdinal timelineOrdinal,
    canonical::DeviceEpoch deviceEpoch,
    const canonical::DynamicExecutionDimensionsV1& dimensions)
{
    canonical::InvocationDescriptorV1 invocation{
        canonical::InvocationIdentity::FromDigest({}), semanticIdentity, timelineOrdinal, deviceEpoch, dimensions};
    invocation.identity = ComputeInvocationIdentityV1(invocation);
    return invocation;
}

TargetProfileDefinitionV1 MakeTargetProfileDefinitionV1(
    std::uint32_t schemaVersion,
    std::span<const std::byte> canonicalPayload)
{
    return MakeBindingDefinition<canonical::TargetProfileIdentity>(
        "SGE.V2.TargetProfileDefinition.V1", schemaVersion, canonicalPayload);
}

ResourceContractDefinitionV1 MakeResourceContractDefinitionV1(
    std::uint32_t schemaVersion,
    std::span<const std::byte> canonicalPayload)
{
    return MakeBindingDefinition<canonical::ResourceContractIdentity>(
        "SGE.V2.ResourceContractDefinition.V1", schemaVersion, canonicalPayload);
}

WriteSetDefinitionV1 MakeWriteSetDefinitionV1(
    std::uint32_t schemaVersion,
    std::span<const std::byte> canonicalPayload)
{
    return MakeBindingDefinition<canonical::WriteSetIdentity>(
        "SGE.V2.WriteSetDefinition.V1", schemaVersion, canonicalPayload);
}

canonical::SemanticIdentity ComputeSemanticIdentityV1(const SemanticDefinitionV1& definition)
{
    std::vector<std::byte> payload;
    AppendU32(payload, definition.descriptor.version.Value());
    AppendPayload(payload, definition.canonicalPayload);
    return MakeIdentity<canonical::SemanticIdentity>(
        "SGE.V2.SemanticDefinition.V1", AuthorityModelSchemaVersionV1, payload);
}

canonical::InvocationIdentity ComputeInvocationIdentityV1(const canonical::InvocationDescriptorV1& invocation)
{
    std::vector<std::byte> payload;
    AppendDigest(payload, invocation.semanticIdentity.Digest());
    AppendU64(payload, invocation.timelineOrdinal.Value());
    AppendU64(payload, invocation.deviceEpoch.Value());
    AppendOptionalScalar(payload, invocation.dimensions.activeCount);
    AppendOptionalScalar(payload, invocation.dimensions.transitionCount);
    AppendOptionalScalar(payload, invocation.dimensions.reuseCount);
    AppendOptionalScalar(payload, invocation.dimensions.updateInterval);
    AppendOptionalScalar(payload, invocation.dimensions.batchSize);
    AppendOptionalScalar(payload, invocation.dimensions.affectedBlockCount);
    return MakeIdentity<canonical::InvocationIdentity>(
        "SGE.V2.InvocationDescriptor.V1", AuthorityModelSchemaVersionV1, payload);
}

canonical::TargetProfileIdentity ComputeTargetProfileIdentityV1(const TargetProfileDefinitionV1& definition)
{
    return ComputeBindingIdentity("SGE.V2.TargetProfileDefinition.V1", definition);
}

canonical::ResourceContractIdentity ComputeResourceContractIdentityV1(const ResourceContractDefinitionV1& definition)
{
    return ComputeBindingIdentity("SGE.V2.ResourceContractDefinition.V1", definition);
}

canonical::WriteSetIdentity ComputeWriteSetIdentityV1(const WriteSetDefinitionV1& definition)
{
    return ComputeBindingIdentity("SGE.V2.WriteSetDefinition.V1", definition);
}

canonical::CandidateIdentity ComputeCandidateIdentityV1(
    const AuthorityRequestV1& request,
    const ExplicitCandidateInputV1& candidateInput)
{
    std::vector<std::byte> payload;
    AppendDigest(payload, request.semantic.descriptor.identity.Digest());
    AppendDigest(payload, request.invocation.identity.Digest());
    AppendDigest(payload, request.targetProfile.identity.Digest());
    AppendDigest(payload, request.resourceContract.identity.Digest());
    AppendDigest(payload, request.legalWriteSet.identity.Digest());
    AppendPayload(payload, candidateInput.canonicalPayload);
    return MakeIdentity<canonical::CandidateIdentity>(
        "SGE.V2.CandidateProposal.V1", candidateInput.schemaVersion, payload);
}

AuthorityContextIdentity ComputeAuthorityContextIdentityV1(const AuthorityRequestV1& request)
{
    std::vector<std::byte> payload;
    AppendDigest(payload, request.semantic.descriptor.identity.Digest());
    AppendDigest(payload, request.invocation.identity.Digest());
    AppendDigest(payload, request.targetProfile.identity.Digest());
    AppendDigest(payload, request.resourceContract.identity.Digest());
    AppendDigest(payload, request.legalWriteSet.identity.Digest());
    return MakeIdentity<AuthorityContextIdentity>(
        "SGE.V2.AuthorityContext.V1", AuthorityModelSchemaVersionV1, payload);
}

OperationSequenceIdentity ComputeOperationSequenceIdentityV1(std::span<const AuthorityOperationV1> operations)
{
    std::vector<std::byte> payload;
    AppendU64(payload, static_cast<std::uint64_t>(operations.size()));
    for (const auto& operation : operations)
    {
        AppendU32(payload, operation.ordinal.Value());
        AppendU8(payload, static_cast<std::uint8_t>(operation.kind));
        AppendDigest(payload, operation.subjectIdentity);
    }
    return MakeIdentity<OperationSequenceIdentity>(
        "SGE.V2.AuthorityOperationSequence.V1", AuthorityModelSchemaVersionV1, payload);
}

PlannerProposalIdentity ComputePlannerProposalIdentityV1(
    const canonical::RawCandidateProposalV1& rawCandidate,
    OperationSequenceIdentity operationSequenceIdentity)
{
    std::vector<std::byte> payload;
    AppendDigest(payload, rawCandidate.identity.Digest());
    AppendDigest(payload, operationSequenceIdentity.Digest());
    return MakeIdentity<PlannerProposalIdentity>(
        "SGE.V2.PlannerProposal.V1", AuthorityModelSchemaVersionV1, payload);
}

canonical::VerifiedPlanIdentity ComputeVerifiedPlanIdentityV1(
    AuthorityContextIdentity contextIdentity,
    canonical::CandidateIdentity candidateIdentity,
    OperationSequenceIdentity operationSequenceIdentity)
{
    std::vector<std::byte> payload;
    AppendDigest(payload, contextIdentity.Digest());
    AppendDigest(payload, candidateIdentity.Digest());
    AppendDigest(payload, operationSequenceIdentity.Digest());
    return MakeIdentity<canonical::VerifiedPlanIdentity>(
        "SGE.V2.VerifiedPlan.V1", AuthorityModelSchemaVersionV1, payload);
}

VerificationSealIdentity ComputeVerificationSealIdentityV1(
    canonical::VerifiedPlanIdentity verifiedPlanIdentity,
    AuthorityContextIdentity contextIdentity,
    canonical::CandidateIdentity candidateIdentity,
    OperationSequenceIdentity operationSequenceIdentity)
{
    std::vector<std::byte> payload;
    AppendDigest(payload, verifiedPlanIdentity.Digest());
    AppendDigest(payload, contextIdentity.Digest());
    AppendDigest(payload, candidateIdentity.Digest());
    AppendDigest(payload, operationSequenceIdentity.Digest());
    return MakeIdentity<VerificationSealIdentity>(
        "SGE.V2.VerificationSeal.V1", AuthorityModelSchemaVersionV1, payload);
}

canonical::FrozenArtifactIdentity ComputeFrozenArtifactIdentityV1(
    canonical::VerifiedPlanIdentity verifiedPlanIdentity,
    VerificationSealIdentity sealIdentity,
    canonical::TargetProfileIdentity targetProfileIdentity,
    canonical::ResourceContractIdentity resourceContractIdentity,
    canonical::WriteSetIdentity writeSetIdentity,
    OperationSequenceIdentity operationSequenceIdentity)
{
    std::vector<std::byte> payload;
    AppendDigest(payload, verifiedPlanIdentity.Digest());
    AppendDigest(payload, sealIdentity.Digest());
    AppendDigest(payload, targetProfileIdentity.Digest());
    AppendDigest(payload, resourceContractIdentity.Digest());
    AppendDigest(payload, writeSetIdentity.Digest());
    AppendDigest(payload, operationSequenceIdentity.Digest());
    return MakeIdentity<canonical::FrozenArtifactIdentity>(
        "SGE.V2.FrozenAuthorityArtifact.V1", AuthorityModelSchemaVersionV1, payload);
}
}
