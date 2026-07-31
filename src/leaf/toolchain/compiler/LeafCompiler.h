#pragma once

#include "../../../canonical/base/Expected.h"

#include <span>
#include "../../model/semantic/SemanticModel.h"
#include "../../../backends/d3d12/compiler/target/TargetModel.h"
#include "../../planner/CandidatePlanner.h"
#include "../../../backends/d3d12/compiler/lowering/D3D12PackageLowering.h"

namespace sge4::compiler
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
[[nodiscard]] base::Expected<PlanningOutput, Error> Compile(
    const semantic::SemanticGraph& graph,
    const target::D3D12TargetProfile& targetProfile,
    const planning::CompilerPolicy& policy,
    const planning::ProfileRecord* profile = nullptr,
    const planning::ProfileSelectionContext* profileContext = nullptr);

[[nodiscard]] base::Expected<PackageOutput, Error> CompileCanonical(
    const semantic::SemanticGraph& graph,
    const target::D3D12TargetProfile& targetProfile);

using FrozenComputePackageEvidenceV1 = d3d12::FrozenComputePackageEvidenceV1;

// Compiler facade for schema-owned Frozen Package evidence inspection.
// Clients do not gain a direct dependency on D3D12PackageSchema.
[[nodiscard]] base::Expected<FrozenComputePackageEvidenceV1, Error>
InspectCanonicalComputePackageEvidenceV1(std::span<const std::byte> packageBytes);
}
