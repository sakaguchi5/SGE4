#pragma once

#include "../00_Foundation/Base.h"
#include "../80_HierarchySemantic/HierarchySemantic.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace sge4_5::spiral2::contracts
{
inline constexpr std::uint32_t DynamicMotorConventionVersionV1 = 1;
inline constexpr std::uint32_t DynamicMotorStrideBytesV1 = 32;
inline constexpr std::uint32_t DynamicPaletteAlignmentV1 = 16;
inline constexpr double DynamicMotorCanonicalSignEpsilonV1 = 0x1p-52;
inline constexpr double ObservationAbsoluteToleranceV1 = 2.0e-4;
inline constexpr double ObservationRelativeToleranceV1 = 2.0e-5;
inline constexpr double ObservationPairwiseAbsoluteToleranceV1 = 1.5e-4;
inline constexpr double ObservationPairwiseRelativeToleranceV1 = 2.0e-5;
inline constexpr double ObservationRigidityToleranceV1 = 5.0e-4;

// V2 closes the componentwise scale semantics required by the Observation Contract.
// V1 is retained as an immutable historical contract.
inline constexpr double ObservationAbsoluteToleranceV2 = 2.5e-4;
inline constexpr double ObservationRelativeToleranceV2 = 2.0e-5;
inline constexpr double ObservationPairwiseAbsoluteToleranceV2 = 2.5e-4;
inline constexpr double ObservationPairwiseRelativeToleranceV2 = 2.0e-5;
inline constexpr double ObservationRigidityToleranceV2 = 5.0e-4;

struct RotationTranslationV1 final
{
    double qw = 1.0, qx = 0.0, qy = 0.0, qz = 0.0;
    double tx = 0.0, ty = 0.0, tz = 0.0;
};

struct DynamicMotorRecordV1 final
{
    std::array<float, 4> qr{};
    std::array<float, 4> qd{};
};
static_assert(sizeof(DynamicMotorRecordV1) == DynamicMotorStrideBytesV1);

struct DynamicLocalMotorPaletteV1 final
{
    std::uint32_t boneCount = 0;
    std::vector<DynamicMotorRecordV1> motors;
};

struct DynamicPaletteInvocationSchemaV1 final
{
    base::Digest256 hierarchySemanticIdentity{};
    base::Digest256 slotIdentity{};
    std::uint32_t boneCount = 0;
    std::uint32_t requiredBytes = 0;
    std::uint32_t alignment = DynamicPaletteAlignmentV1;
};

[[nodiscard]] base::Digest256 DynamicPaletteSlotIdentityV1();
[[nodiscard]] base::Result<DynamicLocalMotorPaletteV1, std::string> BuildDynamicLocalMotorPaletteV1(
    std::uint32_t expectedBoneCount,
    std::span<const RotationTranslationV1> values);
[[nodiscard]] base::Result<void, std::string> ValidateDynamicLocalMotorPaletteV1(
    const DynamicLocalMotorPaletteV1& palette,
    std::uint32_t expectedBoneCount);
[[nodiscard]] std::vector<std::byte> SerializeDynamicLocalMotorPaletteV1(
    const DynamicLocalMotorPaletteV1& palette);
[[nodiscard]] base::Result<DynamicLocalMotorPaletteV1, std::string> DeserializeDynamicLocalMotorPaletteV1(
    std::uint32_t expectedBoneCount,
    std::span<const std::byte> bytes);
[[nodiscard]] DynamicPaletteInvocationSchemaV1 BuildDynamicPaletteInvocationSchemaV1(
    const hierarchy::RigidHierarchySemanticV1& hierarchy);
[[nodiscard]] std::vector<std::byte> SerializeDynamicPaletteInvocationSchemaV1(
    const DynamicPaletteInvocationSchemaV1& schema);
[[nodiscard]] base::Digest256 DynamicPaletteInvocationSchemaIdentityV1(
    const DynamicPaletteInvocationSchemaV1& schema);
}
