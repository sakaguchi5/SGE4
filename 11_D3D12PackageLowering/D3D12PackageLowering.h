#pragma once

#include "../00_Foundation/Result.h"
#include "../05A_CompilationInput/CompilationInput.h"

// Windows SDK headers define `interface` as a preprocessor macro.
// SemanticModel intentionally uses `Program::interface` as a member name,
// so protect that declaration when this header is included after Windows headers.
#ifdef interface
#pragma push_macro("interface")
#undef interface
#define SGE_D3D12_TARGET_COMPILER_RESTORE_INTERFACE_MACRO
#endif

#include "../02_SemanticModel/SemanticModel.h"
#include "../04_SemanticAnalysis/SemanticAnalysis.h"
#include "../05_TargetContract/TargetModel.h"
#include "../06_ExecutionPlanModel/ExecutionPlanModel.h"

#ifdef SGE_D3D12_TARGET_COMPILER_RESTORE_INTERFACE_MACRO
#pragma pop_macro("interface")
#undef SGE_D3D12_TARGET_COMPILER_RESTORE_INTERFACE_MACRO
#endif

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace sge4_5::planning::verification
{
class VerifiedExecutionPlan;
}

namespace sge4_5::compiler::d3d12
{
using CompileError = compilation::CompilationError;
using ValidatedSourceStage = compilation::ValidatedCompilationInput;

enum class ReflectedBindingKind : std::uint16_t
{
    ConstantBuffer = 1,
    SampledTexture = 2,
    ReadOnlyBuffer = 3,
    UnorderedBuffer = 4,
    StaticSampler = 5
};

struct ReflectedBinding final
{
    ReflectedBindingKind kind = ReflectedBindingKind::ConstantBuffer;
    semantic::ShaderStage stage = semantic::ShaderStage::Vertex;
    std::uint32_t shaderRegister = 0;
    std::uint32_t bindCount = 1;
    std::uint64_t requiredBytes = 0;
};

struct ReflectedVertexInput final
{
    semantic::VertexInput::Meaning meaning = semantic::VertexInput::Meaning::Position;
    std::uint16_t componentCount = 0;
    std::uint16_t semanticIndex = 0;
};

struct CompiledShaderStage final
{
    semantic::ShaderStage stage = semantic::ShaderStage::Vertex;
    std::vector<std::byte> bytecode;
    std::vector<ReflectedBinding> bindings;
    std::vector<ReflectedVertexInput> vertexInputs;
};

struct CompiledProgram final
{
    semantic::ProgramId sourceProgram;
    std::vector<CompiledShaderStage> shaders;
};

struct ProgramCompilationStage final
{
    std::vector<CompiledProgram> programs;
};

struct CompileOutput final
{
    std::vector<std::byte> packageBytes;
    std::string executionDigestHex;
    std::vector<std::string> completedStages;
};

[[nodiscard]] base::Result<void, CompileError> ValidateLevel2Capability(
    const semantic::SemanticGraph& graph,
    const target::D3D12TargetProfile& targetProfile);

[[nodiscard]] base::Result<ValidatedSourceStage, CompileError> ValidateSourceStage(
    const semantic::SemanticGraph& graph,
    const target::D3D12TargetProfile& targetProfile);

[[nodiscard]] base::Result<ProgramCompilationStage, CompileError> CompileProgramStage(
    const ValidatedSourceStage& validated);

// Production lowering entry. The verifier-sealed type is the capability token:
// raw ExecutionPlanIR values cannot reach Package lowering.
[[nodiscard]] base::Result<CompileOutput, CompileError> LowerVerifiedPlan(
    const semantic::SemanticGraph& graph,
    const target::D3D12TargetProfile& targetProfile,
    const planning::verification::VerifiedExecutionPlan& selectedPlan);
}
