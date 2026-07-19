#include "Spiral1LeafCompiler.h"

#include "../00_Foundation/FileIO.h"
#include "../00_Foundation/Sha256.h"
#include "../03_SemanticBuilder/SemanticBuilder.h"
#include "../09_FrozenPackageCore/PackageReader.h"
#include "../10_D3D12PackageSchema/D3D12Encoding.h"
#include "../12_SGE4_5Compiler/SGE4_5Compiler.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <span>
#include <utility>

#ifdef _WIN32
#include <Windows.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#endif

namespace sge4_5::spiral1::leaf
{
namespace
{
using package::d3d12_v13::D3D12PackageDescription;
using package::d3d12_v13::D3D12PackageView;

[[nodiscard]] LeafCompileErrorV1 Error(
    LeafCompileErrorCodeV1 code,
    std::string stage,
    std::string message)
{
    return {code, std::move(stage), std::move(message)};
}

[[nodiscard]] bool IsZeroDigest(const base::Digest256& value) noexcept
{
    return std::all_of(value.begin(), value.end(), [](std::byte b) { return b == std::byte{0}; });
}

[[nodiscard]] const std::vector<std::byte>& BinaryFor(
    const NativeProgramSetV1& programs,
    representation::LoweringKindV1 kind)
{
    return kind == representation::LoweringKindV1::MatrixExpandedCompute
        ? programs.matrixProgramBinary
        : programs.directPgaProgramBinary;
}

#ifdef _WIN32
[[nodiscard]] base::Result<std::vector<std::byte>, LeafCompileErrorV1> CompileCanonicalProgram(
    representation::LoweringKindV1 kind)
{
    const auto source = representation::CanonicalProgramTemplateSourceV1(kind);
    if (source.empty())
        return base::Result<std::vector<std::byte>, LeafCompileErrorV1>::Failure(
            Error(LeafCompileErrorCodeV1::NativeProgramCompilationFailed,
                  "native-program", "canonical program template source is empty"));

    Microsoft::WRL::ComPtr<ID3DBlob> shader;
    Microsoft::WRL::ComPtr<ID3DBlob> errors;
    constexpr UINT flags = D3DCOMPILE_ENABLE_STRICTNESS |
                           D3DCOMPILE_WARNINGS_ARE_ERRORS |
                           D3DCOMPILE_IEEE_STRICTNESS |
                           D3DCOMPILE_OPTIMIZATION_LEVEL3;
    const HRESULT result = D3DCompile(
        source.data(), source.size(), nullptr, nullptr, nullptr,
        "CSMain", "cs_5_1", flags, 0, &shader, &errors);
    if (FAILED(result) || !shader)
    {
        std::string message = "D3DCompile failed for canonical Spiral 1 program";
        if (errors)
            message.assign(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize());
        return base::Result<std::vector<std::byte>, LeafCompileErrorV1>::Failure(
            Error(LeafCompileErrorCodeV1::NativeProgramCompilationFailed,
                  "native-program", std::move(message)));
    }
    const auto* first = static_cast<const std::byte*>(shader->GetBufferPointer());
    return base::Result<std::vector<std::byte>, LeafCompileErrorV1>::Success(
        std::vector<std::byte>(first, first + shader->GetBufferSize()));
}

[[nodiscard]] base::Result<base::Digest256, LeafCompileErrorV1> CompilerFingerprint()
{
    HMODULE module = GetModuleHandleW(L"d3dcompiler_47.dll");
    if (module == nullptr)
        module = GetModuleHandleW(L"D3DCompiler_47.dll");
    if (module == nullptr)
        return base::Result<base::Digest256, LeafCompileErrorV1>::Failure(
            Error(LeafCompileErrorCodeV1::NativeCompilerFingerprintFailed,
                  "native-program", "D3DCompiler_47.dll is not loaded"));

    std::wstring path(32768, L'\0');
    const DWORD length = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
    if (length == 0 || length >= path.size())
        return base::Result<base::Digest256, LeafCompileErrorV1>::Failure(
            Error(LeafCompileErrorCodeV1::NativeCompilerFingerprintFailed,
                  "native-program", "GetModuleFileNameW failed for D3DCompiler_47.dll"));
    path.resize(length);
    auto bytes = base::ReadAllBytes(std::filesystem::path(path));
    if (!bytes)
        return base::Result<base::Digest256, LeafCompileErrorV1>::Failure(
            Error(LeafCompileErrorCodeV1::NativeCompilerFingerprintFailed,
                  "native-program", bytes.Error()));
    return base::Result<base::Digest256, LeafCompileErrorV1>::Success(base::Sha256(bytes.Value()));
}
#endif

[[nodiscard]] base::Result<D3D12PackageDescription, LeafCompileErrorV1> CopyDescription(
    const D3D12PackageView& view,
    const representation::RawRepresentationCandidateV1& candidate,
    std::span<const std::byte> canonicalProgramBinary)
{
    D3D12PackageDescription description;
    description.profile = view.Profile();
    description.resources.assign(view.Resources().begin(), view.Resources().end());
    description.allocations.assign(view.Allocations().begin(), view.Allocations().end());
    description.views.assign(view.Views().begin(), view.Views().end());
    description.shaders.assign(view.Shaders().begin(), view.Shaders().end());
    description.programs.assign(view.Programs().begin(), view.Programs().end());
    description.bindingLayouts.assign(view.BindingLayouts().begin(), view.BindingLayouts().end());
    description.rootParameters.assign(view.RootParameters().begin(), view.RootParameters().end());
    description.vertexElements.assign(view.VertexElements().begin(), view.VertexElements().end());
    description.executables.assign(view.Executables().begin(), view.Executables().end());
    description.computeExecutables.assign(view.ComputeExecutables().begin(), view.ComputeExecutables().end());
    description.computeCommands.assign(view.ComputeCommands().begin(), view.ComputeCommands().end());
    description.attachmentOperations.assign(view.AttachmentOperations().begin(), view.AttachmentOperations().end());
    description.rasterCommands.assign(view.RasterCommands().begin(), view.RasterCommands().end());
    description.dynamicSlots.assign(view.DynamicSlots().begin(), view.DynamicSlots().end());
    description.externalSlots.assign(view.ExternalSlots().begin(), view.ExternalSlots().end());
    description.surfaceSlots.assign(view.SurfaceSlots().begin(), view.SurfaceSlots().end());
    description.initialData.assign(view.InitialData().begin(), view.InitialData().end());
    description.nativeObjectData.assign(view.NativeObjectData().begin(), view.NativeObjectData().end());

    description.operationStreams = {
        {package::d3d12_v13::OperationStreamKind::Load, 0,
         static_cast<std::uint32_t>(view.LoadOperations().size()), 0},
        {package::d3d12_v13::OperationStreamKind::Frame,
         static_cast<std::uint32_t>(view.LoadOperations().size()),
         static_cast<std::uint32_t>(view.FrameOperations().size()), 0}};
    for (const auto& operation : view.LoadOperations())
        description.operations.push_back({operation.opcode, operation.operationVersion,
                                          operation.flags, operation.queue,
                                          std::vector<std::byte>(operation.payload.begin(), operation.payload.end())});
    for (const auto& operation : view.FrameOperations())
        description.operations.push_back({operation.opcode, operation.operationVersion,
                                          operation.flags, operation.queue,
                                          std::vector<std::byte>(operation.payload.begin(), operation.payload.end())});

    if (description.shaders.size() != 1 ||
        description.shaders[0].stage != package::d3d12_v13::ShaderStage::Compute)
        return base::Result<D3D12PackageDescription, LeafCompileErrorV1>::Failure(
            Error(LeafCompileErrorCodeV1::PreliminaryPackageInvalid,
                  "leaf-package", "Spiral 1 Leaf must contain exactly one Compute shader"));

    description.shaderData.assign(canonicalProgramBinary.begin(), canonicalProgramBinary.end());
    auto& shader = description.shaders[0];
    shader.bytecode.section = package::SectionKind::ShaderData;
    shader.bytecode.reserved = 0;
    shader.bytecode.offset = 0;
    shader.bytecode.size = canonicalProgramBinary.size();
    shader.bytecodeDigest = base::Sha256(canonicalProgramBinary);
    if (shader.bytecodeDigest != candidate.programBinaryDigest)
        return base::Result<D3D12PackageDescription, LeafCompileErrorV1>::Failure(
            Error(LeafCompileErrorCodeV1::ProgramBinaryAuthorityMismatch,
                  "leaf-package", "canonical program binary does not match Verified Representation Candidate"));

    return base::Result<D3D12PackageDescription, LeafCompileErrorV1>::Success(std::move(description));
}

[[nodiscard]] base::Result<void, LeafCompileErrorV1> ValidateFinalPackage(
    const package::FrozenExecutablePackage& frozen,
    const D3D12PackageView& view,
    const representation::RawRepresentationCandidateV1& candidate)
{
    if (view.ExternalSlots().size() != 2 || view.DynamicSlots().size() != 1 ||
        !view.SurfaceSlots().empty() || view.ComputeExecutables().size() != 1 ||
        view.ComputeCommands().size() != 1 || view.Shaders().size() != 1)
        return base::Result<void, LeafCompileErrorV1>::Failure(
            Error(LeafCompileErrorCodeV1::FrozenPackageQualificationFailed,
                  "leaf-package", "Frozen Leaf topology does not match the locked-constant, two-external-slot Compute contract"));
    const auto& constantSlot = view.DynamicSlots()[0];
    if (constantSlot.id.value != 0 || !constantSlot.destinationResource.IsValid() ||
        constantSlot.destinationResource.value >= view.Resources().size() ||
        constantSlot.requiredBytes != candidate.constantPayload.size() ||
        constantSlot.requiredAlignment != 16 || constantSlot.destinationOffset != 0 ||
        view.Resources()[constantSlot.destinationResource.value].origin !=
            package::d3d12_v13::ResourceOrigin::PackageOwned)
        return base::Result<void, LeafCompileErrorV1>::Failure(
            Error(LeafCompileErrorCodeV1::FrozenPackageQualificationFailed,
                  "leaf-package", "Frozen Leaf constant DynamicDataSlot does not match the Verified Representation Plan"));
    if (view.ComputeCommands()[0].threadGroupCountX != candidate.dispatchX ||
        view.ComputeCommands()[0].threadGroupCountY != 1 ||
        view.ComputeCommands()[0].threadGroupCountZ != 1)
        return base::Result<void, LeafCompileErrorV1>::Failure(
            Error(LeafCompileErrorCodeV1::FrozenPackageQualificationFailed,
                  "leaf-package", "Frozen Leaf dispatch does not match the Verified Representation Plan"));
    if (view.Shaders()[0].bytecodeDigest != candidate.programBinaryDigest)
        return base::Result<void, LeafCompileErrorV1>::Failure(
            Error(LeafCompileErrorCodeV1::ProgramBinaryAuthorityMismatch,
                  "leaf-package", "Frozen Leaf shader digest differs from the Verified Representation Plan"));

    const bool constantRootBound = std::any_of(
        view.RootParameters().begin(), view.RootParameters().end(), [](const auto& parameter) {
            return parameter.kind == package::d3d12_v13::RootParameterKind::ConstantBuffer &&
                   parameter.dynamicSlot.IsValid() && parameter.dynamicSlot.value == 0 &&
                   !parameter.staticView.IsValid();
        });
    if (!constantRootBound)
        return base::Result<void, LeafCompileErrorV1>::Failure(
            Error(LeafCompileErrorCodeV1::FrozenPackageQualificationFailed,
                  "leaf-package", "Frozen Leaf root layout does not bind the verified constant slot"));
    if (IsZeroDigest(frozen.ExecutionDigest()) || IsZeroDigest(frozen.Header().fileDigest))
        return base::Result<void, LeafCompileErrorV1>::Failure(
            Error(LeafCompileErrorCodeV1::FrozenPackageQualificationFailed,
                  "leaf-package", "Frozen Leaf digest is zero"));
    return base::Result<void, LeafCompileErrorV1>::Success();
}
}

target::D3D12TargetProfile CanonicalLeafD3D12TargetProfileV1()
{
    target::D3D12TargetProfile profile;
    profile.framesInFlight = 2;
    profile.directQueueCount = 1;
    profile.computeQueueCount = 1;
    profile.copyQueueCount = 0;
    profile.surfaceImageCount = 0;
    profile.rtvDescriptorCount = 0;
    profile.dsvDescriptorCount = 0;
    profile.shaderDescriptorCount = 2;
    profile.samplerDescriptorCount = 0;
    return profile;
}

base::Result<NativeProgramSetV1, LeafCompileErrorV1> BuildNativeProgramSetV1()
{
#ifndef _WIN32
    return base::Result<NativeProgramSetV1, LeafCompileErrorV1>::Failure(
        Error(LeafCompileErrorCodeV1::NativeCompilerUnavailable,
              "native-program", "D3DCompile is available only on Windows"));
#else
    auto matrix = CompileCanonicalProgram(representation::LoweringKindV1::MatrixExpandedCompute);
    if (!matrix) return base::Result<NativeProgramSetV1, LeafCompileErrorV1>::Failure(matrix.Error());
    auto direct = CompileCanonicalProgram(representation::LoweringKindV1::DirectPgaMotorCompute);
    if (!direct) return base::Result<NativeProgramSetV1, LeafCompileErrorV1>::Failure(direct.Error());
    auto fingerprint = CompilerFingerprint();
    if (!fingerprint) return base::Result<NativeProgramSetV1, LeafCompileErrorV1>::Failure(fingerprint.Error());
    auto profile = representation::BuildRepresentationTargetProfileV1(
        fingerprint.Value(), base::Sha256(matrix.Value()), base::Sha256(direct.Value()));
    if (!profile)
        return base::Result<NativeProgramSetV1, LeafCompileErrorV1>::Failure(
            Error(LeafCompileErrorCodeV1::NativeProgramCompilationFailed,
                  "native-program", profile.Error()));
    NativeProgramSetV1 output;
    output.representationTargetProfile = std::move(profile).Value();
    output.matrixProgramBinary = std::move(matrix).Value();
    output.directPgaProgramBinary = std::move(direct).Value();
    return base::Result<NativeProgramSetV1, LeafCompileErrorV1>::Success(std::move(output));
#endif
}

base::Result<::sge4_5::semantic::SemanticGraph, LeafCompileErrorV1> BuildVerifiedRepresentationLeafGraphV1(
    const semantic::ApplyPgaMotorSemanticV1& semanticValue,
    const representation::RepresentationTargetProfileV1& representationTargetProfile,
    const representation::VerifiedRepresentationPlanV1& verifiedPlan)
{
    auto context = representation::IndependentRepresentationVerifierV1::ValidatePlanContext(
        verifiedPlan, semanticValue, representationTargetProfile);
    if (!context)
        return base::Result<::sge4_5::semantic::SemanticGraph, LeafCompileErrorV1>::Failure(
            Error(LeafCompileErrorCodeV1::VerifiedPlanContextMismatch,
                  "representation-authority", context.Error().message));

    const auto& candidate = verifiedPlan.Candidate();
    const std::uint64_t pointBytes = static_cast<std::uint64_t>(candidate.pointCount) * semantic::Float4PointStrideBytesV1;
    ::sge4_5::semantic::SemanticBuilder builder;
    auto input = builder.AddExternalBuffer("Spiral1.InputPoints", pointBytes, semantic::Float4PointStrideBytesV1);
    auto output = builder.AddExternalBuffer("Spiral1.OutputPoints", pointBytes, semantic::Float4PointStrideBytesV1);
    auto constants = builder.AddDynamicBuffer(
        "Spiral1.VerifiedConstants", candidate.constantPayload.size(), 16);
    if (!input || !output || !constants)
    {
        const std::string message = !input ? input.Error() : !output ? output.Error() : constants.Error();
        return base::Result<::sge4_5::semantic::SemanticGraph, LeafCompileErrorV1>::Failure(
            Error(LeafCompileErrorCodeV1::SemanticGraphConstructionFailed,
                  "semantic-bridge", message));
    }
    auto constantUse = builder.AddUse(constants.Value(), ::sge4_5::semantic::Effect::Read, ::sge4_5::semantic::ViewRole::ConstantData);
    auto inputUse = builder.AddUse(input.Value(), ::sge4_5::semantic::Effect::Read, ::sge4_5::semantic::ViewRole::ShaderBuffer);
    auto outputUse = builder.AddUse(output.Value(), ::sge4_5::semantic::Effect::Write, ::sge4_5::semantic::ViewRole::StorageBuffer);
    if (!constantUse || !inputUse || !outputUse)
    {
        const std::string message = !constantUse ? constantUse.Error() : !inputUse ? inputUse.Error() : outputUse.Error();
        return base::Result<::sge4_5::semantic::SemanticGraph, LeafCompileErrorV1>::Failure(
            Error(LeafCompileErrorCodeV1::SemanticGraphConstructionFailed,
                  "semantic-bridge", message));
    }

    ::sge4_5::semantic::ProgramInterface interfaceDescription;
    interfaceDescription.parameters = {
        {{0}, "Constants", ::sge4_5::semantic::ProgramParameterKind::ConstantBuffer,
         ::sge4_5::semantic::ShaderStage::Compute, 0, candidate.constantPayload.size(), 16},
        {{1}, "InputPoints", ::sge4_5::semantic::ProgramParameterKind::ReadOnlyBuffer,
         ::sge4_5::semantic::ShaderStage::Compute, 0, 0, 1},
        {{2}, "OutputPoints", ::sge4_5::semantic::ProgramParameterKind::UnorderedBuffer,
         ::sge4_5::semantic::ShaderStage::Compute, 0, 0, 1}};
    ::sge4_5::semantic::ProgramSource source;
    source.hlslSource = std::string(representation::CanonicalProgramTemplateSourceV1(candidate.loweringKind));
    source.computeEntry = "CSMain";
    auto program = builder.AddComputeProgram("Spiral1.VerifiedTransform", std::move(interfaceDescription), std::move(source));
    if (!program)
        return base::Result<::sge4_5::semantic::SemanticGraph, LeafCompileErrorV1>::Failure(
            Error(LeafCompileErrorCodeV1::SemanticGraphConstructionFailed,
                  "semantic-bridge", program.Error()));
    const std::array operands = {
        ::sge4_5::semantic::WorkOperand{::sge4_5::semantic::WorkOperandKind::ProgramParameter, constantUse.Value(), {0}},
        ::sge4_5::semantic::WorkOperand{::sge4_5::semantic::WorkOperandKind::ProgramParameter, inputUse.Value(), {1}},
        ::sge4_5::semantic::WorkOperand{::sge4_5::semantic::WorkOperandKind::ProgramParameter, outputUse.Value(), {2}}};
    auto work = builder.AddComputeWorkGeneric(
        "Spiral1.VerifiedTransform.Work", program.Value(), operands,
        candidate.dispatchX, 1, 1);
    if (!work)
        return base::Result<::sge4_5::semantic::SemanticGraph, LeafCompileErrorV1>::Failure(
            Error(LeafCompileErrorCodeV1::SemanticGraphConstructionFailed,
                  "semantic-bridge", work.Error()));
    return base::Result<::sge4_5::semantic::SemanticGraph, LeafCompileErrorV1>::Success(
        std::move(builder).Build());
}

base::Result<FrozenRepresentationLeafV1, LeafCompileErrorV1> CompileVerifiedRepresentationLeafV1(
    const semantic::ApplyPgaMotorSemanticV1& semanticValue,
    const NativeProgramSetV1& nativePrograms,
    const target::D3D12TargetProfile& d3d12TargetProfile,
    const representation::VerifiedRepresentationPlanV1& verifiedPlan)
{
    auto graph = BuildVerifiedRepresentationLeafGraphV1(
        semanticValue, nativePrograms.representationTargetProfile, verifiedPlan);
    if (!graph) return base::Result<FrozenRepresentationLeafV1, LeafCompileErrorV1>::Failure(graph.Error());

    auto compiled = compiler::CompileCanonical(graph.Value(), d3d12TargetProfile);
    if (!compiled)
        return base::Result<FrozenRepresentationLeafV1, LeafCompileErrorV1>::Failure(
            Error(LeafCompileErrorCodeV1::ExecutionPlanCompilationFailed,
                  compiled.Error().stage, compiled.Error().message));

    auto preliminary = package::PackageReader::Read(std::move(compiled).Value().packageBytes);
    if (!preliminary)
        return base::Result<FrozenRepresentationLeafV1, LeafCompileErrorV1>::Failure(
            Error(LeafCompileErrorCodeV1::PreliminaryPackageInvalid,
                  "leaf-package", preliminary.Error().message));
    auto preliminaryView = D3D12PackageView::Decode(preliminary.Value());
    if (!preliminaryView)
        return base::Result<FrozenRepresentationLeafV1, LeafCompileErrorV1>::Failure(
            Error(LeafCompileErrorCodeV1::PreliminaryPackageInvalid,
                  "leaf-package", preliminaryView.Error().message));

    const auto& candidate = verifiedPlan.Candidate();
    const auto& binary = BinaryFor(nativePrograms, candidate.loweringKind);
    if (base::Sha256(binary) != candidate.programBinaryDigest)
        return base::Result<FrozenRepresentationLeafV1, LeafCompileErrorV1>::Failure(
            Error(LeafCompileErrorCodeV1::ProgramBinaryAuthorityMismatch,
                  "leaf-package", "native canonical Program binary digest differs from the Verified Representation Plan"));
    auto description = CopyDescription(preliminaryView.Value(), candidate, binary);
    if (!description)
        return base::Result<FrozenRepresentationLeafV1, LeafCompileErrorV1>::Failure(description.Error());
    auto rebuiltBytes = package::d3d12_v13::BuildFrozenPackage(description.Value());
    if (!rebuiltBytes)
        return base::Result<FrozenRepresentationLeafV1, LeafCompileErrorV1>::Failure(
            Error(LeafCompileErrorCodeV1::FrozenPackageRebuildFailed,
                  "leaf-package", rebuiltBytes.Error().message));
    auto finalPackage = package::PackageReader::Read(rebuiltBytes.Value());
    if (!finalPackage)
        return base::Result<FrozenRepresentationLeafV1, LeafCompileErrorV1>::Failure(
            Error(LeafCompileErrorCodeV1::FrozenPackageQualificationFailed,
                  "leaf-package", finalPackage.Error().message));
    auto finalView = D3D12PackageView::Decode(finalPackage.Value());
    if (!finalView)
        return base::Result<FrozenRepresentationLeafV1, LeafCompileErrorV1>::Failure(
            Error(LeafCompileErrorCodeV1::FrozenPackageQualificationFailed,
                  "leaf-package", finalView.Error().message));
    auto qualified = ValidateFinalPackage(finalPackage.Value(), finalView.Value(), candidate);
    if (!qualified)
        return base::Result<FrozenRepresentationLeafV1, LeafCompileErrorV1>::Failure(qualified.Error());

    FrozenRepresentationLeafV1 output;
    output.loweringKind = candidate.loweringKind;
    output.semanticIdentity = candidate.semanticIdentity;
    output.candidateIdentity = candidate.candidateIdentity;
    output.representationSeal = verifiedPlan.SealDigest();
    output.packageExecutionDigest = finalPackage.Value().ExecutionDigest();
    output.packageFileDigest = finalPackage.Value().Header().fileDigest;
    output.lockedDynamicSlot = 0;
    output.lockedConstantPayload = candidate.constantPayload;
    output.verifiedRepresentationPlanBytes = representation::SerializeVerifiedRepresentationPlanV1(verifiedPlan);
    output.packageBytes = std::move(rebuiltBytes).Value();
    return base::Result<FrozenRepresentationLeafV1, LeafCompileErrorV1>::Success(std::move(output));
}

base::Result<void, LeafCompileErrorV1> ValidateFrozenRepresentationLeafV1(
    const FrozenRepresentationLeafV1& value,
    const semantic::ApplyPgaMotorSemanticV1& semanticValue,
    const NativeProgramSetV1& nativePrograms,
    const representation::VerifiedRepresentationPlanV1& verifiedPlan)
{
    if (value.schemaVersion != FrozenRepresentationLeafSchemaVersionV1 ||
        value.loweringKind != verifiedPlan.Candidate().loweringKind ||
        value.semanticIdentity != semantic::ApplyPgaMotorSemanticIdentityV1(semanticValue) ||
        value.candidateIdentity != verifiedPlan.Candidate().candidateIdentity ||
        value.representationSeal != verifiedPlan.SealDigest() ||
        value.lockedDynamicSlot != 0 ||
        value.lockedConstantPayload != verifiedPlan.Candidate().constantPayload ||
        value.packageBytes.empty())
        return base::Result<void, LeafCompileErrorV1>::Failure(
            Error(LeafCompileErrorCodeV1::FrozenPackageQualificationFailed,
                  "leaf-artifact", "Frozen Representation Leaf identity chain mismatch"));
    auto context = representation::IndependentRepresentationVerifierV1::ValidatePlanContext(
        verifiedPlan, semanticValue, nativePrograms.representationTargetProfile);
    if (!context)
        return base::Result<void, LeafCompileErrorV1>::Failure(
            Error(LeafCompileErrorCodeV1::VerifiedPlanContextMismatch,
                  "leaf-artifact", context.Error().message));
    auto frozen = package::PackageReader::Read(value.packageBytes);
    if (!frozen)
        return base::Result<void, LeafCompileErrorV1>::Failure(
            Error(LeafCompileErrorCodeV1::FrozenPackageQualificationFailed,
                  "leaf-artifact", frozen.Error().message));
    if (frozen.Value().ExecutionDigest() != value.packageExecutionDigest ||
        frozen.Value().Header().fileDigest != value.packageFileDigest)
        return base::Result<void, LeafCompileErrorV1>::Failure(
            Error(LeafCompileErrorCodeV1::FrozenPackageQualificationFailed,
                  "leaf-artifact", "Frozen package digest metadata mismatch"));
    auto view = D3D12PackageView::Decode(frozen.Value());
    if (!view)
        return base::Result<void, LeafCompileErrorV1>::Failure(
            Error(LeafCompileErrorCodeV1::FrozenPackageQualificationFailed,
                  "leaf-artifact", view.Error().message));
    return ValidateFinalPackage(frozen.Value(), view.Value(), verifiedPlan.Candidate());
}

std::vector<std::byte> SerializeFrozenRepresentationLeafEvidenceV1(
    const FrozenRepresentationLeafV1& value)
{
    base::BinaryWriter writer;
    constexpr std::string_view domain = "SGE4-5.Spiral1.FrozenRepresentationLeafEvidence.V1";
    writer.WriteU32(static_cast<std::uint32_t>(domain.size()));
    writer.WriteBytes(std::as_bytes(std::span(domain.data(), domain.size())));
    writer.WriteU32(value.schemaVersion);
    writer.WriteU32(static_cast<std::uint32_t>(value.loweringKind));
    writer.WriteBytes(value.semanticIdentity);
    writer.WriteBytes(value.candidateIdentity);
    writer.WriteBytes(value.representationSeal);
    writer.WriteBytes(value.packageExecutionDigest);
    writer.WriteBytes(value.packageFileDigest);
    writer.WriteU32(value.lockedDynamicSlot);
    writer.WriteU32(static_cast<std::uint32_t>(value.lockedConstantPayload.size()));
    writer.WriteBytes(value.lockedConstantPayload);
    writer.WriteU32(static_cast<std::uint32_t>(value.verifiedRepresentationPlanBytes.size()));
    writer.WriteU32(static_cast<std::uint32_t>(value.packageBytes.size()));
    writer.WriteBytes(value.verifiedRepresentationPlanBytes);
    writer.WriteBytes(value.packageBytes);
    return std::move(writer).Take();
}
}
