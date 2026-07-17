#include "FrozenComposition.h"

#include "../00_Foundation/BinaryIO.h"
#include "../00_Foundation/Sha256.h"
#include "../09_FrozenPackageCore/PackageDigest.h"
#include "../09_FrozenPackageCore/PackageReader.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <map>
#include <set>

namespace sge4::composition
{
namespace
{
constexpr std::array<std::byte, 8> Magic{std::byte{'S'},std::byte{'G'},std::byte{'E'},std::byte{'4'},std::byte{'C'},std::byte{'M'},std::byte{'P'},std::byte{0}};
constexpr std::uint16_t HeaderBytes = 176;
constexpr std::uint16_t DescriptorBytes = 64;
constexpr std::uint32_t EndianTag = 0x01020304u;
constexpr std::size_t FileDigestOffset = 104;
constexpr std::uint32_t RequiredExecution = 3;

struct Section final
{
    CompositionSectionKind kind{};
    std::vector<std::byte> bytes;
    std::uint32_t count = 0;
    std::uint32_t stride = 0;
    std::uint64_t offset = 0;
    base::Digest256 digest{};
};
struct Descriptor final
{
    CompositionSectionKind kind{}; std::uint32_t flags = 0; std::uint64_t offset = 0; std::uint64_t bytes = 0;
    std::uint32_t count = 0; std::uint32_t stride = 0; base::Digest256 digest{};
};

CompositionArtifactError Error(std::string stage, std::string message) { return {std::move(stage), std::move(message)}; }
void Digest(base::BinaryWriter& writer, const base::Digest256& value) { writer.WriteBytes(value); }
void State(base::BinaryWriter& writer, const package::d3d12_v13::ResourceState& value)
{ writer.WriteU16(static_cast<std::uint16_t>(value.stateClass)); writer.WriteU16(value.reserved); writer.WriteU32(value.explicitBits); }

base::Result<package::d3d12_v13::ResourceState, CompositionArtifactError> ReadState(base::BinaryReader& reader, const char* stage)
{
    auto stateClass = reader.ReadU16(); auto reserved = reader.ReadU16(); auto bits = reader.ReadU32();
    if (!stateClass || !reserved || !bits) return base::Result<package::d3d12_v13::ResourceState, CompositionArtifactError>::Failure(Error(stage, "resource state is truncated"));
    package::d3d12_v13::ResourceState value{static_cast<package::d3d12_v13::StateClass>(stateClass.Value()), reserved.Value(), bits.Value()};
    if (value.reserved != 0 || (value.stateClass != package::d3d12_v13::StateClass::Common && value.stateClass != package::d3d12_v13::StateClass::Present && value.stateClass != package::d3d12_v13::StateClass::Explicit))
        return base::Result<package::d3d12_v13::ResourceState, CompositionArtifactError>::Failure(Error(stage, "resource state is invalid"));
    return base::Result<package::d3d12_v13::ResourceState, CompositionArtifactError>::Success(value);
}
base::Result<base::Digest256, CompositionArtifactError> ReadDigest(base::BinaryReader& reader, const char* stage)
{
    auto bytes = reader.ReadBytes(32); if (!bytes) return base::Result<base::Digest256, CompositionArtifactError>::Failure(Error(stage, "digest is truncated"));
    base::Digest256 value{}; std::copy(bytes.Value().begin(), bytes.Value().end(), value.begin());
    return base::Result<base::Digest256, CompositionArtifactError>::Success(value);
}
std::vector<std::byte> WithZeroFileDigest(std::span<const std::byte> bytes)
{
    std::vector<std::byte> copy(bytes.begin(), bytes.end());
    if (copy.size() >= FileDigestOffset + 32) std::fill(copy.begin() + FileDigestOffset, copy.begin() + FileDigestOffset + 32, std::byte{0});
    return copy;
}

std::vector<Section> EncodeSections(const PackageCompositionContract& contract, const LinkPlanIR& plan)
{
    std::vector<Section> sections;
    base::BinaryWriter manifest;
    manifest.WriteU32(1); manifest.WriteU32(static_cast<std::uint32_t>(contract.leaves.size())); manifest.WriteU32(static_cast<std::uint32_t>(contract.resources.size()));
    manifest.WriteU32(static_cast<std::uint32_t>(contract.endpoints.size())); manifest.WriteU32(static_cast<std::uint32_t>(contract.links.size()));
    manifest.WriteU32(static_cast<std::uint32_t>(plan.schedule.size())); manifest.WriteU32(0); manifest.WriteU32(0);
    sections.push_back({CompositionSectionKind::Manifest, std::move(manifest).Take(), 1, 32});

    base::BinaryWriter leafTable; base::BinaryWriter leafBytes;
    for (const auto& leaf : contract.leaves)
    {
        leafBytes.Align(16); const auto offset = leafBytes.Size(); leafBytes.WriteBytes(leaf.packageBytes);
        leafTable.WriteU32(leaf.id.value); leafTable.WriteU32(leaf.targetKind); leafTable.WriteU32(leaf.schemaVersion); leafTable.WriteU32(leaf.minimumRuntimeVersion);
        leafTable.WriteU64(leaf.authorKey); Digest(leafTable, leaf.executionDigest); Digest(leafTable, leaf.fileDigest); Digest(leafTable, leaf.targetProfileDigest);
        leafTable.WriteU32(leaf.surfaceSlotCount); leafTable.WriteU32(static_cast<std::uint32_t>(leaf.externalSlots.size())); leafTable.WriteU64(offset); leafTable.WriteU64(leaf.packageBytes.size());
        for (const auto& slot : leaf.externalSlots)
        {
            leafTable.WriteU32(slot.slot); leafTable.WriteU16(static_cast<std::uint16_t>(slot.kind)); leafTable.WriteU16(0); leafTable.WriteU32(static_cast<std::uint32_t>(slot.format));
            leafTable.WriteU64(slot.minimumBytes); State(leafTable, slot.incomingState); State(leafTable, slot.outgoingState); leafTable.WriteU32(static_cast<std::uint32_t>(slot.synchronization)); leafTable.WriteU32(0);
        }
    }
    sections.push_back({CompositionSectionKind::EmbeddedLeafTable, std::move(leafTable).Take(), static_cast<std::uint32_t>(contract.leaves.size()), 0});
    sections.push_back({CompositionSectionKind::EmbeddedLeafBytes, std::move(leafBytes).Take(), static_cast<std::uint32_t>(contract.leaves.size()), 0});

    base::BinaryWriter resources;
    for (const auto& value : contract.resources)
    {
        resources.WriteU32(value.id.value); resources.WriteU64(value.authorKey); resources.WriteU16(static_cast<std::uint16_t>(value.kind));
        resources.WriteU8(static_cast<std::uint8_t>(value.boundary)); resources.WriteU8(static_cast<std::uint8_t>(value.ownership)); resources.WriteU32(static_cast<std::uint32_t>(value.format));
        resources.WriteU64(value.sizeBytes); State(resources, value.initialState); resources.WriteU64(plan.resources[value.id.value].allocationBytes); resources.WriteU32(0); resources.WriteU32(0);
    }
    sections.push_back({CompositionSectionKind::SharedResourceTable, std::move(resources).Take(), static_cast<std::uint32_t>(contract.resources.size()), 48});

    std::vector<SharedResourceId> bound(contract.endpoints.size()); for (const auto& value : contract.bindings) bound[value.endpoint.value] = value.resource;
    base::BinaryWriter endpoints;
    for (const auto& value : contract.endpoints)
    {
        endpoints.WriteU32(value.id.value); endpoints.WriteU32(value.leaf.value); endpoints.WriteU32(value.externalSlot); endpoints.WriteU8(static_cast<std::uint8_t>(value.direction)); endpoints.WriteZeroes(3);
        endpoints.WriteU32(bound[value.id.value].value); endpoints.WriteU32(0);
    }
    sections.push_back({CompositionSectionKind::EndpointBindingTable, std::move(endpoints).Take(), static_cast<std::uint32_t>(contract.endpoints.size()), 24});

    base::BinaryWriter links;
    for (const auto& value : contract.links)
    { links.WriteU32(value.id.value); links.WriteU32(value.resource.value); links.WriteU32(value.producer.value); links.WriteU32(value.consumer.value); State(links, value.handoffState); }
    sections.push_back({CompositionSectionKind::LinkTable, std::move(links).Take(), static_cast<std::uint32_t>(contract.links.size()), 24});
    base::BinaryWriter schedule; for (const auto& value : plan.schedule) { schedule.WriteU32(value.ordinal); schedule.WriteU32(value.leaf.value); }
    sections.push_back({CompositionSectionKind::PackageSchedule, std::move(schedule).Take(), static_cast<std::uint32_t>(plan.schedule.size()), 8});
    base::BinaryWriter recovery; recovery.WriteU8(static_cast<std::uint8_t>(plan.recovery.policy)); recovery.WriteZeroes(7);
    sections.push_back({CompositionSectionKind::RecoveryPlan, std::move(recovery).Take(), 1, 8});
    return sections;
}
}

base::Result<std::vector<std::byte>, CompositionArtifactError> WriteFrozenComposition(
    const PackageCompositionContract& contract,
    const VerifiedLinkPlan& verifiedPlan)
{
    const auto& plan = verifiedPlan.Plan();
    if (contract.identity != ComputeContractIdentity(contract) || plan.contractIdentity != contract.identity ||
        plan.identity != ComputeLinkPlanIdentity(contract, plan))
        return base::Result<std::vector<std::byte>, CompositionArtifactError>::Failure(Error("composition/write", "verified contract or Link Plan identity is invalid"));
    auto sections = EncodeSections(contract, plan);
    std::uint64_t cursor = HeaderBytes + static_cast<std::uint64_t>(sections.size()) * DescriptorBytes;
    cursor = (cursor + 15u) & ~std::uint64_t{15u};
    for (auto& section : sections)
    {
        section.offset = cursor; section.digest = base::Sha256(section.bytes); cursor += section.bytes.size(); cursor = (cursor + 15u) & ~std::uint64_t{15u};
    }
    base::BinaryWriter writer;
    writer.WriteBytes(Magic); writer.WriteU16(1); writer.WriteU16(0); writer.WriteU16(HeaderBytes); writer.WriteU16(DescriptorBytes);
    writer.WriteU32(EndianTag); writer.WriteU32(static_cast<std::uint32_t>(sections.size())); writer.WriteU64(cursor); writer.WriteU64(HeaderBytes);
    Digest(writer, contract.identity); Digest(writer, plan.identity); writer.WriteZeroes(32); writer.WriteZeroes(HeaderBytes - writer.Size());
    for (const auto& section : sections)
    {
        writer.WriteU32(static_cast<std::uint32_t>(section.kind)); writer.WriteU32(RequiredExecution); writer.WriteU64(section.offset); writer.WriteU64(section.bytes.size());
        writer.WriteU32(section.count); writer.WriteU32(section.stride); Digest(writer, section.digest);
    }
    writer.Align(16);
    for (const auto& section : sections) { while (writer.Size() < section.offset) writer.WriteU8(0); writer.WriteBytes(section.bytes); writer.Align(16); }
    auto bytes = std::move(writer).Take();
    const auto fileDigest = base::Sha256(bytes); base::BinaryWriter patcher; patcher.WriteBytes(fileDigest);
    std::copy(fileDigest.begin(), fileDigest.end(), bytes.begin() + FileDigestOffset);
    return base::Result<std::vector<std::byte>, CompositionArtifactError>::Success(std::move(bytes));
}

base::Result<FrozenComposition, CompositionArtifactError> ReadFrozenComposition(std::span<const std::byte> bytes)
{ return ReadFrozenComposition(std::vector<std::byte>(bytes.begin(), bytes.end())); }

base::Result<FrozenComposition, CompositionArtifactError> ReadFrozenComposition(std::vector<std::byte> bytes)
{
    if (bytes.size() < HeaderBytes) return base::Result<FrozenComposition, CompositionArtifactError>::Failure(Error("composition/header", "file is shorter than the v1 header"));
    base::BinaryReader reader(bytes);
    auto magic = reader.ReadBytes(8); auto major = reader.ReadU16(); auto minor = reader.ReadU16(); auto headerBytes = reader.ReadU16(); auto descriptorBytes = reader.ReadU16();
    auto endian = reader.ReadU32(); auto sectionCount = reader.ReadU32(); auto fileBytes = reader.ReadU64(); auto tableOffset = reader.ReadU64();
    auto contractIdentity = ReadDigest(reader, "composition/header"); auto planIdentity = ReadDigest(reader, "composition/header"); auto fileDigest = ReadDigest(reader, "composition/header");
    if (!magic || !major || !minor || !headerBytes || !descriptorBytes || !endian || !sectionCount || !fileBytes || !tableOffset || !contractIdentity || !planIdentity || !fileDigest ||
        !std::ranges::equal(magic.Value(), Magic) || major.Value() != 1 || minor.Value() != 0 || headerBytes.Value() != HeaderBytes || descriptorBytes.Value() != DescriptorBytes ||
        endian.Value() != EndianTag || sectionCount.Value() != 8 || fileBytes.Value() != bytes.size() || tableOffset.Value() != HeaderBytes)
        return base::Result<FrozenComposition, CompositionArtifactError>::Failure(Error("composition/header", "header contract is invalid"));
    auto reservedHeader = reader.ReadBytes(HeaderBytes - reader.Position());
    if (!reservedHeader || std::ranges::any_of(reservedHeader.Value(), [](auto value) { return value != std::byte{0}; }) || base::Sha256(WithZeroFileDigest(bytes)) != fileDigest.Value())
        return base::Result<FrozenComposition, CompositionArtifactError>::Failure(Error("composition/file-digest", "reserved header or file digest is invalid"));

    std::map<CompositionSectionKind, Descriptor> descriptors;
    std::uint64_t previousEnd = HeaderBytes + static_cast<std::uint64_t>(sectionCount.Value()) * DescriptorBytes;
    for (std::uint32_t index = 0; index < sectionCount.Value(); ++index)
    {
        auto kind = reader.ReadU32(); auto flags = reader.ReadU32(); auto offset = reader.ReadU64(); auto size = reader.ReadU64(); auto count = reader.ReadU32(); auto stride = reader.ReadU32(); auto digest = ReadDigest(reader, "composition/section-table");
        if (!kind || !flags || !offset || !size || !count || !stride || !digest || flags.Value() != RequiredExecution || offset.Value() % 16 != 0 ||
            kind.Value() != index + 1u || offset.Value() < previousEnd || offset.Value() > bytes.size() || size.Value() > bytes.size() - offset.Value() ||
            std::ranges::any_of(std::span<const std::byte>(bytes.data() + previousEnd, static_cast<std::size_t>(offset.Value() - previousEnd)), [](auto value) { return value != std::byte{0}; }))
            return base::Result<FrozenComposition, CompositionArtifactError>::Failure(Error("composition/section-table", "section descriptor is invalid or overlaps"));
        const auto sectionKind = static_cast<CompositionSectionKind>(kind.Value());
        if (sectionKind < CompositionSectionKind::Manifest || sectionKind > CompositionSectionKind::RecoveryPlan || descriptors.contains(sectionKind))
            return base::Result<FrozenComposition, CompositionArtifactError>::Failure(Error("composition/section-table", "unknown required or duplicate section"));
        const std::span<const std::byte> section(bytes.data() + offset.Value(), static_cast<std::size_t>(size.Value()));
        if (base::Sha256(section) != digest.Value()) return base::Result<FrozenComposition, CompositionArtifactError>::Failure(Error("composition/section-digest", "section digest mismatch"));
        descriptors.emplace(sectionKind, Descriptor{sectionKind, flags.Value(), offset.Value(), size.Value(), count.Value(), stride.Value(), digest.Value()});
        previousEnd = offset.Value() + size.Value();
    }
    if (std::ranges::any_of(std::span<const std::byte>(bytes.data() + previousEnd, bytes.size() - static_cast<std::size_t>(previousEnd)), [](auto value) { return value != std::byte{0}; }))
        return base::Result<FrozenComposition, CompositionArtifactError>::Failure(Error("composition/padding", "canonical section padding must be zero"));
    const auto section = [&](CompositionSectionKind kind) { const auto& d = descriptors.at(kind); return std::span<const std::byte>(bytes.data() + d.offset, static_cast<std::size_t>(d.bytes)); };
    base::BinaryReader manifest(section(CompositionSectionKind::Manifest));
    auto version=manifest.ReadU32(); auto leafCount=manifest.ReadU32(); auto resourceCount=manifest.ReadU32(); auto endpointCount=manifest.ReadU32(); auto linkCount=manifest.ReadU32(); auto scheduleCount=manifest.ReadU32(); auto reserved0=manifest.ReadU32(); auto reserved1=manifest.ReadU32();
    if (!version||!leafCount||!resourceCount||!endpointCount||!linkCount||!scheduleCount||!reserved0||!reserved1||version.Value()!=1||reserved0.Value()!=0||reserved1.Value()!=0||manifest.Remaining()!=0)
        return base::Result<FrozenComposition, CompositionArtifactError>::Failure(Error("composition/manifest", "manifest is invalid"));
    if (descriptors.at(CompositionSectionKind::EmbeddedLeafTable).count != leafCount.Value() || descriptors.at(CompositionSectionKind::SharedResourceTable).count != resourceCount.Value() ||
        descriptors.at(CompositionSectionKind::EndpointBindingTable).count != endpointCount.Value() || descriptors.at(CompositionSectionKind::LinkTable).count != linkCount.Value() ||
        descriptors.at(CompositionSectionKind::PackageSchedule).count != scheduleCount.Value() ||
        descriptors.at(CompositionSectionKind::Manifest).count != 1 || descriptors.at(CompositionSectionKind::Manifest).stride != 32 ||
        descriptors.at(CompositionSectionKind::EmbeddedLeafTable).stride != 0 ||
        descriptors.at(CompositionSectionKind::EmbeddedLeafBytes).count != leafCount.Value() || descriptors.at(CompositionSectionKind::EmbeddedLeafBytes).stride != 0 ||
        descriptors.at(CompositionSectionKind::SharedResourceTable).stride != 48 || descriptors.at(CompositionSectionKind::EndpointBindingTable).stride != 24 ||
        descriptors.at(CompositionSectionKind::LinkTable).stride != 24 || descriptors.at(CompositionSectionKind::PackageSchedule).stride != 8 ||
        descriptors.at(CompositionSectionKind::RecoveryPlan).count != 1 || descriptors.at(CompositionSectionKind::RecoveryPlan).stride != 8)
        return base::Result<FrozenComposition, CompositionArtifactError>::Failure(Error("composition/manifest", "manifest counts disagree with sections"));

    PackageCompositionContract contract; LinkPlanIR plan; plan.contractIdentity = contractIdentity.Value(); plan.identity = planIdentity.Value();
    const auto leafBlob = section(CompositionSectionKind::EmbeddedLeafBytes); base::BinaryReader leaves(section(CompositionSectionKind::EmbeddedLeafTable));
    for (std::uint32_t i=0;i<leafCount.Value();++i)
    {
        LeafPackageContract leaf; auto id=leaves.ReadU32(); auto target=leaves.ReadU32(); auto schema=leaves.ReadU32(); auto runtime=leaves.ReadU32(); auto author=leaves.ReadU64();
        auto execution=ReadDigest(leaves,"composition/leaves"); auto file=ReadDigest(leaves,"composition/leaves"); auto profile=ReadDigest(leaves,"composition/leaves");
        auto surfaces=leaves.ReadU32(); auto slots=leaves.ReadU32(); auto offset=leaves.ReadU64(); auto size=leaves.ReadU64();
        if(!id||!target||!schema||!runtime||!author||!execution||!file||!profile||!surfaces||!slots||!offset||!size||id.Value()!=i||offset.Value()>leafBlob.size()||size.Value()>leafBlob.size()-offset.Value())
            return base::Result<FrozenComposition, CompositionArtifactError>::Failure(Error("composition/leaves","Leaf table record is invalid"));
        leaf.id={id.Value()}; leaf.targetKind=target.Value(); leaf.schemaVersion=schema.Value(); leaf.minimumRuntimeVersion=runtime.Value(); leaf.authorKey=author.Value(); leaf.executionDigest=execution.Value(); leaf.fileDigest=file.Value(); leaf.targetProfileDigest=profile.Value(); leaf.surfaceSlotCount=surfaces.Value();
        leaf.packageBytes.assign(leafBlob.begin()+offset.Value(),leafBlob.begin()+offset.Value()+size.Value());
        for(std::uint32_t s=0;s<slots.Value();++s)
        {
            auto slot=leaves.ReadU32(); auto kind=leaves.ReadU16(); auto zero=leaves.ReadU16(); auto format=leaves.ReadU32(); auto minimum=leaves.ReadU64(); auto incoming=ReadState(leaves,"composition/leaves"); auto outgoing=ReadState(leaves,"composition/leaves"); auto sync=leaves.ReadU32(); auto reserved=leaves.ReadU32();
            if(!slot||!kind||!zero||!format||!minimum||!incoming||!outgoing||!sync||!reserved||slot.Value()!=s||zero.Value()!=0||reserved.Value()!=0)
                return base::Result<FrozenComposition, CompositionArtifactError>::Failure(Error("composition/leaves","external slot record is invalid"));
            leaf.externalSlots.push_back({slot.Value(),static_cast<package::d3d12_v13::ResourceKind>(kind.Value()),static_cast<package::d3d12_v13::Format>(format.Value()),minimum.Value(),incoming.Value(),outgoing.Value(),static_cast<package::d3d12_v13::ExternalSynchronizationContract>(sync.Value())});
        }
        contract.leaves.push_back(std::move(leaf));
    }
    if(leaves.Remaining()!=0) return base::Result<FrozenComposition, CompositionArtifactError>::Failure(Error("composition/leaves","Leaf table has trailing bytes"));

    base::BinaryReader resources(section(CompositionSectionKind::SharedResourceTable));
    for(std::uint32_t i=0;i<resourceCount.Value();++i)
    {
        auto id=resources.ReadU32(); auto author=resources.ReadU64(); auto kind=resources.ReadU16(); auto boundary=resources.ReadU8(); auto owner=resources.ReadU8(); auto format=resources.ReadU32(); auto size=resources.ReadU64(); auto initial=ReadState(resources,"composition/resources"); auto allocation=resources.ReadU64(); auto z0=resources.ReadU32(); auto z1=resources.ReadU32();
        if(!id||!author||!kind||!boundary||!owner||!format||!size||!initial||!allocation||!z0||!z1||id.Value()!=i||z0.Value()!=0||z1.Value()!=0)
            return base::Result<FrozenComposition, CompositionArtifactError>::Failure(Error("composition/resources","resource record is invalid"));
        contract.resources.push_back({{i},author.Value(),static_cast<package::d3d12_v13::ResourceKind>(kind.Value()),static_cast<package::d3d12_v13::Format>(format.Value()),size.Value(),initial.Value(),static_cast<SharedResourceBoundary>(boundary.Value()),static_cast<SharedResourceOwnership>(owner.Value())});
        plan.resources.push_back({{i},static_cast<SharedResourceOwnership>(owner.Value()),allocation.Value()});
    }
    if(resources.Remaining()!=0) return base::Result<FrozenComposition, CompositionArtifactError>::Failure(Error("composition/resources","resource table has trailing bytes"));

    base::BinaryReader endpoints(section(CompositionSectionKind::EndpointBindingTable));
    for(std::uint32_t i=0;i<endpointCount.Value();++i)
    {
        auto id=endpoints.ReadU32(); auto leaf=endpoints.ReadU32(); auto slot=endpoints.ReadU32(); auto direction=endpoints.ReadU8(); auto zero=endpoints.ReadBytes(3); auto resource=endpoints.ReadU32(); auto reserved=endpoints.ReadU32();
        if(!id||!leaf||!slot||!direction||!zero||!resource||!reserved||id.Value()!=i||leaf.Value()>=contract.leaves.size()||slot.Value()>=contract.leaves[leaf.Value()].externalSlots.size()||resource.Value()>=contract.resources.size()||reserved.Value()!=0||std::ranges::any_of(zero.Value(),[](auto v){return v!=std::byte{0};}))
            return base::Result<FrozenComposition, CompositionArtifactError>::Failure(Error("composition/endpoints","endpoint record is invalid"));
        contract.endpoints.push_back({{i},{leaf.Value()},slot.Value(),static_cast<EndpointDirection>(direction.Value()),contract.leaves[leaf.Value()].externalSlots[slot.Value()]});
        contract.bindings.push_back({{resource.Value()},{i}}); plan.bindings.push_back({{i},{resource.Value()}});
    }
    if(endpoints.Remaining()!=0) return base::Result<FrozenComposition, CompositionArtifactError>::Failure(Error("composition/endpoints","endpoint table has trailing bytes"));

    base::BinaryReader links(section(CompositionSectionKind::LinkTable));
    for(std::uint32_t i=0;i<linkCount.Value();++i)
    {
        auto id=links.ReadU32(); auto resource=links.ReadU32(); auto producer=links.ReadU32(); auto consumer=links.ReadU32(); auto state=ReadState(links,"composition/links");
        if(!id||!resource||!producer||!consumer||!state||id.Value()!=i) return base::Result<FrozenComposition, CompositionArtifactError>::Failure(Error("composition/links","link record is invalid"));
        contract.links.push_back({{i},{resource.Value()},{producer.Value()},{consumer.Value()},state.Value()}); plan.signalWaits.push_back({{i},{producer.Value()},{consumer.Value()},state.Value()});
    }
    if(links.Remaining()!=0) return base::Result<FrozenComposition, CompositionArtifactError>::Failure(Error("composition/links","link table has trailing bytes"));
    base::BinaryReader schedule(section(CompositionSectionKind::PackageSchedule));
    for(std::uint32_t i=0;i<scheduleCount.Value();++i){auto ordinal=schedule.ReadU32();auto leaf=schedule.ReadU32();if(!ordinal||!leaf) return base::Result<FrozenComposition, CompositionArtifactError>::Failure(Error("composition/schedule","schedule is truncated"));plan.schedule.push_back({ordinal.Value(),{leaf.Value()}});}
    if(schedule.Remaining()!=0) return base::Result<FrozenComposition, CompositionArtifactError>::Failure(Error("composition/schedule","schedule has trailing bytes"));
    base::BinaryReader recovery(section(CompositionSectionKind::RecoveryPlan)); auto policy=recovery.ReadU8(); auto recoveryReserved=recovery.ReadBytes(7);
    if(!policy||!recoveryReserved||recovery.Remaining()!=0||std::ranges::any_of(recoveryReserved.Value(),[](auto v){return v!=std::byte{0};})) return base::Result<FrozenComposition, CompositionArtifactError>::Failure(Error("composition/recovery","recovery plan is invalid"));
    plan.recovery.policy=static_cast<CompositionRecoveryPolicy>(policy.Value()); contract.identity=ComputeContractIdentity(contract);
    if(contract.identity!=contractIdentity.Value()) return base::Result<FrozenComposition, CompositionArtifactError>::Failure(Error("composition/contract-identity","contract identity mismatch"));
    auto verified=VerifyAndSeal(contract,std::move(plan)); if(!verified) return base::Result<FrozenComposition, CompositionArtifactError>::Failure(Error(verified.Error().stage,verified.Error().message));
    auto storage=std::make_shared<const std::vector<std::byte>>(std::move(bytes));
    return base::Result<FrozenComposition, CompositionArtifactError>::Success(FrozenComposition(std::move(storage),std::move(contract),std::move(verified).Value()));
}
}
