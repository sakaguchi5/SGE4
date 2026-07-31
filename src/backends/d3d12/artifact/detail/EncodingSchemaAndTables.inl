constexpr std::uint32_t TargetSchemaVersion = 17;
constexpr std::uint32_t MinimumRuntimeVersion = 17;
constexpr std::uint32_t ManifestStride = 72;
constexpr std::uint32_t ProfileStride = 80;
constexpr std::uint32_t ResourceStride = 96;
constexpr std::uint32_t AllocationStride = 40;
constexpr std::uint32_t ViewStride = 72;
constexpr std::uint32_t ShaderStride = 72;
constexpr std::uint32_t ProgramStride = 56;
constexpr std::uint32_t BindingLayoutStride = 104;
constexpr std::uint32_t RootParameterStride = 40;
constexpr std::uint32_t VertexElementStride = 32;
constexpr std::uint32_t ExecutableStride = 104;
constexpr std::uint32_t ComputeExecutableStride = 48;
constexpr std::uint32_t ComputeCommandStride = 24;
constexpr std::uint32_t AttachmentOperationStride = 48;
constexpr std::uint32_t RasterCommandStride = 80;
constexpr std::uint32_t DynamicSlotStride = 32;
constexpr std::uint32_t ExternalSlotStride = 48;
constexpr std::uint32_t SurfaceSlotStride = 32;
constexpr std::uint32_t OperationStreamStride = 16;
constexpr std::uint32_t OperationStride = 24;

constexpr std::array OperationContractTable = {
    OperationContract{D3D12OperationCode::CreateDescriptorHeaps, 1},
    OperationContract{D3D12OperationCode::CreateResource, 1},
    OperationContract{D3D12OperationCode::UploadBuffer, 1},
    OperationContract{D3D12OperationCode::CreateRootSignature, 1},
    OperationContract{D3D12OperationCode::CreateGraphicsPipeline, 1},
    OperationContract{D3D12OperationCode::InitializeState, 1},
    OperationContract{D3D12OperationCode::VerifyBufferContents, 1},
    OperationContract{D3D12OperationCode::UploadTexture, 1},
    OperationContract{D3D12OperationCode::VerifyTextureContents, 1},
    OperationContract{D3D12OperationCode::CreateComputePipeline, 1},
    OperationContract{D3D12OperationCode::AcquireSurfaceImage, 1},
    OperationContract{D3D12OperationCode::AcquireExternal, 1},
    OperationContract{D3D12OperationCode::WaitExternal, 1},
    OperationContract{D3D12OperationCode::ApplyDynamicData, 1},
    OperationContract{D3D12OperationCode::BeginQueueBatch, 1},
    OperationContract{D3D12OperationCode::Transition, 1},
    OperationContract{D3D12OperationCode::ActivateAlias, 1},
    OperationContract{D3D12OperationCode::ExecuteRaster, 1},
    OperationContract{D3D12OperationCode::ExecuteCompute, 1},
    OperationContract{D3D12OperationCode::ExecuteCopy, 2},
    OperationContract{D3D12OperationCode::EndQueueBatch, 1},
    OperationContract{D3D12OperationCode::SignalQueue, 2},
    OperationContract{D3D12OperationCode::WaitQueue, 2},
    OperationContract{D3D12OperationCode::WaitTemporal, 2},
    OperationContract{D3D12OperationCode::ReleaseExternal, 2},
    OperationContract{D3D12OperationCode::PresentSurface, 1},
};
static_assert(base::ValuesAreUnique(OperationContractTable,
    [](const OperationContract& contract) { return std::to_underlying(contract.code); }));
static_assert(base::AllValuesSatisfy(OperationContractTable,
    [](const OperationContract& contract) { return contract.version != 0; }));


PackageError Error(PackageErrorCode code, std::string message, SectionKind section = {})
{
    PackageError error;
    error.code = code;
    error.section = section;
    error.message = std::move(message);
    return error;
}

