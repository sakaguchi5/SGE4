#include "../00_Foundation/BinaryIO.h"
#include "../00_Foundation/Sha256.h"
#include "../172_SparseTemporalDeltaSemantic/SparseTemporalDeltaSemantic.h"
#include "../178_SparseTemporalDeltaCandidateFamily/DeltaCandidateFamily.h"
#include "../179_SparseTemporalDeltaCandidateFamilyPlanner/DeltaCandidateFamilyPlanner.h"
#include "../180_SparseTemporalDeltaCandidateFamilyVerifier/DeltaCandidateFamilyVerifier.h"
#include "../181_Spiral7DeltaFamilyExecution/Spiral7DeltaFamilyExecution.h"
#include "../185_Spiral7DeltaFamilyRuntime/Spiral7DeltaFamilyRuntime.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
namespace base = sge4_5::base;
namespace s7 = sge4_5::spiral7;
namespace s6 = sge4_5::spiral6;
namespace fc = s7::family_candidate;
namespace fp = s7::family_planner;
namespace fv = s7::family_verification;
namespace fe = s7::family_execution;
namespace rt = s7::architecture_runtime;

void Require(bool condition, std::string_view message)
{
    if (!condition) throw std::runtime_error(std::string(message));
}

void WriteFile(const std::filesystem::path& path, std::span<const std::byte> bytes)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) throw std::runtime_error("Unable to open CU5 evidence output.");
    stream.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    if (!stream) throw std::runtime_error("Unable to write CU5 evidence output.");
}

s7::semantic::ExactSparseWorkSetV1 BuildSet(
    s7::semantic::SparsePatternKindV1 pattern,
    std::uint32_t count)
{
    auto result = s6::semantic::BuildCanonicalSparseWorkSetV1(
        pattern, s6::semantic::SparseCardinalityV1(count));
    if (!result) throw std::runtime_error(result.Error().stage + ": " + result.Error().message);
    return std::move(result).Value();
}

s7::semantic::ExactSparseWorkSetV1 BuildExact(const std::vector<std::uint32_t>& indices)
{
    auto result = s6::semantic::BuildExactSparseWorkSetV1(indices);
    if (!result) throw std::runtime_error(result.Error().stage + ": " + result.Error().message);
    return std::move(result).Value();
}

s7::semantic::ExactSparseWorkSetV1 BuildModified(
    const s7::semantic::ExactSparseWorkSetV1& previous,
    const s7::semantic::ExactSparseWorkSetV1& current,
    std::uint32_t requested)
{
    std::vector<std::uint32_t> survivors;
    std::set_intersection(previous.Indices().begin(), previous.Indices().end(),
                          current.Indices().begin(), current.Indices().end(),
                          std::back_inserter(survivors));
    if (survivors.size() > requested) survivors.resize(requested);
    return BuildExact(survivors);
}

fe::VerifiedDeltaFamilyCaseV1 BuildCase(
    std::string id,
    const s7::semantic::SparseTemporalDeltaSemanticV1& semanticValue,
    const fv::DeltaFamilyVerificationContextV1& context,
    s7::semantic::SparseDeltaInvocationV1 invocation)
{
    auto plannedResult = fp::PlanCanonicalDeltaCandidateFamilyV1(semanticValue, invocation);
    Require(static_cast<bool>(plannedResult), "Delta Candidate Family Planner failed.");
    auto planned = std::move(plannedResult).Value();
    Require(planned.size() == 3, "Planner did not return exactly A/B/C.");

    std::vector<fc::FrozenDeltaFamilyArtifactV1> artifacts;
    std::vector<fv::VerifiedDeltaFamilyCandidateV1> verified;
    for (const auto& proposal : planned)
    {
        auto verifiedResult = fv::VerifyDeltaFamilyCandidateV1(
            semanticValue, invocation, context, proposal);
        Require(static_cast<bool>(verifiedResult), "Independent Delta Family verification failed.");
        verified.push_back(std::move(verifiedResult).Value());
        artifacts.push_back(fc::BuildDeltaFamilyArtifactV1(
            semanticValue, invocation, proposal.kind));
    }
    return fe::VerifiedDeltaFamilyCaseV1(
        std::move(id), std::move(invocation), std::move(artifacts), std::move(verified));
}

struct QualificationCorpus final
{
    std::vector<fe::VerifiedDeltaFamilyCaseV1> cases;
    std::set<std::uint32_t> activeCounts;
    std::set<std::uint32_t> transitionCounts;
    bool hold = false;
    bool dirtyOnly = false;
    bool activateOnly = false;
    bool deactivateOnly = false;
    bool balancedChurn = false;
    bool patternMigration = false;
    bool fullInvalidation = false;
    bool emptyReset = false;
};

