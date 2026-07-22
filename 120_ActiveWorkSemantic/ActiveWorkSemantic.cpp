#include "ActiveWorkSemantic.h"

#include "../00_Foundation/BinaryIO.h"

#include <limits>
#include <stdexcept>
#include <utility>

namespace sge4_5::spiral4::semantic
{
namespace
{
constexpr std::uint32_t SemanticMagic = 0x31573453u; // "S4W1"
}

MaxWorkCountV1::MaxWorkCountV1(std::uint32_t value)
    : value_(value)
{
    if (value == 0 || value > 65535u * 64u)
    {
        throw std::invalid_argument("MaxWorkCountV1 is outside the qualified range.");
    }
}

ActiveWorkCountV1::ActiveWorkCountV1(std::uint32_t value, MaxWorkCountV1 maximum)
    : value_(value)
{
    if (value > maximum.Value())
    {
        throw std::invalid_argument("ActiveWorkCountV1 exceeds MaxWorkCountV1.");
    }
}

ThreadGroupSizeV1::ThreadGroupSizeV1(std::uint32_t value)
    : value_(value)
{
    if (value == 0 || value > 1024)
    {
        throw std::invalid_argument("ThreadGroupSizeV1 is outside the D3D12 Compute range.");
    }
}

ActiveWorkSemanticV1 BuildCanonicalActiveWorkSemanticV1()
{
    return ActiveWorkSemanticV1{};
}

std::vector<std::byte> SerializeActiveWorkSemanticV1(const ActiveWorkSemanticV1& value)
{
    if (value.conventionVersion != 1 ||
        value.boneCount != 8 ||
        value.maxReusePerBone != 512 ||
        value.maxWorkCount.Value() != value.boneCount * value.maxReusePerBone ||
        value.threadsPerGroup.Value() != 64 ||
        value.workRecordStride != 16 ||
        value.outputRecordStride != 16 ||
        value.representationControl != 1)
    {
        throw std::invalid_argument("ActiveWorkSemanticV1 is not canonical.");
    }

    base::BinaryWriter writer;
    writer.WriteU32(SemanticMagic);
    writer.WriteU32(value.conventionVersion);
    writer.WriteU32(value.boneCount);
    writer.WriteU32(value.maxReusePerBone);
    writer.WriteU32(value.maxWorkCount.Value());
    writer.WriteU32(value.threadsPerGroup.Value());
    writer.WriteU32(value.workRecordStride);
    writer.WriteU32(value.outputRecordStride);
    writer.WriteU32(value.representationControl);
    return std::move(writer).Take();
}

base::Digest256 ActiveWorkSemanticIdentityV1(const ActiveWorkSemanticV1& value)
{
    const auto bytes = SerializeActiveWorkSemanticV1(value);
    return base::Sha256(bytes);
}

std::uint32_t DispatchGroupCountX(
    ActiveWorkCountV1 activeCount,
    ThreadGroupSizeV1 groupSize) noexcept
{
    const std::uint32_t n = activeCount.Value();
    const std::uint32_t g = groupSize.Value();
    return n == 0 ? 0u : 1u + ((n - 1u) / g);
}

const std::array<std::uint32_t, 19>& ActiveCountCorpusV1() noexcept
{
    static constexpr std::array<std::uint32_t, 19> values{
        0u, 1u, 63u, 64u, 65u, 127u, 128u, 129u, 255u, 256u,
        257u, 511u, 512u, 513u, 1023u, 1024u, 2048u, 4095u, 4096u
    };
    return values;
}
}
