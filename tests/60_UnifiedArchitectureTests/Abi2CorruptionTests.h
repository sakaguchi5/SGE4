#pragma once

#include <cstddef>
#include <span>

namespace sge4::tests
{
void VerifyAbi2CorruptionRejection(std::span<const std::byte> validBytes);
}
