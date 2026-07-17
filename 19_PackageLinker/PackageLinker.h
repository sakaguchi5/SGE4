#pragma once

#include "../00_Foundation/Result.h"
#include "../17_LinkPlanModel/LinkPlanModel.h"

#include <string>

namespace sge4::composition
{
struct LinkerError final { std::string stage; std::string message; };
[[nodiscard]] base::Result<LinkPlanIR, LinkerError> ProposeLinkPlan(const PackageCompositionContract& contract);
}
