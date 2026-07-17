#include "CompositionLeafCompiler.h"

#include "../04_SemanticAnalysis/SemanticAnalysis.h"
#include "../09_FrozenPackageCore/PackageReader.h"
#include "../10_D3D12PackageSchema/D3D12Encoding.h"
#include "../10A_D3D12CompositionSchema18/D3D12CompositionEncoding18.h"
#include "../12_SGE4Compiler/SGE4Compiler.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <string_view>
#include <utility>

namespace sge4::compiler::composition
{
namespace
{
Error MakeError(std::string message)
{
    return {"schema18-composition-interface", std::move(message)};
}

template<class T>
base::Result<T, Error> Failure(std::string message)
{
    return base::Result<T, Error>::Failure(MakeError(std::move(message)));
}

bool ValidStableKey(std::string_view key) noexcept
{
    if (key.empty() || key.size() > 127) return false;
    return std::all_of(key.begin(), key.end(), [](unsigned char value) {
        const bool alpha = (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z');
        const bool digit = value >= '0' && value <= '9';
        return alpha || digit || value == '.' || value == '_' || value == '-' || value == '/';
    });
}

const semantic::Resource* FindResource(
    const semantic::SemanticGraph& graph,
    semantic::ResourceId id) noexcept
{
    const auto found = std::find_if(graph.resources.begin(), graph.resources.end(),
        [id](const semantic::Resource& resource) { return resource.id == id; });
    return found == graph.resources.end() ? nullptr : &*found;
}

const semantic::ProgramParameter* FindParameter(
    const semantic::ProgramInterface& interfaceDescription,
    semantic::ProgramParameterId id) noexcept
{
    const auto found = std::find_if(
        interfaceDescription.parameters.begin(), interfaceDescription.parameters.end(),
        [id](const semantic::ProgramParameter& parameter) { return parameter.id == id; });
    return found == interfaceDescription.parameters.end() ? nullptr : &*found;
}

const semantic::Program* FindProgram(
    const semantic::SemanticGraph& graph,
    semantic::ProgramId id) noexcept
{
    const auto found = std::find_if(graph.programs.begin(), graph.programs.end(),
        [id](const semantic::Program& program) { return program.id == id; });
    return found == graph.programs.end() ? nullptr : &*found;
}

base::Result<void, Error> ValidateProgramBindings(
    const semantic::SemanticGraph& graph,
    semantic::ResourceId resource,
    package::d3d12_v18::EndpointAccess access)
{
    const auto expectedKind = access == package::d3d12_v18::EndpointAccess::ReadOnly ?
        semantic::ProgramParameterKind::ReadOnlyBuffer :
        semantic::ProgramParameterKind::UnorderedBuffer;
    for (const auto& use : graph.resourceUses)
    {
        if (use.resource != resource) continue;
        std::uint32_t bindingCount = 0;
        for (const auto& work : graph.works)
        {
            for (const auto& operand : work.operands)
            {
                if (operand.use != use.id) continue;
                ++bindingCount;
                if (operand.kind != semantic::WorkOperandKind::ProgramParameter)
                    return base::Result<void, Error>::Failure(MakeError(
                        "F1 composition endpoints must be bound through ProgramParameter operands"));
                const semantic::ProgramId programId = work.kind == semantic::WorkKind::Compute ?
                    work.compute.program : work.kind == semantic::WorkKind::Raster ?
                    work.raster.program : semantic::ProgramId{};
                const auto* program = FindProgram(graph, programId);
                const auto* parameter = program == nullptr ? nullptr :
                    FindParameter(program->interface, operand.parameter);
                if (parameter == nullptr || parameter->kind != expectedKind)
                    return base::Result<void, Error>::Failure(MakeError(
                        "composition endpoint access disagrees with ProgramParameterKind"));
            }
        }
        if (bindingCount != 1)
            return base::Result<void, Error>::Failure(MakeError(
                "each composition endpoint ResourceUse must be owned by exactly one Work operand"));
    }
    return base::Result<void, Error>::Success();
}

base::Result<package::d3d12_v18::EndpointAccess, Error> DeriveAccess(
    const semantic::SemanticGraph& graph,
    semantic::ResourceId resource)
{
    bool sawRead = false;
    bool sawWrite = false;
    std::uint32_t useCount = 0;
    for (const auto& use : graph.resourceUses)
    {
        if (use.resource != resource) continue;
        ++useCount;
        if (use.effect == semantic::Effect::ReadWrite)
            return Failure<package::d3d12_v18::EndpointAccess>(
                "ReadWrite External Resources cannot be Schema 18 composition endpoints");
        if (use.effect == semantic::Effect::Read)
        {
            if (use.role != semantic::ViewRole::ShaderBuffer)
                return Failure<package::d3d12_v18::EndpointAccess>(
                    "ReadOnly composition endpoints are limited to ShaderBuffer/SRV uses in F1");
            sawRead = true;
        }
        else if (use.effect == semantic::Effect::Write)
        {
            if (use.role != semantic::ViewRole::StorageBuffer)
                return Failure<package::d3d12_v18::EndpointAccess>(
                    "WriteOnly composition endpoints are limited to StorageBuffer/UAV uses in F1");
            sawWrite = true;
        }
        else
        {
            return Failure<package::d3d12_v18::EndpointAccess>(
                "composition endpoint ResourceUse has an unknown Effect");
        }
    }
    if (useCount == 0)
        return Failure<package::d3d12_v18::EndpointAccess>(
            "every composition endpoint must be used by at least one Work");
    if (sawRead && sawWrite)
        return Failure<package::d3d12_v18::EndpointAccess>(
            "one composition endpoint cannot mix read and write uses");
    if (sawRead)
        return base::Result<package::d3d12_v18::EndpointAccess, Error>::Success(
            package::d3d12_v18::EndpointAccess::ReadOnly);
    if (sawWrite)
        return base::Result<package::d3d12_v18::EndpointAccess, Error>::Success(
            package::d3d12_v18::EndpointAccess::WriteOnly);
    return Failure<package::d3d12_v18::EndpointAccess>(
        "composition endpoint access could not be derived");
}
}

base::Result<CompositionLeafCompileOutput, Error> CompileCompositionLeafCanonical(
    const semantic::SemanticGraph& graph,
    const target::D3D12TargetProfile& targetProfile,
    std::vector<EndpointDeclaration> declarations)
{
    auto schema17 = compiler::CompileCanonical(graph, targetProfile);
    if (!schema17)
        return base::Result<CompositionLeafCompileOutput, Error>::Failure(schema17.Error());

    auto analyzed = analysis::Analyze(graph);
    if (!analyzed)
        return Failure<CompositionLeafCompileOutput>(
            "SemanticAnalysis unexpectedly failed after Schema 17 compilation");

    std::map<std::uint32_t, std::string> declaredKeys;
    std::set<std::string> uniqueAuthorKeys;
    for (auto& declaration : declarations)
    {
        if (!declaration.resource.IsValid() || !ValidStableKey(declaration.stableKey))
            return Failure<CompositionLeafCompileOutput>(
                "composition declarations require a valid Resource ID and a 1-127 character stable key using [A-Za-z0-9._/-]");
        if (!declaredKeys.emplace(declaration.resource.value, declaration.stableKey).second)
            return Failure<CompositionLeafCompileOutput>(
                "a composition Resource was declared more than once");
        if (!uniqueAuthorKeys.insert(declaration.stableKey).second)
            return Failure<CompositionLeafCompileOutput>(
                "composition stable endpoint keys must be unique before hashing");
    }

    std::vector<const semantic::Resource*> externalBuffers;
    for (const auto resourceId : analyzed.Value().canonicalResourceOrder)
    {
        const auto* resource = FindResource(graph, resourceId);
        if (resource == nullptr)
            return Failure<CompositionLeafCompileOutput>(
                "canonical Resource identity disappeared during composition-interface derivation");
        if (resource->lifetime == semantic::LifetimeIntent::External &&
            resource->kind != semantic::ResourceKind::SurfaceImage)
        {
            if (resource->kind != semantic::ResourceKind::Buffer)
                return Failure<CompositionLeafCompileOutput>(
                    "Level 4 v1 Schema 18 supports only External Buffer endpoints");
            externalBuffers.push_back(resource);
        }
    }

    if (externalBuffers.size() != declarations.size())
        return Failure<CompositionLeafCompileOutput>(
            "every External Buffer must have exactly one stable composition endpoint declaration");

    auto frozen17 = package::PackageReader::Read(schema17.Value().packageBytes);
    if (!frozen17)
        return Failure<CompositionLeafCompileOutput>(
            "Schema 17 compiler output failed PackageReader validation: " + frozen17.Error().message);
    auto view17 = package::d3d12_v13::D3D12PackageView::Decode(frozen17.Value());
    if (!view17)
        return Failure<CompositionLeafCompileOutput>(
            "Schema 17 compiler output failed D3D12 decoding: " + view17.Error().message);
    if (view17.Value().ExternalSlots().size() != externalBuffers.size())
        return Failure<CompositionLeafCompileOutput>(
            "Schema 17 external-slot order disagrees with canonical Semantic Resource order");

    std::vector<package::d3d12_v18::LeafCompositionEndpointArtifact> endpoints;
    endpoints.reserve(externalBuffers.size());
    std::set<package::d3d12_v18::StableEndpointKey> hashedKeys;
    for (std::uint32_t index = 0; index < externalBuffers.size(); ++index)
    {
        const auto* resource = externalBuffers[index];
        const auto declaration = declaredKeys.find(resource->id.value);
        if (declaration == declaredKeys.end())
            return Failure<CompositionLeafCompileOutput>(
                "an External Buffer is missing its stable composition endpoint declaration");
        auto access = DeriveAccess(graph, resource->id);
        if (!access)
            return base::Result<CompositionLeafCompileOutput, Error>::Failure(access.Error());
        auto bindingValidation = ValidateProgramBindings(graph, resource->id, access.Value());
        if (!bindingValidation)
            return base::Result<CompositionLeafCompileOutput, Error>::Failure(bindingValidation.Error());

        const auto& slot = view17.Value().ExternalSlots()[index];
        const auto stableKey = package::d3d12_v18::ComputeStableEndpointKey(declaration->second);
        if (!hashedKeys.insert(stableKey).second)
            return Failure<CompositionLeafCompileOutput>(
                "stable composition endpoint key digests collided");

        package::d3d12_v18::LeafCompositionEndpointArtifact endpoint;
        endpoint.id = index;
        endpoint.stableKey = stableKey;
        endpoint.externalSlot = {index};
        endpoint.resource = slot.resource;
        endpoint.kind = slot.requiredKind;
        endpoint.access = access.Value();
        endpoint.format = slot.requiredFormat;
        endpoint.minimumBytes = slot.minimumBytes;
        endpoint.requiredIncomingState = slot.requiredIncomingState;
        endpoint.guaranteedOutgoingState = slot.guaranteedOutgoingState;
        endpoint.synchronization = slot.synchronizationContract;
        endpoints.push_back(endpoint);
    }

    auto schema18Bytes = package::d3d12_v18::BuildCompositionLeafPackage(
        frozen17.Value(), endpoints);
    if (!schema18Bytes)
        return Failure<CompositionLeafCompileOutput>(
            "Schema 18 encoding rejected the derived composition interface: " +
            schema18Bytes.Error().message);
    auto frozen18 = package::PackageReader::Read(schema18Bytes.Value());
    if (!frozen18)
        return Failure<CompositionLeafCompileOutput>(
            "Schema 18 package failed PackageReader validation: " + frozen18.Error().message);

    CompositionLeafCompileOutput output;
    output.packageBytes = std::move(schema18Bytes).Value();
    output.executionDigestHex = base::ToHex(frozen18.Value().ExecutionDigest());
    output.completedStages = std::move(schema17.Value().completedStages);
    output.completedStages.push_back("schema18-composition-interface");
    output.endpoints = std::move(endpoints);
    return base::Result<CompositionLeafCompileOutput, Error>::Success(std::move(output));
}
}
