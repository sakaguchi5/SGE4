#pragma once

#include "../../src/composition/artifact/VerifiedCompositionArtifact.h"
#include "../../src/canonical/base/Expected.h"
#include "./leaf/SemanticBuilder.h"
#include "../../src/leaf/toolchain/compiler/LeafCompiler.h"
#include "../../src/composition/model/CompositionContract.h"
#include "../../src/composition/model/plan/CompositionPlan.h"
#include "../../src/composition/planner/CompositionPlanner.h"
#include "../../src/composition/verifier/CompositionVerifier.h"
#include "../../src/composition/toolchain/CompositionToolchain.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace sge4::qualification::canonical_runtime_fixture
{
namespace sem = sge4::semantic;
namespace contract = sge4::composition;
namespace planning = sge4::composition::planning;
namespace verification = sge4::composition::verification;
namespace artifact = sge4::composition::artifact;

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

inline base::Expected<CompiledLeaf, std::string> BuildTransformLeaf()
{
    sem::SemanticBuilder builder;
    auto input = builder.AddExternalBuffer("F9.Transform.Input", 16, 16);
    auto output = builder.AddExternalBuffer("F9.Transform.Output", 16, 16);
    if (!input || !output)
        return base::Failure<CompiledLeaf, std::string>(
            "Bufferが検証または実行の契約に違反しています。");
    auto inputUse = builder.AddUse(
        input.value(), sem::Effect::Read, sem::ViewRole::ShaderBuffer);
    auto outputUse = builder.AddUse(
        output.value(), sem::Effect::Write, sem::ViewRole::StorageBuffer);
    if (!inputUse || !outputUse)
        return base::Failure<CompiledLeaf, std::string>(
            "ResourceUseが検証または実行の契約に違反しています。");

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
        return base::Failure<CompiledLeaf, std::string>(program.error());
    const std::array operands = {
        sem::WorkOperand{sem::WorkOperandKind::ProgramParameter, inputUse.value(), {0}},
        sem::WorkOperand{sem::WorkOperandKind::ProgramParameter, outputUse.value(), {1}}};
    auto work = builder.AddComputeWorkGeneric(
        "F9.Transform.Work", program.value(), operands, 1, 1, 1);
    if (!work)
        return base::Failure<CompiledLeaf, std::string>(work.error());

    auto compiled = sge4::compiler::CompileCanonical(
        std::move(builder).Build(), ComputeProfile(4));
    if (!compiled)
        return base::Failure<CompiledLeaf, std::string>(
            compiled.error().stage + "：" + compiled.error().message);
    return base::Success<CompiledLeaf, std::string>(
        {std::move(compiled).value().packageBytes});
}

inline base::Expected<CompiledLeaf, std::string> BuildMergeLeaf()
{
    sem::SemanticBuilder builder;
    auto inputA = builder.AddExternalBuffer("F9.Merge.InputA", 16, 16);
    auto inputB = builder.AddExternalBuffer("F9.Merge.InputB", 16, 16);
    auto output = builder.AddExternalBuffer("F9.Merge.Output", 16, 16);
    if (!inputA || !inputB || !output)
        return base::Failure<CompiledLeaf, std::string>(
            "Bufferが検証または実行の契約に違反しています。");
    auto useA = builder.AddUse(
        inputA.value(), sem::Effect::Read, sem::ViewRole::ShaderBuffer);
    auto useB = builder.AddUse(
        inputB.value(), sem::Effect::Read, sem::ViewRole::ShaderBuffer);
    auto useOutput = builder.AddUse(
        output.value(), sem::Effect::Write, sem::ViewRole::StorageBuffer);
    if (!useA || !useB || !useOutput)
        return base::Failure<CompiledLeaf, std::string>(
            "ResourceUseが検証または実行の契約に違反しています。");

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
        return base::Failure<CompiledLeaf, std::string>(program.error());
    const std::array operands = {
        sem::WorkOperand{sem::WorkOperandKind::ProgramParameter, useA.value(), {0}},
        sem::WorkOperand{sem::WorkOperandKind::ProgramParameter, useB.value(), {1}},
        sem::WorkOperand{sem::WorkOperandKind::ProgramParameter, useOutput.value(), {2}}};
    auto work = builder.AddComputeWorkGeneric(
        "F9.Merge.Work", program.value(), operands, 1, 1, 1);
    if (!work)
        return base::Failure<CompiledLeaf, std::string>(work.error());

    auto compiled = sge4::compiler::CompileCanonical(
        std::move(builder).Build(), ComputeProfile(8));
    if (!compiled)
        return base::Failure<CompiledLeaf, std::string>(
            compiled.error().stage + "：" + compiled.error().message);
    return base::Success<CompiledLeaf, std::string>(
        {std::move(compiled).value().packageBytes});
}

inline base::Expected<CompiledLeaf, std::string> BuildPresenterLeaf()
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
        return base::Failure<CompiledLeaf, std::string>(
            "Resourceが検証または実行の契約に違反しています。");

    auto vertexUse = builder.AddUse(
        vertex.value(), sem::Effect::Read, sem::ViewRole::VertexData);
    auto inputUse = builder.AddUse(
        input.value(), sem::Effect::Read, sem::ViewRole::ShaderBuffer);
    auto colorUse = builder.AddUse(
        surface.value(), sem::Effect::Write, sem::ViewRole::ColorAttachment);
    auto presentUse = builder.AddUse(
        surface.value(), sem::Effect::Read, sem::ViewRole::PresentSource);
    if (!vertexUse || !inputUse || !colorUse || !presentUse)
        return base::Failure<CompiledLeaf, std::string>(
            "ResourceUseが検証または実行の契約に違反しています。");

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
        return base::Failure<CompiledLeaf, std::string>(program.error());
    const std::array operands = {
        sem::WorkOperand{sem::WorkOperandKind::VertexData, vertexUse.value(), {}},
        sem::WorkOperand{sem::WorkOperandKind::ProgramParameter, inputUse.value(), {0}},
        sem::WorkOperand{sem::WorkOperandKind::ColorAttachment, colorUse.value(), {}},
        sem::WorkOperand{sem::WorkOperandKind::PresentSource, presentUse.value(), {}}};
    auto work = builder.AddRasterWorkGeneric(
        "F9.Presenter.Work", program.value(), operands, 3);
    if (!work)
        return base::Failure<CompiledLeaf, std::string>(work.error());

    auto profile = ComputeProfile(4);
    profile.computeQueueCount = 0;
    profile.surfaceImageCount = 2;
    profile.rtvDescriptorCount = 2;
    auto compiled = sge4::compiler::CompileCanonical(
        std::move(builder).Build(), profile);
    if (!compiled)
        return base::Failure<CompiledLeaf, std::string>(
            compiled.error().stage + "：" + compiled.error().message);
    return base::Success<CompiledLeaf, std::string>(
        {std::move(compiled).value().packageBytes});
}

