#pragma once
#include "./D3D12PackageLowering.h"

namespace sge4::compiler::d3d12
{
// Compatibility oracle only. Production SGE4 compilation must enter through
// 12_LeafCompiler and therefore pass CandidatePlanner + VerifyAndSeal.
[[nodiscard]] base::Expected<CompileOutput, CompileError> CompileReferenceCanonical(
    const semantic::SemanticGraph& graph,
    const target::D3D12TargetProfile& targetProfile);
}
