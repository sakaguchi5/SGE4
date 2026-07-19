#include "../00_Foundation/Base.h"
#include "../82_Spiral2Corpus/Spiral2Corpus.h"
#include "../84_HierarchyRepresentationPlanner/HierarchyRepresentationPlanner.h"
#include "../85_HierarchyRepresentationVerifier/HierarchyRepresentationVerifier.h"
#include "../86_Spiral2LeafCompiler/Spiral2LeafCompiler.h"
#include <filesystem>
#include <iostream>

namespace c=sge4_5::spiral2::candidate;namespace p=sge4_5::spiral2::planner;namespace v=sge4_5::spiral2::verification;namespace l=sge4_5::spiral2::leaf;namespace r=sge4_5::spiral2::corpus;namespace b=sge4_5::base;
int main(int argc,char**argv)
{
    auto corpus=r::BuildTopologyCorpusV1();if(!corpus)return 1;const auto& h=corpus.Value()[1].hierarchy;const std::array<c::CandidateKindV1,3> kinds{c::CandidateKindV1::MatrixHierarchy,c::CandidateKindV1::DirectPgaHierarchy,c::CandidateKindV1::HybridHierarchy};b::BinaryWriter evidence;std::uint32_t leaves=0;
    for(std::size_t index=0;index<kinds.size();++index){auto raw=p::PlanCandidateGraphV1(h,kinds[index]);if(!raw)return 2;auto verified=v::VerifyCandidateGraphV1(h,raw.Value());if(!verified)return 3;if(index==0){auto source=l::CompileDynamicInvocationSourceV1(h,verified.Value());if(!source){std::cerr<<source.Error().stage<<": "<<source.Error().message<<'\n';return 4;}if(!l::ValidateFrozenHierarchyLeafV1(source.Value())||source.Value().lockedDynamicBytes!=64||source.Value().lockedDynamicSlot!=0)return 4;const auto bytes=l::SerializeFrozenHierarchyLeafEvidenceV1(source.Value());evidence.WriteU32(static_cast<std::uint32_t>(bytes.size()));evidence.WriteBytes(bytes);++leaves;}auto group=l::CompileVerifiedHierarchyLeafGroupV1(h,verified.Value());if(!group){std::cerr<<group.Error().stage<<": "<<group.Error().message<<'\n';return 5;}const std::size_t expected=kinds[index]==c::CandidateKindV1::DirectPgaHierarchy?2:3;if(group.Value().leaves.size()!=expected)return 6;for(const auto& leaf:group.Value().leaves){if(!l::ValidateFrozenHierarchyLeafV1(leaf)||leaf.lockedDynamicSlot!=b::InvalidIndex)return 7;const auto bytes=l::SerializeFrozenHierarchyLeafEvidenceV1(leaf);evidence.WriteU32(static_cast<std::uint32_t>(bytes.size()));evidence.WriteBytes(bytes);++leaves;}}
    auto bytes=std::move(evidence).Take();if(argc==3&&std::string(argv[1])=="--emit"){auto wrote=b::WriteAllBytes(std::filesystem::path(argv[2]),bytes);if(!wrote)return 8;std::cout<<b::ToHex(b::Sha256(bytes))<<'\n';return 0;}std::cout<<"Spiral 2 vertical slice compiled "<<leaves<<" verifier-sealed Schema 17 hierarchy Leaves.\n";return leaves==9?0:9;
}
