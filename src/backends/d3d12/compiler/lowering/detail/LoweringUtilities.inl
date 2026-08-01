pkg::ResourceState Common() noexcept { return {pkg::StateClass::Common, 0, 0}; }
pkg::ResourceState Present() noexcept { return {pkg::StateClass::Present, 0, 0}; }
pkg::ResourceState Explicit(pkg::ExplicitStateBits bits) noexcept
{
    return {pkg::StateClass::Explicit, 0, static_cast<std::uint32_t>(bits)};
}

std::uint64_t AlignUp(std::uint64_t value, std::uint64_t alignment)
{
    return (value + alignment - 1) / alignment * alignment;
}

std::uint64_t AppendAligned(std::vector<std::byte>& destination,
                            std::span<const std::byte> source,
                            std::size_t alignment = 16)
{
    while (destination.size() % alignment != 0) destination.push_back(std::byte{0});
    const auto offset = static_cast<std::uint64_t>(destination.size());
    destination.insert(destination.end(), source.begin(), source.end());
    return offset;
}

const semantic::ProgramParameter* FindParameter(
    const semantic::ProgramInterface& interfaceDescription,
    semantic::ProgramParameterId id) noexcept
{
    const auto found = std::find_if(interfaceDescription.parameters.begin(),
        interfaceDescription.parameters.end(),
        [id](const semantic::ProgramParameter& parameter) { return parameter.id == id; });
    return found == interfaceDescription.parameters.end() ? nullptr : &*found;
}

std::vector<const semantic::ProgramParameter*> CanonicalParameters(
    const semantic::ProgramInterface& interfaceDescription)
{
    std::vector<const semantic::ProgramParameter*> result;
    result.reserve(interfaceDescription.parameters.size());
    for (const auto& parameter : interfaceDescription.parameters) result.push_back(&parameter);
    std::ranges::sort(result, {}, [](const auto* parameter) { return parameter->id.value; });
    return result;
}

std::vector<const semantic::VertexInput*> CanonicalVertexInputs(
    const semantic::ProgramInterface& interfaceDescription)
{
    std::vector<const semantic::VertexInput*> result;
    result.reserve(interfaceDescription.vertexInputs.size());
    for (const auto& input : interfaceDescription.vertexInputs) result.push_back(&input);
    std::ranges::sort(result, {}, [](const auto* input) {
        return std::tuple{input->byteOffset, input->meaning, input->componentCount};
    });
    return result;
}

std::vector<const semantic::WorkOperand*> CanonicalOperands(const semantic::Work& work)
{
    std::vector<const semantic::WorkOperand*> result;
    result.reserve(work.operands.size());
    for (const auto& operand : work.operands) result.push_back(&operand);
    std::ranges::sort(result, {}, [](const auto* operand) {
        return std::tuple{operand->kind, operand->parameter.value, operand->use.value};
    });
    return result;
}

base::Digest256 InterfaceDigest(const semantic::ProgramInterface& value)
{
    base::BinaryWriter writer;
    writer.WriteU32(value.vertexStrideBytes);
    const auto vertexInputs = CanonicalVertexInputs(value);
    writer.WriteCountU32(vertexInputs.size());
    for (const auto* input : vertexInputs)
    {
        writer.WriteU16(static_cast<std::uint16_t>(input->meaning));
        writer.WriteU16(input->componentCount);
        writer.WriteU32(input->byteOffset);
    }
    const auto parameters = CanonicalParameters(value);
    writer.WriteCountU32(parameters.size());
    for (const auto* parameter : parameters)
    {
        writer.WriteU32(parameter->id.value);
        writer.WriteU16(static_cast<std::uint16_t>(parameter->kind));
        writer.WriteU16(static_cast<std::uint16_t>(parameter->stage));
        writer.WriteU32(parameter->shaderRegister);
        writer.WriteU64(parameter->requiredBytes);
        writer.WriteU32(parameter->requiredAlignment);
    }
    return base::Sha256(writer.Bytes());
}

base::Digest256 RasterSpecializationDigest(const pkg::RasterExecutableArtifact& executable)
{
    base::BinaryWriter writer;
    writer.WriteU32(executable.program.value);
    writer.WriteU32(executable.bindingLayout.value);
    writer.WriteU32(executable.vertexElementRange.first);
    writer.WriteU32(executable.vertexElementRange.count);
    writer.WriteU32(static_cast<std::uint32_t>(executable.colorFormat));
    writer.WriteU32(static_cast<std::uint32_t>(executable.depthFormat));
    writer.WriteU32(static_cast<std::uint32_t>(executable.primitiveTopology));
    writer.WriteU32(executable.rasterStateId);
    writer.WriteU32(executable.blendStateId);
    writer.WriteU32(executable.depthStateId);
    writer.WriteU32(executable.sampleCount);
    return base::Sha256(writer.Bytes());
}

