#include "Spiral1Scenarios.h"

#include "../03_SemanticBuilder/SemanticBuilder.h"
#include "../09_FrozenPackageCore/PackageReader.h"
#include "../10_D3D12PackageSchema/D3D12Encoding.h"
#include "../12_SGE4_5Compiler/SGE4_5Compiler.h"
#include "../18_CompositionPlan/CompositionPlan.h"
#include "../19_CompositionVerifier/CompositionVerifier.h"
#include "../64_D3D12RepresentationPlanner/D3D12RepresentationPlanner.h"
#include "../65_D3D12RepresentationVerifier/D3D12RepresentationVerifier.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#include <sstream>
#include <tuple>
#include <utility>

namespace sge4_5::spiral1::scenarios
{
namespace
{
namespace sem = ::sge4_5::semantic;
namespace rep = ::sge4_5::spiral1::representation;
namespace canonical = ::sge4_5::composition::canonical;
namespace planning = ::sge4_5::composition::canonical::planning;
namespace verification = ::sge4_5::composition::canonical::verification;
namespace runtime = ::sge4_5::composition::canonical::runtime_v1;
namespace pkg = ::sge4_5::package;
namespace schema = ::sge4_5::package::d3d12_v13;

[[nodiscard]] ScenarioErrorV1 Error(std::string stage, std::string message)
{
    return {std::move(stage), std::move(message)};
}

template<class T>
[[nodiscard]] base::Result<T, ScenarioErrorV1> Failure(std::string stage, std::string message)
{
    return base::Result<T, ScenarioErrorV1>::Failure(Error(std::move(stage), std::move(message)));
}

[[nodiscard]] bool IsZeroDigest(const base::Digest256& value) noexcept
{
    return std::all_of(value.begin(), value.end(), [](std::byte byte) { return byte == std::byte{0}; });
}

void WriteDigest(base::BinaryWriter& writer, const base::Digest256& value)
{
    writer.WriteBytes(value);
}

void WriteBlob(base::BinaryWriter& writer, std::span<const std::byte> bytes)
{
    writer.WriteU64(static_cast<std::uint64_t>(bytes.size()));
    writer.WriteBytes(bytes);
}

[[nodiscard]] target::D3D12TargetProfile AuxiliaryLeafProfileV1()
{
    auto profile = leaf::CanonicalLeafD3D12TargetProfileV1();
    profile.shaderDescriptorCount = 8;
    return profile;
}

[[nodiscard]] std::string PointCountPrefix(std::uint32_t pointCount)
{
    return "static const uint PointCount = " + std::to_string(pointCount) + "u;\n";
}

[[nodiscard]] base::Result<std::vector<std::byte>, ScenarioErrorV1> CompileCorpusSourceLeafV1(
    const corpus::QualificationCaseV1& value)
{
    if (value.inputPoints.empty() || value.inputPoints.size() != value.referencePoints.size())
        return Failure<std::vector<std::byte>>(
            "source-leaf/input", "corpus input/reference point counts are invalid");

    sem::SemanticBuilder builder;
    const auto inputBytes = std::as_bytes(std::span(value.inputPoints));
    const auto referenceBytes = std::as_bytes(std::span(value.referencePoints));
    auto inputSource = builder.AddImmutableBuffer(
        "Spiral1.Source.InputInitial", sizeof(semantic::Float4Point), inputBytes);
    auto referenceSource = builder.AddImmutableBuffer(
        "Spiral1.Source.ReferenceInitial", sizeof(semantic::Float4Point), referenceBytes);
    auto inputOutput = builder.AddExternalBuffer(
        "Spiral1.Source.InputOutput", inputBytes.size(), sizeof(semantic::Float4Point));
    auto referenceOutput = builder.AddExternalBuffer(
        "Spiral1.Source.ReferenceOutput", referenceBytes.size(), sizeof(semantic::Float4Point));
    if (!inputSource || !referenceSource || !inputOutput || !referenceOutput)
        return Failure<std::vector<std::byte>>(
            "source-leaf/resources", "Corpus Source Leaf resource construction failed");

    auto useInputSource = builder.AddUse(inputSource.Value(), sem::Effect::Read, sem::ViewRole::ShaderBuffer);
    auto useReferenceSource = builder.AddUse(referenceSource.Value(), sem::Effect::Read, sem::ViewRole::ShaderBuffer);
    auto useInputOutput = builder.AddUse(inputOutput.Value(), sem::Effect::Write, sem::ViewRole::StorageBuffer);
    auto useReferenceOutput = builder.AddUse(referenceOutput.Value(), sem::Effect::Write, sem::ViewRole::StorageBuffer);
    if (!useInputSource || !useReferenceSource || !useInputOutput || !useReferenceOutput)
        return Failure<std::vector<std::byte>>(
            "source-leaf/uses", "Corpus Source Leaf ResourceUse construction failed");

    sem::ProgramInterface interfaceDescription;
    interfaceDescription.parameters = {
        {{0}, "InputInitial", sem::ProgramParameterKind::ReadOnlyBuffer, sem::ShaderStage::Compute, 0, 0, 1},
        {{1}, "ReferenceInitial", sem::ProgramParameterKind::ReadOnlyBuffer, sem::ShaderStage::Compute, 1, 0, 1},
        {{2}, "InputOutput", sem::ProgramParameterKind::UnorderedBuffer, sem::ShaderStage::Compute, 0, 0, 1},
        {{3}, "ReferenceOutput", sem::ProgramParameterKind::UnorderedBuffer, sem::ShaderStage::Compute, 1, 0, 1}};
    const std::string shader = PointCountPrefix(static_cast<std::uint32_t>(value.inputPoints.size())) + R"hlsl(
StructuredBuffer<float4> InputInitial : register(t0);
StructuredBuffer<float4> ReferenceInitial : register(t1);
RWStructuredBuffer<float4> InputOutput : register(u0);
RWStructuredBuffer<float4> ReferenceOutput : register(u1);
[numthreads(64,1,1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint index = dispatchThreadId.x;
    if (index >= PointCount) return;
    InputOutput[index] = InputInitial[index];
    ReferenceOutput[index] = ReferenceInitial[index];
}
)hlsl";
    auto program = builder.AddComputeProgram(
        "Spiral1.CorpusAndReferenceSource.V1", std::move(interfaceDescription),
        {shader, {}, {}, "CSMain"});
    if (!program)
        return Failure<std::vector<std::byte>>("source-leaf/program", program.Error());
    const std::array operands = {
        sem::WorkOperand{sem::WorkOperandKind::ProgramParameter, useInputSource.Value(), {0}},
        sem::WorkOperand{sem::WorkOperandKind::ProgramParameter, useReferenceSource.Value(), {1}},
        sem::WorkOperand{sem::WorkOperandKind::ProgramParameter, useInputOutput.Value(), {2}},
        sem::WorkOperand{sem::WorkOperandKind::ProgramParameter, useReferenceOutput.Value(), {3}}};
    const std::uint32_t dispatchX = static_cast<std::uint32_t>((value.inputPoints.size() + 63u) / 64u);
    auto work = builder.AddComputeWorkGeneric(
        "Spiral1.CorpusAndReferenceSource.Work.V1", program.Value(), operands,
        dispatchX, 1, 1);
    if (!work)
        return Failure<std::vector<std::byte>>("source-leaf/work", work.Error());

    auto compiled = compiler::CompileCanonical(std::move(builder).Build(), AuxiliaryLeafProfileV1());
    if (!compiled)
        return Failure<std::vector<std::byte>>(
            compiled.Error().stage, compiled.Error().message);
    return base::Result<std::vector<std::byte>, ScenarioErrorV1>::Success(
        std::move(compiled).Value().packageBytes);
}

[[nodiscard]] std::string ComparisonShaderV1(std::uint32_t pointCount)
{
    return PointCountPrefix(pointCount) + R"hlsl(
StructuredBuffer<float4> ReferencePoints : register(t0);
StructuredBuffer<float4> MatrixOutputPoints : register(t1);
StructuredBuffer<float4> DirectPgaOutputPoints : register(t2);
RWStructuredBuffer<uint4> ComparisonWords : register(u0);

bool Finite4(float4 value)
{
    return all(isfinite(value));
}
uint OrderedFloat(float value)
{
    const uint bits = asuint(value);
    return (bits & 0x80000000u) != 0u ? 0x80000000u - bits : 0x80000000u + bits;
}
uint UlpDistanceV1(float a, float b)
{
    uint result = 0xffffffffu;
    if (a == 0.0f && b == 0.0f)
    {
        result = 0u;
    }
    else if (isfinite(a) && isfinite(b))
    {
        const uint oa = OrderedFloat(a);
        const uint ob = OrderedFloat(b);
        if (oa >= ob)
        {
            result = oa - ob;
        }
        else
        {
            result = ob - oa;
        }
    }
    return result;
}
bool WithinReferenceV2(float actual, float reference, float referencePointScale)
{
    bool result = false;
    if (isfinite(actual) && isfinite(reference))
    {
        precise float difference = abs(actual - reference);
        const float absoluteTolerance = asfloat(0x38d1b717u);
        const float relativeTolerance = asfloat(0x3727c5acu);
        precise float magnitude = max(referencePointScale, 1.0f);
        precise float scaledTolerance = relativeTolerance * magnitude;
        precise float limit = absoluteTolerance + scaledTolerance;
        result = difference <= limit;
    }
    return result;
}
bool WithinPairV2(float matrixValue, float pgaValue, float pairPointScale)
{
    bool result = false;
    if (isfinite(matrixValue) && isfinite(pgaValue))
    {
        precise float difference = abs(matrixValue - pgaValue);
        const float pairAbsoluteTolerance = asfloat(0x38d1b717u);
        const float relativeTolerance = asfloat(0x3727c5acu);
        precise float magnitude = max(pairPointScale, 1.0f);
        precise float scaledTolerance = relativeTolerance * magnitude;
        precise float limit = pairAbsoluteTolerance + scaledTolerance;
        result = difference <= limit;
    }
    return result;
}
[numthreads(64,1,1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint index = dispatchThreadId.x;
    if (index >= PointCount) return;
    const float4 r = ReferencePoints[index];
    const float4 m = MatrixOutputPoints[index];
    const float4 p = DirectPgaOutputPoints[index];

    uint finiteFlags = 0u;
    uint mismatchFlags = 0u;
    const bool referenceFinite = Finite4(r);
    const bool matrixFinite = Finite4(m);
    const bool pgaFinite = Finite4(p);
    const bool matrixW = asuint(m.w) == 0x3f800000u;
    const bool pgaW = asuint(p.w) == 0x3f800000u;
    if (referenceFinite) finiteFlags |= 1u << 0u;
    if (matrixFinite) finiteFlags |= 1u << 1u;
    if (pgaFinite) finiteFlags |= 1u << 2u;
    if (matrixW) finiteFlags |= 1u << 3u;
    if (pgaW) finiteFlags |= 1u << 4u;
    if (!referenceFinite) mismatchFlags |= 1u << 0u;
    if (!matrixFinite) mismatchFlags |= 1u << 1u;
    if (!pgaFinite) mismatchFlags |= 1u << 2u;
    if (!matrixW) mismatchFlags |= 1u << 3u;
    if (!pgaW) mismatchFlags |= 1u << 4u;

    float3 matrixError = float3(0.0f, 0.0f, 0.0f);
    float3 pgaError = float3(0.0f, 0.0f, 0.0f);
    float3 pairError = float3(0.0f, 0.0f, 0.0f);
    uint3 matrixUlp = uint3(0u, 0u, 0u);
    uint3 pgaUlp = uint3(0u, 0u, 0u);
    uint3 pairUlp = uint3(0u, 0u, 0u);
    precise float referenceScale = max(max(abs(r.x), abs(r.y)), max(abs(r.z), 1.0f));
    precise float matrixScale = max(max(abs(m.x), abs(m.y)), max(abs(m.z), 1.0f));
    precise float pgaScale = max(max(abs(p.x), abs(p.y)), max(abs(p.z), 1.0f));
    precise float pairScale = max(matrixScale, pgaScale);
    [unroll]
    for (uint component = 0u; component < 3u; ++component)
    {
        precise float matrixDifference = m[component] - r[component];
        precise float pgaDifference = p[component] - r[component];
        precise float pairDifference = m[component] - p[component];
        matrixError[component] = abs(matrixDifference);
        pgaError[component] = abs(pgaDifference);
        pairError[component] = abs(pairDifference);
        matrixUlp[component] = UlpDistanceV1(m[component], r[component]);
        pgaUlp[component] = UlpDistanceV1(p[component], r[component]);
        pairUlp[component] = UlpDistanceV1(m[component], p[component]);
        if (!WithinReferenceV2(m[component], r[component], referenceScale)) mismatchFlags |= 1u << (8u + component);
        if (!WithinReferenceV2(p[component], r[component], referenceScale)) mismatchFlags |= 1u << (12u + component);
        if (!WithinPairV2(m[component], p[component], pairScale)) mismatchFlags |= 1u << (16u + component);
    }

    const uint baseIndex = index * 6u;
    ComparisonWords[baseIndex + 0u] = uint4(index, finiteFlags, asuint(matrixError.x), asuint(matrixError.y));
    ComparisonWords[baseIndex + 1u] = uint4(asuint(matrixError.z), asuint(pgaError.x), asuint(pgaError.y), asuint(pgaError.z));
    ComparisonWords[baseIndex + 2u] = uint4(asuint(pairError.x), asuint(pairError.y), asuint(pairError.z), matrixUlp.x);
    ComparisonWords[baseIndex + 3u] = uint4(matrixUlp.y, matrixUlp.z, pgaUlp.x, pgaUlp.y);
    ComparisonWords[baseIndex + 4u] = uint4(pgaUlp.z, pairUlp.x, pairUlp.y, pairUlp.z);
    ComparisonWords[baseIndex + 5u] = uint4(mismatchFlags, 0u, 0u, 0u);
}
)hlsl";
}

[[nodiscard]] base::Result<std::vector<std::byte>, ScenarioErrorV1> CompilePointComparisonLeafV1(
    const corpus::QualificationCaseV1& value)
{
    const std::uint64_t pointBytes = static_cast<std::uint64_t>(value.inputPoints.size()) * sizeof(semantic::Float4Point);
    const std::uint64_t comparisonBytes = static_cast<std::uint64_t>(value.inputPoints.size()) * contracts::ComparisonRecordBytesV1;
    sem::SemanticBuilder builder;
    auto reference = builder.AddExternalBuffer("Spiral1.Compare.Reference", pointBytes, sizeof(semantic::Float4Point));
    auto matrix = builder.AddExternalBuffer("Spiral1.Compare.Matrix", pointBytes, sizeof(semantic::Float4Point));
    auto direct = builder.AddExternalBuffer("Spiral1.Compare.Direct", pointBytes, sizeof(semantic::Float4Point));
    auto records = builder.AddExternalBuffer("Spiral1.Compare.Records", comparisonBytes, 16);
    if (!reference || !matrix || !direct || !records)
        return Failure<std::vector<std::byte>>(
            "comparison-leaf/resources", "Point Comparison Leaf resource construction failed");
    auto useReference = builder.AddUse(reference.Value(), sem::Effect::Read, sem::ViewRole::ShaderBuffer);
    auto useMatrix = builder.AddUse(matrix.Value(), sem::Effect::Read, sem::ViewRole::ShaderBuffer);
    auto useDirect = builder.AddUse(direct.Value(), sem::Effect::Read, sem::ViewRole::ShaderBuffer);
    auto useRecords = builder.AddUse(records.Value(), sem::Effect::Write, sem::ViewRole::StorageBuffer);
    if (!useReference || !useMatrix || !useDirect || !useRecords)
        return Failure<std::vector<std::byte>>(
            "comparison-leaf/uses", "Point Comparison Leaf ResourceUse construction failed");

    sem::ProgramInterface interfaceDescription;
    interfaceDescription.parameters = {
        {{0}, "Reference", sem::ProgramParameterKind::ReadOnlyBuffer, sem::ShaderStage::Compute, 0, 0, 1},
        {{1}, "Matrix", sem::ProgramParameterKind::ReadOnlyBuffer, sem::ShaderStage::Compute, 1, 0, 1},
        {{2}, "Direct", sem::ProgramParameterKind::ReadOnlyBuffer, sem::ShaderStage::Compute, 2, 0, 1},
        {{3}, "Records", sem::ProgramParameterKind::UnorderedBuffer, sem::ShaderStage::Compute, 0, 0, 1}};
    auto program = builder.AddComputeProgram(
        "Spiral1.PointComparison.V2", std::move(interfaceDescription),
        {ComparisonShaderV1(static_cast<std::uint32_t>(value.inputPoints.size())), {}, {}, "CSMain"});
    if (!program)
        return Failure<std::vector<std::byte>>("comparison-leaf/program", program.Error());
    const std::array operands = {
        sem::WorkOperand{sem::WorkOperandKind::ProgramParameter, useReference.Value(), {0}},
        sem::WorkOperand{sem::WorkOperandKind::ProgramParameter, useMatrix.Value(), {1}},
        sem::WorkOperand{sem::WorkOperandKind::ProgramParameter, useDirect.Value(), {2}},
        sem::WorkOperand{sem::WorkOperandKind::ProgramParameter, useRecords.Value(), {3}}};
    const std::uint32_t dispatchX = static_cast<std::uint32_t>((value.inputPoints.size() + 63u) / 64u);
    auto work = builder.AddComputeWorkGeneric(
        "Spiral1.PointComparison.Work.V2", program.Value(), operands, dispatchX, 1, 1);
    if (!work)
        return Failure<std::vector<std::byte>>("comparison-leaf/work", work.Error());
    auto compiled = compiler::CompileCanonical(std::move(builder).Build(), AuxiliaryLeafProfileV1());
    if (!compiled)
        return Failure<std::vector<std::byte>>(
            compiled.Error().stage, compiled.Error().message);
    return base::Result<std::vector<std::byte>, ScenarioErrorV1>::Success(
        std::move(compiled).Value().packageBytes);
}

struct PreparedRepresentationLeaf final
{
    semantic::ApplyPgaMotorSemanticV1 semanticValue;
    rep::VerifiedRepresentationPlanV1 verifiedPlan;
    leaf::FrozenRepresentationLeafV1 frozenLeaf;

