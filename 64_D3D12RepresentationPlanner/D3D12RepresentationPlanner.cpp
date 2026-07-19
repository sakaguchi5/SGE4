#include "D3D12RepresentationPlanner.h"

#include <bit>
#include <cmath>

namespace sge4_5::spiral1::representation
{
namespace
{
[[nodiscard]] float CanonicalFloat(double value) noexcept
{
    const float result = static_cast<float>(value);
    return result == 0.0f ? 0.0f : result;
}

void WriteF32(base::BinaryWriter& writer, double value)
{
    writer.WriteU32(std::bit_cast<std::uint32_t>(CanonicalFloat(value)));
}

[[nodiscard]] std::array<double, 3> ExtractTranslation(const semantic::CanonicalPgaMotorV1& motor) noexcept
{
    const auto& r = motor.Qr();
    const auto& d = motor.Qd();
    return {
        2.0 * (-d[0] * r[1] + r[0] * d[1] - (d[2] * r[3] - d[3] * r[2])),
        2.0 * (-d[0] * r[2] + r[0] * d[2] - (d[3] * r[1] - d[1] * r[3])),
        2.0 * (-d[0] * r[3] + r[0] * d[3] - (d[1] * r[2] - d[2] * r[1]))
    };
}

[[nodiscard]] std::vector<std::byte> BuildMatrixPayload(
    const semantic::CanonicalPgaMotorV1& motor,
    std::uint32_t pointCount)
{
    const auto& q = motor.Qr();
    const double w=q[0], x=q[1], y=q[2], z=q[3];
    const auto t = ExtractTranslation(motor);
    const std::array<double,12> m{
        1.0-2.0*(y*y+z*z), 2.0*(x*y-z*w), 2.0*(x*z+y*w), t[0],
        2.0*(x*y+z*w), 1.0-2.0*(x*x+z*z), 2.0*(y*z-x*w), t[1],
        2.0*(x*z-y*w), 2.0*(y*z+x*w), 1.0-2.0*(x*x+y*y), t[2]
    };
    base::BinaryWriter writer;
    for (double value : m) WriteF32(writer,value);
    writer.WriteU32(pointCount);
    writer.WriteZeroes(12);
    return std::move(writer).Take();
}

[[nodiscard]] std::vector<std::byte> BuildDirectPayload(
    const semantic::CanonicalPgaMotorV1& motor,
    std::uint32_t pointCount)
{
    base::BinaryWriter writer;
    for (double value : motor.Qr()) WriteF32(writer,value);
    for (double value : motor.Qd()) WriteF32(writer,value);
    writer.WriteU32(pointCount);
    writer.WriteZeroes(12);
    return std::move(writer).Take();
}
}

base::Result<RawRepresentationCandidateV1, std::string> PlanRepresentationCandidateV1(
    const semantic::ApplyPgaMotorSemanticV1& semanticValue,
    const RepresentationTargetProfileV1& targetProfile,
    LoweringKindV1 loweringKind)
{
    auto profileValidation = ValidateRepresentationTargetProfileV1(targetProfile);
    if (!profileValidation) return base::Result<RawRepresentationCandidateV1, std::string>::Failure(profileValidation.Error());
    if (semanticValue.pointCount == 0 || semanticValue.pointCount > semantic::MaximumQualificationPointCountV1)
        return base::Result<RawRepresentationCandidateV1, std::string>::Failure("semantic point count is invalid");
    if (loweringKind != LoweringKindV1::MatrixExpandedCompute && loweringKind != LoweringKindV1::DirectPgaMotorCompute)
        return base::Result<RawRepresentationCandidateV1, std::string>::Failure("unsupported lowering kind");

    const auto contract = CanonicalProgramTemplateContractForV1(loweringKind);
    RawRepresentationCandidateV1 output;
    output.loweringKind = loweringKind;
    output.semanticIdentity = semantic::ApplyPgaMotorSemanticIdentityV1(semanticValue);
    output.targetProfileIdentity = targetProfile.identity;
    output.templateIdentity = contract.templateIdentity;
    output.sourceDigest = contract.sourceDigest;
    output.compileRecipeDigest = contract.compileRecipeDigest;
    output.programBinaryDigest = ProgramBinaryDigestForV1(targetProfile,loweringKind);
    output.reflectionInterfaceDigest = contract.reflectionInterfaceDigest;
    output.bindingLayoutDigest = contract.bindingLayoutDigest;
    output.inputSchemaIdentity = semantic::Float4PointSchemaIdentityV1();
    output.outputSchemaIdentity = semantic::Float4PointOutputSchemaIdentityV1();
    output.observationContractIdentity = semanticValue.observationContractIdentity;
    output.pointCount = semanticValue.pointCount;
    output.inputStrideBytes = semantic::Float4PointStrideBytesV1;
    output.outputStrideBytes = semantic::Float4PointStrideBytesV1;
    output.threadGroupSizeX = CanonicalThreadGroupSizeXV1;
    output.dispatchX = (semanticValue.pointCount + 63u) / 64u;
    output.constantPayload = loweringKind == LoweringKindV1::MatrixExpandedCompute
        ? BuildMatrixPayload(semanticValue.motor,semanticValue.pointCount)
        : BuildDirectPayload(semanticValue.motor,semanticValue.pointCount);
    output.constantPayloadDigest = base::Sha256(output.constantPayload);
    output.candidateIdentity = ComputeRawRepresentationCandidateIdentityV1(output);
    auto structural = ValidateRawRepresentationCandidateStructureV1(output);
    if (!structural) return base::Result<RawRepresentationCandidateV1, std::string>::Failure(structural.Error());
    return base::Result<RawRepresentationCandidateV1, std::string>::Success(std::move(output));
}

base::Result<std::array<RawRepresentationCandidateV1, 2>, std::string> PlanAllRepresentationCandidatesV1(
    const semantic::ApplyPgaMotorSemanticV1& semanticValue,
    const RepresentationTargetProfileV1& targetProfile)
{
    auto matrix = PlanRepresentationCandidateV1(semanticValue,targetProfile,LoweringKindV1::MatrixExpandedCompute);
    if (!matrix) return base::Result<std::array<RawRepresentationCandidateV1,2>,std::string>::Failure(matrix.Error());
    auto direct = PlanRepresentationCandidateV1(semanticValue,targetProfile,LoweringKindV1::DirectPgaMotorCompute);
    if (!direct) return base::Result<std::array<RawRepresentationCandidateV1,2>,std::string>::Failure(direct.Error());
    std::array<RawRepresentationCandidateV1,2> output{std::move(matrix).Value(),std::move(direct).Value()};
    return base::Result<std::array<RawRepresentationCandidateV1,2>,std::string>::Success(std::move(output));
}
}
