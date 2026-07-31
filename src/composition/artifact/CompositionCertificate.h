#pragma once

#include "../model/CompositionIdentity.h"
#include "VerifiedCompositionArtifact.h"
#include "../verifier/CompositionVerifier.h"

namespace sge4::composition
{
struct CompositionCertificate final
{
    std::uint32_t schemaVersion = 2;
    FrozenCompositionIdentity artifactIdentity = FrozenCompositionIdentity::FromDigest({});
    CompositionContractIdentity contractIdentity = CompositionContractIdentity::FromDigest({});
    CompositionPlanIdentity planIdentity = CompositionPlanIdentity::FromDigest({});
    CompositionSealIdentity sealIdentity = CompositionSealIdentity::FromDigest({});
    ScheduleIdentity scheduleIdentity = ScheduleIdentity::FromDigest({});
    RecoverySetIdentity recoverySetIdentity = RecoverySetIdentity::FromDigest({});
    std::uint32_t leafCount = 0;
    std::uint32_t flowCount = 0;
};

[[nodiscard]] CompositionCertificate BuildCompositionCertificate(
    const ValidatedCompositionContract& contract,
    const verification::VerifiedCompositionPlan& verified,
    const base::Digest256& identitySeedDigest);

[[nodiscard]] CompositionCertificate BuildCompositionCertificate(
    const artifact::VerifiedFrozenComposition& complete);
}
