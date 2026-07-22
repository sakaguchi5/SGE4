#pragma once

#include "../00_Foundation/Sha256.h"

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace sge4_5::spiral4::semantic
{
class MaxWorkCountV1 final
{
public:
    explicit MaxWorkCountV1(std::uint32_t value);
    [[nodiscard]] std::uint32_t Value() const noexcept { return value_; }
    auto operator<=>(const MaxWorkCountV1&) const = default;
private:
    std::uint32_t value_ = 0;
};

class ActiveWorkCountV1 final
{
public:
    ActiveWorkCountV1(std::uint32_t value, MaxWorkCountV1 maximum);
    [[nodiscard]] std::uint32_t Value() const noexcept { return value_; }
    auto operator<=>(const ActiveWorkCountV1&) const = default;
private:
    std::uint32_t value_ = 0;
};

class ThreadGroupSizeV1 final
{
public:
    explicit ThreadGroupSizeV1(std::uint32_t value);
    [[nodiscard]] std::uint32_t Value() const noexcept { return value_; }
    auto operator<=>(const ThreadGroupSizeV1&) const = default;
private:
    std::uint32_t value_ = 0;
};

struct ActiveWorkSemanticV1 final
{
    std::uint32_t conventionVersion = 1;
    std::uint32_t boneCount = 8;
    std::uint32_t maxReusePerBone = 512;
    MaxWorkCountV1 maxWorkCount{4096};
    ThreadGroupSizeV1 threadsPerGroup{64};
    std::uint32_t workRecordStride = 16;
    std::uint32_t outputRecordStride = 16;
    std::uint32_t representationControl = 1; // DirectPgaThroughConsumerV1
};

[[nodiscard]] ActiveWorkSemanticV1 BuildCanonicalActiveWorkSemanticV1();
[[nodiscard]] std::vector<std::byte> SerializeActiveWorkSemanticV1(const ActiveWorkSemanticV1& value);
[[nodiscard]] base::Digest256 ActiveWorkSemanticIdentityV1(const ActiveWorkSemanticV1& value);
[[nodiscard]] std::uint32_t DispatchGroupCountX(
    ActiveWorkCountV1 activeCount,
    ThreadGroupSizeV1 groupSize) noexcept;

[[nodiscard]] const std::array<std::uint32_t, 19>& ActiveCountCorpusV1() noexcept;
}
