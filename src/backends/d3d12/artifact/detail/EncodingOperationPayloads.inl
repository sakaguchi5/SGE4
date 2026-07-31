std::vector<std::byte> Encode(const CreateResourcePayload& payload)
{
    base::BinaryWriter writer; WriteId(writer, payload.resource); writer.WriteU32(0); return std::move(writer).Take();
}
std::vector<std::byte> Encode(const UploadBufferPayload& payload)
{
    base::BinaryWriter writer; WriteId(writer, payload.resource); writer.WriteU32(0); writer.WriteU64(payload.sourceOffset); writer.WriteU64(payload.bytes); return std::move(writer).Take();
}
std::vector<std::byte> Encode(const UploadTexturePayload& payload)
{
    base::BinaryWriter writer;
    WriteId(writer, payload.resource); writer.WriteU32(0); writer.WriteU64(payload.sourceOffset);
    writer.WriteU32(payload.sourceRowBytes); writer.WriteU32(payload.sourceSliceBytes);
    writer.WriteU16(payload.mipLevel); writer.WriteU16(payload.arrayLayer); writer.WriteU16(payload.plane); writer.WriteU16(payload.reserved);
    return std::move(writer).Take();
}
std::vector<std::byte> Encode(const CreateRootSignaturePayload& payload)
{
    base::BinaryWriter writer; WriteId(writer, payload.layout); writer.WriteU32(0); return std::move(writer).Take();
}
std::vector<std::byte> Encode(const CreateGraphicsPipelinePayload& payload)
{
    base::BinaryWriter writer; WriteId(writer, payload.executable); writer.WriteU32(0); return std::move(writer).Take();
}
std::vector<std::byte> Encode(const CreateComputePipelinePayload& payload)
{
    base::BinaryWriter writer; WriteId(writer, payload.executable); writer.WriteU32(0); return std::move(writer).Take();
}
std::vector<std::byte> Encode(const InitializeStatePayload& payload)
{
    base::BinaryWriter writer; WriteId(writer, payload.resource); writer.WriteU32(0); WriteState(writer, payload.before); WriteState(writer, payload.after); return std::move(writer).Take();
}
std::vector<std::byte> Encode(const VerifyBufferContentsPayload& payload)
{
    base::BinaryWriter writer;
    WriteId(writer, payload.resource);
    writer.WriteU32(0);
    writer.WriteU64(payload.resourceOffset);
    writer.WriteU64(payload.expectedDataOffset);
    writer.WriteU64(payload.bytes);
    return std::move(writer).Take();
}
std::vector<std::byte> Encode(const VerifyTextureContentsPayload& payload)
{
    base::BinaryWriter writer;
    WriteId(writer, payload.resource); writer.WriteU32(0); writer.WriteU64(payload.expectedDataOffset);
    writer.WriteU32(payload.expectedRowBytes); writer.WriteU32(payload.width); writer.WriteU32(payload.height); writer.WriteU32(payload.reserved);
    return std::move(writer).Take();
}
std::vector<std::byte> Encode(const AcquireSurfaceImagePayload& payload)
{
    base::BinaryWriter writer; WriteId(writer, payload.slot); writer.WriteU32(0); return std::move(writer).Take();
}
std::vector<std::byte> Encode(const AcquireExternalPayload& payload)
{
    base::BinaryWriter writer; WriteId(writer, payload.slot); writer.WriteU32(0); return std::move(writer).Take();
}
std::vector<std::byte> Encode(const WaitExternalPayload& payload)
{
    base::BinaryWriter writer; WriteId(writer, payload.slot); writer.WriteU32(0); return std::move(writer).Take();
}
std::vector<std::byte> Encode(const ApplyDynamicDataPayload& payload)
{
    base::BinaryWriter writer; WriteId(writer, payload.slot); writer.WriteU32(0); return std::move(writer).Take();
}
std::vector<std::byte> Encode(const TransitionPayload& payload)
{
    base::BinaryWriter writer; WriteId(writer, payload.view); writer.WriteU32(payload.flags); WriteState(writer, payload.before); WriteState(writer, payload.after); return std::move(writer).Take();
}
std::vector<std::byte> Encode(const ExecuteRasterPayload& payload)
{
    base::BinaryWriter writer; WriteId(writer, payload.command); writer.WriteU32(0); return std::move(writer).Take();
}
std::vector<std::byte> Encode(const ExecuteComputePayload& payload)
{
    base::BinaryWriter writer; WriteId(writer, payload.command); writer.WriteU32(0); return std::move(writer).Take();
}
std::vector<std::byte> Encode(const CopyBufferPayload& payload)
{
    base::BinaryWriter writer;
    WriteId(writer, payload.sourceView);
    WriteId(writer, payload.destinationView);
    writer.WriteU64(payload.sourceOffset);
    writer.WriteU64(payload.destinationOffset);
    writer.WriteU64(payload.bytes);
    return std::move(writer).Take();
}
std::vector<std::byte> Encode(const SignalQueuePayload& payload)
{
    base::BinaryWriter writer; WriteId(writer, payload.signalPoint); writer.WriteU32(0); return std::move(writer).Take();
}
std::vector<std::byte> Encode(const WaitQueuePayload& payload)
{
    base::BinaryWriter writer; WriteId(writer, payload.signalPoint); writer.WriteU32(0); return std::move(writer).Take();
}
std::vector<std::byte> Encode(const WaitTemporalPayload& payload)
{
    base::BinaryWriter writer; WriteId(writer, payload.resource); WriteId(writer, payload.producerSignalPoint); return std::move(writer).Take();
}
std::vector<std::byte> Encode(const ActivateAliasPayload& payload)
{
    base::BinaryWriter writer; WriteId(writer, payload.before); WriteId(writer, payload.after); return std::move(writer).Take();
}
std::vector<std::byte> Encode(const ReleaseExternalPayload& payload)
{
    base::BinaryWriter writer; WriteId(writer, payload.slot); WriteId(writer, payload.releaseSignalPoint); return std::move(writer).Take();
}
std::vector<std::byte> Encode(const PresentSurfacePayload& payload)
{
    base::BinaryWriter writer; WriteId(writer, payload.slot); writer.WriteU32(0); return std::move(writer).Take();
}

