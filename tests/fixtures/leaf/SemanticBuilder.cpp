#include "./SemanticBuilder.h"

#include "../../../src/canonical/base/CheckedMath.h"

#include <algorithm>
#include <limits>
#include <set>
#include <utility>

namespace sge4::semantic
{
namespace
{
template<class Id>
Id NextId(std::size_t size)
{
    return Id{static_cast<std::uint32_t>(size)};
}
}

base::Expected<ResourceId, std::string> SemanticBuilder::AddImmutableBuffer(
    std::string debugName,
    std::uint32_t strideBytes,
    std::span<const std::byte> initialContent)
{
    if (initialContent.empty()) return base::Failure<ResourceId, std::string>("Bufferが検証または実行の契約に違反しています。");
    if (strideBytes == 0 || initialContent.size() % strideBytes != 0)
        return base::Failure<ResourceId, std::string>("Bufferが検証または実行の契約に違反しています。");
    if (initialContent.size() > std::numeric_limits<std::uint32_t>::max())
        return base::Failure<ResourceId, std::string>("検証または実行の契約に違反しています。");

    const auto id = NextId<ResourceId>(graph_.resources.size());
    Resource resource;
    resource.id = id;
    resource.debugName = std::move(debugName);
    resource.kind = ResourceKind::Buffer;
    resource.lifetime = LifetimeIntent::Persistent;
    resource.update = UpdateIntent::Immutable;
    resource.visibility = Visibility::Internal;
    resource.buffer.sizeBytes = initialContent.size();
    resource.buffer.strideBytes = strideBytes;
    resource.initialContent.assign(initialContent.begin(), initialContent.end());
    graph_.resources.push_back(std::move(resource));
    return base::Success<ResourceId, std::string>(id);
}

base::Expected<ResourceId, std::string> SemanticBuilder::AddExternalBuffer(
    std::string debugName,
    std::uint64_t sizeBytes,
    std::uint32_t strideBytes)
{
    if (sizeBytes == 0 || strideBytes == 0 || sizeBytes % strideBytes != 0)
        return base::Failure<ResourceId, std::string>("Bufferが検証または実行の契約に違反しています。");
    const auto id = NextId<ResourceId>(graph_.resources.size());
    Resource resource;
    resource.id = id;
    resource.debugName = std::move(debugName);
    resource.kind = ResourceKind::Buffer;
    resource.lifetime = LifetimeIntent::External;
    resource.update = UpdateIntent::External;
    resource.visibility = Visibility::Published;
    resource.buffer.sizeBytes = sizeBytes;
    resource.buffer.strideBytes = strideBytes;
    graph_.resources.push_back(std::move(resource));
    return base::Success<ResourceId, std::string>(id);
}

base::Expected<ResourceId, std::string> SemanticBuilder::AddPreparationBuffer(
    std::string debugName,
    std::uint32_t strideBytes,
    std::span<const std::byte> initialContent)
{
    if (initialContent.empty()) return base::Failure<ResourceId, std::string>("Bufferが検証または実行の契約に違反しています。");
    if (strideBytes == 0 || initialContent.size() % strideBytes != 0)
        return base::Failure<ResourceId, std::string>("Bufferが検証または実行の契約に違反しています。");
    if (initialContent.size() > std::numeric_limits<std::uint32_t>::max())
        return base::Failure<ResourceId, std::string>("検証または実行の契約に違反しています。");

    const auto id = NextId<ResourceId>(graph_.resources.size());
    Resource resource;
    resource.id = id;
    resource.debugName = std::move(debugName);
    resource.kind = ResourceKind::Buffer;
    resource.lifetime = LifetimeIntent::Preparation;
    resource.update = UpdateIntent::Immutable;
    resource.visibility = Visibility::Internal;
    resource.buffer.sizeBytes = initialContent.size();
    resource.buffer.strideBytes = strideBytes;
    resource.initialContent.assign(initialContent.begin(), initialContent.end());
    graph_.resources.push_back(std::move(resource));
    return base::Success<ResourceId, std::string>(id);
}

base::Expected<ResourceId, std::string> SemanticBuilder::AddImmutableTexture2D(
    std::string debugName,
    std::uint32_t width,
    std::uint32_t height,
    FormatMeaning formatMeaning,
    std::uint32_t rowBytes,
    std::span<const std::byte> initialContent)
{
    if (width == 0 || height == 0) return base::Failure<ResourceId, std::string>("Textureが検証または実行の契約に違反しています。");
    if (formatMeaning != FormatMeaning::Bgra8Unorm)
        return base::Failure<ResourceId, std::string>("Slice 12の不変Texture2D uploadが対応する形式はBgra8Unormのみです。Depthの初期データは禁止されています。");
    if (rowBytes == 0) return base::Failure<ResourceId, std::string>("Textureが検証または実行の契約に違反しています。");
    const std::uint64_t expectedBytes = static_cast<std::uint64_t>(rowBytes) * height;
    if (expectedBytes != initialContent.size())
        return base::Failure<ResourceId, std::string>("Textureが検証または実行の契約に違反しています。");

    const auto id = NextId<ResourceId>(graph_.resources.size());
    Resource resource;
    resource.id = id;
    resource.debugName = std::move(debugName);
    resource.kind = ResourceKind::Texture2D;
    resource.lifetime = LifetimeIntent::Persistent;
    resource.update = UpdateIntent::Immutable;
    resource.visibility = Visibility::Internal;
    resource.texture2D.extentMeaning = TextureExtentMeaning::Fixed;
    resource.texture2D.width = width;
    resource.texture2D.height = height;
    resource.texture2D.formatMeaning = formatMeaning;
    resource.texture2D.rowBytes = rowBytes;
    resource.texture2D.mipLevels = 1;
    resource.initialContent.assign(initialContent.begin(), initialContent.end());
    graph_.resources.push_back(std::move(resource));
    return base::Success<ResourceId, std::string>(id);
}

base::Expected<ResourceId, std::string> SemanticBuilder::AddExternalTexture2D(
    std::string debugName,
    std::uint32_t width,
    std::uint32_t height,
    FormatMeaning formatMeaning,
    std::uint32_t rowBytes)
{
    if (width == 0 || height == 0 ||
        width > std::numeric_limits<std::uint32_t>::max() / 4u ||
        formatMeaning != FormatMeaning::Bgra8Unorm ||
        static_cast<std::uint64_t>(rowBytes) != static_cast<std::uint64_t>(width) * 4u)
        return base::Failure<ResourceId, std::string>(
            "Textureが検証または実行の契約に違反しています。");
    const auto id = NextId<ResourceId>(graph_.resources.size());
    Resource resource;
    resource.id = id;
    resource.debugName = std::move(debugName);
    resource.kind = ResourceKind::Texture2D;
    resource.lifetime = LifetimeIntent::External;
    resource.update = UpdateIntent::External;
    resource.visibility = Visibility::Published;
    resource.texture2D.extentMeaning = TextureExtentMeaning::Fixed;
    resource.texture2D.width = width;
    resource.texture2D.height = height;
    resource.texture2D.formatMeaning = formatMeaning;
    resource.texture2D.rowBytes = rowBytes;
    resource.texture2D.mipLevels = 1;
    graph_.resources.push_back(std::move(resource));
    return base::Success<ResourceId, std::string>(id);
}

base::Expected<ResourceId, std::string> SemanticBuilder::AddDepthAttachmentTexture2D(
    std::string debugName,
    FormatMeaning formatMeaning)
{
    if (formatMeaning != FormatMeaning::Depth32Float)
        return base::Failure<ResourceId, std::string>("検証または実行の契約に違反しています。");

    const auto id = NextId<ResourceId>(graph_.resources.size());
    Resource resource;
    resource.id = id;
    resource.debugName = std::move(debugName);
    resource.kind = ResourceKind::Texture2D;
    resource.lifetime = LifetimeIntent::Persistent;
    resource.update = UpdateIntent::GpuWritten;
    resource.visibility = Visibility::Internal;
    resource.texture2D.extentMeaning = TextureExtentMeaning::PresentationSurface;
    resource.texture2D.formatMeaning = formatMeaning;
    resource.texture2D.mipLevels = 1;
    graph_.resources.push_back(std::move(resource));
    return base::Success<ResourceId, std::string>(id);
}

base::Expected<ResourceId, std::string> SemanticBuilder::AddTemporalGpuWrittenBuffer(
    std::string debugName,
    std::uint32_t strideBytes,
    std::span<const std::byte> initialContent)
{
    if (initialContent.empty() || strideBytes == 0 || initialContent.size() % strideBytes != 0)
        return base::Failure<ResourceId, std::string>("Bufferが検証または実行の契約に違反しています。");
    if (initialContent.size() > std::numeric_limits<std::uint32_t>::max())
        return base::Failure<ResourceId, std::string>("検証または実行の契約に違反しています。");

    const auto id = NextId<ResourceId>(graph_.resources.size());
    Resource resource;
    resource.id = id;
    resource.debugName = std::move(debugName);
    resource.kind = ResourceKind::Buffer;
    resource.lifetime = LifetimeIntent::Temporal;
    resource.update = UpdateIntent::GpuWritten;
    resource.visibility = Visibility::Internal;
    resource.buffer.sizeBytes = initialContent.size();
    resource.buffer.strideBytes = strideBytes;
    resource.initialContent.assign(initialContent.begin(), initialContent.end());
    graph_.resources.push_back(std::move(resource));
    return base::Success<ResourceId, std::string>(id);
}

base::Expected<ResourceId, std::string> SemanticBuilder::AddPersistentGpuWrittenBuffer(
    std::string debugName,
    std::uint64_t sizeBytes,
    std::uint32_t strideBytes)
{
    if (sizeBytes == 0 || strideBytes == 0 || sizeBytes % strideBytes != 0)
        return base::Failure<ResourceId, std::string>("Bufferが検証または実行の契約に違反しています。");
    if (sizeBytes > std::numeric_limits<std::uint32_t>::max())
        return base::Failure<ResourceId, std::string>("検証または実行の契約に違反しています。");

    const auto id = NextId<ResourceId>(graph_.resources.size());
    Resource resource;
    resource.id = id;
    resource.debugName = std::move(debugName);
    resource.kind = ResourceKind::Buffer;
    resource.lifetime = LifetimeIntent::Persistent;
    resource.update = UpdateIntent::GpuWritten;
    resource.visibility = Visibility::Internal;
    resource.buffer.sizeBytes = sizeBytes;
    resource.buffer.strideBytes = strideBytes;
    graph_.resources.push_back(std::move(resource));
    return base::Success<ResourceId, std::string>(id);
}

base::Expected<ResourceId, std::string> SemanticBuilder::AddGpuWrittenBuffer(
    std::string debugName,
    std::uint64_t sizeBytes,
    std::uint32_t strideBytes)
{
    if (sizeBytes == 0 || strideBytes == 0 || sizeBytes % strideBytes != 0)
        return base::Failure<ResourceId, std::string>("Bufferが検証または実行の契約に違反しています。");
    if (sizeBytes > std::numeric_limits<std::uint32_t>::max())
        return base::Failure<ResourceId, std::string>("検証または実行の契約に違反しています。");

    const auto id = NextId<ResourceId>(graph_.resources.size());
    Resource resource;
    resource.id = id;
    resource.debugName = std::move(debugName);
    resource.kind = ResourceKind::Buffer;
    resource.lifetime = LifetimeIntent::FrameLocal;
    resource.update = UpdateIntent::GpuWritten;
    resource.visibility = Visibility::Internal;
    resource.buffer.sizeBytes = sizeBytes;
    resource.buffer.strideBytes = strideBytes;
    graph_.resources.push_back(std::move(resource));
    return base::Success<ResourceId, std::string>(id);
}

base::Expected<ResourceId, std::string> SemanticBuilder::AddDynamicBuffer(
    std::string debugName,
    std::uint64_t requiredBytes,
    std::uint32_t strideBytes,
    std::uint32_t requiredAlignment)
{
    if (requiredBytes == 0 || strideBytes == 0 || requiredBytes % strideBytes != 0)
        return base::Failure<ResourceId, std::string>("Bufferが検証または実行の契約に違反しています。");
    if (!base::IsPowerOfTwo(requiredAlignment))
        return base::Failure<ResourceId, std::string>("Bufferが検証または実行の契約に違反しています。");

    const auto id = NextId<ResourceId>(graph_.resources.size());
    Resource resource;
    resource.id = id;
    resource.debugName = std::move(debugName);
    resource.kind = ResourceKind::Buffer;
    resource.lifetime = LifetimeIntent::FrameLocal;
    resource.update = UpdateIntent::DynamicPerFrame;
    resource.visibility = Visibility::Internal;
    resource.buffer.sizeBytes = requiredBytes;
    resource.buffer.strideBytes = strideBytes;
    resource.dynamicData.requiredBytes = requiredBytes;
    resource.dynamicData.requiredAlignment = requiredAlignment;
    graph_.resources.push_back(std::move(resource));
    return base::Success<ResourceId, std::string>(id);
}

base::Expected<ResourceId, std::string> SemanticBuilder::AddPresentationSurface(
    std::string debugName,
    FormatMeaning formatMeaning)
{
    if (formatMeaning != FormatMeaning::Bgra8Unorm)
        return base::Failure<ResourceId, std::string>("Surfaceが検証または実行の契約に違反しています。");
    const auto id = NextId<ResourceId>(graph_.resources.size());
    Resource resource;
    resource.id = id;
    resource.debugName = std::move(debugName);
    resource.kind = ResourceKind::SurfaceImage;
    resource.lifetime = LifetimeIntent::External;
    resource.update = UpdateIntent::External;
    resource.visibility = Visibility::Published;
    resource.surface.formatMeaning = formatMeaning;
    graph_.resources.push_back(std::move(resource));
    return base::Success<ResourceId, std::string>(id);
}

base::Expected<void, std::string> SemanticBuilder::SetAliasPreparation(
    ResourceId target,
    ResourceId preparation)
{
    if (!target.IsValid() || target.value >= graph_.resources.size() ||
        !preparation.IsValid() || preparation.value >= graph_.resources.size())
        return base::Failure<void, std::string>("Resourceが検証または実行の契約に違反しています。");
    if (target == preparation)
        return base::Failure<void, std::string>("Resourceが検証または実行の契約に違反しています。");
    if (graph_.resources[preparation.value].lifetime != LifetimeIntent::Preparation)
        return base::Failure<void, std::string>("Resourceが検証または実行の契約に違反しています。");
    if (graph_.resources[target.value].lifetime == LifetimeIntent::Preparation ||
        graph_.resources[target.value].lifetime == LifetimeIntent::External)
        return base::Failure<void, std::string>("Packageが検証または実行の契約に違反しています。");
    if (graph_.resources[target.value].aliasPreparation.IsValid())
        return base::Failure<void, std::string>("検証または実行の契約に違反しています。");
    graph_.resources[target.value].aliasPreparation = preparation;
    return base::Success<void, std::string>();
}

base::Expected<ResourceUseId, std::string> SemanticBuilder::AddUse(
    ResourceId resource,
    Effect effect,
    ViewRole role,
    TemporalRelation temporalRelation)
{
    if (!resource.IsValid() || resource.value >= graph_.resources.size())
        return base::Failure<ResourceUseId, std::string>("Resourceが検証または実行の契約に違反しています。");
    const auto id = NextId<ResourceUseId>(graph_.resourceUses.size());
    graph_.resourceUses.push_back(ResourceUse{id, resource, effect, role, temporalRelation});
    return base::Success<ResourceUseId, std::string>(id);
}

namespace
{
bool ValidateParameterIds(const ProgramInterface& interfaceDescription)
{
    std::set<std::uint32_t> identities;
    for (const auto& parameter : interfaceDescription.parameters)
        if (!parameter.id.IsValid() ||
            parameter.id.value >= interfaceDescription.parameters.size() ||
            !identities.insert(parameter.id.value).second)
            return false;
    return identities.size() == interfaceDescription.parameters.size();
}
}

base::Expected<ProgramId, std::string> SemanticBuilder::AddRasterProgram(
    std::string debugName,
    ProgramInterface interfaceDescription,
    ProgramSource source)
{
    if (interfaceDescription.vertexInputs.empty() || interfaceDescription.vertexStrideBytes == 0)
        return base::Failure<ProgramId, std::string>("Programが検証または実行の契約に違反しています。");
    if (!ValidateParameterIds(interfaceDescription))
        return base::Failure<ProgramId, std::string>("ProgramがCanonicalな順序または識別子規則に違反しています。");
    if (source.hlslSource.empty() || source.vertexEntry.empty() || source.pixelEntry.empty())
        return base::Failure<ProgramId, std::string>("Programが検証または実行の契約に違反しています。");
    const auto id = NextId<ProgramId>(graph_.programs.size());
    graph_.programs.push_back(Program{id, std::move(debugName), ProgramKind::Raster, std::move(interfaceDescription), std::move(source)});
    return base::Success<ProgramId, std::string>(id);
}

base::Expected<ProgramId, std::string> SemanticBuilder::AddComputeProgram(
    std::string debugName,
    ProgramInterface interfaceDescription,
    ProgramSource source)
{
    if (!interfaceDescription.vertexInputs.empty() || interfaceDescription.vertexStrideBytes != 0)
        return base::Failure<ProgramId, std::string>("Programが検証または実行の契約に違反しています。");
    if (!ValidateParameterIds(interfaceDescription))
        return base::Failure<ProgramId, std::string>("ProgramがCanonicalな順序または識別子規則に違反しています。");
    if (source.hlslSource.empty() || source.computeEntry.empty())
        return base::Failure<ProgramId, std::string>("Programが検証または実行の契約に違反しています。");
    const auto id = NextId<ProgramId>(graph_.programs.size());
    graph_.programs.push_back(Program{id, std::move(debugName), ProgramKind::Compute, std::move(interfaceDescription), std::move(source)});
    return base::Success<ProgramId, std::string>(id);
}

base::Expected<void, std::string> SemanticBuilder::ValidateOperands(
    ProgramId program,
    std::span<const WorkOperand> operands,
    ProgramKind expectedKind) const
{
    if (!program.IsValid() || program.value >= graph_.programs.size() ||
        graph_.programs[program.value].kind != expectedKind)
        return base::Failure<void, std::string>("Programが検証または実行の契約に違反しています。");
    if (operands.empty())
        return base::Failure<void, std::string>("Workが検証または実行の契約に違反しています。");
    for (const auto& operand : operands)
    {
        if (!operand.use.IsValid() || operand.use.value >= graph_.resourceUses.size())
            return base::Failure<void, std::string>("ResourceUseが検証または実行の契約に違反しています。");
        if (operand.kind == WorkOperandKind::ProgramParameter &&
            (!operand.parameter.IsValid() || operand.parameter.value >= graph_.programs[program.value].interface.parameters.size()))
            return base::Failure<void, std::string>("Programが検証または実行の契約に違反しています。");
        if (operand.kind != WorkOperandKind::ProgramParameter && operand.parameter.IsValid())
            return base::Failure<void, std::string>("Programが検証または実行の契約に違反しています。");
    }
    return base::Success<void, std::string>();
}

base::Expected<WorkId, std::string> SemanticBuilder::AddRasterWorkGeneric(
    std::string debugName,
    ProgramId program,
    std::span<const WorkOperand> operands,
    std::uint32_t vertexCount)
{
    auto valid = ValidateOperands(program, operands, ProgramKind::Raster);
    if (!valid) return base::Failure<WorkId, std::string>(valid.error());
    if (vertexCount == 0)
        return base::Failure<WorkId, std::string>("Workが検証または実行の契約に違反しています。");

    const auto id = NextId<WorkId>(graph_.works.size());
    Work work;
    work.id = id;
    work.debugName = std::move(debugName);
    work.kind = WorkKind::Raster;
    work.operands.assign(operands.begin(), operands.end());
    work.raster.program = program;
    work.raster.vertexCount = vertexCount;
    graph_.works.push_back(std::move(work));
    return base::Success<WorkId, std::string>(id);
}

base::Expected<WorkId, std::string> SemanticBuilder::AddCopyWork(
    std::string debugName,
    ResourceUseId source,
    ResourceUseId destination,
    std::uint64_t bytes)
{
    const auto validUse = [this](ResourceUseId id) { return id.IsValid() && id.value < graph_.resourceUses.size(); };
    if (!validUse(source) || !validUse(destination))
        return base::Failure<WorkId, std::string>("ResourceUseが検証または実行の契約に違反しています。");
    if (bytes == 0)
        return base::Failure<WorkId, std::string>("Workが検証または実行の契約に違反しています。");

    const auto id = NextId<WorkId>(graph_.works.size());
    Work work;
    work.id = id;
    work.debugName = std::move(debugName);
    work.kind = WorkKind::Copy;
    work.operands = {
        WorkOperand{WorkOperandKind::CopySource, source, {}},
        WorkOperand{WorkOperandKind::CopyDestination, destination, {}}};
    work.copy.bytes = bytes;
    graph_.works.push_back(std::move(work));
    return base::Success<WorkId, std::string>(id);
}

base::Expected<WorkId, std::string> SemanticBuilder::AddComputeWorkGeneric(
    std::string debugName,
    ProgramId program,
    std::span<const WorkOperand> operands,
    std::uint32_t threadGroupCountX,
    std::uint32_t threadGroupCountY,
    std::uint32_t threadGroupCountZ)
{
    auto valid = ValidateOperands(program, operands, ProgramKind::Compute);
    if (!valid) return base::Failure<WorkId, std::string>(valid.error());
    if (threadGroupCountX == 0 || threadGroupCountY == 0 || threadGroupCountZ == 0)
        return base::Failure<WorkId, std::string>("Workが検証または実行の契約に違反しています。");

    const auto id = NextId<WorkId>(graph_.works.size());
    Work work;
    work.id = id;
    work.debugName = std::move(debugName);
    work.kind = WorkKind::Compute;
    work.operands.assign(operands.begin(), operands.end());
    work.compute.program = program;
    work.compute.threadGroupCountX = threadGroupCountX;
    work.compute.threadGroupCountY = threadGroupCountY;
    work.compute.threadGroupCountZ = threadGroupCountZ;
    graph_.works.push_back(std::move(work));
    return base::Success<WorkId, std::string>(id);
}

base::Expected<WorkId, std::string> SemanticBuilder::AddPresentWork(
    std::string debugName,
    ResourceUseId presentSource)
{
    if (!presentSource.IsValid() || presentSource.value >= graph_.resourceUses.size())
        return base::Failure<WorkId, std::string>("ResourceUseが検証または実行の契約に違反しています。");
    const auto id = NextId<WorkId>(graph_.works.size());
    Work work;
    work.id = id;
    work.debugName = std::move(debugName);
    work.kind = WorkKind::Present;
    work.operands = {WorkOperand{WorkOperandKind::PresentSource, presentSource, {}}};
    graph_.works.push_back(std::move(work));
    return base::Success<WorkId, std::string>(id);
}

base::Expected<void, std::string> SemanticBuilder::AddDependency(
    WorkId predecessor,
    WorkId successor)
{
    if (!predecessor.IsValid() || !successor.IsValid() || predecessor == successor ||
        predecessor.value >= graph_.works.size() || successor.value >= graph_.works.size())
        return base::Failure<void, std::string>("Workが検証または実行の契約に違反しています。");
    auto& dependencies = graph_.works[successor.value].dependencies;
    if (std::find(dependencies.begin(), dependencies.end(), predecessor) != dependencies.end())
        return base::Failure<void, std::string>("入力または内部状態が検証または実行の契約に違反しています。");
    dependencies.push_back(predecessor);
    return base::Success<void, std::string>();
}

SemanticGraph SemanticBuilder::Build() &&
{
    return std::move(graph_);
}
}