QualificationCorpus BuildQualificationCorpus(
    const s7::semantic::SparseTemporalDeltaSemanticV1& semanticValue,
    const fv::DeltaFamilyVerificationContextV1& context)
{
    QualificationCorpus corpus;
    auto previousActive = BuildExact({});
    auto generations = s7::semantic::BuildInitialInvalidGenerationsV1();

    auto add = [&](std::string id,
                   s7::semantic::ExactSparseWorkSetV1 active,
                   std::uint32_t modifiedCount)
    {
        const auto modified = BuildModified(previousActive, active, modifiedCount);
        auto invocationResult = s7::semantic::BuildSparseDeltaInvocationV1(
            static_cast<std::uint32_t>(corpus.cases.size()),
            previousActive, active, modified, generations, false);
        if (!invocationResult)
            throw std::runtime_error(invocationResult.Error().stage + ": " +
                                     invocationResult.Error().message);
        auto invocation = std::move(invocationResult).Value();

        const auto activeCount = invocation.ActiveSet().Cardinality().Value();
        const auto transitionCount = invocation.TransitionSet().Cardinality().Value();
        const auto activationCount = invocation.ActivationSet().Cardinality().Value();
        const auto deactivationCount = invocation.DeactivationSet().Cardinality().Value();
        const auto modifiedActual = invocation.ModifiedSurvivorSet().Cardinality().Value();
        corpus.activeCounts.insert(activeCount);
        corpus.transitionCounts.insert(transitionCount);
        if (activationCount == 0 && deactivationCount == 0 && modifiedActual == 0) corpus.hold = true;
        if (activationCount == 0 && deactivationCount == 0 && modifiedActual > 0 &&
            modifiedActual < activeCount) corpus.dirtyOnly = true;
        if (activationCount > 0 && deactivationCount == 0 && modifiedActual == 0) corpus.activateOnly = true;
        if (activationCount == 0 && deactivationCount > 0 && modifiedActual == 0) corpus.deactivateOnly = true;
        if (activationCount > 0 && deactivationCount > 0 &&
            invocation.PreviousActiveSet().Cardinality().Value() == activeCount)
        {
            if (id.find("BalancedChurn") != std::string::npos) corpus.balancedChurn = true;
            if (id.find("PatternMigration") != std::string::npos) corpus.patternMigration = true;
        }
        if (activeCount > 0 && modifiedActual == activeCount &&
            activationCount == 0 && deactivationCount == 0) corpus.fullInvalidation = true;
        if (activeCount == 0 && invocation.PreviousActiveSet().Cardinality().Value() > 0)
            corpus.emptyReset = true;

        generations = invocation.ItemGenerations();
        previousActive = active;
        corpus.cases.push_back(BuildCase(std::move(id), semanticValue, context, std::move(invocation)));
    };

    // The first two invocations are also the controlled-Recovery precondition:
    // a canonical empty history followed by a full generation-zero Active state.
    add("Q000_EmptyInitial", BuildExact({}), 0);
    add("Q001_FullActivate4096", BuildSet(s7::semantic::SparsePatternKindV1::PrefixControl, 4096), 0);
    add("Q002_HoldFull4096", BuildSet(s7::semantic::SparsePatternKindV1::PrefixControl, 4096), 0);
    add("Q003_FullInvalidation4096", BuildSet(s7::semantic::SparsePatternKindV1::PrefixControl, 4096), 4096);
    add("Q004_EmptyReset", BuildExact({}), 0);

    std::uint32_t cycle = 0;
    while (corpus.cases.size() < s7::semantic::TimelineLengthV1)
    {
        const auto slot = cycle % 25u;
        const auto prefix = "Q" + std::to_string(corpus.cases.size()) + "_";
        switch (slot)
        {
        case 0: add(prefix + "ActivatePrefix1", BuildSet(s7::semantic::SparsePatternKindV1::PrefixControl, 1), 0); break;
        case 1: add(prefix + "HoldPrefix1", BuildSet(s7::semantic::SparsePatternKindV1::PrefixControl, 1), 0); break;
        case 2: add(prefix + "Dirty1Prefix1", BuildSet(s7::semantic::SparsePatternKindV1::PrefixControl, 1), 1); break;
        case 3: add(prefix + "ActivatePrefix32", BuildSet(s7::semantic::SparsePatternKindV1::PrefixControl, 32), 0); break;
        case 4: add(prefix + "Dirty8Prefix32", BuildSet(s7::semantic::SparsePatternKindV1::PrefixControl, 32), 8); break;
        case 5: add(prefix + "ActivatePrefix63", BuildSet(s7::semantic::SparsePatternKindV1::PrefixControl, 63), 0); break;
        case 6: add(prefix + "Dirty31Prefix63", BuildSet(s7::semantic::SparsePatternKindV1::PrefixControl, 63), 31); break;
        case 7: add(prefix + "ActivatePrefix64", BuildSet(s7::semantic::SparsePatternKindV1::PrefixControl, 64), 0); break;
        case 8: add(prefix + "Dirty32Prefix64", BuildSet(s7::semantic::SparsePatternKindV1::PrefixControl, 64), 32); break;
        case 9: add(prefix + "ActivatePrefix65", BuildSet(s7::semantic::SparsePatternKindV1::PrefixControl, 65), 0); break;
        case 10: add(prefix + "Dirty63Prefix65", BuildSet(s7::semantic::SparsePatternKindV1::PrefixControl, 65), 63); break;
        case 11: add(prefix + "ActivatePrefix256", BuildSet(s7::semantic::SparsePatternKindV1::PrefixControl, 256), 0); break;
        case 12: add(prefix + "Dirty64Prefix256", BuildSet(s7::semantic::SparsePatternKindV1::PrefixControl, 256), 64); break;
        case 13: add(prefix + "BalancedChurnUniform256", BuildSet(s7::semantic::SparsePatternKindV1::UniformStride, 256), 0); break;
        case 14: add(prefix + "Dirty65Uniform256", BuildSet(s7::semantic::SparsePatternKindV1::UniformStride, 256), 65); break;
        case 15: add(prefix + "PatternMigrationBlock256", BuildSet(s7::semantic::SparsePatternKindV1::BlockClusteredPermutation, 256), 0); break;
        case 16: add(prefix + "FullInvalidationBlock256", BuildSet(s7::semantic::SparsePatternKindV1::BlockClusteredPermutation, 256), 256); break;
        case 17: add(prefix + "PatternMigrationHash1024", BuildSet(s7::semantic::SparsePatternKindV1::HashScatterPermutation, 1024), 0); break;
        case 18: add(prefix + "FullInvalidationHash1024", BuildSet(s7::semantic::SparsePatternKindV1::HashScatterPermutation, 1024), 1024); break;
        case 19: add(prefix + "PatternMigrationPrefix2048", BuildSet(s7::semantic::SparsePatternKindV1::PrefixControl, 2048), 0); break;
        case 20: add(prefix + "FullInvalidationPrefix2048", BuildSet(s7::semantic::SparsePatternKindV1::PrefixControl, 2048), 2048); break;
        case 21: add(prefix + "PatternMigrationHash4095", BuildSet(s7::semantic::SparsePatternKindV1::HashScatterPermutation, 4095), 0); break;
        case 22: add(prefix + "ActivatePrefix4096", BuildSet(s7::semantic::SparsePatternKindV1::PrefixControl, 4096), 0); break;
        case 23: add(prefix + "FullInvalidationPrefix4096", BuildSet(s7::semantic::SparsePatternKindV1::PrefixControl, 4096), 4096); break;
        default: add(prefix + "EmptyReset", BuildExact({}), 0); break;
        }
        ++cycle;
    }

    Require(corpus.cases.size() == s7::semantic::TimelineLengthV1,
            "CU5 qualification timeline is not exactly 128 invocations.");
    const std::array<std::uint32_t, 11> requiredActive{
        0,1,32,63,64,65,256,1024,2048,4095,4096};
    const std::array<std::uint32_t, 11> requiredTransition{
        0,1,8,31,32,63,64,65,256,1024,4096};
    for (const auto value : requiredActive)
        Require(corpus.activeCounts.contains(value), "A required Active count is absent from CU5.");
    for (const auto value : requiredTransition)
        Require(corpus.transitionCounts.contains(value), "A required Transition count is absent from CU5.");
    Require(corpus.hold && corpus.dirtyOnly && corpus.activateOnly && corpus.deactivateOnly &&
            corpus.balancedChurn && corpus.patternMigration && corpus.fullInvalidation &&
            corpus.emptyReset,
            "CU5 does not cover every frozen transition kind.");
    return corpus;
}

