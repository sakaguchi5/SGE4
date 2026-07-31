#pragma once

#include "../../canonical/artifact/SectionedArtifact.h"
#include "../../canonical/identity/CanonicalVocabulary.h"
#include "package/FrozenExecutablePackage.h"
#include "../../backends/d3d12/artifact/D3D12Encoding.h"

namespace sge4::leaf
{
struct LeafCertificate final
{
    std::uint32_t schemaVersion = 2;
    canonical::SemanticIdentity semanticIdentity = canonical::SemanticIdentity::FromDigest({});
    canonical::PackageExecutionIdentity executionIdentity = canonical::PackageExecutionIdentity::FromDigest({});
    canonical::VerifiedPlanIdentity verifiedPlanIdentity = canonical::VerifiedPlanIdentity::FromDigest({});
    canonical::FrozenArtifactIdentity artifactIdentity = canonical::FrozenArtifactIdentity::FromDigest({});
    canonical::TargetProfileIdentity targetProfileIdentity = canonical::TargetProfileIdentity::FromDigest({});
    canonical::ResourceContractIdentity resourceContractIdentity = canonical::ResourceContractIdentity::FromDigest({});
    canonical::WriteSetIdentity writeSetIdentity = canonical::WriteSetIdentity::FromDigest({});
    canonical::OperationSequenceIdentity operationSequenceIdentity = canonical::OperationSequenceIdentity::FromDigest({});
    canonical::VerificationSealIdentity sealIdentity = canonical::VerificationSealIdentity::FromDigest({});
};

[[nodiscard]] base::Expected<LeafCertificate, Error> BuildLeafCertificate(
    const package::FrozenExecutablePackage& package,
    const package::d3d12_v13::D3D12PackageView& view);
}
