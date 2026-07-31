template<class T, class DecodeOne>
base::Expected<std::vector<T>, PackageError> DecodeTable(const SectionView& section, std::uint32_t stride, DecodeOne decodeOne)
{
    if (section.descriptor.elementStride != stride ||
        section.bytes.size() != static_cast<std::size_t>(section.descriptor.elementCount) * stride)
        return base::Failure<std::vector<T>, PackageError>(Error(PackageErrorCode::InvalidRecordStride, "Tableが検証または実行の契約に違反しています。", section.descriptor.sectionKind));

    std::vector<T> output;
    output.reserve(section.descriptor.elementCount);
    for (std::uint32_t i = 0; i < section.descriptor.elementCount; ++i)
    {
        base::BinaryReader reader(section.bytes.subspan(static_cast<std::size_t>(i) * stride, stride));
        auto value = decodeOne(reader, section.descriptor.sectionKind);
        if (!value)
        {
            auto error = value.error();
            error.recordIndex = i;
            return base::Failure<std::vector<T>, PackageError>(std::move(error));
        }
        output.push_back(std::move(value).value());
    }
    return base::Success<std::vector<T>, PackageError>(std::move(output));
}

base::Expected<PackageManifest, PackageError> DecodeManifestRecord(base::BinaryReader& reader, SectionKind section)
{
    PackageManifest manifest;
    std::uint32_t* fields[] = {
        &manifest.resourceCount, &manifest.allocationCount, &manifest.viewCount, &manifest.shaderCount,
        &manifest.programCount, &manifest.bindingLayoutCount, &manifest.executableCount, &manifest.rasterCommandCount,
        &manifest.computeExecutableCount, &manifest.computeCommandCount, &manifest.vertexElementCount, &manifest.attachmentOperationCount, &manifest.dynamicSlotCount, &manifest.externalSlotCount,
        &manifest.surfaceSlotCount, &manifest.loadOperationStream, &manifest.frameOperationStream, &manifest.flags
    };
    for (auto* field : fields)
    {
        auto value = reader.ReadU32();
        if (!value) return base::Failure<PackageManifest, PackageError>(Error(PackageErrorCode::InvalidRecordStride, "Manifestが検証または実行の契約に違反しています。", section));
        *field = value.value();
    }
    return base::Success<PackageManifest, PackageError>(manifest);
}

base::Expected<TargetProfile, PackageError> DecodeProfileRecord(base::BinaryReader& reader, SectionKind section)
{
    TargetProfile profile;
    auto featureLevel = reader.ReadU32();
    auto shaderMajor = reader.ReadU16();
    auto shaderMinor = reader.ReadU16();
    auto rootMajor = reader.ReadU16();
    auto rootMinor = reader.ReadU16();
    auto barrier = reader.ReadU32();
    auto binaryFormat = reader.ReadU16();
    auto reserved = reader.ReadU16();
    auto frames = reader.ReadU32();
    auto direct = reader.ReadU32();
    auto compute = reader.ReadU32();
    auto copy = reader.ReadU32();
    auto surface = reader.ReadU32();
    auto rtv = reader.ReadU32();
    auto dsv = reader.ReadU32();
    auto shaderDescriptors = reader.ReadU32();
    auto samplers = reader.ReadU32();
    auto bits0 = reader.ReadU64();
    auto bits1 = reader.ReadU64();
    auto tail = reader.ReadBytes(8);
    if (!featureLevel || !shaderMajor || !shaderMinor || !rootMajor || !rootMinor || !barrier || !binaryFormat ||
        !reserved || !frames || !direct || !compute || !copy || !surface || !rtv || !dsv || !shaderDescriptors ||
        !samplers || !bits0 || !bits1 || !tail)
        return base::Failure<TargetProfile, PackageError>(Error(PackageErrorCode::InvalidTargetProfile, "Fileが検証または実行の契約に違反しています。", section));
    if (reserved.value() != 0 || std::any_of(tail.value().begin(), tail.value().end(), [](std::byte value) { return value != std::byte{0}; }))
        return base::Failure<TargetProfile, PackageError>(Error(PackageErrorCode::InvalidTargetProfile, "Fileが検証または実行の契約に違反しています。", section));

    profile.minimumFeatureLevel = featureLevel.value();
    profile.shaderModelMajor = shaderMajor.value();
    profile.shaderModelMinor = shaderMinor.value();
    profile.rootSignatureMajor = rootMajor.value();
    profile.rootSignatureMinor = rootMinor.value();
    profile.barrierModel = static_cast<BarrierModel>(barrier.value());
    profile.shaderBinaryFormat = static_cast<ShaderBinaryFormat>(binaryFormat.value());
    profile.framesInFlight = frames.value();
    profile.directQueueCount = direct.value();
    profile.computeQueueCount = compute.value();
    profile.copyQueueCount = copy.value();
    profile.surfaceImageCount = surface.value();
    profile.rtvDescriptorCount = rtv.value();
    profile.dsvDescriptorCount = dsv.value();
    profile.shaderDescriptorCount = shaderDescriptors.value();
    profile.samplerDescriptorCount = samplers.value();
    profile.requiredFeatureBits0 = bits0.value();
    profile.requiredFeatureBits1 = bits1.value();

    const auto queueCount = static_cast<std::uint64_t>(profile.directQueueCount) +
                            profile.computeQueueCount + profile.copyQueueCount;
    if (profile.framesInFlight == 0 || profile.directQueueCount != 1 ||
        profile.computeQueueCount > 1 || profile.copyQueueCount > 1 || queueCount > 3 ||
        profile.samplerDescriptorCount != 0 || profile.requiredFeatureBits0 != 0 ||
        profile.requiredFeatureBits1 != 0 ||
        profile.barrierModel != BarrierModel::Legacy ||
        profile.shaderBinaryFormat != ShaderBinaryFormat::Dxbc ||
        profile.shaderModelMajor != 5 || profile.shaderModelMinor > 1 ||
        profile.rootSignatureMajor != 1 || profile.rootSignatureMinor > 1)
        return base::Failure<TargetProfile, PackageError>(Error(PackageErrorCode::InvalidTargetProfile, "Fileが検証または実行の契約に違反しています。", section));
    return base::Success<TargetProfile, PackageError>(profile);
}