void RequireQualifiedReport(
    const fe::DeltaFamilyArchitectureReportV1& report,
    std::size_t expectedInvocations)
{
    Require(report.invocations.size() == expectedInvocations,
            "Delta Family report invocation count differs.");
    const std::array<fc::DeltaFamilyCandidateKindV1, 3> order{
        fc::DeltaFamilyCandidateKindV1::FullActiveDenseRecompute,
        fc::DeltaFamilyCandidateKindV1::CompactDeltaIndexHistoryReuse,
        fc::DeltaFamilyCandidateKindV1::AffectedBlockDeltaHistoryReuse};
    for (const auto& invocation : report.invocations)
    {
        Require(invocation.exactCandidateOrder && invocation.pairwiseOutputByteIdentical,
                "A/B/C order or full-output identity failed.");
        Require(invocation.candidates.size() == order.size(),
                "A/B/C observation count differs.");
        for (std::size_t index = 0; index < order.size(); ++index)
        {
            const auto& candidate = invocation.candidates[index];
            Require(candidate.kind == order[index], "A/B/C Candidate order differs.");
            Require(candidate.activeReferenceQualified &&
                    candidate.inactiveSentinelByteStable &&
                    candidate.retainedHistoryByteStable &&
                    candidate.exactWriteSetQualified &&
                    candidate.homogeneousWQualified &&
                    candidate.finiteQualified &&
                    candidate.observedWriteCount == candidate.legalWriteCount &&
                    candidate.maxAbsoluteError <= 1.0e-6,
                    "CU5 Candidate observation is not qualified.");
        }
    }
}

rt::LoadedDeltaFamilyRuntimeV1 LoadRuntime(
    const s7::semantic::SparseTemporalDeltaSemanticV1& semanticValue,
    const fv::DeltaFamilyVerificationContextV1& context,
    const QualificationCorpus& corpus)
{
    auto loaded = rt::LoadDeltaFamilyRuntimeOnWarpV1(
        semanticValue, context, corpus.cases);
    if (!loaded) throw std::runtime_error(loaded.Error().stage + ": " + loaded.Error().message);
    return std::move(loaded).Value();
}

