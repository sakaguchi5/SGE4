#include "../00_Foundation/Base.h"
#include "../17_CompositionContract/CompositionContract.h"
#include "../18_CompositionPlan/CompositionPlan.h"
#include "../19_CompositionVerifier/CompositionVerifier.h"
#include "../102_Spiral3Corpus/Spiral3Corpus.h"
#include "../107_Spiral3Scenarios/Spiral3Scenarios.h"
#include <array>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace b=sge4_5::base;
namespace can=sge4_5::composition::canonical;
namespace sc=sge4_5::spiral3::scenarios;
namespace cp=sge4_5::spiral3::corpus;
namespace lf=sge4_5::spiral3::leaf;

can::EndpointReferenceDeclaration Ref(const std::string& leaf,const std::string& endpoint){return {leaf,endpoint};}
can::LeafPackageDeclaration Decl(const lf::FrozenReuseLeafV1& item,std::initializer_list<can::LeafEndpointDeclaration> endpoints){can::LeafPackageDeclaration value;value.stableKey=item.role;value.packageBytes=item.packageBytes;value.endpoints=endpoints;return value;}

enum class Mutation{SwapObserverAB,SwapReferencePoints,DuplicateWriter,MissingCPointConsumer};
b::Result<std::vector<std::byte>,std::string> FreezeMutated(const sc::FrozenReuseComparisonScenarioV1& s,Mutation mutation)
{
    const auto& l=s.leaves;can::ContractBuildInput input;
    input.leaves={Decl(l[0],{{0,"motors"},{1,"reference"}}),Decl(l[1],{{0,"points"}}),Decl(l[2],{{0,"input"},{1,"output"}}),Decl(l[3],{{0,"input"},{1,"output"}}),Decl(l[4],{{0,"transforms"},{1,"points"},{2,"output"}}),Decl(l[5],{{0,"input"},{1,"output"}}),Decl(l[6],{{0,"transforms"},{1,"points"},{2,"output"}}),Decl(l[7],{{0,"input"},{1,"output"}}),Decl(l[8],{{0,"input"},{1,"output"}}),Decl(l[9],{{0,"transforms"},{1,"points"},{2,"output"}}),Decl(l[10],{{0,"reference"},{1,"a"},{2,"b"},{3,"c"},{4,"records"}})};
    auto flow=[&](std::string key,std::string producer,std::string endpoint,std::initializer_list<can::EndpointReferenceDeclaration> consumers,can::ResourceBoundary boundary=can::ResourceBoundary::Internal){can::ResourceFlowDeclaration f;f.stableKey=std::move(key);f.boundary=boundary;f.producer=Ref(producer,endpoint);f.consumers=consumers;input.resources.push_back(std::move(f));};
    flow(std::string(sc::MotorsFlowKeyV1),l[0].role,"motors",{Ref(l[2].role,"input"),Ref(l[5].role,"input"),Ref(l[7].role,"input")});
    if(mutation==Mutation::SwapReferencePoints){flow(std::string(sc::ReferenceFlowKeyV1),l[1].role,"points",{Ref(l[10].role,"reference")});flow(std::string(sc::PointsFlowKeyV1),l[0].role,"reference",{Ref(l[4].role,"points"),Ref(l[6].role,"points"),Ref(l[9].role,"points")});}
    else{flow(std::string(sc::ReferenceFlowKeyV1),l[0].role,"reference",{Ref(l[10].role,"reference")});if(mutation==Mutation::MissingCPointConsumer)flow(std::string(sc::PointsFlowKeyV1),l[1].role,"points",{Ref(l[4].role,"points"),Ref(l[6].role,"points")});else flow(std::string(sc::PointsFlowKeyV1),l[1].role,"points",{Ref(l[4].role,"points"),Ref(l[6].role,"points"),Ref(l[9].role,"points")});}
    flow(std::string(sc::ALocalMatrixFlowKeyV1),l[2].role,"output",{Ref(l[3].role,"input")});flow(std::string(sc::AGlobalMatrixFlowKeyV1),l[3].role,"output",{Ref(l[4].role,"transforms")});flow(std::string(sc::AOutputFlowKeyV1),l[4].role,"output",{Ref(l[10].role,mutation==Mutation::SwapObserverAB?"b":"a")});flow(std::string(sc::BGlobalMotorFlowKeyV1),l[5].role,"output",{Ref(l[6].role,"transforms")});flow(std::string(sc::BOutputFlowKeyV1),l[6].role,"output",{Ref(l[10].role,mutation==Mutation::SwapObserverAB?"a":"b")});flow(std::string(sc::CGlobalMotorFlowKeyV1),l[7].role,"output",{Ref(l[8].role,"input")});flow(std::string(sc::CGlobalMatrixFlowKeyV1),l[8].role,"output",{Ref(l[9].role,"transforms")});flow(std::string(sc::COutputFlowKeyV1),l[9].role,"output",{Ref(l[10].role,"c")});flow(std::string(sc::RecordsFlowKeyV1),l[10].role,"records",{},can::ResourceBoundary::CompositionOutput);
    if(mutation==Mutation::DuplicateWriter)flow("spiral3/flow/illegal-second-a-output",l[4].role,"output",{},can::ResourceBoundary::CompositionOutput);
    auto contract=can::BuildCompositionContract(std::move(input));if(!contract)return b::Result<std::vector<std::byte>,std::string>::Failure(contract.Error().stage+": "+contract.Error().message);auto raw=can::planning::ProposeCompositionPlan(contract.Value());if(!raw)return b::Result<std::vector<std::byte>,std::string>::Failure(raw.Error().stage+": "+raw.Error().message);auto verified=can::verification::VerifyAndSeal(contract.Value(),raw.Value());if(!verified)return b::Result<std::vector<std::byte>,std::string>::Failure(verified.Error().stage+": "+verified.Error().message);auto frozen=can::verification::FreezeVerifiedComposition(contract.Value(),verified.Value());if(!frozen)return b::Result<std::vector<std::byte>,std::string>::Failure(frozen.Error().stage+": "+frozen.Error().message);return b::Result<std::vector<std::byte>,std::string>::Success(std::move(frozen.Value()));
}


