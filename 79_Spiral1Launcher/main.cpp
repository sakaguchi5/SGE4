#include "../78_Spiral1PerformanceExperiment/Spiral1PerformanceExperiment.h"

#include "../00_Foundation/Sha256.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace perf = sge4_5::spiral1::performance;

namespace
{
std::string Value(int& index,int argc,char** argv)
{
    if (index + 1 >= argc)
        throw std::runtime_error(std::string("missing value for ") + argv[index]);
    return argv[++index];
}
std::uint32_t U32(std::string_view text)
{
    std::size_t used=0;const auto value=std::stoull(std::string(text),&used,10);if(used!=text.size()||value>0xffffffffull)throw std::runtime_error("invalid unsigned integer argument");return static_cast<std::uint32_t>(value);
}
}

int main(int argc,char** argv)
{
    try{
        bool measure=false,report=false,selfTest=false;std::filesystem::path output="build/tests/spiral1-cu6/measurement";std::filesystem::path input;perf::BenchmarkConfigV1 config;
        for(int i=1;i<argc;++i){const std::string arg=argv[i];if(arg=="--measure")measure=true;else if(arg=="--report")report=true;else if(arg=="--self-test")selfTest=true;else if(arg=="--output")output=Value(i,argc,argv);else if(arg=="--input")input=Value(i,argc,argv);else if(arg=="--warmup")config.warmupSamplesPerAlgorithm=U32(Value(i,argc,argv));else if(arg=="--measurement")config.measurementSamplesPerAlgorithm=U32(Value(i,argc,argv));else if(arg=="--runs")config.runCount=U32(Value(i,argc,argv));else if(arg=="--adapter-index")config.adapterIndex=U32(Value(i,argc,argv));else if(arg=="--clock-policy")config.clockPolicy=Value(i,argc,argv);else throw std::runtime_error("unknown argument: "+arg);}
        if(selfTest){auto result=perf::RunFormatSelfTestV1(output);if(!result)throw std::runtime_error(result.Error());std::cout<<"Spiral 1 CU6 format and deferred-decision self-test passed.\n";return 0;}
        if(measure){auto evidence=perf::RunRealGpuMeasurementV1(config);if(!evidence)throw std::runtime_error(evidence.Error());auto written=perf::WriteBenchmarkArtifactsV1(evidence.Value(),output);if(!written)throw std::runtime_error(written.Error());std::cout<<"Stage 13 real GPU ABBA measurement passed.\nAdapter: "<<evidence.Value().adapter.description<<"\nCases: "<<evidence.Value().cases.size()<<"\nEvidence: "<<(output/"benchmark_evidence_v1.bin").string()<<"\n";}
        if(report){if(input.empty())input=output/"benchmark_evidence_v1.bin";auto evidence=perf::ReadBenchmarkEvidenceV1(input);if(!evidence)throw std::runtime_error(evidence.Error());auto written=perf::WriteDecisionEvidenceReportV1(evidence.Value(),output);if(!written)throw std::runtime_error(written.Error());std::cout<<"Stage 14 decision evidence report generated.\nNext capability selection: DeferredByOwner\n";}
        if (!measure && !report)
            throw std::runtime_error("select --measure, --report, or --self-test");
        return 0;
    }catch(const std::exception& e){std::cerr<<"CU6 failure: "<<e.what()<<'\n';return 1;}
}
