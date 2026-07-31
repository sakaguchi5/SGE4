#include "./BinaryIO.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace sge4::base
{
namespace
{
template<class T>
void WriteLittle(std::vector<std::byte>& out, T value)
{
    for (std::size_t i = 0; i < sizeof(T); ++i)
        out.push_back(static_cast<std::byte>((static_cast<std::uint64_t>(value) >> (i * 8)) & 0xffu));
}

template<class T>
void PatchLittle(std::vector<std::byte>& out, std::size_t offset, T value)
{
    if (offset + sizeof(T) > out.size()) throw std::out_of_range("入力または内部状態が検証または実行の契約に違反しています。");
    for (std::size_t i = 0; i < sizeof(T); ++i)
        out[offset + i] = static_cast<std::byte>((static_cast<std::uint64_t>(value) >> (i * 8)) & 0xffu);
}

template<class T>
Expected<T, std::string> ReadLittle(std::span<const std::byte> bytes, std::size_t& position)
{
    if (position + sizeof(T) > bytes.size()) return Failure<T, std::string>("バイナリ読み込み位置が範囲外です。");
    std::uint64_t value = 0;
    for (std::size_t i = 0; i < sizeof(T); ++i)
        value |= static_cast<std::uint64_t>(std::to_integer<std::uint8_t>(bytes[position + i])) << (i * 8);
    position += sizeof(T);
    return Success<T, std::string>(static_cast<T>(value));
}
}

void BinaryWriter::WriteU8(std::uint8_t value) { bytes_.push_back(static_cast<std::byte>(value)); }
void BinaryWriter::WriteU16(std::uint16_t value) { WriteLittle(bytes_, value); }
void BinaryWriter::WriteU32(std::uint32_t value) { WriteLittle(bytes_, value); }
void BinaryWriter::WriteI32(std::int32_t value) { WriteLittle(bytes_, static_cast<std::uint32_t>(value)); }
void BinaryWriter::WriteU64(std::uint64_t value) { WriteLittle(bytes_, value); }
void BinaryWriter::WriteCountU32(std::size_t value)
{
    if (value > std::numeric_limits<std::uint32_t>::max())
        throw std::length_error("検証または実行の契約に違反しています。");
    WriteU32(static_cast<std::uint32_t>(value));
}
void BinaryWriter::WriteBytes(std::span<const std::byte> bytes) { bytes_.insert(bytes_.end(), bytes.begin(), bytes.end()); }
void BinaryWriter::WriteZeroes(std::size_t count) { bytes_.insert(bytes_.end(), count, std::byte{0}); }
void BinaryWriter::Align(std::uint32_t alignment)
{
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) throw std::invalid_argument("入力または内部状態が検証または実行の契約に違反しています。");
    const auto remainder = bytes_.size() % alignment;
    if (remainder != 0) WriteZeroes(alignment - remainder);
}
void BinaryWriter::PatchU16(std::size_t offset, std::uint16_t value) { PatchLittle(bytes_, offset, value); }
void BinaryWriter::PatchU32(std::size_t offset, std::uint32_t value) { PatchLittle(bytes_, offset, value); }
void BinaryWriter::PatchU64(std::size_t offset, std::uint64_t value) { PatchLittle(bytes_, offset, value); }
void BinaryWriter::PatchBytes(std::size_t offset, std::span<const std::byte> bytes)
{
    if (offset + bytes.size() > bytes_.size()) throw std::out_of_range("入力または内部状態が検証または実行の契約に違反しています。");
    std::copy(bytes.begin(), bytes.end(), bytes_.begin() + static_cast<std::ptrdiff_t>(offset));
}

Expected<std::uint8_t, std::string> BinaryReader::ReadU8() { return ReadLittle<std::uint8_t>(bytes_, position_); }
Expected<std::uint16_t, std::string> BinaryReader::ReadU16() { return ReadLittle<std::uint16_t>(bytes_, position_); }
Expected<std::uint32_t, std::string> BinaryReader::ReadU32() { return ReadLittle<std::uint32_t>(bytes_, position_); }
Expected<std::int32_t, std::string> BinaryReader::ReadI32()
{
    auto value = ReadLittle<std::uint32_t>(bytes_, position_);
    if (!value) return Failure<std::int32_t, std::string>(value.error());
    return Success<std::int32_t, std::string>(static_cast<std::int32_t>(value.value()));
}
Expected<std::uint64_t, std::string> BinaryReader::ReadU64() { return ReadLittle<std::uint64_t>(bytes_, position_); }
Expected<std::span<const std::byte>, std::string> BinaryReader::ReadBytes(std::size_t count)
{
    if (position_ + count > bytes_.size()) return Failure<std::span<const std::byte>, std::string>("バイナリ読み込み位置が範囲外です。");
    auto result = bytes_.subspan(position_, count);
    position_ += count;
    return Success<std::span<const std::byte>, std::string>(result);
}
Expected<void, std::string> BinaryReader::Skip(std::size_t count)
{
    if (position_ + count > bytes_.size()) return Failure<void, std::string>("バイナリの読み飛ばし位置が範囲外です。");
    position_ += count;
    return Success<void, std::string>();
}
}
