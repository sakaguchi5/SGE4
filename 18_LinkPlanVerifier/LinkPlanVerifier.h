#pragma once

#include "../00_Foundation/Result.h"
#include "../17_LinkPlanModel/LinkPlanModel.h"

#include <string>

namespace sge4::composition
{
struct LinkVerificationError final { std::string stage; std::string message; };

class VerifiedLinkPlan final
{
public:
    VerifiedLinkPlan(VerifiedLinkPlan&&) noexcept = default;
    VerifiedLinkPlan& operator=(VerifiedLinkPlan&&) noexcept = default;
    VerifiedLinkPlan(const VerifiedLinkPlan&) = delete;
    VerifiedLinkPlan& operator=(const VerifiedLinkPlan&) = delete;
    [[nodiscard]] const LinkPlanIR& Plan() const noexcept { return plan_; }
private:
    friend base::Result<VerifiedLinkPlan, LinkVerificationError> VerifyAndSeal(
        const PackageCompositionContract&, LinkPlanIR);
    explicit VerifiedLinkPlan(LinkPlanIR plan) : plan_(std::move(plan)) {}
    LinkPlanIR plan_;
};

[[nodiscard]] base::Result<VerifiedLinkPlan, LinkVerificationError> VerifyAndSeal(
    const PackageCompositionContract& contract,
    LinkPlanIR plan);
}