base::Digest256 ComputeSpecializationDigest(const pkg::ComputeExecutableArtifact& executable)
{
    base::BinaryWriter writer;
    writer.WriteU32(executable.program.value);
    writer.WriteU32(executable.bindingLayout.value);
    writer.WriteU32(executable.flags);
    return base::Sha256(writer.Bytes());
}

pkg::ResourceKind ResourceKind(semantic::ResourceKind kind)
{
    switch (kind)
    {
    case semantic::ResourceKind::Buffer: return pkg::ResourceKind::Buffer;
    case semantic::ResourceKind::Texture2D: return pkg::ResourceKind::Texture2D;
    case semantic::ResourceKind::SurfaceImage: return pkg::ResourceKind::SurfaceImage;
    default: return pkg::ResourceKind::Buffer;
    }
}

pkg::Format Format(semantic::FormatMeaning format)
{
    switch (format)
    {
    case semantic::FormatMeaning::Bgra8Unorm: return pkg::Format::B8G8R8A8Unorm;
    case semantic::FormatMeaning::Depth32Float: return pkg::Format::D32Float;
    case semantic::FormatMeaning::Rgba32Float: return pkg::Format::R32G32B32A32Float;
    default: return pkg::Format::Unknown;
    }
}

pkg::Format ResourceFormat(const semantic::Resource& resource)
{
    if (resource.kind == semantic::ResourceKind::Texture2D) return Format(resource.texture2D.formatMeaning);
    if (resource.kind == semantic::ResourceKind::SurfaceImage) return Format(resource.surface.formatMeaning);
    return pkg::Format::Unknown;
}

pkg::ViewClass ViewClass(semantic::ViewRole role)
{
    using semantic::ViewRole;
    switch (role)
    {
    case ViewRole::VertexData: return pkg::ViewClass::VertexBuffer;
    case ViewRole::ConstantData: return pkg::ViewClass::ConstantBuffer;
    case ViewRole::SampledTexture:
    case ViewRole::ShaderBuffer: return pkg::ViewClass::ShaderResource;
    case ViewRole::ColorAttachment: return pkg::ViewClass::RenderTarget;
    case ViewRole::DepthAttachment: return pkg::ViewClass::DepthStencil;
    case ViewRole::StorageBuffer:
    case ViewRole::StorageTexture2D: return pkg::ViewClass::UnorderedAccess;
    case ViewRole::CopySource: return pkg::ViewClass::CopySource;
    case ViewRole::CopyDestination: return pkg::ViewClass::CopyDestination;
    case ViewRole::PresentSource: return pkg::ViewClass::PresentSource;
    default: return pkg::ViewClass::ShaderResource;
    }
}

pkg::ResourceState RequiredState(semantic::ViewRole role, semantic::WorkKind workKind)
{
    using semantic::ViewRole;
    switch (role)
    {
    case ViewRole::VertexData: return Explicit(pkg::ExplicitStateBits::VertexBuffer);
    case ViewRole::ConstantData: return Explicit(pkg::ExplicitStateBits::ConstantBuffer);
    case ViewRole::ColorAttachment: return Explicit(pkg::ExplicitStateBits::RenderTarget);
    case ViewRole::DepthAttachment: return Explicit(pkg::ExplicitStateBits::DepthWrite);
    case ViewRole::StorageBuffer:
    case ViewRole::StorageTexture2D: return Explicit(pkg::ExplicitStateBits::UnorderedWrite);
    case ViewRole::CopySource: return Explicit(pkg::ExplicitStateBits::CopySource);
    case ViewRole::CopyDestination: return Explicit(pkg::ExplicitStateBits::CopyDestination);
    case ViewRole::PresentSource: return Present();
    default:
        // Shader visibility is an execution decision and must be frozen into the
        // Package. Dedicated Compute queues cannot consume PIXEL_SHADER_RESOURCE.
        return Explicit(workKind == semantic::WorkKind::Compute
            ? pkg::ExplicitStateBits::NonPixelShaderRead
            : pkg::ExplicitStateBits::PixelShaderRead);
    }
}

bool DescriptorBacked(pkg::ViewClass viewClass)
{
    return viewClass == pkg::ViewClass::ShaderResource ||
           viewClass == pkg::ViewClass::UnorderedAccess ||
           viewClass == pkg::ViewClass::RenderTarget ||
           viewClass == pkg::ViewClass::DepthStencil;
}

pkg::HeapClass HeapClassFor(const semantic::Resource& resource,
                            std::span<const semantic::ResourceUse* const> uses)
{
    if (resource.update == semantic::UpdateIntent::DynamicPerFrame) return pkg::HeapClass::Upload;
    if (resource.kind == semantic::ResourceKind::Buffer) return pkg::HeapClass::DefaultBuffer;
    const bool attachment = std::any_of(uses.begin(), uses.end(), [](const semantic::ResourceUse* use) {
        return use->role == semantic::ViewRole::ColorAttachment ||
               use->role == semantic::ViewRole::DepthAttachment;
    });
    return attachment ? pkg::HeapClass::RenderTargetOrDepth : pkg::HeapClass::DefaultTexture;
}

