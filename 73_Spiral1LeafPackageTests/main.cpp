#include "../00_Foundation/FileIO.h"
#include "../00_Foundation/Sha256.h"
#include "../09_FrozenPackageCore/PackageReader.h"
#include "../13_PackageRuntime/PackageRuntime.h"
#include "../14_D3D12Backend/D3D12Backend.h"
#include "../60_PgaRigidTransformSemantic/PgaRigidTransformSemantic.h"
#include "../61_Spiral1Contracts/Spiral1Contracts.h"
#include "../62_Spiral1Corpus/Spiral1Corpus.h"
#include "../64_D3D12RepresentationPlanner/D3D12RepresentationPlanner.h"
#include "../65_D3D12RepresentationVerifier/D3D12RepresentationVerifier.h"
#include "../66_Spiral1LeafCompiler/Spiral1LeafCompiler.h"
#include "../67_Spiral1Observer/Spiral1Observer.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace
{
namespace base = sge4_5::base;
namespace sem = sge4_5::spiral1::semantic;
namespace contracts = sge4_5::spiral1::contracts;
namespace corpus = sge4_5::spiral1::corpus;
namespace rep = sge4_5::spiral1::representation;
namespace leaf = sge4_5::spiral1::leaf;
namespace observer = sge4_5::spiral1::observer;

static_assert(!std::is_default_constructible_v<rep::VerifiedRepresentationPlanV1>);
static_assert(!std::is_invocable_v<decltype(&leaf::CompileVerifiedRepresentationLeafV1),
    const sem::ApplyPgaMotorSemanticV1&, const leaf::NativeProgramSetV1&,
    const sge4_5::target::D3D12TargetProfile&, const rep::RawRepresentationCandidateV1&>);

void Require(bool condition, std::string_view message)
{
    if (!condition) throw std::runtime_error(std::string(message));
}

template<class T>
std::span<const std::byte> Bytes(const T& value)
{
    return std::as_bytes(std::span(&value, 1));
}

template<class T>
std::span<const std::byte> Bytes(std::span<const T> values)
{
    return std::as_bytes(values);
}

std::shared_ptr<sge4_5::runtime::ICompletionToken> ReleaseToken(
    const sge4_5::runtime::FrameSubmission& submission,
    std::uint32_t slot)
{
    const auto found = std::find_if(submission.releasedExternalResources.begin(),
        submission.releasedExternalResources.end(), [=](const auto& value) { return value.slot == slot; });
    return found == submission.releasedExternalResources.end() ? nullptr : found->safeAfter;
}

std::vector<sem::Float4Point> DecodePoints(std::span<const std::byte> bytes)
{
    Require(bytes.size() % sizeof(sem::Float4Point) == 0, "readback byte count is not Float4Point aligned");
    std::vector<sem::Float4Point> output(bytes.size() / sizeof(sem::Float4Point));
    if (!bytes.empty()) std::memcpy(output.data(), bytes.data(), bytes.size());
    return output;
}

const corpus::QualificationCaseV1& FindCase(
    const corpus::QualificationCorpusV1& values,
    std::string_view scenario,
    std::uint32_t subscenario = 0)
{
    const auto found = std::find_if(values.cases.begin(), values.cases.end(), [&](const auto& value) {
        return value.scenarioId == scenario && value.subscenarioIndex == subscenario;
    });
    if (found == values.cases.end()) throw std::runtime_error("qualification case was not found");
    return *found;
}

sem::ApplyPgaMotorSemanticV1 BuildSemantic(const corpus::QualificationCaseV1& value)
{
    auto semanticValue = sem::BuildApplyPgaMotorSemanticV1(
        value.motor, static_cast<std::uint32_t>(value.inputPoints.size()),
        value.caseIdentity, contracts::ObservationContractIdentityV1());
    if (!semanticValue) throw std::runtime_error(semanticValue.Error());
    return std::move(semanticValue).Value();
}

struct PreparedLeaf final
{
    sem::ApplyPgaMotorSemanticV1 semanticValue;
    rep::VerifiedRepresentationPlanV1 verifiedPlan;
    leaf::FrozenRepresentationLeafV1 frozenLeaf;

    PreparedLeaf(sem::ApplyPgaMotorSemanticV1 semanticInput,
                 rep::VerifiedRepresentationPlanV1 plan,
                 leaf::FrozenRepresentationLeafV1 artifact)
        : semanticValue(std::move(semanticInput)), verifiedPlan(std::move(plan)), frozenLeaf(std::move(artifact)) {}
};

PreparedLeaf PrepareLeaf(
    const corpus::QualificationCaseV1& value,
    rep::LoweringKindV1 kind,
    const leaf::NativeProgramSetV1& nativePrograms)
{
    auto semanticValue = BuildSemantic(value);
    auto raw = rep::PlanRepresentationCandidateV1(
        semanticValue, nativePrograms.representationTargetProfile, kind);
    if (!raw) throw std::runtime_error(raw.Error());
    auto verified = rep::IndependentRepresentationVerifierV1::Verify(
        semanticValue, nativePrograms.representationTargetProfile, raw.Value());
    if (!verified) throw std::runtime_error(verified.Error().message);
    auto frozen = leaf::CompileVerifiedRepresentationLeafV1(
        semanticValue, nativePrograms, leaf::CanonicalLeafD3D12TargetProfileV1(), verified.Value());
    if (!frozen) throw std::runtime_error(frozen.Error().stage + ": " + frozen.Error().message);
    auto validation = leaf::ValidateFrozenRepresentationLeafV1(
        frozen.Value(), semanticValue, nativePrograms, verified.Value());
    if (!validation) throw std::runtime_error(validation.Error().stage + ": " + validation.Error().message);
    return PreparedLeaf(std::move(semanticValue), std::move(verified).Value(), std::move(frozen).Value());
}

std::vector<sem::Float4Point> ExecuteLeaf(
    const leaf::FrozenRepresentationLeafV1& artifact,
    std::span<const sem::Float4Point> inputPoints)
{
    auto packageValue = sge4_5::package::PackageReader::Read(artifact.packageBytes);
    if (!packageValue) throw std::runtime_error(packageValue.Error().message);
    sge4_5::d3d12::D3D12Backend backend({true, true});
    auto loaded = sge4_5::runtime::LoadPackage(std::move(packageValue).Value(), backend);
    if (!loaded) throw std::runtime_error(loaded.Error().stage + ": " + loaded.Error().message);

    const std::vector<std::byte> zero(inputPoints.size_bytes(), std::byte{0});
    auto input = backend.CreateExternalBuffer(loaded.Value().Instance(), 0, Bytes(inputPoints));
    auto output = backend.CreateExternalBuffer(loaded.Value().Instance(), 1, zero);
    if (!input || !output)
    {
        const auto& error = !input ? input.Error() : output.Error();
        throw std::runtime_error(error.stage + ": " + error.message);
    }
    const sge4_5::runtime::DynamicDataBinding dynamic[1] = {
        {artifact.lockedDynamicSlot, artifact.lockedConstantPayload}};
    const sge4_5::runtime::ExternalResourceBinding external[2] = {
        {0, input.Value().resource, input.Value().availableAfter},
        {1, output.Value().resource, output.Value().availableAfter}};
    sge4_5::runtime::FrameInvocation invocation{0, dynamic, external};
    auto submitted = sge4_5::runtime::Submit(loaded.Value(), backend, invocation);
    if (!submitted) throw std::runtime_error(submitted.Error().stage + ": " + submitted.Error().message);
    Require(submitted.Value().releasedExternalResources.size() == 2,
            "Leaf submission must release both external slots");
    auto readback = backend.ReadExternalBuffer(
        loaded.Value().Instance(), output.Value().resource, ReleaseToken(submitted.Value(), 1));
    if (!readback) throw std::runtime_error(readback.Error().stage + ": " + readback.Error().message);
    return DecodePoints(readback.Value().bytes);
}

void ValidateSingleOutput(
    const corpus::QualificationCaseV1& value,
    std::span<const sem::Float4Point> output,
    std::string_view label)
{
    Require(output.size() == value.referencePoints.size(), "Leaf output point count mismatch");
    for (std::size_t index = 0; index < output.size(); ++index)
    {
        const auto& actual = output[index];
        const auto& expected = value.referencePoints[index];
        if (!contracts::WithinReferenceTolerance(actual.x, expected.x) ||
            !contracts::WithinReferenceTolerance(actual.y, expected.y) ||
            !contracts::WithinReferenceTolerance(actual.z, expected.z) ||
            !std::isfinite(actual.x) || !std::isfinite(actual.y) || !std::isfinite(actual.z) ||
            std::bit_cast<std::uint32_t>(actual.w) != std::bit_cast<std::uint32_t>(1.0f))
        {
            std::cerr << label << " mismatch scenario=" << value.scenarioId
                      << " point=" << index << '\n';
            throw std::runtime_error("Leaf output failed the CPU reference observation contract");
        }
    }
}

std::vector<const corpus::QualificationCaseV1*> RepresentativeCases(
    const corpus::QualificationCorpusV1& values)
{
    return {&FindCase(values, "S00"), &FindCase(values, "S07"), &FindCase(values, "S14")};
}

void RunStage08(
    const corpus::QualificationCorpusV1& values,
    const leaf::NativeProgramSetV1& nativePrograms)
{
    for (const auto* value : RepresentativeCases(values))
    {
        auto matrix = PrepareLeaf(*value, rep::LoweringKindV1::MatrixExpandedCompute, nativePrograms);
        auto output = ExecuteLeaf(matrix.frozenLeaf, value->inputPoints);
        ValidateSingleOutput(*value, output, "MatrixExpandedComputeV1");
    }
    std::cout << "Stage 08 Matrix Frozen Leaf standalone WARP qualification passed.\n";
}

void RunStage09(
    const corpus::QualificationCorpusV1& values,
    const leaf::NativeProgramSetV1& nativePrograms)
{
    for (const auto* value : RepresentativeCases(values))
    {
        auto direct = PrepareLeaf(*value, rep::LoweringKindV1::DirectPgaMotorCompute, nativePrograms);
        auto output = ExecuteLeaf(direct.frozenLeaf, value->inputPoints);
        ValidateSingleOutput(*value, output, "DirectPgaMotorComputeV1");
    }
    std::cout << "Stage 09 Direct PGA Frozen Leaf standalone WARP qualification passed.\n";
}

void RunCU3(
    const corpus::QualificationCorpusV1& values,
    const leaf::NativeProgramSetV1& nativePrograms)
{
    for (const auto* value : RepresentativeCases(values))
    {
        auto matrix = PrepareLeaf(*value, rep::LoweringKindV1::MatrixExpandedCompute, nativePrograms);
        auto direct = PrepareLeaf(*value, rep::LoweringKindV1::DirectPgaMotorCompute, nativePrograms);
        Require(matrix.frozenLeaf.semanticIdentity == direct.frozenLeaf.semanticIdentity,
                "dual Leaves do not share one Semantic identity");
        Require(matrix.frozenLeaf.candidateIdentity != direct.frozenLeaf.candidateIdentity,
                "dual Leaves unexpectedly share one Candidate identity");
        Require(matrix.frozenLeaf.packageExecutionDigest != direct.frozenLeaf.packageExecutionDigest,
                "dual Leaves unexpectedly share one Package execution digest");
        auto corrupted = matrix.frozenLeaf;
        Require(!corrupted.lockedConstantPayload.empty(), "locked constant payload is empty");
        corrupted.lockedConstantPayload[0] ^= std::byte{1};
        auto corruptedValidation = leaf::ValidateFrozenRepresentationLeafV1(
            corrupted, matrix.semanticValue, nativePrograms, matrix.verifiedPlan);
        Require(!corruptedValidation, "tampered locked constant payload was accepted");
        auto matrixOutput = ExecuteLeaf(matrix.frozenLeaf, value->inputPoints);
        auto directOutput = ExecuteLeaf(direct.frozenLeaf, value->inputPoints);
        ValidateSingleOutput(*value, matrixOutput, "MatrixExpandedComputeV1");
        ValidateSingleOutput(*value, directOutput, "DirectPgaMotorComputeV1");
        auto observed = observer::ObserveCaseV1(*value, matrixOutput, directOutput);
        if (!observed) throw std::runtime_error(observed.Error());
        Require(observed.Value().report.mismatchCount == 0,
                "dual Leaf outputs failed pairwise observation");
    }
    std::cout << "CU3 dual Frozen Leaf authority, WARP and common observation qualification passed.\n";
}

std::vector<std::byte> BuildEvidence(
    std::string_view mode,
    const corpus::QualificationCorpusV1& values,
    const leaf::NativeProgramSetV1& nativePrograms)
{
    const auto& value = FindCase(values, "S07");
    base::BinaryWriter writer;
    constexpr std::string_view domain = "SGE4-5.Spiral1.CU3.Evidence.V1";
    writer.WriteU32(static_cast<std::uint32_t>(domain.size()));
    writer.WriteBytes(std::as_bytes(std::span(domain.data(), domain.size())));
    writer.WriteU32(static_cast<std::uint32_t>(mode.size()));
    writer.WriteBytes(std::as_bytes(std::span(mode.data(), mode.size())));
    writer.WriteBytes(nativePrograms.representationTargetProfile.identity);
    writer.WriteBytes(value.caseIdentity);
    const auto emitOne = [&](rep::LoweringKindV1 kind) {
        auto prepared = PrepareLeaf(value, kind, nativePrograms);
        const auto bytes = leaf::SerializeFrozenRepresentationLeafEvidenceV1(prepared.frozenLeaf);
        writer.WriteU32(static_cast<std::uint32_t>(bytes.size()));
        writer.WriteBytes(bytes);
    };
    if (mode == "Stage08" || mode == "CU3") emitOne(rep::LoweringKindV1::MatrixExpandedCompute);
    if (mode == "Stage09" || mode == "CU3") emitOne(rep::LoweringKindV1::DirectPgaMotorCompute);
    return std::move(writer).Take();
}
}

