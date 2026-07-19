#include "D3D12RepresentationCandidate.h"

#include <algorithm>
#include <array>

namespace sge4_5::spiral1::representation
{
namespace
{
constexpr std::string_view MatrixSource = R"hlsl(cbuffer TransformConstants : register(b0)
{
    float4 row0;
    float4 row1;
    float4 row2;
    uint pointCount;
    uint3 reserved;
};
StructuredBuffer<float4> InputPoints : register(t0);
RWStructuredBuffer<float4> OutputPoints : register(u0);
[numthreads(64,1,1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint index = dispatchThreadId.x;
    if (index >= pointCount) return;
    const float3 p = InputPoints[index].xyz;
    const float3 transformed = float3(
        dot(row0.xyz, p) + row0.w,
        dot(row1.xyz, p) + row1.w,
        dot(row2.xyz, p) + row2.w);
    OutputPoints[index] = float4(transformed, 1.0f);
}
)hlsl";

constexpr std::string_view DirectPgaSource = R"hlsl(cbuffer MotorConstants : register(b0)
{
    float4 qr;
    float4 qd;
    uint pointCount;
    uint3 reserved;
};
StructuredBuffer<float4> InputPoints : register(t0);
RWStructuredBuffer<float4> OutputPoints : register(u0);
[numthreads(64,1,1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint index = dispatchThreadId.x;
    if (index >= pointCount) return;
    const float3 p = InputPoints[index].xyz;
    const float3 qv = qr.yzw;
    const float3 rotated = p + 2.0f * cross(qv, cross(qv, p) + qr.x * p);
    const float3 qdv = qd.yzw;
    const float3 translation = 2.0f * (-qd.x * qv + qr.x * qdv - cross(qdv, qv));
    OutputPoints[index] = float4(rotated + translation, 1.0f);
}
)hlsl";

[[nodiscard]] bool IsZeroDigest(const base::Digest256& value) noexcept
{
    return std::all_of(value.begin(), value.end(), [](std::byte b) { return b == std::byte{0}; });
}

void WriteDigest(base::BinaryWriter& writer, const base::Digest256& value)
{
    writer.WriteBytes(value);
}

[[nodiscard]] base::Digest256 LiteralDigest(std::string_view value)
{
    return base::Sha256(std::as_bytes(std::span(value.data(), value.size())));
}

[[nodiscard]] std::uint32_t ReadU32At(std::span<const std::byte> bytes, std::size_t offset) noexcept
{
    return static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset])) |
        (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 1])) << 8) |
        (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 2])) << 16) |
        (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 3])) << 24);
}

[[nodiscard]] bool ReservedBytesAreZero(const RawRepresentationCandidateV1& value) noexcept
{
    const std::size_t first = value.loweringKind == LoweringKindV1::MatrixExpandedCompute ? 52u : 36u;
    if (first > value.constantPayload.size()) return false;
    return std::all_of(value.constantPayload.begin() + static_cast<std::ptrdiff_t>(first), value.constantPayload.end(),
        [](std::byte b) { return b == std::byte{0}; });
}
}

std::string_view CanonicalProgramTemplateSourceV1(LoweringKindV1 kind) noexcept
{
    if (kind == LoweringKindV1::MatrixExpandedCompute) return MatrixSource;
    if (kind == LoweringKindV1::DirectPgaMotorCompute) return DirectPgaSource;
    return {};
}

CanonicalProgramTemplateContractV1 CanonicalProgramTemplateContractForV1(LoweringKindV1 kind)
{
    CanonicalProgramTemplateContractV1 output;
    output.loweringKind = kind;
    const auto source = CanonicalProgramTemplateSourceV1(kind);
    output.sourceDigest = base::Sha256(std::as_bytes(std::span(source.data(), source.size())));
    output.compileRecipeDigest = LiteralDigest("D3DCompile|cs_5_1|CSMain|STRICT|WARNINGS_AS_ERRORS|IEEE_STRICT|O3|V1");
    if (kind == LoweringKindV1::MatrixExpandedCompute)
    {
        output.templateIdentity = LiteralDigest("SGE4-5.Spiral1.MatrixExpandedComputeV1");
        output.reflectionInterfaceDigest = LiteralDigest("t0:StructuredBuffer<float4>|u0:RWStructuredBuffer<float4>|b0:64|threads:64,1,1|V1");
        output.bindingLayoutDigest = LiteralDigest("SGE4-5.Spiral1.BindingLayout.t0-u0-b0.V1");
    }
    else if (kind == LoweringKindV1::DirectPgaMotorCompute)
    {
        output.templateIdentity = LiteralDigest("SGE4-5.Spiral1.DirectPgaMotorComputeV1");
        output.reflectionInterfaceDigest = LiteralDigest("t0:StructuredBuffer<float4>|u0:RWStructuredBuffer<float4>|b0:48|threads:64,1,1|V1");
        output.bindingLayoutDigest = LiteralDigest("SGE4-5.Spiral1.BindingLayout.t0-u0-b0.V1");
    }
    return output;
}