inline contract::LeafPackageDeclaration TransformDeclaration(std::string key, const CompiledLeaf& leaf)
{
    return {std::move(key), leaf.packageBytes,
        {{0, std::string(InputEndpoint)}, {1, std::string(OutputEndpoint)}}};
}
inline contract::LeafPackageDeclaration MergeDeclaration(std::string key, const CompiledLeaf& leaf)
{
    return {std::move(key), leaf.packageBytes,
        {{0, std::string(InputAEndpoint)}, {1, std::string(InputBEndpoint)}, {2, std::string(OutputEndpoint)}}};
}
inline contract::LeafPackageDeclaration PresenterDeclaration(std::string key, const CompiledLeaf& leaf)
{
    return {std::move(key), leaf.packageBytes, {{0, std::string(InputEndpoint)}}};
}

inline contract::EndpointReferenceDeclaration Ref(
    std::string leaf, std::string endpoint)
{
    return {std::move(leaf), std::move(endpoint)};
}

inline base::Expected<std::vector<std::byte>, std::string>
Freeze(contract::ContractBuildInput input)
{
    auto frozen = contract::BuildFrozenCompositionPackage(
        std::move(input), {1, 1});
    if (!frozen)
        return base::Failure<std::vector<std::byte>, std::string>(
            frozen.error().stage + "：" + frozen.error().message);
    const auto bytes = frozen.value().FileBytes();
    return base::Success<std::vector<std::byte>, std::string>(
        std::vector<std::byte>(bytes.begin(), bytes.end()));
}