base::Expected<ResourceArtifact, PackageError> DecodeResource(base::BinaryReader& reader, SectionKind section)
{
    ResourceArtifact value;
    auto id = reader.ReadU32();
    auto kind = reader.ReadU16();
    auto origin = reader.ReadU16();
    auto rebuild = reader.ReadU16();
    auto extent = reader.ReadU16();
    auto flags = reader.ReadU32();
    auto physical = reader.ReadU32();
    auto allocation = reader.ReadU32();
    auto format = reader.ReadU32();
    auto usage = reader.ReadU32();
    auto state = ReadState(reader, section);
    auto size = reader.ReadU64();
    auto width = reader.ReadU32();
    auto height = reader.ReadU32();
    auto depth = reader.ReadU16();
    auto mips = reader.ReadU16();
    auto samples = reader.ReadU16();
    auto planes = reader.ReadU16();
    auto dataOffset = reader.ReadU64();
    auto dataSize = reader.ReadU64();
    auto firstView = reader.ReadU32();
    auto viewCount = reader.ReadU32();
    auto reserved = reader.ReadBytes(8);
    if (!id || !kind || !origin || !rebuild || !extent || !flags || !physical || !allocation || !format || !usage ||
        !state || !size || !width || !height || !depth || !mips || !samples || !planes || !dataOffset || !dataSize ||
        !firstView || !viewCount || !reserved)
        return base::Failure<ResourceArtifact, PackageError>(Error(PackageErrorCode::InvalidRecordStride, "Resourceが検証または実行の契約に違反しています。", section));
    if (std::any_of(reserved.value().begin(), reserved.value().end(), [](std::byte byte) { return byte != std::byte{0}; }))
        return base::Failure<ResourceArtifact, PackageError>(Error(PackageErrorCode::InvalidEnumValue, "Resourceが検証または実行の契約に違反しています。", section));

    value.id = {id.value()};
    value.resourceKind = static_cast<ResourceKind>(kind.value());
    value.origin = static_cast<ResourceOrigin>(origin.value());
    value.rebuildPolicy = static_cast<RebuildPolicy>(rebuild.value());
    value.extentMode = static_cast<ExtentMode>(extent.value());
    value.flags = flags.value();
    value.physicalInstanceCount = physical.value();
    value.allocation = {allocation.value()};
    value.format = static_cast<Format>(format.value());
    value.usageFlags = usage.value();
    value.initialState = state.value();
    value.sizeBytes = size.value();
    value.width = width.value();
    value.height = height.value();
    value.depthOrArraySize = depth.value();
    value.mipLevels = mips.value();
    value.sampleCount = samples.value();
    value.planeCount = planes.value();
    value.initialDataOffset = dataOffset.value();
    value.initialDataSize = dataSize.value();
    value.firstView = firstView.value();
    value.viewCount = viewCount.value();
    return base::Success<ResourceArtifact, PackageError>(value);
}