std::vector<std::byte> SerializeRepresentationTargetProfileV1(const RepresentationTargetProfileV1& value)
{
    base::BinaryWriter writer;
    writer.WriteU32(value.schemaVersion);
    writer.WriteU32(0);
    WriteDigest(writer, value.compilerFingerprint);
    WriteDigest(writer, value.matrixProgramBinaryDigest);
    WriteDigest(writer, value.directPgaProgramBinaryDigest);
    return std::move(writer).Take();
}

base::Digest256 ComputeRepresentationTargetProfileIdentityV1(const RepresentationTargetProfileV1& value)
{
    return base::Sha256(SerializeRepresentationTargetProfileV1(value));
}

base::Result<RepresentationTargetProfileV1, std::string> BuildRepresentationTargetProfileV1(
    const base::Digest256& compilerFingerprint,
    const base::Digest256& matrixProgramBinaryDigest,
    const base::Digest256& directPgaProgramBinaryDigest)
{
    if (IsZeroDigest(compilerFingerprint)) return base::Result<RepresentationTargetProfileV1, std::string>::Failure("compiler fingerprint is zero");
    if (IsZeroDigest(matrixProgramBinaryDigest) || IsZeroDigest(directPgaProgramBinaryDigest))
        return base::Result<RepresentationTargetProfileV1, std::string>::Failure("program binary digest is zero");
    RepresentationTargetProfileV1 output;
    output.compilerFingerprint = compilerFingerprint;
    output.matrixProgramBinaryDigest = matrixProgramBinaryDigest;
    output.directPgaProgramBinaryDigest = directPgaProgramBinaryDigest;
    output.identity = ComputeRepresentationTargetProfileIdentityV1(output);
    return base::Result<RepresentationTargetProfileV1, std::string>::Success(std::move(output));
}

base::Result<void, std::string> ValidateRepresentationTargetProfileV1(const RepresentationTargetProfileV1& value)
{
    if (value.schemaVersion != RepresentationTargetProfileSchemaVersionV1)
        return base::Result<void, std::string>::Failure("unsupported representation target profile schema");
    if (IsZeroDigest(value.compilerFingerprint) || IsZeroDigest(value.matrixProgramBinaryDigest) || IsZeroDigest(value.directPgaProgramBinaryDigest))
        return base::Result<void, std::string>::Failure("representation target profile contains a zero digest");
    if (value.identity != ComputeRepresentationTargetProfileIdentityV1(value))
        return base::Result<void, std::string>::Failure("representation target profile identity mismatch");
    return base::Result<void, std::string>::Success();
}

base::Digest256 ProgramBinaryDigestForV1(const RepresentationTargetProfileV1& value, LoweringKindV1 kind) noexcept
{
    return kind == LoweringKindV1::MatrixExpandedCompute ? value.matrixProgramBinaryDigest : value.directPgaProgramBinaryDigest;
}

