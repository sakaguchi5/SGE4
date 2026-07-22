#include "../00_Foundation/BinaryIO.h"
#include "../00_Foundation/FileIO.h"
#include "../00_Foundation/Sha256.h"
#include "../134_TemporalStateSemantic/TemporalStateSemantic.h"
#include "../136_Spiral5TemporalExecution/Spiral5TemporalExecution.h"
#include "../147_TemporalCandidateFamily/TemporalCandidateFamily.h"
#include "../148_TemporalCandidateFamilyPlanner/TemporalCandidateFamilyPlanner.h"
#include "../149_TemporalCandidateFamilyVerifier/TemporalCandidateFamilyVerifier.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace base = sge4_5::base;
namespace sem = sge4_5::spiral5::semantic;
namespace candidate = sge4_5::spiral5::family_candidate;
namespace planner = sge4_5::spiral5::family_planner;
namespace verify = sge4_5::spiral5::family_verification;
namespace execute = sge4_5::spiral5::execution;

namespace
{
struct CanonicalAuthority final
{
    sem::TemporalStateSemanticV1 semantic;
    verify::TemporalCandidateFamilyVerificationContextV1 context;
    std::vector<sem::UpdateScheduleV1> schedules;
    std::vector<std::vector<candidate::RawTemporalCandidateV1>> raw;
    std::vector<std::vector<verify::VerifiedTemporalCandidateV1>> verified;
    std::vector<execute::TemporalScheduleCandidateAuthorityV1> runtimeAuthorities;
};

void Require(bool condition, std::string_view message)
{
    if (!condition) throw std::runtime_error(std::string(message));
}

CanonicalAuthority BuildCanonicalAuthority()
{
    CanonicalAuthority result;
    result.semantic = sem::BuildCanonicalTemporalStateSemanticV1();
    result.context =
        verify::BuildCanonicalTemporalCandidateFamilyVerificationContextV1();

    for (const std::uint32_t interval : sem::MeasurementUpdateIntervalsV1())
    {
        auto schedule = sem::BuildPeriodicUpdateScheduleV1(interval);
        if (!schedule)
            throw std::runtime_error(schedule.Error().message);
        result.schedules.push_back(std::move(schedule).Value());
    }
    for (const std::string_view name : {"BurstThenHold", "Irregular"})
    {
        auto schedule = sem::BuildNamedUpdateScheduleV1(name);
        if (!schedule)
            throw std::runtime_error(schedule.Error().message);
        result.schedules.push_back(std::move(schedule).Value());
    }
    Require(result.schedules.size() == 9,
            "complete Temporal schedule corpus was not constructed");

    for (const auto& schedule : result.schedules)
    {
        auto planned = planner::PlanCanonicalTemporalCandidateFamilyV1(
            result.semantic, schedule);
        if (!planned) throw std::runtime_error(planned.Error());
        auto raw = std::move(planned).Value();
        Require(raw.size() == 3, "Temporal Candidate family size differs");

        std::vector<verify::VerifiedTemporalCandidateV1> verified;
        for (const auto& proposal : raw)
        {
            auto value = verify::VerifyTemporalCandidateV1(
                result.semantic, schedule, result.context, proposal);
            if (!value) throw std::runtime_error(value.Error());
            verified.push_back(std::move(value).Value());
        }

        execute::TemporalScheduleCandidateAuthorityV1 authority;
        authority.schedule = schedule;
        authority.candidates = verified;
        result.runtimeAuthorities.push_back(std::move(authority));
        result.raw.push_back(std::move(raw));
        result.verified.push_back(std::move(verified));
    }
    return result;
}

void RequireQualifiedArchitecture(
    const execute::TemporalCandidateFamilyArchitectureResultV1& result)
{
    Require(result.pairwiseOutputEquivalent,
            "Temporal Candidate output equivalence failed");
    Require(result.candidates.size() == 3,
            "Temporal Candidate count differs");

    constexpr std::array<candidate::TemporalCandidateKindV1, 3> kinds{
        candidate::TemporalCandidateKindV1::EveryInvocationRecompute,
        candidate::TemporalCandidateKindV1::GlobalMotorHistoryReuse,
        candidate::TemporalCandidateKindV1::FinalOutputHistoryReuse};

    for (std::size_t candidateIndex = 0;
         candidateIndex < result.candidates.size();
         ++candidateIndex)
    {
        const auto& value = result.candidates[candidateIndex];
        Require(value.kind == kinds[candidateIndex],
                "Temporal Candidate ordering differs");
        Require(value.invocations.size() == sem::TimelineLengthV1,
                "Temporal Timeline length differs");

        for (const auto& invocation : value.invocations)
        {
            Require(invocation.expectedHierarchyDispatchX ==
                        invocation.observedHierarchyDispatchX,
                    "Hierarchy Dispatch argument differs");
            Require(invocation.expectedConsumerDispatchX ==
                        invocation.observedConsumerDispatchX,
                    "Consumer Dispatch argument differs");
            Require(invocation.outputMatchesReference,
                    "Temporal output differs from CPU reference");
            Require(invocation.outputMatchesLatestUpdate,
                    "Temporal output differs from latest Update");
            Require(invocation.mismatchCount == 0 &&
                        invocation.nonFiniteCount == 0 &&
                        invocation.homogeneousCoordinateMismatchCount == 0,
                    "Temporal numeric Observation failed");
            Require(invocation.retainedCompletionOrdinal ==
                        static_cast<std::uint64_t>(invocation.invocationIndex) + 1u,
                    "Temporal completion ordinal differs");

            if (invocation.event == sem::UpdateEventV1::Hold)
            {
                if (value.kind ==
                    candidate::TemporalCandidateKindV1::EveryInvocationRecompute)
                {
                    Require(invocation.observedHierarchyDispatchX == 1 &&
                                invocation.observedConsumerDispatchX == 64,
                            "A Hold did not recompute the complete path");
                }
                else if (value.kind ==
                    candidate::TemporalCandidateKindV1::GlobalMotorHistoryReuse)
                {
                    Require(invocation.observedHierarchyDispatchX == 0 &&
                                invocation.observedConsumerDispatchX == 64 &&
                                invocation.retainedHistoryByteStable,
                            "B Hold modified or recomputed Global Motor history");
                }
                else
                {
                    Require(invocation.observedHierarchyDispatchX == 0 &&
                                invocation.observedConsumerDispatchX == 0 &&
                                invocation.retainedHistoryByteStable,
                            "C Hold modified or recomputed Final Output history");
                }
            }
        }
    }
}

void RequireSameObservation(
    const execute::TemporalCandidateFamilyArchitectureResultV1& before,
    const execute::TemporalCandidateFamilyArchitectureResultV1& after)
{
    Require(before.scheduleIdentity == after.scheduleIdentity,
            "schedule identity changed across Recovery");
    Require(before.semanticIdentity == after.semanticIdentity,
            "semantic identity changed across Recovery");
    Require(before.argumentProducerShaderDigest ==
                after.argumentProducerShaderDigest &&
            before.hierarchyShaderDigest == after.hierarchyShaderDigest &&
            before.consumerShaderDigest == after.consumerShaderDigest,
            "Program identity changed across Recovery");
    Require(before.candidates.size() == after.candidates.size(),
            "Candidate count changed across Recovery");

    for (std::size_t candidateIndex = 0;
         candidateIndex < before.candidates.size();
         ++candidateIndex)
    {
        const auto& a = before.candidates[candidateIndex];
        const auto& b = after.candidates[candidateIndex];
        Require(a.kind == b.kind,
                "Candidate kind changed across Recovery");
        Require(a.invocations.size() == b.invocations.size(),
                "Invocation count changed across Recovery");
        for (std::size_t invocationIndex = 0;
             invocationIndex < a.invocations.size();
             ++invocationIndex)
        {
            const auto& x = a.invocations[invocationIndex];
            const auto& y = b.invocations[invocationIndex];
            Require(x.invocationIndex == y.invocationIndex &&
                    x.event == y.event &&
                    x.sourceGeneration == y.sourceGeneration &&
                    x.expectedHierarchyDispatchX == y.expectedHierarchyDispatchX &&
                    x.observedHierarchyDispatchX == y.observedHierarchyDispatchX &&
                    x.expectedConsumerDispatchX == y.expectedConsumerDispatchX &&
                    x.observedConsumerDispatchX == y.observedConsumerDispatchX &&
                    x.retainedHistoryByteStable == y.retainedHistoryByteStable &&
                    x.outputMatchesReference == y.outputMatchesReference &&
                    x.outputMatchesLatestUpdate == y.outputMatchesLatestUpdate &&
                    x.mismatchCount == y.mismatchCount &&
                    x.nonFiniteCount == y.nonFiniteCount &&
                    x.homogeneousCoordinateMismatchCount ==
                        y.homogeneousCoordinateMismatchCount &&
                    x.maxAbsoluteError == y.maxAbsoluteError &&
                    x.retainedHistoryDigest == y.retainedHistoryDigest &&
                    x.outputDigest == y.outputDigest,
                    "Temporal Observation changed across Recovery");
        }
    }
}

void WriteAuthority(
    base::BinaryWriter& writer,
    const CanonicalAuthority& authority)
{
    const auto semanticBytes =
        sem::SerializeTemporalStateSemanticV1(authority.semantic);
    writer.WriteU32(static_cast<std::uint32_t>(semanticBytes.size()));
    writer.WriteBytes(semanticBytes);
    writer.WriteBytes(authority.context.targetProfileIdentity);
    writer.WriteBytes(authority.context.observationContractIdentity);
    writer.WriteBytes(authority.context.deviceEpochPolicyIdentity);
    writer.WriteU32(static_cast<std::uint32_t>(authority.schedules.size()));

    for (std::size_t scheduleIndex = 0;
         scheduleIndex < authority.schedules.size();
         ++scheduleIndex)
    {
        const auto scheduleBytes =
            sem::SerializeUpdateScheduleV1(authority.schedules[scheduleIndex]);
        writer.WriteU32(static_cast<std::uint32_t>(scheduleBytes.size()));
        writer.WriteBytes(scheduleBytes);
        writer.WriteU32(static_cast<std::uint32_t>(
            authority.raw[scheduleIndex].size()));
        for (std::size_t candidateIndex = 0;
             candidateIndex < authority.raw[scheduleIndex].size();
             ++candidateIndex)
        {
            const auto rawBytes = candidate::SerializeRawTemporalCandidateV1(
                authority.raw[scheduleIndex][candidateIndex]);
            const auto verifiedBytes =
                verify::SerializeVerifiedTemporalCandidateV1(
                    authority.verified[scheduleIndex][candidateIndex]);
            writer.WriteU32(static_cast<std::uint32_t>(rawBytes.size()));
            writer.WriteBytes(rawBytes);
            writer.WriteBytes(
                authority.raw[scheduleIndex][candidateIndex].rawCandidateIdentity);
            writer.WriteU32(static_cast<std::uint32_t>(verifiedBytes.size()));
            writer.WriteBytes(verifiedBytes);
        }
    }
}

void WriteSubmission(
    base::BinaryWriter& writer,
    const execute::TemporalRuntimeSubmissionV1& submission)
{
    writer.WriteU64(submission.completion.deviceEpoch);
    writer.WriteU64(submission.completion.submissionOrdinal);
    writer.WriteBytes(submission.completion.tokenIdentity);
    writer.WriteU64(submission.retainedHistory.deviceEpoch);
    writer.WriteBytes(submission.retainedHistory.scheduleIdentity);
    writer.WriteU32(submission.retainedHistory.sourceGeneration);
    writer.WriteBytes(submission.retainedHistory.historyIdentity);
    writer.WriteU32(submission.historyResourcesMaterialized ? 1u : 0u);
    writer.WriteU32(submission.temporalArgumentsRegenerated ? 1u : 0u);
    writer.WriteU32(submission.completionStateMaterialized ? 1u : 0u);
    writer.WriteU32(submission.readbackStateMaterialized ? 1u : 0u);
    const auto architectureBytes =
        execute::SerializeTemporalCandidateFamilyArchitectureEvidenceV1(
            submission.architecture);
    writer.WriteU32(static_cast<std::uint32_t>(architectureBytes.size()));
    writer.WriteBytes(architectureBytes);
}

void WriteFile(
    const std::filesystem::path& path,
    std::vector<std::byte> bytes)
{
    std::filesystem::create_directories(path.parent_path());
    auto written = base::WriteAllBytes(path, bytes);
    if (!written) throw std::runtime_error(written.Error());
}

execute::LoadedTemporalCandidateFamilyRuntimeV1 LoadRuntime(
    const CanonicalAuthority& authority)
{
    auto loaded = execute::LoadTemporalCandidateFamilyRuntimeOnWarpV1(
        authority.semantic, authority.context, authority.runtimeAuthorities);
    if (!loaded)
        throw std::runtime_error(
            loaded.Error().stage + ": " + loaded.Error().message);
    return std::move(loaded).Value();
}

std::vector<std::byte> RunArchitectureQualification()
{
    const auto authority = BuildCanonicalAuthority();
    auto runtime = LoadRuntime(authority);
    Require(runtime.ScheduleCount() == 9,
            "Temporal Runtime schedule count differs");

    auto binding = execute::BindCanonicalTemporalRuntimeInputsV1(runtime);
    if (!binding) throw std::runtime_error(binding.Error().message);
    const auto handle = execute::CaptureTemporalRuntimeEpochHandleV1(runtime);
    auto seed = execute::ReseedTemporalHistoriesV1(
        runtime, handle, binding.Value());
    if (!seed) throw std::runtime_error(seed.Error().message);

    std::vector<execute::TemporalRuntimeSubmissionV1> submissions;
    std::uint32_t candidateInvocations = 0;
    for (const auto& schedule : authority.schedules)
    {
        auto submitted = execute::SubmitTemporalScheduleV1(
            runtime, handle, binding.Value(), seed.Value(),
            sem::UpdateScheduleIdentityV1(schedule));
        if (!submitted)
            throw std::runtime_error(
                submitted.Error().stage + ": " + submitted.Error().message);
        RequireQualifiedArchitecture(submitted.Value().architecture);
        auto tokenValid = execute::ValidateTemporalRuntimeSubmissionTokenV1(
            runtime, submitted.Value().completion);
        Require(static_cast<bool>(tokenValid),
                "current Temporal submission token was rejected");
        auto historyValid = execute::ValidateTemporalHistoryHandleV1(
            runtime, submitted.Value().retainedHistory);
        Require(static_cast<bool>(historyValid),
                "current Temporal history handle was rejected");
        auto forgedToken = submitted.Value().completion;
        forgedToken.tokenIdentity[0] ^= std::byte{1};
        auto forgedTokenResult = execute::ValidateTemporalRuntimeSubmissionTokenV1(
            runtime, forgedToken);
        Require(!forgedTokenResult &&
                forgedTokenResult.Error().stage == "temporal-runtime/token-identity",
                "forged current-epoch Temporal token was accepted");
        auto forgedHistory = submitted.Value().retainedHistory;
        forgedHistory.historyIdentity[0] ^= std::byte{1};
        auto forgedHistoryResult = execute::ValidateTemporalHistoryHandleV1(
            runtime, forgedHistory);
        Require(!forgedHistoryResult &&
                forgedHistoryResult.Error().stage == "temporal-runtime/history-identity",
                "forged current-epoch Temporal history was accepted");
        candidateInvocations += static_cast<std::uint32_t>(
            submitted.Value().architecture.candidates.size() *
            sem::TimelineLengthV1);
        submissions.push_back(std::move(submitted).Value());
    }
    Require(candidateInvocations == 9u * 3u * 128u,
            "complete CU5 Candidate-invocation count differs");

    base::BinaryWriter evidence;
    constexpr std::string_view name =
        "SGE4-5.Spiral5.CU5.ArchitectureEvidence.V1";
    evidence.WriteU32(static_cast<std::uint32_t>(name.size()));
    evidence.WriteBytes(std::as_bytes(std::span(name.data(), name.size())));
    WriteAuthority(evidence, authority);
    const auto runtimeAuthority =
        execute::SerializeTemporalCandidateFamilyRuntimeAuthorityV1(runtime);
    evidence.WriteU32(static_cast<std::uint32_t>(runtimeAuthority.size()));
    evidence.WriteBytes(runtimeAuthority);
    evidence.WriteBytes(runtime.DomainIdentity());
    evidence.WriteBytes(runtime.CandidateBindingSetIdentity());
    evidence.WriteU64(runtime.DeviceEpoch());
    evidence.WriteU32(static_cast<std::uint32_t>(submissions.size()));
    for (const auto& submission : submissions)
        WriteSubmission(evidence, submission);
    evidence.WriteBytes(base::Sha256(evidence.Bytes()));

    std::cout
        << "Complete Temporal WARP qualification passed.\n"
        << "Schedules: P1/P2/P4/P8/P16/P32/P64/BurstThenHold/Irregular\n"
        << "Candidate invocations: 9 x 3 x 128 = 3456\n"
        << "Hold stability and no-unauthorized-write proofs: qualified\n"
        << "Architecture evidence SHA-256: "
        << base::ToHex(base::Sha256(evidence.Bytes())) << '\n';
    return std::move(evidence).Take();
}

std::vector<std::byte> RunControlledRecovery()
{
    const auto authority = BuildCanonicalAuthority();
    auto runtime = LoadRuntime(authority);
    const auto runtimeAuthorityBefore =
        execute::SerializeTemporalCandidateFamilyRuntimeAuthorityV1(runtime);
    const auto bindingSetBefore = runtime.CandidateBindingSetIdentity();
    const auto oldEpoch = runtime.DeviceEpoch();
    const auto oldHandle =
        execute::CaptureTemporalRuntimeEpochHandleV1(runtime);
    auto oldBinding = execute::BindCanonicalTemporalRuntimeInputsV1(runtime);
    if (!oldBinding) throw std::runtime_error(oldBinding.Error().message);
    auto oldSeed = execute::ReseedTemporalHistoriesV1(
        runtime, oldHandle, oldBinding.Value());
    if (!oldSeed) throw std::runtime_error(oldSeed.Error().message);

    const auto p64Identity =
        sem::UpdateScheduleIdentityV1(authority.schedules[6]);
    auto before = execute::SubmitTemporalScheduleV1(
        runtime, oldHandle, oldBinding.Value(), oldSeed.Value(), p64Identity);
    if (!before) throw std::runtime_error(before.Error().message);
    RequireQualifiedArchitecture(before.Value().architecture);
    const auto oldToken = before.Value().completion;
    const auto oldHistory = before.Value().retainedHistory;

    auto recovered = execute::RecoverTemporalCandidateFamilyRuntimeV1(
        runtime,
        execute::TemporalCandidateFamilyRecoveryModeV1::ControlledRebuild);
    if (!recovered)
        throw std::runtime_error(
            recovered.Error().stage + ": " + recovered.Error().message);
    const auto& report = recovered.Value();
    Require(report.stateBefore ==
                execute::TemporalCandidateFamilyRuntimeStateV1::Active &&
            report.stateAfter ==
                execute::TemporalCandidateFamilyRuntimeStateV1::Active,
            "Controlled Temporal Recovery state transition differs");
    Require(report.previousDeviceEpoch == oldEpoch &&
            report.newDeviceEpoch == oldEpoch + 1 &&
            runtime.DeviceEpoch() == oldEpoch + 1,
            "Controlled Temporal Recovery epoch transition differs");
    Require(report.adapterReacquired &&
            report.nativeDeviceRematerialized &&
            report.allRuntimeObjectsReleased &&
            report.executionContextRematerialized &&
            report.historyResourcesInvalidated &&
            report.temporalArgumentsInvalidated &&
            report.completionStateInvalidated &&
            report.readbackStateInvalidated &&
            report.dynamicRebindRequired &&
            report.explicitReseedRequired &&
            report.frozenAuthorityPreserved,
            "Controlled Temporal Recovery report is incomplete");
    Require(!runtime.DynamicInputsBound() && !runtime.HistorySeeded(),
            "Temporal input/history authority survived Recovery");
    Require(runtime.CandidateBindingSetIdentity() == bindingSetBefore,
            "Frozen Temporal Candidate authority changed across Recovery");
    Require(execute::SerializeTemporalCandidateFamilyRuntimeAuthorityV1(runtime) ==
                runtimeAuthorityBefore,
            "Temporal Runtime authority bytes changed across Recovery");

    auto staleHandle = execute::ValidateTemporalRuntimeEpochHandleV1(
        runtime, oldHandle);
    Require(!staleHandle &&
            staleHandle.Error().stage == "temporal-runtime/stale-epoch",
            "stale Temporal Runtime handle escaped rejection");
    auto staleToken = execute::ValidateTemporalRuntimeSubmissionTokenV1(
        runtime, oldToken);
    Require(!staleToken &&
            staleToken.Error().stage == "temporal-runtime/stale-epoch",
            "stale Temporal completion token escaped rejection");
    auto staleHistory = execute::ValidateTemporalHistoryHandleV1(
        runtime, oldHistory);
    Require(!staleHistory &&
            staleHistory.Error().stage == "temporal-runtime/stale-epoch",
            "stale Temporal history escaped rejection");

    const auto newHandle =
        execute::CaptureTemporalRuntimeEpochHandleV1(runtime);
    auto noRebind = execute::SubmitTemporalScheduleV1(
        runtime, newHandle, oldBinding.Value(), oldSeed.Value(), p64Identity);
    Require(!noRebind &&
            noRebind.Error().stage == "temporal-runtime/rebind-required",
            "post-Recovery submission succeeded without rebind");

    auto newBinding = execute::BindCanonicalTemporalRuntimeInputsV1(runtime);
    if (!newBinding) throw std::runtime_error(newBinding.Error().message);
    auto noReseed = execute::SubmitTemporalScheduleV1(
        runtime, newHandle, newBinding.Value(), oldSeed.Value(), p64Identity);
    Require(!noReseed &&
            noReseed.Error().stage == "temporal-runtime/reseed-required",
            "post-Recovery submission succeeded without explicit reseed");

    auto newSeed = execute::ReseedTemporalHistoriesV1(
        runtime, newHandle, newBinding.Value());
    if (!newSeed) throw std::runtime_error(newSeed.Error().message);
    auto after = execute::SubmitTemporalScheduleV1(
        runtime, newHandle, newBinding.Value(), newSeed.Value(), p64Identity);
    if (!after) throw std::runtime_error(after.Error().message);
    RequireQualifiedArchitecture(after.Value().architecture);
    Require(after.Value().historyResourcesMaterialized &&
            after.Value().temporalArgumentsRegenerated &&
            after.Value().completionStateMaterialized &&
            after.Value().readbackStateMaterialized,
            "post-Recovery Temporal objects were not rematerialized");
    RequireSameObservation(before.Value().architecture, after.Value().architecture);

    base::BinaryWriter evidence;
    constexpr std::string_view name =
        "SGE4-5.Spiral5.CU5.ControlledRecoveryEvidence.V1";
    evidence.WriteU32(static_cast<std::uint32_t>(name.size()));
    evidence.WriteBytes(std::as_bytes(std::span(name.data(), name.size())));
    WriteAuthority(evidence, authority);
    evidence.WriteBytes(bindingSetBefore);
    evidence.WriteU64(oldEpoch);
    evidence.WriteU64(runtime.DeviceEpoch());
    WriteSubmission(evidence, before.Value());
    WriteSubmission(evidence, after.Value());
    evidence.WriteBytes(base::Sha256(evidence.Bytes()));

    std::cout
        << "Controlled Temporal Recovery passed.\n"
        << "Epoch: " << oldEpoch << "->" << runtime.DeviceEpoch() << '\n'
        << "Stale handle/token/history rejection: passed\n"
        << "Explicit dynamic rebind and generation-zero reseed: passed\n"
        << "Controlled evidence SHA-256: "
        << base::ToHex(base::Sha256(evidence.Bytes())) << '\n';
    return std::move(evidence).Take();
}

void RunActualRemoval()
{
    const auto authority = BuildCanonicalAuthority();
    auto runtime = LoadRuntime(authority);
    const auto frozenIdentity = runtime.CandidateBindingSetIdentity();
    const auto oldEpoch = runtime.DeviceEpoch();
    const auto oldHandle =
        execute::CaptureTemporalRuntimeEpochHandleV1(runtime);
    auto binding = execute::BindCanonicalTemporalRuntimeInputsV1(runtime);
    if (!binding) throw std::runtime_error(binding.Error().message);
    auto seed = execute::ReseedTemporalHistoriesV1(
        runtime, oldHandle, binding.Value());
    if (!seed) throw std::runtime_error(seed.Error().message);
    const auto p4Identity =
        sem::UpdateScheduleIdentityV1(authority.schedules[2]);
    auto before = execute::SubmitTemporalScheduleV1(
        runtime, oldHandle, binding.Value(), seed.Value(), p4Identity);
    if (!before) throw std::runtime_error(before.Error().message);
    const auto oldToken = before.Value().completion;
    const auto oldHistory = before.Value().retainedHistory;

    auto removed = execute::RecoverTemporalCandidateFamilyRuntimeV1(
        runtime,
        execute::TemporalCandidateFamilyRecoveryModeV1::ForceRemovalForTest);
    if (!removed)
        throw std::runtime_error(
            removed.Error().stage + ": " + removed.Error().message);
    const auto& report = removed.Value();
    Require(report.forcedRemoval &&
            report.stateAfter ==
                execute::TemporalCandidateFamilyRuntimeStateV1::AwaitingAdapter &&
            runtime.State() ==
                execute::TemporalCandidateFamilyRuntimeStateV1::AwaitingAdapter &&
            runtime.DeviceEpoch() == oldEpoch &&
            report.allRuntimeObjectsReleased &&
            report.historyResourcesInvalidated &&
            report.temporalArgumentsInvalidated &&
            report.completionStateInvalidated &&
            report.readbackStateInvalidated &&
            report.dynamicRebindRequired &&
            report.explicitReseedRequired &&
            report.frozenAuthorityPreserved,
            "actual Temporal RemoveDevice quarantine report differs");
    Require(runtime.CandidateBindingSetIdentity() == frozenIdentity,
            "Frozen Temporal authority changed during actual removal");

    auto rejectedHandle = execute::ValidateTemporalRuntimeEpochHandleV1(
        runtime, oldHandle);
    Require(!rejectedHandle &&
            rejectedHandle.Error().stage == "temporal-runtime/device-state",
            "removed-domain handle was not rejected");
    auto rejectedToken = execute::ValidateTemporalRuntimeSubmissionTokenV1(
        runtime, oldToken);
    Require(!rejectedToken &&
            rejectedToken.Error().stage == "temporal-runtime/device-state",
            "removed-domain completion token was not rejected");
    auto rejectedHistory = execute::ValidateTemporalHistoryHandleV1(
        runtime, oldHistory);
    Require(!rejectedHistory &&
            rejectedHistory.Error().stage == "temporal-runtime/device-state",
            "removed-domain history was not rejected");
    auto rejectedSubmit = execute::SubmitTemporalScheduleV1(
        runtime, oldHandle, binding.Value(), seed.Value(), p4Identity);
    Require(!rejectedSubmit &&
            rejectedSubmit.Error().stage == "temporal-runtime/device-state",
            "removed-domain submission was not rejected");

    auto retry = execute::RecoverTemporalCandidateFamilyRuntimeV1(
        runtime,
        execute::TemporalCandidateFamilyRecoveryModeV1::RetryAdapterReacquisition);
    if (!retry) throw std::runtime_error(retry.Error().message);
    Require(!retry.Value().adapterReacquired &&
            runtime.State() ==
                execute::TemporalCandidateFamilyRuntimeStateV1::AwaitingAdapter &&
            runtime.DeviceEpoch() == oldEpoch,
            "removed WARP LUID exclusion was not preserved");

    std::cout
        << "Actual Temporal ID3D12Device5::RemoveDevice quarantine passed.\n"
        << "Old process: AwaitingAdapter; removed WARP LUID excluded.\n"
        << "All submissions, tokens, and retained histories rejected.\n"
        << "Removal reason=" << report.removalReason << '\n';
}

std::vector<std::byte> RunFreshRematerialization()
{
    const auto authority = BuildCanonicalAuthority();
    auto runtime = LoadRuntime(authority);
    auto binding = execute::BindCanonicalTemporalRuntimeInputsV1(runtime);
    if (!binding) throw std::runtime_error(binding.Error().message);
    const auto handle = execute::CaptureTemporalRuntimeEpochHandleV1(runtime);
    auto seed = execute::ReseedTemporalHistoriesV1(
        runtime, handle, binding.Value());
    if (!seed) throw std::runtime_error(seed.Error().message);

    constexpr std::array<std::size_t, 3> selected{0, 6, 8};
    std::vector<execute::TemporalRuntimeSubmissionV1> submissions;
    for (const auto index : selected)
    {
        auto value = execute::SubmitTemporalScheduleV1(
            runtime, handle, binding.Value(), seed.Value(),
            sem::UpdateScheduleIdentityV1(authority.schedules[index]));
        if (!value) throw std::runtime_error(value.Error().message);
        RequireQualifiedArchitecture(value.Value().architecture);
        submissions.push_back(std::move(value).Value());
    }

    base::BinaryWriter evidence;
    constexpr std::string_view name =
        "SGE4-5.Spiral5.CU5.FreshRematerializationEvidence.V1";
    evidence.WriteU32(static_cast<std::uint32_t>(name.size()));
    evidence.WriteBytes(std::as_bytes(std::span(name.data(), name.size())));
    WriteAuthority(evidence, authority);
    const auto runtimeAuthority =
        execute::SerializeTemporalCandidateFamilyRuntimeAuthorityV1(runtime);
    evidence.WriteU32(static_cast<std::uint32_t>(runtimeAuthority.size()));
    evidence.WriteBytes(runtimeAuthority);
    for (const auto& submission : submissions)
        WriteSubmission(evidence, submission);
    evidence.WriteBytes(base::Sha256(evidence.Bytes()));

    std::cout
        << "Fresh-process Temporal rematerialization passed.\n"
        << "Schedules: P1/P64/Irregular; Candidates: A/B/C\n"
        << "Fresh evidence SHA-256: "
        << base::ToHex(base::Sha256(evidence.Bytes())) << '\n';
    return std::move(evidence).Take();
}
}

