#include "CompositionVerifier.h"

#include "../16_FrozenCompositionArtifact/FrozenCompositionReader.h"
#include "../16_FrozenCompositionArtifact/FrozenCompositionWriter.h"

#include "../00_Foundation/BinaryIO.h"

#include <algorithm>
#include <map>
#include <set>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace sge4::composition::canonical::verification
{
namespace
{
constexpr std::uint32_t VerifierSchemaVersion = 1;
constexpr std::string_view SealDomain =
    "SGE4-L4V1-Canonical-R2-Verified-Composition-Plan-v1";

VerificationError Error(std::string stage, std::string message)
{
    return {std::move(stage), std::move(message)};
}

template<class T>
base::Result<T, VerificationError> Failure(std::string stage,
                                           std::string message)
{
    return base::Result<T, VerificationError>::Failure(
        Error(std::move(stage), std::move(message)));
}

planning::AllocationOwnership ExpectedOwnership(ResourceBoundary boundary)
{
    switch (boundary)
    {
    case ResourceBoundary::Internal:
        return planning::AllocationOwnership::CompositionOwned;
    case ResourceBoundary::CompositionInput:
        return planning::AllocationOwnership::ExternalInput;
    case ResourceBoundary::CompositionOutput:
        return planning::AllocationOwnership::ExternalOutput;
    default:
        return static_cast<planning::AllocationOwnership>(0);
    }
}

base::Result<std::vector<std::uint32_t>, VerificationError>
DeriveCanonicalSchedule(const PackageCompositionContract& contract)
{
    std::vector<std::set<std::uint32_t>> outgoing(contract.leaves.size());
    std::vector<std::uint32_t> indegree(contract.leaves.size(), 0);
    for (const auto& resource : contract.resources)
    {
        if (resource.boundary != ResourceBoundary::Internal) continue;
        if (!resource.producer.IsValid() ||
            resource.producer.value >= contract.endpoints.size())
            return Failure<std::vector<std::uint32_t>>(
                "verify/schedule", "Internal Resource Flow producer is invalid");
        const auto producerLeaf =
            contract.endpoints[resource.producer.value].leaf.value;
        for (const auto consumer : resource.consumers)
        {
            if (consumer.value >= contract.endpoints.size())
                return Failure<std::vector<std::uint32_t>>(
                    "verify/schedule", "Resource Flow consumer is invalid");
            const auto consumerLeaf =
                contract.endpoints[consumer.value].leaf.value;
            if (producerLeaf == consumerLeaf)
                return Failure<std::vector<std::uint32_t>>(
                    "verify/schedule", "cross-Leaf Resource Flow self-edge is invalid");
            if (outgoing[producerLeaf].insert(consumerLeaf).second)
                ++indegree[consumerLeaf];
        }
    }

    std::set<std::uint32_t> ready;
    for (std::uint32_t leaf = 0; leaf < contract.leaves.size(); ++leaf)
        if (indegree[leaf] == 0) ready.insert(leaf);

    std::vector<std::uint32_t> result;
    while (!ready.empty())
    {
        const auto leaf = *ready.begin();
        ready.erase(ready.begin());
        result.push_back(leaf);
        for (const auto target : outgoing[leaf])
            if (--indegree[target] == 0) ready.insert(target);
    }
    if (result.size() != contract.leaves.size())
        return Failure<std::vector<std::uint32_t>>(
            "verify/cycle", "Composition Contract dependency graph is cyclic");
    return base::Result<std::vector<std::uint32_t>, VerificationError>::Success(
        std::move(result));
}

base::Result<void, VerificationError>
VerifyAuthority(const ValidatedCompositionContract& validatedContract,
                const planning::RawCompositionPlan& raw)
{
    const auto& contract = validatedContract.Contract();
    auto contractValidation = ValidateCompositionContractShape(contract);
    if (!contractValidation)
        return base::Result<void, VerificationError>::Failure(
            Error("verify/contract", contractValidation.Error().stage + ": " +
                                     contractValidation.Error().message));
    auto shape = planning::ValidateRawCompositionPlanShape(raw);
    if (!shape)
        return base::Result<void, VerificationError>::Failure(
            Error("verify/raw-shape", shape.Error().stage + ": " +
                                      shape.Error().message));
    if (raw.contractIdentity != contract.identity)
        return base::Result<void, VerificationError>::Failure(
            Error("verify/contract-identity",
                  "raw plan does not target the supplied Composition Contract"));

    if (raw.allocations.size() != contract.resources.size())
        return base::Result<void, VerificationError>::Failure(
            Error("verify/allocation", "allocation cardinality is not authoritative"));
    for (std::uint32_t index = 0; index < contract.resources.size(); ++index)
    {
        const auto& source = contract.resources[index];
        const auto& value = raw.allocations[index];
        if (value.resource != source.id ||
            value.ownership != ExpectedOwnership(source.boundary) ||
            value.kind != source.kind || value.format != source.format ||
            value.sizeBytes != source.sizeBytes)
            return base::Result<void, VerificationError>::Failure(
                Error("verify/allocation",
                      "allocation facts disagree with the authoritative Resource Flow"));
    }

    auto schedule = DeriveCanonicalSchedule(contract);
    if (!schedule)
        return base::Result<void, VerificationError>::Failure(schedule.Error());
    if (raw.schedule.size() != schedule.Value().size())
        return base::Result<void, VerificationError>::Failure(
            Error("verify/schedule", "schedule cardinality is invalid"));
    std::vector<std::uint32_t> ordinalByLeaf(contract.leaves.size(), package::InvalidIndex);
    for (std::uint32_t ordinal = 0; ordinal < schedule.Value().size(); ++ordinal)
    {
        if (raw.schedule[ordinal].ordinal != ordinal ||
            raw.schedule[ordinal].leaf.value != schedule.Value()[ordinal])
            return base::Result<void, VerificationError>::Failure(
                Error("verify/schedule",
                      "Leaf schedule is not the canonical topological order"));
        ordinalByLeaf[schedule.Value()[ordinal]] = ordinal;
    }

    if (raw.bindings.size() != contract.bindings.size())
        return base::Result<void, VerificationError>::Failure(
            Error("verify/binding", "binding cardinality is invalid"));
    for (std::uint32_t index = 0; index < contract.bindings.size(); ++index)
    {
        const auto& source = contract.bindings[index];
        const auto& endpoint = contract.endpoints[source.endpoint.value];
        const auto& value = raw.bindings[index];
        if (value.endpoint != source.endpoint || value.resource != source.resource ||
            value.leaf != endpoint.leaf || value.externalSlot != endpoint.localExternalSlot ||
            value.access != endpoint.access)
            return base::Result<void, VerificationError>::Failure(
                Error("verify/binding",
                      "endpoint binding disagrees with the authoritative Leaf interface"));
    }

    std::vector<planning::StateHandoffPlan> expectedHandoffs;
    for (const auto& resource : contract.resources)
    {
        if (resource.boundary != ResourceBoundary::Internal) continue;
        const auto& producer = contract.endpoints[resource.producer.value];
        if (producer.synchronization !=
                package::d3d12_v13::ExternalSynchronizationContract::CompletionTokenRequired ||
            producer.flags != static_cast<std::uint32_t>(
                package::d3d12_v13::ExternalSlotFlags::Required))
            return base::Result<void, VerificationError>::Failure(
                Error("verify/endpoint-contract",
                      "producer synchronization or frame semantics are unsupported"));
        for (const auto consumerId : resource.consumers)
        {
            const auto& consumer = contract.endpoints[consumerId.value];
            if (consumer.synchronization !=
                    package::d3d12_v13::ExternalSynchronizationContract::CompletionTokenRequired ||
                consumer.flags != static_cast<std::uint32_t>(
                    package::d3d12_v13::ExternalSlotFlags::Required))
                return base::Result<void, VerificationError>::Failure(
                    Error("verify/endpoint-contract",
                          "consumer synchronization or frame semantics are unsupported"));
            expectedHandoffs.push_back({
                resource.id, producer.id, consumer.id,
                producer.leaf, consumer.leaf,
                producer.guaranteedOutgoingState,
                consumer.requiredIncomingState});
        }
    }
    std::sort(expectedHandoffs.begin(), expectedHandoffs.end(),
        [](const auto& left, const auto& right) {
            return std::tie(left.resource.value, left.consumer.value) <
                   std::tie(right.resource.value, right.consumer.value);
        });
    if (raw.handoffs.size() != expectedHandoffs.size())
        return base::Result<void, VerificationError>::Failure(
            Error("verify/handoff", "state-handoff cardinality is invalid"));
    for (std::uint32_t index = 0; index < expectedHandoffs.size(); ++index)
    {
        const auto& expected = expectedHandoffs[index];
        const auto& actual = raw.handoffs[index];
        if (actual.resource != expected.resource ||
            actual.producer != expected.producer ||
            actual.consumer != expected.consumer ||
            actual.producerLeaf != expected.producerLeaf ||
            actual.consumerLeaf != expected.consumerLeaf ||
            actual.producerOutgoingState != expected.producerOutgoingState ||
            actual.consumerIncomingState != expected.consumerIncomingState)
            return base::Result<void, VerificationError>::Failure(
                Error("verify/handoff",
                      "state handoff was not derived from authoritative endpoints"));
    }

    std::vector<planning::SignalPointPlan> expectedSignals;
    std::map<std::uint32_t, std::uint32_t> signalByResource;
    for (const auto& resource : contract.resources)
    {
        if (resource.boundary != ResourceBoundary::Internal) continue;
        const auto& producer = contract.endpoints[resource.producer.value];
        const std::uint32_t id = static_cast<std::uint32_t>(expectedSignals.size());
        signalByResource.emplace(resource.id.value, id);
        expectedSignals.push_back({
            id, resource.id, producer.id, producer.leaf,
            ordinalByLeaf[producer.leaf.value]});
    }
    if (raw.signals.size() != expectedSignals.size())
        return base::Result<void, VerificationError>::Failure(
            Error("verify/signal", "signal cardinality is invalid"));
    for (std::uint32_t index = 0; index < expectedSignals.size(); ++index)
    {
        const auto& expected = expectedSignals[index];
        const auto& actual = raw.signals[index];
        if (actual.id != expected.id || actual.resource != expected.resource ||
            actual.producer != expected.producer ||
            actual.producerLeaf != expected.producerLeaf ||
            actual.producerScheduleOrdinal != expected.producerScheduleOrdinal)
            return base::Result<void, VerificationError>::Failure(
                Error("verify/signal",
                      "signal point was not derived from producer authority"));
    }

    if (raw.waits.size() != expectedHandoffs.size())
        return base::Result<void, VerificationError>::Failure(
            Error("verify/wait", "wait cardinality is invalid"));
    for (std::uint32_t index = 0; index < expectedHandoffs.size(); ++index)
    {
        const auto& handoff = expectedHandoffs[index];
        const auto& wait = raw.waits[index];
        const auto expectedSignal = signalByResource.at(handoff.resource.value);
        if (wait.id != index || wait.signal != expectedSignal ||
            wait.resource != handoff.resource || wait.consumer != handoff.consumer ||
            wait.consumerLeaf != handoff.consumerLeaf ||
            wait.consumerScheduleOrdinal != ordinalByLeaf[handoff.consumerLeaf.value])
            return base::Result<void, VerificationError>::Failure(
                Error("verify/wait",
                      "wait point was not derived from consumer authority"));
    }

    if (raw.recovery.schemaVersion != VerifierSchemaVersion ||
        raw.recovery.recreateLeaves.size() != contract.leaves.size() ||
        !raw.recovery.resetTemporalState ||
        !raw.recovery.requireExternalRebind)
        return base::Result<void, VerificationError>::Failure(
            Error("verify/recovery", "recovery policy is incomplete"));
    for (std::uint32_t index = 0; index < contract.leaves.size(); ++index)
        if (raw.recovery.recreateLeaves[index].value != index)
            return base::Result<void, VerificationError>::Failure(
                Error("verify/recovery", "Leaf recovery identity is not authoritative"));

    std::vector<ResourceFlowId> internalResources;
    for (const auto& resource : contract.resources)
        if (resource.boundary == ResourceBoundary::Internal)
            internalResources.push_back(resource.id);
    if (raw.recovery.recreateResources != internalResources)
        return base::Result<void, VerificationError>::Failure(
            Error("verify/recovery",
                  "composition-owned Resource recovery identity is not authoritative"));

    if (raw.identity != planning::ComputeRawCompositionPlanIdentity(raw))
        return base::Result<void, VerificationError>::Failure(
            Error("verify/plan-identity", "raw plan identity is corrupt"));
    return base::Result<void, VerificationError>::Success();
}
}

base::Digest256
ComputeVerifierSeal(const base::Digest256& contractIdentity,
                    const base::Digest256& rawPlanIdentity)
{
    base::BinaryWriter writer;
    writer.WriteU32(VerifierSchemaVersion);
    writer.WriteU32(static_cast<std::uint32_t>(SealDomain.size()));
    writer.WriteBytes(std::as_bytes(
        std::span<const char>(SealDomain.data(), SealDomain.size())));
    writer.WriteBytes(contractIdentity);
    writer.WriteBytes(rawPlanIdentity);
    return base::Sha256(writer.Bytes());
}

base::Result<VerifiedCompositionPlan, VerificationError>
VerifyAndSeal(const ValidatedCompositionContract& validatedContract,
              const planning::RawCompositionPlan& rawPlan)
{
    const auto& contract = validatedContract.Contract();
    auto validation = VerifyAuthority(validatedContract, rawPlan);
    if (!validation)
        return base::Result<VerifiedCompositionPlan, VerificationError>::Failure(
            validation.Error());
    VerificationCertificate certificate;
    certificate.schemaVersion = CompositionVerifierSchemaVersion;
    certificate.algorithm = 1;
    certificate.contractIdentity = contract.identity;
    certificate.planIdentity = rawPlan.identity;
    certificate.seal = ComputeVerifierSeal(contract.identity, rawPlan.identity);
    return base::Result<VerifiedCompositionPlan, VerificationError>::Success(
        VerifiedCompositionPlan(
            rawPlan, certificate,
            VerifiedCompositionPlan::ConstructionToken{}));
}

base::Result<void, VerificationError>
ValidateVerifiedPlan(const ValidatedCompositionContract& validatedContract,
                     const VerifiedCompositionPlan& verified)
{
    const auto& contract = validatedContract.Contract();
    auto validation = VerifyAuthority(validatedContract, verified.Plan());
    if (!validation) return validation;
    const auto expected = ComputeVerifierSeal(
        contract.identity, verified.Plan().identity);
    if (verified.Certificate().seal != expected)
        return base::Result<void, VerificationError>::Failure(
            Error("verify/seal", "Verified Resource Flow Plan seal is invalid"));
    return base::Result<void, VerificationError>::Success();
}


std::vector<std::byte>
SerializeVerificationCertificate(const VerificationCertificate& certificate)
{
    constexpr std::uint32_t CertificateMagic = 0x3256'4753u; // "SGV2"
    base::BinaryWriter writer;
    writer.WriteU32(CertificateMagic);
    writer.WriteU32(certificate.schemaVersion);
    writer.WriteU32(certificate.algorithm);
    writer.WriteU32(0);
    writer.WriteBytes(certificate.contractIdentity);
    writer.WriteBytes(certificate.planIdentity);
    writer.WriteBytes(certificate.seal);
    return std::move(writer).Take();
}

base::Result<VerificationCertificate, VerificationError>
DeserializeVerificationCertificate(std::span<const std::byte> bytes)
{
    constexpr std::uint32_t CertificateMagic = 0x3256'4753u;
    constexpr std::size_t CertificateBytes = 16 + 32 * 3;
    if (bytes.size() != CertificateBytes)
        return Failure<VerificationCertificate>(
            "verify/certificate", "verification certificate size is invalid");
    base::BinaryReader reader(bytes);
    auto magic = reader.ReadU32();
    auto version = reader.ReadU32();
    auto algorithm = reader.ReadU32();
    auto reserved = reader.ReadU32();
    auto contractIdentity = reader.ReadBytes(32);
    auto planIdentity = reader.ReadBytes(32);
    auto seal = reader.ReadBytes(32);
    if (!magic || !version || !algorithm || !reserved || !contractIdentity ||
        !planIdentity || !seal || magic.Value() != CertificateMagic ||
        version.Value() != CompositionVerifierSchemaVersion ||
        algorithm.Value() != 1 || reserved.Value() != 0 || reader.Remaining() != 0)
        return Failure<VerificationCertificate>(
            "verify/certificate", "verification certificate header is invalid");
    VerificationCertificate result;
    result.schemaVersion = version.Value();
    result.algorithm = algorithm.Value();
    std::copy(contractIdentity.Value().begin(), contractIdentity.Value().end(), result.contractIdentity.begin());
    std::copy(planIdentity.Value().begin(), planIdentity.Value().end(), result.planIdentity.begin());
    std::copy(seal.Value().begin(), seal.Value().end(), result.seal.begin());
    return base::Result<VerificationCertificate, VerificationError>::Success(result);
}

base::Result<std::vector<std::byte>, VerificationError>
FreezeVerifiedComposition(
    const ValidatedCompositionContract& validatedContract,
    const VerifiedCompositionPlan& verified)
{
    auto authority = ValidateVerifiedPlan(validatedContract, verified);
    if (!authority)
        return base::Result<std::vector<std::byte>, VerificationError>::Failure(authority.Error());

    const auto& contract = validatedContract.Contract();
    ::sge4::composition::FrozenCompositionBuildInput input;
    input.leaves.reserve(validatedContract.Leaves().size());
    for (const auto& leaf : validatedContract.Leaves())
    {
        ::sge4::composition::EmbeddedLeafInput embedded;
        embedded.stableKey = leaf.stableKey;
        embedded.packageBytes = leaf.packageBytes;
        input.leaves.push_back(std::move(embedded));
    }
    if (contract.presenterLeaf.IsValid())
    {
        if (contract.presenterLeaf.value >= validatedContract.Leaves().size())
            return Failure<std::vector<std::byte>>(
                "freeze/presenter", "verified Contract presenter is out of range");
        input.hasPresenter = true;
        input.presenterStableKey = validatedContract.Leaves()[contract.presenterLeaf.value].stableKey;
    }
    input.contractBytes = SerializeCompositionContract(contract);
    input.verifiedDecisionBytes = planning::SerializeRawCompositionPlan(verified.Plan());
    input.verificationCertificateBytes = SerializeVerificationCertificate(verified.Certificate());

    auto artifact = ::sge4::composition::FrozenCompositionWriter::Write(std::move(input));
    if (!artifact)
        return Failure<std::vector<std::byte>>(
            "freeze/container", artifact.Error().message);
    return base::Result<std::vector<std::byte>, VerificationError>::Success(
        std::move(artifact).Value());
}

base::Result<VerifiedFrozenComposition, VerificationError>
ReadVerifiedFrozenComposition(std::span<const std::byte> bytes)
{
    return ReadVerifiedFrozenComposition(std::vector<std::byte>(bytes.begin(), bytes.end()));
}

base::Result<VerifiedFrozenComposition, VerificationError>
ReadVerifiedFrozenComposition(std::vector<std::byte> bytes)
{
    auto artifactResult = ::sge4::composition::FrozenCompositionReader::Read(std::move(bytes));
    if (!artifactResult)
        return Failure<VerifiedFrozenComposition>(
            "read/container", artifactResult.Error().message);
    auto artifact = std::move(artifactResult).Value();

    auto contractResult = DeserializeCompositionContract(artifact.ContractBytes());
    if (!contractResult)
        return Failure<VerifiedFrozenComposition>(
            "read/contract", contractResult.Error().stage + ": " + contractResult.Error().message);
    std::vector<CanonicalLeafPackage> leaves;
    leaves.reserve(artifact.Leaves().size());
    for (const auto& leaf : artifact.Leaves())
    {
        CanonicalLeafPackage canonical;
        canonical.stableKey = leaf.record.stableKey;
        canonical.packageBytes.assign(leaf.packageBytes.begin(), leaf.packageBytes.end());
        leaves.push_back(std::move(canonical));
    }
    auto validatedResult = ValidateCompositionContractAgainstLeaves(
        std::move(contractResult).Value(), std::move(leaves));
    if (!validatedResult)
        return Failure<VerifiedFrozenComposition>(
            "read/contract-authority",
            validatedResult.Error().stage + ": " + validatedResult.Error().message);
    auto validated = std::move(validatedResult).Value();

    if (artifact.Manifest().leafCount != validated.Contract().leaves.size() ||
        artifact.Manifest().presenterLeafId != validated.Contract().presenterLeaf.value)
        return Failure<VerifiedFrozenComposition>(
            "read/manifest", "Frozen Composition manifest disagrees with the authoritative Contract");

    auto rawResult = planning::DeserializeRawCompositionPlan(artifact.VerifiedDecisionBytes());
    if (!rawResult)
        return Failure<VerifiedFrozenComposition>(
            "read/plan", rawResult.Error().stage + ": " + rawResult.Error().message);
    auto certificateResult = DeserializeVerificationCertificate(
        artifact.VerificationCertificateBytes());
    if (!certificateResult)
        return base::Result<VerifiedFrozenComposition, VerificationError>::Failure(
            certificateResult.Error());

    auto verifiedResult = VerifyAndSeal(validated, rawResult.Value());
    if (!verifiedResult)
        return Failure<VerifiedFrozenComposition>(
            "read/independent-verification",
            verifiedResult.Error().stage + ": " + verifiedResult.Error().message);
    auto verified = std::move(verifiedResult).Value();
    const auto& encoded = certificateResult.Value();
    const auto& derived = verified.Certificate();
    if (encoded.schemaVersion != derived.schemaVersion ||
        encoded.algorithm != derived.algorithm ||
        encoded.contractIdentity != derived.contractIdentity ||
        encoded.planIdentity != derived.planIdentity || encoded.seal != derived.seal)
        return Failure<VerifiedFrozenComposition>(
            "read/certificate", "stored certificate does not match independent verification");

    return base::Result<VerifiedFrozenComposition, VerificationError>::Success(
        VerifiedFrozenComposition(
            std::move(artifact), std::move(validated), std::move(verified),
            VerifiedFrozenComposition::ConstructionToken{}));
}
}
