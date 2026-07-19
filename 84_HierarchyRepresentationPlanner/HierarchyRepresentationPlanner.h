#pragma once
#include "../83_HierarchyRepresentationCandidate/HierarchyRepresentationCandidate.h"
namespace sge4_5::spiral2::planner
{
[[nodiscard]] base::Result<candidate::RawCandidateGraphV1,std::string> PlanCandidateGraphV1(const hierarchy::RigidHierarchySemanticV1& hierarchy,candidate::CandidateKindV1 kind);
}
