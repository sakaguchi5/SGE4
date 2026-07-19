#include "CompilationInput.h"

#include <utility>

namespace sge4_5::compiler::compilation
{
namespace
{
template<class T>
base::Result<T, CompilationError> Failure(std::string stage, std::string message)
{
    return base::Result<T, CompilationError>::Failure(
        CompilationError{std::move(stage), std::move(message)});
}
}

base::Result<void, CompilationError> ValidateD3D12Schema17Capability(
    const semantic::SemanticGraph& graph,
    const target::D3D12TargetProfile& targetProfile)
{
    if (targetProfile.framesInFlight == 0 || targetProfile.directQueueCount != 1 ||
        targetProfile.computeQueueCount > 1 || targetProfile.copyQueueCount > 1 ||
        targetProfile.shaderBinaryFormat != target::ShaderBinaryFormat::Dxbc ||
        targetProfile.shaderModelMajor != 5 || targetProfile.shaderModelMinor > 1 ||
        targetProfile.rootSignatureMajor != 1 || targetProfile.rootSignatureMinor > 1 ||
        targetProfile.barrierModel != target::BarrierModel::Legacy)
        return Failure<void>("target-feasibility", "target profile is outside the SGE4 D3D12 Schema 17 capability constitution");

    std::uint32_t surfaceCount = 0;
    bool hasTemporal = false;
    bool hasSurfaceRelativeResource = false;
    for (const auto& resource : graph.resources)
    {
        if (resource.kind == semantic::ResourceKind::SurfaceImage) ++surfaceCount;
        if (resource.lifetime == semantic::LifetimeIntent::Temporal) hasTemporal = true;
        if (resource.kind == semantic::ResourceKind::Texture2D &&
            resource.texture2D.extentMeaning == semantic::TextureExtentMeaning::PresentationSurface)
            hasSurfaceRelativeResource = true;
        if (resource.kind == semantic::ResourceKind::Texture2D && resource.texture2D.mipLevels != 1)
            return Failure<void>("target-feasibility", "SGE4 D3D12 Schema 17 supports Texture2D with exactly one mip level");
        if (resource.kind == semantic::ResourceKind::Texture2D &&
            resource.texture2D.extentMeaning == semantic::TextureExtentMeaning::Fixed &&
            resource.texture2D.formatMeaning != semantic::FormatMeaning::Bgra8Unorm)
            return Failure<void>("target-feasibility", "fixed Texture2D is limited to Bgra8Unorm in SGE4 D3D12 Schema 17");
        if (resource.kind == semantic::ResourceKind::Texture2D &&
            resource.texture2D.extentMeaning == semantic::TextureExtentMeaning::PresentationSurface &&
            resource.texture2D.formatMeaning != semantic::FormatMeaning::Depth32Float)
            return Failure<void>("target-feasibility", "surface-relative Texture2D is limited to Depth32Float in SGE4 D3D12 Schema 17");
    }
    if (surfaceCount > 1)
        return Failure<void>("target-feasibility", "SGE4 D3D12 Schema 17 supports at most one presentation SurfaceImage");
    if (surfaceCount != 0 && targetProfile.surfaceImageCount == 0)
        return Failure<void>("target-feasibility", "a graph with a SurfaceImage requires a non-zero surface image count");
    if (hasSurfaceRelativeResource && surfaceCount == 0)
        return Failure<void>("target-feasibility", "a surface-relative Texture2D requires a presentation SurfaceImage");
    if (hasTemporal && targetProfile.framesInFlight < 2)
        return Failure<void>("target-feasibility", "Temporal resources require at least two frames in flight");

    bool hasRaster = false;
    for (const auto& work : graph.works)
    {
        if (work.kind == semantic::WorkKind::Present)
            return Failure<void>("target-feasibility", "SGE4 D3D12 Schema 17 uses the Raster PresentSource operand; standalone Present Work is deferred");
        if (work.kind == semantic::WorkKind::Raster)
        {
            hasRaster = true;
            std::uint32_t color = 0;
            std::uint32_t depth = 0;
            std::uint32_t vertex = 0;
            for (const auto& operand : work.operands)
            {
                if (operand.kind == semantic::WorkOperandKind::ColorAttachment) ++color;
                if (operand.kind == semantic::WorkOperandKind::DepthAttachment) ++depth;
                if (operand.kind == semantic::WorkOperandKind::VertexData) ++vertex;
            }
            if (vertex != 1 || color != 1 || depth > 1)
                return Failure<void>("target-feasibility", "SGE4 D3D12 Schema 17 raster work requires one vertex buffer, one color surface, and at most one depth attachment");
        }
    }
    if (surfaceCount != 0 && !hasRaster)
        return Failure<void>("target-feasibility", "a presentation SurfaceImage must be consumed by at least one Raster Work");
    return base::Result<void, CompilationError>::Success();
}

base::Result<ValidatedCompilationInput, CompilationError> ValidateCompilationInput(
    const semantic::SemanticGraph& graph,
    const target::D3D12TargetProfile& targetProfile)
{
    auto analyzedResult = analysis::Analyze(graph);
    if (!analyzedResult)
    {
        std::string message;
        for (const auto& diagnostic : analyzedResult.Error())
        {
            if (!message.empty()) message += "; ";
            message += diagnostic.message;
        }
        return Failure<ValidatedCompilationInput>("semantic-analysis", std::move(message));
    }

    auto capability = ValidateD3D12Schema17Capability(graph, targetProfile);
    if (!capability)
        return base::Result<ValidatedCompilationInput, CompilationError>::Failure(capability.Error());

    ValidatedCompilationInput output;
    output.source = &graph;
    output.targetProfile = targetProfile;
    output.analyzed = std::move(analyzedResult).Value();
    return base::Result<ValidatedCompilationInput, CompilationError>::Success(std::move(output));
}
}