namespace
{
template<class T, class DecodeFunction>
base::Expected<T, PackageError> DecodePayload(std::span<const std::byte> bytes, std::size_t expected, DecodeFunction decode)
{
    if (bytes.size() != expected)
        return base::Failure<T, PackageError>(Error(PackageErrorCode::InvalidOperationStream, "Operationが検証または実行の契約に違反しています。", SectionKind::OperationPayload));
    base::BinaryReader reader(bytes);
    return decode(reader);
}

base::Expected<std::uint32_t, PackageError> ReadPayloadU32(base::BinaryReader& reader)
{
    auto value = reader.ReadU32();
    return value ? base::Success<std::uint32_t, PackageError>(value.value()) :
        base::Failure<std::uint32_t, PackageError>(Error(PackageErrorCode::InvalidOperationStream, "Operationが検証または実行の契約に違反しています。", SectionKind::OperationPayload));
}

template<class T, class Id>
base::Expected<T, PackageError> DecodeIdPayload(std::span<const std::byte> bytes, const char* name)
{
    return DecodePayload<T>(bytes, 8, [name](base::BinaryReader& reader) {
        auto id = ReadPayloadU32(reader);
        auto reserved = ReadPayloadU32(reader);
        if (!id || !reserved || reserved.value() != 0)
            return base::Failure<T, PackageError>(Error(PackageErrorCode::InvalidOperationStream, std::string(name) + "Payloadが検証または実行の契約に違反しています。"));
        return base::Success<T, PackageError>(T{Id{id.value()}});
    });
}
}