base::Expected<AllocationArtifact, PackageError> DecodeAllocation(base::BinaryReader& reader, SectionKind section)
{
    AllocationArtifact value;
    auto id = reader.ReadU32();
    auto kind = reader.ReadU16();
    auto heap = reader.ReadU16();
    auto flags = reader.ReadU32();
    auto physical = reader.ReadU32();
    auto size = reader.ReadU64();
    auto alignment = reader.ReadU64();
    auto alias = reader.ReadU32();
    auto reserved = reader.ReadU32();
    if (!id || !kind || !heap || !flags || !physical || !size || !alignment || !alias || !reserved)
        return base::Failure<AllocationArtifact, PackageError>(Error(PackageErrorCode::InvalidRecordStride, "Allocationが検証または実行の契約に違反しています。", section));
    if (reserved.value() != 0)
        return base::Failure<AllocationArtifact, PackageError>(Error(PackageErrorCode::InvalidEnumValue, "Allocationが検証または実行の契約に違反しています。", section));
    value.id = {id.value()};
    value.kind = static_cast<AllocationKind>(kind.value());
    value.heapClass = static_cast<HeapClass>(heap.value());
    value.flags = flags.value();
    value.physicalInstanceCount = physical.value();
    value.sizeBytes = size.value();
    value.alignment = alignment.value();
    value.aliasGroup = alias.value();
    return base::Success<AllocationArtifact, PackageError>(value);
}

base::Expected<ResourceViewArtifact, PackageError> DecodeView(base::BinaryReader& reader, SectionKind section)
{
    ResourceViewArtifact value;
    auto id = reader.ReadU32();
    auto resource = reader.ReadU32();
    auto viewClass = reader.ReadU16();
    auto reserved16 = reader.ReadU16();
    auto format = reader.ReadU32();
    auto flags = reader.ReadU32();
    auto reserved32 = reader.ReadU32();
    auto offset = reader.ReadU64();
    auto size = reader.ReadU64();
    auto stride = reader.ReadU32();
    auto firstMip = reader.ReadU16();
    auto mipCount = reader.ReadU16();
    auto firstArray = reader.ReadU16();
    auto arrayCount = reader.ReadU16();
    auto firstPlane = reader.ReadU16();
    auto planeCount = reader.ReadU16();
    auto heap = reader.ReadU32();
    auto descriptorIndex = reader.ReadU32();
    auto instanceStride = reader.ReadU32();
    auto tail = reader.ReadU32();
    if (!id || !resource || !viewClass || !reserved16 || !format || !flags || !reserved32 || !offset || !size ||
        !stride || !firstMip || !mipCount || !firstArray || !arrayCount || !firstPlane || !planeCount || !heap ||
        !descriptorIndex || !instanceStride || !tail)
        return base::Failure<ResourceViewArtifact, PackageError>(Error(PackageErrorCode::InvalidRecordStride, "Viewが検証または実行の契約に違反しています。", section));
    if (reserved16.value() != 0 || reserved32.value() != 0 || tail.value() != 0)
        return base::Failure<ResourceViewArtifact, PackageError>(Error(PackageErrorCode::InvalidEnumValue, "Viewが検証または実行の契約に違反しています。", section));

    value.id = {id.value()};
    value.resource = {resource.value()};
    value.viewClass = static_cast<ViewClass>(viewClass.value());
    value.format = static_cast<Format>(format.value());
    value.flags = flags.value();
    value.byteOffset = offset.value();
    value.byteSize = size.value();
    value.strideBytes = stride.value();
    value.firstMip = firstMip.value();
    value.mipCount = mipCount.value();
    value.firstArrayLayer = firstArray.value();
    value.arrayLayerCount = arrayCount.value();
    value.firstPlane = firstPlane.value();
    value.planeCount = planeCount.value();
    value.descriptorHeapClass = heap.value();
    value.descriptorIndex = descriptorIndex.value();
    value.descriptorInstanceStride = instanceStride.value();
    return base::Success<ResourceViewArtifact, PackageError>(value);
}