rt::DeltaRuntimeSubmissionV1 BindBuildSubmitPrefix(
    rt::LoadedDeltaFamilyRuntimeV1& runtime,
    const rt::DeltaRuntimeEpochHandleV1& handle,
    std::uint32_t count)
{
    auto binding = rt::BindDeltaTimelinePrefixV1(runtime, handle, count);
    if (!binding) throw std::runtime_error(binding.Error().stage + ": " + binding.Error().message);
    auto representations = rt::RebuildDeltaTimelineRepresentationsV1(
        runtime, handle, binding.Value());
    if (!representations)
        throw std::runtime_error(representations.Error().stage + ": " + representations.Error().message);
    auto submitted = rt::SubmitDeltaTimelinePrefixV1(
        runtime, handle, binding.Value(), representations.Value());
    if (!submitted)
        throw std::runtime_error(submitted.Error().stage + ": " + submitted.Error().message);
    RequireQualifiedReport(submitted.Value().architecture, count);
    return std::move(submitted).Value();
}

std::vector<std::byte> RunQualification()
{
    const auto semanticValue = s7::semantic::BuildCanonicalSparseTemporalDeltaSemanticV1();
    const auto context = fv::BuildCanonicalDeltaFamilyVerificationContextV1();
    const auto corpus = BuildQualificationCorpus(semanticValue, context);
    auto runtime = LoadRuntime(semanticValue, context, corpus);
    Require(runtime.AuthorityInvocationCount() == 128, "Runtime authority count differs.");
    const auto handle = rt::CaptureDeltaRuntimeEpochHandleV1(runtime);
    auto submission = BindBuildSubmitPrefix(runtime, handle, 128);
    Require(static_cast<bool>(rt::ValidateDeltaRuntimeSubmissionTokenV1(
                runtime, submission.completion)), "Current completion token was rejected.");
    Require(static_cast<bool>(rt::ValidateDeltaHistoryHandleV1(
                runtime, submission.history)), "Current History handle was rejected.");
    Require(static_cast<bool>(rt::ValidateDeltaRepresentationSetHandleV1(
                runtime, submission.representations)), "Current representation handle was rejected.");

    auto forgedToken = submission.completion;
    forgedToken.tokenIdentity[0] ^= std::byte{1};
    Require(!rt::ValidateDeltaRuntimeSubmissionTokenV1(runtime, forgedToken),
            "Forged completion token was accepted.");
    auto forgedHistory = submission.history;
    forgedHistory.historyIdentity[0] ^= std::byte{1};
    Require(!rt::ValidateDeltaHistoryHandleV1(runtime, forgedHistory),
            "Forged History handle was accepted.");
    auto forgedRepresentations = submission.representations;
    forgedRepresentations.representationSetIdentity[0] ^= std::byte{1};
    Require(!rt::ValidateDeltaRepresentationSetHandleV1(runtime, forgedRepresentations),
            "Forged representation handle was accepted.");

    base::BinaryWriter evidence;
    constexpr std::string_view domain =
        "SGE4-5.Spiral7.CU5.ArchitectureQualificationEvidence.V1";
    evidence.WriteBytes(base::Sha256(std::as_bytes(std::span(domain.data(), domain.size()))));
    const auto authority = rt::SerializeDeltaFamilyRuntimeAuthorityV1(runtime);
    evidence.WriteU32(static_cast<std::uint32_t>(authority.size()));
    evidence.WriteBytes(authority);
    evidence.WriteBytes(runtime.DomainIdentity());
    evidence.WriteBytes(runtime.AuthorityIdentity());
    evidence.WriteU64(runtime.DeviceEpoch());
    const auto serializedSubmission = rt::SerializeDeltaRuntimeSubmissionV1(submission);
    evidence.WriteU32(static_cast<std::uint32_t>(serializedSubmission.size()));
    evidence.WriteBytes(serializedSubmission);
    evidence.WriteU32(static_cast<std::uint32_t>(corpus.activeCounts.size()));
    for (const auto value : corpus.activeCounts) evidence.WriteU32(value);
    evidence.WriteU32(static_cast<std::uint32_t>(corpus.transitionCounts.size()));
    for (const auto value : corpus.transitionCounts) evidence.WriteU32(value);
    evidence.WriteU32(128u * 3u);
    evidence.WriteBytes(base::Sha256(evidence.Bytes()));

    std::cout
        << "Complete Spiral 7 WARP architecture qualification passed.\n"
        << "Adapter: " << runtime.AdapterDescription() << '\n'
        << "Timeline: 128 exact Sparse-Temporal invocations\n"
        << "Candidate executions: 128 x 3 = 384\n"
        << "Required Active counts and Transition counts: covered\n"
        << "A/B/C outputs after every invocation: byte-identical\n"
        << "Architecture evidence SHA-256: "
        << base::ToHex(base::Sha256(evidence.Bytes())) << '\n';
    return std::move(evidence).Take();
}

