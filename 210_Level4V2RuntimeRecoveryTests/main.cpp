#include "../192_Level4V2CandidatePlanner/CandidatePlanner.h"
#include "../193_Level4V2IndependentVerifier/IndependentVerifier.h"
#include "../194_Level4V2FrozenAuthority/FrozenAuthority.h"
#include "../197_Level4V2CompositionPlanner/CompositionPlanner.h"
#include "../198_Level4V2CompositionVerifier/CompositionVerifier.h"
#include "../199_Level4V2FrozenComposition/FrozenComposition.h"
#include "../202_Level4V2DynamicInvocationPlanner/DynamicInvocationPlanner.h"
#include "../203_Level4V2DynamicInvocationVerifier/DynamicInvocationVerifier.h"
#include "../204_Level4V2FrozenInvocationHistory/FrozenInvocationHistory.h"
#include "../209_Level4V2RuntimeRecovery/RuntimeRecovery.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace c = sge4_5::v2::canonical;
namespace a = sge4_5::v2::authority;
namespace fa = sge4_5::v2::frozen;
namespace comp = sge4_5::v2::composition;
namespace fc = sge4_5::v2::composition::frozen_composition_detail;
namespace dyn = sge4_5::v2::dynamic;
namespace fd = sge4_5::v2::dynamic::frozen_dynamic_detail;
namespace rt = sge4_5::v2::runtime_recovery;

