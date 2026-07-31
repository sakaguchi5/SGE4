#pragma once

#include "../../../canonical/base/Expected.h"

// Windows SDK headers define `interface` as a preprocessor macro.
#ifdef interface
#pragma push_macro("interface")
#undef interface
#define SGE_COMPILATION_INPUT_RESTORE_INTERFACE_MACRO
#endif

#include "../semantic/SemanticModel.h"
#include "../analysis/SemanticAnalysis.h"
#include "../../../backends/d3d12/compiler/target/TargetModel.h"

#ifdef SGE_COMPILATION_INPUT_RESTORE_INTERFACE_MACRO
#pragma pop_macro("interface")
#undef SGE_COMPILATION_INPUT_RESTORE_INTERFACE_MACRO
#endif

#include <string>

namespace sge4::compiler::compilation
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

[[nodiscard]] base::Expected<void, CompilationError> ValidateD3D12Schema17Capability(
    const semantic::SemanticGraph& graph,
    const target::D3D12TargetProfile& targetProfile);

[[nodiscard]] base::Expected<ValidatedCompilationInput, CompilationError> ValidateCompilationInput(
    const semantic::SemanticGraph& graph,
    const target::D3D12TargetProfile& targetProfile);
}
