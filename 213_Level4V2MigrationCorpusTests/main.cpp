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
#include "../212_Level4V2MigrationCorpus/MigrationCorpus.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <optional>
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
namespace mig = sge4_5::v2::migration;

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
    const a::AuthorityRequestV1 request{
        semantic,
        invocation,
        a::MakeTargetProfileDefinitionV1(1u, SeedBytes(9u, 2u)),
        a::MakeResourceContractDefinitionV1(1u, SeedBytes(7u, 3u)),
        a::MakeWriteSetDefinitionV1(1u, SeedBytes(seed, 4u))};
    const a::ExplicitCandidateInputV1 candidate{1u, SeedBytes(seed, 5u)};
    const auto proposal = a::CandidatePlannerV1::Plan(request, candidate);
    const auto verified = a::IndependentVerifierV1::Verify(request, candidate, proposal);
    if (!verified.Accepted()) throw std::runtime_error("authority verification");
    return fa::FrozenAuthorityBuilderV1::Freeze(*verified.verified);
}

comp::LeafAuthorityV1 MakeLeaf(std::uint8_t seed)
{
    const auto artifact = MakeFrozenAuthority(seed);
    return comp::MakeLeafAuthorityV1(1u, SeedBytes(seed, 10u), artifact);
}

comp::EndpointAuthorityV1 MakeEndpoint(
    const comp::LeafAuthorityV1& leaf,
    std::uint8_t keySeed,
    comp::EndpointAccessV1 access)
{
    const auto incoming = access == comp::EndpointAccessV1::ReadOnly ? comp::ResourceStateV1::Read : comp::ResourceStateV1::Common;
    const auto outgoing = access == comp::EndpointAccessV1::WriteOnly ? comp::ResourceStateV1::Write : comp::ResourceStateV1::Read;
    return comp::MakeEndpointAuthorityV1(
        leaf, 1u, SeedBytes(keySeed, 11u), comp::ResourceKindV1::Buffer, access,
        static_cast<std::uint64_t>(keySeed + 1u) * 16u,
        incoming, outgoing, true, true, false);
}

comp::FlowDeclarationV1 MakeFlow(
    std::uint8_t seed,
    comp::FlowBoundaryV1 boundary,
    std::initializer_list<comp::EndpointIdentity> endpoints)
{
    const std::vector<comp::EndpointIdentity> identities(endpoints);
    return comp::MakeFlowDeclarationV1(1u, SeedBytes(seed, 12u), boundary, identities);
}

fc::OpaqueFrozenCompositionV1 BuildFrozenComposition()
{
    const auto first = MakeLeaf(1u);
    const auto second = MakeLeaf(2u);
    const auto firstIn = MakeEndpoint(first, 1u, comp::EndpointAccessV1::ReadOnly);
    const auto firstOut = MakeEndpoint(first, 2u, comp::EndpointAccessV1::WriteOnly);
    const auto secondIn = MakeEndpoint(second, 3u, comp::EndpointAccessV1::ReadOnly);
    const auto secondOut = MakeEndpoint(second, 4u, comp::EndpointAccessV1::WriteOnly);
    const auto contract = comp::MakeRawCompositionContractV1(
        {first, second}, {firstIn, firstOut, secondIn, secondOut},
        {MakeFlow(1u, comp::FlowBoundaryV1::CompositionInput, {firstIn.Identity()}),
         MakeFlow(2u, comp::FlowBoundaryV1::Internal, {firstOut.Identity(), secondIn.Identity()}),
         MakeFlow(3u, comp::FlowBoundaryV1::CompositionOutput, {secondOut.Identity()})});
    const auto proposal = comp::CompositionPlannerV1::Plan(contract);
    if (!proposal.Planned()) throw std::runtime_error("composition plan");
    const auto verified = comp::CompositionVerifierV1::Verify(contract, *proposal.proposal);
    if (!verified.Accepted()) throw std::runtime_error("composition verification");
    return fc::FrozenCompositionBuilderV1::Freeze(*verified.verified);
}

c::SemanticIdentity MakeSemantic()
{
    const auto payload = SeedBytes(1u, 21u);
    return c::MakeCanonicalIdentityV1<c::SemanticIdentity>({"SGE.V2.Migration.TestSemantic.V1", 1u, payload});
}

