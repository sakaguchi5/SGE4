#include "../00_Foundation/Base.h"
#include "../14_D3D12Backend/D3D12Backend.h"
#include "../19_CompositionVerifier/CompositionVerifier.h"
#include "../22_CompositionRuntime/CompositionRuntime.h"
#include "../82_Spiral2Corpus/Spiral2Corpus.h"
#include "../88_Spiral2Scenarios/Spiral2Scenarios.h"
#include <filesystem>
#include <iostream>

namespace r=sge4_5::spiral2::corpus;
namespace s=sge4_5::spiral2::scenarios;
namespace b=sge4_5::base;
namespace can=sge4_5::composition::canonical;
namespace runtime=sge4_5::composition::canonical::runtime_v1;

int main(int argc,char**argv)
{
    auto corpus=r::BuildTopologyCorpusV1();
    if(!corpus)return 1;
    const auto& hierarchy=corpus.Value()[1].hierarchy;
    auto scenario=s::BuildFrozenHierarchyComparisonScenarioV1(hierarchy);
    if(!scenario){std::cerr<<scenario.Error().stage<<": "<<scenario.Error().message<<'\n';return 2;}
    if(!s::ValidateFrozenHierarchyComparisonScenarioV1(scenario.Value()))return 3;
    auto bytes=s::SerializeFrozenHierarchyComparisonScenarioEvidenceV1(scenario.Value());
    if(argc==3&&std::string(argv[1])=="--emit"){
        auto wrote=b::WriteAllBytes(std::filesystem::path(argv[2]),bytes);
        if(!wrote)return 4;
        std::cout<<b::ToHex(b::Sha256(bytes))<<'\n';
        return 0;
    }
    auto frames=r::BuildFrameCorpusV1(hierarchy);
    if(!frames)return 5;
    auto frozen=can::verification::ReadVerifiedFrozenComposition(scenario.Value().frozenCompositionBytes);
    if(!frozen)return 6;
    can::LeafPackageId source;
    const auto sourceKey=can::ComputeStableLeafKey(scenario.Value().leaves[0].role);
    for(const auto& leaf:frozen.Value().ValidatedContract().Contract().leaves)if(leaf.stableKey==sourceKey)source=leaf.id;
    if(!source.IsValid())return 7;
    auto reference=r::BuildIndependentCpuReferenceV1(hierarchy,frames.Value()[4].palette);
    if(!reference)return 8;
    auto shortReference=r::SerializeReferencePointsV1(reference.Value());
    shortReference.pop_back();
    sge4_5::d3d12::D3D12Backend negativeBackend({true,true});
    auto negativeLoaded=runtime::LoadStaticComposition(scenario.Value().frozenCompositionBytes,negativeBackend);
    if(!negativeLoaded)return 9;
    runtime::StaticCompositionFrameInvocation invalid;
    invalid.dynamicData={{source,scenario.Value().leaves[0].lockedDynamicSlot,sge4_5::spiral2::contracts::SerializeDynamicLocalMotorPaletteV1(frames.Value()[4].palette)},{source,scenario.Value().leaves[0].lockedReferenceSlot,std::move(shortReference)}};
    if(runtime::SubmitStaticComposition(negativeLoaded.Value(),invalid))return 10;
    sge4_5::d3d12::D3D12Backend backend({true,true});
    auto executed=s::ExecuteFrozenHierarchyComparisonScenarioV1(scenario.Value(),hierarchy,frames.Value()[4].palette,backend,0);
    if(!executed){std::cerr<<executed.Error().stage<<": "<<executed.Error().message<<'\n';return 11;}
    if(executed.Value().observation.mismatchCount!=0||executed.Value().deviceEpoch==0)return 12;
    std::cout<<"Spiral 2 ten-Leaf verified Frozen comparison Composition passed on WARP.\n";
    return 0;
}
