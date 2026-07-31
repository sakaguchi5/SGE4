#pragma once

#include "../base/Expected.h"
#include "../base/Sha256.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace sge4
{
using Digest256 = sge4::base::Digest256;

struct Error final
{
    std::string stage;
    std::string message;
};

enum class SectionFlags : std::uint16_t
{
    None = 0,
    Required = 1u << 0,
    ExecutionAffecting = 1u << 1
};

[[nodiscard]] constexpr std::uint16_t operator|(SectionFlags left, SectionFlags right) noexcept
{
    return static_cast<std::uint16_t>(left) | static_cast<std::uint16_t>(right);
}

struct SectionInput final
{
    std::uint32_t kind = 0;
    std::uint16_t schemaVersion = 1;
    std::uint16_t flags = static_cast<std::uint16_t>(SectionFlags::Required) |
        static_cast<std::uint16_t>(SectionFlags::ExecutionAffecting);
    std::uint32_t alignment = 8;
    std::vector<std::byte> bytes;
};

struct SectionView final
{
    std::uint32_t kind = 0;
    std::uint16_t schemaVersion = 1;
    std::uint16_t flags = 0;
    std::uint32_t alignment = 1;
    std::uint64_t fileOffset = 0;
    std::uint64_t storedBytes = 0;
    Digest256 digest{};
    std::span<const std::byte> bytes;
};

class SectionedArtifact final
{
public:
    SectionedArtifact(SectionedArtifact&&) noexcept = default;
    SectionedArtifact& operator=(SectionedArtifact&&) noexcept = default;
    SectionedArtifact(const SectionedArtifact&) = delete;
    SectionedArtifact& operator=(const SectionedArtifact&) = delete;

    [[nodiscard]] std::uint16_t FormatMajor() const noexcept { return formatMajor_; }
    [[nodiscard]] std::uint16_t FormatMinor() const noexcept { return formatMinor_; }
    [[nodiscard]] const Digest256& SemanticDigest() const noexcept { return semanticDigest_; }
    [[nodiscard]] const Digest256& FileDigest() const noexcept { return fileDigest_; }
    [[nodiscard]] std::span<const SectionView> Sections() const noexcept { return sections_; }
    [[nodiscard]] std::span<const std::byte> FileBytes() const noexcept { return storage_; }
    [[nodiscard]] const SectionView* FindSection(std::uint32_t kind) const noexcept;

private:
    SectionedArtifact() = default;
    friend sge4::base::Expected<SectionedArtifact, Error> ReadSectionedArtifact(
        std::span<const std::byte>, const std::array<std::byte, 8>&, std::uint16_t);

    std::uint16_t formatMajor_ = 0;
    std::uint16_t formatMinor_ = 0;
    Digest256 semanticDigest_{};
    Digest256 fileDigest_{};
    std::vector<std::byte> storage_;
    std::vector<SectionView> sections_;
};

inline constexpr std::uint32_t ArtifactHeaderBytes = 128;
inline constexpr std::uint32_t ArtifactSectionDescriptorBytes = 64;
inline constexpr std::size_t ArtifactFileDigestOffset = 80;

[[nodiscard]] Digest256 ComputeDomainDigest(
    std::string_view domain,
    std::uint32_t schemaVersion,
    std::span<const std::byte> payload);

[[nodiscard]] sge4::base::Expected<std::vector<std::byte>, Error> WriteSectionedArtifact(
    const std::array<std::byte, 8>& magic,
    std::uint16_t formatMajor,
    std::uint16_t formatMinor,
    std::vector<SectionInput> sections);

[[nodiscard]] sge4::base::Expected<SectionedArtifact, Error> ReadSectionedArtifact(
    std::span<const std::byte> bytes,
    const std::array<std::byte, 8>& expectedMagic,
    std::uint16_t expectedMajor);

[[nodiscard]] std::vector<std::byte> DigestBytes(const Digest256& digest);
[[nodiscard]] bool DigestEqual(std::span<const std::byte> bytes, const Digest256& digest) noexcept;
}
