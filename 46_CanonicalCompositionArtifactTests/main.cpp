#include "../00_Foundation/BinaryIO.h"
#include "../00_Foundation/FileIO.h"
#include "../00_Foundation/Sha256.h"
#include "../09_FrozenPackageCore/PackageFormat.h"
#include "../09_FrozenPackageCore/PackageWriter.h"
#include "../16_FrozenCompositionArtifact/FrozenCompositionDigest.h"
#include "../16_FrozenCompositionArtifact/FrozenCompositionReader.h"
#include "../16_FrozenCompositionArtifact/FrozenCompositionWriter.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
using namespace sge4;

void Require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

base::Digest256 MakeKey(std::uint8_t seed)
{
    base::Digest256 result{};
    for (std::size_t index = 0; index < result.size(); ++index)
        result[index] = static_cast<std::byte>(static_cast<std::uint8_t>(seed + index));
    return result;
}

std::vector<std::byte> MakeLeaf(std::uint32_t marker, std::uint32_t schemaVersion = 17)
{
    package::PackageBuildInput input;
    input.targetSchemaVersion = schemaVersion;
    input.minimumRuntimeVersion = schemaVersion;

    base::BinaryWriter manifest;
    for (std::uint32_t index = 0; index < 16; ++index)
        manifest.WriteU32(index == 0 ? marker : (index == 13 || index == 14 ? package::InvalidIndex : 0));
    package::PackageSectionInput manifestSection;
    manifestSection.kind = package::SectionKind::Manifest;
    manifestSection.flags = static_cast<std::uint32_t>(
        package::SectionFlags::Required | package::SectionFlags::ExecutionAffecting);
    manifestSection.alignment = 8;
    manifestSection.elementCount = 1;
    manifestSection.elementStride = 64;
    manifestSection.bytes = std::move(manifest).Take();
    input.sections.push_back(std::move(manifestSection));

    package::PackageSectionInput profile;
    profile.kind = package::SectionKind::D3D12TargetProfile;
    profile.flags = static_cast<std::uint32_t>(
        package::SectionFlags::Required | package::SectionFlags::ExecutionAffecting);
    profile.alignment = 8;
    profile.elementCount = 1;
    profile.elementStride = 80;
    profile.bytes.assign(80, std::byte{0});
    profile.bytes[0] = static_cast<std::byte>(marker & 0xffu);
    input.sections.push_back(std::move(profile));

    auto result = package::PackageWriter::Write(std::move(input));
    Require(static_cast<bool>(result), "failed to construct Schema 17 fixture Leaf");
    return std::move(result).Value();
}

composition::FrozenCompositionBuildInput MakeCompositionInput(bool reverseOrder)
{
    composition::FrozenCompositionBuildInput input;
    composition::EmbeddedLeafInput first{MakeKey(0x10), MakeLeaf(0x11111111u)};
    composition::EmbeddedLeafInput second{MakeKey(0x30), MakeLeaf(0x22222222u)};
    if (reverseOrder)
    {
        input.leaves.push_back(std::move(second));
        input.leaves.push_back(std::move(first));
    }
    else
    {
        input.leaves.push_back(std::move(first));
        input.leaves.push_back(std::move(second));
    }
    input.hasPresenter = true;
    input.presenterStableKey = MakeKey(0x30);
    input.contractBytes = {
        std::byte{0x43}, std::byte{0x4f}, std::byte{0x4e}, std::byte{0x54},
        std::byte{0x52}, std::byte{0x41}, std::byte{0x43}};
    input.verifiedDecisionBytes = {
        std::byte{0x56}, std::byte{0x45}, std::byte{0x52}, std::byte{0x49},
        std::byte{0x46}, std::byte{0x49}, std::byte{0x45}};
    input.verificationCertificateBytes = {
        std::byte{0x43}, std::byte{0x45}, std::byte{0x52}, std::byte{0x54},
        std::byte{0x49}, std::byte{0x46}, std::byte{0x49}, std::byte{0x43},
        std::byte{0x41}, std::byte{0x54}, std::byte{0x45}};
    return input;
}

std::uint32_t ReadU32(const std::vector<std::byte>& bytes, std::size_t offset)
{
    std::uint32_t value = 0;
    for (std::size_t index = 0; index < 4; ++index)
        value |= static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + index])) << (index * 8);
    return value;
}

std::uint64_t ReadU64(const std::vector<std::byte>& bytes, std::size_t offset)
{
    std::uint64_t value = 0;
    for (std::size_t index = 0; index < 8; ++index)
        value |= static_cast<std::uint64_t>(std::to_integer<std::uint8_t>(bytes[offset + index])) << (index * 8);
    return value;
}

void WriteU32(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value)
{
    for (std::size_t index = 0; index < 4; ++index)
        bytes[offset + index] = static_cast<std::byte>((value >> (index * 8)) & 0xffu);
}

