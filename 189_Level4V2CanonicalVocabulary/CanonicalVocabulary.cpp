#include "CanonicalVocabulary.h"

#include "../00_Foundation/Sha256.h"

#include <limits>
#include <stdexcept>
#include <vector>

namespace sge4_5::v2::canonical
{
namespace
{
void AppendU32(std::vector<std::byte>& bytes, std::uint32_t value)
{
    for (std::uint32_t index = 0; index < sizeof(value); ++index)
        bytes.push_back(static_cast<std::byte>((value >> (index * 8u)) & 0xffu));
}

void AppendU64(std::vector<std::byte>& bytes, std::uint64_t value)
{
    for (std::uint32_t index = 0; index < sizeof(value); ++index)
        bytes.push_back(static_cast<std::byte>((value >> (index * 8u)) & 0xffu));
}

void AppendString(std::vector<std::byte>& bytes, std::string_view value)
{
    if (value.size() > std::numeric_limits<std::uint32_t>::max())
        throw std::invalid_argument("Canonical identity domain is too large.");
    AppendU32(bytes, static_cast<std::uint32_t>(value.size()));
    for (const char character : value)
        bytes.push_back(static_cast<std::byte>(static_cast<unsigned char>(character)));
}
}

std::optional<DeviceEpoch> DeviceEpoch::TryCreate(std::uint64_t value) noexcept
{
    if (value == 0u) return std::nullopt;
    return DeviceEpoch(value);
}

Digest256V1 ComputeCanonicalDigestV1(const CanonicalIdentityMaterialV1& material)
{
    if (material.domain.empty())
        throw std::invalid_argument("Canonical identity domain must not be empty.");

    std::vector<std::byte> bytes;
    bytes.reserve(material.domain.size() + material.canonicalPayload.size() + 24u);
    AppendString(bytes, material.domain);
    AppendU32(bytes, material.schemaVersion);
    AppendU64(bytes, static_cast<std::uint64_t>(material.canonicalPayload.size()));
    bytes.insert(bytes.end(), material.canonicalPayload.begin(), material.canonicalPayload.end());

    const auto digest = base::Sha256(bytes);
    Digest256V1 result{};
    for (std::size_t index = 0; index < result.size(); ++index)
        result[index] = digest[index];
    return result;
}
}
