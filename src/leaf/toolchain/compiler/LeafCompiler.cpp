#include "./LeafCompiler.h"

#include "../../verifier/ExecutionPlanVerifier.h"

#include <utility>
#include <vector>

namespace sge4::compiler
{
base::Expected<PlanningOutput, Error> Compile(
    const semantic::SemanticGraph& graph,
    const target::D3D12TargetProfile& targetProfile,
    const planning::CompilerPolicy& policy,
    const planning::ProfileRecord* profile,
    const planning::ProfileSelectionContext* profileContext)
{
    auto planned = d3d12::candidate::PlanCandidates(graph, targetProfile, policy);
    if (!planned)
        return base::Failure<PlanningOutput, Error>(planned.error());

    auto candidateSet = std::move(planned).value();
    std::vector<PackageOutput> packages(candidateSet.manifest.candidates.size());
    std::uint32_t loweredCount = 0;
    for (std::size_t index = 0; index < candidateSet.manifest.candidates.size(); ++index)
    {
        auto& record = candidateSet.manifest.candidates[index];
        if (loweredCount >= policy.budget.maxVerifiedCandidates)
            continue;

        // The Planner returns only a raw proposal. The independent Verifier is
        // invoked here by the orchestrator; the Planner has no compile-time or
        // construction path to a VerifiedExecutionPlan.
        auto sealed = planning::verification::VerifyAndSeal(
            candidateSet.obligation, candidateSet.planningContract, record.plan);
        if (!sealed)
        {
            record.verifierAccepted = false;
            record.verifierViolationCount = static_cast<std::uint32_t>(sealed.error().violations.size());
            continue;
        }
        record.verifierAccepted = true;
        record.verifierViolationCount = 0;

        auto package = d3d12::LowerVerifiedPlan(graph, targetProfile, sealed.value());
        if (!package)
            return base::Failure<PlanningOutput, Error>(package.error());

        record.packageExecutionDigestHex = package.value().executionDigestHex;
        packages[index] = std::move(package).value();
        ++loweredCount;
    }

    auto obligation = candidateSet.obligation;
    auto planningContract = candidateSet.planningContract;
    auto selected = d3d12::candidate::SelectCandidate(
        std::move(candidateSet), policy, profile, profileContext);
    if (!selected)
        return base::Failure<PlanningOutput, Error>(selected.error());

    auto selection = std::move(selected).value();
    if (selection.selectedCandidateIndex >= packages.size() ||
        packages[selection.selectedCandidateIndex].packageBytes.empty())
        return base::Failure<PlanningOutput, Error>(
            {"compiler-orchestration", "検証または実行の契約に違反しています。"});

    PlanningOutput output;
    output.selectedPackage = std::move(packages[selection.selectedCandidateIndex]);
    output.obligation = std::move(obligation);
    output.planningContract = planningContract;
    output.selectedPlan = std::move(selection.selectedPlan);
    output.manifest = std::move(selection.manifest);
    return base::Success<PlanningOutput, Error>(std::move(output));
}

base::Expected<PackageOutput, Error> CompileCanonical(
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
        return base::Failure<PackageOutput, Error>(compiled.error());
    return base::Success<PackageOutput, Error>(
        std::move(compiled.value().selectedPackage));
}

base::Expected<FrozenComputePackageEvidenceV1, Error>
InspectCanonicalComputePackageEvidenceV1(std::span<const std::byte> packageBytes)
{
    return d3d12::InspectFrozenComputePackageEvidenceV1(packageBytes);
}

}
