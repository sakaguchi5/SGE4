#include "./PackageReader.h"

#include "../../../canonical/base/BinaryIO.h"
#include "../../../canonical/base/CheckedMath.h"
#include "../../../canonical/base/Sha256.h"
#include "./PackageDigest.h"

#include <algorithm>
#include <cstring>
#include <memory>

namespace sge4::package
{
namespace
{
PackageError Error(PackageErrorCode code, std::string message, SectionKind section = {})
{
    PackageError error;
    error.code = code;
    error.section = section;
    error.message = std::move(message);
    return error;
}

template<class T>
bool Pull(base::Expected<T, std::string>& result, T& out, PackageError& error)
{
    if (!result) { error = Error(PackageErrorCode::InvalidSectionTable, result.error()); return false; }
    out = result.value();
    return true;
}

bool PullBytes(base::Expected<std::span<const std::byte>, std::string>& result, std::span<const std::byte>& out, PackageError& error)
{
    if (!result) { error = Error(PackageErrorCode::InvalidSectionTable, result.error()); return false; }
    out = result.value();
    return true;
}

bool IsKnownSectionKind(SectionKind kind)
{
    switch (kind)
    {
    case SectionKind::Manifest:
    case SectionKind::InvocationSchema:
    case SectionKind::OperationStreamTable:
    case SectionKind::OperationTable:
    case SectionKind::OperationPayload:
    case SectionKind::InitialData:
    case SectionKind::ShaderData:
    case SectionKind::NativeObjectData:
    case SectionKind::D3D12TargetProfile:
    case SectionKind::D3D12ResourceTable:
    case SectionKind::D3D12AllocationTable:
    case SectionKind::D3D12ViewTable:
    case SectionKind::D3D12ShaderTable:
    case SectionKind::D3D12ProgramTable:
    case SectionKind::D3D12BindingLayoutTable:
    case SectionKind::D3D12ExecutableTable:
    case SectionKind::D3D12RasterCommandTable:
    case SectionKind::D3D12DescriptorPlan:
    case SectionKind::D3D12DynamicSlotTable:
    case SectionKind::D3D12ExternalSlotTable:
    case SectionKind::D3D12SurfaceSlotTable:
    case SectionKind::D3D12VertexElementTable:
    case SectionKind::D3D12AttachmentOperationTable:
    case SectionKind::D3D12RootParameterTable:
    case SectionKind::D3D12ComputeExecutableTable:
    case SectionKind::D3D12ComputeCommandTable:
    case SectionKind::StringTable:
    case SectionKind::DebugMap:
    case SectionKind::Provenance:
        return true;
    default:
        return false;
    }
}

base::Expected<PackageHeader, PackageError> ReadHeader(std::span<const std::byte> bytes)
{
    if (bytes.size() < HeaderBytes)
        return base::Failure<PackageHeader, PackageError>(Error(PackageErrorCode::FileSizeMismatch, "検証または実行の契約に違反しています。"));
    if (!std::equal(PackageMagic.begin(), PackageMagic.end(), bytes.begin()))
        return base::Failure<PackageHeader, PackageError>(Error(PackageErrorCode::BadMagic, "Packageが検証または実行の契約に違反しています。"));

    base::BinaryReader reader(bytes.subspan(8, HeaderBytes - 8));
    PackageHeader header;
    PackageError readError;
    auto u16 = reader.ReadU16(); if (!Pull(u16, header.formatMajor, readError)) return base::Failure<PackageHeader, PackageError>(readError);
    u16 = reader.ReadU16(); if (!Pull(u16, header.formatMinor, readError)) return base::Failure<PackageHeader, PackageError>(readError);
    u16 = reader.ReadU16(); if (!Pull(u16, header.headerBytes, readError)) return base::Failure<PackageHeader, PackageError>(readError);
    u16 = reader.ReadU16(); if (!Pull(u16, header.sectionDescriptorBytes, readError)) return base::Failure<PackageHeader, PackageError>(readError);
    auto u32 = reader.ReadU32(); if (!Pull(u32, header.targetKind, readError)) return base::Failure<PackageHeader, PackageError>(readError);
    u32 = reader.ReadU32(); if (!Pull(u32, header.targetSchemaVersion, readError)) return base::Failure<PackageHeader, PackageError>(readError);
    u32 = reader.ReadU32(); if (!Pull(u32, header.minimumRuntimeVersion, readError)) return base::Failure<PackageHeader, PackageError>(readError);
    u32 = reader.ReadU32(); if (!Pull(u32, header.flags, readError)) return base::Failure<PackageHeader, PackageError>(readError);
    u32 = reader.ReadU32(); if (!Pull(u32, header.endianTag, readError)) return base::Failure<PackageHeader, PackageError>(readError);
    u32 = reader.ReadU32(); if (!Pull(u32, header.digestAlgorithm, readError)) return base::Failure<PackageHeader, PackageError>(readError);
    auto u64 = reader.ReadU64(); if (!Pull(u64, header.fileBytes, readError)) return base::Failure<PackageHeader, PackageError>(readError);
    u64 = reader.ReadU64(); if (!Pull(u64, header.sectionTableOffset, readError)) return base::Failure<PackageHeader, PackageError>(readError);
    u32 = reader.ReadU32(); if (!Pull(u32, header.sectionCount, readError)) return base::Failure<PackageHeader, PackageError>(readError);
    std::uint32_t reserved0 = 1; u32 = reader.ReadU32(); if (!Pull(u32, reserved0, readError)) return base::Failure<PackageHeader, PackageError>(readError);
    if (reserved0 != 0) return base::Failure<PackageHeader, PackageError>(Error(PackageErrorCode::InvalidSectionTable, "Headerが検証または実行の契約に違反しています。"));
    std::span<const std::byte> digestBytes;
    auto raw = reader.ReadBytes(32); if (!PullBytes(raw, digestBytes, readError)) return base::Failure<PackageHeader, PackageError>(readError); std::copy(digestBytes.begin(), digestBytes.end(), header.targetProfileDigest.begin());
    raw = reader.ReadBytes(32); if (!PullBytes(raw, digestBytes, readError)) return base::Failure<PackageHeader, PackageError>(readError); std::copy(digestBytes.begin(), digestBytes.end(), header.executionDigest.begin());
    raw = reader.ReadBytes(32); if (!PullBytes(raw, digestBytes, readError)) return base::Failure<PackageHeader, PackageError>(readError); std::copy(digestBytes.begin(), digestBytes.end(), header.fileDigest.begin());
    raw = reader.ReadBytes(16); if (!PullBytes(raw, digestBytes, readError)) return base::Failure<PackageHeader, PackageError>(readError);
    if (std::any_of(digestBytes.begin(), digestBytes.end(), [](std::byte value) { return value != std::byte{0}; }))
        return base::Failure<PackageHeader, PackageError>(Error(PackageErrorCode::InvalidSectionTable, "Headerが検証または実行の契約に違反しています。"));

    if (header.formatMajor != ContainerFormatMajor || header.formatMinor > ContainerFormatMinor)
        return base::Failure<PackageHeader, PackageError>(Error(PackageErrorCode::UnsupportedContainerVersion, "Packageが検証または実行の契約に違反しています。"));
    if (header.headerBytes != HeaderBytes || header.sectionDescriptorBytes != SectionDescriptorBytes)
        return base::Failure<PackageHeader, PackageError>(Error(PackageErrorCode::UnsupportedContainerVersion, "検証または実行の契約に違反しています。"));
    if (header.endianTag != EndianTag)
        return base::Failure<PackageHeader, PackageError>(Error(PackageErrorCode::WrongEndianness, "検証または実行の契約に違反しています。"));
    if (header.digestAlgorithm != DigestAlgorithmSha256)
        return base::Failure<PackageHeader, PackageError>(Error(PackageErrorCode::UnsupportedContainerVersion, "Digestが検証または実行の契約に違反しています。"));
    if (header.flags != 0)
        return base::Failure<PackageHeader, PackageError>(Error(PackageErrorCode::UnsupportedContainerVersion, "Packageが検証または実行の契約に違反しています。"));
    if (header.sectionCount == 0 ||
        static_cast<std::uint64_t>(header.sectionCount) >
            (bytes.size() - HeaderBytes) / SectionDescriptorBytes)
        return base::Failure<PackageHeader, PackageError>(Error(PackageErrorCode::InvalidSectionTable, "Sectionが検証または実行の契約に違反しています。"));
    if (header.fileBytes != bytes.size())
        return base::Failure<PackageHeader, PackageError>(Error(PackageErrorCode::FileSizeMismatch, "Fileが検証または実行の契約に違反しています。"));
    if (header.sectionTableOffset != HeaderBytes)
        return base::Failure<PackageHeader, PackageError>(Error(PackageErrorCode::InvalidSectionTable, "Sectionが検証または実行の契約に違反しています。"));
    return base::Success<PackageHeader, PackageError>(header);
}

base::Expected<std::vector<SectionDescriptor>, PackageError> ReadDescriptors(
    std::span<const std::byte> bytes,
    const PackageHeader& header)
{
    std::uint64_t tableBytes = static_cast<std::uint64_t>(header.sectionCount) * SectionDescriptorBytes;
    std::uint64_t tableEnd = 0;
    if (!base::CheckedAdd(header.sectionTableOffset, tableBytes, tableEnd) || tableEnd > bytes.size())
        return base::Failure<std::vector<SectionDescriptor>, PackageError>(Error(PackageErrorCode::InvalidSectionTable, "検証または実行の契約に違反しています。"));

    base::BinaryReader reader(bytes.subspan(static_cast<std::size_t>(header.sectionTableOffset), static_cast<std::size_t>(tableBytes)));
    std::vector<SectionDescriptor> descriptors;
    descriptors.reserve(header.sectionCount);
    for (std::uint32_t index = 0; index < header.sectionCount; ++index)
    {
        SectionDescriptor descriptor;
        PackageError readError;
        std::uint32_t kind = 0;
        auto u32 = reader.ReadU32(); if (!Pull(u32, kind, readError)) return base::Failure<std::vector<SectionDescriptor>, PackageError>(readError); descriptor.sectionKind = static_cast<SectionKind>(kind);
        auto u16 = reader.ReadU16(); if (!Pull(u16, descriptor.schemaVersion, readError)) return base::Failure<std::vector<SectionDescriptor>, PackageError>(readError);
        u16 = reader.ReadU16(); if (!Pull(u16, descriptor.descriptorBytes, readError)) return base::Failure<std::vector<SectionDescriptor>, PackageError>(readError);
        u32 = reader.ReadU32(); if (!Pull(u32, descriptor.flags, readError)) return base::Failure<std::vector<SectionDescriptor>, PackageError>(readError);
        u32 = reader.ReadU32(); if (!Pull(u32, descriptor.alignment, readError)) return base::Failure<std::vector<SectionDescriptor>, PackageError>(readError);
        auto u64 = reader.ReadU64(); if (!Pull(u64, descriptor.fileOffset, readError)) return base::Failure<std::vector<SectionDescriptor>, PackageError>(readError);
        u64 = reader.ReadU64(); if (!Pull(u64, descriptor.storedBytes, readError)) return base::Failure<std::vector<SectionDescriptor>, PackageError>(readError);
        u64 = reader.ReadU64(); if (!Pull(u64, descriptor.logicalBytes, readError)) return base::Failure<std::vector<SectionDescriptor>, PackageError>(readError);
        u32 = reader.ReadU32(); if (!Pull(u32, descriptor.elementCount, readError)) return base::Failure<std::vector<SectionDescriptor>, PackageError>(readError);
        u32 = reader.ReadU32(); if (!Pull(u32, descriptor.elementStride, readError)) return base::Failure<std::vector<SectionDescriptor>, PackageError>(readError);
        auto raw = reader.ReadBytes(32); std::span<const std::byte> digest; if (!PullBytes(raw, digest, readError)) return base::Failure<std::vector<SectionDescriptor>, PackageError>(readError); std::copy(digest.begin(), digest.end(), descriptor.sectionDigest.begin());

        if (descriptor.descriptorBytes != SectionDescriptorBytes)
            return base::Failure<std::vector<SectionDescriptor>, PackageError>(Error(PackageErrorCode::UnsupportedContainerVersion, "Sectionが検証または実行の契約に違反しています。", descriptor.sectionKind));
        constexpr std::uint32_t KnownSectionFlags =
            static_cast<std::uint32_t>(SectionFlags::Required) |
            static_cast<std::uint32_t>(SectionFlags::ExecutionAffecting) |
            static_cast<std::uint32_t>(SectionFlags::DebugOnly) |
            static_cast<std::uint32_t>(SectionFlags::OpaqueToCore);
        if ((descriptor.flags & ~KnownSectionFlags) != 0 ||
            (HasFlag(descriptor.flags, SectionFlags::DebugOnly) &&
             HasFlag(descriptor.flags, SectionFlags::ExecutionAffecting)))
            return base::Failure<std::vector<SectionDescriptor>, PackageError>(Error(PackageErrorCode::UnsupportedContainerVersion, "Sectionが検証または実行の契約に違反しています。", descriptor.sectionKind));
        if (IsKnownSectionKind(descriptor.sectionKind) && descriptor.schemaVersion != 1)
            return base::Failure<std::vector<SectionDescriptor>, PackageError>(Error(PackageErrorCode::UnsupportedContainerVersion, "Sectionが検証または実行の契約に違反しています。", descriptor.sectionKind));
        if (HasFlag(descriptor.flags, SectionFlags::Required) && !IsKnownSectionKind(descriptor.sectionKind))
            return base::Failure<std::vector<SectionDescriptor>, PackageError>(Error(PackageErrorCode::UnknownRequiredSection, "Sectionが検証または実行の契約に違反しています。", descriptor.sectionKind));
        if (descriptor.fileOffset < tableEnd)
            return base::Failure<std::vector<SectionDescriptor>, PackageError>(Error(PackageErrorCode::SectionOverlap, "検証または実行の契約に違反しています。", descriptor.sectionKind));
        if (!base::IsPowerOfTwo(descriptor.alignment) || descriptor.fileOffset % descriptor.alignment != 0)
            return base::Failure<std::vector<SectionDescriptor>, PackageError>(Error(PackageErrorCode::InvalidAlignment, "Sectionが検証または実行の契約に違反しています。", descriptor.sectionKind));
        if (descriptor.logicalBytes != descriptor.storedBytes)
            return base::Failure<std::vector<SectionDescriptor>, PackageError>(Error(PackageErrorCode::UnsupportedContainerVersion, "Sectionが検証または実行の契約に違反しています。", descriptor.sectionKind));
        if (descriptor.elementCount != 0 && (descriptor.elementStride == 0 || static_cast<std::uint64_t>(descriptor.elementCount) * descriptor.elementStride != descriptor.logicalBytes))
            return base::Failure<std::vector<SectionDescriptor>, PackageError>(Error(PackageErrorCode::InvalidRecordStride, "検証または実行の契約に違反しています。", descriptor.sectionKind));
        std::uint64_t end = 0;
        if (!base::CheckedAdd(descriptor.fileOffset, descriptor.storedBytes, end) || end > bytes.size())
            return base::Failure<std::vector<SectionDescriptor>, PackageError>(Error(PackageErrorCode::SectionOutOfBounds, "Sectionが検証または実行の契約に違反しています。", descriptor.sectionKind));
        if (!descriptors.empty() && static_cast<std::uint32_t>(descriptors.back().sectionKind) >= kind)
            return base::Failure<std::vector<SectionDescriptor>, PackageError>(Error(
                descriptors.back().sectionKind == descriptor.sectionKind ? PackageErrorCode::DuplicateSection : PackageErrorCode::InvalidSectionTable,
                "Sectionが検証または実行の契約に違反しています。", descriptor.sectionKind));
        descriptors.push_back(descriptor);
    }

    // Canonical WriterはSection kind順とfile配置順を一致させる。
    // storedBytes == 0のSectionは次Sectionと同じoffsetを持てるため、offsetだけの
    // 不安定sortで重なりを判定してはならない。descriptor順で範囲とzero paddingを検証する。
    std::uint64_t previousEnd = tableEnd;
    for (const auto& descriptor : descriptors)
    {
        if (descriptor.fileOffset < previousEnd)
            return base::Failure<std::vector<SectionDescriptor>, PackageError>(
                Error(PackageErrorCode::SectionOverlap,
                    "Sectionが前のSectionと重なっています。", descriptor.sectionKind));
        const auto padding = bytes.subspan(
            static_cast<std::size_t>(previousEnd),
            static_cast<std::size_t>(descriptor.fileOffset - previousEnd));
        if (std::ranges::any_of(
                padding, [](std::byte value) { return value != std::byte{0}; }))
            return base::Failure<std::vector<SectionDescriptor>, PackageError>(
                Error(PackageErrorCode::InvalidSectionTable,
                    "Section paddingがCanonicalではありません。", descriptor.sectionKind));
        previousEnd = descriptor.fileOffset + descriptor.storedBytes;
    }
    return base::Success<std::vector<SectionDescriptor>, PackageError>(std::move(descriptors));
}
}

base::Expected<FrozenExecutablePackage, PackageError> PackageReader::Read(std::span<const std::byte> bytes)
{
    return Read(std::vector<std::byte>(bytes.begin(), bytes.end()));
}

base::Expected<FrozenExecutablePackage, PackageError> PackageReader::Read(std::vector<std::byte> bytes)
{
    const auto headerResult = ReadHeader(bytes);
    if (!headerResult) return base::Failure<FrozenExecutablePackage, PackageError>(headerResult.error());
    const auto header = headerResult.value();

    if (ComputeFileDigest(bytes) != header.fileDigest)
        return base::Failure<FrozenExecutablePackage, PackageError>(Error(PackageErrorCode::DigestMismatch, "Digestが検証または実行の契約に違反しています。"));

    const auto descriptorsResult = ReadDescriptors(bytes, header);
    if (!descriptorsResult) return base::Failure<FrozenExecutablePackage, PackageError>(descriptorsResult.error());

    auto storage = std::make_shared<const std::vector<std::byte>>(std::move(bytes));
    FrozenExecutablePackage package;
    package.storage_ = storage;
    package.header_ = header;
    for (const auto& descriptor : descriptorsResult.value())
    {
        const auto sectionBytes = std::span<const std::byte>(*storage).subspan(
            static_cast<std::size_t>(descriptor.fileOffset),
            static_cast<std::size_t>(descriptor.storedBytes));
        if (base::Sha256(sectionBytes) != descriptor.sectionDigest)
            return base::Failure<FrozenExecutablePackage, PackageError>(Error(PackageErrorCode::DigestMismatch, "Sectionが検証または実行の契約に違反しています。", descriptor.sectionKind));
        package.sections_.push_back(SectionView{descriptor, sectionBytes});
    }

    if (ComputeExecutionDigestFromViews(package.sections_) != header.executionDigest)
        return base::Failure<FrozenExecutablePackage, PackageError>(Error(PackageErrorCode::DigestMismatch, "Digestが検証または実行の契約に違反しています。"));

    if (header.targetKind == TargetKindD3D12)
    {
        const auto* profile = package.FindSection(SectionKind::D3D12TargetProfile);
        if (profile == nullptr)
            return base::Failure<FrozenExecutablePackage, PackageError>(Error(PackageErrorCode::MissingRequiredSection, "Fileが検証または実行の契約に違反しています。", SectionKind::D3D12TargetProfile));
        if (base::Sha256(profile->bytes) != header.targetProfileDigest)
            return base::Failure<FrozenExecutablePackage, PackageError>(Error(PackageErrorCode::DigestMismatch, "Digestが検証または実行の契約に違反しています。", SectionKind::D3D12TargetProfile));
    }

    return base::Success<FrozenExecutablePackage, PackageError>(std::move(package));
}
}
