#include "../46_CompositionContractTests/TestFixture.h"
#include "../18_LinkPlanVerifier/LinkPlanVerifier.h"
#include "../19_PackageLinker/PackageLinker.h"

#include <iostream>
#include <type_traits>

int main()
{
    namespace c=sge4::composition;
    static_assert(!std::is_constructible_v<c::VerifiedLinkPlan,c::LinkPlanIR>);
    auto contract=sge4::qualification::composition_fixture::BuildContract(); if(!contract)return 1;
    auto make=[&](){return c::ProposeLinkPlan(contract.Value());};
    auto identity=make(); identity.Value().identity[0]^=std::byte{1}; if(c::VerifyAndSeal(contract.Value(),std::move(identity).Value()))return 2;
    auto order=make(); std::swap(order.Value().schedule[0].leaf,order.Value().schedule[1].leaf); order.Value().identity=c::ComputeLinkPlanIdentity(contract.Value(),order.Value()); if(c::VerifyAndSeal(contract.Value(),std::move(order).Value()))return 3;
    auto binding=make(); binding.Value().bindings[0].resource={1}; binding.Value().identity=c::ComputeLinkPlanIdentity(contract.Value(),binding.Value()); if(c::VerifyAndSeal(contract.Value(),std::move(binding).Value()))return 4;
    auto state=make(); state.Value().signalWaits[0].handoffState={sge4::package::d3d12_v13::StateClass::Common,0,0}; state.Value().identity=c::ComputeLinkPlanIdentity(contract.Value(),state.Value()); if(c::VerifyAndSeal(contract.Value(),std::move(state).Value()))return 5;
    auto extracted=contract.Value(); extracted.leaves[0].externalSlots[0].minimumBytes=8; extracted.endpoints[0].contract.minimumBytes=8;
    extracted.identity=c::ComputeContractIdentity(extracted); auto extractedPlan=c::ProposeLinkPlan(extracted); if(!extractedPlan||c::VerifyAndSeal(extracted,std::move(extractedPlan).Value()))return 6;
    auto raw=make(); if(!raw)return 7; auto sealed=c::VerifyAndSeal(contract.Value(),std::move(raw).Value()); if(!sealed)return 8;
    std::cout<<"Level 4 v1 LinkVerifier authority tests passed\n"; return 0;
}
