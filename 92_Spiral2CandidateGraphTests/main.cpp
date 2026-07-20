#include "../00_Foundation/Base.h"
#include "../82_Spiral2Corpus/Spiral2Corpus.h"
#include "../84_HierarchyRepresentationPlanner/HierarchyRepresentationPlanner.h"
#include "../85_HierarchyRepresentationVerifier/HierarchyRepresentationVerifier.h"
#include <filesystem>
#include <iostream>

namespace c=sge4_5::spiral2::candidate;namespace p=sge4_5::spiral2::planner;namespace v=sge4_5::spiral2::verification;namespace r=sge4_5::spiral2::corpus;namespace b=sge4_5::base;
int main(int argc,char**argv)
{
    auto corpus=r::BuildTopologyCorpusV1();if(!corpus)return 1;b::BinaryWriter bundle;const std::array<c::CandidateKindV1,3> kinds{c::CandidateKindV1::MatrixHierarchy,c::CandidateKindV1::DirectPgaHierarchy,c::CandidateKindV1::HybridHierarchy};
    for(const auto& topology:corpus.Value())for(const auto kind:kinds)
    {
        auto raw=p::PlanCandidateGraphV1(topology.hierarchy,kind);if(!raw)return 2;auto sealed=v::VerifyCandidateGraphV1(topology.hierarchy,raw.Value());if(!sealed)return 3;const auto bytes=v::SerializeVerifiedHierarchyRepresentationV1(sealed.Value());if(bytes.empty()||sealed.Value().Plan().planIdentity==b::Digest256{})return 4;bundle.WriteU32(static_cast<std::uint32_t>(bytes.size()));bundle.WriteBytes(bytes);
        const auto& g=raw.Value();const auto& plan=sealed.Value().Plan();if(g.nodes.front().operation!=c::OperationKindV1::DynamicInvocationSource||g.nodes.front().primaryInputStride!=32||g.nodes.front().primaryOutputStride!=32||g.nodes.front().secondaryInputStride!=16)return 5;if(plan.operations.size()!=g.nodes.size()||plan.hierarchyExecutionModel!=c::HierarchyExecutionModelV1::SingleDispatchCanonicalOrderSerial)return 6;
        for(const auto& operation:plan.operations)if((operation.operation==c::OperationKindV1::MatrixHierarchy||operation.operation==c::OperationKindV1::MotorHierarchy)&&operation.dispatchX!=1)return 7;
        const std::uint32_t expected=kind==c::CandidateKindV1::DirectPgaHierarchy?2u:3u;if(g.proposedCandidateLeafDispatchCount!=expected||plan.candidateLeafDispatchCount!=expected)return 8;
        if(kind==c::CandidateKindV1::MatrixHierarchy&&(g.nodes.size()!=4||g.proposedLoweringPosition!=c::MatrixLoweringPositionV1::BeforeHierarchy))return 9;if(kind==c::CandidateKindV1::DirectPgaHierarchy&&(g.nodes.size()!=3||g.proposedLoweringPosition!=c::MatrixLoweringPositionV1::Absent))return 10;if(kind==c::CandidateKindV1::HybridHierarchy&&(g.nodes.size()!=4||g.proposedLoweringPosition!=c::MatrixLoweringPositionV1::AfterHierarchy))return 11;
    }
    auto bytes=std::move(bundle).Take();if(argc==3&&std::string(argv[1])=="--emit"){auto wrote=b::WriteAllBytes(std::filesystem::path(argv[2]),bytes);if(!wrote)return 12;std::cout<<b::ToHex(b::Sha256(bytes))<<'\n';return 0;}std::cout<<"Spiral 2 exact verified execution-plan tests passed: 27 structures; hierarchy execution is one canonical-order dispatch.\n";return 0;
}
