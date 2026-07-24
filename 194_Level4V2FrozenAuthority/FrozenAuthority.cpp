#include "FrozenAuthority.h"

namespace sge4_5::v2::frozen
{
OpaqueFrozenArtifactV1 FrozenAuthorityBuilderV1::Freeze(const authority::VerifiedAuthorityV1& verified)
{
    const auto frozenIdentity = authority::ComputeFrozenArtifactIdentityV1(
        verified.plan_.Identity(),
        verified.sealIdentity_,
        verified.targetProfileIdentity_,
        verified.resourceContractIdentity_,
        verified.writeSetIdentity_,
        verified.operationSequenceIdentity_);
    canonical::FrozenArtifactDescriptorV1 descriptor{
        frozenIdentity,
        verified.plan_.Identity(),
        verified.targetProfileIdentity_,
        verified.resourceContractIdentity_};
    return OpaqueFrozenArtifactV1{
        std::move(descriptor),
        verified.sealIdentity_,
        verified.writeSetIdentity_,
        verified.operationSequenceIdentity_};
}
}
