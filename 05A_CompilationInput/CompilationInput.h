#pragma once

#include "../00_Foundation/Result.h"

// Windows SDK headers define `interface` as a preprocessor macro.
#ifdef interface
#pragma push_macro("interface")
#undef interface
#define SGE4_5_COMPILATION_INPUT_RESTORE_INTERFACE_MACRO
#endif

#include "../02_SemanticModel/SemanticModel.h"
#include "../04_SemanticAnalysis/SemanticAnalysis.h"
#include "../05_TargetContract/TargetModel.h"

#ifdef SGE4_5_COMPILATION_INPUT_RESTORE_INTERFACE_MACRO
#pragma pop_macro("interface")
#undef SGE4_5_COMPILATION_INPUT_RESTORE_INTERFACE_MACRO
#endif

#include <string>

namespace sge4_5::compiler::compilation
{
struct CompilationError final
{
    std::string stage;
    std::string message;
};

// Neutral validated input shared by candidate planning and target lowering.
// Neither side owns source validation or Schema-17 target feasibility.
struct ValidatedCompilationInput final
{
    const semantic::SemanticGraph* source = nullptr;
    target::D3D12TargetProfile targetProfile;
    analysis::AnalyzedGraph analyzed;
};

[[nodiscard]] base::Result<void, CompilationError> ValidateD3D12Schema17Capability(
    const semantic::SemanticGraph& graph,
    const target::D3D12TargetProfile& targetProfile);

[[nodiscard]] base::Result<ValidatedCompilationInput, CompilationError> ValidateCompilationInput(
    const semantic::SemanticGraph& graph,
    const target::D3D12TargetProfile& targetProfile);
}
