#include "./FileIO.h"

#include <fstream>

namespace sge4::base
{
Expected<std::vector<std::byte>, std::string> ReadAllBytes(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) return Failure<std::vector<std::byte>, std::string>("読み込み対象のファイルを開けませんでした。");
    const auto end = stream.tellg();
    if (end < 0) return Failure<std::vector<std::byte>, std::string>("ファイルサイズを取得できませんでした。");
    std::vector<std::byte> bytes(static_cast<std::size_t>(end));
    stream.seekg(0, std::ios::beg);
    if (!bytes.empty() && !stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
        return Failure<std::vector<std::byte>, std::string>("ファイルを読み込めませんでした。");
    return Success<std::vector<std::byte>, std::string>(std::move(bytes));
}

Expected<void, std::string> WriteAllBytes(const std::filesystem::path& path, std::span<const std::byte> bytes)
{
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) return Failure<void, std::string>("書き込み対象のファイルを開けませんでした。");
    if (!bytes.empty()) stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!stream) return Failure<void, std::string>("ファイルを書き込めませんでした。");
    return Success<void, std::string>();
}
}
