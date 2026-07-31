std::span<const OperationContract> OperationContracts() noexcept
{
    return OperationContractTable;
}

std::uint16_t OperationVersion(D3D12OperationCode code) noexcept
{
    const auto found = std::ranges::find(OperationContractTable, code, &OperationContract::code);
    return found == OperationContractTable.end() ? 0u : found->version;
}

bool IsKnownOperation(D3D12OperationCode code) noexcept
{
    return OperationVersion(code) != 0;
}

base::Expected<std::vector<std::byte>, PackageError> BuildFrozenPackage(const D3D12PackageDescription& description)
{
    if (!DenseIds(description))
        return base::Failure<std::vector<std::byte>, PackageError>(Error(PackageErrorCode::InvalidIdSequence, "入力または内部状態がCanonicalな順序または識別子規則に違反しています。"));
    if (description.provenance.empty())
        return base::Failure<std::vector<std::byte>, PackageError>(
            Error(PackageErrorCode::MissingRequiredSection,
                "Packageが検証または実行の契約に違反しています。",
                SectionKind::Provenance));
    if (description.operationStreams.size() != 2 || description.operationStreams[0].kind != OperationStreamKind::Load ||
        description.operationStreams[1].kind != OperationStreamKind::Frame)
        return base::Failure<std::vector<std::byte>, PackageError>(Error(PackageErrorCode::InvalidOperationStream, "Packageが検証または実行の契約に違反しています。"));

    PackageManifest manifest;
    manifest.resourceCount = static_cast<std::uint32_t>(description.resources.size());
    manifest.allocationCount = static_cast<std::uint32_t>(description.allocations.size());
    manifest.viewCount = static_cast<std::uint32_t>(description.views.size());
    manifest.shaderCount = static_cast<std::uint32_t>(description.shaders.size());
    manifest.programCount = static_cast<std::uint32_t>(description.programs.size());
    manifest.bindingLayoutCount = static_cast<std::uint32_t>(description.bindingLayouts.size());
    manifest.executableCount = static_cast<std::uint32_t>(description.executables.size());
    manifest.rasterCommandCount = static_cast<std::uint32_t>(description.rasterCommands.size());
    manifest.computeExecutableCount = static_cast<std::uint32_t>(description.computeExecutables.size());
    manifest.computeCommandCount = static_cast<std::uint32_t>(description.computeCommands.size());
    manifest.vertexElementCount = static_cast<std::uint32_t>(description.vertexElements.size());
    manifest.attachmentOperationCount = static_cast<std::uint32_t>(description.attachmentOperations.size());
    manifest.dynamicSlotCount = static_cast<std::uint32_t>(description.dynamicSlots.size());
    manifest.externalSlotCount = static_cast<std::uint32_t>(description.externalSlots.size());
    manifest.surfaceSlotCount = static_cast<std::uint32_t>(description.surfaceSlots.size());
    manifest.loadOperationStream = 0;
    manifest.frameOperationStream = 1;

    auto resources = EncodeTable<ResourceArtifact>(description.resources, ResourceStride, [](base::BinaryWriter& writer, const ResourceArtifact& value) {
        WriteId(writer, value.id); writer.WriteU16(static_cast<std::uint16_t>(value.resourceKind)); writer.WriteU16(static_cast<std::uint16_t>(value.origin));
        writer.WriteU16(static_cast<std::uint16_t>(value.rebuildPolicy)); writer.WriteU16(static_cast<std::uint16_t>(value.extentMode)); writer.WriteU32(value.flags);
        writer.WriteU32(value.physicalInstanceCount); WriteId(writer, value.allocation); writer.WriteU32(static_cast<std::uint32_t>(value.format)); writer.WriteU32(value.usageFlags);
        WriteState(writer, value.initialState); writer.WriteU64(value.sizeBytes); writer.WriteU32(value.width); writer.WriteU32(value.height); writer.WriteU16(value.depthOrArraySize);
        writer.WriteU16(value.mipLevels); writer.WriteU16(value.sampleCount); writer.WriteU16(value.planeCount); writer.WriteU64(value.initialDataOffset); writer.WriteU64(value.initialDataSize);
        writer.WriteU32(value.firstView); writer.WriteU32(value.viewCount);
    });
    auto allocations = EncodeTable<AllocationArtifact>(description.allocations, AllocationStride, [](base::BinaryWriter& writer, const AllocationArtifact& value) {
        WriteId(writer, value.id); writer.WriteU16(static_cast<std::uint16_t>(value.kind)); writer.WriteU16(static_cast<std::uint16_t>(value.heapClass)); writer.WriteU32(value.flags);
        writer.WriteU32(value.physicalInstanceCount); writer.WriteU64(value.sizeBytes); writer.WriteU64(value.alignment); writer.WriteU32(value.aliasGroup);
    });
    auto views = EncodeTable<ResourceViewArtifact>(description.views, ViewStride, [](base::BinaryWriter& writer, const ResourceViewArtifact& value) {
        WriteId(writer, value.id); WriteId(writer, value.resource); writer.WriteU16(static_cast<std::uint16_t>(value.viewClass)); writer.WriteU16(0);
        writer.WriteU32(static_cast<std::uint32_t>(value.format)); writer.WriteU32(value.flags); writer.WriteU32(0); writer.WriteU64(value.byteOffset); writer.WriteU64(value.byteSize);
        writer.WriteU32(value.strideBytes); writer.WriteU16(value.firstMip); writer.WriteU16(value.mipCount); writer.WriteU16(value.firstArrayLayer); writer.WriteU16(value.arrayLayerCount);
        writer.WriteU16(value.firstPlane); writer.WriteU16(value.planeCount); writer.WriteU32(value.descriptorHeapClass); writer.WriteU32(value.descriptorIndex); writer.WriteU32(value.descriptorInstanceStride);
    });
    auto shaders = EncodeTable<ShaderArtifact>(description.shaders, ShaderStride, [](base::BinaryWriter& writer, const ShaderArtifact& value) {
        WriteId(writer, value.id); writer.WriteU16(static_cast<std::uint16_t>(value.stage)); writer.WriteU16(static_cast<std::uint16_t>(value.format));
        writer.WriteU16(value.shaderModelMajor); writer.WriteU16(value.shaderModelMinor); writer.WriteU32(value.flags); WriteBlobRef(writer, value.bytecode); writer.WriteBytes(value.bytecodeDigest);
    });
    auto programs = EncodeTable<ProgramArtifact>(description.programs, ProgramStride, [](base::BinaryWriter& writer, const ProgramArtifact& value) {
        WriteId(writer, value.id); writer.WriteU16(static_cast<std::uint16_t>(value.kind)); writer.WriteU16(value.flags); WriteId(writer, value.vertexShader); WriteId(writer, value.pixelShader);
        WriteId(writer, value.computeShader); WriteId(writer, value.bindingLayout); writer.WriteBytes(value.interfaceDigest);
    });
    auto bindings = EncodeTable<BindingLayoutArtifact>(description.bindingLayouts, BindingLayoutStride, [](base::BinaryWriter& writer, const BindingLayoutArtifact& value) {
        WriteId(writer, value.id); writer.WriteU16(value.rootSignatureMajor); writer.WriteU16(value.rootSignatureMinor); writer.WriteU32(value.flags);
        WriteRange(writer, value.parameterRange); WriteRange(writer, value.descriptorRange); WriteRange(writer, value.staticSamplerRange); WriteBlobRef(writer, value.serializedRootSignature);
        writer.WriteBytes(value.layoutDigest); writer.WriteU32(0);
    });
    auto rootParameters = EncodeTable<RootParameterArtifact>(description.rootParameters, RootParameterStride, [](base::BinaryWriter& writer, const RootParameterArtifact& value) {
        WriteId(writer, value.id); writer.WriteU16(static_cast<std::uint16_t>(value.kind)); writer.WriteU16(static_cast<std::uint16_t>(value.visibility)); writer.WriteU32(value.rootParameterIndex);
        writer.WriteU32(value.shaderRegister); writer.WriteU32(value.registerSpace); WriteId(writer, value.dynamicSlot); WriteId(writer, value.staticView); writer.WriteU32(value.flags); writer.WriteU32(0); writer.WriteU32(0);
    });
    auto vertexElements = EncodeTable<VertexElementArtifact>(description.vertexElements, VertexElementStride, [](base::BinaryWriter& writer, const VertexElementArtifact& value) {
        writer.WriteU32(value.id); writer.WriteU16(static_cast<std::uint16_t>(value.meaning)); writer.WriteU16(value.semanticIndex); writer.WriteU32(static_cast<std::uint32_t>(value.format));
        writer.WriteU32(value.inputSlot); writer.WriteU32(value.alignedByteOffset); writer.WriteU32(value.instanceStepRate); writer.WriteU32(value.flags); writer.WriteU32(0);
    });
    auto executables = EncodeTable<RasterExecutableArtifact>(description.executables, ExecutableStride, [](base::BinaryWriter& writer, const RasterExecutableArtifact& value) {
        WriteId(writer, value.id); WriteId(writer, value.program); WriteId(writer, value.bindingLayout); writer.WriteU32(0); WriteRange(writer, value.vertexElementRange); WriteRange(writer, value.colorFormatRange);
        writer.WriteU32(static_cast<std::uint32_t>(value.colorFormat)); writer.WriteU32(static_cast<std::uint32_t>(value.depthFormat)); writer.WriteU32(static_cast<std::uint32_t>(value.primitiveTopology));
        writer.WriteU32(static_cast<std::uint32_t>(value.primitiveTopologyType)); writer.WriteU32(value.rasterStateId); writer.WriteU32(value.blendStateId); writer.WriteU32(value.depthStateId);
        writer.WriteU32(value.sampleCount); writer.WriteU32(value.sampleQuality); writer.WriteU32(0); writer.WriteBytes(value.specializationDigest);
    });

    auto computeExecutables = EncodeTable<ComputeExecutableArtifact>(description.computeExecutables, ComputeExecutableStride, [](base::BinaryWriter& writer, const ComputeExecutableArtifact& value) {
        WriteId(writer, value.id); WriteId(writer, value.program); WriteId(writer, value.bindingLayout); writer.WriteU32(value.flags); writer.WriteBytes(value.specializationDigest);
    });
    auto computeCommands = EncodeTable<ComputeCommandArtifact>(description.computeCommands, ComputeCommandStride, [](base::BinaryWriter& writer, const ComputeCommandArtifact& value) {
        WriteId(writer, value.id); WriteId(writer, value.executable); writer.WriteU32(value.threadGroupCountX); writer.WriteU32(value.threadGroupCountY); writer.WriteU32(value.threadGroupCountZ); writer.WriteU32(value.flags);
    });
    auto attachments = EncodeTable<AttachmentOperationArtifact>(description.attachmentOperations, AttachmentOperationStride, [](base::BinaryWriter& writer, const AttachmentOperationArtifact& value) {
        WriteId(writer, value.id); writer.WriteU16(static_cast<std::uint16_t>(value.colorLoad)); writer.WriteU16(static_cast<std::uint16_t>(value.colorStore));
        for (float component : value.clearColor)
        {
            if (!std::isfinite(component)) throw std::runtime_error("入力または内部状態が検証または実行の契約に違反しています。");
            if (component == 0.0f) component = 0.0f;
            writer.WriteU32(std::bit_cast<std::uint32_t>(component));
        }
        if (!std::isfinite(value.clearDepth) || value.clearDepth < 0.0f || value.clearDepth > 1.0f || value.clearStencil > 255u)
            throw std::runtime_error("入力または内部状態が検証または実行の契約に違反しています。");
        writer.WriteU16(static_cast<std::uint16_t>(value.depthLoad));
        writer.WriteU16(static_cast<std::uint16_t>(value.depthStore));
        float clearDepth = value.clearDepth == 0.0f ? 0.0f : value.clearDepth;
        writer.WriteU32(std::bit_cast<std::uint32_t>(clearDepth));
        writer.WriteU32(value.clearStencil);
        writer.WriteU32(0); writer.WriteU32(0); writer.WriteU32(0);
    });
    auto commands = EncodeTable<RasterCommandArtifact>(description.rasterCommands, RasterCommandStride, [](base::BinaryWriter& writer, const RasterCommandArtifact& value) {
        WriteId(writer, value.id); WriteId(writer, value.executable); WriteRange(writer, value.vertexViewRange); WriteId(writer, value.indexView); WriteRange(writer, value.colorAttachmentRange);
        WriteId(writer, value.depthAttachment); writer.WriteU32(value.vertexCount); writer.WriteU32(value.instanceCount); writer.WriteU32(value.firstVertex); writer.WriteU32(value.firstInstance);
        writer.WriteU32(value.indexCount); writer.WriteU32(value.firstIndex); writer.WriteI32(value.baseVertex); writer.WriteU32(value.viewportId); writer.WriteU32(value.scissorId);
        WriteId(writer, value.attachmentOperation); writer.WriteU32(0);
    });
    auto dynamicSlots = EncodeTable<DynamicDataSlotArtifact>(description.dynamicSlots, DynamicSlotStride, [](base::BinaryWriter& writer, const DynamicDataSlotArtifact& value) {
        WriteId(writer, value.id); WriteId(writer, value.destinationResource); writer.WriteU64(value.destinationOffset); writer.WriteU64(value.requiredBytes);
        writer.WriteU32(value.requiredAlignment); writer.WriteU32(value.flags);
    });
    auto externalSlots = EncodeTable<ExternalResourceSlotArtifact>(description.externalSlots, ExternalSlotStride, [](base::BinaryWriter& writer, const ExternalResourceSlotArtifact& value) {
        WriteId(writer, value.id); WriteId(writer, value.resource); writer.WriteU32(static_cast<std::uint32_t>(value.requiredKind));
        writer.WriteU32(static_cast<std::uint32_t>(value.requiredFormat)); writer.WriteU64(value.minimumBytes);
        WriteState(writer, value.requiredIncomingState); WriteState(writer, value.guaranteedOutgoingState);
        writer.WriteU32(static_cast<std::uint32_t>(value.synchronizationContract)); writer.WriteU32(value.flags);
    });
    auto surfaces = EncodeTable<SurfaceSlotArtifact>(description.surfaceSlots, SurfaceSlotStride, [](base::BinaryWriter& writer, const SurfaceSlotArtifact& value) {
        WriteId(writer, value.id); WriteId(writer, value.imageResource); writer.WriteU32(static_cast<std::uint32_t>(value.requiredFormat)); WriteState(writer, value.acquiredState);
        WriteState(writer, value.presentedState); writer.WriteU32(value.flags);
    });
    auto streams = EncodeTable<OperationStreamArtifact>(description.operationStreams, OperationStreamStride, [](base::BinaryWriter& writer, const OperationStreamArtifact& value) {
        writer.WriteU32(static_cast<std::uint32_t>(value.kind)); writer.WriteU32(value.firstOperation); writer.WriteU32(value.operationCount); writer.WriteU32(value.flags);
    });

    base::BinaryWriter payloadWriter;
    base::BinaryWriter operationWriter;
    for (const auto& operation : description.operations)
    {
        if (!IsKnownOperation(operation.opcode))
            return base::Failure<std::vector<std::byte>, PackageError>(Error(PackageErrorCode::InvalidOperationStream, "Operationが検証または実行の契約に違反しています。"));
        payloadWriter.Align(8);
        const auto offset = payloadWriter.Size();
        payloadWriter.WriteBytes(operation.payload);
        operationWriter.WriteU32(static_cast<std::uint32_t>(operation.opcode));
        operationWriter.WriteU16(operation.operationVersion);
        operationWriter.WriteU16(operation.flags);
        WriteId(operationWriter, operation.queue);
        operationWriter.WriteCountU32(operation.payload.size());
        operationWriter.WriteU64(offset);
    }

    base::BinaryWriter invocation;
    invocation.WriteCountU32(description.dynamicSlots.size());
    invocation.WriteCountU32(description.externalSlots.size());
    invocation.WriteCountU32(description.surfaceSlots.size());
    invocation.WriteU32(0);

    base::BinaryWriter descriptorPlan;
    descriptorPlan.WriteU32(description.profile.rtvDescriptorCount);
    descriptorPlan.WriteU32(description.profile.dsvDescriptorCount);
    descriptorPlan.WriteU32(description.profile.shaderDescriptorCount);
    descriptorPlan.WriteU32(description.profile.samplerDescriptorCount);

    PackageBuildInput input;
    input.targetKind = TargetKindD3D12;
    input.targetSchemaVersion = TargetSchemaVersion;
    input.minimumRuntimeVersion = MinimumRuntimeVersion;
    input.sections.push_back(MakeSection(SectionKind::Manifest, EncodeManifest(manifest), 1, ManifestStride));
    input.sections.push_back(MakeBlobSection(SectionKind::InvocationSchema, std::move(invocation).Take()));
    input.sections.push_back(MakeSection(SectionKind::OperationStreamTable, std::move(streams), static_cast<std::uint32_t>(description.operationStreams.size()), OperationStreamStride));
    input.sections.push_back(MakeSection(SectionKind::OperationTable, std::move(operationWriter).Take(), static_cast<std::uint32_t>(description.operations.size()), OperationStride));
    input.sections.push_back(MakeBlobSection(SectionKind::OperationPayload, std::move(payloadWriter).Take()));
    input.sections.push_back(MakeBlobSection(SectionKind::InitialData, description.initialData));
    input.sections.push_back(MakeBlobSection(SectionKind::ShaderData, description.shaderData));
    input.sections.push_back(MakeBlobSection(SectionKind::NativeObjectData, description.nativeObjectData));
    input.sections.push_back(MakeProvenanceSection(description.provenance));
    input.sections.push_back(MakeSection(SectionKind::D3D12TargetProfile, EncodeProfile(description.profile), 1, ProfileStride));
    input.sections.push_back(MakeSection(SectionKind::D3D12ResourceTable, std::move(resources), static_cast<std::uint32_t>(description.resources.size()), ResourceStride));
    input.sections.push_back(MakeSection(SectionKind::D3D12AllocationTable, std::move(allocations), static_cast<std::uint32_t>(description.allocations.size()), AllocationStride));
    input.sections.push_back(MakeSection(SectionKind::D3D12ViewTable, std::move(views), static_cast<std::uint32_t>(description.views.size()), ViewStride));
    input.sections.push_back(MakeSection(SectionKind::D3D12ShaderTable, std::move(shaders), static_cast<std::uint32_t>(description.shaders.size()), ShaderStride));
    input.sections.push_back(MakeSection(SectionKind::D3D12ProgramTable, std::move(programs), static_cast<std::uint32_t>(description.programs.size()), ProgramStride));
    input.sections.push_back(MakeSection(SectionKind::D3D12BindingLayoutTable, std::move(bindings), static_cast<std::uint32_t>(description.bindingLayouts.size()), BindingLayoutStride));
    input.sections.push_back(MakeSection(SectionKind::D3D12ExecutableTable, std::move(executables), static_cast<std::uint32_t>(description.executables.size()), ExecutableStride));
    input.sections.push_back(MakeSection(SectionKind::D3D12RasterCommandTable, std::move(commands), static_cast<std::uint32_t>(description.rasterCommands.size()), RasterCommandStride));
    input.sections.push_back(MakeSection(SectionKind::D3D12ComputeExecutableTable, std::move(computeExecutables), static_cast<std::uint32_t>(description.computeExecutables.size()), ComputeExecutableStride));
    input.sections.push_back(MakeSection(SectionKind::D3D12ComputeCommandTable, std::move(computeCommands), static_cast<std::uint32_t>(description.computeCommands.size()), ComputeCommandStride));
    input.sections.push_back(MakeBlobSection(SectionKind::D3D12DescriptorPlan, std::move(descriptorPlan).Take()));
    input.sections.push_back(MakeSection(SectionKind::D3D12DynamicSlotTable, std::move(dynamicSlots), static_cast<std::uint32_t>(description.dynamicSlots.size()), DynamicSlotStride));
    input.sections.push_back(MakeSection(SectionKind::D3D12ExternalSlotTable, std::move(externalSlots), static_cast<std::uint32_t>(description.externalSlots.size()), ExternalSlotStride));
    input.sections.push_back(MakeSection(SectionKind::D3D12SurfaceSlotTable, std::move(surfaces), static_cast<std::uint32_t>(description.surfaceSlots.size()), SurfaceSlotStride));
    input.sections.push_back(MakeSection(SectionKind::D3D12VertexElementTable, std::move(vertexElements), static_cast<std::uint32_t>(description.vertexElements.size()), VertexElementStride));
    input.sections.push_back(MakeSection(SectionKind::D3D12AttachmentOperationTable, std::move(attachments), static_cast<std::uint32_t>(description.attachmentOperations.size()), AttachmentOperationStride));
    input.sections.push_back(MakeSection(SectionKind::D3D12RootParameterTable, std::move(rootParameters), static_cast<std::uint32_t>(description.rootParameters.size()), RootParameterStride));
    return PackageWriter::Write(std::move(input));
}