std::uint32_t Flags(SectionFlags a, SectionFlags b)
{
    return static_cast<std::uint32_t>(a | b);
}

void WriteState(base::BinaryWriter& writer, const ResourceState& state)
{
    writer.WriteU16(static_cast<std::uint16_t>(state.stateClass));
    writer.WriteU16(state.reserved);
    writer.WriteU32(state.explicitBits);
}

base::Expected<ResourceState, PackageError> ReadState(base::BinaryReader& reader, SectionKind section)
{
    auto stateClass = reader.ReadU16();
    auto reserved = reader.ReadU16();
    auto bits = reader.ReadU32();
    if (!stateClass || !reserved || !bits)
        return base::Failure<ResourceState, PackageError>(Error(PackageErrorCode::InvalidRecordStride, "Resourceが検証または実行の契約に違反しています。", section));
    ResourceState state{static_cast<StateClass>(stateClass.value()), reserved.value(), bits.value()};
    if (state.reserved != 0)
        return base::Failure<ResourceState, PackageError>(Error(PackageErrorCode::InvalidEnumValue, "Resourceが検証または実行の契約に違反しています。", section));
    if (state.stateClass != StateClass::Common && state.stateClass != StateClass::Present && state.stateClass != StateClass::Explicit)
        return base::Failure<ResourceState, PackageError>(Error(PackageErrorCode::InvalidEnumValue, "Resourceが検証または実行の契約に違反しています。", section));
    if (state.stateClass != StateClass::Explicit && state.explicitBits != 0)
        return base::Failure<ResourceState, PackageError>(Error(PackageErrorCode::InvalidEnumValue, "Stateの状態または世代が実行契約と一致しません。", section));
    if (state.stateClass == StateClass::Explicit)
    {
        constexpr std::uint32_t KnownExplicitBits =
            static_cast<std::uint32_t>(ExplicitStateBits::VertexBuffer) |
            static_cast<std::uint32_t>(ExplicitStateBits::ConstantBuffer) |
            static_cast<std::uint32_t>(ExplicitStateBits::IndexBuffer) |
            static_cast<std::uint32_t>(ExplicitStateBits::RenderTarget) |
            static_cast<std::uint32_t>(ExplicitStateBits::DepthWrite) |
            static_cast<std::uint32_t>(ExplicitStateBits::DepthRead) |
            static_cast<std::uint32_t>(ExplicitStateBits::ShaderRead) |
            static_cast<std::uint32_t>(ExplicitStateBits::UnorderedWrite) |
            static_cast<std::uint32_t>(ExplicitStateBits::CopySource) |
            static_cast<std::uint32_t>(ExplicitStateBits::CopyDestination) |
            static_cast<std::uint32_t>(ExplicitStateBits::IndirectArgument) |
            static_cast<std::uint32_t>(ExplicitStateBits::PixelShaderRead) |
            static_cast<std::uint32_t>(ExplicitStateBits::NonPixelShaderRead);
        if (state.explicitBits == 0 || (state.explicitBits & ~KnownExplicitBits) != 0)
            return base::Failure<ResourceState, PackageError>(Error(PackageErrorCode::InvalidEnumValue, "Stateの状態または世代が実行契約と一致しません。", section));
    }
    return base::Success<ResourceState, PackageError>(state);
}

bool IsCopyQueueState(const ResourceState& state) noexcept
{
    if (state.reserved != 0) return false;
    if (state.stateClass == StateClass::Common) return state.explicitBits == 0;
    if (state.stateClass != StateClass::Explicit) return false;
    const auto copySource = static_cast<std::uint32_t>(ExplicitStateBits::CopySource);
    const auto copyDestination = static_cast<std::uint32_t>(ExplicitStateBits::CopyDestination);
    return state.explicitBits == copySource || state.explicitBits == copyDestination;
}

