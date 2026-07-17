#include "TestFixture.h"

#include "../00_Foundation/FileIO.h"
#include "../00_Foundation/Sha256.h"
#include "../17A_Schema18ResourceFlowPlan/Schema18ResourceFlowPlan.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <span>
#include <string_view>

namespace
{
namespace fixture = sge4::qualification::schema18_planning_fixture;
namespace planning = sge4::composition::schema18::planning;

bool WriteBytes(const std::filesystem::path& path,
                std::span<const std::byte> bytes)
{
    std::filesystem::create_directories(path.parent_path());
    return static_cast<bool>(sge4::base::WriteAllBytes(path, bytes));
}
}

int main(int argc, char** argv)
{
    auto fixtureResult = fixture::BuildFixture();
    if (!fixtureResult)
    {
        std::cerr << fixtureResult.Error() << '\n';
        return 1;
    }
    const auto& contract = fixtureResult.Value().contract;
    auto proposed = planning::ProposeResourceFlowPlan(contract);
    if (!proposed)
    {
        std::cerr << proposed.Error().stage << ": " << proposed.Error().message << '\n';
        return 2;
    }
    const auto& plan = proposed.Value();
    if (plan.allocations.size() != 4 || plan.schedule.size() != 3 ||
        plan.bindings.size() != 6 || plan.handoffs.size() != 2 ||
        plan.signals.size() != 1 || plan.waits.size() != 2 ||
        plan.recovery.recreateLeaves.size() != 3 ||
        plan.recovery.recreateResources.size() != 1)
        return 3;

    std::uint32_t owned = 0;
    std::uint32_t inputs = 0;
    std::uint32_t outputs = 0;
    for (const auto& allocation : plan.allocations)
    {
        if (allocation.ownership == planning::AllocationOwnership::CompositionOwned) ++owned;
        else if (allocation.ownership == planning::AllocationOwnership::ExternalInput) ++inputs;
        else if (allocation.ownership == planning::AllocationOwnership::ExternalOutput) ++outputs;
    }
    if (owned != 1 || inputs != 1 || outputs != 2) return 4;

    for (const auto& handoff : plan.handoffs)
    {
        const auto producerOrdinal = std::find_if(
            plan.schedule.begin(), plan.schedule.end(),
            [&](const auto& entry) { return entry.leaf == handoff.producerLeaf; });
        const auto consumerOrdinal = std::find_if(
            plan.schedule.begin(), plan.schedule.end(),
            [&](const auto& entry) { return entry.leaf == handoff.consumerLeaf; });
        if (producerOrdinal == plan.schedule.end() ||
            consumerOrdinal == plan.schedule.end() ||
            producerOrdinal->ordinal >= consumerOrdinal->ordinal)
            return 5;
    }

    const auto bytes = planning::SerializeRawResourceFlowPlan(plan);
    auto decoded = planning::DeserializeRawResourceFlowPlan(bytes);
    if (!decoded || decoded.Value().identity != plan.identity ||
        planning::SerializeRawResourceFlowPlan(decoded.Value()) != bytes)
        return 6;

    auto repeated = planning::ProposeResourceFlowPlan(contract);
    if (!repeated || planning::SerializeRawResourceFlowPlan(repeated.Value()) != bytes)
        return 7;

    auto cycleContract = fixture::BuildCycleContract(
        fixtureResult.Value().producer, fixtureResult.Value().consumer);
    if (!cycleContract) return 8;
    auto cyclePlan = planning::ProposeResourceFlowPlan(cycleContract.Value());
    if (cyclePlan || cyclePlan.Error().stage != "plan/cycle") return 9;

    auto corruptContract = contract;
    corruptContract.identity[0] ^= std::byte{0x01};
    if (planning::ProposeResourceFlowPlan(corruptContract)) return 10;

    if (argc == 3 && std::string_view(argv[1]) == "--emit")
    {
        if (!WriteBytes(argv[2], bytes)) return 11;
    }

    std::cout << "L4V1-F4 Schema 18 Resource Flow Planning tests passed\n";
    std::cout << "Plan graph: 3 Leaves, 4 allocations, 6 bindings, 2 handoffs, 1 signal, 2 waits\n";
    std::cout << "Cycle gate: cyclic cross-Leaf Resource Flow rejected\n";
    std::cout << "Raw plan identity: " << sge4::base::ToHex(plan.identity) << '\n';
    return 0;
}
