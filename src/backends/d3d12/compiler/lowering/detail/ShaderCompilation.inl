constexpr std::uint64_t DefaultPlacementAlignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
constexpr std::uint64_t ConstantPlacementAlignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;

CompileError Error(std::string stage, std::string message)
{
    return {std::move(stage), std::move(message)};
}

template<class T>
base::Expected<T, CompileError> Failure(std::string stage, std::string message)
{
    return base::Failure<T, CompileError>(Error(std::move(stage), std::move(message)));
}

std::string UpperAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char value) {
        return static_cast<char>(std::toupper(value));
    });
    return value;
}

const char* StageName(semantic::ShaderStage stage) noexcept
{
    switch (stage)
    {
    case semantic::ShaderStage::Vertex: return "Vertex";
    case semantic::ShaderStage::Pixel: return "Pixel";
    case semantic::ShaderStage::Compute: return "Compute";
    default: return "Unknown";
    }
}

const char* ParameterKindName(semantic::ProgramParameterKind kind) noexcept
{
    switch (kind)
    {
    case semantic::ProgramParameterKind::ConstantBuffer: return "ConstantBuffer";
    case semantic::ProgramParameterKind::SampledTexture: return "SampledTexture";
    case semantic::ProgramParameterKind::ReadOnlyBuffer: return "ReadOnlyBuffer";
    case semantic::ProgramParameterKind::UnorderedBuffer: return "UnorderedBuffer";
    case semantic::ProgramParameterKind::UnorderedTexture2D: return "UnorderedTexture2D";
    default: return "Unknown";
    }
}

base::Expected<ReflectedBindingKind, CompileError> ReflectedKind(
    const D3D11_SHADER_INPUT_BIND_DESC& binding,
    semantic::ShaderStage stage)
{
    switch (binding.Type)
    {
    case D3D_SIT_CBUFFER:
        return base::Success<ReflectedBindingKind, CompileError>(
            ReflectedBindingKind::ConstantBuffer);
    case D3D_SIT_TEXTURE:
        if (binding.Dimension == D3D_SRV_DIMENSION_TEXTURE2D)
            return base::Success<ReflectedBindingKind, CompileError>(
                ReflectedBindingKind::SampledTexture);
        if (binding.Dimension == D3D_SRV_DIMENSION_BUFFER ||
            binding.Dimension == D3D_SRV_DIMENSION_BUFFEREX)
            return base::Success<ReflectedBindingKind, CompileError>(
                ReflectedBindingKind::ReadOnlyBuffer);
        break;
    case D3D_SIT_STRUCTURED:
    case D3D_SIT_BYTEADDRESS:
        return base::Success<ReflectedBindingKind, CompileError>(
            ReflectedBindingKind::ReadOnlyBuffer);
    case D3D_SIT_UAV_RWTYPED:
        if (binding.Dimension == D3D_SRV_DIMENSION_TEXTURE2D)
            return base::Success<ReflectedBindingKind, CompileError>(
                ReflectedBindingKind::UnorderedTexture2D);
        if (binding.Dimension == D3D_SRV_DIMENSION_BUFFER ||
            binding.Dimension == D3D_SRV_DIMENSION_BUFFEREX)
            return base::Success<ReflectedBindingKind, CompileError>(
                ReflectedBindingKind::UnorderedBuffer);
        break;
    case D3D_SIT_UAV_RWSTRUCTURED:
    case D3D_SIT_UAV_RWBYTEADDRESS:
    case D3D_SIT_UAV_APPEND_STRUCTURED:
    case D3D_SIT_UAV_CONSUME_STRUCTURED:
    case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
        return base::Success<ReflectedBindingKind, CompileError>(
            ReflectedBindingKind::UnorderedBuffer);
    case D3D_SIT_SAMPLER:
        return base::Success<ReflectedBindingKind, CompileError>(
            ReflectedBindingKind::StaticSampler);
    default:
        break;
    }
    std::ostringstream message;
    message << StageName(stage) << "Resourceが検証または実行の契約に違反しています。"
            << binding.BindPoint;
    return Failure<ReflectedBindingKind>("shader-reflection", message.str());
}

base::Expected<semantic::VertexInput::Meaning, CompileError> VertexMeaning(
    const D3D11_SIGNATURE_PARAMETER_DESC& input)
{
    const auto semanticName = UpperAscii(input.SemanticName ? input.SemanticName : "");
    if (semanticName == "POSITION")
        return base::Success<semantic::VertexInput::Meaning, CompileError>(
            semantic::VertexInput::Meaning::Position);
    if (semanticName == "COLOR")
        return base::Success<semantic::VertexInput::Meaning, CompileError>(
            semantic::VertexInput::Meaning::Color);
    if (semanticName == "TEXCOORD")
        return base::Success<semantic::VertexInput::Meaning, CompileError>(
            semantic::VertexInput::Meaning::TexCoord);
    return Failure<semantic::VertexInput::Meaning>(
        "shader-reflection", "Shaderが検証または実行の契約に違反しています。" + semanticName);
}

