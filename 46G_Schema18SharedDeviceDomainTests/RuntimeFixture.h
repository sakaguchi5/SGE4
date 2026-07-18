#pragma once

#include "../00_Foundation/Result.h"
#include "../03_SemanticBuilder/SemanticBuilder.h"
#include "../12A_CompositionLeafCompiler/CompositionLeafCompiler.h"
#include "../16A_Schema18CompositionContract/Schema18CompositionContract.h"
#include "../17A_Schema18ResourceFlowPlan/Schema18ResourceFlowPlan.h"
#include "../18A_Schema18ResourceFlowVerifier/Schema18ResourceFlowVerifier.h"
#include "../19A_Schema18CompositionLinker/Schema18CompositionLinker.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace sge4::qualification::schema18_runtime_fixture
{
namespace sem = sge4::semantic;
namespace compile18 = sge4::compiler::composition;
namespace contract = sge4::composition::schema18;
namespace planning = sge4::composition::schema18::planning;
namespace verification = sge4::composition::schema18::verification;
namespace linking = sge4::composition::schema18::linking;

inline constexpr std::string_view InputEndpoint = "runtime/input";
inline constexpr std::string_view OutputEndpoint = "runtime/output";
inline constexpr std::string_view InputAEndpoint = "runtime/input/a";
inline constexpr std::string_view InputBEndpoint = "runtime/input/b";

struct CompiledLeaf final
{
    std::vector<std::byte> packageBytes;
};

inline target::D3D12TargetProfile ComputeProfile(std::uint32_t descriptors)
{
    target::D3D12TargetProfile profile;
    profile.directQueueCount = 1;
    profile.computeQueueCount = 1;
    profile.copyQueueCount = 0;
    profile.surfaceImageCount = 0;
    profile.rtvDescriptorCount = 0;
    profile.dsvDescriptorCount = 0;
    profile.shaderDescriptorCount = descriptors;
    return profile;
}

inline base::Result<CompiledLeaf, std::string> BuildTransformLeaf()
{
    sem::SemanticBuilder builder;
    auto input = builder.AddExternalBuffer("F9.Transform.Input", 16, 16);
    auto output = builder.AddExternalBuffer("F9.Transform.Output", 16, 16);
    if (!input || !output)
        return base::Result<CompiledLeaf, std::string>::Failure(
            "transform external Buffer construction failed");
    auto inputUse = builder.AddUse(
        input.Value(), sem::Effect::Read, sem::ViewRole::ShaderBuffer);
    auto outputUse = builder.AddUse(
        output.Value(), sem::Effect::Write, sem::ViewRole::StorageBuffer);
    if (!inputUse || !outputUse)
        return base::Result<CompiledLeaf, std::string>::Failure(
            "transform ResourceUse construction failed");

    sem::ProgramInterface interfaceDescription;
    interfaceDescription.parameters = {
        {{0}, "Input", sem::ProgramParameterKind::ReadOnlyBuffer,
            sem::ShaderStage::Compute, 0, 0, 1},
        {{1}, "Output", sem::ProgramParameterKind::UnorderedBuffer,
            sem::ShaderStage::Compute, 0, 0, 1}};
    auto program = builder.AddComputeProgram(
        "F9.Transform.Program", std::move(interfaceDescription), {R"hlsl(
StructuredBuffer<float4> Input : register(t0);
RWStructuredBuffer<float4> Output : register(u0);
[numthreads(1, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    Output[0] = Input[0] + 1.0f;
}
)hlsl", {}, {}, "CSMain"});
    if (!program)
        return base::Result<CompiledLeaf, std::string>::Failure(program.Error());
    const std::array operands = {
        sem::WorkOperand{sem::WorkOperandKind::ProgramParameter, inputUse.Value(), {0}},
        sem::WorkOperand{sem::WorkOperandKind::ProgramParameter, outputUse.Value(), {1}}};
    auto work = builder.AddComputeWorkGeneric(
        "F9.Transform.Work", program.Value(), operands, 1, 1, 1);
    if (!work)
        return base::Result<CompiledLeaf, std::string>::Failure(work.Error());

    auto compiled = compile18::CompileCompositionLeafCanonical(
        std::move(builder).Build(), ComputeProfile(4),
        {{input.Value(), std::string(InputEndpoint)},
         {output.Value(), std::string(OutputEndpoint)}});
    if (!compiled)
        return base::Result<CompiledLeaf, std::string>::Failure(
            compiled.Error().stage + ": " + compiled.Error().message);
    return base::Result<CompiledLeaf, std::string>::Success(
        {std::move(compiled).Value().packageBytes});
}

inline base::Result<CompiledLeaf, std::string> BuildMergeLeaf()
{
    sem::SemanticBuilder builder;
    auto inputA = builder.AddExternalBuffer("F9.Merge.InputA", 16, 16);
    auto inputB = builder.AddExternalBuffer("F9.Merge.InputB", 16, 16);
    auto output = builder.AddExternalBuffer("F9.Merge.Output", 16, 16);
    if (!inputA || !inputB || !output)
        return base::Result<CompiledLeaf, std::string>::Failure(
            "merge external Buffer construction failed");
    auto useA = builder.AddUse(
        inputA.Value(), sem::Effect::Read, sem::ViewRole::ShaderBuffer);
    auto useB = builder.AddUse(
        inputB.Value(), sem::Effect::Read, sem::ViewRole::ShaderBuffer);
    auto useOutput = builder.AddUse(
        output.Value(), sem::Effect::Write, sem::ViewRole::StorageBuffer);
    if (!useA || !useB || !useOutput)
        return base::Result<CompiledLeaf, std::string>::Failure(
            "merge ResourceUse construction failed");

    sem::ProgramInterface interfaceDescription;
    interfaceDescription.parameters = {
        {{0}, "InputA", sem::ProgramParameterKind::ReadOnlyBuffer,
            sem::ShaderStage::Compute, 0, 0, 1},
        {{1}, "InputB", sem::ProgramParameterKind::ReadOnlyBuffer,
            sem::ShaderStage::Compute, 1, 0, 1},
        {{2}, "Output", sem::ProgramParameterKind::UnorderedBuffer,
            sem::ShaderStage::Compute, 0, 0, 1}};
    auto program = builder.AddComputeProgram(
        "F9.Merge.Program", std::move(interfaceDescription), {R"hlsl(
StructuredBuffer<float4> InputA : register(t0);
StructuredBuffer<float4> InputB : register(t1);
RWStructuredBuffer<float4> Output : register(u0);
[numthreads(1, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    Output[0] = InputA[0] + InputB[0] + 1.0f;
}
)hlsl", {}, {}, "CSMain"});
    if (!program)
        return base::Result<CompiledLeaf, std::string>::Failure(program.Error());
    const std::array operands = {
        sem::WorkOperand{sem::WorkOperandKind::ProgramParameter, useA.Value(), {0}},
        sem::WorkOperand{sem::WorkOperandKind::ProgramParameter, useB.Value(), {1}},
        sem::WorkOperand{sem::WorkOperandKind::ProgramParameter, useOutput.Value(), {2}}};
    auto work = builder.AddComputeWorkGeneric(
        "F9.Merge.Work", program.Value(), operands, 1, 1, 1);
    if (!work)
        return base::Result<CompiledLeaf, std::string>::Failure(work.Error());

    auto compiled = compile18::CompileCompositionLeafCanonical(
        std::move(builder).Build(), ComputeProfile(8),
        {{inputA.Value(), std::string(InputAEndpoint)},
         {inputB.Value(), std::string(InputBEndpoint)},
         {output.Value(), std::string(OutputEndpoint)}});
    if (!compiled)
        return base::Result<CompiledLeaf, std::string>::Failure(
            compiled.Error().stage + ": " + compiled.Error().message);
    return base::Result<CompiledLeaf, std::string>::Success(
        {std::move(compiled).Value().packageBytes});
}

inline base::Result<CompiledLeaf, std::string> BuildPresenterLeaf()
{
    struct Vertex final
    {
        float position[3];
        float color[4];
        float uv[2];
    };
    const std::array vertices = {
        Vertex{{-0.7f, -0.6f, 0.0f}, {1, 0, 0, 1}, {0, 0}},
        Vertex{{ 0.0f,  0.7f, 0.0f}, {0, 1, 0, 1}, {0, 0}},
        Vertex{{ 0.7f, -0.6f, 0.0f}, {0, 0, 1, 1}, {0, 0}}};

    sem::SemanticBuilder builder;
    auto vertex = builder.AddImmutableBuffer(
        "F9.Presenter.Vertices", sizeof(Vertex),
        std::as_bytes(std::span<const Vertex>(vertices)));
    auto input = builder.AddExternalBuffer("F9.Presenter.Input", 16, 16);
    auto surface = builder.AddPresentationSurface(
        "F9.Presenter.Surface", sem::FormatMeaning::Bgra8Unorm);
    if (!vertex || !input || !surface)
        return base::Result<CompiledLeaf, std::string>::Failure(
            "presenter Resource construction failed");

    auto vertexUse = builder.AddUse(
        vertex.Value(), sem::Effect::Read, sem::ViewRole::VertexData);
    auto inputUse = builder.AddUse(
        input.Value(), sem::Effect::Read, sem::ViewRole::ShaderBuffer);
    auto colorUse = builder.AddUse(
        surface.Value(), sem::Effect::Write, sem::ViewRole::ColorAttachment);
    auto presentUse = builder.AddUse(
        surface.Value(), sem::Effect::Read, sem::ViewRole::PresentSource);
    if (!vertexUse || !inputUse || !colorUse || !presentUse)
        return base::Result<CompiledLeaf, std::string>::Failure(
            "presenter ResourceUse construction failed");

    sem::ProgramInterface interfaceDescription;
    interfaceDescription.vertexStrideBytes = sizeof(Vertex);
    interfaceDescription.vertexInputs = {
        {sem::VertexInput::Meaning::Position, 3, 0},
        {sem::VertexInput::Meaning::Color, 4, 12},
        {sem::VertexInput::Meaning::TexCoord, 2, 28}};
    interfaceDescription.parameters = {
        {{0}, "Input", sem::ProgramParameterKind::ReadOnlyBuffer,
            sem::ShaderStage::Pixel, 0, 0, 1}};
    auto program = builder.AddRasterProgram(
        "F9.Presenter.Program", std::move(interfaceDescription), {R"hlsl(
StructuredBuffer<float4> Input : register(t0);
struct VSInput
{
    float3 position : POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
};
struct VSOutput { float4 position : SV_POSITION; };
VSOutput VSMain(VSInput input)
{
    VSOutput output;
    output.position = float4(input.position, 1.0f);
    return output;
}
float4 PSMain(VSOutput input) : SV_TARGET
{
    return Input[0];
}
)hlsl", "VSMain", "PSMain", {}});
    if (!program)
        return base::Result<CompiledLeaf, std::string>::Failure(program.Error());
    const std::array operands = {
        sem::WorkOperand{sem::WorkOperandKind::VertexData, vertexUse.Value(), {}},
        sem::WorkOperand{sem::WorkOperandKind::ProgramParameter, inputUse.Value(), {0}},
        sem::WorkOperand{sem::WorkOperandKind::ColorAttachment, colorUse.Value(), {}},
        sem::WorkOperand{sem::WorkOperandKind::PresentSource, presentUse.Value(), {}}};
    auto work = builder.AddRasterWorkGeneric(
        "F9.Presenter.Work", program.Value(), operands, 3);
    if (!work)
        return base::Result<CompiledLeaf, std::string>::Failure(work.Error());

    auto profile = ComputeProfile(4);
    profile.computeQueueCount = 0;
    profile.surfaceImageCount = 2;
    profile.rtvDescriptorCount = 2;
    auto compiled = compile18::CompileCompositionLeafCanonical(
        std::move(builder).Build(), profile,
        {{input.Value(), std::string(InputEndpoint)}});
    if (!compiled)
        return base::Result<CompiledLeaf, std::string>::Failure(
            compiled.Error().stage + ": " + compiled.Error().message);
    return base::Result<CompiledLeaf, std::string>::Success(
        {std::move(compiled).Value().packageBytes});
}

inline contract::EndpointReferenceDeclaration Ref(
    std::string leaf, std::string endpoint)
{
    return {std::move(leaf), std::move(endpoint)};
}

inline base::Result<linking::FrozenCompositionArtifact, std::string>
Freeze(contract::ContractBuildInput input)
{
    auto built = contract::BuildSchema18CompositionContract(std::move(input));
    if (!built)
        return base::Result<linking::FrozenCompositionArtifact, std::string>::Failure(
            built.Error().stage + ": " + built.Error().message);
    auto proposed = planning::ProposeResourceFlowPlan(built.Value());
    if (!proposed)
        return base::Result<linking::FrozenCompositionArtifact, std::string>::Failure(
            proposed.Error().stage + ": " + proposed.Error().message);
    auto verified = verification::VerifyAndSeal(built.Value(), proposed.Value());
    if (!verified)
        return base::Result<linking::FrozenCompositionArtifact, std::string>::Failure(
            verified.Error().stage + ": " + verified.Error().message);
    auto frozen = linking::FreezeVerifiedComposition(built.Value(), verified.Value());
    if (!frozen)
        return base::Result<linking::FrozenCompositionArtifact, std::string>::Failure(
            frozen.Error().stage + ": " + frozen.Error().message);
    return base::Result<linking::FrozenCompositionArtifact, std::string>::Success(
        std::move(frozen).Value());
}

inline base::Result<linking::FrozenCompositionArtifact, std::string>
BuildDiamondArtifact()
{
    auto transform = BuildTransformLeaf();
    auto merge = BuildMergeLeaf();
    if (!transform || !merge)
        return base::Result<linking::FrozenCompositionArtifact, std::string>::Failure(
            transform ? merge.Error() : transform.Error());

    contract::ContractBuildInput input;
    input.leaves = {
        {"f9/diamond/source", transform.Value().packageBytes},
        {"f9/diamond/left", transform.Value().packageBytes},
        {"f9/diamond/right", transform.Value().packageBytes},
        {"f9/diamond/merge", merge.Value().packageBytes}};

    contract::ResourceFlowDeclaration compositionInput;
    compositionInput.stableKey = "f9/diamond/input";
    compositionInput.boundary = contract::ResourceBoundary::CompositionInput;
    compositionInput.consumers = {Ref("f9/diamond/source", std::string(InputEndpoint))};

    contract::ResourceFlowDeclaration fanout;
    fanout.stableKey = "f9/diamond/fanout";
    fanout.boundary = contract::ResourceBoundary::Internal;
    fanout.producer = Ref("f9/diamond/source", std::string(OutputEndpoint));
    fanout.consumers = {
        Ref("f9/diamond/left", std::string(InputEndpoint)),
        Ref("f9/diamond/right", std::string(InputEndpoint))};

    contract::ResourceFlowDeclaration left;
    left.stableKey = "f9/diamond/left-output";
    left.boundary = contract::ResourceBoundary::Internal;
    left.producer = Ref("f9/diamond/left", std::string(OutputEndpoint));
    left.consumers = {Ref("f9/diamond/merge", std::string(InputAEndpoint))};

    contract::ResourceFlowDeclaration right;
    right.stableKey = "f9/diamond/right-output";
    right.boundary = contract::ResourceBoundary::Internal;
    right.producer = Ref("f9/diamond/right", std::string(OutputEndpoint));
    right.consumers = {Ref("f9/diamond/merge", std::string(InputBEndpoint))};

    contract::ResourceFlowDeclaration output;
    output.stableKey = "f9/diamond/output";
    output.boundary = contract::ResourceBoundary::CompositionOutput;
    output.producer = Ref("f9/diamond/merge", std::string(OutputEndpoint));

    input.resources = {
        std::move(compositionInput), std::move(fanout), std::move(left),
        std::move(right), std::move(output)};
    return Freeze(std::move(input));
}

inline base::Result<linking::FrozenCompositionArtifact, std::string>
BuildLinearArtifact()
{
    auto transform = BuildTransformLeaf();
    if (!transform)
        return base::Result<linking::FrozenCompositionArtifact, std::string>::Failure(
            transform.Error());
    contract::ContractBuildInput input;
    input.leaves = {
        {"f9/linear/first", transform.Value().packageBytes},
        {"f9/linear/second", transform.Value().packageBytes}};

    contract::ResourceFlowDeclaration source;
    source.stableKey = "f9/linear/input";
    source.boundary = contract::ResourceBoundary::CompositionInput;
    source.consumers = {Ref("f9/linear/first", std::string(InputEndpoint))};

    contract::ResourceFlowDeclaration middle;
    middle.stableKey = "f9/linear/middle";
    middle.boundary = contract::ResourceBoundary::Internal;
    middle.producer = Ref("f9/linear/first", std::string(OutputEndpoint));
    middle.consumers = {Ref("f9/linear/second", std::string(InputEndpoint))};

    contract::ResourceFlowDeclaration output;
    output.stableKey = "f9/linear/output";
    output.boundary = contract::ResourceBoundary::CompositionOutput;
    output.producer = Ref("f9/linear/second", std::string(OutputEndpoint));
    input.resources = {std::move(source), std::move(middle), std::move(output)};
    return Freeze(std::move(input));
}

inline base::Result<linking::FrozenCompositionArtifact, std::string>
BuildFanoutArtifact()
{
    auto transform = BuildTransformLeaf();
    if (!transform)
        return base::Result<linking::FrozenCompositionArtifact, std::string>::Failure(
            transform.Error());
    contract::ContractBuildInput input;
    input.leaves = {
        {"f9/fanout/source", transform.Value().packageBytes},
        {"f9/fanout/left", transform.Value().packageBytes},
        {"f9/fanout/right", transform.Value().packageBytes}};

    contract::ResourceFlowDeclaration source;
    source.stableKey = "f9/fanout/input";
    source.boundary = contract::ResourceBoundary::CompositionInput;
    source.consumers = {Ref("f9/fanout/source", std::string(InputEndpoint))};

    contract::ResourceFlowDeclaration shared;
    shared.stableKey = "f9/fanout/shared";
    shared.boundary = contract::ResourceBoundary::Internal;
    shared.producer = Ref("f9/fanout/source", std::string(OutputEndpoint));
    shared.consumers = {
        Ref("f9/fanout/left", std::string(InputEndpoint)),
        Ref("f9/fanout/right", std::string(InputEndpoint))};

    contract::ResourceFlowDeclaration left;
    left.stableKey = "f9/fanout/output/left";
    left.boundary = contract::ResourceBoundary::CompositionOutput;
    left.producer = Ref("f9/fanout/left", std::string(OutputEndpoint));

    contract::ResourceFlowDeclaration right;
    right.stableKey = "f9/fanout/output/right";
    right.boundary = contract::ResourceBoundary::CompositionOutput;
    right.producer = Ref("f9/fanout/right", std::string(OutputEndpoint));
    input.resources = {
        std::move(source), std::move(shared), std::move(left), std::move(right)};
    return Freeze(std::move(input));
}

inline base::Result<linking::FrozenCompositionArtifact, std::string>
BuildMultiInputArtifact()
{
    auto transform = BuildTransformLeaf();
    auto merge = BuildMergeLeaf();
    if (!transform || !merge)
        return base::Result<linking::FrozenCompositionArtifact, std::string>::Failure(
            transform ? merge.Error() : transform.Error());
    contract::ContractBuildInput input;
    input.leaves = {
        {"f9/multi/source/a", transform.Value().packageBytes},
        {"f9/multi/source/b", transform.Value().packageBytes},
        {"f9/multi/merge", merge.Value().packageBytes}};

    for (const auto suffix : {"a", "b"})
    {
        contract::ResourceFlowDeclaration source;
        source.stableKey = std::string("f9/multi/input/") + suffix;
        source.boundary = contract::ResourceBoundary::CompositionInput;
        source.consumers = {
            Ref(std::string("f9/multi/source/") + suffix, std::string(InputEndpoint))};
        input.resources.push_back(std::move(source));
    }

    contract::ResourceFlowDeclaration a;
    a.stableKey = "f9/multi/intermediate/a";
    a.boundary = contract::ResourceBoundary::Internal;
    a.producer = Ref("f9/multi/source/a", std::string(OutputEndpoint));
    a.consumers = {Ref("f9/multi/merge", std::string(InputAEndpoint))};
    input.resources.push_back(std::move(a));

    contract::ResourceFlowDeclaration b;
    b.stableKey = "f9/multi/intermediate/b";
    b.boundary = contract::ResourceBoundary::Internal;
    b.producer = Ref("f9/multi/source/b", std::string(OutputEndpoint));
    b.consumers = {Ref("f9/multi/merge", std::string(InputBEndpoint))};
    input.resources.push_back(std::move(b));

    contract::ResourceFlowDeclaration output;
    output.stableKey = "f9/multi/output";
    output.boundary = contract::ResourceBoundary::CompositionOutput;
    output.producer = Ref("f9/multi/merge", std::string(OutputEndpoint));
    input.resources.push_back(std::move(output));
    return Freeze(std::move(input));
}

inline base::Result<linking::FrozenCompositionArtifact, std::string>
BuildIndependentArtifact()
{
    auto transform = BuildTransformLeaf();
    if (!transform)
        return base::Result<linking::FrozenCompositionArtifact, std::string>::Failure(
            transform.Error());
    contract::ContractBuildInput input;
    input.leaves = {
        {"f9/independent/a", transform.Value().packageBytes},
        {"f9/independent/b", transform.Value().packageBytes}};

    for (const auto suffix : {"a", "b"})
    {
        contract::ResourceFlowDeclaration source;
        source.stableKey = std::string("f9/independent/input/") + suffix;
        source.boundary = contract::ResourceBoundary::CompositionInput;
        source.consumers = {
            Ref(std::string("f9/independent/") + suffix, std::string(InputEndpoint))};
        input.resources.push_back(std::move(source));

        contract::ResourceFlowDeclaration output;
        output.stableKey = std::string("f9/independent/output/") + suffix;
        output.boundary = contract::ResourceBoundary::CompositionOutput;
        output.producer =
            Ref(std::string("f9/independent/") + suffix, std::string(OutputEndpoint));
        input.resources.push_back(std::move(output));
    }
    return Freeze(std::move(input));
}

inline base::Result<linking::FrozenCompositionArtifact, std::string>
BuildPresenterArtifact()
{
    auto transform = BuildTransformLeaf();
    auto presenter = BuildPresenterLeaf();
    if (!transform || !presenter)
        return base::Result<linking::FrozenCompositionArtifact, std::string>::Failure(
            transform ? presenter.Error() : transform.Error());
    contract::ContractBuildInput input;
    input.leaves = {
        {"f9/present/source", transform.Value().packageBytes},
        {"f9/present/presenter", presenter.Value().packageBytes}};

    contract::ResourceFlowDeclaration source;
    source.stableKey = "f9/present/input";
    source.boundary = contract::ResourceBoundary::CompositionInput;
    source.consumers = {
        Ref("f9/present/source", std::string(InputEndpoint))};

    contract::ResourceFlowDeclaration color;
    color.stableKey = "f9/present/color";
    color.boundary = contract::ResourceBoundary::Internal;
    color.producer = Ref("f9/present/source", std::string(OutputEndpoint));
    color.consumers = {
        Ref("f9/present/presenter", std::string(InputEndpoint))};
    input.resources = {std::move(source), std::move(color)};
    return Freeze(std::move(input));
}

inline std::vector<std::byte> Bytes(const std::array<float, 4>& value)
{
    std::vector<std::byte> bytes(sizeof(value));
    std::memcpy(bytes.data(), value.data(), sizeof(value));
    return bytes;
}

inline bool Equals(
    std::span<const std::byte> bytes,
    const std::array<float, 4>& expected,
    float tolerance = 0.0001f)
{
    if (bytes.size() < sizeof(expected)) return false;
    std::array<float, 4> actual{};
    std::memcpy(actual.data(), bytes.data(), sizeof(actual));
    for (std::size_t index = 0; index < actual.size(); ++index)
        if (std::abs(actual[index] - expected[index]) > tolerance) return false;
    return true;
}

inline contract::ResourceFlowId FindResourceFlow(
    const linking::FrozenCompositionArtifact& artifact,
    std::string_view authorKey)
{
    const auto key = contract::ComputeStableResourceKey(authorKey);
    const auto found = std::find_if(
        artifact.contract.resources.begin(), artifact.contract.resources.end(),
        [&](const auto& resource) { return resource.stableKey == key; });
    return found == artifact.contract.resources.end()
        ? contract::ResourceFlowId{} : found->id;
}


inline contract::CompositionEndpointId FindEndpoint(
    const linking::FrozenCompositionArtifact& artifact,
    std::string_view leafAuthorKey,
    std::string_view endpointAuthorKey)
{
    const auto leafKey = contract::ComputeStableLeafKey(leafAuthorKey);
    const auto endpointKey = package::d3d12_v18::ComputeStableEndpointKey(endpointAuthorKey);
    const auto leaf = std::find_if(
        artifact.contract.leaves.begin(), artifact.contract.leaves.end(),
        [&](const auto& value) { return value.stableKey == leafKey; });
    if (leaf == artifact.contract.leaves.end()) return {};
    const auto found = std::find_if(
        artifact.contract.endpoints.begin(), artifact.contract.endpoints.end(),
        [&](const auto& endpoint) {
            return endpoint.leaf == leaf->id && endpoint.stableKey == endpointKey;
        });
    return found == artifact.contract.endpoints.end()
        ? contract::CompositionEndpointId{} : found->id;
}

inline contract::LeafPackageId FindLeaf(
    const linking::FrozenCompositionArtifact& artifact,
    std::string_view authorKey)
{
    const auto key = contract::ComputeStableLeafKey(authorKey);
    const auto found = std::find_if(
        artifact.contract.leaves.begin(), artifact.contract.leaves.end(),
        [&](const auto& leaf) { return leaf.stableKey == key; });
    return found == artifact.contract.leaves.end()
        ? contract::LeafPackageId{} : found->id;
}
}