std::vector<std::byte> RunControlledRecovery()
{
    const auto semanticValue = s7::semantic::BuildCanonicalSparseTemporalDeltaSemanticV1();
    const auto context = fv::BuildCanonicalDeltaFamilyVerificationContextV1();
    const auto corpus = BuildQualificationCorpus(semanticValue, context);
    auto runtime = LoadRuntime(semanticValue, context, corpus);
    const auto authorityBefore = rt::SerializeDeltaFamilyRuntimeAuthorityV1(runtime);
    const auto oldHandle = rt::CaptureDeltaRuntimeEpochHandleV1(runtime);
    auto before = BindBuildSubmitPrefix(runtime, oldHandle, 3);
    const auto oldEpoch = runtime.DeviceEpoch();

    auto recovered = rt::RecoverDeltaFamilyRuntimeV1(
        runtime, rt::DeltaFamilyRecoveryModeV1::ControlledRebuild);
    if (!recovered)
        throw std::runtime_error(recovered.Error().stage + ": " + recovered.Error().message);
    const auto& report = recovered.Value();
    Require(report.stateBefore == rt::DeltaFamilyRuntimeStateV1::Active &&
            report.stateAfter == rt::DeltaFamilyRuntimeStateV1::Active &&
            report.previousDeviceEpoch == oldEpoch &&
            report.newDeviceEpoch == oldEpoch + 1 &&
            runtime.DeviceEpoch() == oldEpoch + 1 &&
            report.adapterReacquired && report.nativeDeviceRematerialized &&
            report.allRuntimeObjectsReleased && report.executionContextRematerialized &&
            report.representationResourcesInvalidated &&
            report.indirectArgumentsInvalidated && report.historyStateInvalidated &&
            report.completionStateInvalidated && report.readbackStateInvalidated &&
            report.externalSetRebindRequired &&
            report.explicitRepresentationRebuildRequired &&
            report.forcedFullActiveSeedRequired && report.frozenAuthorityPreserved,
            "Controlled Recovery report is incomplete.");
    Require(runtime.RecoverySeedRequired(), "Recovery seed was not required.");

    auto staleEpoch = rt::ValidateDeltaRuntimeEpochHandleV1(runtime, oldHandle);
    auto staleToken = rt::ValidateDeltaRuntimeSubmissionTokenV1(runtime, before.completion);
    auto staleHistory = rt::ValidateDeltaHistoryHandleV1(runtime, before.history);
    auto staleRepresentations = rt::ValidateDeltaRepresentationSetHandleV1(
        runtime, before.representations);
    Require(!staleEpoch && staleEpoch.Error().stage == "delta-runtime/stale-epoch",
            "Old epoch handle survived Recovery.");
    Require(!staleToken && staleToken.Error().stage == "delta-runtime/stale-epoch",
            "Old completion token survived Recovery.");
    Require(!staleHistory && staleHistory.Error().stage == "delta-runtime/stale-epoch",
            "Old History handle survived Recovery.");
    Require(!staleRepresentations && staleRepresentations.Error().stage == "delta-runtime/stale-epoch",
            "Old representation handle survived Recovery.");

    const auto newHandle = rt::CaptureDeltaRuntimeEpochHandleV1(runtime);
    auto normalBeforeSeed = rt::BindDeltaTimelinePrefixV1(runtime, newHandle, 3);
    Require(!normalBeforeSeed &&
            normalBeforeSeed.Error().stage == "delta-runtime/recovery-seed-required",
            "Normal timeline was accepted before forced Recovery seed.");

    const auto& active = corpus.cases[2].invocation.ActiveSet();
    const auto recoveryModified = BuildModified(active, active, 64);
    auto semanticRecoveryResult = s7::semantic::BuildSparseDeltaInvocationV1(
        3, active, active, recoveryModified,
        corpus.cases[2].invocation.ItemGenerations(), true);
    if (!semanticRecoveryResult)
        throw std::runtime_error(semanticRecoveryResult.Error().message);
    auto semanticRecovery = std::move(semanticRecoveryResult).Value();
    Require(semanticRecovery.UpdateSet().Identity() == active.Identity() &&
            semanticRecovery.RetainSet().Cardinality().Value() == 0 &&
            semanticRecovery.ModifiedSurvivorSet().Cardinality().Value() == 64,
            "Semantic Recovery did not apply exact M_t while forcing W=A and H=empty.");

    auto initializationSeedResult = s7::semantic::BuildSparseDeltaInvocationV1(
        0, BuildExact({}), active, BuildExact({}),
        s7::semantic::BuildInitialInvalidGenerationsV1(), true);
    if (!initializationSeedResult)
        throw std::runtime_error(initializationSeedResult.Error().message);
    auto initializationSeedCase = BuildCase(
        "ControlledRecoveryInitializationSeed", semanticValue, context,
        std::move(initializationSeedResult).Value());

    // The internal second materialization invocation reproduces the exact current
    // per-item generations. Modified survivors consume their pre-update generation;
    // all other active members preserve theirs. Because ForceFullActiveRebuild is set,
    // the final GPU write set remains W=A and no stale history is retained.
    auto executionPreviousGenerations = semanticRecovery.ItemGenerations();
    for (const std::uint32_t index : semanticRecovery.ModifiedSurvivorSet().Indices())
    {
        Require(executionPreviousGenerations[index] > 0,
                "Recovery modified survivor has no predecessor generation.");
        --executionPreviousGenerations[index];
    }
    auto executionRecoveryResult = s7::semantic::BuildSparseDeltaInvocationV1(
        1, active, active, semanticRecovery.ModifiedSurvivorSet(),
        executionPreviousGenerations, true);
    if (!executionRecoveryResult)
        throw std::runtime_error(executionRecoveryResult.Error().message);
    auto executionRecoveryCase = BuildCase(
        "ControlledRecoveryExactGenerationRebuild", semanticValue, context,
        std::move(executionRecoveryResult).Value());
    Require(executionRecoveryCase.invocation.ItemGenerations() ==
                semanticRecovery.ItemGenerations(),
            "GPU Recovery materialization generations differ from rebound Semantic generations.");

    rt::DeltaRecoverySeedBindingV1 missingBinding;
    rt::DeltaRecoveryRepresentationSetHandleV1 missingRepresentations;
    auto noRebind = rt::SubmitDeltaRecoverySeedV1(
        runtime, newHandle, missingBinding, missingRepresentations,
        initializationSeedCase, executionRecoveryCase);
    Require(!noRebind && noRebind.Error().stage == "delta-runtime/rebind-required",
            "Recovery seed submitted without explicit set rebind.");

    auto binding = rt::BindDeltaRecoverySeedV1(
        runtime, newHandle, semanticRecovery,
        initializationSeedCase, executionRecoveryCase);
    if (!binding) throw std::runtime_error(binding.Error().message);
    auto noRebuild = rt::SubmitDeltaRecoverySeedV1(
        runtime, newHandle, binding.Value(), missingRepresentations,
        initializationSeedCase, executionRecoveryCase);
    Require(!noRebuild && noRebuild.Error().stage == "delta-runtime/rebuild-required",
            "Recovery seed submitted without representation rebuild.");
    auto representations = rt::RebuildDeltaRecoverySeedRepresentationsV1(
        runtime, newHandle, binding.Value(),
        initializationSeedCase, executionRecoveryCase);
    if (!representations) throw std::runtime_error(representations.Error().message);
    auto after = rt::SubmitDeltaRecoverySeedV1(
        runtime, newHandle, binding.Value(), representations.Value(),
        initializationSeedCase, executionRecoveryCase);
    if (!after) throw std::runtime_error(after.Error().message);
    RequireQualifiedReport(after.Value().architecture, 2);
    Require(after.Value().history.outputDigest != before.history.outputDigest,
            "Recovery M_t update did not change the materialized History output.");
    Require(!runtime.RecoverySeedRequired(), "Recovery seed requirement was not cleared.");
    Require(static_cast<bool>(rt::ValidateDeltaHistoryHandleV1(
                runtime, after.Value().history)), "New History handle was rejected.");
    Require(static_cast<bool>(rt::ValidateDeltaRuntimeSubmissionTokenV1(
                runtime, after.Value().completion)), "New completion token was rejected.");
    Require(rt::SerializeDeltaFamilyRuntimeAuthorityV1(runtime) == authorityBefore,
            "Frozen timeline authority changed across Recovery.");

    base::BinaryWriter evidence;
    constexpr std::string_view domain =
        "SGE4-5.Spiral7.CU5.ControlledRecoveryEvidence.V1";
    evidence.WriteBytes(base::Sha256(std::as_bytes(std::span(domain.data(), domain.size()))));
    evidence.WriteBytes(runtime.AuthorityIdentity());
    const auto recoveryBytes = rt::SerializeDeltaFamilyRecoveryReportV1(report);
    evidence.WriteU32(static_cast<std::uint32_t>(recoveryBytes.size()));
    evidence.WriteBytes(recoveryBytes);
    const auto beforeBytes = rt::SerializeDeltaRuntimeSubmissionV1(before);
    const auto afterBytes = rt::SerializeDeltaRuntimeSubmissionV1(after.Value());
    evidence.WriteU32(static_cast<std::uint32_t>(beforeBytes.size()));
    evidence.WriteBytes(beforeBytes);
    evidence.WriteU32(static_cast<std::uint32_t>(afterBytes.size()));
    evidence.WriteBytes(afterBytes);
    evidence.WriteBytes(semanticRecovery.Identity());
    evidence.WriteBytes(initializationSeedCase.invocation.Identity());
    evidence.WriteBytes(executionRecoveryCase.invocation.Identity());
    evidence.WriteBytes(base::Sha256(evidence.Bytes()));

    std::cout
        << "Controlled Spiral 7 Recovery passed.\n"
        << "Device epoch: " << oldEpoch << " -> " << runtime.DeviceEpoch() << '\n'
        << "Stale epoch/representation/history/completion handles: rejected\n"
        << "Explicit A_t/M_t rebind and Candidate representation rebuild: required\n"
        << "First post-Recovery semantic invocation: exact M_t rebound, W_t=A_t, H_t=empty\n"
        << "Controlled evidence SHA-256: "
        << base::ToHex(base::Sha256(evidence.Bytes())) << '\n';
    return std::move(evidence).Take();
}