std::optional<semantic::ProgramParameterKind> SemanticKind(ReflectedBindingKind kind) noexcept
{
    switch (kind)
    {
    case ReflectedBindingKind::ConstantBuffer:
        return semantic::ProgramParameterKind::ConstantBuffer;
    case ReflectedBindingKind::SampledTexture:
        return semantic::ProgramParameterKind::SampledTexture;
    case ReflectedBindingKind::ReadOnlyBuffer:
        return semantic::ProgramParameterKind::ReadOnlyBuffer;
    case ReflectedBindingKind::UnorderedBuffer:
        return semantic::ProgramParameterKind::UnorderedBuffer;
    case ReflectedBindingKind::UnorderedTexture2D:
        return semantic::ProgramParameterKind::UnorderedTexture2D;
    default:
        return std::nullopt;
    }
}

base::Expected<void, CompileError> ValidateReflectedInterface(
    const semantic::Program& program,
    const CompiledShaderStage& shader)
{
    std::map<std::pair<std::uint16_t, std::uint32_t>, const semantic::ProgramParameter*> expected;
    bool expectsStaticSampler = false;
    for (const auto& parameter : program.interface.parameters)
    {
        if (parameter.stage != shader.stage) continue;
        expected[{static_cast<std::uint16_t>(parameter.kind), parameter.shaderRegister}] = &parameter;
        if (parameter.kind == semantic::ProgramParameterKind::SampledTexture)
            expectsStaticSampler = true;
    }

    std::set<std::pair<std::uint16_t, std::uint32_t>> reflected;
    std::uint32_t samplerCount = 0;
    for (const auto& binding : shader.bindings)
    {
        if (binding.kind == ReflectedBindingKind::StaticSampler)
        {
            ++samplerCount;
            if (shader.stage != semantic::ShaderStage::Pixel || binding.shaderRegister != 0 ||
                binding.bindCount != 1)
                return Failure<void>("shader-reflection",
                    "入力または内部状態が検証または実行の契約に違反しています。");
            continue;
        }
        const auto semanticKind = SemanticKind(binding.kind);
        if (!semanticKind)
            return Failure<void>("shader-reflection", "Programが検証または実行の契約に違反しています。");
        const auto key = std::pair{static_cast<std::uint16_t>(*semanticKind), binding.shaderRegister};
        if (!reflected.insert(key).second)
            return Failure<void>("shader-reflection", "Shaderが検証または実行の契約に違反しています。");
        const auto found = expected.find(key);
        if (found == expected.end())
        {
            std::ostringstream message;
            message << StageName(shader.stage) << "Shaderが検証または実行の契約に違反しています。" << ParameterKindName(*semanticKind)
                    << "入力または内部状態が検証または実行の契約に違反しています。" << binding.shaderRegister
                    << "Programが検証または実行の契約に違反しています。";
            return Failure<void>("shader-reflection", message.str());
        }
        if (binding.bindCount != 1)
            return Failure<void>("shader-reflection", "Shaderが検証または実行の契約に違反しています。");
        if (*semanticKind == semantic::ProgramParameterKind::ConstantBuffer &&
            binding.requiredBytes != found->second->requiredBytes)
        {
            std::ostringstream message;
            message << StageName(shader.stage) << "Bufferが検証または実行の契約に違反しています。" << binding.shaderRegister
                    << "入力または内部状態が検証または実行の契約に違反しています。" << binding.requiredBytes << "Programが検証または実行の契約に違反しています。"
                    << found->second->requiredBytes;
            return Failure<void>("shader-reflection", message.str());
        }
    }
    if (reflected.size() != expected.size())
    {
        for (const auto& [key, parameter] : expected)
            if (!reflected.contains(key))
            {
                std::ostringstream message;
                message << StageName(shader.stage) << "Programが検証または実行の契約に違反しています。"
                        << parameter->debugName << "入力または内部状態が検証または実行の契約に違反しています。" << parameter->shaderRegister
                        << "Shaderが検証または実行の契約に違反しています。";
                return Failure<void>("shader-reflection", message.str());
            }
    }
    if ((expectsStaticSampler && samplerCount != 1) || (!expectsStaticSampler && samplerCount != 0))
        return Failure<void>("shader-reflection",
            "Textureが検証または実行の契約に違反しています。");

    if (shader.stage == semantic::ShaderStage::Vertex)
    {
        if (shader.vertexInputs.size() != program.interface.vertexInputs.size())
            return Failure<void>("shader-reflection",
                "Shaderが検証または実行の契約に違反しています。");
        std::map<semantic::VertexInput::Meaning, std::uint16_t> expectedInputs;
        for (const auto& input : program.interface.vertexInputs)
            expectedInputs[input.meaning] = input.componentCount;
        for (const auto& reflectedInput : shader.vertexInputs)
        {
            const auto found = expectedInputs.find(reflectedInput.meaning);
            if (found == expectedInputs.end() || found->second != reflectedInput.componentCount ||
                reflectedInput.semanticIndex != 0)
                return Failure<void>("shader-reflection",
                    "Shaderが検証または実行の契約に違反しています。");
        }
    }
    else if (!shader.vertexInputs.empty())
    {
        return Failure<void>("shader-reflection", "検証または実行の契約に違反しています。");
    }
    return base::Success<void, CompileError>();
}

