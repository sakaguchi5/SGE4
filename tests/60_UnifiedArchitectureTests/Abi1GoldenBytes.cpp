#include "Abi1GoldenBytes.h"

#include "../../src/canonical/artifact/SectionedArtifact.h"
#include "../../src/canonical/base/Sha256.h"
#include "../../src/composition/migration/abi1/container/FrozenCompositionWriter.h"
#include "../../src/leaf/artifact/package/PackageWriter.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sge4::tests
{
namespace
{
[[nodiscard]] std::vector<std::byte> Bytes(std::string_view text)
{
    std::vector<std::byte> result;
    result.reserve(text.size());
    for (const unsigned char value : text)
        result.push_back(static_cast<std::byte>(value));
    return result;
}

[[nodiscard]] base::Digest256 StableKey(std::string_view text)
{
    const auto bytes = Bytes(text);
    return base::Sha256(bytes);
}

void RequireDigest(std::span<const std::byte> bytes, std::string_view expected, const char* message)
{
    if (base::ToHex(base::Sha256(bytes)) != expected)
        throw std::runtime_error(message);
}

[[nodiscard]] std::vector<std::byte> BuildLeaf(std::string_view name, std::uint8_t marker)
{
    package::PackageBuildInput input;
    input.targetKind = package::TargetKindD3D12;
    input.targetSchemaVersion = 17;
    input.minimumRuntimeVersion = 17;

    package::PackageSectionInput profile;
    profile.kind = package::SectionKind::D3D12TargetProfile;
    profile.schemaVersion = 1;
    profile.flags = static_cast<std::uint32_t>(package::SectionFlags::Required) |
        static_cast<std::uint32_t>(package::SectionFlags::ExecutionAffecting);
    profile.alignment = 8;
    profile.bytes = Bytes("d3d12-profile-v17");
    profile.bytes.push_back(static_cast<std::byte>(marker));

    package::PackageSectionInput manifest;
    manifest.kind = package::SectionKind::Manifest;
    manifest.schemaVersion = 1;
    manifest.flags = static_cast<std::uint32_t>(package::SectionFlags::Required) |
        static_cast<std::uint32_t>(package::SectionFlags::ExecutionAffecting);
    manifest.alignment = 8;
    manifest.bytes = Bytes(name);

    input.sections.push_back(std::move(profile));
    input.sections.push_back(std::move(manifest));
    auto written = package::PackageWriter::Write(std::move(input));
    if (!written)
        throw std::runtime_error("Frozen Leaf PackageのGolden bytes生成に失敗しました。");
    return std::move(written).value();
}
}

void VerifyAbi1GoldenBytes()
{
    const std::array<std::byte, 8> magic = {
        std::byte{'A'}, std::byte{'B'}, std::byte{'I'}, std::byte{'T'},
        std::byte{'E'}, std::byte{'S'}, std::byte{'T'}, std::byte{0}};

    std::vector<SectionInput> genericSections;
    genericSections.push_back({2, 3, static_cast<std::uint16_t>(SectionFlags::Required),
        16, Bytes("second")});
    genericSections.push_back({1, 2,
        static_cast<std::uint16_t>(SectionFlags::Required) |
            static_cast<std::uint16_t>(SectionFlags::ExecutionAffecting),
        8, Bytes("first")});
    auto generic = WriteSectionedArtifact(magic, 1, 1, std::move(genericSections));
    if (!generic)
        throw std::runtime_error("Sectioned ArtifactのGolden bytes生成に失敗しました。");
    RequireDigest(generic.value(),
        "3f3c1a30b5daeb71eeeeaa6783805679d74da29e00802000de429f32033fc106",
        "Sectioned ArtifactのABI 1.x bytesがv1.4 Baselineと一致しません。");

    auto leafA = BuildLeaf("leaf-a-manifest", 0x31);
    auto leafB = BuildLeaf("leaf-b-manifest", 0x32);
    RequireDigest(leafA,
        "edbde3d1929b8f6208765761673b3fa265f48f3a936752adac3cd16a11d5f01e",
        "Frozen Leaf Package AのABI 1.x bytesがv1.4 Baselineと一致しません。");
    RequireDigest(leafB,
        "2b4df7947fd216ba564ceb0c656d2ba2912baba0745226d1000d7e024e437932",
        "Frozen Leaf Package BのABI 1.x bytesがv1.4 Baselineと一致しません。");

    composition::FrozenCompositionBuildInput input;
    input.leaves.push_back({StableKey("leaf-a"), leafA});
    input.leaves.push_back({StableKey("leaf-b"), leafB});
    input.contractBytes = Bytes("contract-v1");
    input.verifiedDecisionBytes = Bytes("verified-decision-v1");
    input.verificationCertificateBytes = Bytes("verification-certificate-v1");
    auto compositionBytes = composition::FrozenCompositionWriter::Write(std::move(input));
    if (!compositionBytes)
        throw std::runtime_error("Frozen CompositionのGolden bytes生成に失敗しました。");
    RequireDigest(compositionBytes.value(),
        "46b140c4dfdf224be571fcc5f03ddf4f0489994e0b2a89aa0ac7466d919adfea",
        "Frozen CompositionのABI 1.x bytesがv1.4 Baselineと一致しません。");
}
}