void WriteBlobRef(base::BinaryWriter& writer, const BlobRef& blob)
{
    writer.WriteU32(static_cast<std::uint32_t>(blob.section));
    writer.WriteU32(blob.reserved);
    writer.WriteU64(blob.offset);
    writer.WriteU64(blob.size);
}

base::Expected<BlobRef, PackageError> ReadBlobRef(base::BinaryReader& reader, SectionKind owner)
{
    auto section = reader.ReadU32();
    auto reserved = reader.ReadU32();
    auto offset = reader.ReadU64();
    auto size = reader.ReadU64();
    if (!section || !reserved || !offset || !size)
        return base::Failure<BlobRef, PackageError>(Error(PackageErrorCode::InvalidRecordStride, "入力または内部状態の参照先または所有関係が無効です。", owner));
    BlobRef blob{static_cast<SectionKind>(section.value()), reserved.value(), offset.value(), size.value()};
    if (blob.reserved != 0)
        return base::Failure<BlobRef, PackageError>(Error(PackageErrorCode::InvalidBlobReference, "入力または内部状態の参照先または所有関係が無効です。", owner));
    return base::Success<BlobRef, PackageError>(blob);
}

void WriteRange(base::BinaryWriter& writer, IndexRange range)
{
    writer.WriteU32(range.first);
    writer.WriteU32(range.count);
}

base::Expected<IndexRange, PackageError> ReadRange(base::BinaryReader& reader, SectionKind section)
{
    auto first = reader.ReadU32();
    auto count = reader.ReadU32();
    if (!first || !count)
        return base::Failure<IndexRange, PackageError>(Error(PackageErrorCode::InvalidRecordStride, "入力または内部状態が検証または実行の契約に違反しています。", section));
    return base::Success<IndexRange, PackageError>({first.value(), count.value()});
}

template<class Id>
void WriteId(base::BinaryWriter& writer, Id id)
{
    writer.WriteU32(id.value);
}

template<class T, class EncodeOne>
std::vector<std::byte> EncodeTable(std::span<const T> records, std::uint32_t stride, EncodeOne encodeOne)
{
    base::BinaryWriter writer;
    for (const auto& record : records)
    {
        const auto start = writer.Size();
        encodeOne(writer, record);
        if (writer.Size() - start > stride) throw std::runtime_error("検証または実行の契約に違反しています。");
        writer.WriteZeroes(stride - (writer.Size() - start));
    }
    return std::move(writer).Take();
}

PackageSectionInput MakeSection(
    SectionKind kind,
    std::vector<std::byte> bytes,
    std::uint32_t count,
    std::uint32_t stride,
    std::uint32_t alignment = 8)
{
    PackageSectionInput section;
    section.kind = kind;
    section.schemaVersion = 1;
    section.flags = Flags(SectionFlags::Required, SectionFlags::ExecutionAffecting);
    section.alignment = alignment;
    section.elementCount = count;
    section.elementStride = stride;
    section.bytes = std::move(bytes);
    return section;
}

PackageSectionInput MakeBlobSection(SectionKind kind, std::vector<std::byte> bytes, bool required = true)
{
    PackageSectionInput section;
    section.kind = kind;
    section.schemaVersion = 1;
    section.flags = static_cast<std::uint32_t>(SectionFlags::ExecutionAffecting) |
        (required ? static_cast<std::uint32_t>(SectionFlags::Required) : 0u);
    section.alignment = 16;
    section.bytes = std::move(bytes);
    return section;
}

PackageSectionInput MakeProvenanceSection(std::vector<std::byte> bytes)
{
    PackageSectionInput section;
    section.kind = SectionKind::Provenance;
    section.schemaVersion = 1;
    section.flags = static_cast<std::uint32_t>(SectionFlags::Required) |
        static_cast<std::uint32_t>(SectionFlags::OpaqueToCore);
    section.alignment = 8;
    section.bytes = std::move(bytes);
    return section;
}