base::Expected<ShaderArtifact, PackageError> DecodeShader(base::BinaryReader& reader, SectionKind section)
{
    ShaderArtifact value;
    auto id = reader.ReadU32();
    auto stage = reader.ReadU16();
    auto format = reader.ReadU16();
    auto major = reader.ReadU16();
    auto minor = reader.ReadU16();
    auto flags = reader.ReadU32();
    auto blob = ReadBlobRef(reader, section);
    auto digest = reader.ReadBytes(32);
    if (!id || !stage || !format || !major || !minor || !flags || !blob || !digest)
        return base::Failure<ShaderArtifact, PackageError>(Error(PackageErrorCode::InvalidRecordStride, "Shaderが検証または実行の契約に違反しています。", section));
    value.id = {id.value()};
    value.stage = static_cast<ShaderStage>(stage.value());
    value.format = static_cast<ShaderBinaryFormat>(format.value());
    value.shaderModelMajor = major.value();
    value.shaderModelMinor = minor.value();
    value.flags = flags.value();
    value.bytecode = blob.value();
    std::copy(digest.value().begin(), digest.value().end(), value.bytecodeDigest.begin());
    return base::Success<ShaderArtifact, PackageError>(value);
}

base::Expected<ProgramArtifact, PackageError> DecodeProgram(base::BinaryReader& reader, SectionKind section)
{
    ProgramArtifact value;
    auto id = reader.ReadU32();
    auto kind = reader.ReadU16();
    auto flags = reader.ReadU16();
    auto vertex = reader.ReadU32();
    auto pixel = reader.ReadU32();
    auto compute = reader.ReadU32();
    auto layout = reader.ReadU32();
    auto digest = reader.ReadBytes(32);
    if (!id || !kind || !flags || !vertex || !pixel || !compute || !layout || !digest)
        return base::Failure<ProgramArtifact, PackageError>(Error(PackageErrorCode::InvalidRecordStride, "Programが検証または実行の契約に違反しています。", section));
    value.id = {id.value()};
    value.kind = static_cast<ProgramKind>(kind.value());
    value.flags = flags.value();
    value.vertexShader = {vertex.value()};
    value.pixelShader = {pixel.value()};
    value.computeShader = {compute.value()};
    value.bindingLayout = {layout.value()};
    std::copy(digest.value().begin(), digest.value().end(), value.interfaceDigest.begin());
    return base::Success<ProgramArtifact, PackageError>(value);
}

base::Expected<BindingLayoutArtifact, PackageError> DecodeBinding(base::BinaryReader& reader, SectionKind section)
{
    BindingLayoutArtifact value;
    auto id = reader.ReadU32();
    auto major = reader.ReadU16();
    auto minor = reader.ReadU16();
    auto flags = reader.ReadU32();
    auto parameters = ReadRange(reader, section);
    auto descriptors = ReadRange(reader, section);
    auto samplers = ReadRange(reader, section);
    auto blob = ReadBlobRef(reader, section);
    auto digest = reader.ReadBytes(32);
    auto reserved = reader.ReadU32();
    if (!id || !major || !minor || !flags || !parameters || !descriptors || !samplers || !blob || !digest || !reserved)
        return base::Failure<BindingLayoutArtifact, PackageError>(Error(PackageErrorCode::InvalidRecordStride, "Bindingが検証または実行の契約に違反しています。", section));
    if (reserved.value() != 0)
        return base::Failure<BindingLayoutArtifact, PackageError>(Error(PackageErrorCode::InvalidEnumValue, "Bindingが検証または実行の契約に違反しています。", section));
    value.id = {id.value()};
    value.rootSignatureMajor = major.value();
    value.rootSignatureMinor = minor.value();
    value.flags = flags.value();
    value.parameterRange = parameters.value();
    value.descriptorRange = descriptors.value();
    value.staticSamplerRange = samplers.value();
    value.serializedRootSignature = blob.value();
    std::copy(digest.value().begin(), digest.value().end(), value.layoutDigest.begin());
    return base::Success<BindingLayoutArtifact, PackageError>(value);
}