dyn::ExactIndexSetV1 MakeSet(dyn::UniverseCount universe, std::initializer_list<std::uint32_t> indices)
{
    const std::vector<std::uint32_t> values(indices);
    const auto result = dyn::BuildExactIndexSetV1(universe, values);
    if (!result.Accepted()) throw std::runtime_error("exact set");
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
    if (!verified.Accepted()) throw std::runtime_error("dynamic verification");
    return fd::FrozenDynamicInvocationBuilderV1::Freeze(*verified.verified);
}

rt::AdapterIdentity Adapter(std::uint8_t seed)
{
    return rt::ComputeAdapterIdentityV1(SeedBytes(seed, 31u));
}

c::ResourceInstanceIdentity ResourceIdentity(std::uint8_t seed)
{
    const auto bytes = SeedBytes(seed, 32u);
    return c::MakeCanonicalIdentityV1<c::ResourceInstanceIdentity>({"SGE.V2.Migration.TestResource.V1", 1u, bytes});
}

rt::ExternalBindingSetV1 Bindings(
    const fc::OpaqueFrozenCompositionV1& composition,
    c::DeviceEpoch epoch,
    std::uint8_t seed)
{
    rt::ExternalBindingV1 binding{rt::ExternalSlotOrdinal(0u), ResourceIdentity(seed), {}};
    binding.stateDigest = c::ComputeCanonicalDigestV1({"SGE.V2.Migration.ExternalState.V1", 1u, SeedBytes(seed, 33u)});
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
        current_ = adapter;
        acquired_ = true;
        return true;
    }
    void ReleaseAllRuntimeObjects() noexcept override { materialized_ = false; rebound_ = false; }
    bool Materialize(const rt::NativeMaterializationRequestV1& request) override
    {
        if (!acquired_ || !current_.has_value() || request.adapterIdentity != *current_) return false;
        materialized_ = true;
        return true;
    }
    bool RebindExternalState(const rt::ExternalBindingSetV1&) override
    {
        if (!materialized_) return false;
        rebound_ = true;
        return true;
    }
    bool Submit(const rt::NativeSubmissionRequestV1& request) override
    {
        if (!materialized_ || !rebound_) return false;
        submissions_.push_back(request);
        return true;
    }
    bool ForceRemoveCurrentAdapter() override
    {
        if (!acquired_ || !current_.has_value()) return false;
        acquired_ = false;
        materialized_ = false;
        rebound_ = false;
        return true;
    }

    std::vector<rt::AdapterDescriptorV1> adapters_;
    std::optional<rt::AdapterIdentity> current_;
    std::vector<rt::NativeSubmissionRequestV1> submissions_;
    bool acquired_ = false;
    bool materialized_ = false;
    bool rebound_ = false;
};

void Require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

mig::SourceLineageV1 Source(const char* role, const char* sha)
{
    return {role, mig::ParseSha256HexV1(sha)};
}

mig::EvidenceRecordV1 Evidence(const char* id, mig::EvidenceLaneV1 lane, const char* sha)
{
    return {id, lane, mig::ParseSha256HexV1(sha)};
}

mig::MigrationCaseV1 Case(
    const char* id,
    mig::EvidenceLaneV1 lane,
    std::initializer_list<const char*> sources,
    std::initializer_list<std::uint32_t> invariants,
    const c::Digest256V1& authority,
    std::initializer_list<const char*> evidence)
{
    mig::MigrationCaseV1 result;
    result.id = id;
    result.lane = lane;
    for (const auto* source : sources) result.sourceRoles.emplace_back(source);
    result.invariantOrdinals.assign(invariants.begin(), invariants.end());
    result.bindingKind = lane == mig::EvidenceLaneV1::Correctness
        ? mig::BindingKindV1::V2Authority
        : mig::BindingKindV1::RetainedObservationalEvidence;
    result.boundIdentity = authority;
    for (const auto* item : evidence) result.evidenceIds.emplace_back(item);
    return result;
}

