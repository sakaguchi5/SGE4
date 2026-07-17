#include "D3D12CompositionEncoding18.h"

#include "../00_Foundation/BinaryIO.h"
#include "../09_FrozenPackageCore/PackageReader.h"
#include "../09_FrozenPackageCore/PackageWriter.h"
#include "../10_D3D12PackageSchema/D3D12Encoding.h"

#include <algorithm>
#include <array>
#include <set>
#include <string>
#include <utility>

namespace sge4::package::d3d12_v18
{
namespace
{
constexpr std::uint32_t EndpointSectionFlags =
    static_cast<std::uint32_t>(SectionFlags::Required) |
    static_cast<std::uint32_t>(SectionFlags::ExecutionAffecting);

PackageError Error(
    PackageErrorCode code,
    std::string message,
    std::uint32_t recordIndex = InvalidIndex)
{
    PackageError error;
    error.code = code;
    error.section = CompositionEndpointTableSection;
    error.recordIndex = recordIndex;
    error.message = std::move(message);
    return error;
}

void WriteState(base::BinaryWriter& writer, const ResourceState& state)
{
    writer.WriteU16(static_cast<std::uint16_t>(state.stateClass));
    writer.WriteU16(state.reserved);
    writer.WriteU32(state.explicitBits);
}

base::Result<ResourceState, PackageError> ReadState(
    base::BinaryReader& reader,
    std::uint32_t recordIndex)
{
    auto stateClass = reader.ReadU16();
    auto reserved = reader.ReadU16();
    auto bits = reader.ReadU32();
    if (!stateClass || !reserved || !bits)
        return base::Result<ResourceState, PackageError>::Failure(
            Error(PackageErrorCode::InvalidRecordStride,
                  "composition endpoint state is truncated", recordIndex));

    ResourceState state{
        static_cast<d3d12_v13::StateClass>(stateClass.Value()),
        reserved.Value(), bits.Value()};
    if (state.reserved != 0)
        return base::Result<ResourceState, PackageError>::Failure(
            Error(PackageErrorCode::InvalidEnumValue,
                  "composition endpoint state reserved field must be zero", recordIndex));
    if (state.stateClass != d3d12_v13::StateClass::Common &&
        state.stateClass != d3d12_v13::StateClass::Present &&
        state.stateClass != d3d12_v13::StateClass::Explicit)
        return base::Result<ResourceState, PackageError>::Failure(
            Error(PackageErrorCode::InvalidEnumValue,
                  "composition endpoint state class is unknown", recordIndex));
    if (state.stateClass != d3d12_v13::StateClass::Explicit && state.explicitBits != 0)
        return base::Result<ResourceState, PackageError>::Failure(
            Error(PackageErrorCode::InvalidEnumValue,
                  "composition endpoint non-explicit state contains explicit bits", recordIndex));
    if (state.stateClass == d3d12_v13::StateClass::Explicit && state.explicitBits == 0)
        return base::Result<ResourceState, PackageError>::Failure(
            Error(PackageErrorCode::InvalidEnumValue,
                  "composition endpoint explicit state is empty", recordIndex));
    return base::Result<ResourceState, PackageError>::Success(state);
}

std::vector<std::byte> EncodeEndpoints(
    std::span<const LeafCompositionEndpointArtifact> endpoints)
{
    base::BinaryWriter writer;
    for (const auto& endpoint : endpoints)
    {
        const auto start = writer.Size();
        writer.WriteU32(endpoint.id);
        writer.WriteBytes(endpoint.stableKey);
        writer.WriteU32(endpoint.externalSlot.value);
        writer.WriteU32(endpoint.resource.value);
        writer.WriteU16(static_cast<std::uint16_t>(endpoint.kind));
        writer.WriteU16(static_cast<std::uint16_t>(endpoint.access));
        writer.WriteU32(static_cast<std::uint32_t>(endpoint.format));
        writer.WriteU64(endpoint.minimumBytes);
        WriteState(writer, endpoint.requiredIncomingState);
        WriteState(writer, endpoint.guaranteedOutgoingState);
        writer.WriteU32(static_cast<std::uint32_t>(endpoint.synchronization));
        writer.WriteU32(static_cast<std::uint32_t>(endpoint.frameSemantics));
        writer.WriteU32(endpoint.flags);
        writer.WriteU32(endpoint.reserved);
        if (writer.Size() - start < CompositionEndpointStride)
            writer.WriteZeroes(CompositionEndpointStride - (writer.Size() - start));
    }
    return std::move(writer).Take();
}

base::Result<std::vector<LeafCompositionEndpointArtifact>, PackageError> DecodeEndpoints(
    const SectionView& section)
{
    const auto& descriptor = section.descriptor;
    if (descriptor.schemaVersion != CompositionEndpointSectionSchemaVersion ||
        descriptor.elementStride != CompositionEndpointStride ||
        descriptor.alignment != 8 ||
        descriptor.flags != EndpointSectionFlags ||
        static_cast<std::uint64_t>(descriptor.elementCount) * CompositionEndpointStride !=
            section.bytes.size())
        return base::Result<std::vector<LeafCompositionEndpointArtifact>, PackageError>::Failure(
            Error(PackageErrorCode::InvalidRecordStride,
                  "Schema 18 composition endpoint section descriptor is invalid"));

    base::BinaryReader reader(section.bytes);
    std::vector<LeafCompositionEndpointArtifact> endpoints;
    endpoints.reserve(descriptor.elementCount);
    for (std::uint32_t index = 0; index < descriptor.elementCount; ++index)
    {
        LeafCompositionEndpointArtifact endpoint;
        auto id = reader.ReadU32();
        auto stableKey = reader.ReadBytes(endpoint.stableKey.size());
        auto externalSlot = reader.ReadU32();
        auto resource = reader.ReadU32();
        auto kind = reader.ReadU16();
        auto access = reader.ReadU16();
        auto format = reader.ReadU32();
        auto minimumBytes = reader.ReadU64();
        if (!id || !stableKey || !externalSlot || !resource || !kind || !access ||
            !format || !minimumBytes)
            return base::Result<std::vector<LeafCompositionEndpointArtifact>, PackageError>::Failure(
                Error(PackageErrorCode::InvalidRecordStride,
                      "Schema 18 composition endpoint record is truncated", index));

        endpoint.id = id.Value();
        std::copy(stableKey.Value().begin(), stableKey.Value().end(), endpoint.stableKey.begin());
        endpoint.externalSlot = {externalSlot.Value()};
        endpoint.resource = {resource.Value()};
        endpoint.kind = static_cast<ResourceKind>(kind.Value());
        endpoint.access = static_cast<EndpointAccess>(access.Value());
        endpoint.format = static_cast<Format>(format.Value());
        endpoint.minimumBytes = minimumBytes.Value();

        auto incoming = ReadState(reader, index);
        auto outgoing = ReadState(reader, index);
        auto synchronization = reader.ReadU32();
        auto frameSemantics = reader.ReadU32();
        auto flags = reader.ReadU32();
        auto reserved = reader.ReadU32();
        auto padding = reader.ReadU32();
        if (!incoming || !outgoing || !synchronization || !frameSemantics || !flags || !reserved || !padding)
            return base::Result<std::vector<LeafCompositionEndpointArtifact>, PackageError>::Failure(
                incoming ? (outgoing ? Error(PackageErrorCode::InvalidRecordStride,
                    "Schema 18 composition endpoint tail is truncated", index) : outgoing.Error()) : incoming.Error());

        endpoint.requiredIncomingState = incoming.Value();
        endpoint.guaranteedOutgoingState = outgoing.Value();
        endpoint.synchronization =
            static_cast<ExternalSynchronizationContract>(synchronization.Value());
        endpoint.frameSemantics =
            static_cast<EndpointFrameSemantics>(frameSemantics.Value());
        endpoint.flags = flags.Value();
        endpoint.reserved = reserved.Value();
        if (padding.Value() != 0)
            return base::Result<std::vector<LeafCompositionEndpointArtifact>, PackageError>::Failure(
                Error(PackageErrorCode::InvalidRecordStride,
                      "Schema 18 composition endpoint padding must be zero", index));
        endpoints.push_back(endpoint);
    }
    if (reader.Remaining() != 0)
        return base::Result<std::vector<LeafCompositionEndpointArtifact>, PackageError>::Failure(
            Error(PackageErrorCode::InvalidRecordStride,
                  "Schema 18 composition endpoint table contains trailing bytes"));
    return base::Result<std::vector<LeafCompositionEndpointArtifact>, PackageError>::Success(
        std::move(endpoints));
}

bool IsAllowedView(EndpointAccess access, d3d12_v13::ViewClass viewClass) noexcept
{
    if (access == EndpointAccess::ReadOnly)
        return viewClass == d3d12_v13::ViewClass::ShaderResource;
    if (access == EndpointAccess::WriteOnly)
        return viewClass == d3d12_v13::ViewClass::UnorderedAccess;
    return false;
}

base::Result<void, PackageError> ValidateEndpoints(
    const d3d12_v13::D3D12PackageView& baseView,
    std::span<const LeafCompositionEndpointArtifact> endpoints)
{
    if (endpoints.size() != baseView.ExternalSlots().size())
        return base::Result<void, PackageError>::Failure(
            Error(PackageErrorCode::InvalidInvocationSchema,
                  "Schema 18 requires exactly one composition endpoint for every external Buffer slot"));

    std::set<StableEndpointKey> stableKeys;
    std::vector<bool> coveredSlots(baseView.ExternalSlots().size(), false);
    for (std::uint32_t index = 0; index < endpoints.size(); ++index)
    {
        const auto& endpoint = endpoints[index];
        if (endpoint.id != index)
            return base::Result<void, PackageError>::Failure(
                Error(PackageErrorCode::InvalidIdSequence,
                      "composition endpoint IDs must be dense and ordered", index));
        if (IsZeroStableEndpointKey(endpoint.stableKey) ||
            !stableKeys.insert(endpoint.stableKey).second)
            return base::Result<void, PackageError>::Failure(
                Error(PackageErrorCode::InvalidInvocationSchema,
                      "composition stable endpoint keys must be non-zero and unique", index));
        if (!endpoint.externalSlot.IsValid() || endpoint.externalSlot.value != index ||
            endpoint.externalSlot.value >= baseView.ExternalSlots().size() ||
            coveredSlots[endpoint.externalSlot.value])
            return base::Result<void, PackageError>::Failure(
                Error(PackageErrorCode::InvalidReference,
                      "composition endpoint external slot is invalid or duplicated", index));
        coveredSlots[endpoint.externalSlot.value] = true;

        const auto& slot = baseView.ExternalSlots()[endpoint.externalSlot.value];
        if (endpoint.resource != slot.resource || endpoint.kind != slot.requiredKind ||
            endpoint.format != slot.requiredFormat || endpoint.minimumBytes != slot.minimumBytes ||
            endpoint.requiredIncomingState != slot.requiredIncomingState ||
            endpoint.guaranteedOutgoingState != slot.guaranteedOutgoingState ||
            endpoint.synchronization != slot.synchronizationContract)
            return base::Result<void, PackageError>::Failure(
                Error(PackageErrorCode::InvalidInvocationSchema,
                      "composition endpoint does not exactly reproduce its Leaf external-slot contract", index));
        if (endpoint.kind != ResourceKind::Buffer || endpoint.format != Format::Unknown ||
            endpoint.minimumBytes == 0)
            return base::Result<void, PackageError>::Failure(
                Error(PackageErrorCode::InvalidInvocationSchema,
                      "Level 4 v1 Schema 18 composition endpoints are limited to unformatted Buffers", index));
        if (endpoint.access != EndpointAccess::ReadOnly &&
            endpoint.access != EndpointAccess::WriteOnly)
            return base::Result<void, PackageError>::Failure(
                Error(PackageErrorCode::InvalidEnumValue,
                      "composition endpoint access must be ReadOnly or WriteOnly", index));
        if (endpoint.frameSemantics != EndpointFrameSemantics::PerFrame ||
            endpoint.flags != static_cast<std::uint32_t>(EndpointFlags::Required) ||
            endpoint.reserved != 0)
            return base::Result<void, PackageError>::Failure(
                Error(PackageErrorCode::InvalidInvocationSchema,
                      "composition endpoint frame semantics, flags, or reserved field is invalid", index));
        if (endpoint.access == EndpointAccess::ReadOnly &&
            endpoint.requiredIncomingState != endpoint.guaranteedOutgoingState)
            return base::Result<void, PackageError>::Failure(
                Error(PackageErrorCode::InvalidInvocationSchema,
                      "ReadOnly composition endpoints must preserve their Resource state", index));

        if (!endpoint.resource.IsValid() || endpoint.resource.value >= baseView.Resources().size())
            return base::Result<void, PackageError>::Failure(
                Error(PackageErrorCode::InvalidReference,
                      "composition endpoint Resource is invalid", index));
        const auto& resource = baseView.Resources()[endpoint.resource.value];
        const auto viewEnd = static_cast<std::uint64_t>(resource.firstView) + resource.viewCount;
        if (resource.origin != d3d12_v13::ResourceOrigin::External ||
            resource.resourceKind != ResourceKind::Buffer || resource.viewCount == 0 ||
            viewEnd > baseView.Views().size())
            return base::Result<void, PackageError>::Failure(
                Error(PackageErrorCode::InvalidReference,
                      "composition endpoint Resource/view range is invalid", index));
        for (std::uint32_t viewIndex = resource.firstView; viewIndex < viewEnd; ++viewIndex)
        {
            const auto& view = baseView.Views()[viewIndex];
            if (view.resource != endpoint.resource || !IsAllowedView(endpoint.access, view.viewClass))
                return base::Result<void, PackageError>::Failure(
                    Error(PackageErrorCode::InvalidInvocationSchema,
                          "composition endpoint access disagrees with the frozen Leaf view class", index));
        }
    }

    if (std::any_of(coveredSlots.begin(), coveredSlots.end(), [](bool covered) { return !covered; }))
        return base::Result<void, PackageError>::Failure(
            Error(PackageErrorCode::InvalidInvocationSchema,
                  "a Leaf external slot is absent from the composition interface"));
    return base::Result<void, PackageError>::Success();
}

bool IsAllowedSchema18Section(SectionKind kind) noexcept
{
    switch (kind)
    {
    case SectionKind::Manifest:
    case SectionKind::InvocationSchema:
    case SectionKind::OperationStreamTable:
    case SectionKind::OperationTable:
    case SectionKind::OperationPayload:
    case SectionKind::InitialData:
    case SectionKind::ShaderData:
    case SectionKind::NativeObjectData:
    case SectionKind::D3D12TargetProfile:
    case SectionKind::D3D12ResourceTable:
    case SectionKind::D3D12AllocationTable:
    case SectionKind::D3D12ViewTable:
    case SectionKind::D3D12ShaderTable:
    case SectionKind::D3D12ProgramTable:
    case SectionKind::D3D12BindingLayoutTable:
    case SectionKind::D3D12ExecutableTable:
    case SectionKind::D3D12RasterCommandTable:
    case SectionKind::D3D12DescriptorPlan:
    case SectionKind::D3D12DynamicSlotTable:
    case SectionKind::D3D12ExternalSlotTable:
    case SectionKind::D3D12SurfaceSlotTable:
    case SectionKind::D3D12VertexElementTable:
    case SectionKind::D3D12AttachmentOperationTable:
    case SectionKind::D3D12RootParameterTable:
    case SectionKind::D3D12ComputeExecutableTable:
    case SectionKind::D3D12ComputeCommandTable:
    case SectionKind::D3D12CompositionEndpointTable:
    case SectionKind::StringTable:
    case SectionKind::DebugMap:
    case SectionKind::Provenance:
        return true;
    default:
        return false;
    }
}

PackageBuildInput CopyPackageSections(
    const FrozenExecutablePackage& package,
    std::uint32_t targetSchemaVersion,
    std::uint32_t minimumRuntimeVersion,
    bool omitCompositionSection)
{
    PackageBuildInput input;
    input.targetKind = package.Header().targetKind;
    input.targetSchemaVersion = targetSchemaVersion;
    input.minimumRuntimeVersion = minimumRuntimeVersion;
    input.flags = package.Header().flags;
    for (const auto& section : package.Sections())
    {
        if (omitCompositionSection && section.descriptor.sectionKind == CompositionEndpointTableSection)
            continue;
        PackageSectionInput copy;
        copy.kind = section.descriptor.sectionKind;
        copy.schemaVersion = section.descriptor.schemaVersion;
        copy.flags = section.descriptor.flags;
        copy.alignment = section.descriptor.alignment;
        copy.elementCount = section.descriptor.elementCount;
        copy.elementStride = section.descriptor.elementStride;
        copy.bytes.assign(section.bytes.begin(), section.bytes.end());
        input.sections.push_back(std::move(copy));
    }
    return input;
}

base::Result<std::vector<std::byte>, PackageError> RebuildSchema17Base(
    const FrozenExecutablePackage& schema18Package)
{
    auto input = CopyPackageSections(schema18Package, 17, 17, true);
    return PackageWriter::Write(std::move(input));
}
}

StableEndpointKey ComputeStableEndpointKey(std::string_view authorKey)
{
    constexpr std::string_view Domain = "SGE4.Schema18.CompositionEndpoint.v1:";
    std::vector<std::byte> material;
    material.reserve(Domain.size() + authorKey.size());
    const auto domainBytes = std::as_bytes(std::span(Domain.data(), Domain.size()));
    const auto keyBytes = std::as_bytes(std::span(authorKey.data(), authorKey.size()));
    material.insert(material.end(), domainBytes.begin(), domainBytes.end());
    material.insert(material.end(), keyBytes.begin(), keyBytes.end());
    return base::Sha256(material);
}

bool IsZeroStableEndpointKey(const StableEndpointKey& key) noexcept
{
    return std::all_of(key.begin(), key.end(), [](std::byte value) { return value == std::byte{0}; });
}

base::Result<std::vector<std::byte>, PackageError> BuildCompositionLeafPackage(
    const FrozenExecutablePackage& schema17Package,
    std::span<const LeafCompositionEndpointArtifact> endpoints)
{
    if (schema17Package.Target() != TargetKindD3D12 ||
        schema17Package.Header().targetSchemaVersion != 17 ||
        schema17Package.Header().minimumRuntimeVersion != 17)
        return base::Result<std::vector<std::byte>, PackageError>::Failure(
            Error(PackageErrorCode::UnsupportedTargetSchema,
                  "Schema 18 composition wrapping requires a validated D3D12 Schema 17 Leaf"));

    auto baseView = d3d12_v13::D3D12PackageView::Decode(schema17Package);
    if (!baseView)
        return base::Result<std::vector<std::byte>, PackageError>::Failure(baseView.Error());
    auto validation = ValidateEndpoints(baseView.Value(), endpoints);
    if (!validation)
        return base::Result<std::vector<std::byte>, PackageError>::Failure(validation.Error());

    auto input = CopyPackageSections(schema17Package, TargetSchemaVersion, MinimumRuntimeVersion, false);
    PackageSectionInput endpointSection;
    endpointSection.kind = CompositionEndpointTableSection;
    endpointSection.schemaVersion = CompositionEndpointSectionSchemaVersion;
    endpointSection.flags = EndpointSectionFlags;
    endpointSection.alignment = 8;
    endpointSection.elementCount = static_cast<std::uint32_t>(endpoints.size());
    endpointSection.elementStride = CompositionEndpointStride;
    endpointSection.bytes = EncodeEndpoints(endpoints);
    input.sections.push_back(std::move(endpointSection));

    auto bytes = PackageWriter::Write(std::move(input));
    if (!bytes) return bytes;
    auto frozen = PackageReader::Read(bytes.Value());
    if (!frozen)
        return base::Result<std::vector<std::byte>, PackageError>::Failure(frozen.Error());
    auto decoded = CompositionLeafView::Decode(frozen.Value());
    if (!decoded)
        return base::Result<std::vector<std::byte>, PackageError>::Failure(decoded.Error());
    return bytes;
}

base::Result<CompositionLeafView, PackageError> CompositionLeafView::Decode(
    const FrozenExecutablePackage& package)
{
    if (package.Target() != TargetKindD3D12 ||
        package.Header().targetSchemaVersion != TargetSchemaVersion)
        return base::Result<CompositionLeafView, PackageError>::Failure(
            Error(PackageErrorCode::UnsupportedTargetSchema,
                  "package is not a D3D12 Schema 18 composition Leaf"));
    if (package.Header().minimumRuntimeVersion != MinimumRuntimeVersion)
        return base::Result<CompositionLeafView, PackageError>::Failure(
            Error(PackageErrorCode::TargetCapabilityMismatch,
                  "Schema 18 composition Leaf must require Package Runtime 18"));

    for (const auto& section : package.Sections())
        if (!IsAllowedSchema18Section(section.descriptor.sectionKind))
            return base::Result<CompositionLeafView, PackageError>::Failure(
                Error(PackageErrorCode::UnknownRequiredSection,
                      "Schema 18 composition Leaf contains an unknown Section"));

    const auto* endpointSection = package.FindSection(CompositionEndpointTableSection);
    if (endpointSection == nullptr)
        return base::Result<CompositionLeafView, PackageError>::Failure(
            Error(PackageErrorCode::MissingRequiredSection,
                  "Schema 18 composition endpoint table is missing"));
    auto endpoints = DecodeEndpoints(*endpointSection);
    if (!endpoints)
        return base::Result<CompositionLeafView, PackageError>::Failure(endpoints.Error());

    auto baseBytes = RebuildSchema17Base(package);
    if (!baseBytes)
        return base::Result<CompositionLeafView, PackageError>::Failure(baseBytes.Error());
    auto basePackage = PackageReader::Read(baseBytes.Value());
    if (!basePackage)
        return base::Result<CompositionLeafView, PackageError>::Failure(basePackage.Error());
    auto baseView = d3d12_v13::D3D12PackageView::Decode(basePackage.Value());
    if (!baseView)
        return base::Result<CompositionLeafView, PackageError>::Failure(baseView.Error());
    auto validation = ValidateEndpoints(baseView.Value(), endpoints.Value());
    if (!validation)
        return base::Result<CompositionLeafView, PackageError>::Failure(validation.Error());

    CompositionLeafView output;
    output.endpoints_ = std::move(endpoints).Value();
    output.schema17BasePackageBytes_ = std::move(baseBytes).Value();
    return base::Result<CompositionLeafView, PackageError>::Success(std::move(output));
}
}