base::Expected<RootParameterArtifact, PackageError> DecodeRootParameter(base::BinaryReader& reader, SectionKind section)
{
    RootParameterArtifact value;
    auto id = reader.ReadU32();
    auto kind = reader.ReadU16();
    auto visibility = reader.ReadU16();
    auto rootIndex = reader.ReadU32();
    auto shaderRegister = reader.ReadU32();
    auto registerSpace = reader.ReadU32();
    auto dynamicSlot = reader.ReadU32();
    auto staticView = reader.ReadU32();
    auto flags = reader.ReadU32();
    auto reserved = reader.ReadU32();
    auto tail = reader.ReadU32();
    if (!id || !kind || !visibility || !rootIndex || !shaderRegister || !registerSpace || !dynamicSlot || !staticView || !flags || !reserved || !tail)
        return base::Failure<RootParameterArtifact, PackageError>(Error(PackageErrorCode::InvalidRecordStride, "入力または内部状態が検証または実行の契約に違反しています。", section));
    if (reserved.value() != 0 || tail.value() != 0)
        return base::Failure<RootParameterArtifact, PackageError>(Error(PackageErrorCode::InvalidEnumValue, "入力または内部状態の数値条件を満たしていません。", section));
    value.id = {id.value()};
    value.kind = static_cast<RootParameterKind>(kind.value());
    value.visibility = static_cast<ShaderVisibility>(visibility.value());
    value.rootParameterIndex = rootIndex.value();
    value.shaderRegister = shaderRegister.value();
    value.registerSpace = registerSpace.value();
    value.dynamicSlot = {dynamicSlot.value()};
    value.staticView = {staticView.value()};
    value.flags = flags.value();
    return base::Success<RootParameterArtifact, PackageError>(value);
}

base::Expected<VertexElementArtifact, PackageError> DecodeVertexElement(base::BinaryReader& reader, SectionKind section)
{
    VertexElementArtifact value;
    auto id = reader.ReadU32();
    auto meaning = reader.ReadU16();
    auto semanticIndex = reader.ReadU16();
    auto format = reader.ReadU32();
    auto inputSlot = reader.ReadU32();
    auto offset = reader.ReadU32();
    auto step = reader.ReadU32();
    auto flags = reader.ReadU32();
    auto reserved = reader.ReadU32();
    if (!id || !meaning || !semanticIndex || !format || !inputSlot || !offset || !step || !flags || !reserved)
        return base::Failure<VertexElementArtifact, PackageError>(Error(PackageErrorCode::InvalidRecordStride, "入力または内部状態が検証または実行の契約に違反しています。", section));
    if (reserved.value() != 0)
        return base::Failure<VertexElementArtifact, PackageError>(Error(PackageErrorCode::InvalidEnumValue, "入力または内部状態の数値条件を満たしていません。", section));
    value.id = id.value();
    value.meaning = static_cast<VertexMeaning>(meaning.value());
    value.semanticIndex = semanticIndex.value();
    value.format = static_cast<Format>(format.value());
    value.inputSlot = inputSlot.value();
    value.alignedByteOffset = offset.value();
    value.instanceStepRate = step.value();
    value.flags = flags.value();
    return base::Success<VertexElementArtifact, PackageError>(value);
}

base::Expected<RasterExecutableArtifact, PackageError> DecodeExecutable(base::BinaryReader& reader, SectionKind section)
{
    RasterExecutableArtifact value;
    auto id = reader.ReadU32();
    auto program = reader.ReadU32();
    auto layout = reader.ReadU32();
    auto reserved0 = reader.ReadU32();
    auto vertexRange = ReadRange(reader, section);
    auto colorRange = ReadRange(reader, section);
    auto colorFormat = reader.ReadU32();
    auto depthFormat = reader.ReadU32();
    auto topology = reader.ReadU32();
    auto topologyType = reader.ReadU32();
    auto rasterState = reader.ReadU32();
    auto blendState = reader.ReadU32();
    auto depthState = reader.ReadU32();
    auto samples = reader.ReadU32();
    auto quality = reader.ReadU32();
    auto reserved1 = reader.ReadU32();
    auto digest = reader.ReadBytes(32);
    if (!id || !program || !layout || !reserved0 || !vertexRange || !colorRange || !colorFormat || !depthFormat ||
        !topology || !topologyType || !rasterState || !blendState || !depthState || !samples || !quality || !reserved1 || !digest)
        return base::Failure<RasterExecutableArtifact, PackageError>(Error(PackageErrorCode::InvalidRecordStride, "Tableが検証または実行の契約に違反しています。", section));
    if (reserved0.value() != 0 || reserved1.value() != 0)
        return base::Failure<RasterExecutableArtifact, PackageError>(Error(PackageErrorCode::InvalidEnumValue, "Tableが検証または実行の契約に違反しています。", section));
    value.id = {id.value()};
    value.program = {program.value()};
    value.bindingLayout = {layout.value()};
    value.vertexElementRange = vertexRange.value();
    value.colorFormatRange = colorRange.value();
    value.colorFormat = static_cast<Format>(colorFormat.value());
    value.depthFormat = static_cast<Format>(depthFormat.value());
    value.primitiveTopology = static_cast<PrimitiveTopology>(topology.value());
    value.primitiveTopologyType = static_cast<PrimitiveTopologyType>(topologyType.value());
    value.rasterStateId = rasterState.value();
    value.blendStateId = blendState.value();
    value.depthStateId = depthState.value();
    value.sampleCount = samples.value();
    value.sampleQuality = quality.value();
    std::copy(digest.value().begin(), digest.value().end(), value.specializationDigest.begin());
    return base::Success<RasterExecutableArtifact, PackageError>(value);
}