void RunActualRemoval()
{
    const auto semanticValue = s7::semantic::BuildCanonicalSparseTemporalDeltaSemanticV1();
    const auto context = fv::BuildCanonicalDeltaFamilyVerificationContextV1();
    const auto corpus = BuildQualificationCorpus(semanticValue, context);
    auto runtime = LoadRuntime(semanticValue, context, corpus);
    const auto oldHandle = rt::CaptureDeltaRuntimeEpochHandleV1(runtime);
    const auto oldEpoch = runtime.DeviceEpoch();

    auto removed = rt::RecoverDeltaFamilyRuntimeV1(
        runtime, rt::DeltaFamilyRecoveryModeV1::ForceRemovalForTest);
    if (!removed)
        throw std::runtime_error(removed.Error().stage + ": " + removed.Error().message);
    Require(removed.Value().forcedRemoval && removed.Value().removalReason < 0 &&
            removed.Value().stateBefore == rt::DeltaFamilyRuntimeStateV1::Active &&
            removed.Value().stateAfter == rt::DeltaFamilyRuntimeStateV1::AwaitingAdapter &&
            runtime.State() == rt::DeltaFamilyRuntimeStateV1::AwaitingAdapter &&
            runtime.DeviceEpoch() == oldEpoch &&
            removed.Value().historyStateInvalidated &&
            removed.Value().externalSetRebindRequired &&
            removed.Value().forcedFullActiveSeedRequired &&
            removed.Value().frozenAuthorityPreserved,
            "Actual RemoveDevice quarantine report differs.");
    auto rejected = rt::ValidateDeltaRuntimeEpochHandleV1(runtime, oldHandle);
    Require(!rejected && rejected.Error().stage == "delta-runtime/device-state",
            "Old epoch handle survived actual removal.");

    auto retry = rt::RecoverDeltaFamilyRuntimeV1(
        runtime, rt::DeltaFamilyRecoveryModeV1::RetryAdapterReacquisition);
    if (!retry) throw std::runtime_error(retry.Error().message);
    Require(!retry.Value().adapterReacquired &&
            retry.Value().stateAfter == rt::DeltaFamilyRuntimeStateV1::AwaitingAdapter &&
            runtime.State() == rt::DeltaFamilyRuntimeStateV1::AwaitingAdapter &&
            runtime.DeviceEpoch() == oldEpoch,
            "Removed WARP adapter was incorrectly reacquired.");

    std::cout
        << "Actual Spiral 7 ID3D12Device5::RemoveDevice quarantine passed.\n"
        << "State: Active -> AwaitingAdapter\n"
        << "Removed WARP LUID excluded from retry.\n"
        << "Runtime Candidate-policy decision: None\n";
}

