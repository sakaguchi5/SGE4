#include "SGE3CompatibilityOracle.h"

#include "../07_ExecutionPlanVerifier/ExecutionPlanVerifier.h"
#include "../11_D3D12PackageLowering/D3D12PackageLoweringInternal.h"

#include <utility>
#include <vector>

namespace sge4::compatibility::sge3
{
base::Result<compiler::d3d12::CompileOutput, compiler::d3d12::CompileError> Compile(
    const semantic::SemanticGraph& graph,
    const target::D3D12TargetProfile& targetProfile)
{
    return compiler::d3d12::CompileReferenceCanonical(graph, targetProfile);
}

base::Result<ReferencePlanningOutput, compiler::d3d12::CompileError> CompilePlanningReference(
    const semantic::SemanticGraph& graph,
    const target::D3D12TargetProfile& targetProfile,
    const planning::CompilerPolicy& policy,
    const planning::ProfileRecord* profile,
    const planning::ProfileSelectionContext* profileContext)
{
    auto planned = compiler::d3d12::candidate::PlanCandidates(graph, targetProfile, policy);
    if (!planned)
        return base::Result<ReferencePlanningOutput, compiler::d3d12::CompileError>::Failure(planned.Error());

    auto candidateSet = std::move(planned).Value();
    std::vector<compiler::d3d12::CompileOutput> packages(candidateSet.manifest.candidates.size());
    std::uint32_t loweredCount = 0;
    for (std::size_t index = 0; index < candidateSet.manifest.candidates.size(); ++index)
    {
        auto& record = candidateSet.manifest.candidates[index];
        if (!record.verification.verified || loweredCount >= policy.budget.maxVerifiedCandidates)
            continue;
        auto sealed = planning::verification::VerifyAndSeal(
            candidateSet.obligation, candidateSet.planningContract, record.plan);
        if (!sealed)
            return base::Result<ReferencePlanningOutput, compiler::d3d12::CompileError>::Failure(
                {"sge3-oracle", "SGE3 reference candidate failed verifier re-sealing"});
        auto package = compiler::d3d12::LowerVerifiedPlan(graph, targetProfile, sealed.Value());
        if (!package)
            return base::Result<ReferencePlanningOutput, compiler::d3d12::CompileError>::Failure(package.Error());
        record.packageExecutionDigestHex = package.Value().executionDigestHex;
        packages[index] = std::move(package).Value();
        ++loweredCount;
    }

    auto obligation = candidateSet.obligation;
    auto contract = candidateSet.planningContract;
    auto selected = compiler::d3d12::candidate::SelectCandidate(
        std::move(candidateSet), policy, profile, profileContext);
    if (!selected)
        return base::Result<ReferencePlanningOutput, compiler::d3d12::CompileError>::Failure(selected.Error());
    auto selection = std::move(selected).Value();
    if (selection.selectedCandidateIndex >= packages.size() ||
        packages[selection.selectedCandidateIndex].packageBytes.empty())
        return base::Result<ReferencePlanningOutput, compiler::d3d12::CompileError>::Failure(
            {"sge3-oracle", "SGE3 reference selection has no Package artifact"});

    ReferencePlanningOutput output;
    output.selectedPackage = std::move(packages[selection.selectedCandidateIndex]);
    output.obligation = std::move(obligation);
    output.planningContract = contract;
    output.selectedPlan = std::move(selection.selectedPlan);
    output.manifest = std::move(selection.manifest);
    return base::Result<ReferencePlanningOutput, compiler::d3d12::CompileError>::Success(std::move(output));
}
}
