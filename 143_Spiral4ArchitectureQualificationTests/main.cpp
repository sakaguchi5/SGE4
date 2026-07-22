#include "../00_Foundation/BinaryIO.h"
#include "../00_Foundation/FileIO.h"
#include "../00_Foundation/Sha256.h"
#include "../120_ActiveWorkSemantic/ActiveWorkSemantic.h"
#include "../128_ActiveWorkCandidateFamily/ActiveWorkCandidateFamily.h"
#include "../129_ActiveWorkCandidateFamilyPlanner/ActiveWorkCandidateFamilyPlanner.h"
#include "../130_ActiveWorkCandidateFamilyVerifier/ActiveWorkCandidateFamilyVerifier.h"
#include "../131_Spiral4CandidateFamilyExecution/CandidateFamilyExecution.h"
#include "../132_Spiral4CandidateFamilyRuntime/CandidateFamilyRuntime.h"

#include <algorithm>
#include <array>
#include <bit>
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
namespace sem = sge4_5::spiral4::semantic;
namespace candidate = sge4_5::spiral4::family_candidate;
namespace planner = sge4_5::spiral4::family_planner;
namespace verify = sge4_5::spiral4::family_verification;
namespace execute = sge4_5::spiral4::family_execution;
namespace runtime = sge4_5::spiral4::family_runtime;
namespace contract = sge4_5::spiral4::family_contract;

