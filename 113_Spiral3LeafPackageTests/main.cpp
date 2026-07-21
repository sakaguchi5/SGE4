#include "../00_Foundation/Base.h"
#include "../102_Spiral3Corpus/Spiral3Corpus.h"
#include "../104_ReuseRepresentationPlanner/ReuseRepresentationPlanner.h"
#include "../105_ReuseRepresentationVerifier/ReuseRepresentationVerifier.h"
#include "../106_Spiral3LeafCompiler/Spiral3LeafCompiler.h"
#include <array>
#include <filesystem>
#include <iostream>
#include <string>
#include <type_traits>

namespace b=sge4_5::base;
namespace c=sge4_5::spiral3::candidate;
namespace p=sge4_5::spiral3::planner;
namespace v=sge4_5::spiral3::verification;
namespace l=sge4_5::spiral3::leaf;
namespace r=sge4_5::spiral3::corpus;

static_assert(!std::is_default_constructible_v<v::VerifiedReuseRepresentationV1>);
static_assert(!std::is_constructible_v<v::VerifiedReuseRepresentationV1,c::RawCandidateGraphV1>);
static_assert(!std::is_convertible_v<c::RawCandidateGraphV1,v::VerifiedReuseRepresentationV1>);

int main(int argc,char**argv)
{
    auto hierarchy=r::BuildCanonicalMeasurementHierarchyV1();if(!hierarchy)return 1;
    auto cases=r::BuildReuseCorpusV1(hierarchy.Value());if(!cases||cases.Value().size()!=6)return 2;
    const std::array kinds{c::CandidateKindV1::MatrixHierarchy,c::CandidateKindV1::DirectPgaHierarchy,c::CandidateKindV1::HybridHierarchy};
    b::BinaryWriter evidence;std::uint32_t leaves=0,groups=0,mutations=0;
    auto mustReject=[&](l::FrozenReuseLeafV1 value){if(l::ValidateFrozenReuseLeafV1(value))return false;++mutations;return true;};
    auto mutate=[&](const l::FrozenReuseLeafV1& item){
        auto x=item;x.packageBindingIdentity[0]^=std::byte{1};if(!mustReject(std::move(x)))return false;
        x=item;x.verifiedPlanIdentity[0]^=std::byte{1};if(!mustReject(std::move(x)))return false;
        x=item;x.verifiedOperationIdentity[0]^=std::byte{1};if(!mustReject(std::move(x)))return false;
        x=item;x.verifiedProgramTemplateIdentity[0]^=std::byte{1};if(!mustReject(std::move(x)))return false;
        x=item;++x.plannedDispatchX;if(!mustReject(std::move(x)))return false;
        x=item;++x.reuseCount;if(!mustReject(std::move(x)))return false;
        x=item;++x.pointCount;if(!mustReject(std::move(x)))return false;
        x=item;x.executionDigest[0]^=std::byte{1};if(!mustReject(std::move(x)))return false;
        return true;};
    for(std::size_t ri=0;ri<cases.Value().size();++ri)
    {
        const auto& rc=cases.Value()[ri];
        std::array<b::Result<v::VerifiedReuseRepresentationV1,std::string>,3> verified={
            v::VerifyCandidateGraphV1(hierarchy.Value(),rc.semantic,p::PlanCandidateGraphV1(hierarchy.Value(),rc.semantic,kinds[0]).Value()),
            v::VerifyCandidateGraphV1(hierarchy.Value(),rc.semantic,p::PlanCandidateGraphV1(hierarchy.Value(),rc.semantic,kinds[1]).Value()),
            v::VerifyCandidateGraphV1(hierarchy.Value(),rc.semantic,p::PlanCandidateGraphV1(hierarchy.Value(),rc.semantic,kinds[2]).Value())};
        for(const auto& item:verified)if(!item)return 3;
        auto dynamic=l::CompileDynamicInvocationSourceV1(hierarchy.Value(),rc.semantic,verified[0].Value());if(!dynamic){std::cerr<<dynamic.Error().stage<<": "<<dynamic.Error().message<<'\n';return 4;}
        auto points=l::CompileConsumerPointSourceV1(rc,verified[0].Value());if(!points){std::cerr<<points.Error().stage<<": "<<points.Error().message<<'\n';return 5;}
        if(!l::ValidateFrozenReuseLeafV1(dynamic.Value())||!l::ValidateFrozenReuseLeafV1(points.Value()))return 6;
        if(dynamic.Value().lockedDynamicBytes!=hierarchy.Value().boneCount*32u||dynamic.Value().lockedReferenceBytes!=rc.semantic.pointCount*16u||dynamic.Value().expectedDynamicSlotCount!=2||dynamic.Value().expectedExternalSlotCount!=2)return 7;
        if(points.Value().packageOwnedInitialBytes!=rc.semantic.pointCount*16ull||points.Value().expectedDynamicSlotCount!=0||points.Value().expectedExternalSlotCount!=1)return 8;
        for(const auto* source:{&dynamic.Value(),&points.Value()}){if(ri==0&&!mutate(*source))return 9;const auto bytes=l::SerializeFrozenReuseLeafEvidenceV1(*source);evidence.WriteU32(static_cast<std::uint32_t>(bytes.size()));evidence.WriteBytes(bytes);++leaves;}
        for(std::size_t ki=0;ki<kinds.size();++ki)
        {
            auto group=l::CompileVerifiedReuseLeafGroupV1(hierarchy.Value(),rc.semantic,verified[ki].Value());if(!group){std::cerr<<group.Error().stage<<": "<<group.Error().message<<'\n';return 10;}
            const std::size_t expected=ki==1?2:3;if(group.Value().leaves.size()!=expected||group.Value().verifiedPlanIdentity!=verified[ki].Value().Plan().planIdentity||group.Value().reuseCount!=rc.reuseCount)return 11;
            ++groups;
            for(const auto& item:group.Value().leaves){if(!l::ValidateFrozenReuseLeafV1(item)||item.reuseSemanticIdentity!=sge4_5::spiral3::semantic::ReuseSweepSemanticIdentityV1(rc.semantic)||item.localPointCorpusIdentity!=rc.semantic.localPointCorpusIdentity)return 12;if(ri==0&&!mutate(item))return 13;const auto bytes=l::SerializeFrozenReuseLeafEvidenceV1(item);evidence.WriteU32(static_cast<std::uint32_t>(bytes.size()));evidence.WriteBytes(bytes);++leaves;}
        }
    }
    auto bytes=std::move(evidence).Take();
    if(argc==3&&std::string(argv[1])=="--emit"){auto write=b::WriteAllBytes(std::filesystem::path(argv[2]),bytes);if(!write)return 14;std::cout<<b::ToHex(b::Sha256(bytes))<<'\n';return 0;}
    std::cout<<"Spiral 3 lowered "<<groups<<" verified reuse candidate groups into "<<leaves<<" Package-bound Leaves; "<<mutations<<" Leaf authority mutations rejected.\n";
    return groups==18&&leaves==60&&mutations==80?0:15;
}
