#include "D3D12RepresentationVerifier.h"

#include <bit>
#include <cmath>

namespace sge4_5::spiral1::representation
{
namespace
{
[[nodiscard]] RepresentationVerificationErrorV1 Error(RepresentationVerificationErrorCodeV1 code, std::string message)
{
    return {code,std::move(message)};
}

[[nodiscard]] float CanonicalFloat(double value) noexcept
{
    const float output = static_cast<float>(value);
    return output == 0.0f ? 0.0f : output;
}

void WriteFloat(base::BinaryWriter& writer, double value)
{
    writer.WriteU32(std::bit_cast<std::uint32_t>(CanonicalFloat(value)));
}

[[nodiscard]] std::array<double,3> IndependentlyExtractTranslation(const semantic::CanonicalPgaMotorV1& motor) noexcept
{
    const auto& a=motor.Qd(); const auto& b=motor.Qr();
    const double vx = -a[0]*b[1] + b[0]*a[1] - a[2]*b[3] + a[3]*b[2];
    const double vy = -a[0]*b[2] + b[0]*a[2] - a[3]*b[1] + a[1]*b[3];
    const double vz = -a[0]*b[3] + b[0]*a[3] - a[1]*b[2] + a[2]*b[1];
    return {2.0*vx,2.0*vy,2.0*vz};
}

[[nodiscard]] std::vector<std::byte> IndependentlyDeriveConstants(
    const semantic::ApplyPgaMotorSemanticV1& semanticValue,
    LoweringKindV1 kind)
{
    base::BinaryWriter writer;
    if (kind == LoweringKindV1::MatrixExpandedCompute)
    {
        const auto& r=semanticValue.motor.Qr();
        const double w=r[0],x=r[1],y=r[2],z=r[3];
        const auto t=IndependentlyExtractTranslation(semanticValue.motor);
        WriteFloat(writer,1.0-2.0*y*y-2.0*z*z); WriteFloat(writer,2.0*x*y-2.0*z*w); WriteFloat(writer,2.0*x*z+2.0*y*w); WriteFloat(writer,t[0]);
        WriteFloat(writer,2.0*x*y+2.0*z*w); WriteFloat(writer,1.0-2.0*x*x-2.0*z*z); WriteFloat(writer,2.0*y*z-2.0*x*w); WriteFloat(writer,t[1]);
        WriteFloat(writer,2.0*x*z-2.0*y*w); WriteFloat(writer,2.0*y*z+2.0*x*w); WriteFloat(writer,1.0-2.0*x*x-2.0*y*y); WriteFloat(writer,t[2]);
    }
    else
    {
        for (double value : semanticValue.motor.Qr()) WriteFloat(writer,value);
        for (double value : semanticValue.motor.Qd()) WriteFloat(writer,value);
    }
    writer.WriteU32(semanticValue.pointCount);
    writer.WriteZeroes(12);
    return std::move(writer).Take();
}

[[nodiscard]] base::Digest256 ComputeSeal(
    const RawRepresentationCandidateV1& candidate,
    const base::Digest256& verifierSchema)
{
    base::BinaryWriter writer;
    constexpr std::string_view domain="SGE4-5.Spiral1.VerifiedRepresentationSeal.V1";
    writer.WriteU32(static_cast<std::uint32_t>(domain.size()));
    writer.WriteBytes(std::as_bytes(std::span(domain.data(),domain.size())));
    writer.WriteBytes(candidate.semanticIdentity);
    writer.WriteBytes(candidate.candidateIdentity);
    writer.WriteBytes(candidate.targetProfileIdentity);
    writer.WriteBytes(candidate.observationContractIdentity);
    writer.WriteBytes(verifierSchema);
    return base::Sha256(std::move(writer).Take());
}
}

base::Digest256 RepresentationVerifierSchemaIdentityV1()
{
    constexpr std::string_view value="SGE4-5.Spiral1.IndependentRepresentationVerifier.V1";
    return base::Sha256(std::as_bytes(std::span(value.data(),value.size())));
}

base::Result<VerifiedRepresentationPlanV1, RepresentationVerificationErrorV1> IndependentRepresentationVerifierV1::Verify(
    const semantic::ApplyPgaMotorSemanticV1& semanticValue,
    const RepresentationTargetProfileV1& targetProfile,
    const RawRepresentationCandidateV1& candidate)
{
    auto profile=ValidateRepresentationTargetProfileV1(targetProfile);
    if (!profile) return base::Result<VerifiedRepresentationPlanV1,RepresentationVerificationErrorV1>::Failure(Error(RepresentationVerificationErrorCodeV1::InvalidTargetProfile,profile.Error()));
    auto structure=ValidateRawRepresentationCandidateStructureV1(candidate);
    if (!structure) return base::Result<VerifiedRepresentationPlanV1,RepresentationVerificationErrorV1>::Failure(Error(RepresentationVerificationErrorCodeV1::InvalidRawStructure,structure.Error()));
    const auto semanticIdentity=semantic::ApplyPgaMotorSemanticIdentityV1(semanticValue);
    if (candidate.semanticIdentity != semanticIdentity)
        return base::Result<VerifiedRepresentationPlanV1,RepresentationVerificationErrorV1>::Failure(Error(RepresentationVerificationErrorCodeV1::SemanticIdentityMismatch,"semantic identity mismatch"));
    if (candidate.targetProfileIdentity != targetProfile.identity)
        return base::Result<VerifiedRepresentationPlanV1,RepresentationVerificationErrorV1>::Failure(Error(RepresentationVerificationErrorCodeV1::TargetProfileMismatch,"target profile identity mismatch"));
    const auto contract=CanonicalProgramTemplateContractForV1(candidate.loweringKind);
    if (candidate.templateIdentity!=contract.templateIdentity || candidate.sourceDigest!=contract.sourceDigest || candidate.compileRecipeDigest!=contract.compileRecipeDigest ||
        candidate.reflectionInterfaceDigest!=contract.reflectionInterfaceDigest || candidate.bindingLayoutDigest!=contract.bindingLayoutDigest)
        return base::Result<VerifiedRepresentationPlanV1,RepresentationVerificationErrorV1>::Failure(Error(RepresentationVerificationErrorCodeV1::TemplateContractMismatch,"canonical program template contract mismatch"));
    if (candidate.programBinaryDigest != ProgramBinaryDigestForV1(targetProfile,candidate.loweringKind))
        return base::Result<VerifiedRepresentationPlanV1,RepresentationVerificationErrorV1>::Failure(Error(RepresentationVerificationErrorCodeV1::ProgramBinaryMismatch,"program binary digest mismatch"));
    if (candidate.inputSchemaIdentity!=semantic::Float4PointSchemaIdentityV1() || candidate.outputSchemaIdentity!=semantic::Float4PointOutputSchemaIdentityV1() ||
        candidate.inputStrideBytes!=semantic::Float4PointStrideBytesV1 || candidate.outputStrideBytes!=semantic::Float4PointStrideBytesV1)
        return base::Result<VerifiedRepresentationPlanV1,RepresentationVerificationErrorV1>::Failure(Error(RepresentationVerificationErrorCodeV1::SchemaMismatch,"input/output schema mismatch"));
    if (candidate.observationContractIdentity!=semanticValue.observationContractIdentity || candidate.observationContractIdentity!=contracts::ObservationContractIdentityV2())
        return base::Result<VerifiedRepresentationPlanV1,RepresentationVerificationErrorV1>::Failure(Error(RepresentationVerificationErrorCodeV1::ObservationContractMismatch,"observation contract mismatch"));
    if (candidate.pointCount!=semanticValue.pointCount)
        return base::Result<VerifiedRepresentationPlanV1,RepresentationVerificationErrorV1>::Failure(Error(RepresentationVerificationErrorCodeV1::PointCountMismatch,"point count mismatch"));
    if (candidate.threadGroupSizeX!=CanonicalThreadGroupSizeXV1 || candidate.dispatchX!=(semanticValue.pointCount+63u)/64u)
        return base::Result<VerifiedRepresentationPlanV1,RepresentationVerificationErrorV1>::Failure(Error(RepresentationVerificationErrorCodeV1::DispatchMismatch,"dispatch mismatch"));
    const auto expected=IndependentlyDeriveConstants(semanticValue,candidate.loweringKind);
    if (candidate.constantPayload!=expected || candidate.constantPayloadDigest!=base::Sha256(expected))
        return base::Result<VerifiedRepresentationPlanV1,RepresentationVerificationErrorV1>::Failure(Error(RepresentationVerificationErrorCodeV1::ConstantPayloadMismatch,"independently derived constant payload mismatch"));
    if (candidate.candidateIdentity!=ComputeRawRepresentationCandidateIdentityV1(candidate))
        return base::Result<VerifiedRepresentationPlanV1,RepresentationVerificationErrorV1>::Failure(Error(RepresentationVerificationErrorCodeV1::CandidateIdentityMismatch,"candidate identity mismatch"));
    const auto schema=RepresentationVerifierSchemaIdentityV1();
    auto plan=VerifiedRepresentationPlanV1(candidate,schema,ComputeSeal(candidate,schema));
    return base::Result<VerifiedRepresentationPlanV1,RepresentationVerificationErrorV1>::Success(std::move(plan));
}

base::Result<void, RepresentationVerificationErrorV1> IndependentRepresentationVerifierV1::ValidatePlanContext(
    const VerifiedRepresentationPlanV1& plan,
    const semantic::ApplyPgaMotorSemanticV1& semanticValue,
    const RepresentationTargetProfileV1& targetProfile)
{
    if (plan.VerifierSchemaIdentity()!=RepresentationVerifierSchemaIdentityV1())
        return base::Result<void,RepresentationVerificationErrorV1>::Failure(Error(RepresentationVerificationErrorCodeV1::VerifiedPlanContextMismatch,"verifier schema identity mismatch"));
    if (plan.Candidate().semanticIdentity!=semantic::ApplyPgaMotorSemanticIdentityV1(semanticValue) || plan.Candidate().targetProfileIdentity!=targetProfile.identity ||
        plan.Candidate().observationContractIdentity!=semanticValue.observationContractIdentity)
        return base::Result<void,RepresentationVerificationErrorV1>::Failure(Error(RepresentationVerificationErrorCodeV1::VerifiedPlanContextMismatch,"verified plan context mismatch"));
    if (plan.SealDigest()!=ComputeSeal(plan.Candidate(),plan.VerifierSchemaIdentity()))
        return base::Result<void,RepresentationVerificationErrorV1>::Failure(Error(RepresentationVerificationErrorCodeV1::VerifiedPlanSealMismatch,"verified plan seal mismatch"));
    return base::Result<void,RepresentationVerificationErrorV1>::Success();
}

std::vector<std::byte> SerializeVerifiedRepresentationPlanV1(const VerifiedRepresentationPlanV1& value)
{
    base::BinaryWriter writer;
    writer.WriteU32(1); writer.WriteU32(0);
    writer.WriteBytes(value.VerifierSchemaIdentity());
    writer.WriteBytes(value.SealDigest());
    const auto candidateBytes=SerializeRawRepresentationCandidateV1(value.Candidate());
    writer.WriteU32(static_cast<std::uint32_t>(candidateBytes.size()));
    writer.WriteU32(0);
    writer.WriteBytes(candidateBytes);
    return std::move(writer).Take();
}
}