base::Expected<ComputeExecutableArtifact, PackageError> DecodeComputeExecutable(base::BinaryReader& reader, SectionKind section)
{
    ComputeExecutableArtifact value;
    auto id = reader.ReadU32();
    auto program = reader.ReadU32();
    auto layout = reader.ReadU32();
    auto flags = reader.ReadU32();
    auto digest = reader.ReadBytes(32);
    if (!id || !program || !layout || !flags || !digest)
        return base::Failure<ComputeExecutableArtifact, PackageError>(Error(PackageErrorCode::InvalidRecordStride, "Tableが検証または実行の契約に違反しています。", section));
    value.id = {id.value()};
    value.program = {program.value()};
    value.bindingLayout = {layout.value()};
    value.flags = flags.value();
    std::copy(digest.value().begin(), digest.value().end(), value.specializationDigest.begin());
    return base::Success<ComputeExecutableArtifact, PackageError>(value);
}

base::Expected<ComputeCommandArtifact, PackageError> DecodeComputeCommand(base::BinaryReader& reader, SectionKind section)
{
    ComputeCommandArtifact value;
    auto id = reader.ReadU32();
    auto executable = reader.ReadU32();
    auto x = reader.ReadU32();
    auto y = reader.ReadU32();
    auto z = reader.ReadU32();
    auto flags = reader.ReadU32();
    if (!id || !executable || !x || !y || !z || !flags)
        return base::Failure<ComputeCommandArtifact, PackageError>(Error(PackageErrorCode::InvalidRecordStride, "入力または内部状態が検証または実行の契約に違反しています。", section));
    value.id = {id.value()};
    value.executable = {executable.value()};
    value.threadGroupCountX = x.value();
    value.threadGroupCountY = y.value();
    value.threadGroupCountZ = z.value();
    value.flags = flags.value();
    return base::Success<ComputeCommandArtifact, PackageError>(value);
}

base::Expected<AttachmentOperationArtifact, PackageError> DecodeAttachment(base::BinaryReader& reader, SectionKind section)
{
    AttachmentOperationArtifact value;
    auto id = reader.ReadU32();
    auto colorLoad = reader.ReadU16();
    auto colorStore = reader.ReadU16();
    if (!id || !colorLoad || !colorStore)
        return base::Failure<AttachmentOperationArtifact, PackageError>(Error(PackageErrorCode::InvalidRecordStride, "入力または内部状態が検証または実行の契約に違反しています。", section));
    value.id = {id.value()};
    value.colorLoad = static_cast<AttachmentLoadOp>(colorLoad.value());
    value.colorStore = static_cast<AttachmentStoreOp>(colorStore.value());
    for (float& component : value.clearColor)
    {
        auto bits = reader.ReadU32();
        if (!bits) return base::Failure<AttachmentOperationArtifact, PackageError>(Error(PackageErrorCode::InvalidRecordStride, "入力または内部状態が検証または実行の契約に違反しています。", section));
        component = std::bit_cast<float>(bits.value());
        if (!std::isfinite(component))
            return base::Failure<AttachmentOperationArtifact, PackageError>(Error(PackageErrorCode::InvalidEnumValue, "入力または内部状態が検証または実行の契約に違反しています。", section));
    }
    auto depthLoad = reader.ReadU16();
    auto depthStore = reader.ReadU16();
    auto clearDepthBits = reader.ReadU32();
    auto clearStencil = reader.ReadU32();
    auto reserved0 = reader.ReadU32();
    auto reserved1 = reader.ReadU32();
    auto reserved2 = reader.ReadU32();
    if (!depthLoad || !depthStore || !clearDepthBits || !clearStencil || !reserved0 || !reserved1 || !reserved2)
        return base::Failure<AttachmentOperationArtifact, PackageError>(Error(PackageErrorCode::InvalidRecordStride, "入力または内部状態が検証または実行の契約に違反しています。", section));
    value.depthLoad = static_cast<AttachmentLoadOp>(depthLoad.value());
    value.depthStore = static_cast<AttachmentStoreOp>(depthStore.value());
    value.clearDepth = std::bit_cast<float>(clearDepthBits.value());
    value.clearStencil = clearStencil.value();
    if (!std::isfinite(value.clearDepth) || value.clearDepth < 0.0f || value.clearDepth > 1.0f || value.clearStencil > 255u)
        return base::Failure<AttachmentOperationArtifact, PackageError>(Error(PackageErrorCode::InvalidEnumValue, "入力または内部状態が検証または実行の契約に違反しています。", section));
    if (reserved0.value() != 0 || reserved1.value() != 0 || reserved2.value() != 0)
        return base::Failure<AttachmentOperationArtifact, PackageError>(Error(PackageErrorCode::InvalidEnumValue, "入力または内部状態の数値条件を満たしていません。", section));
    return base::Success<AttachmentOperationArtifact, PackageError>(value);
}

