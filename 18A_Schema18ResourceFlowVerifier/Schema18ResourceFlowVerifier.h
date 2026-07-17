#pragma once

#include "../00_Foundation/Result.h"
#include "../00_Foundation/Sha256.h"
#include "../16A_Schema18CompositionContract/Schema18CompositionContract.h"
#include "../17A_Schema18ResourceFlowPlan/Schema18ResourceFlowPlan.h"

#include <string>
#include <utility>

namespace sge4::composition::schema18::verification
{
struct VerificationError final
{
    std::string stage;
    std::string message;
};

class VerifiedResourceFlowPlan final
{
public:
    VerifiedResourceFlowPlan(const VerifiedResourceFlowPlan&) = default;
    VerifiedResourceFlowPlan(VerifiedResourceFlowPlan&&) noexcept = default;
    VerifiedResourceFlowPlan& operator=(const VerifiedResourceFlowPlan&) = default;
    VerifiedResourceFlowPlan& operator=(VerifiedResourceFlowPlan&&) noexcept = default;

    [[nodiscard]] const planning::RawResourceFlowPlan& Plan() const noexcept
    {
        return plan_;
    }

    [[nodiscard]] const base::Digest256& Seal() const noexcept
    {
        return seal_;
    }

private:
    struct ConstructionToken final {};
    VerifiedResourceFlowPlan(planning::RawResourceFlowPlan plan,
                             base::Digest256 seal,
                             ConstructionToken)
        : plan_(std::move(plan)), seal_(seal) {}

    planning::RawResourceFlowPlan plan_;
    base::Digest256 seal_{};

    friend base::Result<VerifiedResourceFlowPlan, VerificationError>
    VerifyAndSeal(const PackageCompositionContract&,
                  const planning::RawResourceFlowPlan&);
};

[[nodiscard]] base::Digest256
ComputeVerifierSeal(const base::Digest256& contractIdentity,
                    const base::Digest256& rawPlanIdentity);

[[nodiscard]] base::Result<VerifiedResourceFlowPlan, VerificationError>
VerifyAndSeal(const PackageCompositionContract& contract,
              const planning::RawResourceFlowPlan& rawPlan);

[[nodiscard]] base::Result<void, VerificationError>
ValidateVerifiedPlan(const PackageCompositionContract& contract,
                     const VerifiedResourceFlowPlan& verified);
}
