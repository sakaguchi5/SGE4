#include "./ExecutionPlanModel.h"

#include "../../../canonical/base/BinaryIO.h"

#include <algorithm>
#include <array>
#include <map>
#include <set>
#include <span>
#include <tuple>

namespace sge4::planning
{
namespace
{
inline constexpr std::array<std::byte, 8> VerificationCertificateMagic = {
    std::byte{'S'}, std::byte{'G'}, std::byte{'E'}, std::byte{'4'},
    std::byte{'V'}, std::byte{'F'}, std::byte{'Y'}, std::byte{0}
};

void WriteDigest(base::BinaryWriter& writer, const base::Digest256& digest) { writer.WriteBytes(digest); }
void WriteId(base::BinaryWriter& writer, std::uint32_t id) { writer.WriteU32(id); }
std::uint64_t ResourceSize(const semantic::Resource& resource)
{
    if (resource.kind == semantic::ResourceKind::Buffer) return resource.buffer.sizeBytes;
    // Schema 17 delegates native texture allocation size to D3D12 materialization.
    // Keep the planning value zero so the Plan and frozen AllocationArtifact
    // describe the same contract instead of inventing a linear texture size.
    return 0;
}
}

AbstractState RequiredState(semantic::ViewRole role, semantic::WorkKind workKind)
{
    switch (role)
    {
    case semantic::ViewRole::VertexData: return AbstractState::VertexBuffer;
    case semantic::ViewRole::ConstantData: return AbstractState::ConstantBuffer;
    case semantic::ViewRole::SampledTexture:
    case semantic::ViewRole::ShaderBuffer: return AbstractState::ShaderRead;
    case semantic::ViewRole::StorageBuffer: return AbstractState::UnorderedWrite;
    case semantic::ViewRole::ColorAttachment: return AbstractState::RenderTarget;
    case semantic::ViewRole::PresentSource: return AbstractState::Present;
    case semantic::ViewRole::DepthAttachment: return AbstractState::DepthWrite;
    case semantic::ViewRole::CopySource: return AbstractState::CopySource;
    case semantic::ViewRole::CopyDestination: return AbstractState::CopyDestination;
    }
    return workKind == semantic::WorkKind::Copy ? AbstractState::CopySource : AbstractState::Common;
}

base::Expected<SemanticObligation, std::string> BuildSemanticObligation(
    const semantic::SemanticGraph& graph, const analysis::AnalyzedGraph& analyzed)
{
    if (analyzed.source != &graph)
        return base::Failure<SemanticObligation, std::string>("入力または内部状態が検証または実行の契約に違反しています。");

    std::map<std::uint32_t, const semantic::Resource*> resources;
    std::map<std::uint32_t, const semantic::ResourceUse*> uses;
    std::map<std::uint32_t, const semantic::Work*> works;
    std::map<std::uint32_t, analysis::ResourceLifetime> lifetimes;
    for (const auto& value : graph.resources) resources[value.id.value] = &value;
    for (const auto& value : graph.resourceUses) uses[value.id.value] = &value;
    for (const auto& value : graph.works) works[value.id.value] = &value;
    for (const auto& value : analyzed.resourceLifetimes) lifetimes[value.resource.value] = value;

    std::map<std::uint32_t, semantic::WorkId> useOwners;
    for (const auto& work : graph.works)
        for (const auto& operand : work.operands) useOwners[operand.use.value] = work.id;

    SemanticObligation output;
    for (const auto id : analyzed.canonicalResourceOrder)
    {
        const auto& source = *resources.at(id.value);
        const auto lifetime = lifetimes.at(id.value);
        output.resources.push_back({source.id, source.kind, source.lifetime, source.update,
            source.visibility, ResourceSize(source),
            source.update == semantic::UpdateIntent::DynamicPerFrame ? 256u : 65536u,
            lifetime.firstUse, lifetime.lastUse, lifetime.usedByWork, source.aliasPreparation});
    }
    for (const auto id : analyzed.canonicalResourceUseOrder)
    {
        const auto& source = *uses.at(id.value);
        const auto owner = useOwners.find(id.value);
        if (owner == useOwners.end())
            return base::Failure<SemanticObligation, std::string>("ResourceUseが検証または実行の契約に違反しています。");
        output.uses.push_back({source.id, source.resource, owner->second, source.effect,
            source.role, source.temporalRelation});
    }
    for (const auto id : analyzed.canonicalWorkOrder)
    {
        const auto& source = *works.at(id.value);
        ObligationWork work{source.id, source.kind, {}};
        for (const auto& operand : source.operands) work.uses.push_back(operand.use);
        std::sort(work.uses.begin(), work.uses.end(), [](auto left, auto right) { return left.value < right.value; });
        output.works.push_back(std::move(work));
    }
    for (const auto& program : graph.programs)
    {
        for (const auto& parameter : program.interface.parameters)
            output.parameters.push_back({program.id, parameter.id, parameter.kind, parameter.stage,
                parameter.shaderRegister, parameter.requiredBytes});
    }
    std::sort(output.parameters.begin(), output.parameters.end(), [](const auto& left, const auto& right) {
        return std::tuple{left.program.value, left.id.value} < std::tuple{right.program.value, right.id.value};
    });
    output.dependencies = analyzed.dependencies;
    std::sort(output.dependencies.begin(), output.dependencies.end(), [](const auto& left, const auto& right) {
        return std::tuple{left.producer.value, left.consumer.value, left.resource.value,
            static_cast<std::uint16_t>(left.kind)} <
            std::tuple{right.producer.value, right.consumer.value, right.resource.value,
            static_cast<std::uint16_t>(right.kind)};
    });
    output.digest = base::Sha256(EncodeCanonical(output));
    return base::Success<SemanticObligation, std::string>(std::move(output));
}

D3D12PlanningContract BuildPlanningContract(const target::D3D12TargetProfile& profile)
{
    D3D12PlanningContract output;
    output.framesInFlight = profile.framesInFlight;
    output.directQueueCount = profile.directQueueCount;
    output.computeQueueCount = profile.computeQueueCount;
    output.copyQueueCount = profile.copyQueueCount;
    output.digest = base::Sha256(EncodeCanonical(output));
    return output;
}

std::vector<std::byte> EncodeCanonical(const SemanticObligation& value)
{
    base::BinaryWriter writer;
    writer.WriteU32(value.version);
    writer.WriteCountU32(value.resources.size());
    for (const auto& item : value.resources)
    {
        WriteId(writer, item.id.value); writer.WriteU16(static_cast<std::uint16_t>(item.kind));
        writer.WriteU16(static_cast<std::uint16_t>(item.lifetime)); writer.WriteU16(static_cast<std::uint16_t>(item.update));
        writer.WriteU16(static_cast<std::uint16_t>(item.visibility)); writer.WriteU64(item.sizeBytes);
        writer.WriteU32(item.alignment); writer.WriteU32(item.firstUse); writer.WriteU32(item.lastUse);
        writer.WriteU8(item.usedByWork ? 1u : 0u); WriteId(writer, item.aliasCompatibleWith.value);
    }
    writer.WriteCountU32(value.uses.size());
    for (const auto& item : value.uses)
    {
        WriteId(writer, item.id.value); WriteId(writer, item.resource.value); WriteId(writer, item.owner.value);
        writer.WriteU16(static_cast<std::uint16_t>(item.effect)); writer.WriteU16(static_cast<std::uint16_t>(item.role));
        writer.WriteU16(static_cast<std::uint16_t>(item.temporalRelation));
    }
    writer.WriteCountU32(value.works.size());
    for (const auto& item : value.works)
    {
        WriteId(writer, item.id.value); writer.WriteU16(static_cast<std::uint16_t>(item.kind));
        writer.WriteCountU32(item.uses.size());
        for (const auto id : item.uses) WriteId(writer, id.value);
    }
    writer.WriteCountU32(value.parameters.size());
    for (const auto& item : value.parameters)
    {
        WriteId(writer, item.program.value); WriteId(writer, item.id.value);
        writer.WriteU16(static_cast<std::uint16_t>(item.kind)); writer.WriteU16(static_cast<std::uint16_t>(item.stage));
        writer.WriteU32(item.shaderRegister); writer.WriteU64(item.requiredBytes);
    }
    writer.WriteCountU32(value.dependencies.size());
    for (const auto& item : value.dependencies)
    {
        WriteId(writer, item.producer.value); WriteId(writer, item.consumer.value); WriteId(writer, item.resource.value);
        writer.WriteU16(static_cast<std::uint16_t>(item.kind));
    }
    return std::move(writer).Take();
}

std::vector<std::byte> EncodeCanonical(const D3D12PlanningContract& value)
{
    base::BinaryWriter writer;
    writer.WriteU32(value.version); writer.WriteU32(value.framesInFlight);
    writer.WriteU32(value.directQueueCount); writer.WriteU32(value.computeQueueCount); writer.WriteU32(value.copyQueueCount);
    writer.WriteU32(value.maximumResources); writer.WriteU32(value.maximumAllocations);
    writer.WriteU32(value.maximumViews); writer.WriteU32(value.maximumOperations);
    writer.WriteU8(value.standalonePlacedAllocation ? 1u : 0u);
    return std::move(writer).Take();
}

std::vector<std::byte> EncodeCanonical(const ExecutionPlanIR& value, bool includeIdentity)
{
    base::BinaryWriter writer;
    writer.WriteU32(value.version); writer.WriteU16(static_cast<std::uint16_t>(value.scheduleStrategy));
    writer.WriteU16(static_cast<std::uint16_t>(value.queueStrategy)); writer.WriteU16(static_cast<std::uint16_t>(value.allocationStrategy));
    writer.WriteCountU32(value.workSchedule.size());
    for (const auto id : value.workSchedule) WriteId(writer, id.value);
    writer.WriteCountU32(value.queueAssignments.size());
    for (const auto& item : value.queueAssignments) { WriteId(writer, item.work.value); writer.WriteU16(static_cast<std::uint16_t>(item.queueClass)); writer.WriteU32(item.queueIndex); }
    writer.WriteCountU32(value.synchronization.size());
    for (const auto& item : value.synchronization) { WriteId(writer, item.producer.value); WriteId(writer, item.consumer.value); writer.WriteU32(item.signalPoint); }
    writer.WriteCountU32(value.resourceInstances.size());
    for (const auto& item : value.resourceInstances) { WriteId(writer, item.resource.value); writer.WriteU32(item.physicalInstanceCount); writer.WriteU32(item.firstUse); writer.WriteU32(item.lastUse); writer.WriteU32(item.allocation); }
    writer.WriteCountU32(value.allocations.size());
    for (const auto& item : value.allocations)
    {
        writer.WriteU32(item.id); writer.WriteU16(static_cast<std::uint16_t>(item.kind)); writer.WriteU64(item.sizeBytes);
        writer.WriteU64(item.alignment); writer.WriteU32(item.physicalInstanceCount); writer.WriteU32(item.aliasGroup);
        writer.WriteCountU32(item.resources.size()); for (const auto id : item.resources) WriteId(writer, id.value);
    }
    writer.WriteCountU32(value.useStates.size());
    for (const auto& item : value.useStates) { WriteId(writer, item.use.value); writer.WriteU16(static_cast<std::uint16_t>(item.required)); }
    writer.WriteCountU32(value.bindings.size());
    for (const auto& item : value.bindings) { WriteId(writer, item.program.value); WriteId(writer, item.parameter.value); writer.WriteU32(item.rootParameterIndex); writer.WriteU32(item.descriptorIndex); }
    writer.WriteCountU32(value.externalBoundaries.size()); for (const auto id : value.externalBoundaries) WriteId(writer, id.value);
    writer.WriteCountU32(value.presentBoundaries.size()); for (const auto id : value.presentBoundaries) WriteId(writer, id.value);
    if (includeIdentity) WriteDigest(writer, value.identity);
    return std::move(writer).Take();
}

std::vector<std::byte> EncodeCanonical(const ProfileRecord& value)
{
    base::BinaryWriter writer;
    writer.WriteU32(value.version); WriteDigest(writer, value.planIdentity); WriteDigest(writer, value.packageExecutionDigest);
    WriteDigest(writer, value.targetProfileDigest); WriteDigest(writer, value.adapterDriverFingerprint);
    WriteDigest(writer, value.measurementScenarioDigest); writer.WriteU32(value.sampleCount);
    writer.WriteU64(value.measuredNanoseconds); writer.WriteU64(value.observedPeakBytes);
    return std::move(writer).Take();
}

base::Digest256 ComputePlanIdentity(const ExecutionPlanIR& value)
{
    return base::Sha256(EncodeCanonical(value, false));
}

base::Digest256 ComputeVerificationSeal(const PlanVerificationCertificate& value)
{
    base::BinaryWriter writer;
    writer.WriteBytes(VerificationCertificateMagic);
    writer.WriteU32(value.schemaVersion);
    writer.WriteU32(0);
    writer.WriteBytes(value.obligationDigest);
    writer.WriteBytes(value.planningContractDigest);
    writer.WriteBytes(value.planIdentity);
    return base::Sha256(writer.Bytes());
}

std::vector<std::byte> EncodeVerificationCertificate(const PlanVerificationCertificate& value)
{
    base::BinaryWriter writer;
    writer.WriteBytes(VerificationCertificateMagic);
    writer.WriteU32(value.schemaVersion);
    writer.WriteU32(0);
    writer.WriteBytes(value.obligationDigest);
    writer.WriteBytes(value.planningContractDigest);
    writer.WriteBytes(value.planIdentity);
    writer.WriteBytes(value.seal);
    return std::move(writer).Take();
}

base::Expected<PlanVerificationCertificate, std::string> DecodeVerificationCertificate(
    std::span<const std::byte> bytes)
{
    base::BinaryReader reader(bytes);
    auto magic = reader.ReadBytes(VerificationCertificateMagic.size());
    auto schema = reader.ReadU32();
    auto reserved = reader.ReadU32();
    auto obligation = reader.ReadBytes(32);
    auto contract = reader.ReadBytes(32);
    auto plan = reader.ReadBytes(32);
    auto seal = reader.ReadBytes(32);
    if (!magic || !schema || !reserved || !obligation || !contract || !plan || !seal ||
        reader.Remaining() != 0)
        return base::Failure<PlanVerificationCertificate, std::string>(
            "Certificateが検証または実行の契約に違反しています。");
    if (!std::equal(magic.value().begin(), magic.value().end(), VerificationCertificateMagic.begin()))
        return base::Failure<PlanVerificationCertificate, std::string>(
            "Certificateが検証または実行の契約に違反しています。");
    if (schema.value() != 1 || reserved.value() != 0)
        return base::Failure<PlanVerificationCertificate, std::string>(
            "Certificateが検証または実行の契約に違反しています。");

    PlanVerificationCertificate result;
    result.schemaVersion = schema.value();
    std::copy(obligation.value().begin(), obligation.value().end(), result.obligationDigest.begin());
    std::copy(contract.value().begin(), contract.value().end(), result.planningContractDigest.begin());
    std::copy(plan.value().begin(), plan.value().end(), result.planIdentity.begin());
    std::copy(seal.value().begin(), seal.value().end(), result.seal.begin());
    if (result.seal != ComputeVerificationSeal(result))
        return base::Failure<PlanVerificationCertificate, std::string>(
            "Certificateが検証または実行の契約に違反しています。");
    return base::Success<PlanVerificationCertificate, std::string>(std::move(result));
}
}
