#pragma once

#include "CompositionContract.h"

#include <cstdint>

namespace sge4::composition
{
enum class DynamicExecutionModeV1 : std::uint32_t
{
    AuthorityOnly = 0,
    VerifiedDenseSlot = 1
};

// Level 4 Generalization 1 keeps the execution route deliberately narrow:
// one exact dynamic member universe is materialized into one dense Dynamic Slot.
// The route is frozen in the Composition and cannot be selected by Runtime.
struct DynamicContractV1 final
{
    std::uint32_t schemaVersion = 2;
    std::uint32_t universeCount = 0;
    DynamicExecutionModeV1 executionMode = DynamicExecutionModeV1::AuthorityOnly;
    LeafPackageId targetLeaf;
    std::uint32_t targetDynamicSlot = package::InvalidIndex;
    std::uint32_t memberBytes = 0;
};

[[nodiscard]] inline DynamicContractV1 MakeAuthorityOnlyDynamicContractV1(
    std::uint32_t universeCount) noexcept
{
    return {2, universeCount, DynamicExecutionModeV1::AuthorityOnly,
        {}, package::InvalidIndex, 0};
}

[[nodiscard]] inline DynamicContractV1 MakeVerifiedDenseSlotDynamicContractV1(
    std::uint32_t universeCount,
    LeafPackageId targetLeaf,
    std::uint32_t targetDynamicSlot,
    std::uint32_t memberBytes) noexcept
{
    return {2, universeCount, DynamicExecutionModeV1::VerifiedDenseSlot,
        targetLeaf, targetDynamicSlot, memberBytes};
}
}