base::Expected<RasterCommandArtifact, PackageError> DecodeRasterCommand(base::BinaryReader& reader, SectionKind section)
{
    RasterCommandArtifact value;
    auto id = reader.ReadU32();
    auto executable = reader.ReadU32();
    auto vertexViews = ReadRange(reader, section);
    auto indexView = reader.ReadU32();
    auto colorAttachments = ReadRange(reader, section);
    auto depthAttachment = reader.ReadU32();
    auto vertexCount = reader.ReadU32();
    auto instanceCount = reader.ReadU32();
    auto firstVertex = reader.ReadU32();
    auto firstInstance = reader.ReadU32();
    auto indexCount = reader.ReadU32();
    auto firstIndex = reader.ReadU32();
    auto baseVertex = reader.ReadI32();
    auto viewport = reader.ReadU32();
    auto scissor = reader.ReadU32();
    auto attachment = reader.ReadU32();
    auto reserved = reader.ReadBytes(4);
    if (!id || !executable || !vertexViews || !indexView || !colorAttachments || !depthAttachment || !vertexCount ||
        !instanceCount || !firstVertex || !firstInstance || !indexCount || !firstIndex || !baseVertex || !viewport ||
        !scissor || !attachment || !reserved)
        return base::Failure<RasterCommandArtifact, PackageError>(Error(PackageErrorCode::InvalidRecordStride, "入力または内部状態が検証または実行の契約に違反しています。", section));
    if (std::any_of(reserved.value().begin(), reserved.value().end(), [](std::byte byte) { return byte != std::byte{0}; }))
        return base::Failure<RasterCommandArtifact, PackageError>(Error(PackageErrorCode::InvalidEnumValue, "入力または内部状態の数値条件を満たしていません。", section));
    value.id = {id.value()};
    value.executable = {executable.value()};
    value.vertexViewRange = vertexViews.value();
    value.indexView = {indexView.value()};
    value.colorAttachmentRange = colorAttachments.value();
    value.depthAttachment = {depthAttachment.value()};
    value.vertexCount = vertexCount.value();
    value.instanceCount = instanceCount.value();
    value.firstVertex = firstVertex.value();
    value.firstInstance = firstInstance.value();
    value.indexCount = indexCount.value();
    value.firstIndex = firstIndex.value();
    value.baseVertex = baseVertex.value();
    value.viewportId = viewport.value();
    value.scissorId = scissor.value();
    value.attachmentOperation = {attachment.value()};
    return base::Success<RasterCommandArtifact, PackageError>(value);
}

base::Expected<DynamicDataSlotArtifact, PackageError> DecodeDynamicSlot(base::BinaryReader& reader, SectionKind section)
{
    DynamicDataSlotArtifact value;
    auto id = reader.ReadU32();
    auto resource = reader.ReadU32();
    auto offset = reader.ReadU64();
    auto bytes = reader.ReadU64();
    auto alignment = reader.ReadU32();
    auto flags = reader.ReadU32();
    if (!id || !resource || !offset || !bytes || !alignment || !flags)
        return base::Failure<DynamicDataSlotArtifact, PackageError>(Error(PackageErrorCode::InvalidRecordStride, "入力または内部状態が検証または実行の契約に違反しています。", section));
    value.id = {id.value()};
    value.destinationResource = {resource.value()};
    value.destinationOffset = offset.value();
    value.requiredBytes = bytes.value();
    value.requiredAlignment = alignment.value();
    value.flags = flags.value();
    return base::Success<DynamicDataSlotArtifact, PackageError>(value);
}

