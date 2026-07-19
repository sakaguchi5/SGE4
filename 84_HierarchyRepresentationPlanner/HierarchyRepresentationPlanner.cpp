#include "HierarchyRepresentationPlanner.h"
namespace sge4_5::spiral2::planner
{
base::Result<candidate::RawCandidateGraphV1,std::string> PlanCandidateGraphV1(const hierarchy::RigidHierarchySemanticV1& hierarchyValue,candidate::CandidateKindV1 kind)
{
    return candidate::BuildRawCandidateGraphV1(hierarchyValue,kind);
}
}
