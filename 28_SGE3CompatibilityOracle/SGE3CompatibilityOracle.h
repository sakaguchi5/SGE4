#pragma once

#include "../08_CandidatePlanner/CandidatePlanner.h"
#include "../11_D3D12PackageLowering/D3D12PackageLowering.h"

namespace sge4::compatibility::sge3
{
struct ReferencePlanningOutput final
{
    compiler::d3d12::CompileOutput selectedPackage;
    planning::SemanticObligation obligation;
    planning::D3D12PlanningContract planningContract;
    planning::ExecutionPlanIR selectedPlan;
    compiler::d3d12::candidate::CandidateManifest manifest;
};

// Frozen SGE3 canonical path retained only as a Stage-0D comparison oracle.
[[nodiscard]] base::Result<compiler::d3d12::CompileOutput, compiler::d3d12::CompileError> Compile(
    const semantic::SemanticGraph& graph,
    const target::D3D12TargetProfile& targetProfile);

// Reproduces the SGE3 Level-3 orchestration outside SGE4Compiler so Stage 0D
// can compare candidate manifests, selected Plan identity, and Package bytes.
[[nodiscard]] base::Result<ReferencePlanningOutput, compiler::d3d12::CompileError> CompilePlanningReference(
    const semantic::SemanticGraph& graph,
    const target::D3D12TargetProfile& targetProfile,
    const planning::CompilerPolicy& policy,
    const planning::ProfileRecord* profile = nullptr,
    const planning::ProfileSelectionContext* profileContext = nullptr);
}