std::vector<std::byte> EncodeManifest(const PackageManifest& value)
{
    base::BinaryWriter writer;
    writer.WriteU32(value.resourceCount);
    writer.WriteU32(value.allocationCount);
    writer.WriteU32(value.viewCount);
    writer.WriteU32(value.shaderCount);
    writer.WriteU32(value.programCount);
    writer.WriteU32(value.bindingLayoutCount);
    writer.WriteU32(value.executableCount);
    writer.WriteU32(value.rasterCommandCount);
    writer.WriteU32(value.computeExecutableCount);
    writer.WriteU32(value.computeCommandCount);
    writer.WriteU32(value.vertexElementCount);
    writer.WriteU32(value.attachmentOperationCount);
    writer.WriteU32(value.dynamicSlotCount);
    writer.WriteU32(value.externalSlotCount);
    writer.WriteU32(value.surfaceSlotCount);
    writer.WriteU32(value.loadOperationStream);
    writer.WriteU32(value.frameOperationStream);
    writer.WriteU32(value.flags);
    return std::move(writer).Take();
}

std::vector<std::byte> EncodeProfile(const TargetProfile& profile)
{
    base::BinaryWriter writer;
    writer.WriteU32(profile.minimumFeatureLevel);
    writer.WriteU16(profile.shaderModelMajor);
    writer.WriteU16(profile.shaderModelMinor);
    writer.WriteU16(profile.rootSignatureMajor);
    writer.WriteU16(profile.rootSignatureMinor);
    writer.WriteU32(static_cast<std::uint32_t>(profile.barrierModel));
    writer.WriteU16(static_cast<std::uint16_t>(profile.shaderBinaryFormat));
    writer.WriteU16(0);
    writer.WriteU32(profile.framesInFlight);
    writer.WriteU32(profile.directQueueCount);
    writer.WriteU32(profile.computeQueueCount);
    writer.WriteU32(profile.copyQueueCount);
    writer.WriteU32(profile.surfaceImageCount);
    writer.WriteU32(profile.rtvDescriptorCount);
    writer.WriteU32(profile.dsvDescriptorCount);
    writer.WriteU32(profile.shaderDescriptorCount);
    writer.WriteU32(profile.samplerDescriptorCount);
    writer.WriteU64(profile.requiredFeatureBits0);
    writer.WriteU64(profile.requiredFeatureBits1);
    writer.WriteZeroes(8);
    return std::move(writer).Take();
}

template<class Records>
bool Dense(const Records& records)
{
    for (std::size_t i = 0; i < records.size(); ++i)
        if (records[i].id.value != i) return false;
    return true;
}

bool DenseIds(const D3D12PackageDescription& description)
{
    if (!Dense(description.resources) || !Dense(description.allocations) || !Dense(description.views) ||
        !Dense(description.shaders) || !Dense(description.programs) || !Dense(description.bindingLayouts) ||
        !Dense(description.rootParameters) || !Dense(description.executables) || !Dense(description.computeExecutables) ||
        !Dense(description.computeCommands) || !Dense(description.attachmentOperations) || !Dense(description.rasterCommands) || !Dense(description.dynamicSlots) || !Dense(description.externalSlots) || !Dense(description.surfaceSlots))
        return false;
    for (std::size_t i = 0; i < description.vertexElements.size(); ++i)
        if (description.vertexElements[i].id != i) return false;
    return true;
}

base::Expected<const SectionView*, PackageError> RequireSection(
    const FrozenExecutablePackage& package,
    SectionKind kind,
    std::uint32_t stride = 0)
{
    const auto* section = package.FindSection(kind);
    if (section == nullptr)
        return base::Failure<const SectionView*, PackageError>(Error(PackageErrorCode::MissingRequiredSection, "Sectionが検証または実行の契約に違反しています。", kind));
    if (stride != 0 && section->descriptor.elementStride != stride)
        return base::Failure<const SectionView*, PackageError>(Error(PackageErrorCode::InvalidRecordStride, "入力または内部状態が検証または実行の契約に違反しています。", kind));
    return base::Success<const SectionView*, PackageError>(section);
}