namespace
{
std::vector<std::byte> Bytes(std::initializer_list<std::uint8_t> values)
{
    std::vector<std::byte> out;
    for (const auto value : values) out.push_back(static_cast<std::byte>(value));
    return out;
}

std::vector<std::byte> SeedBytes(std::uint8_t seed, std::uint8_t domain)
{
    return Bytes({domain, seed, static_cast<std::uint8_t>(seed ^ 0x5au), static_cast<std::uint8_t>(seed + 17u)});
}

fa::OpaqueFrozenArtifactV1 MakeFrozenAuthority(std::uint8_t seed)
{
    const auto semanticPayload = SeedBytes(seed, 1u);
    const auto semantic = a::MakeSemanticDefinitionV1(c::SemanticVersion(1u), semanticPayload);
    const auto epoch = c::DeviceEpoch::TryCreate(1u);
    if (!epoch) throw std::runtime_error("epoch");
    c::DynamicExecutionDimensionsV1 dimensions;
    dimensions.activeCount = c::ActiveCount(static_cast<std::uint32_t>(seed) + 1u);
    const auto invocation = a::MakeInvocationDescriptorV1(
        semantic.descriptor.identity, c::TimelineOrdinal(seed), *epoch, dimensions);
    const auto targetPayload = SeedBytes(9u, 2u);
    const auto resourcePayload = SeedBytes(7u, 3u);
    const auto writePayload = SeedBytes(seed, 4u);
    const a::AuthorityRequestV1 request{
        semantic,
        invocation,
        a::MakeTargetProfileDefinitionV1(1u, targetPayload),
        a::MakeResourceContractDefinitionV1(1u, resourcePayload),
        a::MakeWriteSetDefinitionV1(1u, writePayload)};
    const a::ExplicitCandidateInputV1 candidate{1u, SeedBytes(seed, 5u)};
    const auto proposal = a::CandidatePlannerV1::Plan(request, candidate);
    const auto verified = a::IndependentVerifierV1::Verify(request, candidate, proposal);
    if (!verified.Accepted()) throw std::runtime_error("authority verification");
    return fa::FrozenAuthorityBuilderV1::Freeze(*verified.verified);
}

comp::LeafAuthorityV1 MakeLeaf(std::uint8_t seed)
{
    const auto artifact = MakeFrozenAuthority(seed);
    const auto key = SeedBytes(seed, 10u);
    return comp::MakeLeafAuthorityV1(1u, key, artifact);
}

comp::EndpointAuthorityV1 MakeEndpoint(
    const comp::LeafAuthorityV1& leaf,
    std::uint8_t keySeed,
    comp::EndpointAccessV1 access)
{
    const auto key = SeedBytes(keySeed, 11u);
    const auto incoming = access == comp::EndpointAccessV1::ReadOnly ? comp::ResourceStateV1::Read : comp::ResourceStateV1::Common;
    const auto outgoing = access == comp::EndpointAccessV1::WriteOnly ? comp::ResourceStateV1::Write : comp::ResourceStateV1::Read;
    return comp::MakeEndpointAuthorityV1(
        leaf, 1u, key, comp::ResourceKindV1::Buffer, access,
        static_cast<std::uint64_t>(keySeed + 1u) * 16u,
        incoming, outgoing, true, true, false);
}

comp::FlowDeclarationV1 MakeFlow(
    std::uint8_t seed,
    comp::FlowBoundaryV1 boundary,
    std::initializer_list<comp::EndpointIdentity> endpoints)
{
    const auto key = SeedBytes(seed, 12u);
    const std::vector<comp::EndpointIdentity> identities(endpoints);
    return comp::MakeFlowDeclarationV1(1u, key, boundary, identities);
}

fc::OpaqueFrozenCompositionV1 BuildFrozenComposition(std::uint8_t offset)
{
    const auto first = MakeLeaf(static_cast<std::uint8_t>(1u + offset));
    const auto second = MakeLeaf(static_cast<std::uint8_t>(2u + offset));
    const auto firstIn = MakeEndpoint(first, static_cast<std::uint8_t>(1u + offset), comp::EndpointAccessV1::ReadOnly);
    const auto firstOut = MakeEndpoint(first, static_cast<std::uint8_t>(2u + offset), comp::EndpointAccessV1::WriteOnly);
    const auto secondIn = MakeEndpoint(second, static_cast<std::uint8_t>(3u + offset), comp::EndpointAccessV1::ReadOnly);
    const auto secondOut = MakeEndpoint(second, static_cast<std::uint8_t>(4u + offset), comp::EndpointAccessV1::WriteOnly);
    auto contract = comp::MakeRawCompositionContractV1(
        {first, second}, {firstIn, firstOut, secondIn, secondOut},
        {MakeFlow(static_cast<std::uint8_t>(1u + offset), comp::FlowBoundaryV1::CompositionInput, {firstIn.Identity()}),
         MakeFlow(static_cast<std::uint8_t>(2u + offset), comp::FlowBoundaryV1::Internal, {firstOut.Identity(), secondIn.Identity()}),
         MakeFlow(static_cast<std::uint8_t>(3u + offset), comp::FlowBoundaryV1::CompositionOutput, {secondOut.Identity()})});
    const auto proposal = comp::CompositionPlannerV1::Plan(contract);
    if (!proposal.Planned()) throw std::runtime_error("composition plan");
    const auto verified = comp::CompositionVerifierV1::Verify(contract, *proposal.proposal);
    if (!verified.Accepted()) throw std::runtime_error("composition verify");
    return fc::FrozenCompositionBuilderV1::Freeze(*verified.verified);
}

c::SemanticIdentity MakeSemantic(std::uint8_t seed)
{
    const auto payload = SeedBytes(seed, 21u);
    return c::MakeCanonicalIdentityV1<c::SemanticIdentity>({"SGE.V2.Runtime.TestSemantic.V1", 1u, payload});
}

dyn::ExactIndexSetV1 MakeSet(dyn::UniverseCount universe, std::initializer_list<std::uint32_t> indices)
{
    const std::vector<std::uint32_t> values(indices);
    auto result = dyn::BuildExactIndexSetV1(universe, values);
    if (!result.Accepted()) throw std::runtime_error("set");
    return *result.set;
}

fd::OpaqueFrozenDynamicInvocationV1 MakeInvocation(
    const fc::OpaqueFrozenCompositionV1& composition,
    c::SemanticIdentity semantic,
    std::uint64_t timeline,
    c::DeviceEpoch epoch,
    dyn::UniverseCount universe,
    dyn::InvocationModeV1 mode,
    dyn::ExactIndexSetV1 active,
    dyn::ExactIndexSetV1 modified,
    std::optional<dyn::VerifiedHistoryStateV1> history = std::nullopt)
{
    const auto request = dyn::MakeDynamicInvocationRequestV1(
        composition, semantic, c::TimelineOrdinal(timeline), epoch, universe,
        mode, std::move(active), std::move(modified), std::move(history));
    const auto proposal = dyn::DynamicInvocationPlannerV1::Plan(request);
    if (!proposal.Planned()) throw std::runtime_error("dynamic plan");
    const auto verified = dyn::DynamicInvocationVerifierV1::Verify(request, *proposal.proposal);
    if (!verified.Accepted()) throw std::runtime_error("dynamic verify");
    return fd::FrozenDynamicInvocationBuilderV1::Freeze(*verified.verified);
}

rt::AdapterIdentity Adapter(std::uint8_t seed)
{
    const auto bytes = SeedBytes(seed, 31u);
    return rt::ComputeAdapterIdentityV1(bytes);
}

c::ResourceInstanceIdentity ResourceIdentity(std::uint8_t seed)
{
    const auto bytes = SeedBytes(seed, 32u);
    return c::MakeCanonicalIdentityV1<c::ResourceInstanceIdentity>({"SGE.V2.Runtime.TestResource.V1", 1u, bytes});
}

rt::ExternalBindingSetV1 Bindings(
    const fc::OpaqueFrozenCompositionV1& composition,
    c::DeviceEpoch epoch,
    std::uint8_t seed = 1u)
{
    rt::ExternalBindingV1 binding{
        rt::ExternalSlotOrdinal(0u), ResourceIdentity(seed), {}};
    const auto digest = c::ComputeCanonicalDigestV1({"SGE.V2.Runtime.TestExternalState.V1", 1u, SeedBytes(seed, 33u)});
    binding.stateDigest = digest;
    return rt::MakeExternalBindingSetV1(composition.Identity(), epoch, {binding});
}

class QualificationBackend final : public rt::IRuntimeBackendV1
{
public:
    QualificationBackend(rt::AdapterIdentity first, rt::AdapterIdentity second)
        : adapters_{{first, true}, {second, false}} {}

