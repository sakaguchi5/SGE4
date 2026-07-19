#pragma once

#include "../00_Foundation/Base.h"
#include "../02_SemanticModel/SemanticModel.h"
#include "../05_TargetContract/TargetModel.h"
#include "../65_D3D12RepresentationVerifier/D3D12RepresentationVerifier.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace sge4_5::spiral1::leaf
{
inline constexpr std::uint32_t FrozenRepresentationLeafSchemaVersionV1 = 1;

enum class LeafCompileErrorCodeV1 : std::uint32_t
{
    NativeCompilerUnavailable,
    NativeProgramCompilationFailed,
    NativeCompilerFingerprintFailed,
    VerifiedPlanContextMismatch,
    SemanticGraphConstructionFailed,
    ExecutionPlanCompilationFailed,
    PreliminaryPackageInvalid,
    ProgramBinaryAuthorityMismatch,
    FrozenPackageRebuildFailed,
    FrozenPackageQualificationFailed
};

struct LeafCompileErrorV1 final
{
    LeafCompileErrorCodeV1 code = LeafCompileErrorCodeV1::SemanticGraphConstructionFailed;
    std::string stage;
    std::string message;
};

struct NativeProgramSetV1 final
{
    representation::RepresentationTargetProfileV1 representationTargetProfile;
    std::vector<std::byte> matrixProgramBinary;
    std::vector<std::byte> directPgaProgramBinary;
};

struct FrozenRepresentationLeafV1 final
{
    std::uint32_t schemaVersion = FrozenRepresentationLeafSchemaVersionV1;
    representation::LoweringKindV1 loweringKind = representation::LoweringKindV1::MatrixExpandedCompute;
    base::Digest256 semanticIdentity{};
    base::Digest256 candidateIdentity{};
    base::Digest256 representationSeal{};
    base::Digest256 packageExecutionDigest{};
    base::Digest256 packageFileDigest{};
    std::uint32_t lockedDynamicSlot = base::InvalidIndex;
    std::vector<std::byte> lockedConstantPayload;
    std::vector<std::byte> verifiedRepresentationPlanBytes;
    std::vector<std::byte> packageBytes;
};

[[nodiscard]] target::D3D12TargetProfile CanonicalLeafD3D12TargetProfileV1();
[[nodiscard]] base::Result<NativeProgramSetV1, LeafCompileErrorV1> BuildNativeProgramSetV1();
[[nodiscard]] base::Result<::sge4_5::semantic::SemanticGraph, LeafCompileErrorV1> BuildVerifiedRepresentationLeafGraphV1(
    const semantic::ApplyPgaMotorSemanticV1& semanticValue,
    const representation::RepresentationTargetProfileV1& representationTargetProfile,
    const representation::VerifiedRepresentationPlanV1& verifiedPlan);
[[nodiscard]] base::Result<FrozenRepresentationLeafV1, LeafCompileErrorV1> CompileVerifiedRepresentationLeafV1(
    const semantic::ApplyPgaMotorSemanticV1& semanticValue,
    const NativeProgramSetV1& nativePrograms,
    const target::D3D12TargetProfile& d3d12TargetProfile,
    const representation::VerifiedRepresentationPlanV1& verifiedPlan);
[[nodiscard]] base::Result<void, LeafCompileErrorV1> ValidateFrozenRepresentationLeafV1(
    const FrozenRepresentationLeafV1& value,
    const semantic::ApplyPgaMotorSemanticV1& semanticValue,
    const NativeProgramSetV1& nativePrograms,
    const representation::VerifiedRepresentationPlanV1& verifiedPlan);
[[nodiscard]] std::vector<std::byte> SerializeFrozenRepresentationLeafEvidenceV1(
    const FrozenRepresentationLeafV1& value);
}
