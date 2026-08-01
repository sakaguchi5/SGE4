#pragma once

#include "CompositionContract.h"

#include <cstdint>
#include <utility>
#include <vector>

namespace sge4::composition
{
struct ConditionalRegionTag;
using ConditionalRegionId = Id32<ConditionalRegionTag>;

enum class DynamicExecutionModeV1 : std::uint32_t
{
    AuthorityOnly = 0,
    VerifiedDenseSlot = 1
};

// Generalization 2 deliberately derives branch selection only from exact sets that
// are already independently verified by the Dynamic Verifier. Runtime never accepts
// an unsealed bool and never invents a branch policy.
enum class ConditionalPredicateKindV1 : std::uint32_t
{
    ActiveSetNonEmpty = 1,
    ActivationSetNonEmpty = 2,
    DeactivationSetNonEmpty = 3,
    UpdateSetNonEmpty = 4,
    RetainSetNonEmpty = 5,
    TransitionSetNonEmpty = 6
};

// Non-nested v1 region. Leaves may belong to at most one region. Unselected leaves
// are not submitted; their resources and completion tokens retain the last accepted
// state. Cross-region and conditional-to-unconditional data dependencies are rejected
// by the Composition Toolchain.
struct ConditionalRegionV1 final
{
    ConditionalRegionId id;
    ConditionalPredicateKindV1 predicate =
        ConditionalPredicateKindV1::ActiveSetNonEmpty;
    std::vector<LeafPackageId> trueLeaves;
    std::vector<LeafPackageId> falseLeaves;
};

// Level 4 Generalization 1 keeps the execution route deliberately narrow:
// one exact dynamic member universe is materialized into one dense Dynamic Slot.
// Generalization 2 adds a finite set of non-nested Conditional Regions whose
// predicates are derived from the same verified Dynamic Decision.
struct DynamicContractV1 final
{
    std::uint32_t schemaVersion = 3;
    std::uint32_t universeCount = 0;
    DynamicExecutionModeV1 executionMode = DynamicExecutionModeV1::AuthorityOnly;
    LeafPackageId targetLeaf;
    std::uint32_t targetDynamicSlot = package::InvalidIndex;
    std::uint32_t memberBytes = 0;
    std::vector<ConditionalRegionV1> conditionalRegions;
};

[[nodiscard]] inline ConditionalRegionV1 MakeConditionalRegionV1(
    std::uint32_t id,
    ConditionalPredicateKindV1 predicate,
    std::vector<LeafPackageId> trueLeaves,
    std::vector<LeafPackageId> falseLeaves = {})
{
    return {ConditionalRegionId{id}, predicate,
        std::move(trueLeaves), std::move(falseLeaves)};
}

[[nodiscard]] inline DynamicContractV1 MakeAuthorityOnlyDynamicContractV1(
    std::uint32_t universeCount,
    std::vector<ConditionalRegionV1> conditionalRegions = {})
{
    return {3, universeCount, DynamicExecutionModeV1::AuthorityOnly,
        {}, package::InvalidIndex, 0, std::move(conditionalRegions)};
}

[[nodiscard]] inline DynamicContractV1 MakeVerifiedDenseSlotDynamicContractV1(
    std::uint32_t universeCount,
    LeafPackageId targetLeaf,
    std::uint32_t targetDynamicSlot,
    std::uint32_t memberBytes,
    std::vector<ConditionalRegionV1> conditionalRegions = {})
{
    return {3, universeCount, DynamicExecutionModeV1::VerifiedDenseSlot,
        targetLeaf, targetDynamicSlot, memberBytes, std::move(conditionalRegions)};
}
}