int Fail(int code,std::string_view stage,std::string_view detail={})
{
    std::cerr<<"Spiral 3 CU4 failure ["<<code<<"] "<<stage;
    if(!detail.empty())std::cerr<<": "<<detail;
    std::cerr<<'\n';
    return code;
}

std::string TopologySummary(const can::PackageCompositionContract& contract,const can::planning::RawCompositionPlan& plan)
{
    return "leaves="+std::to_string(contract.leaves.size())+
        " resources="+std::to_string(contract.resources.size())+
        " schedule="+std::to_string(plan.schedule.size())+
        " allocations="+std::to_string(plan.allocations.size())+
        " handoffs="+std::to_string(plan.handoffs.size())+
        " signals="+std::to_string(plan.signals.size())+
        " waits="+std::to_string(plan.waits.size())+
        " recoveryLeaves="+std::to_string(plan.recovery.recreateLeaves.size())+
        " recoveryResources="+std::to_string(plan.recovery.recreateResources.size());
}

int main(int argc,char**argv)
{
    auto hierarchy=cp::BuildCanonicalMeasurementHierarchyV1();
    if(!hierarchy)return Fail(1,"canonical hierarchy",hierarchy.Error());
    auto corpus=cp::BuildReuseCorpusV1(hierarchy.Value());
    if(!corpus)return Fail(2,"reuse corpus",corpus.Error());
    if(corpus.Value().size()!=6)return Fail(2,"reuse corpus cardinality",std::to_string(corpus.Value().size()));

    std::vector<sc::FrozenReuseComparisonScenarioV1> scenarios;
    scenarios.reserve(6);
    b::BinaryWriter evidence;
    std::uint32_t mutations=0;
    auto rejected=[&](const cp::ReuseCaseV1& rc,sc::FrozenReuseComparisonScenarioV1 value)
    {
        if(sc::ValidateFrozenReuseComparisonScenarioV1(hierarchy.Value(),rc,value))return false;
        ++mutations;
        return true;
    };

    for(const auto& rc:corpus.Value())
    {
        const auto reuseLabel="R"+std::to_string(rc.semantic.reuseCount);
        auto scenario=sc::BuildFrozenReuseComparisonScenarioV1(hierarchy.Value(),rc);
        if(!scenario)return Fail(3,reuseLabel+" scenario build",scenario.Error().stage+": "+scenario.Error().message);
        auto directValidation=sc::ValidateFrozenReuseComparisonScenarioV1(hierarchy.Value(),rc,scenario.Value());
        if(!directValidation)return Fail(4,reuseLabel+" direct validation",directValidation.Error().stage+": "+directValidation.Error().message);

        auto frozen=can::verification::ReadVerifiedFrozenComposition(scenario.Value().frozenCompositionBytes);
        if(!frozen)return Fail(5,reuseLabel+" frozen composition read",frozen.Error().stage+": "+frozen.Error().message);
        const auto& contract=frozen.Value().ValidatedContract().Contract();
        const auto& plan=frozen.Value().VerifiedPlan().Plan();
        if(contract.leaves.size()!=11||contract.resources.size()!=12||plan.schedule.size()!=11||
           plan.allocations.size()!=12||plan.handoffs.size()!=15||plan.signals.size()!=11||
           plan.waits.size()!=15||plan.recovery.recreateLeaves.size()!=11||
           plan.recovery.recreateResources.size()!=11)
            return Fail(6,reuseLabel+" topology",TopologySummary(contract,plan));

        auto x=scenario.Value();
        x.scenarioIdentity[0]^=std::byte{1};
        if(!rejected(rc,std::move(x)))return Fail(7,reuseLabel+" mutation","scenario identity corruption was accepted");

        x=scenario.Value();
        x.frozenCompositionBytes.back()^=std::byte{1};
        x.frozenCompositionDigest=b::Sha256(x.frozenCompositionBytes);
        x.scenarioIdentity=sc::ComputeFrozenReuseComparisonScenarioIdentityV1(x);
        if(!rejected(rc,std::move(x)))return Fail(8,reuseLabel+" mutation","frozen composition byte corruption was accepted");

        x=scenario.Value();
        ++x.reuseCount;
        x.scenarioIdentity=sc::ComputeFrozenReuseComparisonScenarioIdentityV1(x);
        if(!rejected(rc,std::move(x)))return Fail(9,reuseLabel+" mutation","reuse-count corruption was accepted");

        x=scenario.Value();
        x.leaves[4].verifiedPlanIdentity[0]^=std::byte{1};
        x.scenarioIdentity=sc::ComputeFrozenReuseComparisonScenarioIdentityV1(x);
        if(!rejected(rc,std::move(x)))return Fail(10,reuseLabel+" mutation","Leaf plan identity corruption was accepted");

        for(auto mutation:{Mutation::SwapObserverAB,Mutation::SwapReferencePoints})
        {
            const char* mutationName=mutation==Mutation::SwapObserverAB?"swap observer A/B":"swap reference/points";
            auto changed=FreezeMutated(scenario.Value(),mutation);
            if(!changed)return Fail(11,reuseLabel+" freeze valid-shaped mutation",std::string(mutationName)+": "+changed.Error());
            x=scenario.Value();
            x.frozenCompositionBytes=std::move(changed.Value());
            x.frozenCompositionDigest=b::Sha256(x.frozenCompositionBytes);
            x.scenarioIdentity=sc::ComputeFrozenReuseComparisonScenarioIdentityV1(x);
            if(!rejected(rc,std::move(x)))return Fail(12,reuseLabel+" scenario authority",std::string(mutationName)+" was accepted");
        }

        auto duplicateWriter=FreezeMutated(scenario.Value(),Mutation::DuplicateWriter);
        if(duplicateWriter)return Fail(13,reuseLabel+" generic contract authority","duplicate writer was accepted");
        auto missingConsumer=FreezeMutated(scenario.Value(),Mutation::MissingCPointConsumer);
        if(missingConsumer)return Fail(13,reuseLabel+" generic contract authority","missing C point consumer was accepted");
        mutations+=2;

        auto bytes=sc::SerializeFrozenReuseComparisonScenarioEvidenceV1(scenario.Value());
        evidence.WriteU64(bytes.size());
        evidence.WriteBytes(bytes);
        scenarios.push_back(std::move(scenario.Value()));
        std::cout<<"[CU4] "<<reuseLabel<<" composition and mutations passed.\n";
    }

    for(std::size_t i=0;i<scenarios.size();++i)
    {
        auto x=scenarios[i];
        x.frozenCompositionBytes=scenarios[(i+1)%scenarios.size()].frozenCompositionBytes;
        x.frozenCompositionDigest=b::Sha256(x.frozenCompositionBytes);
        x.scenarioIdentity=sc::ComputeFrozenReuseComparisonScenarioIdentityV1(x);
        if(!rejected(corpus.Value()[i],std::move(x)))
            return Fail(14,"cross-R replay","R"+std::to_string(corpus.Value()[i].semantic.reuseCount)+" accepted another R composition");
    }

    auto bytes=std::move(evidence).Take();
    if(argc==3&&std::string(argv[1])=="--emit")
    {
        auto wrote=b::WriteAllBytes(std::filesystem::path(argv[2]),bytes);
        if(!wrote)return Fail(15,"evidence write",wrote.Error());
        std::cout<<b::ToHex(b::Sha256(bytes))<<'\n';
        return 0;
    }
    std::cout<<"Spiral 3 froze "<<scenarios.size()<<" reuse scenarios as 11-Leaf/12-Flow compositions; "<<mutations<<" composition authority mutations rejected.\n";
    if(scenarios.size()!=6||mutations!=54)
        return Fail(16,"final cardinality","scenarios="+std::to_string(scenarios.size())+" mutations="+std::to_string(mutations));
    return 0;
}