inline base::Expected<std::vector<std::byte>, std::string>
BuildDiamondArtifact()
{
    auto transform = BuildTransformLeaf();
    auto merge = BuildMergeLeaf();
    if (!transform || !merge)
        return base::Failure<std::vector<std::byte>, std::string>(
            transform ? merge.error() : transform.error());

    contract::ContractBuildInput input;
    input.leaves = {
        TransformDeclaration("f9/diamond/source", transform.value()),
        TransformDeclaration("f9/diamond/left", transform.value()),
        TransformDeclaration("f9/diamond/right", transform.value()),
        MergeDeclaration("f9/diamond/merge", merge.value())};

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

inline base::Expected<std::vector<std::byte>, std::string>
BuildLinearArtifact()
{
    auto transform = BuildTransformLeaf();
    if (!transform)
        return base::Failure<std::vector<std::byte>, std::string>(
            transform.error());
    contract::ContractBuildInput input;
    input.leaves = {
        TransformDeclaration("f9/linear/first", transform.value()),
        TransformDeclaration("f9/linear/second", transform.value())};

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

inline base::Expected<std::vector<std::byte>, std::string>
BuildFanoutArtifact()
{
    auto transform = BuildTransformLeaf();
    if (!transform)
        return base::Failure<std::vector<std::byte>, std::string>(
            transform.error());
    contract::ContractBuildInput input;
    input.leaves = {
        TransformDeclaration("f9/fanout/source", transform.value()),
        TransformDeclaration("f9/fanout/left", transform.value()),
        TransformDeclaration("f9/fanout/right", transform.value())};

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

inline base::Expected<std::vector<std::byte>, std::string>
BuildMultiInputArtifact()
{
    auto transform = BuildTransformLeaf();
    auto merge = BuildMergeLeaf();
    if (!transform || !merge)
        return base::Failure<std::vector<std::byte>, std::string>(
            transform ? merge.error() : transform.error());
    contract::ContractBuildInput input;
    input.leaves = {
        TransformDeclaration("f9/multi/source/a", transform.value()),
        TransformDeclaration("f9/multi/source/b", transform.value()),
        MergeDeclaration("f9/multi/merge", merge.value())};

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

inline base::Expected<std::vector<std::byte>, std::string>
BuildIndependentArtifact()
{
    auto transform = BuildTransformLeaf();
    if (!transform)
        return base::Failure<std::vector<std::byte>, std::string>(
            transform.error());
    contract::ContractBuildInput input;
    input.leaves = {
        TransformDeclaration("f9/independent/a", transform.value()),
        TransformDeclaration("f9/independent/b", transform.value())};

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

inline base::Expected<std::vector<std::byte>, std::string>
BuildPresenterArtifact()
{
    auto transform = BuildTransformLeaf();
    auto presenter = BuildPresenterLeaf();
    if (!transform || !presenter)
        return base::Failure<std::vector<std::byte>, std::string>(
            transform ? presenter.error() : transform.error());
    contract::ContractBuildInput input;
    input.leaves = {
        TransformDeclaration("f9/present/source", transform.value()),
        PresenterDeclaration("f9/present/presenter", presenter.value())};

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

inline contract::ResourceFlowId FindResourceFlow(std::span<const std::byte> bytes, std::string_view authorKey)
{
    auto artifact = artifact::ReadVerifiedFrozenComposition(bytes);
    if (!artifact) return {};
    const auto key = contract::ComputeStableResourceKey(authorKey);
    const auto& resources = artifact.value().ValidatedContract().Contract().resources;
    const auto found = std::find_if(resources.begin(), resources.end(), [&](const auto& value){ return value.stableKey == key; });
    return found == resources.end() ? contract::ResourceFlowId{} : found->id;
}
inline contract::LeafPackageId FindLeaf(std::span<const std::byte> bytes, std::string_view authorKey)
{
    auto artifact = artifact::ReadVerifiedFrozenComposition(bytes);
    if (!artifact) return {};
    const auto key = contract::ComputeStableLeafKey(authorKey);
    const auto& leaves = artifact.value().ValidatedContract().Contract().leaves;
    const auto found = std::find_if(leaves.begin(), leaves.end(), [&](const auto& value){ return value.stableKey == key; });
    return found == leaves.end() ? contract::LeafPackageId{} : found->id;
}
inline bool HasPresenter(std::span<const std::byte> bytes)
{
    auto artifact = artifact::ReadVerifiedFrozenComposition(bytes);
    return artifact && artifact.value().ValidatedContract().Contract().presenterLeaf.IsValid();
}
}
