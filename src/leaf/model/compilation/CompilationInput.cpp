#include "./CompilationInput.h"

#include <utility>

namespace sge4::compiler::compilation
{
namespace
{
template<class T>
base::Expected<T, CompilationError> Failure(std::string stage, std::string message)
{
    return base::Failure<T, CompilationError>(
        CompilationError{std::move(stage), std::move(message)});
}
}

base::Expected<void, CompilationError> ValidateD3D12Schema17Capability(
    const semantic::SemanticGraph& graph,
    const target::D3D12TargetProfile& targetProfile)
{
    if (targetProfile.framesInFlight == 0 || targetProfile.directQueueCount != 1 ||
        targetProfile.computeQueueCount > 1 || targetProfile.copyQueueCount > 1 ||
        targetProfile.shaderBinaryFormat != target::ShaderBinaryFormat::Dxbc ||
        targetProfile.shaderModelMajor != 5 || targetProfile.shaderModelMinor > 1 ||
        targetProfile.rootSignatureMajor != 1 || targetProfile.rootSignatureMinor > 1 ||
        targetProfile.barrierModel != target::BarrierModel::Legacy)
        return Failure<void>("target-feasibility", "Fileが検証または実行の契約に違反しています。");

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
            return Failure<void>("target-feasibility", "検証または実行の契約に違反しています。");
        if (resource.kind == semantic::ResourceKind::Texture2D &&
            resource.texture2D.extentMeaning == semantic::TextureExtentMeaning::Fixed &&
            resource.texture2D.formatMeaning != semantic::FormatMeaning::Bgra8Unorm)
            return Failure<void>("target-feasibility", "検証または実行の契約に違反しています。");
        if (resource.kind == semantic::ResourceKind::Texture2D &&
            resource.texture2D.extentMeaning == semantic::TextureExtentMeaning::PresentationSurface &&
            resource.texture2D.formatMeaning != semantic::FormatMeaning::Depth32Float)
            return Failure<void>("target-feasibility", "検証または実行の契約に違反しています。");
    }
    if (surfaceCount > 1)
        return Failure<void>("target-feasibility", "検証または実行の契約に違反しています。");
    if (surfaceCount != 0 && targetProfile.surfaceImageCount == 0)
        return Failure<void>("target-feasibility", "Surfaceが検証または実行の契約に違反しています。");
    if (hasSurfaceRelativeResource && surfaceCount == 0)
        return Failure<void>("target-feasibility", "Texture2Dが検証または実行の契約に違反しています。");
    if (hasTemporal && targetProfile.framesInFlight < 2)
        return Failure<void>("target-feasibility", "Resourceが検証または実行の契約に違反しています。");

    bool hasRaster = false;
    for (const auto& work : graph.works)
    {
        if (work.kind == semantic::WorkKind::Present)
            return Failure<void>("target-feasibility", "SGE4 D3D12 Schema 17ではRasterのPresentSource operandを使用します。独立したPresent Workは現時点では対象外です。");
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
                return Failure<void>("target-feasibility", "Bufferが検証または実行の契約に違反しています。");
        }
    }
    if (surfaceCount != 0 && !hasRaster)
        return Failure<void>("target-feasibility", "Surfaceが検証または実行の契約に違反しています。");
    return base::Success<void, CompilationError>();
}

base::Expected<ValidatedCompilationInput, CompilationError> ValidateCompilationInput(
    const semantic::SemanticGraph& graph,
    const target::D3D12TargetProfile& targetProfile)
{
    auto analyzedResult = analysis::Analyze(graph);
    if (!analyzedResult)
    {
        std::string message;
        for (const auto& diagnostic : analyzedResult.error())
        {
            if (!message.empty()) message += "; ";
            message += diagnostic.message;
        }
        return Failure<ValidatedCompilationInput>("semantic-analysis", std::move(message));
    }

    auto capability = ValidateD3D12Schema17Capability(graph, targetProfile);
    if (!capability)
        return base::Failure<ValidatedCompilationInput, CompilationError>(capability.error());

    ValidatedCompilationInput output;
    output.source = &graph;
    output.targetProfile = targetProfile;
    output.analyzed = std::move(analyzedResult).value();
    return base::Success<ValidatedCompilationInput, CompilationError>(std::move(output));
}
}
