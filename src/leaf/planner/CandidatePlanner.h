#pragma once

#include "../model/compilation/CompilationInput.h"
#include "../model/plan/ExecutionPlanModel.h"
#include "../../canonical/base/Sha256.h"

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
    bool verifierAccepted = false;
    std::uint32_t verifierViolationCount = 0;
    planning::CostVector cost;

    // Filled only by LeafCompiler after a verifier-sealed Plan is lowered.
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

// Pure planning phase: validates the source/target input and generates raw candidates.
// It has no verifier dependency and cannot create a Verified Plan or Package artifact.
[[nodiscard]] base::Expected<PlanningCandidateSet, PlanningError> PlanCandidates(
    const semantic::SemanticGraph& graph,
    const target::D3D12TargetProfile& targetProfile,
    const planning::CompilerPolicy& policy);

// Selection phase: LeafCompiler first fills Package execution digests for the
// verifier-sealed candidates it actually lowered, then asks the planner to make
// the deterministic policy/profile selection and freeze the planning manifest.
[[nodiscard]] base::Expected<PlanningSelection, PlanningError> SelectCandidate(
    PlanningCandidateSet candidateSet,
    const planning::CompilerPolicy& policy,
    const planning::ProfileRecord* profile = nullptr,
    const planning::ProfileSelectionContext* profileContext = nullptr);
}
