#pragma once

#include "../../canonical/base/Expected.h"
#include "../model/plan/ExecutionPlanModel.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace sge4::planning::verification
{
enum class DiagnosticCode : std::uint32_t
{
    None = 0,
    PlanIdentityMismatch,
    MissingWork,
    DuplicateWork,
    UnknownWork,
    DependencyOrderViolation,
    MissingQueueAssignment,
    DuplicateQueueAssignment,
    QueueCapabilityViolation,
    MissingSynchronization,
    InvalidSynchronization,
    DuplicateResourcePlan,
    ResourceInstanceViolation,
    LifetimeViolation,
    AllocationCoverageViolation,
    AllocationAliasViolation,
    StatePlanViolation,
    BindingPlanViolation,
    BoundaryViolation,
    ArtifactCardinalityViolation,
    ObligationIdentityMismatch,
    PlanningContractIdentityMismatch
};

struct Violation final
{
    DiagnosticCode code = DiagnosticCode::None;
    std::string stage;
    std::string message;
    std::uint32_t work = base::InvalidIndex;
    std::uint32_t resource = base::InvalidIndex;
    std::uint32_t use = base::InvalidIndex;
};

struct VerificationReport final
{
    bool verified = false;
    std::vector<Violation> violations;
};

class VerifiedExecutionPlan final
{
public:
    VerifiedExecutionPlan(const VerifiedExecutionPlan&) = default;
    VerifiedExecutionPlan(VerifiedExecutionPlan&&) noexcept = default;
    VerifiedExecutionPlan& operator=(const VerifiedExecutionPlan&) = default;
    VerifiedExecutionPlan& operator=(VerifiedExecutionPlan&&) noexcept = default;

    [[nodiscard]] const ExecutionPlanIR& Plan() const noexcept { return plan_; }
    [[nodiscard]] const PlanVerificationCertificate& Certificate() const noexcept { return certificate_; }

private:
    VerifiedExecutionPlan(ExecutionPlanIR plan, PlanVerificationCertificate certificate)
        : plan_(std::move(plan)), certificate_(std::move(certificate)) {}

    ExecutionPlanIR plan_;
    PlanVerificationCertificate certificate_;

    friend base::Expected<VerifiedExecutionPlan, VerificationReport> VerifyAndSeal(
        const SemanticObligation& obligation,
        const D3D12PlanningContract& contract,
        const ExecutionPlanIR& plan);
};

[[nodiscard]] VerificationReport Verify(
    const SemanticObligation& obligation,
    const D3D12PlanningContract& contract,
    const ExecutionPlanIR& plan);

[[nodiscard]] base::Expected<VerifiedExecutionPlan, VerificationReport> VerifyAndSeal(
    const SemanticObligation& obligation,
    const D3D12PlanningContract& contract,
    const ExecutionPlanIR& plan);
}
