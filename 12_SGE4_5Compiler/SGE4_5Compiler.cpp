#include "SGE4_5Compiler.h"

#include "../07_ExecutionPlanVerifier/ExecutionPlanVerifier.h"

#include <utility>
#include <vector>

namespace sge4_5::compiler
{
base::Result<PlanningOutput, Error> Compile(
    const semantic::SemanticGraph& graph,
    const target::D3D12TargetProfile& targetProfile,
    const planning::CompilerPolicy& policy,
    const planning::ProfileRecord* profile,
    const planning::ProfileSelectionContext* profileContext)
{
    auto planned = d3d12::candidate::PlanCandidates(graph, targetProfile, policy);
    if (!planned)
        return base::Result<PlanningOutput, Error>::Failure(planned.Error());

    auto candidateSet = std::move(planned).Value();
    std::vector<PackageOutput> packages(candidateSet.manifest.candidates.size());
    std::uint32_t loweredCount = 0;
    for (std::size_t index = 0; index < candidateSet.manifest.candidates.size(); ++index)
    {
        auto& record = candidateSet.manifest.candidates[index];
        if (!record.verification.verified ||
            loweredCount >= policy.budget.maxVerifiedCandidates)
            continue;

        // CandidatePlanner exposes only data and a report. The Compiler must
        // independently obtain a fresh verifier capability token immediately
        // before Package lowering; a raw or mutated Plan cannot cross this gate.
        auto sealed = planning::verification::VerifyAndSeal(
            candidateSet.obligation, candidateSet.planningContract, record.plan);
        if (!sealed)
            return base::Result<PlanningOutput, Error>::Failure(
                {"compiler-orchestration", "a Planner-verified candidate failed re-sealing before Package lowering"});

        auto package = d3d12::LowerVerifiedPlan(graph, targetProfile, sealed.Value());
        if (!package)
            return base::Result<PlanningOutput, Error>::Failure(package.Error());

        record.packageExecutionDigestHex = package.Value().executionDigestHex;
        packages[index] = std::move(package).Value();
        ++loweredCount;
    }

    auto obligation = candidateSet.obligation;
    auto planningContract = candidateSet.planningContract;
    auto selected = d3d12::candidate::SelectCandidate(
        std::move(candidateSet), policy, profile, profileContext);
    if (!selected)
        return base::Result<PlanningOutput, Error>::Failure(selected.Error());

    auto selection = std::move(selected).Value();
    if (selection.selectedCandidateIndex >= packages.size() ||
        packages[selection.selectedCandidateIndex].packageBytes.empty())
        return base::Result<PlanningOutput, Error>::Failure(
            {"compiler-orchestration", "selected candidate has no lowered Package artifact"});

    PlanningOutput output;
    output.selectedPackage = std::move(packages[selection.selectedCandidateIndex]);
    output.obligation = std::move(obligation);
    output.planningContract = planningContract;
    output.selectedPlan = std::move(selection.selectedPlan);
    output.manifest = std::move(selection.manifest);
    return base::Result<PlanningOutput, Error>::Success(std::move(output));
}

base::Result<PackageOutput, Error> CompileCanonical(
    const semantic::SemanticGraph& graph,
    const target::D3D12TargetProfile& targetProfile)
{
    planning::CompilerPolicy policy;
    policy.kind = planning::CompilerPolicyKind::CanonicalSafe;
    policy.budget.maxProposedCandidates = 1;
    policy.budget.maxVerifiedCandidates = 1;
    policy.budget.maxCandidatesPerAxis = 1;
    policy.budget.compileWorkUnitBudget = base::InvalidIndex;
    auto compiled = Compile(graph, targetProfile, policy);
    if (!compiled)
        return base::Result<PackageOutput, Error>::Failure(compiled.Error());
    return base::Result<PackageOutput, Error>::Success(
        std::move(compiled.Value().selectedPackage));
}

base::Result<FrozenComputePackageEvidenceV1, Error>
InspectCanonicalComputePackageEvidenceV1(std::span<const std::byte> packageBytes)
{
    return d3d12::InspectFrozenComputePackageEvidenceV1(packageBytes);
}

}
