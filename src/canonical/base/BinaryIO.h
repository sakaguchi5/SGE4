#pragma once

#include "./Expected.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace sge4::base
{
class BinaryWriter final
{
public:
    void WriteU8(std::uint8_t value);
    void WriteU16(std::uint16_t value);
    void WriteU32(std::uint32_t value);
    void WriteI32(std::int32_t value);
    void WriteU64(std::uint64_t value);

    template<class Enum>
        requires std::is_enum_v<Enum>
    void WriteEnumU16(Enum value)
    {
        using Underlying = std::underlying_type_t<Enum>;
        static_assert(sizeof(Underlying) <= sizeof(std::uint16_t));
        WriteU16(static_cast<std::uint16_t>(std::to_underlying(value)));
    }

    template<class Enum>
        requires std::is_enum_v<Enum>
    void WriteEnumU32(Enum value)
    {
        using Underlying = std::underlying_type_t<Enum>;
        static_assert(sizeof(Underlying) <= sizeof(std::uint32_t));
        WriteU32(static_cast<std::uint32_t>(std::to_underlying(value)));
    }

    void WriteCountU32(std::size_t value);
    void WriteBytes(std::span<const std::byte> bytes);
    void WriteZeroes(std::size_t count);
    void Align(std::uint32_t alignment);
    void PatchU16(std::size_t offset, std::uint16_t value);
    void PatchU32(std::size_t offset, std::uint32_t value);
    void PatchU64(std::size_t offset, std::uint64_t value);
    void PatchBytes(std::size_t offset, std::span<const std::byte> bytes);

    [[nodiscard]] std::size_t Size() const noexcept { return bytes_.size(); }
    [[nodiscard]] const std::vector<std::byte>& Bytes() const noexcept { return bytes_; }
    [[nodiscard]] std::vector<std::byte> Take() && noexcept { return std::move(bytes_); }

private:
    std::vector<std::byte> bytes_;
};

class BinaryReader final
{
public:
    explicit BinaryReader(std::span<const std::byte> bytes) : bytes_(bytes) {}

    [[nodiscard]] Expected<std::uint8_t, std::string> ReadU8();
    [[nodiscard]] Expected<std::uint16_t, std::string> ReadU16();
    [[nodiscard]] Expected<std::uint32_t, std::string> ReadU32();
    [[nodiscard]] Expected<std::int32_t, std::string> ReadI32();
    [[nodiscard]] Expected<std::uint64_t, std::string> ReadU64();
    [[nodiscard]] Expected<std::span<const std::byte>, std::string> ReadBytes(std::size_t count);
    [[nodiscard]] Expected<void, std::string> Skip(std::size_t count);
    [[nodiscard]] std::size_t Position() const noexcept { return position_; }
    [[nodiscard]] std::size_t Remaining() const noexcept { return bytes_.size() - position_; }

private:
    std::span<const std::byte> bytes_;
    std::size_t position_ = 0;
};
}