base::Expected<D3D12PackageView, PackageError> D3D12PackageView::Decode(const FrozenExecutablePackage& package)
{
    if (package.Target() != TargetKindD3D12 || package.Header().targetSchemaVersion != TargetSchemaVersion)
        return base::Failure<D3D12PackageView, PackageError>(Error(PackageErrorCode::UnsupportedTargetSchema, "検証または実行の契約に違反しています。"));
    if (package.Header().minimumRuntimeVersion > MinimumRuntimeVersion)
        return base::Failure<D3D12PackageView, PackageError>(Error(PackageErrorCode::TargetCapabilityMismatch, "Runtimeが検証または実行の契約に違反しています。"));

    D3D12PackageView output;
    output.package_ = &package;
    auto manifestSection = RequireSection(package, SectionKind::Manifest, ManifestStride);
    auto invocationSection = RequireSection(package, SectionKind::InvocationSchema);
    auto descriptorPlanSection = RequireSection(package, SectionKind::D3D12DescriptorPlan);
    auto profileSection = RequireSection(package, SectionKind::D3D12TargetProfile, ProfileStride);
    auto resourceSection = RequireSection(package, SectionKind::D3D12ResourceTable, ResourceStride);
    auto allocationSection = RequireSection(package, SectionKind::D3D12AllocationTable, AllocationStride);
    auto viewSection = RequireSection(package, SectionKind::D3D12ViewTable, ViewStride);
    auto shaderSection = RequireSection(package, SectionKind::D3D12ShaderTable, ShaderStride);
    auto programSection = RequireSection(package, SectionKind::D3D12ProgramTable, ProgramStride);
    auto bindingSection = RequireSection(package, SectionKind::D3D12BindingLayoutTable, BindingLayoutStride);
    auto rootParameterSection = RequireSection(package, SectionKind::D3D12RootParameterTable, RootParameterStride);
    auto vertexSection = RequireSection(package, SectionKind::D3D12VertexElementTable, VertexElementStride);
    auto executableSection = RequireSection(package, SectionKind::D3D12ExecutableTable, ExecutableStride);
    auto computeExecutableSection = RequireSection(package, SectionKind::D3D12ComputeExecutableTable, ComputeExecutableStride);
    auto computeCommandSection = RequireSection(package, SectionKind::D3D12ComputeCommandTable, ComputeCommandStride);
    auto attachmentSection = RequireSection(package, SectionKind::D3D12AttachmentOperationTable, AttachmentOperationStride);
    auto commandSection = RequireSection(package, SectionKind::D3D12RasterCommandTable, RasterCommandStride);
    auto dynamicSection = RequireSection(package, SectionKind::D3D12DynamicSlotTable, DynamicSlotStride);
    auto externalSection = RequireSection(package, SectionKind::D3D12ExternalSlotTable, ExternalSlotStride);
    auto surfaceSection = RequireSection(package, SectionKind::D3D12SurfaceSlotTable, SurfaceSlotStride);
    auto streamSection = RequireSection(package, SectionKind::OperationStreamTable, OperationStreamStride);
    auto operationSection = RequireSection(package, SectionKind::OperationTable, OperationStride);
    auto payloadSection = RequireSection(package, SectionKind::OperationPayload);
    auto initialSection = RequireSection(package, SectionKind::InitialData);
    auto shaderDataSection = RequireSection(package, SectionKind::ShaderData);
    auto nativeSection = RequireSection(package, SectionKind::NativeObjectData);

    const base::Expected<const SectionView*, PackageError>* required[] = {
        &manifestSection, &invocationSection, &descriptorPlanSection, &profileSection, &resourceSection, &allocationSection,
        &viewSection, &shaderSection, &programSection, &bindingSection, &rootParameterSection, &vertexSection, &executableSection,
        &computeExecutableSection, &computeCommandSection, &attachmentSection, &commandSection, &dynamicSection, &externalSection, &surfaceSection, &streamSection, &operationSection, &payloadSection,
        &initialSection, &shaderDataSection, &nativeSection
    };
    for (const auto* result : required)
        if (!*result) return base::Failure<D3D12PackageView, PackageError>(result->error());

    auto manifests = DecodeTable<PackageManifest>(*manifestSection.value(), ManifestStride, DecodeManifestRecord);
    auto profiles = DecodeTable<TargetProfile>(*profileSection.value(), ProfileStride, DecodeProfileRecord);
    auto resources = DecodeTable<ResourceArtifact>(*resourceSection.value(), ResourceStride, DecodeResource);
    auto allocations = DecodeTable<AllocationArtifact>(*allocationSection.value(), AllocationStride, DecodeAllocation);
    auto views = DecodeTable<ResourceViewArtifact>(*viewSection.value(), ViewStride, DecodeView);
    auto shaders = DecodeTable<ShaderArtifact>(*shaderSection.value(), ShaderStride, DecodeShader);
    auto programs = DecodeTable<ProgramArtifact>(*programSection.value(), ProgramStride, DecodeProgram);
    auto bindings = DecodeTable<BindingLayoutArtifact>(*bindingSection.value(), BindingLayoutStride, DecodeBinding);
    auto rootParameters = DecodeTable<RootParameterArtifact>(*rootParameterSection.value(), RootParameterStride, DecodeRootParameter);
    auto vertices = DecodeTable<VertexElementArtifact>(*vertexSection.value(), VertexElementStride, DecodeVertexElement);
    auto executables = DecodeTable<RasterExecutableArtifact>(*executableSection.value(), ExecutableStride, DecodeExecutable);
    auto computeExecutables = DecodeTable<ComputeExecutableArtifact>(*computeExecutableSection.value(), ComputeExecutableStride, DecodeComputeExecutable);
    auto computeCommands = DecodeTable<ComputeCommandArtifact>(*computeCommandSection.value(), ComputeCommandStride, DecodeComputeCommand);
    auto attachments = DecodeTable<AttachmentOperationArtifact>(*attachmentSection.value(), AttachmentOperationStride, DecodeAttachment);
    auto commands = DecodeTable<RasterCommandArtifact>(*commandSection.value(), RasterCommandStride, DecodeRasterCommand);
    auto dynamicSlots = DecodeTable<DynamicDataSlotArtifact>(*dynamicSection.value(), DynamicSlotStride, DecodeDynamicSlot);
    auto externalSlots = DecodeTable<ExternalResourceSlotArtifact>(*externalSection.value(), ExternalSlotStride, DecodeExternalSlot);
    auto surfaces = DecodeTable<SurfaceSlotArtifact>(*surfaceSection.value(), SurfaceSlotStride, DecodeSurface);
    auto streams = DecodeTable<OperationStreamArtifact>(*streamSection.value(), OperationStreamStride, DecodeStream);
    auto rawOperations = DecodeTable<RawOperation>(*operationSection.value(), OperationStride, DecodeOperation);

    const PackageError* decodeError = nullptr;
    if (!manifests) decodeError = &manifests.error();
    else if (!profiles) decodeError = &profiles.error();
    else if (!resources) decodeError = &resources.error();
    else if (!allocations) decodeError = &allocations.error();
    else if (!views) decodeError = &views.error();
    else if (!shaders) decodeError = &shaders.error();
    else if (!programs) decodeError = &programs.error();
    else if (!bindings) decodeError = &bindings.error();
    else if (!rootParameters) decodeError = &rootParameters.error();
    else if (!vertices) decodeError = &vertices.error();
    else if (!executables) decodeError = &executables.error();
    else if (!computeExecutables) decodeError = &computeExecutables.error();
    else if (!computeCommands) decodeError = &computeCommands.error();
    else if (!attachments) decodeError = &attachments.error();
    else if (!commands) decodeError = &commands.error();
    else if (!dynamicSlots) decodeError = &dynamicSlots.error();
    else if (!externalSlots) decodeError = &externalSlots.error();
    else if (!surfaces) decodeError = &surfaces.error();
    else if (!streams) decodeError = &streams.error();
    else if (!rawOperations) decodeError = &rawOperations.error();
    if (decodeError) return base::Failure<D3D12PackageView, PackageError>(*decodeError);

    if (manifests.value().size() != 1 || profiles.value().size() != 1 || streams.value().size() != 2)
        return base::Failure<D3D12PackageView, PackageError>(Error(PackageErrorCode::InvalidOperationStream, "Manifestが検証または実行の契約に違反しています。"));

    if (invocationSection.value()->bytes.size() != 16)
        return base::Failure<D3D12PackageView, PackageError>(Error(PackageErrorCode::InvalidInvocationSchema, "Invocationが検証または実行の契約に違反しています。", SectionKind::InvocationSchema));
    base::BinaryReader invocationReader(invocationSection.value()->bytes);
    auto dynamicCount = invocationReader.ReadU32();
    auto externalCount = invocationReader.ReadU32();
    auto surfaceCount = invocationReader.ReadU32();
    auto invocationReserved = invocationReader.ReadU32();
    if (!dynamicCount || !externalCount || !surfaceCount || !invocationReserved ||
        dynamicCount.value() != dynamicSlots.value().size() || externalCount.value() != externalSlots.value().size() ||
        surfaceCount.value() != surfaces.value().size() || invocationReserved.value() != 0)
        return base::Failure<D3D12PackageView, PackageError>(Error(PackageErrorCode::InvalidInvocationSchema, "Invocationが検証または実行の契約に違反しています。", SectionKind::InvocationSchema));

    if (descriptorPlanSection.value()->bytes.size() != 16)
        return base::Failure<D3D12PackageView, PackageError>(Error(PackageErrorCode::InvalidTargetProfile, "Planが検証または実行の契約に違反しています。", SectionKind::D3D12DescriptorPlan));
    base::BinaryReader descriptorReader(descriptorPlanSection.value()->bytes);
    auto rtvCount = descriptorReader.ReadU32();
    auto dsvCount = descriptorReader.ReadU32();
    auto shaderDescriptorCount = descriptorReader.ReadU32();
    auto samplerCount = descriptorReader.ReadU32();
    if (!rtvCount || !dsvCount || !shaderDescriptorCount || !samplerCount ||
        rtvCount.value() != profiles.value()[0].rtvDescriptorCount || dsvCount.value() != profiles.value()[0].dsvDescriptorCount ||
        shaderDescriptorCount.value() != profiles.value()[0].shaderDescriptorCount || samplerCount.value() != profiles.value()[0].samplerDescriptorCount)
        return base::Failure<D3D12PackageView, PackageError>(Error(PackageErrorCode::InvalidTargetProfile, "Planが検証または実行の契約に違反しています。", SectionKind::D3D12DescriptorPlan));

    output.manifest_ = manifests.value()[0];
    output.profile_ = profiles.value()[0];
    output.resources_ = std::move(resources).value();
    output.allocations_ = std::move(allocations).value();
    output.views_ = std::move(views).value();
    output.shaders_ = std::move(shaders).value();
    output.programs_ = std::move(programs).value();
    output.bindingLayouts_ = std::move(bindings).value();
    output.rootParameters_ = std::move(rootParameters).value();
    output.vertexElements_ = std::move(vertices).value();
    output.executables_ = std::move(executables).value();
    output.computeExecutables_ = std::move(computeExecutables).value();
    output.computeCommands_ = std::move(computeCommands).value();
    output.attachmentOperations_ = std::move(attachments).value();
    output.rasterCommands_ = std::move(commands).value();
    output.dynamicSlots_ = std::move(dynamicSlots).value();
    output.externalSlots_ = std::move(externalSlots).value();
    output.surfaceSlots_ = std::move(surfaces).value();
    output.initialData_ = initialSection.value()->bytes;
    output.shaderData_ = shaderDataSection.value()->bytes;
    output.nativeObjectData_ = nativeSection.value()->bytes;

    std::vector<OperationView> operations;
    operations.reserve(rawOperations.value().size());
    std::uint64_t canonicalPayloadEnd = 0;
    for (std::size_t i = 0; i < rawOperations.value().size(); ++i)
    {
        const auto& raw = rawOperations.value()[i];
        if (!IsKnownOperation(raw.opcode) || raw.version != OperationVersion(raw.opcode))
        {
            auto error = Error(PackageErrorCode::InvalidOperationStream, "Operationが検証または実行の契約に違反しています。", SectionKind::OperationTable);
            error.operationIndex = static_cast<std::uint32_t>(i);
            return base::Failure<D3D12PackageView, PackageError>(error);
        }
        const auto expectedOffset = base::AlignUp(canonicalPayloadEnd, 8);
        std::uint64_t end = 0;
        if (raw.payloadOffset != expectedOffset ||
            !base::CheckedAdd(raw.payloadOffset, raw.payloadBytes, end) ||
            end > payloadSection.value()->bytes.size())
        {
            auto error = Error(PackageErrorCode::InvalidOperationStream,
                "OperationがCanonicalな順序または識別子規則に違反しています。",
                SectionKind::OperationPayload);
            error.operationIndex = static_cast<std::uint32_t>(i);
            return base::Failure<D3D12PackageView, PackageError>(error);
        }
        canonicalPayloadEnd = end;
        operations.push_back({raw.opcode, raw.version, raw.flags, raw.queue,
            payloadSection.value()->bytes.subspan(static_cast<std::size_t>(raw.payloadOffset), raw.payloadBytes)});
    }
    if (canonicalPayloadEnd != payloadSection.value()->bytes.size())
        return base::Failure<D3D12PackageView, PackageError>(Error(
            PackageErrorCode::InvalidOperationStream,
            "Sectionが検証または実行の契約に違反しています。",
            SectionKind::OperationPayload));

    const auto& decodedStreams = streams.value();
    for (const auto& stream : decodedStreams)
        if (stream.flags != 0 ||
            static_cast<std::uint64_t>(stream.firstOperation) + stream.operationCount > operations.size())
            return base::Failure<D3D12PackageView, PackageError>(Error(PackageErrorCode::InvalidOperationStream, "Operationが検証または実行の契約に違反しています。", SectionKind::OperationStreamTable));
    if (decodedStreams[0].kind != OperationStreamKind::Load || decodedStreams[1].kind != OperationStreamKind::Frame ||
        decodedStreams[0].firstOperation != 0 ||
        decodedStreams[1].firstOperation != decodedStreams[0].operationCount ||
        static_cast<std::uint64_t>(decodedStreams[1].firstOperation) + decodedStreams[1].operationCount != operations.size() ||
        output.manifest_.loadOperationStream != 0 || output.manifest_.frameOperationStream != 1)
        return base::Failure<D3D12PackageView, PackageError>(Error(PackageErrorCode::InvalidOperationStream, "OperationがCanonicalな順序または識別子規則に違反しています。", SectionKind::OperationStreamTable));

    output.loadOperations_.assign(
        operations.begin() + decodedStreams[0].firstOperation,
        operations.begin() + decodedStreams[0].firstOperation + decodedStreams[0].operationCount);
    output.frameOperations_.assign(
        operations.begin() + decodedStreams[1].firstOperation,
        operations.begin() + decodedStreams[1].firstOperation + decodedStreams[1].operationCount);

    auto references = ValidateReferences(output);
    if (!references) return base::Failure<D3D12PackageView, PackageError>(references.error());
    auto operationValidation = ValidateOperations(output);
    if (!operationValidation) return base::Failure<D3D12PackageView, PackageError>(operationValidation.error());
    return base::Success<D3D12PackageView, PackageError>(std::move(output));
}