void WriteU64(std::vector<std::byte>& bytes, std::size_t offset, std::uint64_t value)
{
    for (std::size_t index = 0; index < 8; ++index)
        bytes[offset + index] = static_cast<std::byte>((value >> (index * 8)) & 0xffu);
}

std::size_t DescriptorOffset(std::size_t index)
{
    return composition::FrozenCompositionHeaderBytes +
        index * composition::FrozenCompositionSectionDescriptorBytes;
}

std::size_t SectionOffset(const std::vector<std::byte>& bytes, std::size_t index)
{
    return static_cast<std::size_t>(ReadU64(bytes, DescriptorOffset(index) + 16));
}

std::size_t SectionSize(const std::vector<std::byte>& bytes, std::size_t index)
{
    return static_cast<std::size_t>(ReadU64(bytes, DescriptorOffset(index) + 24));
}

void PatchDigest(std::vector<std::byte>& bytes, std::size_t offset, const base::Digest256& digest)
{
    std::copy(digest.begin(), digest.end(), bytes.begin() + static_cast<std::ptrdiff_t>(offset));
}

void RefreshSectionDigest(std::vector<std::byte>& bytes, std::size_t index)
{
    const auto offset = SectionOffset(bytes, index);
    const auto size = SectionSize(bytes, index);
    const auto digest = base::Sha256(std::span<const std::byte>(bytes).subspan(offset, size));
    PatchDigest(bytes, DescriptorOffset(index) + 48, digest);
}

void RefreshSemanticDigest(std::vector<std::byte>& bytes)
{
    std::vector<composition::FrozenCompositionSectionInput> sections;
    for (std::size_t index = 0; index < 6; ++index)
    {
        composition::FrozenCompositionSectionInput section;
        section.kind = static_cast<composition::FrozenCompositionSectionKind>(
            ReadU32(bytes, DescriptorOffset(index)));
        const auto offset = SectionOffset(bytes, index);
        const auto size = SectionSize(bytes, index);
        section.bytes.assign(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
            bytes.begin() + static_cast<std::ptrdiff_t>(offset + size));
        sections.push_back(std::move(section));
    }
    PatchDigest(bytes, 56, composition::ComputeSemanticDigest(sections));
}

void RefreshFileDigest(std::vector<std::byte>& bytes)
{
    PatchDigest(bytes, composition::FrozenCompositionFileDigestOffset,
        composition::ComputeFrozenCompositionFileDigest(bytes));
}

void RefreshExecutionDigests(std::vector<std::byte>& bytes, std::size_t changedSection)
{
    RefreshSectionDigest(bytes, changedSection);
    if (changedSection <= 3) RefreshSemanticDigest(bytes);
    if (changedSection == 4)
    {
        const auto offset = SectionOffset(bytes, changedSection);
        const auto size = SectionSize(bytes, changedSection);
        PatchDigest(bytes, 88, base::Sha256(std::span<const std::byte>(bytes).subspan(offset, size)));
    }
    if (changedSection == 5)
    {
        const auto offset = SectionOffset(bytes, changedSection);
        const auto size = SectionSize(bytes, changedSection);
        PatchDigest(bytes, 120, base::Sha256(std::span<const std::byte>(bytes).subspan(offset, size)));
    }
    RefreshFileDigest(bytes);
}

void ExpectRejected(std::vector<std::byte> bytes, const char* message)
{
    if (composition::FrozenCompositionReader::Read(std::move(bytes)))
        throw std::runtime_error(message);
}

composition::FrozenCompositionBuildInput RebuildInput(const composition::FrozenComposition& artifact)
{
    composition::FrozenCompositionBuildInput input;
    for (const auto& leaf : artifact.Leaves())
    {
        composition::EmbeddedLeafInput copy;
        copy.stableKey = leaf.record.stableKey;
        copy.packageBytes.assign(leaf.packageBytes.begin(), leaf.packageBytes.end());
        input.leaves.push_back(std::move(copy));
    }
    if (artifact.Manifest().presenterLeafId != composition::FrozenCompositionInvalidIndex)
    {
        input.hasPresenter = true;
        input.presenterStableKey = artifact.Leaves()[artifact.Manifest().presenterLeafId].record.stableKey;
    }
    input.contractBytes.assign(artifact.ContractBytes().begin(), artifact.ContractBytes().end());
    input.verifiedDecisionBytes.assign(
        artifact.VerifiedDecisionBytes().begin(), artifact.VerifiedDecisionBytes().end());
    input.verificationCertificateBytes.assign(
        artifact.VerificationCertificateBytes().begin(), artifact.VerificationCertificateBytes().end());
    return input;
}
}