base::Expected<CreateResourcePayload, PackageError> DecodeCreateResource(std::span<const std::byte> bytes)
{
    return DecodeIdPayload<CreateResourcePayload, ResourceId>(bytes, "CreateResource");
}
base::Expected<UploadBufferPayload, PackageError> DecodeUploadBuffer(std::span<const std::byte> bytes)
{
    return DecodePayload<UploadBufferPayload>(bytes, 24, [](base::BinaryReader& reader) {
        auto id = ReadPayloadU32(reader); auto reserved = ReadPayloadU32(reader); auto offset = reader.ReadU64(); auto count = reader.ReadU64();
        if (!id || !reserved || !offset || !count || reserved.value() != 0)
            return base::Failure<UploadBufferPayload, PackageError>(Error(PackageErrorCode::InvalidOperationStream, "Bufferが検証または実行の契約に違反しています。"));
        return base::Success<UploadBufferPayload, PackageError>({ResourceId{id.value()}, offset.value(), count.value()});
    });
}
base::Expected<UploadTexturePayload, PackageError> DecodeUploadTexture(std::span<const std::byte> bytes)
{
    return DecodePayload<UploadTexturePayload>(bytes, 32, [](base::BinaryReader& reader) {
        auto id = ReadPayloadU32(reader); auto reserved32 = ReadPayloadU32(reader); auto offset = reader.ReadU64();
        auto row = reader.ReadU32(); auto slice = reader.ReadU32(); auto mip = reader.ReadU16(); auto arrayLayer = reader.ReadU16();
        auto plane = reader.ReadU16(); auto reserved16 = reader.ReadU16();
        if (!id || !reserved32 || !offset || !row || !slice || !mip || !arrayLayer || !plane || !reserved16 || reserved32.value() != 0 || reserved16.value() != 0)
            return base::Failure<UploadTexturePayload, PackageError>(Error(PackageErrorCode::InvalidOperationStream, "Textureが検証または実行の契約に違反しています。"));
        return base::Success<UploadTexturePayload, PackageError>({ResourceId{id.value()}, offset.value(), row.value(), slice.value(), mip.value(), arrayLayer.value(), plane.value(), 0});
    });
}
base::Expected<CreateRootSignaturePayload, PackageError> DecodeCreateRootSignature(std::span<const std::byte> bytes)
{
    return DecodeIdPayload<CreateRootSignaturePayload, BindingLayoutId>(bytes, "CreateRootSignature");
}
base::Expected<CreateGraphicsPipelinePayload, PackageError> DecodeCreateGraphicsPipeline(std::span<const std::byte> bytes)
{
    return DecodeIdPayload<CreateGraphicsPipelinePayload, ExecutableId>(bytes, "CreateGraphicsPipeline");
}
base::Expected<CreateComputePipelinePayload, PackageError> DecodeCreateComputePipeline(std::span<const std::byte> bytes)
{
    return DecodeIdPayload<CreateComputePipelinePayload, ComputeExecutableId>(bytes, "CreateComputePipeline");
}
base::Expected<InitializeStatePayload, PackageError> DecodeInitializeState(std::span<const std::byte> bytes)
{
    return DecodePayload<InitializeStatePayload>(bytes, 24, [](base::BinaryReader& reader) {
        auto id = ReadPayloadU32(reader); auto reserved = ReadPayloadU32(reader); auto before = ReadState(reader, SectionKind::OperationPayload); auto after = ReadState(reader, SectionKind::OperationPayload);
        if (!id || !reserved || !before || !after || reserved.value() != 0)
            return base::Failure<InitializeStatePayload, PackageError>(Error(PackageErrorCode::InvalidOperationStream, "Payloadが検証または実行の契約に違反しています。"));
        return base::Success<InitializeStatePayload, PackageError>({ResourceId{id.value()}, before.value(), after.value()});
    });
}
base::Expected<VerifyBufferContentsPayload, PackageError> DecodeVerifyBufferContents(std::span<const std::byte> bytes)
{
    return DecodePayload<VerifyBufferContentsPayload>(bytes, 32, [](base::BinaryReader& reader) {
        auto id = ReadPayloadU32(reader);
        auto reserved = ReadPayloadU32(reader);
        auto resourceOffset = reader.ReadU64();
        auto expectedOffset = reader.ReadU64();
        auto count = reader.ReadU64();
        if (!id || !reserved || !resourceOffset || !expectedOffset || !count || reserved.value() != 0)
            return base::Failure<VerifyBufferContentsPayload, PackageError>(Error(PackageErrorCode::InvalidOperationStream, "Bufferが検証または実行の契約に違反しています。"));
        return base::Success<VerifyBufferContentsPayload, PackageError>(
            {ResourceId{id.value()}, resourceOffset.value(), expectedOffset.value(), count.value()});
    });
}
base::Expected<VerifyTextureContentsPayload, PackageError> DecodeVerifyTextureContents(std::span<const std::byte> bytes)
{
    return DecodePayload<VerifyTextureContentsPayload>(bytes, 32, [](base::BinaryReader& reader) {
        auto id = ReadPayloadU32(reader); auto reserved0 = ReadPayloadU32(reader); auto expected = reader.ReadU64();
        auto row = reader.ReadU32(); auto width = reader.ReadU32(); auto height = reader.ReadU32(); auto reserved1 = reader.ReadU32();
        if (!id || !reserved0 || !expected || !row || !width || !height || !reserved1 || reserved0.value() != 0 || reserved1.value() != 0)
            return base::Failure<VerifyTextureContentsPayload, PackageError>(Error(PackageErrorCode::InvalidOperationStream, "Textureが検証または実行の契約に違反しています。"));
        return base::Success<VerifyTextureContentsPayload, PackageError>({ResourceId{id.value()}, expected.value(), row.value(), width.value(), height.value(), 0});
    });
}
base::Expected<AcquireSurfaceImagePayload, PackageError> DecodeAcquireSurfaceImage(std::span<const std::byte> bytes)
{
    return DecodeIdPayload<AcquireSurfaceImagePayload, SurfaceSlotId>(bytes, "AcquireSurfaceImage");
}
base::Expected<AcquireExternalPayload, PackageError> DecodeAcquireExternal(std::span<const std::byte> bytes)
{
    return DecodeIdPayload<AcquireExternalPayload, ExternalSlotId>(bytes, "AcquireExternal");
}
base::Expected<WaitExternalPayload, PackageError> DecodeWaitExternal(std::span<const std::byte> bytes)
{
    return DecodeIdPayload<WaitExternalPayload, ExternalSlotId>(bytes, "WaitExternal");
}
base::Expected<ApplyDynamicDataPayload, PackageError> DecodeApplyDynamicData(std::span<const std::byte> bytes)
{
    return DecodeIdPayload<ApplyDynamicDataPayload, DynamicSlotId>(bytes, "ApplyDynamicData");
}
base::Expected<TransitionPayload, PackageError> DecodeTransition(std::span<const std::byte> bytes)
{
    return DecodePayload<TransitionPayload>(bytes, 24, [](base::BinaryReader& reader) {
        auto id = ReadPayloadU32(reader); auto flags = ReadPayloadU32(reader); auto before = ReadState(reader, SectionKind::OperationPayload); auto after = ReadState(reader, SectionKind::OperationPayload);
        if (!id || !flags || !before || !after)
            return base::Failure<TransitionPayload, PackageError>(Error(PackageErrorCode::InvalidOperationStream, "Payloadが検証または実行の契約に違反しています。"));
        return base::Success<TransitionPayload, PackageError>({ViewId{id.value()}, flags.value(), before.value(), after.value()});
    });
}
base::Expected<ExecuteRasterPayload, PackageError> DecodeExecuteRaster(std::span<const std::byte> bytes)
{
    return DecodeIdPayload<ExecuteRasterPayload, RasterCommandId>(bytes, "ExecuteRaster");
}
base::Expected<ExecuteComputePayload, PackageError> DecodeExecuteCompute(std::span<const std::byte> bytes)
{
    return DecodeIdPayload<ExecuteComputePayload, ComputeCommandId>(bytes, "ExecuteCompute");
}
base::Expected<CopyBufferPayload, PackageError> DecodeCopyBuffer(std::span<const std::byte> bytes)
{
    return DecodePayload<CopyBufferPayload>(bytes, 32, [](base::BinaryReader& reader) {
        auto sourceView = reader.ReadU32(); auto destinationView = reader.ReadU32();
        auto sourceOffset = reader.ReadU64(); auto destinationOffset = reader.ReadU64(); auto count = reader.ReadU64();
        if (!sourceView || !destinationView || !sourceOffset || !destinationOffset || !count)
            return base::Failure<CopyBufferPayload, PackageError>(Error(PackageErrorCode::InvalidOperationStream, "Payloadが検証または実行の契約に違反しています。"));
        return base::Success<CopyBufferPayload, PackageError>(
            {{sourceView.value()}, {destinationView.value()}, sourceOffset.value(), destinationOffset.value(), count.value()});
    });
}
base::Expected<SignalQueuePayload, PackageError> DecodeSignalQueue(std::span<const std::byte> bytes)
{
    return DecodeIdPayload<SignalQueuePayload, SignalPointId>(bytes, "SignalQueue");
}
base::Expected<WaitQueuePayload, PackageError> DecodeWaitQueue(std::span<const std::byte> bytes)
{
    return DecodeIdPayload<WaitQueuePayload, SignalPointId>(bytes, "WaitQueue");
}
base::Expected<WaitTemporalPayload, PackageError> DecodeWaitTemporal(std::span<const std::byte> bytes)
{
    return DecodePayload<WaitTemporalPayload>(bytes, 8, [](base::BinaryReader& reader) {
        auto resource = reader.ReadU32(); auto signal = reader.ReadU32();
        if (!resource || !signal) return base::Failure<WaitTemporalPayload, PackageError>(
            Error(PackageErrorCode::InvalidOperationStream, "Payloadが検証または実行の契約に違反しています。"));
        return base::Success<WaitTemporalPayload, PackageError>({{resource.value()}, {signal.value()}});
    });
}
base::Expected<ActivateAliasPayload, PackageError> DecodeActivateAlias(std::span<const std::byte> bytes)
{
    return DecodePayload<ActivateAliasPayload>(bytes, 8, [](base::BinaryReader& reader) {
        auto before = reader.ReadU32(); auto after = reader.ReadU32();
        if (!before || !after) return base::Failure<ActivateAliasPayload, PackageError>(Error(PackageErrorCode::InvalidOperationStream, "Payloadが検証または実行の契約に違反しています。"));
        return base::Success<ActivateAliasPayload, PackageError>({{before.value()}, {after.value()}});
    });
}
base::Expected<ReleaseExternalPayload, PackageError> DecodeReleaseExternal(std::span<const std::byte> bytes)
{
    return DecodePayload<ReleaseExternalPayload>(bytes, 8, [](base::BinaryReader& reader) {
        auto slot = reader.ReadU32(); auto signal = reader.ReadU32();
        if (!slot || !signal) return base::Failure<ReleaseExternalPayload, PackageError>(
            Error(PackageErrorCode::InvalidOperationStream, "Payloadが検証または実行の契約に違反しています。"));
        return base::Success<ReleaseExternalPayload, PackageError>({{slot.value()}, {signal.value()}});
    });
}
base::Expected<PresentSurfacePayload, PackageError> DecodePresentSurface(std::span<const std::byte> bytes)
{
    return DecodeIdPayload<PresentSurfacePayload, SurfaceSlotId>(bytes, "PresentSurface");
}