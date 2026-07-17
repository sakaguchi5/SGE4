#include "TestFixture.h"

#include <iostream>

int main()
{
    namespace fixture = sge4::qualification::composition_fixture;
    auto a = fixture::BuildContract(false); auto b = fixture::BuildContract(true);
    if (!a || !b) { std::cerr << (a ? b.Error() : a.Error()) << '\n'; return 1; }
    if (a.Value().identity != b.Value().identity || a.Value().leaves.size()!=2 || a.Value().endpoints.size()!=4 || a.Value().resources.size()!=3 || a.Value().links.size()!=1) return 2;
    if (a.Value().leaves[0].packageBytes != b.Value().leaves[0].packageBytes || a.Value().links[0].producer.value != 1 || a.Value().links[0].consumer.value != 2) return 3;
    auto bytes = fixture::BuildTransformLeaf(); if (!bytes) return 4;
    sge4::composition::ContractBuildInput invalid;
    invalid.leaves={{1,bytes.Value()},{2,bytes.Value()}}; invalid.endpoints={{1,99,sge4::composition::EndpointDirection::Import}};
    auto rejected=sge4::composition::BuildCompositionContract(std::move(invalid)); if(rejected) return 5;
    std::cout << "Level 4 v1 composition contract tests passed\n";
    return 0;
}
