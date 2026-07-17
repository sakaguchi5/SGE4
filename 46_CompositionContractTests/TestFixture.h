#pragma once

#include "../03_SemanticBuilder/SemanticBuilder.h"
#include "../12_SGE4Compiler/SGE4Compiler.h"
#include "../16_CompositionContract/CompositionContract.h"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

namespace sge4::qualification::composition_fixture
{
inline base::Result<std::vector<std::byte>, std::string> BuildTransformLeaf()
{
    semantic::SemanticBuilder builder;
    auto input = builder.AddExternalBuffer("L4V1.Input", 16, 16);
    auto output = builder.AddExternalBuffer("L4V1.Output", 16, 16);
    if (!input || !output) return base::Result<std::vector<std::byte>, std::string>::Failure("external Buffer construction failed");
    auto inputUse = builder.AddUse(input.Value(), semantic::Effect::ReadWrite, semantic::ViewRole::StorageBuffer);
    auto outputUse = builder.AddUse(output.Value(), semantic::Effect::Write, semantic::ViewRole::StorageBuffer);
    if (!inputUse || !outputUse) return base::Result<std::vector<std::byte>, std::string>::Failure("external Buffer use construction failed");
    semantic::ProgramInterface interfaceDescription;
    interfaceDescription.parameters = {
        {{0}, "Input", semantic::ProgramParameterKind::UnorderedBuffer, semantic::ShaderStage::Compute, 0, 0, 1},
        {{1}, "Output", semantic::ProgramParameterKind::UnorderedBuffer, semantic::ShaderStage::Compute, 1, 0, 1}};
    auto program = builder.AddComputeProgram("L4V1.Transform", std::move(interfaceDescription), {R"hlsl(
RWStructuredBuffer<float4> Input : register(u0);
RWStructuredBuffer<float4> Output : register(u1);
[numthreads(1, 1, 1)]
void CSTransform(uint3 id : SV_DispatchThreadID) { Output[0] = Input[0] + 1.0f; }
)hlsl", {}, {}, "CSTransform"});
    if (!program) return base::Result<std::vector<std::byte>, std::string>::Failure(program.Error());
    const std::array operands = {
        semantic::WorkOperand{semantic::WorkOperandKind::ProgramParameter, inputUse.Value(), {0}},
        semantic::WorkOperand{semantic::WorkOperandKind::ProgramParameter, outputUse.Value(), {1}}};
    auto work = builder.AddComputeWorkGeneric("L4V1.Transform.Work", program.Value(), operands, 1, 1, 1);
    if (!work) return base::Result<std::vector<std::byte>, std::string>::Failure(work.Error());
    target::D3D12TargetProfile profile;
    profile.computeQueueCount = 1; profile.copyQueueCount = 0; profile.surfaceImageCount = 0; profile.rtvDescriptorCount = 0;
    profile.dsvDescriptorCount = 0; profile.shaderDescriptorCount = 8;
    auto compiled = compiler::CompileCanonical(std::move(builder).Build(), profile);
    if (!compiled) return base::Result<std::vector<std::byte>, std::string>::Failure(compiled.Error().stage + ": " + compiled.Error().message);
    return base::Result<std::vector<std::byte>, std::string>::Success(std::move(compiled).Value().packageBytes);
}

inline base::Result<std::vector<std::byte>, std::string> BuildMergeLeaf()
{
    semantic::SemanticBuilder builder;
    auto inputA = builder.AddExternalBuffer("L4V1.InputA", 16, 16);
    auto inputB = builder.AddExternalBuffer("L4V1.InputB", 16, 16);
    auto output = builder.AddExternalBuffer("L4V1.Output", 16, 16);
    if (!inputA || !inputB || !output)
        return base::Result<std::vector<std::byte>, std::string>::Failure("external Buffer construction failed");
    auto inputAUse = builder.AddUse(inputA.Value(), semantic::Effect::ReadWrite, semantic::ViewRole::StorageBuffer);
    auto inputBUse = builder.AddUse(inputB.Value(), semantic::Effect::ReadWrite, semantic::ViewRole::StorageBuffer);
    auto outputUse = builder.AddUse(output.Value(), semantic::Effect::Write, semantic::ViewRole::StorageBuffer);
    if (!inputAUse || !inputBUse || !outputUse)
        return base::Result<std::vector<std::byte>, std::string>::Failure("external Buffer use construction failed");
    semantic::ProgramInterface interfaceDescription;
    interfaceDescription.parameters = {
        {{0}, "InputA", semantic::ProgramParameterKind::UnorderedBuffer, semantic::ShaderStage::Compute, 0, 0, 1},
        {{1}, "InputB", semantic::ProgramParameterKind::UnorderedBuffer, semantic::ShaderStage::Compute, 1, 0, 1},
        {{2}, "Output", semantic::ProgramParameterKind::UnorderedBuffer, semantic::ShaderStage::Compute, 2, 0, 1}};
    auto program = builder.AddComputeProgram("L4V1.Merge", std::move(interfaceDescription), {R"hlsl(
RWStructuredBuffer<float4> InputA : register(u0);
RWStructuredBuffer<float4> InputB : register(u1);
RWStructuredBuffer<float4> Output : register(u2);
[numthreads(1, 1, 1)]
void CSMerge(uint3 id : SV_DispatchThreadID) { Output[0] = InputA[0] + InputB[0] + 1.0f; }
)hlsl", {}, {}, "CSMerge"});
    if (!program) return base::Result<std::vector<std::byte>, std::string>::Failure(program.Error());
    const std::array operands = {
        semantic::WorkOperand{semantic::WorkOperandKind::ProgramParameter, inputAUse.Value(), {0}},
        semantic::WorkOperand{semantic::WorkOperandKind::ProgramParameter, inputBUse.Value(), {1}},
        semantic::WorkOperand{semantic::WorkOperandKind::ProgramParameter, outputUse.Value(), {2}}};
    auto work = builder.AddComputeWorkGeneric("L4V1.Merge.Work", program.Value(), operands, 1, 1, 1);
    if (!work) return base::Result<std::vector<std::byte>, std::string>::Failure(work.Error());
    target::D3D12TargetProfile profile;
    profile.computeQueueCount = 1; profile.copyQueueCount = 0; profile.surfaceImageCount = 0; profile.rtvDescriptorCount = 0;
    profile.dsvDescriptorCount = 0; profile.shaderDescriptorCount = 8;
    auto compiled = compiler::CompileCanonical(std::move(builder).Build(), profile);
    if (!compiled) return base::Result<std::vector<std::byte>, std::string>::Failure(compiled.Error().stage + ": " + compiled.Error().message);
    return base::Result<std::vector<std::byte>, std::string>::Success(std::move(compiled).Value().packageBytes);
}

inline base::Result<std::vector<std::byte>, std::string> BuildPresenterLeaf()
{
    struct Vertex final { float position[3]; float color[4]; float uv[2]; };
    const std::array vertices = {
        Vertex{{-0.7f,-0.6f,0.0f},{1,0,0,1},{0,0}},
        Vertex{{ 0.0f, 0.7f,0.0f},{0,1,0,1},{0,0}},
        Vertex{{ 0.7f,-0.6f,0.0f},{0,0,1,1},{0,0}}};
    semantic::SemanticBuilder builder;
    auto vertex = builder.AddImmutableBuffer("L4V1.Presenter.Vertex", sizeof(Vertex), std::as_bytes(std::span(vertices)));
    auto shared = builder.AddExternalBuffer("L4V1.Presenter.Input", 16, 16);
    auto surface = builder.AddPresentationSurface("L4V1.Presenter.Surface", semantic::FormatMeaning::Bgra8Unorm);
    if (!vertex || !shared || !surface)
        return base::Result<std::vector<std::byte>, std::string>::Failure("presenter resource construction failed");
    auto computeUse = builder.AddUse(shared.Value(), semantic::Effect::ReadWrite, semantic::ViewRole::StorageBuffer);
    auto vertexUse = builder.AddUse(vertex.Value(), semantic::Effect::Read, semantic::ViewRole::VertexData);
    auto rasterUse = builder.AddUse(shared.Value(), semantic::Effect::Read, semantic::ViewRole::ShaderBuffer);
    auto colorUse = builder.AddUse(surface.Value(), semantic::Effect::Write, semantic::ViewRole::ColorAttachment);
    auto presentUse = builder.AddUse(surface.Value(), semantic::Effect::Read, semantic::ViewRole::PresentSource);
    if (!computeUse || !vertexUse || !rasterUse || !colorUse || !presentUse)
        return base::Result<std::vector<std::byte>, std::string>::Failure("presenter use construction failed");
    const std::string shader = R"hlsl(
RWStructuredBuffer<float4> ComputeInput : register(u0);
StructuredBuffer<float4> RasterInput : register(t0);
struct VSInput { float3 position : POSITION; float4 color : COLOR; float2 uv : TEXCOORD; };
struct VSOutput { float4 position : SV_Position; };
VSOutput VSMain(VSInput input) { VSOutput output; output.position=float4(input.position,1.0f); return output; }
float4 PSMain(VSOutput input) : SV_Target { return RasterInput[0]; }
[numthreads(1,1,1)]
void CSMain(uint3 id : SV_DispatchThreadID) { ComputeInput[0] = ComputeInput[0]; }
)hlsl";
    semantic::ProgramInterface computeInterface;
    computeInterface.parameters = {{{0},"ComputeInput",semantic::ProgramParameterKind::UnorderedBuffer,semantic::ShaderStage::Compute,0,0,1}};
    auto computeProgram = builder.AddComputeProgram("L4V1.Presenter.Compute",std::move(computeInterface),{shader,{},{},"CSMain"});
    if (!computeProgram) return base::Result<std::vector<std::byte>, std::string>::Failure(computeProgram.Error());
    const std::array computeOperands = {semantic::WorkOperand{semantic::WorkOperandKind::ProgramParameter,computeUse.Value(),{0}}};
    auto computeWork = builder.AddComputeWorkGeneric("L4V1.Presenter.Compute.Work",computeProgram.Value(),computeOperands,1,1,1);
    if (!computeWork) return base::Result<std::vector<std::byte>, std::string>::Failure(computeWork.Error());
    semantic::ProgramInterface rasterInterface;
    rasterInterface.vertexStrideBytes=sizeof(Vertex);
    rasterInterface.vertexInputs={{semantic::VertexInput::Meaning::Position,3,0},{semantic::VertexInput::Meaning::Color,4,12},{semantic::VertexInput::Meaning::TexCoord,2,28}};
    rasterInterface.parameters = {{{0},"RasterInput",semantic::ProgramParameterKind::ReadOnlyBuffer,semantic::ShaderStage::Pixel,0,0,1}};
    auto rasterProgram = builder.AddRasterProgram("L4V1.Presenter.Raster",std::move(rasterInterface),{shader,"VSMain","PSMain",{}});
    if (!rasterProgram) return base::Result<std::vector<std::byte>, std::string>::Failure(rasterProgram.Error());
    const std::array rasterOperands = {
        semantic::WorkOperand{semantic::WorkOperandKind::VertexData,vertexUse.Value(),{}},
        semantic::WorkOperand{semantic::WorkOperandKind::ProgramParameter,rasterUse.Value(),{0}},
        semantic::WorkOperand{semantic::WorkOperandKind::ColorAttachment,colorUse.Value(),{}},
        semantic::WorkOperand{semantic::WorkOperandKind::PresentSource,presentUse.Value(),{}}};
    auto rasterWork = builder.AddRasterWorkGeneric("L4V1.Presenter.Raster.Work",rasterProgram.Value(),rasterOperands,3);
    if (!rasterWork) return base::Result<std::vector<std::byte>, std::string>::Failure(rasterWork.Error());
    auto dependency = builder.AddDependency(computeWork.Value(),rasterWork.Value());
    if (!dependency) return base::Result<std::vector<std::byte>, std::string>::Failure(dependency.Error());
    target::D3D12TargetProfile profile;
    profile.computeQueueCount=1; profile.copyQueueCount=0; profile.surfaceImageCount=2;
    profile.rtvDescriptorCount=2; profile.dsvDescriptorCount=0; profile.shaderDescriptorCount=8;
    auto compiled=compiler::CompileCanonical(std::move(builder).Build(),profile);
    if(!compiled)return base::Result<std::vector<std::byte>,std::string>::Failure(compiled.Error().stage+": "+compiled.Error().message);
    return base::Result<std::vector<std::byte>,std::string>::Success(std::move(compiled).Value().packageBytes);
}

inline base::Result<composition::PackageCompositionContract, std::string> BuildContract(bool reverseInput = false)
{
    auto bytes = BuildTransformLeaf();
    if (!bytes) return base::Result<composition::PackageCompositionContract, std::string>::Failure(bytes.Error());
    composition::ContractBuildInput input;
    input.leaves = {{1, bytes.Value()}, {2, bytes.Value()}};
    input.endpoints = {{1,0,composition::EndpointDirection::Import},{1,1,composition::EndpointDirection::Export},
                       {2,0,composition::EndpointDirection::Import},{2,1,composition::EndpointDirection::Export}};
    const package::d3d12_v13::ResourceState unordered{package::d3d12_v13::StateClass::Explicit,0,
        static_cast<std::uint32_t>(package::d3d12_v13::ExplicitStateBits::UnorderedWrite)};
    input.resources = {{10,16,package::d3d12_v13::Format::Unknown,unordered,composition::SharedResourceBoundary::CompositionInput},
                       {20,16,package::d3d12_v13::Format::Unknown,unordered,composition::SharedResourceBoundary::Internal},
                       {30,16,package::d3d12_v13::Format::Unknown,unordered,composition::SharedResourceBoundary::CompositionOutput}};
    input.bindings = {{10,1,0},{20,1,1},{20,2,0},{30,2,1}};
    input.links = {{20,1,1,2,0}};
    if (reverseInput)
    {
        std::reverse(input.leaves.begin(), input.leaves.end()); std::reverse(input.endpoints.begin(), input.endpoints.end());
        std::reverse(input.resources.begin(), input.resources.end()); std::reverse(input.bindings.begin(), input.bindings.end());
    }
    auto contract = composition::BuildCompositionContract(std::move(input));
    if (!contract) return base::Result<composition::PackageCompositionContract, std::string>::Failure(contract.Error().stage + ": " + contract.Error().message);
    return base::Result<composition::PackageCompositionContract, std::string>::Success(std::move(contract).Value());
}

inline base::Result<composition::PackageCompositionContract, std::string> BuildDiamondContract()
{
    auto transform = BuildTransformLeaf();
    auto merge = BuildMergeLeaf();
    if (!transform) return base::Result<composition::PackageCompositionContract, std::string>::Failure(transform.Error());
    if (!merge) return base::Result<composition::PackageCompositionContract, std::string>::Failure(merge.Error());

    composition::ContractBuildInput input;
    input.leaves = {{10, transform.Value()}, {20, transform.Value()}, {30, transform.Value()}, {40, merge.Value()}};
    input.endpoints = {
        {10,0,composition::EndpointDirection::Import},{10,1,composition::EndpointDirection::Export},
        {20,0,composition::EndpointDirection::Import},{20,1,composition::EndpointDirection::Export},
        {30,0,composition::EndpointDirection::Import},{30,1,composition::EndpointDirection::Export},
        {40,0,composition::EndpointDirection::Import},{40,1,composition::EndpointDirection::Import},
        {40,2,composition::EndpointDirection::Export}};
    const package::d3d12_v13::ResourceState unordered{package::d3d12_v13::StateClass::Explicit,0,
        static_cast<std::uint32_t>(package::d3d12_v13::ExplicitStateBits::UnorderedWrite)};
    input.resources = {
        {100,16,package::d3d12_v13::Format::Unknown,unordered,composition::SharedResourceBoundary::CompositionInput},
        {200,16,package::d3d12_v13::Format::Unknown,unordered,composition::SharedResourceBoundary::Internal},
        {300,16,package::d3d12_v13::Format::Unknown,unordered,composition::SharedResourceBoundary::Internal},
        {400,16,package::d3d12_v13::Format::Unknown,unordered,composition::SharedResourceBoundary::Internal},
        {500,16,package::d3d12_v13::Format::Unknown,unordered,composition::SharedResourceBoundary::CompositionOutput}};
    input.bindings = {
        {100,10,0},
        {200,10,1},{200,20,0},{200,30,0},
        {300,20,1},{300,40,0},
        {400,30,1},{400,40,1},
        {500,40,2}};
    input.links = {
        {200,10,1,20,0},{200,10,1,30,0},
        {300,20,1,40,0},{400,30,1,40,1}};
    auto contract = composition::BuildCompositionContract(std::move(input));
    if (!contract)
        return base::Result<composition::PackageCompositionContract, std::string>::Failure(contract.Error().stage + ": " + contract.Error().message);
    return base::Result<composition::PackageCompositionContract, std::string>::Success(std::move(contract).Value());
}

inline base::Result<composition::PackageCompositionContract, std::string> BuildPresentContract()
{
    auto producer=BuildTransformLeaf(); auto presenter=BuildPresenterLeaf();
    if(!producer)return base::Result<composition::PackageCompositionContract,std::string>::Failure(producer.Error());
    if(!presenter)return base::Result<composition::PackageCompositionContract,std::string>::Failure(presenter.Error());
    composition::ContractBuildInput input; input.leaves={{1,producer.Value()},{2,presenter.Value()}};
    input.endpoints={{1,0,composition::EndpointDirection::Import},{1,1,composition::EndpointDirection::Export},{2,0,composition::EndpointDirection::Import}};
    const package::d3d12_v13::ResourceState unordered{package::d3d12_v13::StateClass::Explicit,0,
        static_cast<std::uint32_t>(package::d3d12_v13::ExplicitStateBits::UnorderedWrite)};
    input.resources={{10,16,package::d3d12_v13::Format::Unknown,unordered,composition::SharedResourceBoundary::CompositionInput},
                     {20,16,package::d3d12_v13::Format::Unknown,unordered,composition::SharedResourceBoundary::Internal}};
    input.bindings={{10,1,0},{20,1,1},{20,2,0}}; input.links={{20,1,1,2,0}};
    auto contract=composition::BuildCompositionContract(std::move(input));
    if(!contract)return base::Result<composition::PackageCompositionContract,std::string>::Failure(contract.Error().stage+": "+contract.Error().message);
    return base::Result<composition::PackageCompositionContract,std::string>::Success(std::move(contract).Value());
}

inline base::Result<composition::PackageCompositionContract, std::string> BuildIndependentContract()
{
    auto leaf=BuildTransformLeaf(); if(!leaf)return base::Result<composition::PackageCompositionContract,std::string>::Failure(leaf.Error());
    composition::ContractBuildInput input; input.leaves={{1,leaf.Value()},{2,leaf.Value()}};
    input.endpoints={{1,0,composition::EndpointDirection::Import},{1,1,composition::EndpointDirection::Export},
                     {2,0,composition::EndpointDirection::Import},{2,1,composition::EndpointDirection::Export}};
    const package::d3d12_v13::ResourceState unordered{package::d3d12_v13::StateClass::Explicit,0,
        static_cast<std::uint32_t>(package::d3d12_v13::ExplicitStateBits::UnorderedWrite)};
    input.resources={{10,16,package::d3d12_v13::Format::Unknown,unordered,composition::SharedResourceBoundary::CompositionInput},
                     {20,16,package::d3d12_v13::Format::Unknown,unordered,composition::SharedResourceBoundary::CompositionOutput},
                     {30,16,package::d3d12_v13::Format::Unknown,unordered,composition::SharedResourceBoundary::CompositionInput},
                     {40,16,package::d3d12_v13::Format::Unknown,unordered,composition::SharedResourceBoundary::CompositionOutput}};
    input.bindings={{10,1,0},{20,1,1},{30,2,0},{40,2,1}};
    auto contract=composition::BuildCompositionContract(std::move(input));
    if(!contract)return base::Result<composition::PackageCompositionContract,std::string>::Failure(contract.Error().stage+": "+contract.Error().message);
    return base::Result<composition::PackageCompositionContract,std::string>::Success(std::move(contract).Value());
}
}