base::Expected<ExternalResourceSlotArtifact, PackageError> DecodeExternalSlot(base::BinaryReader& reader, SectionKind section)
{
    ExternalResourceSlotArtifact value;
    auto id = reader.ReadU32(); auto resource = reader.ReadU32(); auto kind = reader.ReadU32(); auto format = reader.ReadU32();
    auto minimumBytes = reader.ReadU64(); auto incoming = ReadState(reader, section); auto outgoing = ReadState(reader, section);
    auto synchronization = reader.ReadU32(); auto flags = reader.ReadU32();
    if (!id || !resource || !kind || !format || !minimumBytes || !incoming || !outgoing || !synchronization || !flags)
        return base::Failure<ExternalResourceSlotArtifact, PackageError>(Error(PackageErrorCode::InvalidRecordStride, "入力または内部状態が検証または実行の契約に違反しています。", section));
    value.id={id.value()}; value.resource={resource.value()}; value.requiredKind=static_cast<ResourceKind>(kind.value());
    value.requiredFormat=static_cast<Format>(format.value()); value.minimumBytes=minimumBytes.value();
    value.requiredIncomingState=incoming.value(); value.guaranteedOutgoingState=outgoing.value();
    value.synchronizationContract=static_cast<ExternalSynchronizationContract>(synchronization.value()); value.flags=flags.value();
    return base::Success<ExternalResourceSlotArtifact, PackageError>(value);
}

base::Expected<SurfaceSlotArtifact, PackageError> DecodeSurface(base::BinaryReader& reader, SectionKind section)
{
    SurfaceSlotArtifact value;
    auto id = reader.ReadU32();
    auto resource = reader.ReadU32();
    auto format = reader.ReadU32();
    auto acquired = ReadState(reader, section);
    auto presented = ReadState(reader, section);
    auto flags = reader.ReadU32();
    if (!id || !resource || !format || !acquired || !presented || !flags)
        return base::Failure<SurfaceSlotArtifact, PackageError>(Error(PackageErrorCode::InvalidRecordStride, "Surfaceが検証または実行の契約に違反しています。", section));
    value.id = {id.value()};
    value.imageResource = {resource.value()};
    value.requiredFormat = static_cast<Format>(format.value());
    value.acquiredState = acquired.value();
    value.presentedState = presented.value();
    value.flags = flags.value();
    return base::Success<SurfaceSlotArtifact, PackageError>(value);
}

base::Expected<OperationStreamArtifact, PackageError> DecodeStream(base::BinaryReader& reader, SectionKind section)
{
    OperationStreamArtifact value;
    auto kind = reader.ReadU32();
    auto first = reader.ReadU32();
    auto count = reader.ReadU32();
    auto flags = reader.ReadU32();
    if (!kind || !first || !count || !flags)
        return base::Failure<OperationStreamArtifact, PackageError>(Error(PackageErrorCode::InvalidOperationStream, "Operationが検証または実行の契約に違反しています。", section));
    value.kind = static_cast<OperationStreamKind>(kind.value());
    value.firstOperation = first.value();
    value.operationCount = count.value();
    value.flags = flags.value();
    return base::Success<OperationStreamArtifact, PackageError>(value);
}

struct RawOperation final
{
    D3D12OperationCode opcode{};
    std::uint16_t version = 1;
    std::uint16_t flags = 0;
    QueueId queue;
    std::uint32_t payloadBytes = 0;
    std::uint64_t payloadOffset = 0;
};

base::Expected<RawOperation, PackageError> DecodeOperation(base::BinaryReader& reader, SectionKind section)
{
    RawOperation value;
    auto code = reader.ReadU32();
    auto version = reader.ReadU16();
    auto flags = reader.ReadU16();
    auto queue = reader.ReadU32();
    auto payloadBytes = reader.ReadU32();
    auto payloadOffset = reader.ReadU64();
    if (!code || !version || !flags || !queue || !payloadBytes || !payloadOffset)
        return base::Failure<RawOperation, PackageError>(Error(PackageErrorCode::InvalidOperationStream, "Operationが検証または実行の契約に違反しています。", section));
    value.opcode = static_cast<D3D12OperationCode>(code.value());
    value.version = version.value();
    value.flags = flags.value();
    value.queue = {queue.value()};
    value.payloadBytes = payloadBytes.value();
    value.payloadOffset = payloadOffset.value();
    return base::Success<RawOperation, PackageError>(value);
}