base::Expected<CompiledShaderStage, CompileError> CompileAndReflectShader(
    const semantic::Program& program,
    semantic::ShaderStage shaderStage,
    const std::string& entry,
    const char* profile)
{
    ComPtr<ID3DBlob> shader;
    ComPtr<ID3DBlob> errors;
    constexpr UINT flags = D3DCOMPILE_ENABLE_STRICTNESS |
                           D3DCOMPILE_WARNINGS_ARE_ERRORS |
                           D3DCOMPILE_OPTIMIZATION_LEVEL3;
    const HRESULT result = D3DCompile(
        program.source.hlslSource.data(), program.source.hlslSource.size(), nullptr, nullptr, nullptr,
        entry.c_str(), profile, flags, 0, &shader, &errors);
    if (FAILED(result))
    {
        std::string message = "D3DCompileに失敗しました";
        if (errors)
            message.assign(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize());
        return Failure<CompiledShaderStage>("shader-compilation", std::move(message));
    }

    ComPtr<ID3D11ShaderReflection> reflection;
    const HRESULT reflectionResult = D3DReflect(
        shader->GetBufferPointer(), shader->GetBufferSize(), IID_ID3D11ShaderReflection,
        reinterpret_cast<void**>(reflection.GetAddressOf()));
    if (FAILED(reflectionResult) || !reflection)
        return Failure<CompiledShaderStage>("shader-reflection", "入力または内部状態が検証または実行の契約に違反しています。");

    D3D11_SHADER_DESC shaderDescription{};
    if (FAILED(reflection->GetDesc(&shaderDescription)))
        return Failure<CompiledShaderStage>("shader-reflection", "Shaderが検証または実行の契約に違反しています。");

    CompiledShaderStage output;
    output.stage = shaderStage;
    for (UINT index = 0; index < shaderDescription.BoundResources; ++index)
    {
        D3D11_SHADER_INPUT_BIND_DESC binding{};
        if (FAILED(reflection->GetResourceBindingDesc(index, &binding)))
            return Failure<CompiledShaderStage>("shader-reflection", "Resourceが検証または実行の契約に違反しています。");
        auto kind = ReflectedKind(binding, shaderStage);
        if (!kind) return base::Failure<CompiledShaderStage, CompileError>(kind.error());
        ReflectedBinding reflectedBinding;
        reflectedBinding.kind = kind.value();
        reflectedBinding.stage = shaderStage;
        reflectedBinding.shaderRegister = binding.BindPoint;
        reflectedBinding.bindCount = binding.BindCount;
        if (binding.Type == D3D_SIT_CBUFFER)
        {
            auto* constantBuffer = reflection->GetConstantBufferByName(binding.Name);
            D3D11_SHADER_BUFFER_DESC bufferDescription{};
            if (constantBuffer == nullptr || FAILED(constantBuffer->GetDesc(&bufferDescription)))
                return Failure<CompiledShaderStage>("shader-reflection", "Bufferが検証または実行の契約に違反しています。");
            reflectedBinding.requiredBytes = bufferDescription.Size;
        }
        output.bindings.push_back(reflectedBinding);
    }
    std::sort(output.bindings.begin(), output.bindings.end(), [](const auto& left, const auto& right) {
        return std::tuple{left.kind, left.shaderRegister} < std::tuple{right.kind, right.shaderRegister};
    });

    if (shaderStage == semantic::ShaderStage::Vertex)
    {
        for (UINT index = 0; index < shaderDescription.InputParameters; ++index)
        {
            D3D11_SIGNATURE_PARAMETER_DESC input{};
            if (FAILED(reflection->GetInputParameterDesc(index, &input)))
                return Failure<CompiledShaderStage>("shader-reflection", "入力または内部状態が検証または実行の契約に違反しています。");
            if (input.SystemValueType != D3D_NAME_UNDEFINED) continue;
            auto meaning = VertexMeaning(input);
            if (!meaning) return base::Failure<CompiledShaderStage, CompileError>(meaning.error());
            output.vertexInputs.push_back({meaning.value(),
                static_cast<std::uint16_t>(std::popcount(static_cast<unsigned int>(input.Mask & 0x0f))),
                static_cast<std::uint16_t>(input.SemanticIndex)});
        }
    }

    auto interfaceValidation = ValidateReflectedInterface(program, output);
    if (!interfaceValidation)
        return base::Failure<CompiledShaderStage, CompileError>(interfaceValidation.error());

    ComPtr<ID3DBlob> stripped;
    constexpr UINT stripFlags = D3DCOMPILER_STRIP_REFLECTION_DATA |
                                D3DCOMPILER_STRIP_DEBUG_INFO |
                                D3DCOMPILER_STRIP_TEST_BLOBS |
                                D3DCOMPILER_STRIP_PRIVATE_DATA;
    if (FAILED(D3DStripShader(shader->GetBufferPointer(), shader->GetBufferSize(), stripFlags, &stripped)))
        return Failure<CompiledShaderStage>("shader-compilation", "Shaderが検証または実行の契約に違反しています。");
    const auto* begin = static_cast<const std::byte*>(stripped->GetBufferPointer());
    output.bytecode.assign(begin, begin + stripped->GetBufferSize());
    return base::Success<CompiledShaderStage, CompileError>(std::move(output));
}

