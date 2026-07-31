struct ShaderIds final
{
    pkg::ShaderId vertex;
    pkg::ShaderId pixel;
    pkg::ShaderId compute;
};

struct WorkArtifacts final
{
    pkg::ProgramId program;
    pkg::BindingLayoutId layout;
    pkg::ExecutableId rasterExecutable;
    pkg::RasterCommandId rasterCommand;
    pkg::ComputeExecutableId computeExecutable;
    pkg::ComputeCommandId computeCommand;
};

struct StateCell final
{
    std::uint32_t resource = 0;
    std::uint32_t temporalRelation = 0; // 0 normal, 1 current, 2 previous
    auto operator<=>(const StateCell&) const = default;
};

struct LoweredPackageStage final
{
    pkg::D3D12PackageDescription description;
};

}

base::Expected<void, CompileError> ValidateLevel2Capability(
    const semantic::SemanticGraph& graph,
    const target::D3D12TargetProfile& targetProfile)
{
    return compilation::ValidateD3D12Schema17Capability(graph, targetProfile);
}

base::Expected<ValidatedSourceStage, CompileError> ValidateSourceStage(
    const semantic::SemanticGraph& graph,
    const target::D3D12TargetProfile& targetProfile)
{
    return compilation::ValidateCompilationInput(graph, targetProfile);
}

base::Expected<ProgramCompilationStage, CompileError> CompileProgramStage(
    const ValidatedSourceStage& validated)
{
    if (validated.source == nullptr || validated.analyzed.source != validated.source)
        return Failure<ProgramCompilationStage>("shader-compilation", "検証または実行の契約に違反しています。");

    std::map<std::uint32_t, const semantic::Program*> programs;
    for (const auto& program : validated.source->programs)
        programs[program.id.value] = &program;
    std::set<std::uint32_t> usedProgramIds;
    for (const auto& work : validated.source->works)
    {
        if (work.kind == semantic::WorkKind::Raster) usedProgramIds.insert(work.raster.program.value);
        if (work.kind == semantic::WorkKind::Compute) usedProgramIds.insert(work.compute.program.value);
    }

    ProgramCompilationStage output;
    for (const auto programId : validated.analyzed.canonicalProgramOrder)
    {
        if (!usedProgramIds.contains(programId.value)) continue;
        const auto found = programs.find(programId.value);
        if (found == programs.end())
            return Failure<ProgramCompilationStage>("shader-compilation", "ProgramがCanonicalな順序または識別子規則に違反しています。");
        const auto& program = *found->second;
        CompiledProgram compiled;
        compiled.sourceProgram = program.id;
        if (program.kind == semantic::ProgramKind::Raster)
        {
            auto vertex = CompileAndReflectShader(
                program, semantic::ShaderStage::Vertex, program.source.vertexEntry, "vs_5_1");
            if (!vertex) return base::Failure<ProgramCompilationStage, CompileError>(vertex.error());
            auto pixel = CompileAndReflectShader(
                program, semantic::ShaderStage::Pixel, program.source.pixelEntry, "ps_5_1");
            if (!pixel) return base::Failure<ProgramCompilationStage, CompileError>(pixel.error());
            compiled.shaders.push_back(std::move(vertex).value());
            compiled.shaders.push_back(std::move(pixel).value());
        }
        else
        {
            auto compute = CompileAndReflectShader(
                program, semantic::ShaderStage::Compute, program.source.computeEntry, "cs_5_1");
            if (!compute) return base::Failure<ProgramCompilationStage, CompileError>(compute.error());
            compiled.shaders.push_back(std::move(compute).value());
        }
        output.programs.push_back(std::move(compiled));
    }
    return base::Success<ProgramCompilationStage, CompileError>(std::move(output));
}

namespace
{
