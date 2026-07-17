#include "../03_SemanticBuilder/SemanticBuilder.h"
#include "../09_FrozenPackageCore/PackageReader.h"
#include "../09_FrozenPackageCore/PackageWriter.h"
#include "../10_D3D12PackageSchema/D3D12Encoding.h"
#include "../10A_D3D12CompositionSchema18/D3D12CompositionEncoding18.h"
#include "../12_SGE4Compiler/SGE4Compiler.h"
#include "../12A_CompositionLeafCompiler/CompositionLeafCompiler.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
using sge4::base::Result;

Result<sge4::semantic::SemanticGraph, std::string> BuildLeaf(bool readWriteInput)
{
    namespace sem = sge4::semantic;
    sem::SemanticBuilder builder;
    auto input = builder.AddExternalBuffer("F1.Input", 16, 16);
    auto output = builder.AddExternalBuffer("F1.Output", 16, 16);
    if (!input || !output)
        return Result<sem::SemanticGraph, std::string>::Failure("external Buffer creation failed");

    const auto inputEffect = readWriteInput ? sem::Effect::ReadWrite : sem::Effect::Read;
    const auto inputRole = readWriteInput ? sem::ViewRole::StorageBuffer : sem::ViewRole::ShaderBuffer;
    auto inputUse = builder.AddUse(input.Value(), inputEffect, inputRole);
    auto outputUse = builder.AddUse(output.Value(), sem::Effect::Write, sem::ViewRole::StorageBuffer);
    if (!inputUse || !outputUse)
        return Result<sem::SemanticGraph, std::string>::Failure("external Buffer use creation failed");

    sem::ProgramInterface interfaceDescription;
    std::string shader;
    if (readWriteInput)
    {
        interfaceDescription.parameters = {
            {{0}, "Input", sem::ProgramParameterKind::UnorderedBuffer, sem::ShaderStage::Compute, 0, 0, 1},
            {{1}, "Output", sem::ProgramParameterKind::UnorderedBuffer, sem::ShaderStage::Compute, 1, 0, 1}};
        shader = R"hlsl(
RWStructuredBuffer<float4> Input : register(u0);
RWStructuredBuffer<float4> Output : register(u1);
[numthreads(1, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID) { Output[0] = Input[0] + 1.0f; }
)hlsl";
    }
    else
    {
        interfaceDescription.parameters = {
            {{0}, "Input", sem::ProgramParameterKind::ReadOnlyBuffer, sem::ShaderStage::Compute, 0, 0, 1},
            {{1}, "Output", sem::ProgramParameterKind::UnorderedBuffer, sem::ShaderStage::Compute, 0, 0, 1}};
        shader = R"hlsl(
StructuredBuffer<float4> Input : register(t0);
RWStructuredBuffer<float4> Output : register(u0);
[numthreads(1, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID) { Output[0] = Input[0] + 1.0f; }
)hlsl";
    }

    auto program = builder.AddComputeProgram(
        "F1.Transform", std::move(interfaceDescription), {std::move(shader), {}, {}, "CSMain"});
    if (!program)
        return Result<sem::SemanticGraph, std::string>::Failure(program.Error());
    const std::array operands = {
        sem::WorkOperand{sem::WorkOperandKind::ProgramParameter, inputUse.Value(), {0}},
        sem::WorkOperand{sem::WorkOperandKind::ProgramParameter, outputUse.Value(), {1}}};
    auto work = builder.AddComputeWorkGeneric(
        "F1.Transform.Work", program.Value(), operands, 1, 1, 1);
    if (!work)
        return Result<sem::SemanticGraph, std::string>::Failure(work.Error());
    return Result<sem::SemanticGraph, std::string>::Success(std::move(builder).Build());
}

sge4::target::D3D12TargetProfile Profile()
{
    sge4::target::D3D12TargetProfile profile;
    profile.computeQueueCount = 1;
    profile.copyQueueCount = 0;
    profile.surfaceImageCount = 0;
    profile.rtvDescriptorCount = 0;
    profile.dsvDescriptorCount = 0;
    profile.shaderDescriptorCount = 8;
    return profile;
}

bool WriteBytes(const std::filesystem::path& path, std::span<const std::byte> bytes)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(output);
}