    std::vector<rt::AdapterDescriptorV1> EnumerateAdapters() const override { return adapters_; }
    bool AcquireAdapter(const rt::AdapterIdentity& adapter) override
    {
        if (!rt::ContainsAdapterIdentityV1(adapters_, adapter)) return false;
        current_ = adapter; acquired_ = true; return true;
    }
    void ReleaseAllRuntimeObjects() noexcept override { materialized_ = false; rebound_ = false; }
    bool Materialize(const rt::NativeMaterializationRequestV1& request) override
    {
        if (!acquired_ || !current_.has_value() || request.adapterIdentity != *current_) return false;
        materialized_ = true; lastMaterialization_ = request; return true;
    }
    bool RebindExternalState(const rt::ExternalBindingSetV1& bindings) override
    {
        if (!materialized_) return false;
        rebound_ = true; lastBindings_ = bindings.identity; return true;
    }
    bool Submit(const rt::NativeSubmissionRequestV1& request) override
    {
        if (!materialized_ || !rebound_) return false;
        submissions_.push_back(request); return true;
    }
    bool ForceRemoveCurrentAdapter() override
    {
        if (!acquired_ || !current_.has_value()) return false;
        removed_.push_back(*current_); acquired_ = false; materialized_ = false; rebound_ = false; return true;
    }

    std::vector<rt::AdapterDescriptorV1> adapters_;
    std::optional<rt::AdapterIdentity> current_;
    std::vector<rt::AdapterIdentity> removed_;
    std::vector<rt::NativeSubmissionRequestV1> submissions_;
    std::optional<rt::NativeMaterializationRequestV1> lastMaterialization_;
    std::optional<rt::ExternalBindingSetIdentity> lastBindings_;
    bool acquired_ = false;
    bool materialized_ = false;
    bool rebound_ = false;
};

void Require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

void AppendU8(std::vector<std::byte>& bytes, std::uint8_t value) { bytes.push_back(static_cast<std::byte>(value)); }
void AppendU32(std::vector<std::byte>& bytes, std::uint32_t value)
{
    for (std::uint32_t index = 0; index < 4u; ++index)
        bytes.push_back(static_cast<std::byte>((value >> (index * 8u)) & 0xffu));
}
void AppendU64(std::vector<std::byte>& bytes, std::uint64_t value)
{
    for (std::uint32_t index = 0; index < 8u; ++index)
        bytes.push_back(static_cast<std::byte>((value >> (index * 8u)) & 0xffu));
}
void AppendDigest(std::vector<std::byte>& bytes, const c::Digest256V1& digest)
{
    bytes.insert(bytes.end(), digest.begin(), digest.end());
}
void Emit(const std::string& path, const std::vector<std::byte>& bytes)
{
    std::ofstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("open evidence");
    stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!stream) throw std::runtime_error("write evidence");
}
}

