#include "CompositionCertificate.h"

#include "../../canonical/artifact/SectionedArtifact.h"
#include "../../canonical/base/BinaryIO.h"
#include "../model/plan/CompositionPlan.h"

namespace sge4::composition
{
namespace
{
using base::BinaryWriter;

template<class Identity>
[[nodiscard]] Identity FromBaseDigest(const base::Digest256& digest)
{
    return Identity::FromDigest(digest);
}

template<class Identity>
[[nodiscard]] Identity FromPayload(std::string_view domain, std::span<const std::byte> payload)
{
    return Identity::FromDigest(ComputeDomainDigest(domain, 1, payload));
}
}

CompositionCertificate BuildCompositionCertificate(
    const ValidatedCompositionContract& validated,
    const verification::VerifiedCompositionPlan& verified,
    const base::Digest256& identitySeedDigest)
{
    const auto& contract = validated.Contract();
    const auto& plan = verified.Plan();
    const auto& verifier = verified.Certificate();

    CompositionCertificate result;
    result.contractIdentity = FromBaseDigest<CompositionContractIdentity>(verifier.contractIdentity);
    result.planIdentity = FromBaseDigest<CompositionPlanIdentity>(verifier.planIdentity);
    result.sealIdentity = FromBaseDigest<CompositionSealIdentity>(verifier.seal);

    BinaryWriter schedule;
    schedule.WriteCountU32(plan.schedule.size());
    for (const auto& entry : plan.schedule)
    {
        schedule.WriteU32(entry.ordinal);
        schedule.WriteU32(entry.leaf.value);
    }
    result.scheduleIdentity = FromPayload<ScheduleIdentity>(
        "sge4.composition.schedule", schedule.Bytes());

    BinaryWriter recovery;
    recovery.WriteU32(plan.recovery.schemaVersion);
    recovery.WriteU8(plan.recovery.resetTemporalState ? 1u : 0u);
    recovery.WriteU8(plan.recovery.requireExternalRebind ? 1u : 0u);
    recovery.WriteU16(0);
    recovery.WriteCountU32(plan.recovery.recreateLeaves.size());
    for (const auto leaf : plan.recovery.recreateLeaves) recovery.WriteU32(leaf.value);
    recovery.WriteCountU32(plan.recovery.recreateResources.size());
    for (const auto flow : plan.recovery.recreateResources) recovery.WriteU32(flow.value);
    result.recoverySetIdentity = FromPayload<RecoverySetIdentity>(
        "sge4.composition.recovery-set", recovery.Bytes());

    result.leafCount = static_cast<std::uint32_t>(contract.leaves.size());
    result.flowCount = static_cast<std::uint32_t>(contract.resources.size());

    BinaryWriter artifact;
    artifact.WriteBytes(identitySeedDigest);
    artifact.WriteBytes(result.contractIdentity.Digest());
    artifact.WriteBytes(result.planIdentity.Digest());
    artifact.WriteBytes(result.sealIdentity.Digest());
    artifact.WriteBytes(result.scheduleIdentity.Digest());
    artifact.WriteBytes(result.recoverySetIdentity.Digest());
    artifact.WriteU32(result.leafCount);
    artifact.WriteU32(result.flowCount);
    result.artifactIdentity = FromPayload<FrozenCompositionIdentity>(
        "sge4.composition.frozen-artifact", artifact.Bytes());
    return result;
}

CompositionCertificate BuildCompositionCertificate(
    const artifact::VerifiedFrozenComposition& complete)
{
    return BuildCompositionCertificate(
        complete.ValidatedContract(), complete.VerifiedPlan(), complete.IdentitySeedDigest());
}
}
