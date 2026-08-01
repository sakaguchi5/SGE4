#pragma once

#include "../../toolchain/CompositionToolchain.h"

#include <span>
#include <vector>

namespace sge4::composition::migration::abi1
{
// v1.5.2のSGE4UNI 1.1を資格試験用に再生成する。
[[nodiscard]] base::Expected<std::vector<std::byte>, Error>
BuildFrozenCompositionPackageAbi1ForMigration(
    ContractBuildInput input,
    DynamicContractV1 dynamicContract);

// Production RuntimeはABI 1を受理しない。明示的なMigration Toolだけが
// SGE4UNI 1.1を読み、意味を再検証してSGE4UNI 2.3へ変換する。
[[nodiscard]] base::Expected<FrozenCompositionPackage, Error>
MigrateFrozenCompositionPackageAbi1ToAbi2(std::span<const std::byte> bytes);
}