int main(int argc, char** argv)
{
    try
    {
        auto first = composition::FrozenCompositionWriter::Write(MakeCompositionInput(false));
        auto reordered = composition::FrozenCompositionWriter::Write(MakeCompositionInput(true));
        Require(static_cast<bool>(first), "canonical Frozen Composition writer failed");
        Require(static_cast<bool>(reordered), "reordered Frozen Composition writer failed");
        Require(first.Value() == reordered.Value(), "Leaf declaration order changed canonical bytes");

        auto decoded = composition::FrozenCompositionReader::Read(first.Value());
        Require(static_cast<bool>(decoded), "canonical Frozen Composition reader failed");
        Require(decoded.Value().Leaves().size() == 2, "decoded Leaf count differs");
        Require(decoded.Value().Manifest().presenterLeafId == 1, "presenter was not canonicalized by stable key");
        Require(decoded.Value().Leaves()[0].record.stableKey == MakeKey(0x10), "Leaf table is not stable-key ordered");

        auto rewritten = composition::FrozenCompositionWriter::Write(RebuildInput(decoded.Value()));
        Require(static_cast<bool>(rewritten), "round-trip writer failed");
        Require(rewritten.Value() == first.Value(), "round-trip bytes differ");

        auto oneLeaf = MakeCompositionInput(false);
        oneLeaf.leaves.pop_back();
        Require(!composition::FrozenCompositionWriter::Write(std::move(oneLeaf)), "one-Leaf composition was accepted");

        auto duplicate = MakeCompositionInput(false);
        duplicate.leaves[1].stableKey = duplicate.leaves[0].stableKey;
        Require(!composition::FrozenCompositionWriter::Write(std::move(duplicate)), "duplicate stable key was accepted");

        auto missingCertificate = MakeCompositionInput(false);
        missingCertificate.verificationCertificateBytes.clear();
        Require(!composition::FrozenCompositionWriter::Write(std::move(missingCertificate)), "empty certificate was accepted");

        auto unsupportedLeaf = MakeCompositionInput(false);
        unsupportedLeaf.leaves[0].packageBytes = MakeLeaf(0x33333333u, 18);
        Require(!composition::FrozenCompositionWriter::Write(std::move(unsupportedLeaf)), "Schema 18 Leaf was accepted by R1");

        auto badMagic = first.Value();
        badMagic[0] ^= std::byte{1};
        ExpectRejected(std::move(badMagic), "bad magic was accepted");

        auto rawCorruption = first.Value();
        rawCorruption.back() ^= std::byte{1};
        ExpectRejected(std::move(rawCorruption), "raw corruption was accepted");

        auto wrongSectionOrder = first.Value();
        WriteU32(wrongSectionOrder, DescriptorOffset(0), 2);
        RefreshFileDigest(wrongSectionOrder);
        ExpectRejected(std::move(wrongSectionOrder), "non-canonical section order was accepted");

        auto invalidPresenter = first.Value();
        WriteU32(invalidPresenter, SectionOffset(invalidPresenter, 0) + 4, 2);
        RefreshExecutionDigests(invalidPresenter, 0);
        ExpectRejected(std::move(invalidPresenter), "out-of-range presenter was accepted");

        auto invalidLeafOffset = first.Value();
        WriteU64(invalidLeafOffset, SectionOffset(invalidLeafOffset, 1) + 40, 8);
        RefreshExecutionDigests(invalidLeafOffset, 1);
        ExpectRejected(std::move(invalidLeafOffset), "non-canonical Leaf offset was accepted");

        auto corruptEmbeddedLeaf = first.Value();
        corruptEmbeddedLeaf[SectionOffset(corruptEmbeddedLeaf, 2)] ^= std::byte{1};
        RefreshExecutionDigests(corruptEmbeddedLeaf, 2);
        ExpectRejected(std::move(corruptEmbeddedLeaf), "corrupt embedded Leaf was accepted after outer digest repair");

        auto staleSemanticDigest = first.Value();
        staleSemanticDigest[SectionOffset(staleSemanticDigest, 3)] ^= std::byte{1};
        RefreshSectionDigest(staleSemanticDigest, 3);
        RefreshFileDigest(staleSemanticDigest);
        ExpectRejected(std::move(staleSemanticDigest), "contract mutation with stale semantic digest was accepted");

        auto nonZeroPadding = first.Value();
        const auto contractEnd = SectionOffset(nonZeroPadding, 3) + SectionSize(nonZeroPadding, 3);
        Require(contractEnd < SectionOffset(nonZeroPadding, 4), "test fixture did not create section padding");
        nonZeroPadding[contractEnd] = std::byte{1};
        RefreshFileDigest(nonZeroPadding);
        ExpectRejected(std::move(nonZeroPadding), "non-zero canonical padding was accepted");

        if (argc == 3 && std::string(argv[1]) == "--emit")
        {
            const auto write = base::WriteAllBytes(std::filesystem::path(argv[2]), first.Value());
            Require(static_cast<bool>(write), "failed to emit canonical Frozen Composition artifact");
        }
        else if (argc != 1)
        {
            throw std::runtime_error("usage: 46_CanonicalCompositionArtifactTests [--emit <path>]");
        }

        std::cout << "Canonical Frozen Composition boundary tests passed.\n";
        std::cout << "Artifact bytes: " << first.Value().size() << '\n';
        std::cout << "Artifact SHA-256: " << base::ToHex(base::Sha256(first.Value())) << '\n';
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