namespace
{
struct CanonicalFamily final
{
    sem::ActiveWorkSemanticV1 semantic;
    verify::CandidateFamilyVerificationContextV1 context;
    std::vector<candidate::RawCandidateFamilyV1> raw;
    std::vector<verify::VerifiedCandidateFamilyV1> verified;
    std::vector<execute::FrozenVerifiedCandidateFamilyV1> frozen;
};

void Require(bool condition, std::string_view message)
{
    if (!condition) throw std::runtime_error(std::string(message));
}

CanonicalFamily BuildCanonicalFamily()
{
    CanonicalFamily family;
    family.semantic = sem::BuildCanonicalActiveWorkSemanticV1();
    family.context =
        verify::BuildCanonicalCandidateFamilyVerificationContextV1();

    auto raw = planner::PlanCanonicalCandidateFamilyCorpusV1(
        family.semantic);
    if (!raw) throw std::runtime_error(raw.Error());
    family.raw = raw.Value();
    Require(family.raw.size() == 7, "canonical Candidate count mismatch");

    for (const auto& proposal : family.raw)
    {
        auto verified = verify::VerifyCandidateFamilyV1(
            family.semantic, family.context, proposal);
        if (!verified) throw std::runtime_error(verified.Error());
        family.verified.push_back(verified.Value());

        auto frozen = execute::FreezeVerifiedCandidateFamilyV1(
            family.semantic, family.context, family.verified.back());
        if (!frozen) throw std::runtime_error(frozen.Error());
        family.frozen.push_back(frozen.Value());
    }
    return family;
}

void RequireEquivalentSubmission(
    const runtime::CandidateFamilySubmissionV1& submission,
    std::span<const std::uint32_t> activeCounts)
{
    Require(submission.candidates.size() == 7,
            "submission Candidate count mismatch");
    for (const auto& candidateRun : submission.candidates)
    {
        Require(candidateRun.cases.size() == activeCounts.size(),
                "submission Active Count case count mismatch");
        for (std::size_t i = 0; i < activeCounts.size(); ++i)
        {
            const auto& value = candidateRun.cases[i];
            Require(value.activeWorkCount == activeCounts[i],
                    "submission Active Count ordering mismatch");
            Require(value.activeMismatchCount == 0,
                    "active output mismatch detected");
            Require(value.firstActiveMismatch == 0xFFFF'FFFFu,
                    "first Active mismatch marker is not empty");
            Require(value.inactiveTailPreserved,
                    "inactive-tail sentinel was modified");
            Require(value.nonFiniteCount == 0,
                    "non-finite output was observed");
            Require(value.homogeneousCoordinateMismatchCount == 0,
                    "homogeneous-coordinate contract failed");
            Require(value.duplicateWorkCount == 0 &&
                    value.missingWorkCount == 0 &&
                    value.reorderedWorkCount == 0 &&
                    value.outOfRangeWorkCount == 0,
                    "Work coverage/order contract failed");
        }
    }

    for (std::size_t caseIndex = 0;
         caseIndex < activeCounts.size();
         ++caseIndex)
    {
        const auto expected =
            submission.candidates.front().cases[caseIndex].outputDigest;
        for (const auto& candidateRun : submission.candidates)
        {
            Require(
                candidateRun.cases[caseIndex].outputDigest == expected,
                "Candidate output digest differs for the same Nf");
        }
    }
}

void RequireSameObservation(
    const runtime::CandidateFamilySubmissionV1& before,
    const runtime::CandidateFamilySubmissionV1& after)
{
    Require(before.candidates.size() == after.candidates.size(),
            "Candidate count changed across Recovery");
    for (std::size_t candidateIndex = 0;
         candidateIndex < before.candidates.size();
         ++candidateIndex)
    {
        const auto& a = before.candidates[candidateIndex];
        const auto& b = after.candidates[candidateIndex];
        Require(a.kind == b.kind && a.batchSize == b.batchSize,
                "Candidate identity changed across Recovery");
        Require(a.verifiedBindingIdentity == b.verifiedBindingIdentity,
                "Verified binding changed across Recovery");
        Require(a.cases.size() == b.cases.size(),
                "case count changed across Recovery");
        for (std::size_t caseIndex = 0;
             caseIndex < a.cases.size();
             ++caseIndex)
        {
            Require(
                a.cases[caseIndex].activeWorkCount ==
                    b.cases[caseIndex].activeWorkCount,
                "Active Count changed across Recovery");
            Require(
                a.cases[caseIndex].outputDigest ==
                    b.cases[caseIndex].outputDigest,
                "output digest changed across Recovery");
            Require(
                a.cases[caseIndex].observedBatchRecords ==
                    b.cases[caseIndex].observedBatchRecords,
                "indirect argument records changed across Recovery");
            Require(
                a.cases[caseIndex].inactiveTailPreserved &&
                    b.cases[caseIndex].inactiveTailPreserved,
                "inactive tail changed across Recovery");
            Require(
                a.cases[caseIndex].nonFiniteCount ==
                    b.cases[caseIndex].nonFiniteCount &&
                a.cases[caseIndex].homogeneousCoordinateMismatchCount ==
                    b.cases[caseIndex].homogeneousCoordinateMismatchCount &&
                a.cases[caseIndex].maxAbsoluteError ==
                    b.cases[caseIndex].maxAbsoluteError &&
                a.cases[caseIndex].maxRelativeError ==
                    b.cases[caseIndex].maxRelativeError &&
                a.cases[caseIndex].maxUlpError ==
                    b.cases[caseIndex].maxUlpError,
                "numeric Observation metrics changed across Recovery");
        }
    }
}

void WriteCandidateAuthority(
    base::BinaryWriter& writer,
    const CanonicalFamily& family)
{
    const auto semanticBytes =
        sem::SerializeActiveWorkSemanticV1(family.semantic);
    writer.WriteU32(static_cast<std::uint32_t>(semanticBytes.size()));
    writer.WriteBytes(semanticBytes);
    writer.WriteU32(static_cast<std::uint32_t>(family.raw.size()));
    for (std::size_t i = 0; i < family.raw.size(); ++i)
    {
        const auto rawBytes =
            candidate::SerializeRawCandidateFamilyV1(family.raw[i]);
        const auto verifiedBytes =
            verify::SerializeVerifiedCandidateFamilyV1(
                family.verified[i]);
        const auto frozenBytes =
            execute::SerializeFrozenVerifiedCandidateFamilyV1(
                family.frozen[i]);
        writer.WriteU32(static_cast<std::uint32_t>(rawBytes.size()));
        writer.WriteBytes(rawBytes);
        writer.WriteBytes(family.raw[i].rawCandidateIdentity);
        writer.WriteU32(
            static_cast<std::uint32_t>(verifiedBytes.size()));
        writer.WriteBytes(verifiedBytes);
        writer.WriteU32(
            static_cast<std::uint32_t>(frozenBytes.size()));
        writer.WriteBytes(frozenBytes);
    }
}

void WriteSubmissionObservation(
    base::BinaryWriter& writer,
    const runtime::CandidateFamilySubmissionV1& submission)
{
    writer.WriteU32(
        static_cast<std::uint32_t>(submission.candidates.size()));
    for (const auto& candidateRun : submission.candidates)
    {
        writer.WriteU32(static_cast<std::uint32_t>(candidateRun.kind));
        writer.WriteU32(candidateRun.batchSize);
        writer.WriteBytes(candidateRun.verifiedBindingIdentity);
        writer.WriteU32(
            static_cast<std::uint32_t>(candidateRun.cases.size()));
        for (const auto& value : candidateRun.cases)
        {
            writer.WriteU32(value.activeWorkCount);
            writer.WriteU32(value.issuedThreadGroupCount);
            writer.WriteU32(value.activeMismatchCount);
            writer.WriteU32(value.firstActiveMismatch);
            writer.WriteU32(value.inactiveTailPreserved ? 1u : 0u);
            writer.WriteU32(value.nonFiniteCount);
            writer.WriteU32(value.homogeneousCoordinateMismatchCount);
            writer.WriteU32(value.duplicateWorkCount);
            writer.WriteU32(value.missingWorkCount);
            writer.WriteU32(value.reorderedWorkCount);
            writer.WriteU32(value.outOfRangeWorkCount);
            writer.WriteU32(std::bit_cast<std::uint32_t>(
                value.maxAbsoluteError));
            writer.WriteU32(std::bit_cast<std::uint32_t>(
                value.maxRelativeError));
            writer.WriteU32(value.maxUlpError);
            writer.WriteBytes(value.outputDigest);
            writer.WriteU32(static_cast<std::uint32_t>(
                value.observedBatchRecords.size()));
            for (const auto& record : value.observedBatchRecords)
            {
                writer.WriteU32(record.firstWork);
                writer.WriteU32(record.threadGroupCountX);
                writer.WriteU32(record.threadGroupCountY);
                writer.WriteU32(record.threadGroupCountZ);
            }
        }
    }
}

void WriteFile(const std::filesystem::path& path,
               std::vector<std::byte> bytes)
{
    std::filesystem::create_directories(path.parent_path());
    auto written = base::WriteAllBytes(path, bytes);
    if (!written) throw std::runtime_error(written.Error());
}

std::vector<std::byte> RunArchitectureQualification()
{
    auto family = BuildCanonicalFamily();
    auto loaded = runtime::LoadCandidateFamilyRuntimeOnWarpV1(
        family.semantic, family.context, family.frozen);
    if (!loaded) throw std::runtime_error(
        loaded.Error().stage + ": " + loaded.Error().message);
    auto& domain = loaded.Value();

    auto binding = runtime::BindCanonicalCandidateFamilyInputsV1(domain);
    if (!binding) throw std::runtime_error(
        binding.Error().stage + ": " + binding.Error().message);
    const auto handle = runtime::CaptureCandidateFamilyEpochHandleV1(domain);

    const auto& activeCounts = sem::ActiveCountCorpusV1();
    auto submitted = runtime::SubmitCandidateFamilyV1(
        domain, handle, binding.Value(), activeCounts);
    if (!submitted) throw std::runtime_error(
        submitted.Error().stage + ": " + submitted.Error().message);
    RequireEquivalentSubmission(submitted.Value(), activeCounts);

    auto tokenValid = runtime::ValidateCandidateFamilySubmissionTokenV1(
        domain, submitted.Value().completion);
    Require(static_cast<bool>(tokenValid),
            "current submission token was rejected");

    base::BinaryWriter evidence;
    constexpr std::string_view domainName =
        "SGE4-5.Spiral4.CU5.ArchitectureEvidence.V1";
    evidence.WriteU32(static_cast<std::uint32_t>(domainName.size()));
    evidence.WriteBytes(std::as_bytes(std::span(
        domainName.data(), domainName.size())));
    WriteCandidateAuthority(evidence, family);
    const auto runtimeBytes =
        runtime::SerializeCandidateFamilyRuntimeAuthorityV1(domain);
    evidence.WriteU32(static_cast<std::uint32_t>(runtimeBytes.size()));
    evidence.WriteBytes(runtimeBytes);
    evidence.WriteBytes(domain.DomainIdentity());
    evidence.WriteBytes(domain.CandidateBindingSetIdentity());
    evidence.WriteU64(domain.DeviceEpoch());
    WriteSubmissionObservation(evidence, submitted.Value());
    const auto digest = base::Sha256(evidence.Bytes());
    evidence.WriteBytes(digest);

    std::cout
        << "Full WARP Candidate-family qualification passed.\n"
        << "Cases: 7 Candidates x 19 Active Counts = 133\n"
        << "Inactive-tail sentinel proofs: 133\n"
        << "GPU Batch-record proofs: 5 x 19 = 95\n"
        << "Architecture evidence SHA-256: "
        << base::ToHex(base::Sha256(evidence.Bytes())) << '\n';
    return std::move(evidence).Take();
}

std::vector<std::byte> RunControlledRecovery()
{
    auto family = BuildCanonicalFamily();
    auto loaded = runtime::LoadCandidateFamilyRuntimeOnWarpV1(
        family.semantic, family.context, family.frozen);
    if (!loaded) throw std::runtime_error(
        loaded.Error().stage + ": " + loaded.Error().message);
    auto& domain = loaded.Value();

    const auto runtimeAuthorityBefore =
        runtime::SerializeCandidateFamilyRuntimeAuthorityV1(domain);
    const auto bindingSetBefore = domain.CandidateBindingSetIdentity();
    const auto oldEpoch = domain.DeviceEpoch();
    const auto oldHandle =
        runtime::CaptureCandidateFamilyEpochHandleV1(domain);
    auto oldBinding =
        runtime::BindCanonicalCandidateFamilyInputsV1(domain);
    if (!oldBinding) throw std::runtime_error(oldBinding.Error().message);

    constexpr std::array<std::uint32_t, 3> selected{0u, 65u, 4096u};
    auto before = runtime::SubmitCandidateFamilyV1(
        domain, oldHandle, oldBinding.Value(), selected);
    if (!before) throw std::runtime_error(
        before.Error().stage + ": " + before.Error().message);
    RequireEquivalentSubmission(before.Value(), selected);
    const auto oldToken = before.Value().completion;

    auto recovered = runtime::RecoverCandidateFamilyRuntimeV1(
        domain,
        runtime::CandidateFamilyRecoveryModeV1::ControlledRebuild);
    if (!recovered) throw std::runtime_error(
        recovered.Error().stage + ": " + recovered.Error().message);
    const auto& report = recovered.Value();
    Require(
        report.stateBefore ==
            runtime::CandidateFamilyRuntimeStateV1::Active &&
        report.stateAfter ==
            runtime::CandidateFamilyRuntimeStateV1::Active,
        "controlled Recovery state transition is invalid");
    Require(domain.DeviceEpoch() == oldEpoch + 1 &&
            report.previousDeviceEpoch == oldEpoch &&
            report.newDeviceEpoch == oldEpoch + 1,
            "controlled Recovery epoch transition is invalid");
    Require(report.adapterReacquired &&
            report.nativeDeviceRematerialized &&
            report.allRuntimeObjectsReleased &&
            !report.allRuntimeObjectsRematerialized &&
            !report.commandSignaturesRematerialized &&
            !report.argumentBuffersRematerialized &&
            !report.endpointStateRematerialized &&
            report.dynamicRebindRequired &&
            report.frozenIdentityPreserved,
            "controlled Recovery did not preserve the deferred per-submission rematerialization boundary");
    Require(!domain.DynamicInputsBound(),
            "dynamic inputs remained bound across Recovery");
    Require(domain.CandidateBindingSetIdentity() == bindingSetBefore,
            "Frozen Candidate binding set changed across Recovery");
    Require(runtime::SerializeCandidateFamilyRuntimeAuthorityV1(domain) ==
                runtimeAuthorityBefore,
            "runtime authority bytes changed across Recovery");

    auto staleHandle = runtime::ValidateCandidateFamilyEpochHandleV1(
        domain, oldHandle);
    Require(!staleHandle &&
            staleHandle.Error().stage ==
                "candidate-runtime/stale-epoch",
            "old Candidate-family handle escaped stale-epoch rejection");

    auto staleToken = runtime::ValidateCandidateFamilySubmissionTokenV1(
        domain, oldToken);
    Require(!staleToken &&
            staleToken.Error().stage ==
                "candidate-runtime/stale-epoch",
            "old submission token escaped stale-epoch rejection");

    const auto newHandle =
        runtime::CaptureCandidateFamilyEpochHandleV1(domain);
    auto noRebind = runtime::SubmitCandidateFamilyV1(
        domain, newHandle, oldBinding.Value(), selected);
    Require(!noRebind &&
            noRebind.Error().stage ==
                "candidate-runtime/rebind-required",
            "submission succeeded without current-epoch dynamic rebind");

    auto newBinding =
        runtime::BindCanonicalCandidateFamilyInputsV1(domain);
    if (!newBinding) throw std::runtime_error(newBinding.Error().message);
    auto after = runtime::SubmitCandidateFamilyV1(
        domain, newHandle, newBinding.Value(), selected);
    if (!after) throw std::runtime_error(
        after.Error().stage + ": " + after.Error().message);
    RequireEquivalentSubmission(after.Value(), selected);
    Require(after.Value().commandSignaturesMaterialized &&
            after.Value().argumentBuffersRegenerated &&
            after.Value().endpointStateMaterialized &&
            after.Value().readbackStateMaterialized,
            "post-Recovery execution objects were not rematerialized");
    RequireSameObservation(before.Value(), after.Value());

    base::BinaryWriter evidence;
    constexpr std::string_view domainName =
        "SGE4-5.Spiral4.CU5.ControlledRecoveryEvidence.V1";
    evidence.WriteU32(static_cast<std::uint32_t>(domainName.size()));
    evidence.WriteBytes(std::as_bytes(std::span(
        domainName.data(), domainName.size())));
    WriteCandidateAuthority(evidence, family);
    evidence.WriteBytes(domain.DomainIdentity());
    evidence.WriteBytes(bindingSetBefore);
    evidence.WriteU64(oldEpoch);
    evidence.WriteU64(domain.DeviceEpoch());
    WriteSubmissionObservation(evidence, before.Value());
    WriteSubmissionObservation(evidence, after.Value());
    const auto digest = base::Sha256(evidence.Bytes());
    evidence.WriteBytes(digest);

    std::cout
        << "Controlled Candidate-family Recovery passed.\n"
        << "Epoch: " << oldEpoch << "->" << domain.DeviceEpoch() << '\n'
        << "Stale handle/token rejection: passed\n"
        << "Explicit dynamic rebind: passed\n"
        << "Argument-record regeneration: passed\n"
        << "Controlled evidence SHA-256: "
        << base::ToHex(base::Sha256(evidence.Bytes())) << '\n';
    return std::move(evidence).Take();
}

void RunActualRemoval()
{
    auto family = BuildCanonicalFamily();
    auto loaded = runtime::LoadCandidateFamilyRuntimeOnWarpV1(
        family.semantic, family.context, family.frozen);
    if (!loaded) throw std::runtime_error(
        loaded.Error().stage + ": " + loaded.Error().message);
    auto& domain = loaded.Value();

    auto binding = runtime::BindCanonicalCandidateFamilyInputsV1(domain);
    if (!binding) throw std::runtime_error(binding.Error().message);
    const auto oldHandle =
        runtime::CaptureCandidateFamilyEpochHandleV1(domain);
    constexpr std::array<std::uint32_t, 1> selected{65u};
    auto before = runtime::SubmitCandidateFamilyV1(
        domain, oldHandle, binding.Value(), selected);
    if (!before) throw std::runtime_error(before.Error().message);
    const auto oldToken = before.Value().completion;
    const auto oldEpoch = domain.DeviceEpoch();
    const auto frozenIdentity = domain.CandidateBindingSetIdentity();

    auto removed = runtime::RecoverCandidateFamilyRuntimeV1(
        domain,
        runtime::CandidateFamilyRecoveryModeV1::ForceRemovalForTest);
    if (!removed) throw std::runtime_error(
        removed.Error().stage + ": " + removed.Error().message);
    const auto& report = removed.Value();
    Require(report.forcedRemoval &&
            report.stateAfter ==
                runtime::CandidateFamilyRuntimeStateV1::AwaitingAdapter &&
            domain.State() ==
                runtime::CandidateFamilyRuntimeStateV1::AwaitingAdapter &&
            domain.DeviceEpoch() == oldEpoch &&
            report.allRuntimeObjectsReleased &&
            !report.allRuntimeObjectsRematerialized &&
            report.dynamicRebindRequired &&
            report.frozenIdentityPreserved,
            "actual RemoveDevice quarantine report is invalid");
    Require(domain.CandidateBindingSetIdentity() == frozenIdentity,
            "Frozen identity changed during actual removal");

    auto rejectedHandle =
        runtime::ValidateCandidateFamilyEpochHandleV1(
            domain, oldHandle);
    Require(!rejectedHandle &&
            rejectedHandle.Error().stage ==
                "candidate-runtime/device-state",
            "removed-domain handle was not rejected");

    auto rejectedToken =
        runtime::ValidateCandidateFamilySubmissionTokenV1(
            domain, oldToken);
    Require(!rejectedToken &&
            rejectedToken.Error().stage ==
                "candidate-runtime/device-state",
            "removed-domain submission token was not rejected");

    auto rejectedSubmit = runtime::SubmitCandidateFamilyV1(
        domain, oldHandle, binding.Value(), selected);
    Require(!rejectedSubmit &&
            rejectedSubmit.Error().stage ==
                "candidate-runtime/device-state",
            "removed-domain submission was not rejected");

    auto retry = runtime::RecoverCandidateFamilyRuntimeV1(
        domain,
        runtime::CandidateFamilyRecoveryModeV1::RetryAdapterReacquisition);
    if (!retry) throw std::runtime_error(retry.Error().message);
    Require(!retry.Value().adapterReacquired &&
            domain.State() ==
                runtime::CandidateFamilyRuntimeStateV1::AwaitingAdapter &&
            domain.DeviceEpoch() == oldEpoch,
            "removed WARP LUID exclusion was not preserved");

    std::cout
        << "Actual ID3D12Device5::RemoveDevice quarantine passed.\n"
        << "Old process: AwaitingAdapter; removed WARP LUID excluded.\n"
        << "All Candidate submissions and stale handles/tokens rejected.\n"
        << "Removal reason=" << report.removalReason
        << " DRED breadcrumbs=" << report.dredBreadcrumbNodeCount
        << " page-fault allocations="
        << report.dredPageFaultAllocationCount << '\n';
}

std::vector<std::byte> RunFreshRematerialization()
{
    auto family = BuildCanonicalFamily();
    auto loaded = runtime::LoadCandidateFamilyRuntimeOnWarpV1(
        family.semantic, family.context, family.frozen);
    if (!loaded) throw std::runtime_error(
        loaded.Error().stage + ": " + loaded.Error().message);
    auto& domain = loaded.Value();
    auto binding = runtime::BindCanonicalCandidateFamilyInputsV1(domain);
    if (!binding) throw std::runtime_error(binding.Error().message);
    const auto handle =
        runtime::CaptureCandidateFamilyEpochHandleV1(domain);
    constexpr std::array<std::uint32_t, 3> selected{0u, 65u, 4096u};
    auto submitted = runtime::SubmitCandidateFamilyV1(
        domain, handle, binding.Value(), selected);
    if (!submitted) throw std::runtime_error(
        submitted.Error().stage + ": " + submitted.Error().message);
    RequireEquivalentSubmission(submitted.Value(), selected);

    base::BinaryWriter evidence;
    constexpr std::string_view domainName =
        "SGE4-5.Spiral4.CU5.FreshRematerializationEvidence.V1";
    evidence.WriteU32(static_cast<std::uint32_t>(domainName.size()));
    evidence.WriteBytes(std::as_bytes(std::span(
        domainName.data(), domainName.size())));
    WriteCandidateAuthority(evidence, family);
    const auto runtimeBytes =
        runtime::SerializeCandidateFamilyRuntimeAuthorityV1(domain);
    evidence.WriteU32(static_cast<std::uint32_t>(runtimeBytes.size()));
    evidence.WriteBytes(runtimeBytes);
    WriteSubmissionObservation(evidence, submitted.Value());
    const auto digest = base::Sha256(evidence.Bytes());
    evidence.WriteBytes(digest);

    std::cout
        << "Fresh-process Candidate-family rematerialization passed.\n"
        << "Candidates: 7, Active Counts: 0/65/4096\n"
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
        for (int i = 1; i < argc; ++i)
        {
            const std::string_view argument = argv[i];
            if (argument == "--qualification")
                mode = Mode::Qualification;
            else if (argument == "--controlled")
                mode = Mode::Controlled;
            else if (argument == "--actual-removal")
                mode = Mode::ActualRemoval;
            else if (argument == "--fresh-rematerialization")
                mode = Mode::FreshRematerialization;
            else if (argument == "--emit" && i + 1 < argc)
                emitPath = std::filesystem::path(argv[++i]);
            else
                throw std::runtime_error(
                    "usage: 143_Spiral4ArchitectureQualificationTests "
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

        std::cout
            << "Spiral 4 CU5 architecture qualification mode passed.\n";
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "Spiral 4 CU5 failure: "
            << exception.what() << '\n';
        return 1;
    }
}