std::vector<std::byte> RunFreshRematerialization()
{
    const auto semanticValue = s7::semantic::BuildCanonicalSparseTemporalDeltaSemanticV1();
    const auto context = fv::BuildCanonicalDeltaFamilyVerificationContextV1();
    const auto corpus = BuildQualificationCorpus(semanticValue, context);
    auto runtime = LoadRuntime(semanticValue, context, corpus);
    const auto handle = rt::CaptureDeltaRuntimeEpochHandleV1(runtime);
    auto submission = BindBuildSubmitPrefix(runtime, handle, 16);

    base::BinaryWriter evidence;
    constexpr std::string_view domain =
        "SGE4-5.Spiral7.CU5.FreshRematerializationEvidence.V1";
    evidence.WriteBytes(base::Sha256(std::as_bytes(std::span(domain.data(), domain.size()))));
    const auto authority = rt::SerializeDeltaFamilyRuntimeAuthorityV1(runtime);
    evidence.WriteU32(static_cast<std::uint32_t>(authority.size()));
    evidence.WriteBytes(authority);
    evidence.WriteBytes(runtime.DomainIdentity());
    evidence.WriteBytes(runtime.AuthorityIdentity());
    const auto submissionBytes = rt::SerializeDeltaRuntimeSubmissionV1(submission);
    evidence.WriteU32(static_cast<std::uint32_t>(submissionBytes.size()));
    evidence.WriteBytes(submissionBytes);
    evidence.WriteBytes(base::Sha256(evidence.Bytes()));

    std::cout
        << "Fresh-process Spiral 7 rematerialization passed.\n"
        << "Frozen authority: 128 invocations / A-B-C Candidate set preserved\n"
        << "Representative fresh execution: 16 invocations / 48 Candidate executions\n"
        << "Fresh evidence SHA-256: "
        << base::ToHex(base::Sha256(evidence.Bytes())) << '\n';
    return std::move(evidence).Take();
}

