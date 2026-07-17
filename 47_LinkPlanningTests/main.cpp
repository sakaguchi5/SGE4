#include "../46_CompositionContractTests/TestFixture.h"
#include "../00_Foundation/FileIO.h"
#include "../18_LinkPlanVerifier/LinkPlanVerifier.h"
#include "../19_PackageLinker/FrozenComposition.h"
#include "../19_PackageLinker/PackageLinker.h"

#include <iostream>
#include <string_view>

int main(int argc, char** argv)
{
    namespace c=sge4::composition; auto contract=sge4::qualification::composition_fixture::BuildContract();
    if(!contract){std::cerr<<contract.Error()<<'\n';return 1;} auto plan=c::ProposeLinkPlan(contract.Value()); if(!plan)return 2;
    auto verified=c::VerifyAndSeal(contract.Value(),std::move(plan).Value()); if(!verified)return 3;
    auto bytes=c::WriteFrozenComposition(contract.Value(),verified.Value()); if(!bytes){std::cerr<<bytes.Error().message<<'\n';return 4;}
    auto read=c::ReadFrozenComposition(bytes.Value()); if(!read){std::cerr<<read.Error().stage<<": "<<read.Error().message<<'\n';return 5;}
    auto rewritten=c::WriteFrozenComposition(read.Value().Contract(),read.Value().VerifiedPlan()); if(!rewritten||rewritten.Value()!=bytes.Value())return 6;
    auto corrupt=bytes.Value(); corrupt.back()^=std::byte{1}; if(c::ReadFrozenComposition(std::move(corrupt)))return 7;
    auto second=c::WriteFrozenComposition(contract.Value(),verified.Value()); if(!second||second.Value()!=bytes.Value())return 8;
    if(argc==3&&std::string_view(argv[1])=="--emit-composition")
    { auto written=sge4::base::WriteAllBytes(argv[2],bytes.Value()); if(!written){std::cerr<<written.Error()<<'\n';return 9;} }
    std::cout<<"Level 4 v1 link planning and frozen composition tests passed\n"; return 0;
}
