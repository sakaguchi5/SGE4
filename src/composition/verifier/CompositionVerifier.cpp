#include "./CompositionVerifier.h"


#include "../../canonical/base/BinaryIO.h"

#include <algorithm>
#include <map>
#include <set>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace sge4::composition::verification
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
base::Expected<T, VerificationError> Failure(std::string stage,
                                           std::string message)
{
    return base::Failure<T, VerificationError>(
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

base::Expected<std::vector<std::uint32_t>, VerificationError>
DeriveCanonicalSchedule(const PackageCompositionContract& contract)
{
    std::vector<std::set<std::uint32_t>> outgoing(contract.leaves.size());
    std::vector<std::uint32_t> indegree(contract.leaves.size(), 0);
    for (const auto& resource : contract.resources)
    {
        if (resource.boundary != ResourceBoundary::Internal ||
            resource.lifetime == ResourceFlowLifetime::TemporalHistory) continue;
        if (!resource.producer.IsValid() ||
            resource.producer.value >= contract.endpoints.size())
            return Failure<std::vector<std::uint32_t>>(
                "verify/schedule", "Resource Flowが検証または実行の契約に違反しています。");
        const auto producerLeaf =
            contract.endpoints[resource.producer.value].leaf.value;
        for (const auto consumer : resource.consumers)
        {
            if (consumer.value >= contract.endpoints.size())
                return Failure<std::vector<std::uint32_t>>(
                    "verify/schedule", "Resource Flowが検証または実行の契約に違反しています。");
            const auto consumerLeaf =
                contract.endpoints[consumer.value].leaf.value;
            if (producerLeaf == consumerLeaf)
                return Failure<std::vector<std::uint32_t>>(
                    "verify/schedule", "Resource Flowが検証または実行の契約に違反しています。");
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
            "verify/cycle", "検証または実行の契約に違反しています。");
    return base::Success<std::vector<std::uint32_t>, VerificationError>(
        std::move(result));
}

base::Expected<void, VerificationError>
VerifyAuthority(const ValidatedCompositionContract& validatedContract,
                const planning::RawCompositionPlan& raw)
{
    const auto& contract = validatedContract.Contract();
    auto contractValidation = ValidateCompositionContractShape(contract);
    if (!contractValidation)
        return base::Failure<void, VerificationError>(
            Error("verify/contract", contractValidation.error().stage + "：" +
                                     contractValidation.error().message));
    auto shape = planning::ValidateRawCompositionPlanShape(raw);
    if (!shape)
        return base::Failure<void, VerificationError>(
            Error("verify/raw-shape", shape.error().stage + "：" +
                                      shape.error().message));
    if (raw.contractIdentity != contract.identity)
        return base::Failure<void, VerificationError>(
            Error("verify/contract-identity",
                  "Compositionが検証または実行の契約に違反しています。"));

    if (raw.allocations.size() != contract.resources.size())
        return base::Failure<void, VerificationError>(
            Error("verify/allocation", "検証または実行の契約に違反しています。"));
    for (std::uint32_t index = 0; index < contract.resources.size(); ++index)
    {
        const auto& source = contract.resources[index];
        const auto& value = raw.allocations[index];
        const auto expectedBytes = source.kind == package::d3d12_v13::ResourceKind::Texture2D
            ? static_cast<std::uint64_t>(source.texture2D.rowBytes) * source.texture2D.height
            : source.sizeBytes;
        if (value.resource != source.id ||
            value.ownership != ExpectedOwnership(source.boundary) ||
            value.kind != source.kind || value.format != source.format ||
            value.sizeBytes != expectedBytes || value.texture2D != source.texture2D ||
            value.lifetime != source.lifetime ||
            value.historyDepth != source.historyDepth ||
            value.physicalInstanceCount !=
                (source.lifetime == ResourceFlowLifetime::TemporalHistory ? 2u : 1u))
            return base::Failure<void, VerificationError>(
                Error("verify/allocation",
                      "検証または実行の契約に違反しています。"));
    }

    auto schedule = DeriveCanonicalSchedule(contract);
    if (!schedule)
        return base::Failure<void, VerificationError>(schedule.error());
    if (raw.schedule.size() != schedule.value().size())
        return base::Failure<void, VerificationError>(
            Error("verify/schedule", "Scheduleが検証または実行の契約に違反しています。"));
    std::vector<std::uint32_t> ordinalByLeaf(contract.leaves.size(), package::InvalidIndex);
    for (std::uint32_t ordinal = 0; ordinal < schedule.value().size(); ++ordinal)
    {
        if (raw.schedule[ordinal].ordinal != ordinal ||
            raw.schedule[ordinal].leaf.value != schedule.value()[ordinal])
            return base::Failure<void, VerificationError>(
                Error("verify/schedule",
                      "検証または実行の契約に違反しています。"));
        ordinalByLeaf[schedule.value()[ordinal]] = ordinal;
    }

    if (raw.bindings.size() != contract.bindings.size())
        return base::Failure<void, VerificationError>(
            Error("verify/binding", "Bindingが検証または実行の契約に違反しています。"));
    for (std::uint32_t index = 0; index < contract.bindings.size(); ++index)
    {
        const auto& source = contract.bindings[index];
        const auto& endpoint = contract.endpoints[source.endpoint.value];
        const auto& value = raw.bindings[index];
        if (value.endpoint != source.endpoint || value.resource != source.resource ||
            value.leaf != endpoint.leaf || value.externalSlot != endpoint.localExternalSlot ||
            value.access != endpoint.access)
            return base::Failure<void, VerificationError>(
                Error("verify/binding",
                      "検証または実行の契約に違反しています。"));
    }

    std::vector<planning::StateHandoffPlan> expectedHandoffs;
    for (const auto& resource : contract.resources)
    {
        if (resource.boundary != ResourceBoundary::Internal ||
            resource.lifetime == ResourceFlowLifetime::TemporalHistory) continue;
        const auto& producer = contract.endpoints[resource.producer.value];
        if (producer.synchronization !=
                package::d3d12_v13::ExternalSynchronizationContract::CompletionTokenRequired ||
            producer.flags != static_cast<std::uint32_t>(
                package::d3d12_v13::ExternalSlotFlags::Required))
            return base::Failure<void, VerificationError>(
                Error("verify/endpoint-contract",
                      "入力または内部状態が検証または実行の契約に違反しています。"));
        for (const auto consumerId : resource.consumers)
        {
            const auto& consumer = contract.endpoints[consumerId.value];
            if (consumer.synchronization !=
                    package::d3d12_v13::ExternalSynchronizationContract::CompletionTokenRequired ||
                consumer.flags != static_cast<std::uint32_t>(
                    package::d3d12_v13::ExternalSlotFlags::Required))
                return base::Failure<void, VerificationError>(
                    Error("verify/endpoint-contract",
                          "入力または内部状態が検証または実行の契約に違反しています。"));
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
        return base::Failure<void, VerificationError>(
            Error("verify/handoff", "Stateの状態または世代が実行契約と一致しません。"));
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
            return base::Failure<void, VerificationError>(
                Error("verify/handoff",
                      "Endpointが検証または実行の契約に違反しています。"));
    }

    std::vector<planning::SignalPointPlan> expectedSignals;
    std::map<std::uint32_t, std::uint32_t> signalByResource;
    for (const auto& resource : contract.resources)
    {
        if (resource.boundary != ResourceBoundary::Internal ||
            resource.lifetime == ResourceFlowLifetime::TemporalHistory) continue;
        const auto& producer = contract.endpoints[resource.producer.value];
        const std::uint32_t id = static_cast<std::uint32_t>(expectedSignals.size());
        signalByResource.emplace(resource.id.value, id);
        expectedSignals.push_back({
            id, resource.id, producer.id, producer.leaf,
            ordinalByLeaf[producer.leaf.value]});
    }
    if (raw.signals.size() != expectedSignals.size())
        return base::Failure<void, VerificationError>(
            Error("verify/signal", "Signalが検証または実行の契約に違反しています。"));
    for (std::uint32_t index = 0; index < expectedSignals.size(); ++index)
    {
        const auto& expected = expectedSignals[index];
        const auto& actual = raw.signals[index];
        if (actual.id != expected.id || actual.resource != expected.resource ||
            actual.producer != expected.producer ||
            actual.producerLeaf != expected.producerLeaf ||
            actual.producerScheduleOrdinal != expected.producerScheduleOrdinal)
            return base::Failure<void, VerificationError>(
                Error("verify/signal",
                      "Signalが検証または実行の契約に違反しています。"));
    }

    if (raw.waits.size() != expectedHandoffs.size())
        return base::Failure<void, VerificationError>(
            Error("verify/wait", "Waitが検証または実行の契約に違反しています。"));
    for (std::uint32_t index = 0; index < expectedHandoffs.size(); ++index)
    {
        const auto& handoff = expectedHandoffs[index];
        const auto& wait = raw.waits[index];
        const auto expectedSignal = signalByResource.at(handoff.resource.value);
        if (wait.id != index || wait.signal != expectedSignal ||
            wait.resource != handoff.resource || wait.consumer != handoff.consumer ||
            wait.consumerLeaf != handoff.consumerLeaf ||
            wait.consumerScheduleOrdinal != ordinalByLeaf[handoff.consumerLeaf.value])
            return base::Failure<void, VerificationError>(
                Error("verify/wait",
                      "Waitが検証または実行の契約に違反しています。"));
    }

    std::vector<planning::TemporalBufferPlan> expectedTemporalBuffers;
    for (const auto& resource : contract.resources)
    {
        if (resource.lifetime != ResourceFlowLifetime::TemporalHistory) continue;
        const auto& producer = contract.endpoints[resource.producer.value];
        planning::TemporalBufferPlan temporal;
        temporal.resource = resource.id;
        temporal.currentProducer = producer.id;
        temporal.currentProducerLeaf = producer.leaf;
        temporal.historyDepth = resource.historyDepth;
        temporal.physicalInstanceCount = 2;
        temporal.previousConsumers = resource.consumers;
        expectedTemporalBuffers.push_back(std::move(temporal));
    }
    if (raw.temporalBuffers.size() != expectedTemporalBuffers.size())
        return base::Failure<void, VerificationError>(
            Error("verify/temporal", "Temporal BufferがVerified Planへ固定されていません。"));
    for (std::uint32_t index = 0; index < expectedTemporalBuffers.size(); ++index)
    {
        const auto& expected = expectedTemporalBuffers[index];
        const auto& actual = raw.temporalBuffers[index];
        if (actual.resource != expected.resource ||
            actual.currentProducer != expected.currentProducer ||
            actual.currentProducerLeaf != expected.currentProducerLeaf ||
            actual.historyDepth != expected.historyDepth ||
            actual.physicalInstanceCount != expected.physicalInstanceCount ||
            actual.previousConsumers != expected.previousConsumers)
            return base::Failure<void, VerificationError>(
                Error("verify/temporal", "Temporal Bufferが検証または実行の契約に違反しています。"));
    }

    if (raw.recovery.schemaVersion != VerifierSchemaVersion ||
        raw.recovery.recreateLeaves.size() != contract.leaves.size() ||
        !raw.recovery.resetTemporalState ||
        !raw.recovery.requireExternalRebind)
        return base::Failure<void, VerificationError>(
            Error("verify/recovery", "検証または実行の契約に違反しています。"));
    for (std::uint32_t index = 0; index < contract.leaves.size(); ++index)
        if (raw.recovery.recreateLeaves[index].value != index)
            return base::Failure<void, VerificationError>(
                Error("verify/recovery", "検証または実行の契約に違反しています。"));

    std::vector<ResourceFlowId> internalResources;
    for (const auto& resource : contract.resources)
        if (resource.boundary == ResourceBoundary::Internal)
            internalResources.push_back(resource.id);
    if (raw.recovery.recreateResources != internalResources)
        return base::Failure<void, VerificationError>(
            Error("verify/recovery",
                  "検証または実行の契約に違反しています。"));

    if (raw.identity != planning::ComputeRawCompositionPlanIdentity(raw))
        return base::Failure<void, VerificationError>(
            Error("verify/plan-identity", "Planが検証または実行の契約に違反しています。"));
    return base::Success<void, VerificationError>();
}
}

base::Digest256
ComputeVerifierSeal(const base::Digest256& contractIdentity,
                    const base::Digest256& rawPlanIdentity)
{
    base::BinaryWriter writer;
    writer.WriteU32(VerifierSchemaVersion);
    writer.WriteCountU32(SealDomain.size());
    writer.WriteBytes(std::as_bytes(
        std::span<const char>(SealDomain.data(), SealDomain.size())));
    writer.WriteBytes(contractIdentity);
    writer.WriteBytes(rawPlanIdentity);
    return base::Sha256(writer.Bytes());
}

base::Expected<VerifiedCompositionPlan, VerificationError>
VerifyAndSeal(const ValidatedCompositionContract& validatedContract,
              const planning::RawCompositionPlan& rawPlan)
{
    const auto& contract = validatedContract.Contract();
    auto validation = VerifyAuthority(validatedContract, rawPlan);
    if (!validation)
        return base::Failure<VerifiedCompositionPlan, VerificationError>(
            validation.error());
    VerificationCertificate certificate;
    certificate.schemaVersion = CompositionVerifierSchemaVersion;
    certificate.algorithm = 1;
    certificate.contractIdentity = contract.identity;
    certificate.planIdentity = rawPlan.identity;
    certificate.seal = ComputeVerifierSeal(contract.identity, rawPlan.identity);
    return base::Success<VerifiedCompositionPlan, VerificationError>(
        VerifiedCompositionPlan(
            rawPlan, certificate,
            VerifiedCompositionPlan::ConstructionToken{}));
}

base::Expected<void, VerificationError>
ValidateVerifiedPlan(const ValidatedCompositionContract& validatedContract,
                     const VerifiedCompositionPlan& verified)
{
    const auto& contract = validatedContract.Contract();
    auto validation = VerifyAuthority(validatedContract, verified.Plan());
    if (!validation) return validation;
    const auto expected = ComputeVerifierSeal(
        contract.identity, verified.Plan().identity);
    if (verified.Certificate().seal != expected)
        return base::Failure<void, VerificationError>(
            Error("verify/seal", "Resource Flowが検証または実行の契約に違反しています。"));
    return base::Success<void, VerificationError>();
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

base::Expected<VerificationCertificate, VerificationError>
DeserializeVerificationCertificate(std::span<const std::byte> bytes)
{
    constexpr std::uint32_t CertificateMagic = 0x3256'4753u;
    constexpr std::size_t CertificateBytes = 16 + 32 * 3;
    if (bytes.size() != CertificateBytes)
        return Failure<VerificationCertificate>(
            "verify/certificate", "Certificateが検証または実行の契約に違反しています。");
    base::BinaryReader reader(bytes);
    auto magic = reader.ReadU32();
    auto version = reader.ReadU32();
    auto algorithm = reader.ReadU32();
    auto reserved = reader.ReadU32();
    auto contractIdentity = reader.ReadBytes(32);
    auto planIdentity = reader.ReadBytes(32);
    auto seal = reader.ReadBytes(32);
    if (!magic || !version || !algorithm || !reserved || !contractIdentity ||
        !planIdentity || !seal || magic.value() != CertificateMagic ||
        version.value() != CompositionVerifierSchemaVersion ||
        algorithm.value() != 1 || reserved.value() != 0 || reader.Remaining() != 0)
        return Failure<VerificationCertificate>(
            "verify/certificate", "Certificateが検証または実行の契約に違反しています。");
    VerificationCertificate result;
    result.schemaVersion = version.value();
    result.algorithm = algorithm.value();
    std::copy(contractIdentity.value().begin(), contractIdentity.value().end(), result.contractIdentity.begin());
    std::copy(planIdentity.value().begin(), planIdentity.value().end(), result.planIdentity.begin());
    std::copy(seal.value().begin(), seal.value().end(), result.seal.begin());
    return base::Success<VerificationCertificate, VerificationError>(result);
}
}