int main(int argc, char** argv)
{
    try
    {
        auto corpusValue = corpus::BuildQualificationCorpusV1();
        if (!corpusValue) throw std::runtime_error(corpusValue.Error());
        auto nativePrograms = leaf::BuildNativeProgramSetV1();
        if (!nativePrograms) throw std::runtime_error(nativePrograms.Error().stage + ": " + nativePrograms.Error().message);

        if (argc == 3 && std::string_view(argv[1]).starts_with("--emit-"))
        {
            std::string mode;
            const std::string_view arg = argv[1];
            if (arg == "--emit-stage08") mode = "Stage08";
            else if (arg == "--emit-stage09") mode = "Stage09";
            else if (arg == "--emit-cu3") mode = "CU3";
            else throw std::runtime_error("unknown emit mode");
            auto evidence = BuildEvidence(mode, corpusValue.Value(), nativePrograms.Value());
            auto written = base::WriteAllBytes(std::filesystem::path(argv[2]), evidence);
            if (!written) throw std::runtime_error(written.Error());
            return 0;
        }

        const std::string_view mode = argc >= 2 ? std::string_view(argv[1]) : "--cu3";
        if (mode == "--stage08") RunStage08(corpusValue.Value(), nativePrograms.Value());
        else if (mode == "--stage09") RunStage09(corpusValue.Value(), nativePrograms.Value());
        else if (mode == "--cu3") RunCU3(corpusValue.Value(), nativePrograms.Value());
        else throw std::runtime_error("unknown test mode");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "CU3 failure: " << error.what() << '\n';
        return 1;
    }
}