sge4::package::PackageBuildInput CopyPackage(
    const sge4::package::FrozenExecutablePackage& package,
    std::uint32_t targetSchemaVersion,
    std::uint32_t minimumRuntimeVersion)
{
    namespace pkg = sge4::package;
    pkg::PackageBuildInput input;
    input.targetKind = package.Header().targetKind;
    input.targetSchemaVersion = targetSchemaVersion;
    input.minimumRuntimeVersion = minimumRuntimeVersion;
    input.flags = package.Header().flags;
    for (const auto& section : package.Sections())
    {
        pkg::PackageSectionInput copy;
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

Result<std::vector<std::byte>, sge4::package::PackageError> ResignWithForgedInputAccess(
    const sge4::package::FrozenExecutablePackage& package)
{
    namespace pkg = sge4::package;
    auto input = CopyPackage(
        package, package.Header().targetSchemaVersion, package.Header().minimumRuntimeVersion);
    for (auto& copy : input.sections)
    {
        if (copy.kind == pkg::SectionKind::D3D12CompositionEndpointTable)
        {
            constexpr std::size_t AccessOffset = 46;
            if (copy.bytes.size() < AccessOffset + 2)
                return Result<std::vector<std::byte>, pkg::PackageError>::Failure(
                    {pkg::PackageErrorCode::InvalidRecordStride, copy.kind, pkg::InvalidIndex,
                     pkg::InvalidIndex, "composition endpoint table is too small"});
            copy.bytes[AccessOffset] = std::byte{2};
            copy.bytes[AccessOffset + 1] = std::byte{0};
        }
    }
    return pkg::PackageWriter::Write(std::move(input));
}

Result<std::vector<std::byte>, sge4::package::PackageError> ResignAsSchema17(
    const sge4::package::FrozenExecutablePackage& package)
{
    auto input = CopyPackage(package, 17, 17);
    return sge4::package::PackageWriter::Write(std::move(input));
}

Result<std::vector<std::byte>, sge4::package::PackageError> ResignWithUnknownSection(
    const sge4::package::FrozenExecutablePackage& package)
{
    namespace pkg = sge4::package;
    auto input = CopyPackage(
        package, package.Header().targetSchemaVersion, package.Header().minimumRuntimeVersion);
    pkg::PackageSectionInput unknown;
    unknown.kind = static_cast<pkg::SectionKind>(0x0000'6000u);
    unknown.schemaVersion = 1;
    unknown.flags = static_cast<std::uint32_t>(pkg::SectionFlags::ExecutionAffecting);
    unknown.alignment = 1;
    unknown.bytes = {std::byte{0x42}};
    input.sections.push_back(std::move(unknown));
    return pkg::PackageWriter::Write(std::move(input));
}
}

int main(int argc, char** argv)
{
    namespace pkg17 = sge4::package::d3d12_v13;
    namespace pkg18 = sge4::package::d3d12_v18;
    namespace comp = sge4::compiler::composition;

    auto graph = BuildLeaf(false);
    if (!graph) { std::cerr << graph.Error() << '\n'; return 1; }

    auto schema17 = sge4::compiler::CompileCanonical(graph.Value(), Profile());
    if (!schema17) { std::cerr << schema17.Error().stage << ": " << schema17.Error().message << '\n'; return 2; }
    auto frozen17 = sge4::package::PackageReader::Read(schema17.Value().packageBytes);
    if (!frozen17 || frozen17.Value().Header().targetSchemaVersion != 17 ||
        frozen17.Value().Header().minimumRuntimeVersion != 17) return 3;

    std::vector<comp::EndpointDeclaration> declarations = {
        {{0}, "frame/input"},
        {{1}, "frame/output"}};
    auto compiled = comp::CompileCompositionLeafCanonical(graph.Value(), Profile(), declarations);
    if (!compiled) { std::cerr << compiled.Error().stage << ": " << compiled.Error().message << '\n'; return 4; }

    std::reverse(declarations.begin(), declarations.end());
    auto reversed = comp::CompileCompositionLeafCanonical(graph.Value(), Profile(), declarations);
    if (!reversed || reversed.Value().packageBytes != compiled.Value().packageBytes) return 5;

    auto frozen18 = sge4::package::PackageReader::Read(compiled.Value().packageBytes);
    if (!frozen18 || frozen18.Value().Header().targetSchemaVersion != 18 ||
        frozen18.Value().Header().minimumRuntimeVersion != 18) return 6;
    if (pkg17::D3D12PackageView::Decode(frozen18.Value())) return 7;

    auto view18 = pkg18::CompositionLeafView::Decode(frozen18.Value());
    if (!view18 || view18.Value().Endpoints().size() != 2) return 8;
    const auto endpoints = view18.Value().Endpoints();
    if (endpoints[0].id != 0 || endpoints[0].externalSlot.value != 0 ||
        endpoints[0].access != pkg18::EndpointAccess::ReadOnly ||
        endpoints[1].id != 1 || endpoints[1].externalSlot.value != 1 ||
        endpoints[1].access != pkg18::EndpointAccess::WriteOnly ||
        endpoints[0].stableKey == endpoints[1].stableKey) return 9;
    if (!std::equal(view18.Value().Schema17BasePackageBytes().begin(),
                    view18.Value().Schema17BasePackageBytes().end(),
                    schema17.Value().packageBytes.begin(), schema17.Value().packageBytes.end())) return 10;

    auto tamperedAccess = compiled.Value().endpoints;
    tamperedAccess[0].access = pkg18::EndpointAccess::WriteOnly;
    if (pkg18::BuildCompositionLeafPackage(frozen17.Value(), tamperedAccess)) return 11;

    auto resignedForgery = ResignWithForgedInputAccess(frozen18.Value());
    if (!resignedForgery) return 12;
    auto resignedPackage = sge4::package::PackageReader::Read(resignedForgery.Value());
    if (!resignedPackage || pkg18::CompositionLeafView::Decode(resignedPackage.Value())) return 13;

    auto schema17Forgery = ResignAsSchema17(frozen18.Value());
    if (!schema17Forgery) return 14;
    auto schema17ForgeryRead = sge4::package::PackageReader::Read(schema17Forgery.Value());
    if (schema17ForgeryRead ||
        schema17ForgeryRead.Error().code != sge4::package::PackageErrorCode::UnsupportedTargetSchema) return 15;

    auto unknownSection = ResignWithUnknownSection(frozen18.Value());
    if (!unknownSection) return 16;
    auto unknownPackage = sge4::package::PackageReader::Read(unknownSection.Value());
    if (!unknownPackage || pkg18::CompositionLeafView::Decode(unknownPackage.Value())) return 17;

    auto reorderedEndpoints = compiled.Value().endpoints;
    std::swap(reorderedEndpoints[0], reorderedEndpoints[1]);
    reorderedEndpoints[0].id = 0;
    reorderedEndpoints[1].id = 1;
    if (pkg18::BuildCompositionLeafPackage(frozen17.Value(), reorderedEndpoints)) return 18;

    auto duplicateKey = compiled.Value().endpoints;
    duplicateKey[1].stableKey = duplicateKey[0].stableKey;
    if (pkg18::BuildCompositionLeafPackage(frozen17.Value(), duplicateKey)) return 19;

    auto missingEndpoint = compiled.Value().endpoints;
    missingEndpoint.pop_back();
    if (pkg18::BuildCompositionLeafPackage(frozen17.Value(), missingEndpoint)) return 20;

    auto missingDeclaration = comp::CompileCompositionLeafCanonical(
        graph.Value(), Profile(), {{{0}, "frame/input"}});
    if (missingDeclaration || missingDeclaration.Error().stage != "schema18-composition-interface") return 21;

    auto duplicateDeclarationKey = comp::CompileCompositionLeafCanonical(
        graph.Value(), Profile(), {{{0}, "same"}, {{1}, "same"}});
    if (duplicateDeclarationKey) return 22;

    auto invalidDeclarationKey = comp::CompileCompositionLeafCanonical(
        graph.Value(), Profile(), {{{0}, "bad key"}, {{1}, "frame/output"}});
    if (invalidDeclarationKey) return 23;

    auto readWriteGraph = BuildLeaf(true);
    if (!readWriteGraph) return 24;
    auto readWriteRejected = comp::CompileCompositionLeafCanonical(
        readWriteGraph.Value(), Profile(), {{{0}, "frame/input"}, {{1}, "frame/output"}});
    if (readWriteRejected || readWriteRejected.Error().stage != "schema18-composition-interface") return 25;

    if (argc == 3 && std::string_view(argv[1]) == "--emit")
    {
        if (!WriteBytes(argv[2], compiled.Value().packageBytes)) return 26;
    }

    std::cout << "L4V1-F1 Schema 18 Composition Interface tests passed\n";
    std::cout << "Schema 17 preserved; Schema 18 endpoints: ReadOnly input, WriteOnly output\n";
    std::cout << "Execution digest: " << compiled.Value().executionDigestHex << '\n';
    return 0;
}