base::Expected<std::vector<std::byte>, CompileError> SerializeRootSignature(
    const D3D12_ROOT_SIGNATURE_DESC& description,
    const char* stage)
{
    ComPtr<ID3DBlob> blob;
    ComPtr<ID3DBlob> errors;
    const HRESULT result = D3D12SerializeRootSignature(
        &description, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errors);
    if (FAILED(result))
    {
        std::string message = "D3D12SerializeRootSignatureに失敗しました";
        if (errors)
            message.assign(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize());
        return Failure<std::vector<std::byte>>(stage, std::move(message));
    }
    const auto* begin = static_cast<const std::byte*>(blob->GetBufferPointer());
    return base::Success<std::vector<std::byte>, CompileError>(
        std::vector<std::byte>(begin, begin + blob->GetBufferSize()));
}

enum class NativeBindingKind { Constant, ShaderResource, UnorderedAccess };
struct NativeBinding final
{
    NativeBindingKind kind = NativeBindingKind::Constant;
    std::uint32_t shaderRegister = 0;
    semantic::ShaderStage stage = semantic::ShaderStage::Vertex;
};

base::Expected<std::vector<std::byte>, CompileError> SerializeBindingLayout(
    std::span<const NativeBinding> bindings,
    bool raster,
    bool staticSampler)
{
    std::size_t descriptorCount = 0;
    for (const auto& binding : bindings)
        if (binding.kind != NativeBindingKind::Constant) ++descriptorCount;

    std::vector<D3D12_DESCRIPTOR_RANGE> ranges(descriptorCount);
    std::vector<D3D12_ROOT_PARAMETER> parameters(bindings.size());
    std::size_t rangeIndex = 0;
    for (std::size_t index = 0; index < bindings.size(); ++index)
    {
        const auto& binding = bindings[index];
        auto& parameter = parameters[index];
        parameter.ShaderVisibility = binding.stage == semantic::ShaderStage::Vertex ?
            D3D12_SHADER_VISIBILITY_VERTEX :
            binding.stage == semantic::ShaderStage::Pixel ?
            D3D12_SHADER_VISIBILITY_PIXEL : D3D12_SHADER_VISIBILITY_ALL;
        if (binding.kind == NativeBindingKind::Constant)
        {
            parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
            parameter.Descriptor.ShaderRegister = binding.shaderRegister;
            parameter.Descriptor.RegisterSpace = 0;
            continue;
        }
        auto& range = ranges[rangeIndex++];
        range.RangeType = binding.kind == NativeBindingKind::ShaderResource ?
            D3D12_DESCRIPTOR_RANGE_TYPE_SRV : D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        range.NumDescriptors = 1;
        range.BaseShaderRegister = binding.shaderRegister;
        range.RegisterSpace = 0;
        range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameter.DescriptorTable.NumDescriptorRanges = 1;
        parameter.DescriptorTable.pDescriptorRanges = &range;
    }

    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.MaxAnisotropy = 1;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
    sampler.MinLOD = 0.0f;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = raster ? D3D12_SHADER_VISIBILITY_PIXEL : D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC description{};
    description.NumParameters = static_cast<UINT>(parameters.size());
    description.pParameters = parameters.empty() ? nullptr : parameters.data();
    description.NumStaticSamplers = staticSampler ? 1u : 0u;
    description.pStaticSamplers = staticSampler ? &sampler : nullptr;
    description.Flags = raster ? D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT :
                                 D3D12_ROOT_SIGNATURE_FLAG_NONE;
    return SerializeRootSignature(description, raster ? "raster-root-signature" : "compute-root-signature");
}

