#include "../46D_Schema18ResourceFlowPlanningTests/TestFixture.h"

#include "../00_Foundation/Sha256.h"
#include "../17A_Schema18ResourceFlowPlan/Schema18ResourceFlowPlan.h"
#include "../18A_Schema18ResourceFlowVerifier/Schema18ResourceFlowVerifier.h"

#include <iostream>
#include <type_traits>

namespace
{
namespace fixture = sge4::qualification::schema18_planning_fixture;
namespace planning = sge4::composition::schema18::planning;
namespace verification = sge4::composition::schema18::verification;

void Resign(planning::RawResourceFlowPlan& plan)
{
    plan.identity = planning::ComputeRawResourceFlowPlanIdentity(plan);
}

bool Rejected(
    const sge4::composition::schema18::PackageCompositionContract& contract,
    const planning::RawResourceFlowPlan& plan)
{
    return !verification::VerifyAndSeal(contract, plan);
}
}

int main()
{
    static_assert(!std::is_default_constructible_v<
        verification::VerifiedResourceFlowPlan>);

    auto fixtureResult = fixture::BuildFixture();
    if (!fixtureResult)
    {
        std::cerr << fixtureResult.Error() << '\n';
        return 1;
    }
    const auto& contract = fixtureResult.Value().contract;
    auto proposed = planning::ProposeResourceFlowPlan(contract);
    if (!proposed) return 2;
    const auto raw = proposed.Value();

    auto verified = verification::VerifyAndSeal(contract, raw);
    if (!verified) return 3;
    if (!verification::ValidateVerifiedPlan(contract, verified.Value())) return 4;
    if (verified.Value().Seal() != verification::ComputeVerifierSeal(
            contract.identity, raw.identity)) return 5;

    auto corruptIdentity = raw;
    corruptIdentity.identity[0] ^= std::byte{0x01};
    if (!Rejected(contract, corruptIdentity)) return 6;

    auto contractIdentityForgery = raw;
    contractIdentityForgery.contractIdentity[0] ^= std::byte{0x01};
    Resign(contractIdentityForgery);
    if (!Rejected(contract, contractIdentityForgery)) return 7;

    auto allocationForgery = raw;
    allocationForgery.allocations[0].sizeBytes += 16;
    Resign(allocationForgery);
    if (!Rejected(contract, allocationForgery)) return 8;

    auto scheduleForgery = raw;
    std::swap(scheduleForgery.schedule[0], scheduleForgery.schedule[1]);
    scheduleForgery.schedule[0].ordinal = 0;
    scheduleForgery.schedule[1].ordinal = 1;
    Resign(scheduleForgery);
    if (!Rejected(contract, scheduleForgery)) return 9;

    auto bindingForgery = raw;
    bindingForgery.bindings[0].resource = {
        (bindingForgery.bindings[0].resource.value + 1) %
        static_cast<std::uint32_t>(raw.allocations.size())};
    Resign(bindingForgery);
    if (!Rejected(contract, bindingForgery)) return 10;

    auto handoffForgery = raw;
    handoffForgery.handoffs[0].consumerIncomingState.explicitBits ^= 1u;
    Resign(handoffForgery);
    if (!Rejected(contract, handoffForgery)) return 11;

    auto signalForgery = raw;
    signalForgery.signals[0].producerScheduleOrdinal += 1;
    Resign(signalForgery);
    if (!Rejected(contract, signalForgery)) return 12;

    auto waitForgery = raw;
    waitForgery.waits[0].consumerScheduleOrdinal =
        waitForgery.signals[0].producerScheduleOrdinal;
    Resign(waitForgery);
    if (!Rejected(contract, waitForgery)) return 13;

    auto recoveryForgery = raw;
    recoveryForgery.recovery.recreateResources.clear();
    Resign(recoveryForgery);
    if (!Rejected(contract, recoveryForgery)) return 14;

    auto accessForgery = raw;
    accessForgery.bindings[0].access =
        accessForgery.bindings[0].access ==
            sge4::package::d3d12_v18::EndpointAccess::ReadOnly ?
            sge4::package::d3d12_v18::EndpointAccess::WriteOnly :
            sge4::package::d3d12_v18::EndpointAccess::ReadOnly;
    Resign(accessForgery);
    if (!Rejected(contract, accessForgery)) return 15;

    auto cycleContract = fixture::BuildCycleContract(
        fixtureResult.Value().producer, fixtureResult.Value().consumer);
    if (!cycleContract) return 16;
    // An attacker cannot bypass cycle rejection by presenting the valid acyclic raw plan.
    auto cycleTargetForgery = raw;
    cycleTargetForgery.contractIdentity = cycleContract.Value().identity;
    Resign(cycleTargetForgery);
    if (!Rejected(cycleContract.Value(), cycleTargetForgery)) return 17;

    std::cout << "L4V1-F5 Schema 18 independent Resource Flow verification tests passed\n";
    std::cout << "Authority gates: contract, allocation, schedule, binding, state, signal/wait, recovery\n";
    std::cout << "Raw-plan direct authority: rejected; Verified Plan construction: verifier-only\n";
    std::cout << "Verifier seal: " << sge4::base::ToHex(verified.Value().Seal()) << '\n';
    return 0;
}
