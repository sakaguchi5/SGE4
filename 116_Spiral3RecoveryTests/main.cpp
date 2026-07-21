#include "../00_Foundation/Base.h"
#include "../14_D3D12Backend/D3D12Backend.h"
#include "../22_CompositionRuntime/CompositionRuntime.h"
#include "../23_CompositionRecovery/CompositionRecovery.h"
#include "../82_Spiral2Corpus/Spiral2Corpus.h"
#include "../102_Spiral3Corpus/Spiral3Corpus.h"
#include "../107_Spiral3Scenarios/Spiral3Scenarios.h"
#include "../108_Spiral3Execution/Spiral3Execution.h"
#include <filesystem>
#include <iostream>

namespace b=sge4_5::base;
namespace c=sge4_5::spiral3::corpus;
namespace s=sge4_5::spiral3::scenarios;
namespace e=sge4_5::spiral3::execution;
namespace runtime=sge4_5::composition::canonical::runtime_v1;

namespace
{
int Failure(int code,const std::string& stage,const std::string& message)
{
    std::cerr<<"Spiral 3 CU5 recovery failure ["<<code<<"] "<<stage<<": "<<message<<'\n';
    return code;
}

bool WriteEvidence(const std::filesystem::path& path,std::span<const std::byte> evidence)
{
    auto wrote=b::WriteAllBytes(path,evidence);
    if(!wrote)return false;
    std::cout<<b::ToHex(b::Sha256(evidence))<<'\n';
    return true;
}

int Controlled(const std::filesystem::path* output)
{
    auto hierarchy=c::BuildCanonicalMeasurementHierarchyV1();
    if(!hierarchy)return Failure(1,"controlled/hierarchy",hierarchy.Error());
    auto cases=c::BuildReuseCorpusV1(hierarchy.Value());
    if(!cases)return Failure(2,"controlled/reuse-corpus",cases.Error());
    const auto& reuseCase=cases.Value()[3];
    auto frames=sge4_5::spiral2::corpus::BuildFrameCorpusV1(hierarchy.Value());
    if(!frames)return Failure(3,"controlled/frame-corpus",frames.Error());
    auto scenario=s::BuildFrozenReuseComparisonScenarioV1(hierarchy.Value(),reuseCase);
    if(!scenario)return Failure(4,scenario.Error().stage,scenario.Error().message);
    sge4_5::d3d12::D3D12Backend backend({true,true});
    auto loaded=runtime::LoadStaticComposition(scenario.Value().frozenCompositionBytes,backend);
    if(!loaded)return Failure(5,loaded.Error().stage,loaded.Error().message);
    const auto recordsFlow=s::FindFlowByAuthorKeyV1(
        loaded.Value().Artifact().ValidatedContract().Contract(),s::RecordsFlowKeyV1);
    if(!recordsFlow.IsValid())return Failure(6,"controlled/identity","records Flow was not found");
    auto before=e::ExecuteLoadedReuseComparisonScenarioV1(
        scenario.Value(),hierarchy.Value(),reuseCase,frames.Value()[4].palette,loaded.Value(),0);
    if(!before)return Failure(7,before.Error().stage,before.Error().message);
    const auto beforeEvidence=e::SerializeReuseExecutionEvidenceV1(scenario.Value(),before.Value());
    const auto staleResource=loaded.Value().SharedResource(recordsFlow);
    const auto staleToken=loaded.Value().AvailableAfter(recordsFlow);
    const auto oldEpoch=loaded.Value().DeviceEpoch();
    auto recovered=runtime::RecoverStaticComposition(
        loaded.Value(),sge4_5::runtime::DeviceRecoveryMode::ControlledRebuild);
    if(!recovered)return Failure(8,recovered.Error().stage,recovered.Error().message);
    const auto& report=recovered.Value();
    if(loaded.Value().State()!=sge4_5::runtime::DeviceRuntimeState::Active||
       loaded.Value().DeviceEpoch()!=oldEpoch+1||
       report.leafCountBefore!=11||report.leafCountAfter!=11||
       report.resourceCountBefore!=12||report.resourceCountAfter!=12||
       !report.frozenArtifactRevalidated||!report.allRuntimeObjectsReleased||
       !report.allRuntimeObjectsRematerialized)
        return Failure(9,"controlled/report","whole-composition recovery report did not preserve the eleven-Leaf/twelve-resource authority");
    auto stale=runtime::ValidateStaticCompositionHandleEpoch(loaded.Value(),staleResource,staleToken);
    if(stale||stale.Error().stage!="static-runtime/stale-epoch")
        return Failure(10,"controlled/stale-epoch","pre-recovery resource/token pair was not rejected as stale");
    runtime::StaticCompositionFrameInvocation missingRebind;
    missingRebind.frameNumber=1;
    if(runtime::SubmitStaticComposition(loaded.Value(),missingRebind))
        return Failure(11,"controlled/rebind","submission without Motor/reference rebind was accepted");
    auto after=e::ExecuteLoadedReuseComparisonScenarioV1(
        scenario.Value(),hierarchy.Value(),reuseCase,frames.Value()[4].palette,loaded.Value(),2);
    if(!after)return Failure(12,after.Error().stage,after.Error().message);
    const auto afterEvidence=e::SerializeReuseExecutionEvidenceV1(scenario.Value(),after.Value());
    if(beforeEvidence!=afterEvidence)
        return Failure(13,"controlled/evidence","recovery changed the R64/F04 execution evidence");
    auto current=runtime::ValidateStaticCompositionHandleEpoch(
        loaded.Value(),loaded.Value().SharedResource(recordsFlow),loaded.Value().AvailableAfter(recordsFlow));
    if(!current)return Failure(14,current.Error().stage,current.Error().message);
    if(output&&!WriteEvidence(*output,afterEvidence))
        return Failure(15,"controlled/evidence","could not write controlled-recovery evidence");
    std::cout<<"Spiral 3 controlled whole-composition recovery passed for R64, epoch "
             <<oldEpoch<<"->"<<loaded.Value().DeviceEpoch()
             <<"; explicit Motor/reference rebind required.\n";
    return 0;
}

int ActualRemoval(const std::filesystem::path& output)
{
    auto hierarchy=c::BuildCanonicalMeasurementHierarchyV1();
    if(!hierarchy)return Failure(20,"actual/hierarchy",hierarchy.Error());
    auto cases=c::BuildReuseCorpusV1(hierarchy.Value());
    if(!cases)return Failure(21,"actual/reuse-corpus",cases.Error());
    const auto& reuseCase=cases.Value()[4];
    auto frames=sge4_5::spiral2::corpus::BuildFrameCorpusV1(hierarchy.Value());
    if(!frames)return Failure(22,"actual/frame-corpus",frames.Error());
    auto scenario=s::BuildFrozenReuseComparisonScenarioV1(hierarchy.Value(),reuseCase);
    if(!scenario)return Failure(23,scenario.Error().stage,scenario.Error().message);
    sge4_5::d3d12::D3D12Backend backend({true,true});
    auto loaded=runtime::LoadStaticComposition(scenario.Value().frozenCompositionBytes,backend);
    if(!loaded)return Failure(24,loaded.Error().stage,loaded.Error().message);
    const auto recordsFlow=s::FindFlowByAuthorKeyV1(
        loaded.Value().Artifact().ValidatedContract().Contract(),s::RecordsFlowKeyV1);
    auto before=e::ExecuteLoadedReuseComparisonScenarioV1(
        scenario.Value(),hierarchy.Value(),reuseCase,frames.Value()[7].palette,loaded.Value(),0);
    if(!before)return Failure(25,before.Error().stage,before.Error().message);
    if(!recordsFlow.IsValid())return Failure(26,"actual/identity","records Flow was not found");
    const auto evidence=e::SerializeReuseExecutionEvidenceV1(scenario.Value(),before.Value());
    const auto staleResource=loaded.Value().SharedResource(recordsFlow);
    const auto staleToken=loaded.Value().AvailableAfter(recordsFlow);
    const auto oldEpoch=loaded.Value().DeviceEpoch();
    auto removed=runtime::RecoverStaticComposition(
        loaded.Value(),sge4_5::runtime::DeviceRecoveryMode::ForceRemovalForTest);
    if(!removed)return Failure(27,removed.Error().stage,removed.Error().message);
    if(!removed.Value().device.forcedRemoval||
       loaded.Value().State()!=sge4_5::runtime::DeviceRuntimeState::AwaitingAdapter||
       loaded.Value().DeviceEpoch()!=oldEpoch||loaded.Value().LeafCount()!=0||
       loaded.Value().ResourceCount()!=0||!removed.Value().allRuntimeObjectsReleased||
       removed.Value().allRuntimeObjectsRematerialized)
        return Failure(28,"actual/quarantine","actual removal did not release all runtime objects into AwaitingAdapter");
    auto stale=runtime::ValidateStaticCompositionHandleEpoch(loaded.Value(),staleResource,staleToken);
    if(stale||stale.Error().stage!="static-runtime/device-state")
        return Failure(29,"actual/stale-state","pre-removal handles were not rejected by device state");
    if(runtime::SubmitStaticComposition(loaded.Value(),{}))
        return Failure(30,"actual/submission","submission was accepted while AwaitingAdapter");
    auto retry=runtime::RecoverStaticComposition(
        loaded.Value(),sge4_5::runtime::DeviceRecoveryMode::RetryAdapterReacquisition);
    if(!retry)return Failure(31,retry.Error().stage,retry.Error().message);
    if(retry.Value().device.adapterReacquired||
       loaded.Value().State()!=sge4_5::runtime::DeviceRuntimeState::AwaitingAdapter)
        return Failure(32,"actual/removed-adapter","removed WARP adapter was reacquired");
    if(!WriteEvidence(output,evidence))
        return Failure(33,"actual/evidence","could not write pre-removal evidence");
    std::cout<<"Spiral 3 actual RemoveDevice quarantine passed for R256/F07; "
                "removed WARP LUID excluded and stale submission rejected.\n";
    return 0;
}

int FreshRematerialization(const std::filesystem::path& output)
{
    auto hierarchy=c::BuildCanonicalMeasurementHierarchyV1();
    if(!hierarchy)return Failure(40,"fresh/hierarchy",hierarchy.Error());
    auto cases=c::BuildReuseCorpusV1(hierarchy.Value());
    if(!cases)return Failure(41,"fresh/reuse-corpus",cases.Error());
    const auto& reuseCase=cases.Value()[4];
    auto frames=sge4_5::spiral2::corpus::BuildFrameCorpusV1(hierarchy.Value());
    if(!frames)return Failure(42,"fresh/frame-corpus",frames.Error());
    auto scenario=s::BuildFrozenReuseComparisonScenarioV1(hierarchy.Value(),reuseCase);
    if(!scenario)return Failure(43,scenario.Error().stage,scenario.Error().message);
    sge4_5::d3d12::D3D12Backend backend({true,true});
    auto loaded=runtime::LoadStaticComposition(scenario.Value().frozenCompositionBytes,backend);
    if(!loaded)return Failure(44,loaded.Error().stage,loaded.Error().message);
    auto observed=e::ExecuteLoadedReuseComparisonScenarioV1(
        scenario.Value(),hierarchy.Value(),reuseCase,frames.Value()[7].palette,loaded.Value(),0);
    if(!observed)return Failure(45,observed.Error().stage,observed.Error().message);
    if(!WriteEvidence(output,e::SerializeReuseExecutionEvidenceV1(scenario.Value(),observed.Value())))
        return Failure(46,"fresh/evidence","could not write fresh-process evidence");
    std::cout<<"Spiral 3 fresh-process R256/F07 rematerialization passed.\n";
    return 0;
}
}

int main(int argc,char**argv)
{
    if(argc==4&&std::string(argv[2])=="--emit")
    {
        const std::filesystem::path output=argv[3];
        if(std::string(argv[1])=="--controlled")return Controlled(&output);
        if(std::string(argv[1])=="--actual-removal")return ActualRemoval(output);
        if(std::string(argv[1])=="--fresh-rematerialize")return FreshRematerialization(output);
    }
    if(argc==1||(argc==2&&std::string(argv[1])=="--controlled"))return Controlled(nullptr);
    return Failure(64,"arguments","unsupported command line");
}
