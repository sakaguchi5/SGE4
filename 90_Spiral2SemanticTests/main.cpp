#include "../00_Foundation/Base.h"
#include "../80_HierarchySemantic/HierarchySemantic.h"
#include "../82_Spiral2Corpus/Spiral2Corpus.h"

#include <filesystem>
#include <iostream>
#include <limits>

namespace h=sge4_5::spiral2::hierarchy;
namespace c=sge4_5::spiral2::corpus;
namespace b=sge4_5::base;

int main(int argc,char** argv)
{
    auto corpus=c::BuildTopologyCorpusV1();
    if(!corpus||corpus.Value().size()!=9) return 1;
    const std::array<std::uint32_t,9> counts{1,2,8,8,15,32,64,64,128};
    for(std::size_t i=0;i<counts.size();++i)
    {
        const auto& value=corpus.Value()[i].hierarchy;
        if(value.boneCount!=counts[i]||!h::ValidateRigidHierarchySemanticV1(value)) return 2;
        auto rebuilt=h::BuildRigidHierarchySemanticV1(value.parentIndices);
        if(!rebuilt||h::SerializeRigidHierarchySemanticV1(rebuilt.Value())!=h::SerializeRigidHierarchySemanticV1(value)) return 3;
    }
    if(h::BuildRigidHierarchySemanticV1(std::span<const std::int32_t>{})) return 4;
    const std::array<std::int32_t,2> range{-1,2}, self{-1,1}, roots{-1,-1}, cycle{1,0};
    if(h::BuildRigidHierarchySemanticV1(range)||h::BuildRigidHierarchySemanticV1(self)||
       h::BuildRigidHierarchySemanticV1(roots)||h::BuildRigidHierarchySemanticV1(cycle)) return 5;
    const std::array<std::int32_t,2> parentAfterChild{1,-1};
    auto reordered=h::BuildRigidHierarchySemanticV1(parentAfterChild);
    if(!reordered||reordered.Value().rootBone!=1u||
       reordered.Value().canonicalBoneOrder!=std::vector<std::uint32_t>{1u,0u}||
       !h::ValidateRigidHierarchySemanticV1(reordered.Value())) return 6;
    auto changed=corpus.Value()[4].hierarchy;
    std::swap(changed.canonicalBoneOrder[1],changed.canonicalBoneOrder[2]);
    if(h::ValidateRigidHierarchySemanticV1(changed)) return 7;
    changed=corpus.Value()[4].hierarchy; ++changed.depthOffsets[1];
    if(h::ValidateRigidHierarchySemanticV1(changed)) return 8;
    changed=corpus.Value()[4].hierarchy; changed.observationContractIdentity={};
    if(h::ValidateRigidHierarchySemanticV1(changed)) return 9;
    if(h::RigidHierarchySemanticIdentityV1(corpus.Value()[2].hierarchy)==h::RigidHierarchySemanticIdentityV1(corpus.Value()[3].hierarchy)) return 10;
    if(argc==3&&std::string(argv[1])=="--emit")
    {
        b::BinaryWriter writer;
        for(const auto& item:corpus.Value()){const auto bytes=h::SerializeRigidHierarchySemanticV1(item.hierarchy);writer.WriteU32(static_cast<std::uint32_t>(bytes.size()));writer.WriteBytes(bytes);}
        auto bytes=std::move(writer).Take(); auto result=b::WriteAllBytes(std::filesystem::path(argv[2]),bytes); if(!result)return 11;
        std::cout<<b::ToHex(b::Sha256(bytes))<<'\n'; return 0;
    }
    std::cout<<"Spiral 2 canonical hierarchy semantic tests passed: H00-H08 and negative authority gates.\n";
    return 0;
}