base::Expected<std::span<const std::byte>, PackageError> D3D12PackageView::ResolveBlob(const BlobRef& blob) const
{
    if (blob.reserved != 0)
        return base::Failure<std::span<const std::byte>, PackageError>(Error(PackageErrorCode::InvalidBlobReference, "入力または内部状態の数値条件を満たしていません。", blob.section));
    std::span<const std::byte> source;
    if (blob.section == SectionKind::InitialData) source = initialData_;
    else if (blob.section == SectionKind::ShaderData) source = shaderData_;
    else if (blob.section == SectionKind::NativeObjectData) source = nativeObjectData_;
    else return base::Failure<std::span<const std::byte>, PackageError>(Error(PackageErrorCode::InvalidBlobReference, "Sectionが検証または実行の契約に違反しています。", blob.section));
    std::uint64_t end = 0;
    if (!base::CheckedAdd(blob.offset, blob.size, end) || end > source.size())
        return base::Failure<std::span<const std::byte>, PackageError>(Error(PackageErrorCode::InvalidBlobReference, "入力または内部状態が検証または実行の契約に違反しています。", blob.section));
    return base::Success<std::span<const std::byte>, PackageError>(source.subspan(static_cast<std::size_t>(blob.offset), static_cast<std::size_t>(blob.size)));
}

