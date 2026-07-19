#pragma once

#include "../00_Foundation/Result.h"
#include "../02_SemanticModel/SemanticModel.h"
#include "../05_TargetContract/TargetModel.h"
#include "../08_CandidatePlanner/CandidatePlanner.h"
#include "../11_D3D12PackageLowering/D3D12PackageLowering.h"

namespace sge4_5::compiler
{
using Error = d3d12::CompileError;
using PackageOutput = d3d12::CompileOutput;

struct PlanningOutput final
{
    PackageOutput selectedPackage;
    planning::SemanticObligation obligation;
    planning::D3D12PlanningContract planningContract;
    planning::ExecutionPlanIR selectedPlan;
    d3d12::candidate::CandidateManifest manifest;
};

// The only normal SGE4 compile entry. CandidatePlanner cannot lower Packages;
// this facade is the sole orchestrator that re-seals verified candidates,
// invokes D3D12PackageLowering, and requests deterministic policy selection.
[[nodiscard]] base::Result<PlanningOutput, Error> Compile(
    const semantic::SemanticGraph& graph,
    const target::D3D12TargetProfile& targetProfile,
    const planning::CompilerPolicy& policy,
    const planning::ProfileRecord* profile = nullptr,
    const planning::ProfileSelectionContext* profileContext = nullptr);

[[nodiscard]] base::Result<PackageOutput, Error> CompileCanonical(
    const semantic::SemanticGraph& graph,
    const target::D3D12TargetProfile& targetProfile);
}
