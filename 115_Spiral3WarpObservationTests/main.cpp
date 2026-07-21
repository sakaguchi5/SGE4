#include "../00_Foundation/Base.h"
#include "../14_D3D12Backend/D3D12Backend.h"
#include "../82_Spiral2Corpus/Spiral2Corpus.h"
#include "../102_Spiral3Corpus/Spiral3Corpus.h"
#include "../107_Spiral3Scenarios/Spiral3Scenarios.h"
#include "../108_Spiral3Execution/Spiral3Execution.h"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <limits>

namespace b=sge4_5::base;
namespace s2c=sge4_5::spiral2::corpus;
namespace c=sge4_5::spiral3::corpus;
namespace s=sge4_5::spiral3::scenarios;
namespace e=sge4_5::spiral3::execution;

namespace
{
int Failure(int code,const std::string& stage,const std::string& message)
{
    std::cerr<<"Spiral 3 CU5 WARP failure ["<<code<<"] "<<stage<<": "<<message<<'\n';
    return code;
}

std::vector<std::byte> BuildCaseEvidence(
    const c::ReuseCaseV1& reuseCase,
    const s::FrozenReuseComparisonScenarioV1& scenario,
    std::span<const s2c::FrameCaseV1> frames,
    const e::ReuseCorpusExecutionResultV1& executed)
{
    b::BinaryWriter writer;
    constexpr std::string_view domain="SGE4-5.Spiral3.CU5.WarpCaseEvidence.V1";
    writer.WriteU32(static_cast<std::uint32_t>(domain.size()));
    writer.WriteBytes(std::as_bytes(std::span(domain.data(),domain.size())));
    writer.WriteU32(static_cast<std::uint32_t>(reuseCase.id.size()));
    writer.WriteBytes(std::as_bytes(std::span(reuseCase.id.data(),reuseCase.id.size())));
    writer.WriteU32(reuseCase.reuseCount);
    writer.WriteU32(reuseCase.semantic.pointCount);
    writer.WriteBytes(executed.frozenDigestBefore);
    writer.WriteBytes(executed.frozenDigestAfter);
    auto scenarioBytes=s::SerializeFrozenReuseComparisonScenarioEvidenceV1(scenario);
    writer.WriteU64(scenarioBytes.size());
    writer.WriteBytes(scenarioBytes);
    writer.WriteU32(static_cast<std::uint32_t>(executed.frames.size()));
    for(std::size_t i=0;i<executed.frames.size();++i)
    {
        writer.WriteU32(static_cast<std::uint32_t>(frames[i].id.size()));
        writer.WriteBytes(std::as_bytes(std::span(frames[i].id.data(),frames[i].id.size())));
        auto palette=sge4_5::spiral2::contracts::SerializeDynamicLocalMotorPaletteV1(frames[i].palette);
        writer.WriteBytes(b::Sha256(palette));
        auto evidence=e::SerializeReuseExecutionEvidenceV1(scenario,executed.frames[i]);
        writer.WriteU64(evidence.size());
        writer.WriteBytes(evidence);
    }
    return std::move(writer).Take();
}

bool WriteEvidence(const std::filesystem::path& path,std::span<const std::byte> bytes)
{
    auto wrote=b::WriteAllBytes(path,bytes);
    if(!wrote)return false;
    std::cout<<b::ToHex(b::Sha256(bytes))<<'\n';
    return true;
}

int ExecuteCase(std::size_t index,const std::filesystem::path* output)
{
    auto hierarchy=c::BuildCanonicalMeasurementHierarchyV1();
    if(!hierarchy)return Failure(1,"hierarchy",hierarchy.Error());
    auto cases=c::BuildReuseCorpusV1(hierarchy.Value());
    if(!cases)return Failure(2,"reuse-corpus",cases.Error());
    if(index>=cases.Value().size())return Failure(3,"arguments","reuse index is outside the canonical corpus");
    const auto& reuseCase=cases.Value()[index];
    auto frames=s2c::BuildFrameCorpusV1(hierarchy.Value());
    if(!frames)return Failure(4,"frame-corpus",frames.Error());
    auto scenario=s::BuildFrozenReuseComparisonScenarioV1(hierarchy.Value(),reuseCase);
    if(!scenario)return Failure(5,scenario.Error().stage,scenario.Error().message);
    sge4_5::d3d12::D3D12Backend backend({true,true});
    auto executed=e::ExecuteFrozenReuseCorpusScenarioV1(
        scenario.Value(),hierarchy.Value(),reuseCase,frames.Value(),backend);
    if(!executed)return Failure(6,reuseCase.id+"/"+executed.Error().stage,executed.Error().message);
    if(executed.Value().frames.size()!=12)
        return Failure(7,reuseCase.id,"execution did not produce exactly twelve frame observations");
    if(executed.Value().frozenDigestBefore!=executed.Value().frozenDigestAfter||
       executed.Value().frozenDigestBefore!=scenario.Value().frozenCompositionDigest)
        return Failure(8,reuseCase.id,"Frozen Composition bytes changed across dynamic palettes");

    double maxReference=0,maxPairwise=0;
    std::uint64_t maxUlp=0,maxPairwiseUlp=0;
    for(std::size_t frameIndex=0;frameIndex<executed.Value().frames.size();++frameIndex)
    {
        const auto& report=executed.Value().frames[frameIndex].observation;
        if(report.mismatchCount||report.nonFiniteCount||
           report.firstMismatchIndex!=std::numeric_limits<std::uint64_t>::max())
            return Failure(9,reuseCase.id+"/F"+std::to_string(frameIndex),"observation report retained a mismatch or non-finite point");
        if((reuseCase.reuseCount>=4)!=report.rigidityQualified||
           !report.orientationPreserved||!(report.minimumDeterminant>0))
            return Failure(10,reuseCase.id+"/F"+std::to_string(frameIndex),"rigidity or orientation qualification failed");
        maxReference=std::max(maxReference,report.maxAbsoluteError);
        maxPairwise=std::max(maxPairwise,report.maxPairwiseError);
        maxUlp=std::max(maxUlp,report.maxUlpDistance);
        maxPairwiseUlp=std::max(maxPairwiseUlp,report.maxPairwiseUlpDistance);
    }
    if(e::SerializeReuseExecutionEvidenceV1(scenario.Value(),executed.Value().frames[0])==
       e::SerializeReuseExecutionEvidenceV1(scenario.Value(),executed.Value().frames[4]))
        return Failure(11,reuseCase.id,"distinct frame palettes produced identical execution evidence");
    auto evidence=BuildCaseEvidence(reuseCase,scenario.Value(),frames.Value(),executed.Value());
    if(output&&!WriteEvidence(*output,evidence))
        return Failure(12,reuseCase.id,"could not write WARP case evidence");
    std::cout<<"[WARP] "<<reuseCase.id<<" points="<<reuseCase.semantic.pointCount
             <<" passed 12 frames; max-reference="<<maxReference
             <<", max-pairwise="<<maxPairwise<<", max-ulp="<<maxUlp
             <<", max-pairwise-ulp="<<maxPairwiseUlp<<".\n";
    return 0;
}

int Reorder()
{
    auto hierarchy=c::BuildCanonicalMeasurementHierarchyV1();
    if(!hierarchy)return Failure(20,"reorder/hierarchy",hierarchy.Error());
    auto cases=c::BuildReuseCorpusV1(hierarchy.Value());
    if(!cases)return Failure(21,"reorder/reuse-corpus",cases.Error());
    const auto& reuseCase=cases.Value()[3];
    auto frames=s2c::BuildFrameCorpusV1(hierarchy.Value());
    if(!frames)return Failure(22,"reorder/frame-corpus",frames.Error());
    auto scenario=s::BuildFrozenReuseComparisonScenarioV1(hierarchy.Value(),reuseCase);
    if(!scenario)return Failure(23,scenario.Error().stage,scenario.Error().message);
    auto reversedFrames=frames.Value();
    std::reverse(reversedFrames.begin(),reversedFrames.end());
    sge4_5::d3d12::D3D12Backend reverseBackend({true,true});
    auto reversed=e::ExecuteFrozenReuseCorpusScenarioV1(
        scenario.Value(),hierarchy.Value(),reuseCase,reversedFrames,reverseBackend,100);
    if(!reversed)return Failure(24,"reorder/reversed/"+reversed.Error().stage,reversed.Error().message);
    sge4_5::d3d12::D3D12Backend normalBackend({true,true});
    auto normal=e::ExecuteFrozenReuseCorpusScenarioV1(
        scenario.Value(),hierarchy.Value(),reuseCase,frames.Value(),normalBackend,1000);
    if(!normal)return Failure(25,"reorder/normal/"+normal.Error().stage,normal.Error().message);
    for(std::size_t i=0;i<12;++i)
    {
        if(e::SerializeReuseExecutionEvidenceV1(scenario.Value(),normal.Value().frames[i])!=
           e::SerializeReuseExecutionEvidenceV1(scenario.Value(),reversed.Value().frames[11-i]))
            return Failure(26,"reorder/R64","frame-order permutation changed per-palette execution evidence");
    }
    std::cout<<"[WARP] R64 frame-order permutation passed.\n";
    return 0;
}
}

int main(int argc,char**argv)
{
    try
    {
        if(argc==2&&std::string(argv[1])=="--reorder")return Reorder();
        if((argc==3||argc==5)&&std::string(argv[1])=="--warp-index")
        {
            const auto index=static_cast<std::size_t>(std::stoul(argv[2]));
            if(argc==3)return ExecuteCase(index,nullptr);
            if(std::string(argv[3])!="--emit")return Failure(63,"arguments","expected --emit after --warp-index");
            const std::filesystem::path output=argv[4];
            return ExecuteCase(index,&output);
        }
        if(argc==1)
        {
            for(std::size_t i=0;i<6;++i)
            {
                const int result=ExecuteCase(i,nullptr);
                if(result)return result;
            }
            return Reorder();
        }
        return Failure(64,"arguments","unsupported command line");
    }
    catch(const std::exception& error)
    {
        return Failure(65,"exception",error.what());
    }
}