int main(int argc, char** argv)
{
    try
    {
        enum class Mode
        {
            None,
            Qualification,
            Controlled,
            ActualRemoval,
            FreshRematerialization
        };

        Mode mode = Mode::None;
        std::filesystem::path emitPath;
        for (int index = 1; index < argc; ++index)
        {
            const std::string_view argument = argv[index];
            if (argument == "--qualification")
                mode = Mode::Qualification;
            else if (argument == "--controlled")
                mode = Mode::Controlled;
            else if (argument == "--actual-removal")
                mode = Mode::ActualRemoval;
            else if (argument == "--fresh-rematerialization")
                mode = Mode::FreshRematerialization;
            else if (argument == "--emit" && index + 1 < argc)
                emitPath = std::filesystem::path(argv[++index]);
            else
                throw std::runtime_error(
                    "usage: 151_Spiral5ArchitectureQualificationTests "
                    "--qualification|--controlled|--actual-removal|"
                    "--fresh-rematerialization [--emit path]");
        }

        if (mode == Mode::None)
            throw std::runtime_error("one CU5 mode is required");
        if (mode == Mode::ActualRemoval && !emitPath.empty())
            throw std::runtime_error(
                "actual-removal evidence is intentionally noncanonical");

        std::vector<std::byte> evidence;
        switch (mode)
        {
        case Mode::Qualification:
            evidence = RunArchitectureQualification();
            break;
        case Mode::Controlled:
            evidence = RunControlledRecovery();
            break;
        case Mode::ActualRemoval:
            RunActualRemoval();
            break;
        case Mode::FreshRematerialization:
            evidence = RunFreshRematerialization();
            break;
        default:
            throw std::runtime_error("invalid CU5 mode");
        }

        if (!emitPath.empty())
            WriteFile(emitPath, std::move(evidence));

        std::cout << "Spiral 5 CU5 architecture qualification mode passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Spiral 5 CU5 failure: " << error.what() << '\n';
        return 1;
    }
}
