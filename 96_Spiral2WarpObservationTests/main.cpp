#include "../00_Foundation/Base.h"
#include "../14_D3D12Backend/D3D12Backend.h"
#include "../82_Spiral2Corpus/Spiral2Corpus.h"
#include "../88_Spiral2Scenarios/Spiral2Scenarios.h"
#include <algorithm>
#include <filesystem>
#include <iostream>

namespace b=sge4_5::base;
namespace c=sge4_5::spiral2::corpus;
namespace s=sge4_5::spiral2::scenarios;

namespace
{
std::vector<std::byte> OutputBytes(const s::HierarchyComparisonExecutionResultV1& value)
{
    b::BinaryWriter writer;
    const auto matrix=c::SerializeReferencePointsV1(value.matrix),direct=c::SerializeReferencePointsV1(value.direct),hybrid=c::SerializeReferencePointsV1(value.hybrid);
    writer.WriteU64(matrix.size());writer.WriteBytes(matrix);writer.WriteU64(direct.size());writer.WriteBytes(direct);writer.WriteU64(hybrid.size());writer.WriteBytes(hybrid);writer.WriteU64(value.gpuRecords.size());writer.WriteBytes(value.gpuRecords);return std::move(writer).Take();
}
std::vector<std::byte> BuildFrozenEvidence(const std::vector<c::TopologyCaseV1>& topologies)
{
    b::BinaryWriter writer;constexpr std::string_view domain="SGE4-5.Spiral2.CU5.FrozenCorpusEvidence.V1";writer.WriteU32(static_cast<std::uint32_t>(domain.size()));writer.WriteBytes(std::as_bytes(std::span(domain.data(),domain.size())));writer.WriteU32(static_cast<std::uint32_t>(topologies.size()));for(const auto& topology:topologies){auto scenario=s::BuildFrozenHierarchyComparisonScenarioV1(topology.hierarchy);if(!scenario)throw std::runtime_error(scenario.Error().stage+": "+scenario.Error().message);auto evidence=s::SerializeFrozenHierarchyComparisonScenarioEvidenceV1(scenario.Value());writer.WriteU32(static_cast<std::uint32_t>(topology.id.size()));writer.WriteBytes(std::as_bytes(std::span(topology.id.data(),topology.id.size())));writer.WriteU64(evidence.size());writer.WriteBytes(evidence);auto frames=c::BuildFrameCorpusV1(topology.hierarchy);if(!frames)throw std::runtime_error(frames.Error());writer.WriteU32(static_cast<std::uint32_t>(frames.Value().size()));for(const auto& frame:frames.Value()){auto palette=sge4_5::spiral2::contracts::SerializeDynamicLocalMotorPaletteV1(frame.palette);auto reference=c::BuildIndependentCpuReferenceV1(topology.hierarchy,frame.palette);if(!reference)throw std::runtime_error(reference.Error());auto referenceBytes=c::SerializeReferencePointsV1(reference.Value());writer.WriteU32(static_cast<std::uint32_t>(frame.id.size()));writer.WriteBytes(std::as_bytes(std::span(frame.id.data(),frame.id.size())));writer.WriteBytes(b::Sha256(palette));writer.WriteBytes(b::Sha256(referenceBytes));}}
    return std::move(writer).Take();
}
}

int main(int argc,char**argv)
{
    auto topologies=c::BuildTopologyCorpusV1();if(!topologies){std::cerr<<topologies.Error()<<'\n';return 1;}
    if(argc==3&&std::string(argv[1])=="--emit"){try{auto evidence=BuildFrozenEvidence(topologies.Value());auto wrote=b::WriteAllBytes(std::filesystem::path(argv[2]),evidence);if(!wrote)return 2;std::cout<<b::ToHex(b::Sha256(evidence))<<'\n';return 0;}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 3;}}
    const bool reorderOnly=argc==2&&std::string(argv[1])=="--reorder";std::size_t begin=0,end=topologies.Value().size();if(argc==3&&std::string(argv[1])=="--warp-index"){begin=std::stoul(argv[2]);if(begin>=end)return 15;end=begin+1;}else if(reorderOnly){end=0;}else if(argc!=1)return 16;
    std::uint32_t cases=0;double maxAbsolute=0,maxPairwise=0,maxRigidity=0;
    for(std::size_t topologyIndex=begin;topologyIndex<end;++topologyIndex){const auto& topology=topologies.Value()[topologyIndex];
        auto scenario=s::BuildFrozenHierarchyComparisonScenarioV1(topology.hierarchy);if(!scenario){std::cerr<<scenario.Error().stage<<": "<<scenario.Error().message<<'\n';return 4;}
        auto frames=c::BuildFrameCorpusV1(topology.hierarchy);if(!frames)return 5;
        sge4_5::d3d12::D3D12Backend backend({true,true});auto executed=s::ExecuteFrozenHierarchyCorpusScenarioV1(scenario.Value(),topology.hierarchy,frames.Value(),backend);if(!executed){std::cerr<<topology.id<<' '<<executed.Error().stage<<": "<<executed.Error().message<<'\n';return 6;}
        if(executed.Value().frames.size()!=12||executed.Value().frozenDigestBefore!=executed.Value().frozenDigestAfter)return 7;
        for(const auto& frame:executed.Value().frames){if(frame.observation.mismatchCount||frame.observation.nonFiniteCount||!frame.observation.orientationPreserved)return 8;maxAbsolute=std::max(maxAbsolute,frame.observation.maxAbsoluteError);maxPairwise=std::max(maxPairwise,frame.observation.maxPairwiseError);maxRigidity=std::max({maxRigidity,frame.observation.maxAxisLengthError,frame.observation.maxOrthogonalityError});++cases;}
        if(OutputBytes(executed.Value().frames[0])==OutputBytes(executed.Value().frames[4]))return 9;
        std::cout<<"[WARP] "<<topology.id<<" passed 12 consecutive frames.\n"<<std::flush;
    }
    if(argc==1||reorderOnly){const auto& reorderTopology=topologies.Value()[3];auto reorderScenario=s::BuildFrozenHierarchyComparisonScenarioV1(reorderTopology.hierarchy);auto reorderFrames=c::BuildFrameCorpusV1(reorderTopology.hierarchy);if(!reorderScenario||!reorderFrames)return 10;std::reverse(reorderFrames.Value().begin(),reorderFrames.Value().end());sge4_5::d3d12::D3D12Backend reorderBackend({true,true});auto reversed=s::ExecuteFrozenHierarchyCorpusScenarioV1(reorderScenario.Value(),reorderTopology.hierarchy,reorderFrames.Value(),reorderBackend,100);if(!reversed)return 11;std::reverse(reorderFrames.Value().begin(),reorderFrames.Value().end());sge4_5::d3d12::D3D12Backend normalBackend({true,true});auto normal=s::ExecuteFrozenHierarchyCorpusScenarioV1(reorderScenario.Value(),reorderTopology.hierarchy,reorderFrames.Value(),normalBackend,1000);if(!normal)return 12;for(std::size_t i=0;i<12;++i)if(OutputBytes(normal.Value().frames[i])!=OutputBytes(reversed.Value().frames[11-i]))return 13;std::cout<<"[WARP] frame-order permutation passed.\n";}
    std::cout<<"Spiral 2 WARP corpus passed: "<<cases<<" H/F cases, max-reference="<<maxAbsolute<<", max-pairwise="<<maxPairwise<<", max-rigidity="<<maxRigidity<<".\n";
    const auto expected=reorderOnly?0u:static_cast<std::uint32_t>((end-begin)*12);return cases==expected?0:14;
}
