#include "../89_Spiral2PerformanceExperiment/Spiral2PerformanceExperiment.h"
#include "../82_Spiral2Corpus/Spiral2Corpus.h"
#include "../88_Spiral2Scenarios/Spiral2Scenarios.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace perf=sge4_5::spiral2::performance;
namespace corpus=sge4_5::spiral2::corpus;
namespace scenarios=sge4_5::spiral2::scenarios;

namespace
{
std::string Value(int& index,int argc,char** argv){if(index+1>=argc)throw std::runtime_error(std::string("missing value for ")+argv[index]);return argv[++index];}
std::uint32_t U32(std::string_view text){std::size_t used=0;const auto value=std::stoull(std::string(text),&used,10);if(used!=text.size()||value>0xffffffffull)throw std::runtime_error("invalid unsigned integer argument");return static_cast<std::uint32_t>(value);}

void RunWarpSmoke()
{
    auto topologies=corpus::BuildTopologyCorpusV1();if(!topologies)throw std::runtime_error(topologies.Error());auto frames=corpus::BuildFrameCorpusV1(topologies.Value()[0].hierarchy);if(!frames)throw std::runtime_error(frames.Error());auto scenario=scenarios::BuildFrozenHierarchyComparisonScenarioV1(topologies.Value()[0].hierarchy);if(!scenario)throw std::runtime_error(scenario.Error().stage+": "+scenario.Error().message);sge4_5::d3d12::D3D12Backend backend({true,false,false});auto result=scenarios::ExecuteFrozenHierarchyComparisonScenarioV1(scenario.Value(),topologies.Value()[0].hierarchy,frames.Value()[0].palette,backend,0);if(!result)throw std::runtime_error(result.Error().stage+": "+result.Error().message);std::cout<<"Spiral 2 launcher WARP smoke passed: H00/F00.\n";
}
}

int main(int argc,char** argv)
{
    try{
        bool selfTest=false,warp=false,measure=false,evidence=false,report=false;std::filesystem::path output="build/tests/spiral2/measurement";std::filesystem::path input;perf::MeasurementConfigV1 config;
        for(int i=1;i<argc;++i){const std::string arg=argv[i];if(arg=="--self-test")selfTest=true;else if(arg=="--warp")warp=true;else if(arg=="--measure")measure=true;else if(arg=="--evidence")evidence=true;else if(arg=="--report")report=true;else if(arg=="--output")output=Value(i,argc,argv);else if(arg=="--input")input=Value(i,argc,argv);else if(arg=="--warmup-cycles")config.warmupCyclesPerOrder=U32(Value(i,argc,argv));else if(arg=="--measurement-cycles")config.measurementCyclesPerOrder=U32(Value(i,argc,argv));else if(arg=="--runs")config.runCount=U32(Value(i,argc,argv));else if(arg=="--adapter-index")config.adapterIndex=U32(Value(i,argc,argv));else if(arg=="--clock-policy")config.clockPolicy=Value(i,argc,argv);else throw std::runtime_error("unknown argument: "+arg);}
        if(input.empty())input=output/"spiral2_measurement_evidence_v2.bin";
        if(selfTest){auto result=perf::RunFormatSelfTestV1(output);if(!result)throw std::runtime_error(result.Error());std::cout<<"Spiral 2 CU6 format and Owner-deferred decision self-test passed.\n";}
        if(warp)RunWarpSmoke();
        if(measure){auto measured=perf::RunRealGpuMeasurementV1(config);if(!measured)throw std::runtime_error(measured.Error());auto written=perf::WriteMeasurementArtifactsV1(measured.Value(),output);if(!written)throw std::runtime_error(written.Error());std::cout<<"Spiral 2 S2-20 real GPU measurement passed.\nAdapter: "<<measured.Value().adapter.description<<"\nCases: "<<measured.Value().cases.size()<<"\nEvidence: "<<(output/"spiral2_measurement_evidence_v2.bin").string()<<'\n';}
        if(evidence){auto loaded=perf::ReadMeasurementEvidenceV1(input);if(!loaded)throw std::runtime_error(loaded.Error());std::cout<<"Spiral 2 binary evidence validated. Cases: "<<loaded.Value().cases.size()<<"\n";}
        if(report){auto loaded=perf::ReadMeasurementEvidenceV1(input);if(!loaded)throw std::runtime_error(loaded.Error());auto written=perf::WriteDecisionEvidenceReportV1(loaded.Value(),output);if(!written)throw std::runtime_error(written.Error());std::cout<<"Spiral 2 S2-21 Decision Evidence generated.\nNextCapabilitySelection = DeferredByOwner\nSelectionStatus = OWNER_DECISION_REQUIRED\n";}
        if(!selfTest&&!warp&&!measure&&!evidence&&!report)throw std::runtime_error("select --self-test, --warp, --measure, --evidence, or --report");
        return 0;
    }catch(const std::exception& error){std::cerr<<"Spiral 2 launcher failure: "<<error.what()<<'\n';return 1;}
}
