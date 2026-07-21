#pragma once
#include "../103_ReuseRepresentationCandidate/ReuseRepresentationCandidate.h"
namespace sge4_5::spiral3::planner
{
[[nodiscard]] base::Result<candidate::RawCandidateGraphV1,std::string> PlanCandidateGraphV1(const spiral2::hierarchy::RigidHierarchySemanticV1& hierarchy,const semantic::ReuseSweepSemanticV1& semantic,candidate::CandidateKindV1 kind);
}