std::vector<fe::VerifiedDeltaFamilyCaseV1> BuildDebugSmokeCases(
    const s7::semantic::SparseTemporalDeltaSemanticV1& semanticValue,
    const fv::DeltaFamilyVerificationContextV1& context)
{
    std::vector<fe::VerifiedDeltaFamilyCaseV1> cases;
    auto previousActive = BuildExact({});
    auto generations = s7::semantic::BuildInitialInvalidGenerationsV1();

    auto add = [&](std::string id,
                   s7::semantic::ExactSparseWorkSetV1 active,
                   std::uint32_t modifiedCount)
    {
        const auto modified = BuildModified(previousActive, active, modifiedCount);
        auto invocationResult = s7::semantic::BuildSparseDeltaInvocationV1(
            static_cast<std::uint32_t>(cases.size()),
            previousActive, active, modified, generations, false);
        if (!invocationResult)
            throw std::runtime_error(invocationResult.Error().stage + ": " +
                                     invocationResult.Error().message);
        auto invocation = std::move(invocationResult).Value();
        generations = invocation.ItemGenerations();
        previousActive = active;
        cases.push_back(BuildCase(std::move(id), semanticValue, context,
                                  std::move(invocation)));
    };

    add("DebugSmokeInitialPrefix64",
        BuildSet(s7::semantic::SparsePatternKindV1::PrefixControl, 64), 0);
    add("DebugSmokeHoldPrefix64",
        BuildSet(s7::semantic::SparsePatternKindV1::PrefixControl, 64), 0);
    add("DebugSmokeDirty8Prefix64",
        BuildSet(s7::semantic::SparsePatternKindV1::PrefixControl, 64), 8);
    add("DebugSmokeEmptyReset", BuildExact({}), 0);
    Require(cases.size() == 4, "CU5 Debug smoke corpus size differs.");
    return cases;
}

void RunDebugSmoke()
{
    const auto semanticValue = s7::semantic::BuildCanonicalSparseTemporalDeltaSemanticV1();
    const auto context = fv::BuildCanonicalDeltaFamilyVerificationContextV1();
    const auto cases = BuildDebugSmokeCases(semanticValue, context);
    auto report = fe::ExecuteVerifiedDeltaCandidateFamilyV1(semanticValue, cases);
    if (!report)
        throw std::runtime_error(report.Error().stage + ": " + report.Error().message);
    RequireQualifiedReport(report.Value(), cases.size());
    std::cout
        << "Spiral 7 CU5 Debug smoke passed.\n"
        << "Representative timeline: Initial, Hold, DirtyOnly, EmptyReset\n"
        << "Candidate executions: 4 x 3 = 12\n"
        << "A/B/C outputs after every smoke invocation: byte-identical\n";
}

void RunPortableSelfTest()
{
    const auto semanticValue = s7::semantic::BuildCanonicalSparseTemporalDeltaSemanticV1();
    const auto context = fv::BuildCanonicalDeltaFamilyVerificationContextV1();
    const auto corpus = BuildQualificationCorpus(semanticValue, context);
    std::uint32_t candidateCount = 0;
    for (const auto& testCase : corpus.cases)
    {
        Require(testCase.artifacts.size() == 3 && testCase.verifiedCandidates.size() == 3,
                "Portable Candidate family shape differs.");
        for (std::size_t index = 0; index < 3; ++index)
        {
            auto artifact = fc::ValidateDeltaFamilyArtifactV1(
                testCase.artifacts[index], semanticValue, testCase.invocation);
            Require(static_cast<bool>(artifact), "Portable artifact validation failed.");
            auto verified = fv::ValidateVerifiedDeltaFamilyCandidateV1(
                testCase.verifiedCandidates[index], semanticValue,
                testCase.invocation, context);
            Require(static_cast<bool>(verified), "Portable Verified Candidate validation failed.");
            ++candidateCount;
        }
    }
    Require(candidateCount == 384, "Portable Candidate total differs.");
    std::cout
        << "Spiral 7 CU5 portable corpus self-test passed.\n"
        << "128 exact Sparse-Temporal invocations and 384 Verified Candidate artifacts qualified.\n";
}
} // namespace

int main(int argc, char** argv)
{
    try
    {
        enum class Mode { Qualification, Controlled, ActualRemoval, Fresh, Portable, DebugSmoke };
        Mode mode = Mode::Qualification;
        std::filesystem::path output;
        for (int index = 1; index < argc; ++index)
        {
            const std::string_view argument = argv[index];
            if (argument == "--qualification") mode = Mode::Qualification;
            else if (argument == "--controlled") mode = Mode::Controlled;
            else if (argument == "--actual-removal") mode = Mode::ActualRemoval;
            else if (argument == "--fresh-rematerialization") mode = Mode::Fresh;
            else if (argument == "--portable-self-test") mode = Mode::Portable;
            else if (argument == "--debug-smoke") mode = Mode::DebugSmoke;
            else if (argument == "--emit" && index + 1 < argc) output = argv[++index];
            else throw std::runtime_error("Unknown or incomplete argument: " + std::string(argument));
        }

        if (mode == Mode::ActualRemoval)
        {
            RunActualRemoval();
            return 0;
        }
        if (mode == Mode::Portable)
        {
            RunPortableSelfTest();
            return 0;
        }
        if (mode == Mode::DebugSmoke)
        {
            RunDebugSmoke();
            return 0;
        }

        std::vector<std::byte> evidence;
        if (mode == Mode::Qualification) evidence = RunQualification();
        else if (mode == Mode::Controlled) evidence = RunControlledRecovery();
        else evidence = RunFreshRematerialization();
        if (!output.empty()) WriteFile(output, evidence);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Spiral 7 CU5 architecture qualification failed: " << error.what() << '\n';
        return 1;
    }
}
