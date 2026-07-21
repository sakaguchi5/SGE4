#include "ReuseRepresentationPlanner.h"
namespace sge4_5::spiral3::planner
{
base::Result<candidate::RawCandidateGraphV1,std::string> PlanCandidateGraphV1(const spiral2::hierarchy::RigidHierarchySemanticV1& h,const semantic::ReuseSweepSemanticV1& s,candidate::CandidateKindV1 k){return candidate::BuildRawCandidateGraphV1(h,s,k);}
}
