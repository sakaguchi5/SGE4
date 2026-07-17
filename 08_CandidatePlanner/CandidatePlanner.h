#pragma once

#include "../05A_CompilationInput/CompilationInput.h"
#include "../07_ExecutionPlanVerifier/ExecutionPlanVerifier.h"

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace sge4::compiler::d3d12::candidate
{
using PlanningError = compilation::CompilationError;

struct CandidateRecord final
{
    planning::ExecutionPlanIR plan;
    planning::verification::VerificationReport verification;
    planning::CostVector cost;

    // Filled only by SGE4Compiler after a verifier-sealed Plan is lowered.
    // CandidatePlanner never depends on Package schema or Package lowering.
    std::string packageExecutionDigestHex;
};

struct CandidateManifest final
{
    std::uint32_t version = 1;
    base::Digest256 obligationDigest{};
    base::Digest256 planningContractDigest{};
    base::Digest256 policyDigest{};
    planning::CandidateBudget budget;
    std::vector<CandidateRecord> candidates;
    base::Digest256 selectedPlanIdentity{};
    std::string fallbackReason;
    std::vector<std::byte> canonicalBytes;
};

struct PlanningCandidateSet final
{
    planning::SemanticObligation obligation;
    planning::D3D12PlanningContract planningContract;
    CandidateManifest manifest;
};

struct PlanningSelection final
{
    std::size_t selectedCandidateIndex = 0;
    planning::ExecutionPlanIR selectedPlan;
    CandidateManifest manifest;
};

[[nodiscard]] planning::ExecutionPlanIR BuildCanonicalSafePlan(
    const planning::SemanticObligation& obligation,
    const planning::D3D12PlanningContract& contract);

[[nodiscard]] std::vector<planning::ExecutionPlanIR> GenerateCandidatePlans(
    const planning::SemanticObligation& obligation,
    const planning::D3D12PlanningContract& contract,
    const planning::CompilerPolicy& policy);

[[nodiscard]] planning::CostVector CalculateCost(
    const planning::SemanticObligation& obligation,
    const planning::ExecutionPlanIR& plan);

[[nodiscard]] std::vector<std::size_t> ParetoFrontier(
    std::span<const CandidateRecord> candidates);

// Pure planning phase: validates the source/target input, generates candidates,
// and independently verifies them. It does not create any Package artifacts.
[[nodiscard]] base::Result<PlanningCandidateSet, PlanningError> PlanCandidates(
    const semantic::SemanticGraph& graph,
    const target::D3D12TargetProfile& targetProfile,
    const planning::CompilerPolicy& policy);

// Selection phase: SGE4Compiler first fills Package execution digests for the
// verifier-sealed candidates it actually lowered, then asks the planner to make
// the deterministic policy/profile selection and freeze the planning manifest.
[[nodiscard]] base::Result<PlanningSelection, PlanningError> SelectCandidate(
    PlanningCandidateSet candidateSet,
    const planning::CompilerPolicy& policy,
    const planning::ProfileRecord* profile = nullptr,
    const planning::ProfileSelectionContext* profileContext = nullptr);
}
