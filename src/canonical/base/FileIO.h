#pragma once

#include "./Expected.h"

#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace sge4::base
{
[[nodiscard]] Expected<std::vector<std::byte>, std::string> ReadAllBytes(const std::filesystem::path& path);
[[nodiscard]] Expected<void, std::string> WriteAllBytes(const std::filesystem::path& path, std::span<const std::byte> bytes);
}
