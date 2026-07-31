base::Expected<CompileOutput, CompileError> CompileReferenceCanonical(
    const semantic::SemanticGraph& graph,
    const target::D3D12TargetProfile& targetProfile)
{
    auto validated = ValidateSourceStage(graph, targetProfile);
    if (!validated)
        return base::Failure<CompileOutput, CompileError>(validated.error());
    auto programs = CompileProgramStage(validated.value());
    if (!programs)
        return base::Failure<CompileOutput, CompileError>(programs.error());
    auto lowered = LowerPackageStage(validated.value(), programs.value(), nullptr);
    if (!lowered)
        return base::Failure<CompileOutput, CompileError>(lowered.error());
    auto frozen = FreezePackageStage(std::move(lowered).value());
    if (!frozen)
        return frozen;
    frozen.value().completedStages = {
        "source-validation",
        "shader-compilation-reflection",
        "package-lowering",
        "package-serialization-validation"};
    return frozen;
}

base::Expected<CompileOutput, CompileError> LowerVerifiedPlan(
    const semantic::SemanticGraph& graph,
    const target::D3D12TargetProfile& targetProfile,
    const planning::verification::VerifiedExecutionPlan& selectedPlan)
{
    auto validated = ValidateSourceStage(graph, targetProfile);
    if (!validated) return base::Failure<CompileOutput, CompileError>(validated.error());
    auto programs = CompileProgramStage(validated.value());
    if (!programs) return base::Failure<CompileOutput, CompileError>(programs.error());
    const auto& plan = selectedPlan.Plan();
    auto lowered = LowerPackageStage(validated.value(), programs.value(), &plan);
    if (!lowered) return base::Failure<CompileOutput, CompileError>(lowered.error());
    lowered.value().description.provenance =
        planning::EncodeVerificationCertificate(selectedPlan.Certificate());
    auto frozen = FreezePackageStage(std::move(lowered).value());
    if (frozen)
        frozen.value().completedStages = {"source-validation", "program-compilation", "verified-plan-lowering", "package-freeze"};
    return frozen;
}

base::Expected<FrozenComputePackageEvidenceV1, CompileError>
InspectFrozenComputePackageEvidenceV1(std::span<const std::byte> packageBytes)
{
    auto packageResult = package::PackageReader::Read(packageBytes);
    if (!packageResult)
        return Failure<FrozenComputePackageEvidenceV1>("package-evidence-read", packageResult.error().message);

    auto viewResult = pkg::D3D12PackageView::Decode(packageResult.value());
    if (!viewResult)
        return Failure<FrozenComputePackageEvidenceV1>("package-evidence-schema", viewResult.error().message);

    const auto& package = packageResult.value();
    const auto& view = viewResult.value();
    std::uint32_t executeCount = 0;
    pkg::ComputeCommandId commandId;
    for (const auto& operation : view.FrameOperations())
    {
        if (operation.opcode != pkg::D3D12OperationCode::ExecuteCompute)
            continue;
        auto decoded = pkg::DecodeExecuteCompute(operation.payload);
        if (!decoded)
            return Failure<FrozenComputePackageEvidenceV1>("package-evidence-operation", decoded.error().message);
        commandId = decoded.value().command;
        ++executeCount;
    }

    if (executeCount != 1 || !commandId.IsValid() || commandId.value >= view.ComputeCommands().size())
        return Failure<FrozenComputePackageEvidenceV1>(
            "package-evidence-operation", "Packageが検証または実行の契約に違反しています。");
    if (view.Programs().size() != 1 || view.BindingLayouts().size() != 1 ||
        view.Shaders().size() != 1 || view.ComputeExecutables().size() != 1)
        return Failure<FrozenComputePackageEvidenceV1>(
            "package-evidence-program", "Packageが検証または実行の契約に違反しています。");

    const auto& command = view.ComputeCommands()[commandId.value];
    if (!command.executable.IsValid() || command.executable.value >= view.ComputeExecutables().size())
        return Failure<FrozenComputePackageEvidenceV1>("package-evidence-program", "Tableが検証または実行の契約に違反しています。");
    const auto& executable = view.ComputeExecutables()[command.executable.value];
    if (!executable.program.IsValid() || executable.program.value >= view.Programs().size())
        return Failure<FrozenComputePackageEvidenceV1>("package-evidence-program", "Programが検証または実行の契約に違反しています。");
    const auto& program = view.Programs()[executable.program.value];
    if (!program.bindingLayout.IsValid() || program.bindingLayout.value >= view.BindingLayouts().size() ||
        !program.computeShader.IsValid() || program.computeShader.value >= view.Shaders().size())
        return Failure<FrozenComputePackageEvidenceV1>("package-evidence-program", "Programが検証または実行の契約に違反しています。");

    FrozenComputePackageEvidenceV1 evidence;
    evidence.executionDigest = package.ExecutionDigest();
    evidence.fileDigest = package.Header().fileDigest;
    evidence.programInterfaceDigest = program.interfaceDigest;
    evidence.bindingLayoutDigest = view.BindingLayouts()[program.bindingLayout.value].layoutDigest;
    evidence.shaderBytecodeDigest = view.Shaders()[program.computeShader.value].bytecodeDigest;
    evidence.dispatchX = command.threadGroupCountX;
    evidence.dispatchY = command.threadGroupCountY;
    evidence.dispatchZ = command.threadGroupCountZ;
    evidence.dynamicSlots.reserve(view.DynamicSlots().size());
    for (const auto& slot : view.DynamicSlots())
        evidence.dynamicSlots.push_back({slot.requiredBytes, slot.requiredAlignment, slot.flags});
    evidence.externalSlots.reserve(view.ExternalSlots().size());
    for (const auto& slot : view.ExternalSlots())
        evidence.externalSlots.push_back({slot.minimumBytes});
    return base::Success<FrozenComputePackageEvidenceV1, CompileError>(std::move(evidence));
}