mig::MigrationCorpusInputV1 BuildCorpus(
    const c::SemanticIdentity& semantic,
    const fa::OpaqueFrozenArtifactV1& leaf,
    const fc::OpaqueFrozenCompositionV1& composition,
    const fd::OpaqueFrozenDynamicInvocationV1& initial,
    const rt::RuntimeInstanceIdentity& initialRuntime,
    const rt::RecoveryReportV1& controlled,
    const fd::OpaqueFrozenDynamicInvocationV1& recoverySeed,
    const rt::RuntimeSubmissionV1& recoverySubmission,
    const rt::RecoveryReportV1& removal)
{
    using Lane = mig::EvidenceLaneV1;
    mig::MigrationCorpusInputV1 input;
    input.sources = {
        Source("Level4V1ReconstructionPolicy", "b296dba718ed87b76af4f7a06433f9ea8972d2264fa8032a640bfaa66661b56c"),
        Source("Level4V1RuntimeRecoveryQualification", "3d83adeabb73b0e496fbbb78ea9c646abad6f98c36fcffca36da23a53e829a96"),
        Source("Spiral1ProofLedger", "3c7a687d682200d640892b64e084714cf21105a39330a77b0bb3e4d976e8c4b2"),
        Source("Spiral2ProofLedger", "617f9bf43e9b99154f0212974bfd47dae525751cc42a2d9b3af18054a4fdb083"),
        Source("Spiral3ProofLedger", "f426d7df7a1ed0cd0414b2eb88072e7be970be29eb6fda57ddaed2c91214530f"),
        Source("Spiral4ProofLedger", "fd1ff7ab08dfb5d080ff4abb8db487a4adb18e44e9df08d345f4aec4383b252b"),
        Source("Spiral5ProofLedger", "7816b49fa61641e717d94314c9ce994bb58ba8a5608cb7ca87aca34a5d0fe4fd"),
        Source("Spiral6ProofLedger", "2b86deae0562602ba2d43bb2e34dc4f03d988cf21702f92c7b5ccc425c903c2c"),
        Source("Spiral7ProofLedger", "a91bae148b77f175432b0b6b3a13a869b2cead751c61ec005be344f2332ea175"),
        Source("Spiral7Closure", "c4c7927ea02b0f926f8f54fa4a4482e4d53f550b5c3acf747ef17e42092eaca1"),
        Source("AcceptedArchitectureQualification", "4b9db516ba8809d482e039394a9cbb284c0bb4d91600ddb1ac654ab46e652766"),
        Source("AcceptedObservationalMeasurement", "76aee55429d6c67db5242987f879785a72f5bec142fc59fc363312eae44f747d"),
        Source("FinalDecisionSummary", "97345b8190627308697ec22c7da43232cea07f218918037eccebe0ad534abf5c"),
        Source("CanonicalInputNarrative", "80d0d983dd0b9f5120a426b298aac65fefca7684157ad4712ebdd6a4b1bb5110"),
        Source("CapabilityMatrix", "c3c06debdda4e555121fb7ba3f13d935b58edb00b15baedb9c38a6b955f31d72"),
        Source("RetainedReferenceLayout", "a336bf09bed936461ded74c504ce417cdfe73ee301999a2c50ec091d0a928026")};

    input.evidence = {
        Evidence("R1", Lane::Correctness, "35e751e131dcf4c88cad1b2da7bbc8e745f497e56d5ba22bd23079c7e2ec2abf"),
        Evidence("R2", Lane::Correctness, "745429438eb047adf4e8d1e85f69d0d0d2228ebc01fe3ec132fa2b2705ed8a33"),
        Evidence("R3", Lane::Correctness, "dab306b33d0816b742c61611c28bf054d2df5dbfe2e39179a1fae4c7fb70befb"),
        Evidence("R4", Lane::Correctness, "78f16d2fa4185412144205dc54f842f2929e6e269711dfca9ced9967f37cfbe1"),
        Evidence("R5C", Lane::Correctness, "40d78dc35894a55e50b314bd0afcbb60966f92e942ac70856155964cd8e66498"),
        Evidence("R5W", Lane::Correctness, "1e4adaa04dc8d1d235f8f3438f4295b09b36480174f6a7d9fa0fbaba79ea7a9a"),
        Evidence("ARCH", Lane::Correctness, "4b9db516ba8809d482e039394a9cbb284c0bb4d91600ddb1ac654ab46e652766"),
        Evidence("RETENTION", Lane::Correctness, "a336bf09bed936461ded74c504ce417cdfe73ee301999a2c50ec091d0a928026"),
        Evidence("OBS-BUNDLE", Lane::ObservationalPerformance, "9ee852c2a2574a8cb9c163b6e94213b855317bba74319e6443e9aa98c10c24d9"),
        Evidence("OBS-DECISION", Lane::ObservationalPerformance, "b840cb1425f5e45ccac2f0c84750584313350458062dab98f8552290e8bfda81")};

    for (std::uint32_t ordinal = 1u; ordinal <= mig::RequiredInvariantCountV1; ++ordinal)
    {
        std::string evidenceId;
        if (ordinal == 1u || ordinal == 6u || ordinal == 20u || ordinal == 40u) evidenceId = "R1";
        else if ((ordinal >= 7u && ordinal <= 13u) || ordinal == 38u || ordinal == 39u) evidenceId = "R2";
        else if ((ordinal >= 14u && ordinal <= 17u) || ordinal == 19u) evidenceId = "R3";
        else if ((ordinal >= 2u && ordinal <= 5u) || (ordinal >= 21u && ordinal <= 26u)) evidenceId = "R4";
        else if (ordinal == 18u || (ordinal >= 27u && ordinal <= 31u) || ordinal == 35u) evidenceId = "R5C";
        else if (ordinal == 32u) evidenceId = "ARCH";
        else if (ordinal == 33u || ordinal == 34u || ordinal == 36u) evidenceId = "OBS-BUNDLE";
        else if (ordinal == 37u) evidenceId = "RETENTION";
        else throw std::runtime_error("unmapped invariant");
        input.invariants.push_back({ordinal, mig::MigrationOutcomeV1::Reproduced, {evidenceId}});
    }

    const auto r1 = input.evidence[0].evidenceSha256;
    const auto r5w = input.evidence[5].evidenceSha256;
    const auto arch = input.evidence[6].evidenceSha256;
    const auto retention = input.evidence[7].evidenceSha256;
    const auto observation = input.evidence[8].evidenceSha256;
    const auto decision = input.evidence[9].evidenceSha256;
    input.cases = {
        Case("C01-SemanticExecutionSeparation", Lane::Correctness, {"Level4V1ReconstructionPolicy","Spiral1ProofLedger","CanonicalInputNarrative"}, {1u,6u,40u}, semantic.Digest(), {"R1"}),
        Case("C02-StrongVocabulary", Lane::Correctness, {"CapabilityMatrix","CanonicalInputNarrative"}, {20u}, r1, {"R1"}),
        Case("C03-UnifiedAuthority", Lane::Correctness, {"Spiral1ProofLedger","Spiral2ProofLedger","Spiral3ProofLedger","Spiral4ProofLedger","Spiral5ProofLedger","Spiral6ProofLedger","Spiral7ProofLedger"}, {7u,8u,9u,10u,13u,38u,39u}, leaf.Descriptor().identity.Digest(), {"R2"}),
        Case("C04-FrozenIdentityBinding", Lane::Correctness, {"AcceptedArchitectureQualification","Level4V1RuntimeRecoveryQualification"}, {11u,12u}, leaf.Descriptor().identity.Digest(), {"R2"}),
        Case("C05-FiniteBufferDag", Lane::Correctness, {"Level4V1ReconstructionPolicy","CapabilityMatrix"}, {14u,15u}, composition.Identity().Digest(), {"R3"}),
        Case("C06-OptionalPresenter", Lane::Correctness, {"Level4V1ReconstructionPolicy"}, {16u}, composition.Identity().Digest(), {"R3"}),
        Case("C07-SingleAdapterBoundary", Lane::Correctness, {"Level4V1ReconstructionPolicy","CanonicalInputNarrative"}, {17u,19u}, composition.Identity().Digest(), {"R3"}),
        Case("C08-WholeCompositionRecovery", Lane::Correctness, {"Level4V1RuntimeRecoveryQualification"}, {18u}, controlled.identity.Digest(), {"R5C"}),
        Case("C09-ExplicitDynamicInvocation", Lane::Correctness, {"Spiral4ProofLedger","Spiral5ProofLedger","Spiral6ProofLedger","Spiral7ProofLedger"}, {2u}, initial.Identity().Digest(), {"R4"}),
        Case("C10-ExternalMembership", Lane::Correctness, {"Spiral7ProofLedger"}, {3u}, initial.NextHistory().ActiveSet().Identity().Digest(), {"R4"}),
        Case("C11-ExactTransitionAlgebra", Lane::Correctness, {"Spiral7ProofLedger"}, {4u}, initial.DecisionIdentity().Digest(), {"R4"}),
        Case("C12-FirstFullActiveSeed", Lane::Correctness, {"Spiral7ProofLedger"}, {5u}, initial.Identity().Digest(), {"R4"}),
        Case("C13-VerifiedIndirectQuantity", Lane::Correctness, {"Spiral4ProofLedger"}, {21u}, initial.WriteSetIdentity().Digest(), {"R4"}),
        Case("C14-ExplicitHistory", Lane::Correctness, {"Spiral5ProofLedger","Spiral7ProofLedger"}, {22u,26u}, initial.NextHistory().Descriptor().identity.Digest(), {"R4"}),
        Case("C15-ExactSparseMembership", Lane::Correctness, {"Spiral6ProofLedger"}, {23u}, initial.NextHistory().ActiveSet().Identity().Digest(), {"R4"}),
        Case("C16-SparseTemporalWriteSet", Lane::Correctness, {"Spiral7ProofLedger"}, {24u,25u}, initial.WriteSetIdentity().Digest(), {"R4"}),
        Case("C17-EpochBoundHandles", Lane::Correctness, {"Spiral7ProofLedger","AcceptedArchitectureQualification"}, {27u}, initialRuntime.Digest(), {"R5C"}),
        Case("C18-RecoveryInvalidatesTemporalState", Lane::Correctness, {"AcceptedArchitectureQualification","Level4V1RuntimeRecoveryQualification"}, {28u}, controlled.identity.Digest(), {"R5C"}),
        Case("C19-ExplicitExternalRebind", Lane::Correctness, {"Spiral7ProofLedger","AcceptedArchitectureQualification"}, {29u}, recoverySubmission.identity.Digest(), {"R5C"}),
        Case("C20-RecoverySeed", Lane::Correctness, {"Spiral7ProofLedger","AcceptedArchitectureQualification"}, {30u}, recoverySeed.Identity().Digest(), {"R5C"}),
        Case("C21-RemovedAdapterExclusion", Lane::Correctness, {"AcceptedArchitectureQualification"}, {31u}, removal.identity.Digest(), {"R5W"}),
        Case("C22-CorrectnessMeasurementSeparation", Lane::Correctness, {"AcceptedArchitectureQualification"}, {32u}, arch, {"ARCH"}),
        Case("C23-ObservationIdentityAndPairedDecision", Lane::ObservationalPerformance, {"AcceptedObservationalMeasurement","FinalDecisionSummary"}, {33u,34u}, decision, {"OBS-DECISION"}),
        Case("C24-NoRuntimePerformancePolicy", Lane::Correctness, {"Spiral7Closure","FinalDecisionSummary"}, {35u}, r5w, {"R5W"}),
        Case("C25-ExternalRawEvidenceRetention", Lane::ObservationalPerformance, {"Spiral7Closure","AcceptedObservationalMeasurement"}, {36u}, observation, {"OBS-BUNDLE"}),
        Case("C26-ReferenceRetentionUntilDecision", Lane::Correctness, {"RetainedReferenceLayout","CanonicalInputNarrative"}, {37u}, retention, {"RETENTION"})};
    return input;
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
        static_assert(!std::is_default_constructible_v<mig::OpaqueMigrationCertificateV1>);
        static_assert(!std::is_constructible_v<mig::OpaqueMigrationCertificateV1, mig::MigrationCorpusInputV1>);

        std::string emitPath;
        for (int index = 1; index < argc; ++index)
            if (std::string(argv[index]) == "--emit" && index + 1 < argc) emitPath = argv[++index];
        if (emitPath.empty()) throw std::runtime_error("--emit is required");

        const auto leaf = MakeFrozenAuthority(1u);
        const auto composition = BuildFrozenComposition();
        const auto semantic = MakeSemantic();
        const auto firstAdapter = Adapter(1u);
        const auto secondAdapter = Adapter(2u);
        QualificationBackend backend(firstAdapter, secondAdapter);
        auto materialized = rt::MaterializeRuntimeV1(composition, firstAdapter, backend);
        Require(materialized.Accepted(), "materialize");
        auto loaded = std::move(*materialized.loaded);
        const auto initialRuntime = loaded.Identity();
        Require(rt::RebindExternalStateV1(loaded, Bindings(composition, loaded.Epoch(), 1u)) == rt::RuntimeErrorV1::None, "initial rebind");

        const auto universe = dyn::UniverseCount(8u);
        const auto empty = MakeSet(universe, {});
        const auto initial = MakeInvocation(
            composition, semantic, 0u, loaded.Epoch(), universe,
            dyn::InvocationModeV1::InitialSeed, MakeSet(universe, {1u,4u,7u}), empty);
        Require(rt::SubmitFrozenInvocationV1(loaded, initial).Accepted(), "initial submit");

        const auto controlled = rt::RuntimeRecoveryCoordinatorV1::ControlledRebuild(loaded);
        Require(controlled.Accepted(), "controlled recovery");
        Require(rt::RebindExternalStateV1(loaded, Bindings(composition, loaded.Epoch(), 2u)) == rt::RuntimeErrorV1::None, "recovery rebind");
        const auto recoverySeed = MakeInvocation(
            composition, semantic, 0u, loaded.Epoch(), universe,
            dyn::InvocationModeV1::RecoverySeed, MakeSet(universe, {1u,4u,7u}), empty);
        const auto recoverySubmission = rt::SubmitFrozenInvocationV1(loaded, recoverySeed);
        Require(recoverySubmission.Accepted(), "recovery submission");

        const auto removal = rt::RuntimeRecoveryCoordinatorV1::ForceRemovalForQualification(loaded);
        Require(removal.Accepted(), "forced removal");
        const std::array ordered{firstAdapter, secondAdapter};
        const auto reacquired = rt::RuntimeRecoveryCoordinatorV1::RetryAdapterReacquisition(loaded, ordered);
        Require(reacquired.Accepted() && reacquired.report->newAdapter == secondAdapter, "removed adapter exclusion");

        const auto corpus = BuildCorpus(
            semantic, leaf, composition, initial, initialRuntime,
            *controlled.report, recoverySeed, *recoverySubmission.submission, *removal.report);
        const auto certificate = mig::BuildMigrationCertificateV1(corpus);
        Require(certificate.Accepted(), "migration certificate");
        Require(certificate.certificate->SourceCount() == 16u, "source count");
        Require(certificate.certificate->InvariantCount() == 40u, "invariant count");
        Require(certificate.certificate->CaseCount() == 26u, "case count");
        Require(certificate.certificate->CorrectnessCaseCount() == 24u, "correctness case count");
        Require(certificate.certificate->ObservationalCaseCount() == 2u, "observational case count");

        std::vector<std::uint8_t> rejected;
        auto expect = [&](mig::MigrationCorpusInputV1 mutated, mig::MigrationCorpusErrorV1 expected, const char* message) {
            const auto result = mig::BuildMigrationCertificateV1(mutated);
            Require(result.error == expected && !result.certificate.has_value(), message);
            rejected.push_back(static_cast<std::uint8_t>(result.error));
        };
        { auto x=corpus; x.sources.pop_back(); expect(std::move(x), mig::MigrationCorpusErrorV1::SourceCountMismatch, "source count rejection"); }
        { auto x=corpus; x.sources[1].role=x.sources[0].role; expect(std::move(x), mig::MigrationCorpusErrorV1::DuplicateSourceRole, "duplicate source rejection"); }
        { auto x=corpus; x.sources[0].sourceSha256={}; expect(std::move(x), mig::MigrationCorpusErrorV1::InvalidSourceDigest, "invalid source digest rejection"); }
        { auto x=corpus; x.evidence[1].id=x.evidence[0].id; expect(std::move(x), mig::MigrationCorpusErrorV1::DuplicateEvidenceId, "duplicate evidence rejection"); }
        { auto x=corpus; x.evidence[0].evidenceSha256={}; expect(std::move(x), mig::MigrationCorpusErrorV1::InvalidEvidenceDigest, "invalid evidence digest rejection"); }
        { auto x=corpus; x.invariants[1].ordinal=x.invariants[0].ordinal; expect(std::move(x), mig::MigrationCorpusErrorV1::DuplicateInvariant, "duplicate invariant rejection"); }
        { auto x=corpus; x.invariants.pop_back(); expect(std::move(x), mig::MigrationCorpusErrorV1::IncompleteInvariantSet, "incomplete invariant rejection"); }
        { auto x=corpus; x.invariants[0].outcome=static_cast<mig::MigrationOutcomeV1>(255u); expect(std::move(x), mig::MigrationCorpusErrorV1::NonTerminalOutcome, "non-terminal outcome rejection"); }
        { auto x=corpus; x.invariants[0].evidenceIds.clear(); expect(std::move(x), mig::MigrationCorpusErrorV1::InvariantEvidenceMissing, "missing invariant Evidence rejection"); }
        { auto x=corpus; x.cases[1].id=x.cases[0].id; expect(std::move(x), mig::MigrationCorpusErrorV1::DuplicateCaseId, "duplicate case rejection"); }
        { auto x=corpus; x.cases[0].sourceRoles[0]="UnknownSource"; expect(std::move(x), mig::MigrationCorpusErrorV1::UnknownSourceReference, "unknown source rejection"); }
        { auto x=corpus; x.cases[0].evidenceIds[0]="UNKNOWN"; expect(std::move(x), mig::MigrationCorpusErrorV1::UnknownEvidenceReference, "unknown evidence rejection"); }
        { auto x=corpus; x.cases[0].invariantOrdinals[0]=99u; expect(std::move(x), mig::MigrationCorpusErrorV1::UnknownInvariantReference, "unknown invariant rejection"); }
        { auto x=corpus; x.cases[0].boundIdentity={}; expect(std::move(x), mig::MigrationCorpusErrorV1::InvalidBoundIdentity, "invalid bound identity rejection"); }
        { auto x=corpus; x.cases[0].bindingKind=mig::BindingKindV1::RetainedObservationalEvidence; expect(std::move(x), mig::MigrationCorpusErrorV1::BindingKindMismatch, "binding kind rejection"); }
        { auto x=corpus; for(auto& item:x.cases) item.sourceRoles.erase(std::remove(item.sourceRoles.begin(),item.sourceRoles.end(),"Spiral2ProofLedger"),item.sourceRoles.end()); expect(std::move(x), mig::MigrationCorpusErrorV1::SourceCoverageIncomplete, "source coverage rejection"); }
        { auto x=corpus; for(auto& item:x.cases) item.invariantOrdinals.erase(std::remove(item.invariantOrdinals.begin(),item.invariantOrdinals.end(),37u),item.invariantOrdinals.end()); expect(std::move(x), mig::MigrationCorpusErrorV1::InvariantCoverageIncomplete, "invariant coverage rejection"); }
        { auto x=corpus; x.cases[0].evidenceIds={"OBS-BUNDLE"}; expect(std::move(x), mig::MigrationCorpusErrorV1::EvidenceLaneMismatch, "lane rejection"); }
        { auto x=corpus; x.runtimeCandidatePolicyAuthorized=true; expect(std::move(x), mig::MigrationCorpusErrorV1::RuntimePolicyLeak, "policy rejection"); }
        { auto x=corpus; x.referenceProjectsDeleted=true; expect(std::move(x), mig::MigrationCorpusErrorV1::ReferenceDeletionBeforeDecision, "retirement rejection"); }

        std::vector<std::byte> evidence;
        evidence.insert(evidence.end(), {std::byte{'L'},std::byte{'4'},std::byte{'V'},std::byte{'2'},std::byte{'R'},std::byte{'6'},std::byte{'V'},std::byte{'1'}});
        AppendU32(evidence, certificate.certificate->SourceCount());
        AppendU32(evidence, certificate.certificate->InvariantCount());
        AppendU32(evidence, certificate.certificate->CaseCount());
        AppendU32(evidence, certificate.certificate->CorrectnessCaseCount());
        AppendU32(evidence, certificate.certificate->ObservationalCaseCount());
        AppendU32(evidence, static_cast<std::uint32_t>(rejected.size()));
        AppendDigest(evidence, certificate.certificate->Identity().Digest());
        AppendDigest(evidence, leaf.Descriptor().identity.Digest());
        AppendDigest(evidence, composition.Identity().Digest());
        AppendDigest(evidence, initial.Identity().Digest());
        AppendDigest(evidence, initialRuntime.Digest());
        AppendDigest(evidence, controlled.report->identity.Digest());
        AppendDigest(evidence, recoverySeed.Identity().Digest());
        AppendDigest(evidence, recoverySubmission.submission->identity.Digest());
        AppendDigest(evidence, removal.report->identity.Digest());
        AppendU64(evidence, loaded.Epoch().Value());
        for (const auto code : rejected) AppendU8(evidence, code);
        Emit(emitPath, evidence);

        std::cout << "Level 4 v2 R6 migration corpus self-test passed.\n";
        std::cout << "Source lineages: 16\n";
        std::cout << "Terminal invariant outcomes: 40\n";
        std::cout << "Migration cases: 26 (correctness=24 observational=2)\n";
        std::cout << "Migration-certificate rejections: " << rejected.size() << "\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "R6 failure: " << error.what() << '\n';
        return 1;
    }
}