std::vector<std::byte> SerializeRawRepresentationCandidateIdentityPreimageV1(const RawRepresentationCandidateV1& value)
{
    base::BinaryWriter writer;
    writer.WriteU32(value.schemaVersion);
    writer.WriteU32(static_cast<std::uint32_t>(value.loweringKind));
    writer.WriteU32(value.loweringVersion);
    writer.WriteU32(0);
    WriteDigest(writer, value.semanticIdentity);
    WriteDigest(writer, value.targetProfileIdentity);
    WriteDigest(writer, value.templateIdentity);
    WriteDigest(writer, value.sourceDigest);
    WriteDigest(writer, value.compileRecipeDigest);
    WriteDigest(writer, value.programBinaryDigest);
    WriteDigest(writer, value.reflectionInterfaceDigest);
    WriteDigest(writer, value.bindingLayoutDigest);
    WriteDigest(writer, value.inputSchemaIdentity);
    WriteDigest(writer, value.outputSchemaIdentity);
    WriteDigest(writer, value.observationContractIdentity);
    writer.WriteU32(value.pointCount);
    writer.WriteU32(value.inputStrideBytes);
    writer.WriteU32(value.outputStrideBytes);
    writer.WriteU32(value.threadGroupSizeX);
    writer.WriteU32(value.dispatchX);
    writer.WriteU32(static_cast<std::uint32_t>(value.constantPayload.size()));
    WriteDigest(writer, value.constantPayloadDigest);
    writer.WriteBytes(value.constantPayload);
    return std::move(writer).Take();
}

base::Digest256 ComputeRawRepresentationCandidateIdentityV1(const RawRepresentationCandidateV1& value)
{
    return base::Sha256(SerializeRawRepresentationCandidateIdentityPreimageV1(value));
}

std::vector<std::byte> SerializeRawRepresentationCandidateV1(const RawRepresentationCandidateV1& value)
{
    auto bytes = SerializeRawRepresentationCandidateIdentityPreimageV1(value);
    bytes.insert(bytes.end(), value.candidateIdentity.begin(), value.candidateIdentity.end());
    return bytes;
}

base::Result<void, std::string> ValidateRawRepresentationCandidateStructureV1(const RawRepresentationCandidateV1& value)
{
    if (value.schemaVersion != RepresentationCandidateSchemaVersionV1 || value.loweringVersion != RepresentationLoweringVersionV1)
        return base::Result<void, std::string>::Failure("unsupported raw candidate version");
    if (value.loweringKind != LoweringKindV1::MatrixExpandedCompute && value.loweringKind != LoweringKindV1::DirectPgaMotorCompute)
        return base::Result<void, std::string>::Failure("unknown lowering kind");
    const std::array<const base::Digest256*, 12> digests{
        &value.semanticIdentity,&value.targetProfileIdentity,&value.templateIdentity,&value.sourceDigest,&value.compileRecipeDigest,
        &value.programBinaryDigest,&value.reflectionInterfaceDigest,&value.bindingLayoutDigest,&value.inputSchemaIdentity,
        &value.outputSchemaIdentity,&value.observationContractIdentity,&value.constantPayloadDigest};
    for (const auto* digest : digests) if (IsZeroDigest(*digest)) return base::Result<void, std::string>::Failure("raw candidate contains a zero digest");
    if (value.pointCount == 0 || value.pointCount > semantic::MaximumQualificationPointCountV1)
        return base::Result<void, std::string>::Failure("raw candidate point count is invalid");
    if (value.inputStrideBytes != semantic::Float4PointStrideBytesV1 || value.outputStrideBytes != semantic::Float4PointStrideBytesV1)
        return base::Result<void, std::string>::Failure("raw candidate stride is invalid");
    if (value.threadGroupSizeX != CanonicalThreadGroupSizeXV1 || value.dispatchX != (value.pointCount + 63u) / 64u)
        return base::Result<void, std::string>::Failure("raw candidate dispatch contract is invalid");
    const std::size_t expectedBytes = value.loweringKind == LoweringKindV1::MatrixExpandedCompute ? MatrixConstantPayloadBytesV1 : DirectPgaConstantPayloadBytesV1;
    const std::size_t pointCountOffset = value.loweringKind == LoweringKindV1::MatrixExpandedCompute ? 48u : 32u;
    if (value.constantPayload.size() != expectedBytes) return base::Result<void, std::string>::Failure("raw candidate constant payload size is invalid");
    if (ReadU32At(value.constantPayload, pointCountOffset) != value.pointCount) return base::Result<void, std::string>::Failure("constant payload point count mismatch");
    if (!ReservedBytesAreZero(value)) return base::Result<void, std::string>::Failure("constant payload reserved bytes are nonzero");
    if (base::Sha256(value.constantPayload) != value.constantPayloadDigest) return base::Result<void, std::string>::Failure("constant payload digest mismatch");
    if (ComputeRawRepresentationCandidateIdentityV1(value) != value.candidateIdentity) return base::Result<void, std::string>::Failure("raw candidate identity mismatch");
    return base::Result<void, std::string>::Success();
}
}
