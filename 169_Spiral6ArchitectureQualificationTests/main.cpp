#include "../00_Foundation/BinaryIO.h"
#include "../00_Foundation/FileIO.h"
#include "../00_Foundation/Sha256.h"
#include "../154_SparseWorkSetSemantic/SparseWorkSetSemantic.h"
#include "../160_SparseCandidateFamily/SparseCandidateFamily.h"
#include "../161_SparseCandidateFamilyPlanner/SparseCandidateFamilyPlanner.h"
#include "../162_SparseCandidateFamilyVerifier/SparseCandidateFamilyVerifier.h"
#include "../163_Spiral6SparseFamilyExecution/Spiral6SparseFamilyExecution.h"

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
namespace sem = sge4_5::spiral6::semantic;
namespace candidate = sge4_5::spiral6::family_candidate;
namespace planner = sge4_5::spiral6::family_planner;
namespace verify = sge4_5::spiral6::family_verification;
namespace execute = sge4_5::spiral6::family_execution;

namespace
{
struct CanonicalAuthority final
{
    sem::SparsePointTransformSemanticV1 semantic;
    verify::SparseFamilyVerificationContextV1 context;
    std::vector<execute::SparseSetCandidateAuthorityV1> runtimeAuthorities;
};

void Require(bool condition, std::string_view message)
{
    if (!condition) throw std::runtime_error(std::string(message));
}

CanonicalAuthority BuildCanonicalAuthority()
{
    CanonicalAuthority result;
    result.semantic = sem::BuildCanonicalSparsePointTransformSemanticV1();
    result.context = verify::BuildCanonicalSparseFamilyVerificationContextV1();

    for (const auto pattern : sem::SparsePatternCorpusV1())
    {
        for (const std::uint32_t cardinality : sem::QualificationCardinalitiesV1())
        {
            auto setResult = sem::BuildCanonicalSparseWorkSetV1(
                pattern, sem::SparseCardinalityV1(cardinality));
            if (!setResult)
                throw std::runtime_error(setResult.Error().message);
            auto set = std::move(setResult).Value();

            auto rawResult = planner::PlanCanonicalSparseCandidateFamilyV1(
                result.semantic, set);
            if (!rawResult) throw std::runtime_error(rawResult.Error());
            auto raw = std::move(rawResult).Value();
            Require(raw.size() == 3, "Sparse Candidate family size differs");

            std::vector<verify::VerifiedSparseFamilyCandidateV1> verified;
            for (const auto& proposal : raw)
            {
                auto value = verify::VerifySparseFamilyCandidateV1(
                    result.semantic, set, result.context, proposal);
                if (!value) throw std::runtime_error(value.Error());
                verified.push_back(std::move(value).Value());
            }
            result.runtimeAuthorities.emplace_back(
                pattern, std::move(set), std::move(verified));
        }
    }
    Require(result.runtimeAuthorities.size() == 84,
        "complete Sparse qualification corpus was not constructed");
    return result;
}

const execute::SparseSetCandidateAuthorityV1& FindAuthority(
    const CanonicalAuthority& authority,
    sem::SparsePatternKindV1 pattern,
    std::uint32_t cardinality)
{
    const auto found = std::find_if(
        authority.runtimeAuthorities.begin(), authority.runtimeAuthorities.end(),
        [&](const execute::SparseSetCandidateAuthorityV1& value)
        {
            return value.pattern == pattern &&
                value.set.Cardinality().Value() == cardinality;
        });
    if (found == authority.runtimeAuthorities.end())
        throw std::runtime_error("requested Sparse authority was not found");
    return *found;
}

void RequireQualifiedCase(
    const execute::SparseCandidateFamilyCaseResultV1& result,
    const sem::ExactSparseWorkSetV1& set)
{
    Require(result.pairwiseOutputByteIdentical,
        "Sparse Candidate pairwise output identity failed");
    Require(result.sparseSetIdentity == set.Identity(),
        "Sparse result set identity differs");
    Require(result.candidates.size() == 3,
        "Sparse Candidate count differs");

    constexpr std::array<candidate::SparseFamilyCandidateKindV1, 3> kinds{
        candidate::SparseFamilyCandidateKindV1::DenseMembershipMaskPredicate,
        candidate::SparseFamilyCandidateKindV1::CompactIndexListDispatch,
        candidate::SparseFamilyCandidateKindV1::ActiveBlockListLocalMask};
    const std::uint32_t k = set.Cardinality().Value();

    for (std::size_t index = 0; index < result.candidates.size(); ++index)
    {
        const auto& observation = result.candidates[index];
        Require(observation.kind == kinds[index],
            "Sparse Candidate ordering differs");
        Require(observation.activeCount == k &&
                observation.activeWriteCount == k,
            "Sparse exact write count differs");
        Require(observation.expectedDispatchGroupsX ==
                observation.observedDispatchGroupsX,
            "Sparse observed Dispatch differs from Verified Plan");
        Require(observation.activeReferenceQualified &&
                observation.inactiveSentinelByteStable &&
                observation.activeHomogeneousWQualified &&
                observation.finiteQualified &&
                observation.maxAbsoluteError <= 1.0e-6,
            "Sparse numeric or inactive-sentinel qualification failed");
    }

    const auto& dense = result.candidates[0];
    const auto& compact = result.candidates[1];
    const auto& blocks = result.candidates[2];
    Require(dense.expectedDispatchGroupsX == 64,
        "Dense Mask must dispatch the fixed 4096-thread universe");
    Require(compact.expectedDispatchGroupsX ==
            (k == 0 ? 0u : 1u + ((k - 1u) / 64u)),
        "Compact Index Dispatch does not equal ceil(K/64)");
    Require(blocks.expectedDispatchGroupsX == blocks.activeBlockCount,
        "Active Block Dispatch does not equal active block count");
    if (k == 0)
    {
        Require(dense.observedDispatchGroupsX == 64 &&
                compact.observedDispatchGroupsX == 0 &&
                blocks.observedDispatchGroupsX == 0,
            "zero-work Dispatch behavior differs");
    }
}

void RequireSameObservation(
    const execute::SparseCandidateFamilyCaseResultV1& before,
    const execute::SparseCandidateFamilyCaseResultV1& after)
{
    Require(before.pattern == after.pattern &&
            before.semanticIdentity == after.semanticIdentity &&
            before.sparseSetIdentity == after.sparseSetIdentity &&
            before.pairwiseOutputByteIdentical == after.pairwiseOutputByteIdentical &&
            before.candidates.size() == after.candidates.size(),
        "Sparse case identity changed across Recovery");
    for (std::size_t i = 0; i < before.candidates.size(); ++i)
    {
        const auto& a = before.candidates[i];
        const auto& b = after.candidates[i];
        Require(a.kind == b.kind &&
                a.activeCount == b.activeCount &&
                a.activeBlockCount == b.activeBlockCount &&
                a.expectedDispatchGroupsX == b.expectedDispatchGroupsX &&
                a.observedDispatchGroupsX == b.observedDispatchGroupsX &&
                a.activeWriteCount == b.activeWriteCount &&
                a.activeReferenceQualified == b.activeReferenceQualified &&
                a.inactiveSentinelByteStable == b.inactiveSentinelByteStable &&
                a.activeHomogeneousWQualified == b.activeHomogeneousWQualified &&
                a.finiteQualified == b.finiteQualified &&
                a.maxAbsoluteError == b.maxAbsoluteError &&
                a.artifactIdentity == b.artifactIdentity &&
                a.outputDigest == b.outputDigest,
            "Sparse Observation changed across Recovery");
    }
}

execute::LoadedSparseCandidateFamilyRuntimeV1 LoadRuntime(
    const CanonicalAuthority& authority)
{
    auto loaded = execute::LoadSparseCandidateFamilyRuntimeOnWarpV1(
        authority.semantic, authority.context, authority.runtimeAuthorities);
    if (!loaded)
        throw std::runtime_error(
            loaded.Error().stage + ": " + loaded.Error().message);
    return std::move(loaded).Value();
}

execute::SparseRuntimeSubmissionV1 BindBuildSubmit(
    execute::LoadedSparseCandidateFamilyRuntimeV1& runtime,
    const execute::SparseRuntimeEpochHandleV1& handle,
    const execute::SparseSetCandidateAuthorityV1& authority)
{
    auto binding = execute::BindSparseWorkSetV1(
        runtime, handle, authority.pattern, authority.set.Identity());
    if (!binding) throw std::runtime_error(binding.Error().message);
    auto representations = execute::RebuildSparseRepresentationsV1(
        runtime, handle, binding.Value());
    if (!representations)
        throw std::runtime_error(representations.Error().message);
    auto submitted = execute::SubmitSparseWorkSetV1(
        runtime, handle, binding.Value(), representations.Value());
    if (!submitted)
        throw std::runtime_error(
            submitted.Error().stage + ": " + submitted.Error().message);
    RequireQualifiedCase(submitted.Value().architecture, authority.set);
    return std::move(submitted).Value();
}

void WriteSubmission(
    base::BinaryWriter& writer,
    const execute::SparseRuntimeSubmissionV1& submission)
{
    writer.WriteU64(submission.completion.deviceEpoch);
    writer.WriteU64(submission.completion.submissionOrdinal);
    writer.WriteBytes(submission.completion.tokenIdentity);
    writer.WriteU64(submission.representations.deviceEpoch);
    writer.WriteU32(static_cast<std::uint32_t>(submission.representations.pattern));
    writer.WriteBytes(submission.representations.sparseSetIdentity);
    writer.WriteBytes(submission.representations.representationSetIdentity);
    writer.WriteU32(submission.representationResourcesMaterialized ? 1u : 0u);
    writer.WriteU32(submission.indirectArgumentsMaterialized ? 1u : 0u);
    writer.WriteU32(submission.completionStateMaterialized ? 1u : 0u);
    writer.WriteU32(submission.readbackStateMaterialized ? 1u : 0u);
    const auto architecture = execute::SerializeSparseCandidateFamilyCaseResultV1(
        submission.architecture);
    writer.WriteU32(static_cast<std::uint32_t>(architecture.size()));
    writer.WriteBytes(architecture);
}

void WriteFile(const std::filesystem::path& path, std::vector<std::byte> bytes)
{
    std::filesystem::create_directories(path.parent_path());
    auto written = base::WriteAllBytes(path, bytes);
    if (!written) throw std::runtime_error(written.Error());
}

std::vector<std::byte> RunArchitectureQualification()
{
    const auto authority = BuildCanonicalAuthority();
    auto runtime = LoadRuntime(authority);
    Require(runtime.AuthorityCount() == 84,
        "Sparse Runtime authority count differs");
    const auto handle = execute::CaptureSparseRuntimeEpochHandleV1(runtime);

    std::vector<execute::SparseRuntimeSubmissionV1> submissions;
    std::uint32_t candidateExecutions = 0;
    for (const auto& setAuthority : authority.runtimeAuthorities)
    {
        auto submission = BindBuildSubmit(runtime, handle, setAuthority);
        auto tokenValid = execute::ValidateSparseRuntimeSubmissionTokenV1(
            runtime, submission.completion);
        Require(static_cast<bool>(tokenValid),
            "current Sparse submission token was rejected");
        auto representationValid = execute::ValidateSparseRepresentationSetHandleV1(
            runtime, submission.representations);
        Require(static_cast<bool>(representationValid),
            "current Sparse representation handle was rejected");

        auto forgedToken = submission.completion;
        forgedToken.tokenIdentity[0] ^= std::byte{1};
        auto forgedTokenResult = execute::ValidateSparseRuntimeSubmissionTokenV1(
            runtime, forgedToken);
        Require(!forgedTokenResult &&
                forgedTokenResult.Error().stage == "sparse-runtime/token-identity",
            "forged current-epoch Sparse token was accepted");

        auto forgedRepresentation = submission.representations;
        forgedRepresentation.representationSetIdentity[0] ^= std::byte{1};
        auto forgedRepresentationResult = execute::ValidateSparseRepresentationSetHandleV1(
            runtime, forgedRepresentation);
        Require(!forgedRepresentationResult &&
                forgedRepresentationResult.Error().stage ==
                    "sparse-runtime/representation-identity",
            "forged current-epoch Sparse representation was accepted");

        candidateExecutions += static_cast<std::uint32_t>(
            submission.architecture.candidates.size());
        submissions.push_back(std::move(submission));
    }
    Require(candidateExecutions == 21u * 4u * 3u,
        "complete CU5 Candidate execution count differs");

    base::BinaryWriter evidence;
    constexpr std::string_view name =
        "SGE4-5.Spiral6.CU5.ArchitectureEvidence.V1";
    evidence.WriteU32(static_cast<std::uint32_t>(name.size()));
    evidence.WriteBytes(std::as_bytes(std::span(name.data(), name.size())));
    const auto runtimeAuthority =
        execute::SerializeSparseCandidateFamilyRuntimeAuthorityV1(runtime);
    evidence.WriteU32(static_cast<std::uint32_t>(runtimeAuthority.size()));
    evidence.WriteBytes(runtimeAuthority);
    evidence.WriteBytes(runtime.DomainIdentity());
    evidence.WriteBytes(runtime.CandidateBindingSetIdentity());
    evidence.WriteU64(runtime.DeviceEpoch());
    evidence.WriteU32(static_cast<std::uint32_t>(submissions.size()));
    for (const auto& submission : submissions) WriteSubmission(evidence, submission);
    evidence.WriteBytes(base::Sha256(evidence.Bytes()));

    std::cout
        << "Complete Sparse WARP qualification passed.\n"
        << "Corpus: 21 cardinalities x 4 patterns = 84 exact Sparse sets\n"
        << "Candidate executions: 84 x 3 = 252\n"
        << "Inactive sentinel and zero-work behavior: qualified\n"
        << "Architecture evidence SHA-256: "
        << base::ToHex(base::Sha256(evidence.Bytes())) << '\n';
    return std::move(evidence).Take();
}

std::vector<std::byte> RunControlledRecovery()
{
    const auto authority = BuildCanonicalAuthority();
    const auto& selected = FindAuthority(
        authority, sem::SparsePatternKindV1::HashScatterPermutation, 1024);
    auto runtime = LoadRuntime(authority);
    const auto runtimeAuthorityBefore =
        execute::SerializeSparseCandidateFamilyRuntimeAuthorityV1(runtime);
    const auto bindingSetBefore = runtime.CandidateBindingSetIdentity();
    const auto oldEpoch = runtime.DeviceEpoch();
    const auto oldHandle = execute::CaptureSparseRuntimeEpochHandleV1(runtime);
    auto before = BindBuildSubmit(runtime, oldHandle, selected);
    const auto oldToken = before.completion;
    const auto oldRepresentations = before.representations;

    auto recovered = execute::RecoverSparseCandidateFamilyRuntimeV1(
        runtime, execute::SparseCandidateFamilyRecoveryModeV1::ControlledRebuild);
    if (!recovered)
        throw std::runtime_error(
            recovered.Error().stage + ": " + recovered.Error().message);
    const auto& report = recovered.Value();
    Require(report.stateBefore == execute::SparseCandidateFamilyRuntimeStateV1::Active &&
            report.stateAfter == execute::SparseCandidateFamilyRuntimeStateV1::Active,
        "Controlled Sparse Recovery state transition differs");
    Require(report.previousDeviceEpoch == oldEpoch &&
            report.newDeviceEpoch == oldEpoch + 1 &&
            runtime.DeviceEpoch() == oldEpoch + 1,
        "Controlled Sparse Recovery epoch transition differs");
    Require(report.adapterReacquired &&
            report.nativeDeviceRematerialized &&
            report.allRuntimeObjectsReleased &&
            report.executionContextRematerialized &&
            report.representationResourcesInvalidated &&
            report.indirectArgumentsInvalidated &&
            report.completionStateInvalidated &&
            report.readbackStateInvalidated &&
            report.sparseRebindRequired &&
            report.explicitRepresentationRebuildRequired &&
            report.frozenAuthorityPreserved,
        "Controlled Sparse Recovery report is incomplete");
    Require(!runtime.SparseSetBound() && !runtime.RepresentationsBuilt(),
        "Sparse binding or representations survived Recovery");

    auto staleHandle = execute::ValidateSparseRuntimeEpochHandleV1(runtime, oldHandle);
    Require(!staleHandle && staleHandle.Error().stage == "sparse-runtime/stale-epoch",
        "old Sparse epoch handle survived Recovery");
    auto staleToken = execute::ValidateSparseRuntimeSubmissionTokenV1(runtime, oldToken);
    Require(!staleToken && staleToken.Error().stage == "sparse-runtime/stale-epoch",
        "old Sparse completion token survived Recovery");
    auto staleRepresentation = execute::ValidateSparseRepresentationSetHandleV1(
        runtime, oldRepresentations);
    Require(!staleRepresentation &&
            staleRepresentation.Error().stage == "sparse-runtime/stale-epoch",
        "old Sparse representation handle survived Recovery");

    const auto newHandle = execute::CaptureSparseRuntimeEpochHandleV1(runtime);
    auto noRebind = execute::SubmitSparseWorkSetV1(
        runtime, newHandle, {}, {});
    Require(!noRebind && noRebind.Error().stage == "sparse-runtime/rebind-required",
        "post-Recovery Sparse submission succeeded without explicit rebind");

    auto newBinding = execute::BindSparseWorkSetV1(
        runtime, newHandle, selected.pattern, selected.set.Identity());
    if (!newBinding) throw std::runtime_error(newBinding.Error().message);
    auto noRebuild = execute::SubmitSparseWorkSetV1(
        runtime, newHandle, newBinding.Value(), {});
    Require(!noRebuild && noRebuild.Error().stage == "sparse-runtime/rebuild-required",
        "post-Recovery Sparse submission succeeded without explicit representation rebuild");

    auto newRepresentations = execute::RebuildSparseRepresentationsV1(
        runtime, newHandle, newBinding.Value());
    if (!newRepresentations)
        throw std::runtime_error(newRepresentations.Error().message);
    auto after = execute::SubmitSparseWorkSetV1(
        runtime, newHandle, newBinding.Value(), newRepresentations.Value());
    if (!after) throw std::runtime_error(after.Error().message);
    RequireQualifiedCase(after.Value().architecture, selected.set);
    RequireSameObservation(before.architecture, after.Value().architecture);
    Require(runtime.CandidateBindingSetIdentity() == bindingSetBefore &&
            execute::SerializeSparseCandidateFamilyRuntimeAuthorityV1(runtime) ==
                runtimeAuthorityBefore,
        "Frozen Sparse authority changed across Recovery");

    base::BinaryWriter evidence;
    constexpr std::string_view name =
        "SGE4-5.Spiral6.CU5.ControlledRecoveryEvidence.V1";
    evidence.WriteU32(static_cast<std::uint32_t>(name.size()));
    evidence.WriteBytes(std::as_bytes(std::span(name.data(), name.size())));
    evidence.WriteBytes(bindingSetBefore);
    evidence.WriteU64(report.previousDeviceEpoch);
    evidence.WriteU64(report.newDeviceEpoch);
    evidence.WriteU32(report.adapterReacquired ? 1u : 0u);
    evidence.WriteU32(report.nativeDeviceRematerialized ? 1u : 0u);
    evidence.WriteU32(report.allRuntimeObjectsReleased ? 1u : 0u);
    evidence.WriteU32(report.executionContextRematerialized ? 1u : 0u);
    evidence.WriteU32(report.representationResourcesInvalidated ? 1u : 0u);
    evidence.WriteU32(report.indirectArgumentsInvalidated ? 1u : 0u);
    evidence.WriteU32(report.completionStateInvalidated ? 1u : 0u);
    evidence.WriteU32(report.readbackStateInvalidated ? 1u : 0u);
    evidence.WriteU32(report.sparseRebindRequired ? 1u : 0u);
    evidence.WriteU32(report.explicitRepresentationRebuildRequired ? 1u : 0u);
    WriteSubmission(evidence, before);
    WriteSubmission(evidence, after.Value());
    evidence.WriteBytes(base::Sha256(evidence.Bytes()));

    std::cout
        << "Controlled Sparse Recovery passed.\n"
        << "Device epoch: " << oldEpoch << " -> " << runtime.DeviceEpoch() << '\n'
        << "Stale Set/Representation/Completion handles: rejected\n"
        << "Explicit Sparse rebind and representation rebuild: passed\n"
        << "Controlled evidence SHA-256: "
        << base::ToHex(base::Sha256(evidence.Bytes())) << '\n';
    return std::move(evidence).Take();
}

void RunActualRemoval()
{
    const auto authority = BuildCanonicalAuthority();
    const auto& selected = FindAuthority(
        authority, sem::SparsePatternKindV1::BlockClusteredPermutation, 1024);
    auto runtime = LoadRuntime(authority);
    const auto oldEpoch = runtime.DeviceEpoch();
    const auto oldHandle = execute::CaptureSparseRuntimeEpochHandleV1(runtime);
    auto before = BindBuildSubmit(runtime, oldHandle, selected);

    auto removed = execute::RecoverSparseCandidateFamilyRuntimeV1(
        runtime, execute::SparseCandidateFamilyRecoveryModeV1::ForceRemovalForTest);
    if (!removed)
        throw std::runtime_error(
            removed.Error().stage + ": " + removed.Error().message);
    const auto& report = removed.Value();
    Require(report.forcedRemoval && report.removalReason < 0 &&
            report.stateBefore == execute::SparseCandidateFamilyRuntimeStateV1::Active &&
            report.stateAfter == execute::SparseCandidateFamilyRuntimeStateV1::AwaitingAdapter &&
            runtime.State() == execute::SparseCandidateFamilyRuntimeStateV1::AwaitingAdapter &&
            runtime.DeviceEpoch() == oldEpoch &&
            report.allRuntimeObjectsReleased &&
            report.representationResourcesInvalidated &&
            report.indirectArgumentsInvalidated &&
            report.completionStateInvalidated &&
            report.readbackStateInvalidated &&
            report.sparseRebindRequired &&
            report.explicitRepresentationRebuildRequired &&
            report.frozenAuthorityPreserved,
        "actual Sparse RemoveDevice quarantine report differs");

    auto oldHandleRejected = execute::ValidateSparseRuntimeEpochHandleV1(runtime, oldHandle);
    Require(!oldHandleRejected &&
            oldHandleRejected.Error().stage == "sparse-runtime/device-state",
        "old Sparse handle survived actual removal");
    auto oldTokenRejected = execute::ValidateSparseRuntimeSubmissionTokenV1(
        runtime, before.completion);
    Require(!oldTokenRejected &&
            oldTokenRejected.Error().stage == "sparse-runtime/device-state",
        "old Sparse token survived actual removal");
    auto oldRepresentationRejected = execute::ValidateSparseRepresentationSetHandleV1(
        runtime, before.representations);
    Require(!oldRepresentationRejected &&
            oldRepresentationRejected.Error().stage == "sparse-runtime/device-state",
        "old Sparse representation survived actual removal");

    auto retry = execute::RecoverSparseCandidateFamilyRuntimeV1(
        runtime, execute::SparseCandidateFamilyRecoveryModeV1::RetryAdapterReacquisition);
    if (!retry) throw std::runtime_error(retry.Error().message);
    Require(!retry.Value().adapterReacquired &&
            retry.Value().stateAfter == execute::SparseCandidateFamilyRuntimeStateV1::AwaitingAdapter &&
            runtime.State() == execute::SparseCandidateFamilyRuntimeStateV1::AwaitingAdapter,
        "removed WARP LUID was incorrectly reacquired");

    std::cout
        << "Actual Sparse ID3D12Device5::RemoveDevice quarantine passed.\n"
        << "State: Active -> AwaitingAdapter\n"
        << "Removed WARP LUID excluded from retry.\n"
        << "Runtime sparse-policy decision: None\n";
}

std::vector<std::byte> RunFreshRematerialization()
{
    const auto authority = BuildCanonicalAuthority();
    auto runtime = LoadRuntime(authority);
    const auto handle = execute::CaptureSparseRuntimeEpochHandleV1(runtime);
    const std::array<std::pair<sem::SparsePatternKindV1, std::uint32_t>, 6> selected{{
        {sem::SparsePatternKindV1::PrefixControl, 0},
        {sem::SparsePatternKindV1::PrefixControl, 65},
        {sem::SparsePatternKindV1::UniformStride, 65},
        {sem::SparsePatternKindV1::BlockClusteredPermutation, 65},
        {sem::SparsePatternKindV1::HashScatterPermutation, 65},
        {sem::SparsePatternKindV1::PrefixControl, 4096}}};

    std::vector<execute::SparseRuntimeSubmissionV1> submissions;
    for (const auto& [pattern, k] : selected)
    {
        const auto& setAuthority = FindAuthority(authority, pattern, k);
        submissions.push_back(BindBuildSubmit(runtime, handle, setAuthority));
    }

    base::BinaryWriter evidence;
    constexpr std::string_view name =
        "SGE4-5.Spiral6.CU5.FreshRematerializationEvidence.V1";
    evidence.WriteU32(static_cast<std::uint32_t>(name.size()));
    evidence.WriteBytes(std::as_bytes(std::span(name.data(), name.size())));
    const auto runtimeAuthority =
        execute::SerializeSparseCandidateFamilyRuntimeAuthorityV1(runtime);
    evidence.WriteU32(static_cast<std::uint32_t>(runtimeAuthority.size()));
    evidence.WriteBytes(runtimeAuthority);
    evidence.WriteU32(static_cast<std::uint32_t>(submissions.size()));
    for (const auto& submission : submissions) WriteSubmission(evidence, submission);
    evidence.WriteBytes(base::Sha256(evidence.Bytes()));

    std::cout
        << "Fresh-process Sparse rematerialization passed.\n"
        << "Representative sets: K=0, four K=65 patterns, K=4096\n"
        << "Same Frozen Sparse authority reconstructed and observed.\n"
        << "Fresh evidence SHA-256: "
        << base::ToHex(base::Sha256(evidence.Bytes())) << '\n';
    return std::move(evidence).Take();
}

void RunPortableSelfTest()
{
    const auto authority = BuildCanonicalAuthority();
    Require(authority.runtimeAuthorities.size() == 84,
        "portable authority count differs");
    std::uint32_t candidateCount = 0;
    for (const auto& setAuthority : authority.runtimeAuthorities)
    {
        Require(setAuthority.candidates.size() == 3,
            "portable Candidate count differs");
        for (const auto& verified : setAuthority.candidates)
        {
            auto valid = verify::ValidateVerifiedSparseFamilyCandidateV1(
                verified, authority.semantic, setAuthority.set, authority.context);
            Require(static_cast<bool>(valid),
                "portable Verified Candidate validation failed");
            const auto artifact = candidate::BuildSparseFamilyArtifactV1(
                authority.semantic, setAuthority.set,
                verified.Plan().operation.kind);
            auto artifactValid = candidate::ValidateSparseFamilyArtifactV1(
                artifact, authority.semantic, setAuthority.set);
            Require(static_cast<bool>(artifactValid),
                "portable Sparse artifact validation failed");
            ++candidateCount;
        }
    }
    Require(candidateCount == 252,
        "portable Candidate total differs");
    std::cout
        << "Spiral 6 CU5 portable corpus self-test passed.\n"
        << "84 exact Sparse sets and 252 Verified Candidate artifacts qualified.\n";
}
}

int main(int argc, char** argv)
{
    try
    {
        enum class Mode { Qualification, Controlled, ActualRemoval, Fresh, Portable };
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

        std::vector<std::byte> evidence;
        if (mode == Mode::Qualification) evidence = RunArchitectureQualification();
        else if (mode == Mode::Controlled) evidence = RunControlledRecovery();
        else evidence = RunFreshRematerialization();
        if (!output.empty()) WriteFile(output, std::move(evidence));
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Spiral 6 CU5 failure: " << error.what() << '\n';
        return 2;
    }
}
