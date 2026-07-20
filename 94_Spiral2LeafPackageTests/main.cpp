#include "../00_Foundation/Base.h"
#include "../82_Spiral2Corpus/Spiral2Corpus.h"
#include "../84_HierarchyRepresentationPlanner/HierarchyRepresentationPlanner.h"
#include "../85_HierarchyRepresentationVerifier/HierarchyRepresentationVerifier.h"
#include "../86_Spiral2LeafCompiler/Spiral2LeafCompiler.h"

#include <array>
#include <filesystem>
#include <iostream>

namespace c=sge4_5::spiral2::candidate;
namespace p=sge4_5::spiral2::planner;
namespace v=sge4_5::spiral2::verification;
namespace l=sge4_5::spiral2::leaf;
namespace r=sge4_5::spiral2::corpus;
namespace b=sge4_5::base;

int main(int argc,char** argv)
{
    auto corpus=r::BuildTopologyCorpusV1();
    if(!corpus)return 1;
    const auto& hierarchy=corpus.Value()[1].hierarchy;
    const std::array<c::CandidateKindV1,3> kinds{
        c::CandidateKindV1::MatrixHierarchy,
        c::CandidateKindV1::DirectPgaHierarchy,
        c::CandidateKindV1::HybridHierarchy};

    b::BinaryWriter evidence;
    std::uint32_t leaves=0;
    std::uint32_t mutations=0;
    auto mustReject=[&](l::FrozenHierarchyLeafV1 value)
    {
        if(l::ValidateFrozenHierarchyLeafV1(value))return false;
        ++mutations;
        return true;
    };
    auto exerciseBindingMutations=[&](const l::FrozenHierarchyLeafV1& item)
    {
        auto mutation=item;mutation.packageBindingIdentity[0]^=std::byte{1};if(!mustReject(std::move(mutation)))return false;
        mutation=item;mutation.verifiedOperationIdentity[0]^=std::byte{1};if(!mustReject(std::move(mutation)))return false;
        mutation=item;mutation.verifiedProgramTemplateIdentity[0]^=std::byte{1};if(!mustReject(std::move(mutation)))return false;
        mutation=item;++mutation.plannedDispatchX;if(!mustReject(std::move(mutation)))return false;
        mutation=item;mutation.packageProgramInterfaceDigest[0]^=std::byte{1};if(!mustReject(std::move(mutation)))return false;
        mutation=item;mutation.packageBindingLayoutDigest[0]^=std::byte{1};if(!mustReject(std::move(mutation)))return false;
        mutation=item;mutation.packageShaderBytecodeDigest[0]^=std::byte{1};if(!mustReject(std::move(mutation)))return false;
        return true;
    };

    for(std::size_t kindIndex=0;kindIndex<kinds.size();++kindIndex)
    {
        auto raw=p::PlanCandidateGraphV1(hierarchy,kinds[kindIndex]);
        if(!raw)return 2;
        auto verified=v::VerifyCandidateGraphV1(hierarchy,raw.Value());
        if(!verified)return 3;

        if(kindIndex==0)
        {
            auto source=l::CompileDynamicInvocationSourceV1(hierarchy,verified.Value());
            if(!source){std::cerr<<source.Error().stage<<": "<<source.Error().message<<'\n';return 4;}
            const auto& operation=verified.Value().Plan().operations[0];
            if(!l::ValidateFrozenHierarchyLeafV1(source.Value())||
               source.Value().lockedDynamicBytes!=64||source.Value().lockedDynamicSlot!=0||
               source.Value().lockedReferenceBytes!=160||source.Value().lockedReferenceSlot!=1||
               source.Value().plannedDispatchX!=1||
               source.Value().verifiedOperationIdentity!=operation.operationIdentity||
               source.Value().verifiedProgramTemplateIdentity!=operation.programIdentity)return 5;
            if(!exerciseBindingMutations(source.Value()))return 6;
            const auto bytes=l::SerializeFrozenHierarchyLeafEvidenceV1(source.Value());
            evidence.WriteU32(static_cast<std::uint32_t>(bytes.size()));evidence.WriteBytes(bytes);++leaves;
        }

        auto group=l::CompileVerifiedHierarchyLeafGroupV1(hierarchy,verified.Value());
        if(!group){std::cerr<<group.Error().stage<<": "<<group.Error().message<<'\n';return 7;}
        const std::size_t expected=kinds[kindIndex]==c::CandidateKindV1::DirectPgaHierarchy?2:3;
        if(group.Value().leaves.size()!=expected||group.Value().verifiedPlanIdentity!=verified.Value().Plan().planIdentity)return 8;

        for(std::size_t leafIndex=0;leafIndex<group.Value().leaves.size();++leafIndex)
        {
            const auto& item=group.Value().leaves[leafIndex];
            const auto& operation=verified.Value().Plan().operations[leafIndex+1];
            if(!l::ValidateFrozenHierarchyLeafV1(item)||
               item.lockedDynamicSlot!=b::InvalidIndex||item.lockedReferenceSlot!=b::InvalidIndex||
               item.operationOrdinal!=leafIndex+1||
               item.verifiedOperationIdentity!=operation.operationIdentity||
               item.verifiedProgramTemplateIdentity!=operation.programIdentity)return 9;
            if(!exerciseBindingMutations(item))return 10;
            const auto bytes=l::SerializeFrozenHierarchyLeafEvidenceV1(item);
            evidence.WriteU32(static_cast<std::uint32_t>(bytes.size()));evidence.WriteBytes(bytes);++leaves;
        }
    }

    auto bytes=std::move(evidence).Take();
    if(argc==3&&std::string(argv[1])=="--emit")
    {
        auto wrote=b::WriteAllBytes(std::filesystem::path(argv[2]),bytes);
        if(!wrote)return 11;
        std::cout<<b::ToHex(b::Sha256(bytes))<<'\n';
        return 0;
    }
    std::cout<<"Spiral 2 lowered "<<leaves<<" verifier-plan operations into Package-bound Leaves; "<<mutations<<" operation/program/binding/dispatch mutations rejected.\n";
    return leaves==9&&mutations==63?0:12;
}
