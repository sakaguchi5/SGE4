#pragma once

#include "CompositionContract.h"

#include <compare>
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
    // Schema 5 generalizes the original single dense slot into one or more
    // independently materialized dense routes.  The historic enum value is
    // preserved so existing single-route compositions remain source-compatible.
    VerifiedDenseSlot = 1
};

struct DynamicExecutionRouteV1 final
{
    LeafPackageId targetLeaf;
    std::uint32_t targetDynamicSlot = package::InvalidIndex;
    std::uint32_t sourceByteOffset = 0;
    std::uint32_t routeMemberBytes = 0;
    auto operator<=>(const DynamicExecutionRouteV1&) const = default;
};

[[nodiscard]] inline DynamicExecutionRouteV1 MakeDynamicExecutionRouteV1(
    LeafPackageId targetLeaf,
    std::uint32_t targetDynamicSlot,
    std::uint32_t sourceByteOffset,
    std::uint32_t routeMemberBytes)
{
    return {targetLeaf, targetDynamicSlot, sourceByteOffset, routeMemberBytes};
}

enum class IndirectExecutionModeV1 : std::uint32_t
{
    None = 0,
    VerifiedDispatch = 1
};

struct VerifiedIndirectDispatchContractV1 final
{
    IndirectExecutionModeV1 mode = IndirectExecutionModeV1::None;
    LeafPackageId targetLeaf;
    std::uint32_t targetComputeCommand = package::InvalidIndex;
    std::uint32_t maxWorkCount = 0;
};

[[nodiscard]] inline VerifiedIndirectDispatchContractV1 MakeVerifiedIndirectDispatchContractV1(
    LeafPackageId targetLeaf,
    std::uint32_t targetComputeCommand,
    std::uint32_t maxWorkCount)
{
    return {IndirectExecutionModeV1::VerifiedDispatch, targetLeaf,
        targetComputeCommand, maxWorkCount};
}

enum class ConditionalPredicateKindV1 : std::uint32_t
{
    ActiveSetNonEmpty = 1,
    ActivationSetNonEmpty = 2,
    DeactivationSetNonEmpty = 3,
    UpdateSetNonEmpty = 4,
    RetainSetNonEmpty = 5,
    TransitionSetNonEmpty = 6
};

struct ConditionalRegionV1 final
{
    ConditionalRegionId id;
    ConditionalPredicateKindV1 predicate =
        ConditionalPredicateKindV1::ActiveSetNonEmpty;
    std::vector<LeafPackageId> trueLeaves;
    std::vector<LeafPackageId> falseLeaves;
};

// Generalization 6 keeps one canonical payload per member and maps fixed byte
// slices into one or more dense Dynamic Slots.  Every route observes the same
// exact Update/Clear/Retain sets.  Runtime may copy bytes but may not transform
// or infer them.
struct DynamicContractV1 final
{
    std::uint32_t schemaVersion = 5;
    std::uint32_t universeCount = 0;
    DynamicExecutionModeV1 executionMode = DynamicExecutionModeV1::AuthorityOnly;
    std::uint32_t canonicalMemberBytes = 0;
    std::vector<DynamicExecutionRouteV1> executionRoutes;
    std::vector<ConditionalRegionV1> conditionalRegions;
    VerifiedIndirectDispatchContractV1 indirectDispatch;
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
    std::vector<ConditionalRegionV1> conditionalRegions = {},
    VerifiedIndirectDispatchContractV1 indirectDispatch = {})
{
    return {5, universeCount, DynamicExecutionModeV1::AuthorityOnly,
        0, {}, std::move(conditionalRegions), indirectDispatch};
}

[[nodiscard]] inline DynamicContractV1 MakeVerifiedDenseSlotDynamicContractV1(
    std::uint32_t universeCount,
    LeafPackageId targetLeaf,
    std::uint32_t targetDynamicSlot,
    std::uint32_t memberBytes,
    std::vector<ConditionalRegionV1> conditionalRegions = {},
    VerifiedIndirectDispatchContractV1 indirectDispatch = {})
{
    std::vector<DynamicExecutionRouteV1> routes;
    routes.push_back(MakeDynamicExecutionRouteV1(
        targetLeaf, targetDynamicSlot, 0, memberBytes));
    return {5, universeCount, DynamicExecutionModeV1::VerifiedDenseSlot,
        memberBytes, std::move(routes), std::move(conditionalRegions),
        indirectDispatch};
}

[[nodiscard]] inline DynamicContractV1 MakeVerifiedRoutedSlotsDynamicContractV1(
    std::uint32_t universeCount,
    std::uint32_t canonicalMemberBytes,
    std::vector<DynamicExecutionRouteV1> routes,
    std::vector<ConditionalRegionV1> conditionalRegions = {},
    VerifiedIndirectDispatchContractV1 indirectDispatch = {})
{
    return {5, universeCount, DynamicExecutionModeV1::VerifiedDenseSlot,
        canonicalMemberBytes, std::move(routes), std::move(conditionalRegions),
        indirectDispatch};
}
}