    PreparedRepresentationLeaf(
        semantic::ApplyPgaMotorSemanticV1 semanticInput,
        rep::VerifiedRepresentationPlanV1 plan,
        leaf::FrozenRepresentationLeafV1 artifact)
        : semanticValue(std::move(semanticInput)),
          verifiedPlan(std::move(plan)),
          frozenLeaf(std::move(artifact)) {}
};

[[nodiscard]] base::Result<PreparedRepresentationLeaf, ScenarioErrorV1> PrepareRepresentationLeafV1(
    const corpus::QualificationCaseV1& value,
    rep::LoweringKindV1 kind,
    const leaf::NativeProgramSetV1& nativePrograms)
{
    auto semanticValue = semantic::BuildApplyPgaMotorSemanticV1(
        value.motor, static_cast<std::uint32_t>(value.inputPoints.size()),
        value.caseIdentity, contracts::ObservationContractIdentityV2());
    if (!semanticValue)
        return Failure<PreparedRepresentationLeaf>("representation/semantic", semanticValue.Error());
    auto raw = rep::PlanRepresentationCandidateV1(
        semanticValue.Value(), nativePrograms.representationTargetProfile, kind);
    if (!raw)
        return Failure<PreparedRepresentationLeaf>("representation/planner", raw.Error());
    auto verified = rep::IndependentRepresentationVerifierV1::Verify(
        semanticValue.Value(), nativePrograms.representationTargetProfile, raw.Value());
    if (!verified)
        return Failure<PreparedRepresentationLeaf>("representation/verifier", verified.Error().message);
    auto frozen = leaf::CompileVerifiedRepresentationLeafV1(
        semanticValue.Value(), nativePrograms, leaf::CanonicalLeafD3D12TargetProfileV1(), verified.Value());
    if (!frozen)
        return Failure<PreparedRepresentationLeaf>(frozen.Error().stage, frozen.Error().message);
    auto valid = leaf::ValidateFrozenRepresentationLeafV1(
        frozen.Value(), semanticValue.Value(), nativePrograms, verified.Value());
    if (!valid)
        return Failure<PreparedRepresentationLeaf>(valid.Error().stage, valid.Error().message);
    return base::Result<PreparedRepresentationLeaf, ScenarioErrorV1>::Success(
        PreparedRepresentationLeaf(
            std::move(semanticValue).Value(), std::move(verified).Value(), std::move(frozen).Value()));
}

[[nodiscard]] base::Result<void, ScenarioErrorV1> ValidateAuxiliaryPackageTopology(
    std::span<const std::byte> bytes,
    std::string_view stage,
    std::size_t expectedExternalSlots)
{
    auto packageValue = pkg::PackageReader::Read(bytes);
    if (!packageValue)
        return Failure<void>(std::string(stage), packageValue.Error().message);
    auto view = schema::D3D12PackageView::Decode(packageValue.Value());
    if (!view)
        return Failure<void>(std::string(stage), view.Error().message);
    if (view.Value().ExternalSlots().size() != expectedExternalSlots ||
        !view.Value().DynamicSlots().empty() || !view.Value().SurfaceSlots().empty() ||
        view.Value().ComputeExecutables().size() != 1 || view.Value().ComputeCommands().size() != 1)
        return Failure<void>(std::string(stage), "auxiliary Leaf topology differs from its frozen contract");
    return base::Result<void, ScenarioErrorV1>::Success();
}

[[nodiscard]] base::Result<base::Digest256, ScenarioErrorV1> PackageExecutionDigest(
    std::span<const std::byte> bytes,
    std::string_view stage)
{
    auto packageValue = pkg::PackageReader::Read(bytes);
    if (!packageValue)
        return Failure<base::Digest256>(std::string(stage), packageValue.Error().message);
    return base::Result<base::Digest256, ScenarioErrorV1>::Success(packageValue.Value().ExecutionDigest());
}

[[nodiscard]] canonical::LeafPackageDeclaration LeafDeclaration(
    std::string key,
    std::vector<std::byte> packageBytes,
    std::initializer_list<canonical::LeafEndpointDeclaration> endpoints)
{
    canonical::LeafPackageDeclaration declaration;
    declaration.stableKey = std::move(key);
    declaration.packageBytes = std::move(packageBytes);
    declaration.endpoints.assign(endpoints.begin(), endpoints.end());
    return declaration;
}

[[nodiscard]] canonical::EndpointReferenceDeclaration Ref(
    std::string leafKey,
    std::string endpointKey)
{
    return {std::move(leafKey), std::move(endpointKey)};
}

[[nodiscard]] canonical::ContractBuildInput BuildFourLeafContractInput(
    const FrozenComparisonScenarioV1& artifact)
{
    canonical::ContractBuildInput input;
    input.leaves = {
        LeafDeclaration(std::string(SourceLeafKeyV1), artifact.corpusSourcePackageBytes,
            {{0, "input-points"}, {1, "reference-points"}}),
        LeafDeclaration(std::string(MatrixLeafKeyV1), artifact.matrixLeaf.packageBytes,
            {{0, "input"}, {1, "output"}}),
        LeafDeclaration(std::string(DirectLeafKeyV1), artifact.directPgaLeaf.packageBytes,
            {{0, "input"}, {1, "output"}}),
        LeafDeclaration(std::string(ComparisonLeafKeyV1), artifact.comparisonPackageBytes,
            {{0, "reference"}, {1, "matrix"}, {2, "direct"}, {3, "records"}})};

    canonical::ResourceFlowDeclaration inputPoints;
    inputPoints.stableKey = std::string(InputPointsFlowKeyV1);
    inputPoints.boundary = canonical::ResourceBoundary::Internal;
    inputPoints.producer = Ref(std::string(SourceLeafKeyV1), "input-points");
    inputPoints.consumers = {
        Ref(std::string(MatrixLeafKeyV1), "input"),
        Ref(std::string(DirectLeafKeyV1), "input")};

    canonical::ResourceFlowDeclaration referencePoints;
    referencePoints.stableKey = std::string(ReferencePointsFlowKeyV1);
    referencePoints.boundary = canonical::ResourceBoundary::Internal;
    referencePoints.producer = Ref(std::string(SourceLeafKeyV1), "reference-points");
    referencePoints.consumers = {Ref(std::string(ComparisonLeafKeyV1), "reference")};

    canonical::ResourceFlowDeclaration matrixOutput;
    matrixOutput.stableKey = std::string(MatrixOutputFlowKeyV1);
    matrixOutput.boundary = canonical::ResourceBoundary::Internal;
    matrixOutput.producer = Ref(std::string(MatrixLeafKeyV1), "output");
    matrixOutput.consumers = {Ref(std::string(ComparisonLeafKeyV1), "matrix")};

    canonical::ResourceFlowDeclaration directOutput;
    directOutput.stableKey = std::string(DirectOutputFlowKeyV1);
    directOutput.boundary = canonical::ResourceBoundary::Internal;
    directOutput.producer = Ref(std::string(DirectLeafKeyV1), "output");
    directOutput.consumers = {Ref(std::string(ComparisonLeafKeyV1), "direct")};

    canonical::ResourceFlowDeclaration comparisonRecords;
    comparisonRecords.stableKey = std::string(ComparisonRecordsFlowKeyV1);
    comparisonRecords.boundary = canonical::ResourceBoundary::CompositionOutput;
    comparisonRecords.producer = Ref(std::string(ComparisonLeafKeyV1), "records");

    input.resources = {
        std::move(inputPoints), std::move(referencePoints), std::move(matrixOutput),
        std::move(directOutput), std::move(comparisonRecords)};
    return input;
}

[[nodiscard]] base::Result<std::vector<std::byte>, ScenarioErrorV1> FreezeFourLeafComposition(
    canonical::ContractBuildInput input)
{
    auto contract = canonical::BuildCompositionContract(std::move(input));
    if (!contract)
        return Failure<std::vector<std::byte>>(contract.Error().stage, contract.Error().message);
    auto raw = planning::ProposeCompositionPlan(contract.Value());
    if (!raw)
        return Failure<std::vector<std::byte>>(raw.Error().stage, raw.Error().message);
    auto verified = verification::VerifyAndSeal(contract.Value(), raw.Value());
    if (!verified)
        return Failure<std::vector<std::byte>>(verified.Error().stage, verified.Error().message);
    auto frozen = verification::FreezeVerifiedComposition(contract.Value(), verified.Value());
    if (!frozen)
        return Failure<std::vector<std::byte>>(frozen.Error().stage, frozen.Error().message);
    return base::Result<std::vector<std::byte>, ScenarioErrorV1>::Success(std::move(frozen).Value());
}

[[nodiscard]] const canonical::CompositionEndpointContract* FindEndpoint(
    const canonical::PackageCompositionContract& contract,
    canonical::LeafPackageId leafId,
    std::string_view endpointKey) noexcept
{
    const auto key = canonical::ComputeStableEndpointKey(endpointKey);
    for (const auto& endpoint : contract.endpoints)
        if (endpoint.leaf == leafId && endpoint.stableKey == key) return &endpoint;
    return nullptr;
}

[[nodiscard]] const canonical::ResourceFlowContract* FindFlow(
    const canonical::PackageCompositionContract& contract,
    std::string_view flowKey) noexcept
{
    const auto key = canonical::ComputeStableResourceKey(flowKey);
    for (const auto& resource : contract.resources)
        if (resource.stableKey == key) return &resource;
    return nullptr;
}

[[nodiscard]] bool SameConsumers(
    std::span<const canonical::CompositionEndpointId> actual,
    std::initializer_list<canonical::CompositionEndpointId> expected)
{
    std::vector<std::uint32_t> a;
    std::vector<std::uint32_t> e;
    for (const auto value : actual) a.push_back(value.value);
    for (const auto value : expected) e.push_back(value.value);
    std::sort(a.begin(), a.end());
    std::sort(e.begin(), e.end());
    return a == e;
}

[[nodiscard]] base::Result<void, ScenarioErrorV1> ValidateRoleGraph(
    const canonical::PackageCompositionContract& contract)
{
    if (contract.leaves.size() != 4 || contract.resources.size() != 5 ||
        contract.presenterLeaf.IsValid())
        return Failure<void>("scenario/roles", "Spiral 1 requires four headless Leaves and five Resource Flows");
    const auto source = FindLeafByAuthorKeyV1(contract, SourceLeafKeyV1);
    const auto matrix = FindLeafByAuthorKeyV1(contract, MatrixLeafKeyV1);
    const auto direct = FindLeafByAuthorKeyV1(contract, DirectLeafKeyV1);
    const auto compare = FindLeafByAuthorKeyV1(contract, ComparisonLeafKeyV1);
    if (!source.IsValid() || !matrix.IsValid() || !direct.IsValid() || !compare.IsValid())
        return Failure<void>("scenario/roles", "a canonical Spiral 1 Leaf role is missing");

    const auto* sourceInput = FindEndpoint(contract, source, "input-points");
    const auto* sourceReference = FindEndpoint(contract, source, "reference-points");
    const auto* matrixInput = FindEndpoint(contract, matrix, "input");
    const auto* matrixOutputEndpoint = FindEndpoint(contract, matrix, "output");
    const auto* directInput = FindEndpoint(contract, direct, "input");
    const auto* directOutputEndpoint = FindEndpoint(contract, direct, "output");
    const auto* compareReference = FindEndpoint(contract, compare, "reference");
    const auto* compareMatrix = FindEndpoint(contract, compare, "matrix");
    const auto* compareDirect = FindEndpoint(contract, compare, "direct");
    const auto* compareRecords = FindEndpoint(contract, compare, "records");
    if (!sourceInput || !sourceReference || !matrixInput || !matrixOutputEndpoint ||
        !directInput || !directOutputEndpoint || !compareReference || !compareMatrix ||
        !compareDirect || !compareRecords)
        return Failure<void>("scenario/roles", "a canonical Spiral 1 endpoint role is missing");

    const auto* inputFlow = FindFlow(contract, InputPointsFlowKeyV1);
    const auto* referenceFlow = FindFlow(contract, ReferencePointsFlowKeyV1);
    const auto* matrixFlow = FindFlow(contract, MatrixOutputFlowKeyV1);
    const auto* directFlow = FindFlow(contract, DirectOutputFlowKeyV1);
    const auto* recordsFlow = FindFlow(contract, ComparisonRecordsFlowKeyV1);
    if (!inputFlow || !referenceFlow || !matrixFlow || !directFlow || !recordsFlow)
        return Failure<void>("scenario/roles", "a canonical Spiral 1 Resource Flow is missing");

    const bool valid =
        inputFlow->boundary == canonical::ResourceBoundary::Internal &&
        inputFlow->producer == sourceInput->id &&
        SameConsumers(inputFlow->consumers, {matrixInput->id, directInput->id}) &&
        referenceFlow->boundary == canonical::ResourceBoundary::Internal &&
        referenceFlow->producer == sourceReference->id &&
        SameConsumers(referenceFlow->consumers, {compareReference->id}) &&
        matrixFlow->boundary == canonical::ResourceBoundary::Internal &&
        matrixFlow->producer == matrixOutputEndpoint->id &&
        SameConsumers(matrixFlow->consumers, {compareMatrix->id}) &&
        directFlow->boundary == canonical::ResourceBoundary::Internal &&
        directFlow->producer == directOutputEndpoint->id &&
        SameConsumers(directFlow->consumers, {compareDirect->id}) &&
        recordsFlow->boundary == canonical::ResourceBoundary::CompositionOutput &&
        recordsFlow->producer == compareRecords->id && recordsFlow->consumers.empty();
    if (!valid)
        return Failure<void>("scenario/roles", "Frozen Composition role wiring differs from the Spiral 1 Research Contract");
    return base::Result<void, ScenarioErrorV1>::Success();
}

[[nodiscard]] std::vector<semantic::Float4Point> DecodePoints(std::span<const std::byte> bytes)
{
    std::vector<semantic::Float4Point> output;
    if (bytes.size() % sizeof(semantic::Float4Point) != 0) return output;
    output.resize(bytes.size() / sizeof(semantic::Float4Point));
    if (!bytes.empty()) std::memcpy(output.data(), bytes.data(), bytes.size());
    return output;
}

[[nodiscard]] base::Result<std::vector<contracts::PointComparisonRecordV1>, ScenarioErrorV1>
DecodeRecords(std::span<const std::byte> bytes, std::uint32_t pointCount)
{
    if (bytes.size() != static_cast<std::size_t>(pointCount) * contracts::ComparisonRecordBytesV1)
        return Failure<std::vector<contracts::PointComparisonRecordV1>>(
            "scenario/readback", "Comparison Record readback byte count is invalid");
    std::vector<contracts::PointComparisonRecordV1> output(pointCount);
    if (!bytes.empty()) std::memcpy(output.data(), bytes.data(), bytes.size());
    for (std::uint32_t index = 0; index < pointCount; ++index)
    {
        if (output[index].pointIndex != index ||
            std::ranges::any_of(output[index].reserved, [](std::uint32_t value) { return value != 0; }))
            return Failure<std::vector<contracts::PointComparisonRecordV1>>(
                "scenario/readback", "Comparison Records are not canonical point-index-ordered V1 records");
    }
    return base::Result<std::vector<contracts::PointComparisonRecordV1>, ScenarioErrorV1>::Success(
        std::move(output));
}

[[nodiscard]] std::vector<std::byte> SerializeIdentityPreimage(
    const FrozenComparisonScenarioV1& artifact)
{
    base::BinaryWriter writer;
    constexpr std::string_view domain = "SGE4-5.Spiral1.FrozenComparisonScenario.V1";
    writer.WriteU32(static_cast<std::uint32_t>(domain.size()));
    writer.WriteBytes(std::as_bytes(std::span(domain.data(), domain.size())));
    writer.WriteU32(artifact.schemaVersion);
    WriteDigest(writer, artifact.caseIdentity);
    writer.WriteU32(artifact.pointCount);
    writer.WriteU32(0);
    WriteDigest(writer, artifact.observationContractIdentity);
    const auto matrix = leaf::SerializeFrozenRepresentationLeafEvidenceV1(artifact.matrixLeaf);
    const auto direct = leaf::SerializeFrozenRepresentationLeafEvidenceV1(artifact.directPgaLeaf);
    WriteBlob(writer, matrix);
    WriteBlob(writer, direct);
    WriteDigest(writer, artifact.corpusSourcePackageExecutionDigest);
    WriteDigest(writer, artifact.comparisonPackageExecutionDigest);
    WriteBlob(writer, artifact.corpusSourcePackageBytes);
    WriteBlob(writer, artifact.comparisonPackageBytes);
    WriteBlob(writer, artifact.frozenCompositionBytes);
    return std::move(writer).Take();
}
}

composition::canonical::LeafPackageId FindLeafByAuthorKeyV1(
    const composition::canonical::PackageCompositionContract& contract,
    std::string_view authorKey) noexcept
{
    const auto key = composition::canonical::ComputeStableLeafKey(authorKey);
    for (const auto& leafValue : contract.leaves)
        if (leafValue.stableKey == key) return leafValue.id;
    return {};
}

composition::canonical::ResourceFlowId FindFlowByAuthorKeyV1(
    const composition::canonical::PackageCompositionContract& contract,
    std::string_view authorKey) noexcept
{
    const auto key = composition::canonical::ComputeStableResourceKey(authorKey);
    for (const auto& resource : contract.resources)
        if (resource.stableKey == key) return resource.id;
    return {};
}

base::Digest256 ComputeFrozenComparisonScenarioIdentityV1(
    const FrozenComparisonScenarioV1& artifact)
{
    const auto bytes = SerializeIdentityPreimage(artifact);
    return base::Sha256(bytes);
}

std::vector<std::byte> SerializeFrozenComparisonScenarioEvidenceV1(
    const FrozenComparisonScenarioV1& artifact)
{
    auto bytes = SerializeIdentityPreimage(artifact);
    base::BinaryWriter writer;
    writer.WriteBytes(bytes);
    WriteDigest(writer, artifact.artifactIdentity);
    return std::move(writer).Take();
}

base::Result<FrozenComparisonScenarioV1, ScenarioErrorV1>
BuildFrozenComparisonScenarioV1(
    const corpus::QualificationCaseV1& qualificationCase,
    const leaf::NativeProgramSetV1& nativePrograms)
{
    auto source = CompileCorpusSourceLeafV1(qualificationCase);
    if (!source) return base::Result<FrozenComparisonScenarioV1, ScenarioErrorV1>::Failure(source.Error());
    auto matrix = PrepareRepresentationLeafV1(
        qualificationCase, rep::LoweringKindV1::MatrixExpandedCompute, nativePrograms);
    if (!matrix) return base::Result<FrozenComparisonScenarioV1, ScenarioErrorV1>::Failure(matrix.Error());
    auto direct = PrepareRepresentationLeafV1(
        qualificationCase, rep::LoweringKindV1::DirectPgaMotorCompute, nativePrograms);
    if (!direct) return base::Result<FrozenComparisonScenarioV1, ScenarioErrorV1>::Failure(direct.Error());
    auto comparison = CompilePointComparisonLeafV1(qualificationCase);
    if (!comparison) return base::Result<FrozenComparisonScenarioV1, ScenarioErrorV1>::Failure(comparison.Error());

    FrozenComparisonScenarioV1 artifact;
    artifact.caseIdentity = qualificationCase.caseIdentity;
    artifact.pointCount = static_cast<std::uint32_t>(qualificationCase.inputPoints.size());
    artifact.observationContractIdentity = contracts::ObservationContractIdentityV2();
    artifact.matrixLeaf = std::move(matrix).Value().frozenLeaf;
    artifact.directPgaLeaf = std::move(direct).Value().frozenLeaf;
    artifact.corpusSourcePackageBytes = std::move(source).Value();
    artifact.comparisonPackageBytes = std::move(comparison).Value();

    auto sourceDigest = PackageExecutionDigest(artifact.corpusSourcePackageBytes, "source-leaf/package");
    if (!sourceDigest) return base::Result<FrozenComparisonScenarioV1, ScenarioErrorV1>::Failure(sourceDigest.Error());
    auto comparisonDigest = PackageExecutionDigest(artifact.comparisonPackageBytes, "comparison-leaf/package");
    if (!comparisonDigest) return base::Result<FrozenComparisonScenarioV1, ScenarioErrorV1>::Failure(comparisonDigest.Error());
    artifact.corpusSourcePackageExecutionDigest = sourceDigest.Value();
    artifact.comparisonPackageExecutionDigest = comparisonDigest.Value();

    auto frozen = FreezeFourLeafComposition(BuildFourLeafContractInput(artifact));
    if (!frozen) return base::Result<FrozenComparisonScenarioV1, ScenarioErrorV1>::Failure(frozen.Error());
    artifact.frozenCompositionBytes = std::move(frozen).Value();
    artifact.artifactIdentity = ComputeFrozenComparisonScenarioIdentityV1(artifact);

    auto valid = ValidateFrozenComparisonScenarioV1(artifact, qualificationCase, nativePrograms);
    if (!valid)
        return base::Result<FrozenComparisonScenarioV1, ScenarioErrorV1>::Failure(valid.Error());
    return base::Result<FrozenComparisonScenarioV1, ScenarioErrorV1>::Success(std::move(artifact));
}

base::Result<void, ScenarioErrorV1>
ValidateFrozenComparisonScenarioV1(
    const FrozenComparisonScenarioV1& artifact,
    const corpus::QualificationCaseV1& qualificationCase,
    const leaf::NativeProgramSetV1& nativePrograms)
{
    if (artifact.schemaVersion != FrozenComparisonScenarioSchemaVersionV1 ||
        artifact.caseIdentity != qualificationCase.caseIdentity ||
        artifact.pointCount != qualificationCase.inputPoints.size() || artifact.pointCount == 0 ||
        artifact.observationContractIdentity != contracts::ObservationContractIdentityV2() ||
        artifact.artifactIdentity != ComputeFrozenComparisonScenarioIdentityV1(artifact) ||
        IsZeroDigest(artifact.artifactIdentity))
        return Failure<void>("scenario/identity", "Frozen Comparison Scenario identity or context is invalid");

    auto matrixSemantic = semantic::BuildApplyPgaMotorSemanticV1(
        qualificationCase.motor, artifact.pointCount, qualificationCase.caseIdentity,
        contracts::ObservationContractIdentityV2());
    if (!matrixSemantic) return Failure<void>("scenario/semantic", matrixSemantic.Error());
    for (const auto* frozenLeaf : {&artifact.matrixLeaf, &artifact.directPgaLeaf})
    {
        auto raw = rep::PlanRepresentationCandidateV1(
            matrixSemantic.Value(), nativePrograms.representationTargetProfile, frozenLeaf->loweringKind);
        if (!raw) return Failure<void>("scenario/planner", raw.Error());
        auto verified = rep::IndependentRepresentationVerifierV1::Verify(
            matrixSemantic.Value(), nativePrograms.representationTargetProfile, raw.Value());
        if (!verified) return Failure<void>("scenario/verifier", verified.Error().message);
        auto valid = leaf::ValidateFrozenRepresentationLeafV1(
            *frozenLeaf, matrixSemantic.Value(), nativePrograms, verified.Value());
        if (!valid) return Failure<void>(valid.Error().stage, valid.Error().message);
    }
    if (artifact.matrixLeaf.loweringKind != rep::LoweringKindV1::MatrixExpandedCompute ||
        artifact.directPgaLeaf.loweringKind != rep::LoweringKindV1::DirectPgaMotorCompute ||
        artifact.matrixLeaf.semanticIdentity != artifact.directPgaLeaf.semanticIdentity ||
        artifact.matrixLeaf.candidateIdentity == artifact.directPgaLeaf.candidateIdentity)
        return Failure<void>("scenario/representation", "dual Representation Leaf authority is invalid");

    auto sourceDigest = PackageExecutionDigest(artifact.corpusSourcePackageBytes, "scenario/source-package");
    auto comparisonDigest = PackageExecutionDigest(artifact.comparisonPackageBytes, "scenario/comparison-package");
    if (!sourceDigest || !comparisonDigest)
        return base::Result<void, ScenarioErrorV1>::Failure(
            !sourceDigest ? sourceDigest.Error() : comparisonDigest.Error());
    if (sourceDigest.Value() != artifact.corpusSourcePackageExecutionDigest ||
        comparisonDigest.Value() != artifact.comparisonPackageExecutionDigest)
        return Failure<void>("scenario/package", "auxiliary Leaf execution digest mismatch");

    auto sourceTopology = ValidateAuxiliaryPackageTopology(
        artifact.corpusSourcePackageBytes, "scenario/source-package", 2);
    auto comparisonTopology = ValidateAuxiliaryPackageTopology(
        artifact.comparisonPackageBytes, "scenario/comparison-package", 4);
    if (!sourceTopology || !comparisonTopology)
        return base::Result<void, ScenarioErrorV1>::Failure(
            !sourceTopology ? sourceTopology.Error() : comparisonTopology.Error());

    auto frozen = verification::ReadVerifiedFrozenComposition(artifact.frozenCompositionBytes);
    if (!frozen)
        return Failure<void>(frozen.Error().stage, frozen.Error().message);
    const auto& contract = frozen.Value().ValidatedContract().Contract();
    auto roles = ValidateRoleGraph(contract);
    if (!roles) return roles;

    const auto packageFor = [&](std::string_view leafKey) -> std::span<const std::byte> {
        const auto stableKey = canonical::ComputeStableLeafKey(leafKey);
        for (const auto& embedded : frozen.Value().Artifact().Leaves())
            if (embedded.record.stableKey == stableKey) return embedded.packageBytes;
        return {};
    };
    const auto sourceBytes = packageFor(SourceLeafKeyV1);
    const auto matrixBytes = packageFor(MatrixLeafKeyV1);
    const auto directBytes = packageFor(DirectLeafKeyV1);
    const auto comparisonBytes = packageFor(ComparisonLeafKeyV1);
    if (sourceBytes.empty() || matrixBytes.empty() || directBytes.empty() || comparisonBytes.empty() ||
        !std::equal(sourceBytes.begin(), sourceBytes.end(), artifact.corpusSourcePackageBytes.begin(), artifact.corpusSourcePackageBytes.end()) ||
        !std::equal(matrixBytes.begin(), matrixBytes.end(), artifact.matrixLeaf.packageBytes.begin(), artifact.matrixLeaf.packageBytes.end()) ||
        !std::equal(directBytes.begin(), directBytes.end(), artifact.directPgaLeaf.packageBytes.begin(), artifact.directPgaLeaf.packageBytes.end()) ||
        !std::equal(comparisonBytes.begin(), comparisonBytes.end(), artifact.comparisonPackageBytes.begin(), artifact.comparisonPackageBytes.end()))
        return Failure<void>("scenario/composition", "embedded Leaf bytes do not match the authoritative Spiral 1 aggregate");
    return base::Result<void, ScenarioErrorV1>::Success();
}

base::Result<ComparisonExecutionResultV1, ScenarioErrorV1>
ExecuteFrozenComparisonScenarioV1(
    const FrozenComparisonScenarioV1& artifact,
    const corpus::QualificationCaseV1& qualificationCase,
    d3d12::D3D12Backend& backend,
    std::uint64_t frameNumber)
{
    auto verifiedArtifact = verification::ReadVerifiedFrozenComposition(artifact.frozenCompositionBytes);
    if (!verifiedArtifact)
        return Failure<ComparisonExecutionResultV1>(
            verifiedArtifact.Error().stage, verifiedArtifact.Error().message);
    const auto& contract = verifiedArtifact.Value().ValidatedContract().Contract();
    const auto matrixLeafId = FindLeafByAuthorKeyV1(contract, MatrixLeafKeyV1);
    const auto directLeafId = FindLeafByAuthorKeyV1(contract, DirectLeafKeyV1);
    const auto inputFlow = FindFlowByAuthorKeyV1(contract, InputPointsFlowKeyV1);
    const auto referenceFlow = FindFlowByAuthorKeyV1(contract, ReferencePointsFlowKeyV1);
    const auto matrixFlow = FindFlowByAuthorKeyV1(contract, MatrixOutputFlowKeyV1);
    const auto directFlow = FindFlowByAuthorKeyV1(contract, DirectOutputFlowKeyV1);
    const auto recordsFlow = FindFlowByAuthorKeyV1(contract, ComparisonRecordsFlowKeyV1);
    if (!matrixLeafId.IsValid() || !directLeafId.IsValid() || !inputFlow.IsValid() ||
        !referenceFlow.IsValid() || !matrixFlow.IsValid() || !directFlow.IsValid() || !recordsFlow.IsValid())
        return Failure<ComparisonExecutionResultV1>(
            "scenario/runtime", "canonical Leaf or Resource Flow identity was not found");

    auto loaded = runtime::LoadStaticComposition(artifact.frozenCompositionBytes, backend);
    if (!loaded)
        return Failure<ComparisonExecutionResultV1>(loaded.Error().stage, loaded.Error().message);
    runtime::StaticCompositionFrameInvocation invocation;
    invocation.frameNumber = frameNumber;
    invocation.dynamicData = {
        {matrixLeafId, artifact.matrixLeaf.lockedDynamicSlot, artifact.matrixLeaf.lockedConstantPayload},
        {directLeafId, artifact.directPgaLeaf.lockedDynamicSlot, artifact.directPgaLeaf.lockedConstantPayload}};
    auto submitted = runtime::SubmitStaticComposition(loaded.Value(), invocation);
    if (!submitted)
        return Failure<ComparisonExecutionResultV1>(submitted.Error().stage, submitted.Error().message);
    if (submitted.Value().executionOrder.size() != 4 || submitted.Value().leaves.size() != 4)
        return Failure<ComparisonExecutionResultV1>(
            "scenario/runtime", "Frozen Composition did not execute exactly four Leaves");

    auto inputRead = runtime::ReadStaticCompositionBuffer(loaded.Value(), inputFlow);
    auto referenceRead = runtime::ReadStaticCompositionBuffer(loaded.Value(), referenceFlow);
    auto matrixRead = runtime::ReadStaticCompositionBuffer(loaded.Value(), matrixFlow);
    auto directRead = runtime::ReadStaticCompositionBuffer(loaded.Value(), directFlow);
    auto recordsRead = runtime::ReadStaticCompositionBuffer(loaded.Value(), recordsFlow);
    if (!inputRead || !referenceRead || !matrixRead || !directRead || !recordsRead)
    {
        const auto* error = !inputRead ? &inputRead.Error() : !referenceRead ? &referenceRead.Error() :
            !matrixRead ? &matrixRead.Error() : !directRead ? &directRead.Error() : &recordsRead.Error();
        return Failure<ComparisonExecutionResultV1>(error->stage, error->message);
    }

    const auto expectedInput = semantic::SerializePoints(qualificationCase.inputPoints);
    const auto expectedReference = semantic::SerializePoints(qualificationCase.referencePoints);
    if (inputRead.Value().bytes != expectedInput || referenceRead.Value().bytes != expectedReference)
        return Failure<ComparisonExecutionResultV1>(
            "scenario/source-observation", "Corpus Source Leaf did not reproduce frozen input/reference bytes");

    auto matrixPoints = DecodePoints(matrixRead.Value().bytes);
    auto directPoints = DecodePoints(directRead.Value().bytes);
    if (matrixPoints.size() != artifact.pointCount || directPoints.size() != artifact.pointCount)
        return Failure<ComparisonExecutionResultV1>(
            "scenario/readback", "transform output readback point count is invalid");
    auto observed = observer::ObserveCaseV1(qualificationCase, matrixPoints, directPoints);
    if (!observed)
        return Failure<ComparisonExecutionResultV1>("scenario/observer", observed.Error());
    const auto expectedRecords = contracts::SerializeComparisonRecordsV1(observed.Value().records);
    if (recordsRead.Value().bytes != expectedRecords)
    {
        const auto actual = std::span<const std::byte>(recordsRead.Value().bytes);
        const auto expected = std::span<const std::byte>(expectedRecords);
        std::size_t firstDifference = 0;
        while (firstDifference < actual.size() && firstDifference < expected.size() &&
               actual[firstDifference] == expected[firstDifference])
            ++firstDifference;
        const std::size_t recordIndex = firstDifference / contracts::ComparisonRecordBytesV1;
        const std::size_t recordOffset = firstDifference % contracts::ComparisonRecordBytesV1;
        std::ostringstream message;
        message << "GPU Comparison Leaf bytes differ from the deterministic CPU V1 record contract"
                << "; firstByte=" << firstDifference
                << "; record=" << recordIndex
                << "; recordOffset=" << recordOffset;
        if (firstDifference < actual.size() && firstDifference < expected.size())
            message << "; gpu=0x" << std::hex << std::to_integer<unsigned int>(actual[firstDifference])
                    << "; cpu=0x" << std::to_integer<unsigned int>(expected[firstDifference]);
        return Failure<ComparisonExecutionResultV1>("scenario/comparison", message.str());
    }
    auto decodedRecords = DecodeRecords(recordsRead.Value().bytes, artifact.pointCount);
    if (!decodedRecords)
        return base::Result<ComparisonExecutionResultV1, ScenarioErrorV1>::Failure(decodedRecords.Error());
    if (observed.Value().report.mismatchCount != 0 || observed.Value().report.nonFiniteCount != 0)
    {
        const auto& report = observed.Value().report;
        std::ostringstream message;
        message << "Frozen Composition failed Observation Contract V2"
                << "; mismatches=" << report.mismatchCount
                << "; nonFinite=" << report.nonFiniteCount
                << "; firstMismatch=" << report.firstMismatchIndex
                << "; matrixMaxAbs=" << report.matrix.maxAbsoluteError
                << "; pgaMaxAbs=" << report.pga.maxAbsoluteError
                << "; pairMaxAbs=" << report.pair.maxAbsoluteError;
        return Failure<ComparisonExecutionResultV1>("scenario/observation", message.str());
    }

    ComparisonExecutionResultV1 result;
    result.matrixOutput = std::move(matrixPoints);
    result.directPgaOutput = std::move(directPoints);
    result.comparisonRecords = std::move(decodedRecords).Value();
    result.report = std::move(observed).Value().report;
    result.frozenCompositionDigest = base::Sha256(artifact.frozenCompositionBytes);
    return base::Result<ComparisonExecutionResultV1, ScenarioErrorV1>::Success(std::move(result));
}
}