int main(int argc, char** argv)
{
    try
    {
        static_assert(!std::is_default_constructible_v<rt::LoadedRuntimeV1>);
        static_assert(!std::is_constructible_v<rt::LoadedRuntimeV1, dyn::DynamicPlannerProposalV1>);

        std::string emitPath;
        for (int index = 1; index < argc; ++index)
            if (std::string(argv[index]) == "--emit" && index + 1 < argc) emitPath = argv[++index];
        if (emitPath.empty()) throw std::runtime_error("--emit is required");

        const auto composition = BuildFrozenComposition(0u);
        const auto otherComposition = BuildFrozenComposition(40u);
        const auto semantic = MakeSemantic(1u);
        const auto firstAdapter = Adapter(1u);
        const auto secondAdapter = Adapter(2u);
        QualificationBackend backend(firstAdapter, secondAdapter);

        auto materialized = rt::MaterializeRuntimeV1(composition, firstAdapter, backend);
        Require(materialized.Accepted(), "materialize");
        auto loaded = std::move(*materialized.loaded);
        Require(loaded.State() == rt::RuntimeStateV1::ActiveNeedsExternalRebind, "initial state");
        const auto oldRepresentation = loaded.RepresentationHandle();
        const auto oldHistoryHandle = loaded.HistoryHandle();
        Require(rt::RebindExternalStateV1(loaded, Bindings(composition, loaded.Epoch())) == rt::RuntimeErrorV1::None, "initial rebind");

        const auto universe = dyn::UniverseCount(8u);
        const auto empty = MakeSet(universe, {});
        const auto initial = MakeInvocation(composition, semantic, 0u, loaded.Epoch(), universe,
            dyn::InvocationModeV1::InitialSeed, MakeSet(universe, {1u, 4u, 7u}), empty);
        auto firstSubmit = rt::SubmitFrozenInvocationV1(loaded, initial);
        Require(firstSubmit.Accepted(), "initial submit");
        Require(backend.submissions_.back().indirectWorkCount.Value() == 3u, "verified indirect count not consumed");

        std::vector<std::uint8_t> rejections;
        auto record = [&](rt::RuntimeErrorV1 actual, rt::RuntimeErrorV1 expected, const char* message) {
            Require(actual == expected, message); rejections.push_back(static_cast<std::uint8_t>(actual));
        };

        auto wrongCompositionInvocation = MakeInvocation(otherComposition, semantic, 0u, loaded.Epoch(), universe,
            dyn::InvocationModeV1::InitialSeed, MakeSet(universe, {1u}), empty);
        record(rt::SubmitFrozenInvocationV1(loaded, wrongCompositionInvocation).error,
            rt::RuntimeErrorV1::InvocationCompositionMismatch, "composition replay");

        const auto controlled = rt::RuntimeRecoveryCoordinatorV1::ControlledRebuild(loaded);
        Require(controlled.Accepted(), "controlled recovery");
        Require(controlled.report->newEpoch == controlled.report->previousEpoch + 1u, "epoch did not advance");
        Require(loaded.State() == rt::RuntimeStateV1::ActiveNeedsExternalRebind, "rebind state");
        record(rt::ValidateRuntimeHandleEpochV1(loaded, oldRepresentation), rt::RuntimeErrorV1::StaleHandle, "stale representation");
        record(rt::ValidateRuntimeHandleEpochV1(loaded, oldHistoryHandle), rt::RuntimeErrorV1::StaleHandle, "stale history handle");
        record(rt::SubmitFrozenInvocationV1(loaded, initial).error, rt::RuntimeErrorV1::ExternalRebindRequired, "submit before rebind");
        record(rt::RebindExternalStateV1(loaded, Bindings(composition, *c::DeviceEpoch::TryCreate(1u))),
            rt::RuntimeErrorV1::ExternalBindingEpochMismatch, "stale external binding");
        Require(rt::RebindExternalStateV1(loaded, Bindings(composition, loaded.Epoch(), 2u)) == rt::RuntimeErrorV1::None, "recovery rebind");

        const auto wrongSeed = MakeInvocation(composition, semantic, 0u, loaded.Epoch(), universe,
            dyn::InvocationModeV1::InitialSeed, MakeSet(universe, {1u, 4u, 7u}), empty);
        record(rt::SubmitFrozenInvocationV1(loaded, wrongSeed).error, rt::RuntimeErrorV1::RecoverySeedRequired, "initial accepted after recovery");
        const auto recoverySeed = MakeInvocation(composition, semantic, 0u, loaded.Epoch(), universe,
            dyn::InvocationModeV1::RecoverySeed, MakeSet(universe, {1u, 4u, 7u}), empty);
        auto recoverySubmit = rt::SubmitFrozenInvocationV1(loaded, recoverySeed);
        Require(recoverySubmit.Accepted(), "recovery seed");

        const auto staleEpochInvocation = MakeInvocation(composition, semantic, 0u, *c::DeviceEpoch::TryCreate(1u), universe,
            dyn::InvocationModeV1::RecoverySeed, MakeSet(universe, {1u, 4u, 7u}), empty);
        record(rt::SubmitFrozenInvocationV1(loaded, staleEpochInvocation).error,
            rt::RuntimeErrorV1::InvocationEpochMismatch, "stale invocation epoch");

        const auto removal = rt::RuntimeRecoveryCoordinatorV1::ForceRemovalForQualification(loaded);
        Require(removal.Accepted(), "force removal");
        Require(loaded.State() == rt::RuntimeStateV1::AwaitingAdapter, "not awaiting adapter");
        record(rt::SubmitFrozenInvocationV1(loaded, recoverySeed).error, rt::RuntimeErrorV1::RuntimeNotActive, "submit while awaiting");
        Require(rt::ContainsExcludedAdapterV1(loaded.ExcludedAdapters(), firstAdapter), "removed adapter not excluded");

        const std::array onlyRemoved{firstAdapter};
        const auto retryRemoved = rt::RuntimeRecoveryCoordinatorV1::RetryAdapterReacquisition(loaded, onlyRemoved);
        Require(retryRemoved.Accepted() && loaded.State() == rt::RuntimeStateV1::AwaitingAdapter, "excluded retry changed state");
        Require(!retryRemoved.report->newAdapter.has_value(), "excluded adapter reacquired");

        const std::array ordered{firstAdapter, secondAdapter};
        const auto retrySecond = rt::RuntimeRecoveryCoordinatorV1::RetryAdapterReacquisition(loaded, ordered);
        Require(retrySecond.Accepted() && retrySecond.report->newAdapter == secondAdapter, "explicit candidate order not honored");
        Require(loaded.State() == rt::RuntimeStateV1::ActiveNeedsExternalRebind, "retry did not require rebind");
        record(rt::SubmitFrozenInvocationV1(loaded, recoverySeed).error, rt::RuntimeErrorV1::ExternalRebindRequired, "retry submit before rebind");
        Require(rt::RebindExternalStateV1(loaded, Bindings(composition, loaded.Epoch(), 3u)) == rt::RuntimeErrorV1::None, "retry rebind");
        const auto retrySeed = MakeInvocation(composition, semantic, 0u, loaded.Epoch(), universe,
            dyn::InvocationModeV1::RecoverySeed, MakeSet(universe, {2u, 6u}), empty);
        Require(rt::SubmitFrozenInvocationV1(loaded, retrySeed).Accepted(), "retry recovery seed");

        auto duplicateBindings = Bindings(composition, loaded.Epoch(), 4u);
        duplicateBindings.bindings.push_back(duplicateBindings.bindings.front());
        duplicateBindings.identity = rt::ComputeExternalBindingSetIdentityV1(duplicateBindings);
        record(rt::RebindExternalStateV1(loaded, duplicateBindings), rt::RuntimeErrorV1::ExternalBindingDuplicateSlot, "duplicate slot");

        QualificationBackend missingBackend(firstAdapter, secondAdapter);
        record(rt::MaterializeRuntimeV1(composition, Adapter(99u), missingBackend).error,
            rt::RuntimeErrorV1::AdapterNotEnumerated, "unknown adapter");
        record(rt::RuntimeRecoveryCoordinatorV1::RetryAdapterReacquisition(loaded, ordered).error,
            rt::RuntimeErrorV1::RetryNotAwaitingAdapter, "retry while active");

        std::vector<std::byte> evidence;
        evidence.insert(evidence.end(), {std::byte{'L'}, std::byte{'4'}, std::byte{'V'}, std::byte{'2'}, std::byte{'R'}, std::byte{'5'}, std::byte{'V'}, std::byte{'1'}});
        AppendU32(evidence, 10u);
        AppendU32(evidence, static_cast<std::uint32_t>(rejections.size()));
        AppendDigest(evidence, composition.Identity().Digest());
        AppendDigest(evidence, loaded.Identity().Digest());
        AppendDigest(evidence, retrySecond.report->identity.Digest());
        AppendDigest(evidence, retrySeed.Identity().Digest());
        AppendU64(evidence, loaded.Epoch().Value());
        AppendU32(evidence, static_cast<std::uint32_t>(backend.submissions_.size()));
        for (const auto rejection : rejections) AppendU8(evidence, rejection);
        Emit(emitPath, evidence);

        std::cout << "Level 4 v2 R5 runtime and recovery self-test passed.\n";
        std::cout << "Accepted scenarios: 10\n";
        std::cout << "Runtime/recovery rejections: " << rejections.size() << "\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "R5 failure: " << error.what() << '\n';
        return 1;
    }
}
