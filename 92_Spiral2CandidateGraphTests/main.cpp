#include "../00_Foundation/Base.h"
#include "../82_Spiral2Corpus/Spiral2Corpus.h"
#include "../84_HierarchyRepresentationPlanner/HierarchyRepresentationPlanner.h"
#include "../85_HierarchyRepresentationVerifier/HierarchyRepresentationVerifier.h"
#include <filesystem>
#include <iostream>

namespace c=sge4_5::spiral2::candidate;namespace p=sge4_5::spiral2::planner;namespace v=sge4_5::spiral2::verification;namespace r=sge4_5::spiral2::corpus;namespace b=sge4_5::base;
int main(int argc,char**argv)
{
    auto corpus=r::BuildTopologyCorpusV1();if(!corpus)return 1;b::BinaryWriter bundle;
    const std::array<c::CandidateKindV1,3> kinds{c::CandidateKindV1::MatrixHierarchy,c::CandidateKindV1::DirectPgaHierarchy,c::CandidateKindV1::HybridHierarchy};
    for(const auto& topology:corpus.Value())for(const auto kind:kinds)
    {
        auto raw=p::PlanCandidateGraphV1(topology.hierarchy,kind);if(!raw)return 2;
        auto sealed=v::VerifyCandidateGraphV1(topology.hierarchy,raw.Value());if(!sealed)return 3;
        const auto bytes=v::SerializeVerifiedHierarchyRepresentationV1(sealed.Value());if(bytes.empty())return 4;bundle.WriteU32(static_cast<std::uint32_t>(bytes.size()));bundle.WriteBytes(bytes);
        const auto& g=raw.Value();if(g.nodes.front().operation!=c::OperationKindV1::DynamicInvocationSource||g.nodes.front().inputStride!=32||g.nodes.front().outputStride!=32)return 5;
        if(kind==c::CandidateKindV1::MatrixHierarchy&&(g.nodes.size()!=4||g.proposedLoweringPosition!=c::MatrixLoweringPositionV1::BeforeHierarchy))return 6;
        if(kind==c::CandidateKindV1::DirectPgaHierarchy&&(g.nodes.size()!=3||g.proposedLoweringPosition!=c::MatrixLoweringPositionV1::Absent))return 7;
        if(kind==c::CandidateKindV1::HybridHierarchy&&(g.nodes.size()!=4||g.proposedLoweringPosition!=c::MatrixLoweringPositionV1::AfterHierarchy))return 8;
    }
    auto bytes=std::move(bundle).Take();if(argc==3&&std::string(argv[1])=="--emit"){auto wrote=b::WriteAllBytes(std::filesystem::path(argv[2]),bytes);if(!wrote)return 9;std::cout<<b::ToHex(b::Sha256(bytes))<<'\n';return 0;}
    std::cout<<"Spiral 2 candidate graph and independent seal tests passed: 27 verified structures.\n";return 0;
}
