#pragma once

#include "../00_Foundation/Base.h"
#include "../60_PgaRigidTransformSemantic/PgaRigidTransformSemantic.h"
#include "../61_Spiral1Contracts/Spiral1Contracts.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sge4_5::spiral1::representation
{
inline constexpr std::uint32_t RepresentationCandidateSchemaVersionV1 = 1;
inline constexpr std::uint32_t RepresentationTargetProfileSchemaVersionV1 = 1;
inline constexpr std::uint32_t RepresentationLoweringVersionV1 = 1;
inline constexpr std::uint32_t CanonicalThreadGroupSizeXV1 = 64;
inline constexpr std::uint32_t MatrixConstantPayloadBytesV1 = 64;
inline constexpr std::uint32_t DirectPgaConstantPayloadBytesV1 = 48;

enum class LoweringKindV1 : std::uint32_t
{
    MatrixExpandedCompute = 1,
    DirectPgaMotorCompute = 2
};

struct CanonicalProgramTemplateContractV1 final
{
    LoweringKindV1 loweringKind = LoweringKindV1::MatrixExpandedCompute;
    std::uint32_t templateVersion = 1;
    base::Digest256 templateIdentity{};
    base::Digest256 sourceDigest{};
    base::Digest256 compileRecipeDigest{};
    base::Digest256 reflectionInterfaceDigest{};
    base::Digest256 bindingLayoutDigest{};
};

[[nodiscard]] std::string_view CanonicalProgramTemplateSourceV1(LoweringKindV1 kind) noexcept;
[[nodiscard]] CanonicalProgramTemplateContractV1 CanonicalProgramTemplateContractForV1(LoweringKindV1 kind);

struct RepresentationTargetProfileV1 final
{
    std::uint32_t schemaVersion = RepresentationTargetProfileSchemaVersionV1;
    base::Digest256 compilerFingerprint{};
    base::Digest256 matrixProgramBinaryDigest{};
    base::Digest256 directPgaProgramBinaryDigest{};
    base::Digest256 identity{};
};

[[nodiscard]] base::Result<RepresentationTargetProfileV1, std::string> BuildRepresentationTargetProfileV1(
    const base::Digest256& compilerFingerprint,
    const base::Digest256& matrixProgramBinaryDigest,
    const base::Digest256& directPgaProgramBinaryDigest);
[[nodiscard]] std::vector<std::byte> SerializeRepresentationTargetProfileV1(const RepresentationTargetProfileV1& value);
[[nodiscard]] base::Digest256 ComputeRepresentationTargetProfileIdentityV1(const RepresentationTargetProfileV1& value);
[[nodiscard]] base::Result<void, std::string> ValidateRepresentationTargetProfileV1(const RepresentationTargetProfileV1& value);
[[nodiscard]] base::Digest256 ProgramBinaryDigestForV1(const RepresentationTargetProfileV1& value, LoweringKindV1 kind) noexcept;

struct RawRepresentationCandidateV1 final
{
    std::uint32_t schemaVersion = RepresentationCandidateSchemaVersionV1;
    LoweringKindV1 loweringKind = LoweringKindV1::MatrixExpandedCompute;
    std::uint32_t loweringVersion = RepresentationLoweringVersionV1;
    base::Digest256 semanticIdentity{};
    base::Digest256 targetProfileIdentity{};
    base::Digest256 templateIdentity{};
    base::Digest256 sourceDigest{};
    base::Digest256 compileRecipeDigest{};
    base::Digest256 programBinaryDigest{};
    base::Digest256 reflectionInterfaceDigest{};
    base::Digest256 bindingLayoutDigest{};
    base::Digest256 inputSchemaIdentity{};
    base::Digest256 outputSchemaIdentity{};
    base::Digest256 observationContractIdentity{};
    std::uint32_t pointCount = 0;
    std::uint32_t inputStrideBytes = 0;
    std::uint32_t outputStrideBytes = 0;
    std::uint32_t threadGroupSizeX = 0;
    std::uint32_t dispatchX = 0;
    std::vector<std::byte> constantPayload;
    base::Digest256 constantPayloadDigest{};
    base::Digest256 candidateIdentity{};
};

[[nodiscard]] std::vector<std::byte> SerializeRawRepresentationCandidateIdentityPreimageV1(const RawRepresentationCandidateV1& value);
[[nodiscard]] base::Digest256 ComputeRawRepresentationCandidateIdentityV1(const RawRepresentationCandidateV1& value);
[[nodiscard]] std::vector<std::byte> SerializeRawRepresentationCandidateV1(const RawRepresentationCandidateV1& value);
[[nodiscard]] base::Result<void, std::string> ValidateRawRepresentationCandidateStructureV1(const RawRepresentationCandidateV1& value);
}
