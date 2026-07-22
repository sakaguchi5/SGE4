#include "../00_Foundation/BinaryIO.h"
#include "../00_Foundation/FileIO.h"
#include "../00_Foundation/Sha256.h"
#include "../134_TemporalStateSemantic/TemporalStateSemantic.h"
#include "../135_Spiral5TemporalContracts/Spiral5TemporalContracts.h"
#include "../136_Spiral5TemporalExecution/Spiral5TemporalExecution.h"
#include "../137_TemporalLoweringCandidate/TemporalLoweringCandidate.h"
#include "../138_TemporalLoweringPlanner/TemporalLoweringPlanner.h"
#include "../139_TemporalLoweringVerifier/TemporalLoweringVerifier.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace base = sge4_5::base;
namespace semantic = sge4_5::spiral5::semantic;
namespace contract = sge4_5::spiral5::contract;
namespace execution = sge4_5::spiral5::execution;
namespace candidate = sge4_5::spiral5::candidate;
namespace planner = sge4_5::spiral5::planner;
namespace verification = sge4_5::spiral5::verification;

static_assert(!std::is_aggregate_v<verification::VerifiedTemporalLoweringV1>);
static_assert(!std::is_default_constructible_v<verification::VerifiedTemporalLoweringV1>);
static_assert(!std::is_convertible_v<
    candidate::RawTemporalLoweringCandidateV1,
    verification::VerifiedTemporalLoweringV1>);
static_assert(!std::is_default_constructible_v<
    execution::FrozenVerifiedGlobalMotorHistoryExecutionV1>);
static_assert(!std::is_convertible_v<
    candidate::RawTemporalLoweringCandidateV1,
    execution::FrozenVerifiedGlobalMotorHistoryExecutionV1>);
static_assert(!std::is_convertible_v<candidate::TemporalLoweringKindV1, std::uint32_t>);
static_assert(!std::is_convertible_v<candidate::HistoryGenerationPolicyV1, std::uint32_t>);

namespace
{
void Require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

void Toggle(base::Digest256& value)
{
    value[0] ^= std::byte{0x01};
}

void WriteAuthorityBundle(
    const std::filesystem::path& path,
    const semantic::TemporalStateSemanticV1& semanticValue,
    const semantic::UpdateScheduleV1& schedule,
    const candidate::RawTemporalLoweringCandidateV1& raw,
    const verification::VerifiedTemporalLoweringV1& verified,
    const execution::FrozenVerifiedGlobalMotorHistoryExecutionV1& frozen)
{
    const auto semanticBytes = semantic::SerializeTemporalStateSemanticV1(semanticValue);
    const auto scheduleBytes = semantic::SerializeUpdateScheduleV1(schedule);
    const auto rawBytes = candidate::SerializeRawTemporalLoweringCandidateV1(raw);
    const auto verifiedBytes = verification::SerializeVerifiedTemporalLoweringV1(verified);
    const auto frozenBytes = execution::SerializeFrozenVerifiedGlobalMotorHistoryExecutionV1(frozen);

    base::BinaryWriter writer;
    writer.WriteU32(0x33413553u); // "S5A3"
    writer.WriteU32(1);
    writer.WriteU32(static_cast<std::uint32_t>(semanticBytes.size()));
    writer.WriteBytes(semanticBytes);
    writer.WriteU32(static_cast<std::uint32_t>(scheduleBytes.size()));
    writer.WriteBytes(scheduleBytes);
    writer.WriteU32(static_cast<std::uint32_t>(rawBytes.size()));
    writer.WriteBytes(rawBytes);
    writer.WriteBytes(raw.rawCandidateIdentity);
    writer.WriteU32(static_cast<std::uint32_t>(verifiedBytes.size()));
    writer.WriteBytes(verifiedBytes);
    writer.WriteU32(static_cast<std::uint32_t>(frozenBytes.size()));
    writer.WriteBytes(frozenBytes);
    writer.WriteBytes(base::Sha256(writer.Bytes()));

    std::filesystem::create_directories(path.parent_path());
    const auto bytes = std::move(writer).Take();
    auto written = base::WriteAllBytes(path, bytes);
    if (!written) throw std::runtime_error(written.Error());
}
}

int main(int argc, char** argv)
{
    try
    {
        const auto semanticValue = semantic::BuildCanonicalTemporalStateSemanticV1();
        auto p4Result = semantic::BuildPeriodicUpdateScheduleV1(4);
        auto p64Result = semantic::BuildPeriodicUpdateScheduleV1(64);
        Require(p4Result && p64Result, "canonical schedule construction failed");
        const auto p4 = p4Result.Value();
        const auto p64 = p64Result.Value();
        const auto context = verification::BuildCanonicalTemporalVerificationContextV1();

        auto rawResult = planner::PlanGlobalMotorHistoryReuseCandidateV1(
            semanticValue, p4);
        Require(static_cast<bool>(rawResult), "canonical Temporal Planner proposal failed");
        const auto raw = rawResult.Value();

        auto verifiedResult = verification::VerifyTemporalLoweringCandidateV1(
            semanticValue, p4, context, raw);
        Require(static_cast<bool>(verifiedResult),
                "canonical Temporal proposal failed independent verification");
        const auto verified = verifiedResult.Value();

        std::uint32_t attempted = 0;
        std::uint32_t rejected = 0;
        auto reject = [&](candidate::RawTemporalLoweringCandidateV1 proposal, bool rehash)
        {
            ++attempted;
            if (rehash)
                proposal.rawCandidateIdentity =
                    candidate::RawTemporalLoweringCandidateIdentityV1(proposal);
            if (!verification::VerifyTemporalLoweringCandidateV1(
                    semanticValue, p4, context, proposal))
                ++rejected;
        };

        auto proposal = raw; Toggle(proposal.semanticIdentity); reject(proposal, false);
        proposal = raw; Toggle(proposal.scheduleIdentity); reject(proposal, false);
        proposal = raw; Toggle(proposal.targetProfileIdentity); reject(proposal, false);
        proposal = raw; Toggle(proposal.observationContractIdentity); reject(proposal, false);
        proposal = raw; Toggle(proposal.historyResourceContractIdentity); reject(proposal, false);
        proposal = raw; Toggle(proposal.retainedCompletionContractIdentity); reject(proposal, false);
        proposal = raw; Toggle(proposal.deviceEpochPolicyIdentity); reject(proposal, false);
        proposal = raw; Toggle(proposal.spiral4IndirectArtifactIdentity); reject(proposal, false);
        proposal = raw; Toggle(proposal.argumentProducerProgramIdentity); reject(proposal, false);
        proposal = raw; Toggle(proposal.hierarchyProgramIdentity); reject(proposal, false);
        proposal = raw; Toggle(proposal.consumerProgramIdentity); reject(proposal, false);
        proposal = raw; Toggle(proposal.proposedArtifactIdentity); reject(proposal, false);
        proposal = raw; Toggle(proposal.proposedHistoryResourceIdentity); reject(proposal, false);
        proposal = raw; Toggle(proposal.invocationAuthorityIdentity); reject(proposal, false);
        proposal = raw; proposal.kind = static_cast<candidate::TemporalLoweringKindV1>(0); reject(proposal, false);
        proposal = raw; proposal.extensionStrategy = static_cast<contract::TemporalExtensionStrategyV1>(0); reject(proposal, false);
        proposal = raw; proposal.historyRole = static_cast<contract::TemporalHistoryRoleV1>(0); reject(proposal, false);
        proposal = raw; proposal.completionPolicy = static_cast<contract::RetainedHistoryCompletionV1>(0); reject(proposal, false);
        proposal = raw; proposal.generationPolicy = static_cast<candidate::HistoryGenerationPolicyV1>(0); reject(proposal, false);
        proposal = raw; proposal.holdPolicy = static_cast<candidate::HoldExecutionPolicyV1>(0); reject(proposal, false);
        proposal = raw; ++proposal.timelineLength; reject(proposal, false);
        proposal = raw; ++proposal.workCount; reject(proposal, false);
        proposal = raw; ++proposal.threadsPerGroup; reject(proposal, false);
        proposal = raw; ++proposal.historyDepth; reject(proposal, false);
        proposal = raw; ++proposal.historyBytes; reject(proposal, false);
        proposal = raw; ++proposal.localMotorBytesPerGeneration; reject(proposal, false);
        proposal = raw; ++proposal.argumentStrideBytes; reject(proposal, false);
        proposal = raw; ++proposal.maxCommandCount; reject(proposal, false);
        proposal = raw; ++proposal.argumentProducerDispatchX; reject(proposal, false);
        proposal = raw; ++proposal.hierarchyUpdateDispatchX; reject(proposal, false);
        proposal = raw; ++proposal.hierarchyHoldDispatchX; reject(proposal, false);
        proposal = raw; ++proposal.consumerDispatchX; reject(proposal, false);
        proposal = raw; ++proposal.updateCount; reject(proposal, false);
        proposal = raw; ++proposal.holdCount; reject(proposal, false);
        proposal = raw; ++proposal.maximumSourceGeneration; reject(proposal, false);
        proposal = raw; Toggle(proposal.rawCandidateIdentity); reject(proposal, false);
        proposal = raw; proposal.historyRole = static_cast<contract::TemporalHistoryRoleV1>(2); reject(proposal, true);
        proposal = raw; ++proposal.updateCount; reject(proposal, true);
        proposal = raw; Toggle(proposal.invocationAuthorityIdentity); reject(proposal, true);
        proposal = raw; Toggle(proposal.proposedArtifactIdentity); reject(proposal, true);

        Require(attempted == 40 && rejected == attempted,
                "one or more Temporal authority mutations were accepted");

        Require(!verification::ValidateVerifiedTemporalLoweringContextV1(
                    verified, semanticValue, p64, context),
                "cross-schedule Verified seal replay was accepted");

        auto replayContext = context;
        Toggle(replayContext.targetProfileIdentity);
        Require(!verification::ValidateVerifiedTemporalLoweringContextV1(
                    verified, semanticValue, p4, replayContext),
                "cross-target Verified seal replay was accepted");
        replayContext = context;
        Toggle(replayContext.historyResourceContractIdentity);
        Require(!verification::ValidateVerifiedTemporalLoweringContextV1(
                    verified, semanticValue, p4, replayContext),
                "cross-History-contract Verified seal replay was accepted");
        replayContext = context;
        Toggle(replayContext.deviceEpochPolicyIdentity);
        Require(!verification::ValidateVerifiedTemporalLoweringContextV1(
                    verified, semanticValue, p4, replayContext),
                "cross-epoch-policy Verified seal replay was accepted");

        const auto cu2Artifact = contract::BuildCu2GlobalMotorHistoryArtifactV1(
            semanticValue, p4);
        auto binding = execution::BuildCanonicalGlobalMotorHistoryResourceBindingInputV1(
            cu2Artifact, 1);
        auto frozenResult = execution::FreezeVerifiedGlobalMotorHistoryExecutionV1(
            semanticValue, p4, context, verified, binding);
        Require(static_cast<bool>(frozenResult),
                "verified Temporal plan failed actual artifact and Resource binding");
        const auto frozen = frozenResult.Value();

        auto badBinding = binding;
        Toggle(badBinding.actualHistoryResourceIdentity);
        Require(!execution::FreezeVerifiedGlobalMotorHistoryExecutionV1(
                    semanticValue, p4, context, verified, badBinding),
                "wrong actual History Resource identity was accepted");
        badBinding = binding;
        badBinding.historyRole = static_cast<contract::TemporalHistoryRoleV1>(2);
        Require(!execution::FreezeVerifiedGlobalMotorHistoryExecutionV1(
                    semanticValue, p4, context, verified, badBinding),
                "wrong History Role binding was accepted");
        badBinding = binding;
        ++badBinding.historyBytes;
        Require(!execution::FreezeVerifiedGlobalMotorHistoryExecutionV1(
                    semanticValue, p4, context, verified, badBinding),
                "wrong History byte size was accepted");
        badBinding = binding;
        badBinding.deviceEpoch = 0;
        Require(!execution::FreezeVerifiedGlobalMotorHistoryExecutionV1(
                    semanticValue, p4, context, verified, badBinding),
                "zero device epoch History binding was accepted");

        Require(!execution::ValidateFrozenVerifiedGlobalMotorHistoryExecutionV1(
                    frozen, semanticValue, p4, context, 2),
                "cross-device-epoch Frozen History replay was accepted");
        Require(!execution::ValidateFrozenVerifiedGlobalMotorHistoryExecutionV1(
                    frozen, semanticValue, p64, context, 1),
                "cross-schedule Frozen History replay was accepted");

        auto run = execution::RunVerifiedGlobalMotorHistoryOnWarpV1(
            frozen, semanticValue, p4, context, 1);
        if (!run)
        {
            std::cerr << "Verified CU3 WARP failure [" << run.Error().stage
                      << "]: " << run.Error().message << '\n';
            return 2;
        }
        Require(run.Value().architecture.schedules.size() == 1,
                "verified Temporal WARP schedule count mismatch");
        Require(run.Value().architecture.schedules[0].invocations.size() ==
                    semantic::TimelineLengthV1,
                "verified Temporal WARP invocation count mismatch");
        Require(run.Value().architecture.argumentProducerShaderDigest != base::Digest256{},
                "argument-producer shader digest missing");
        Require(run.Value().architecture.hierarchyShaderDigest != base::Digest256{},
                "hierarchy shader digest missing");
        Require(run.Value().architecture.consumerShaderDigest != base::Digest256{},
                "consumer shader digest missing");
        Require(run.Value().actualExecutionIdentity != base::Digest256{},
                "actual verified Temporal execution identity missing");

        if (argc == 3 && std::string(argv[1]) == "--emit")
        {
            WriteAuthorityBundle(argv[2], semanticValue, p4, raw, verified, frozen);
        }
        else if (argc != 1)
        {
            std::cerr << "Usage: 146_Spiral5AuthorityMutationTests [--emit path]\n";
            return 3;
        }

        std::cout << "Spiral 5 CU3 authority mutation tests rejected " << rejected
                  << " invalid proposals; compile-time Raw/Verified boundaries passed.\n"
                  << "Verified schedule: P4, 128 invocations, device epoch 1\n"
                  << "Verified plan SHA-256: " << base::ToHex(verified.Plan().planIdentity) << '\n'
                  << "Verifier certificate SHA-256: " << base::ToHex(verified.CertificateIdentity()) << '\n'
                  << "Frozen History binding SHA-256: " << base::ToHex(frozen.BindingIdentity()) << '\n'
                  << "Actual execution SHA-256: " << base::ToHex(run.Value().actualExecutionIdentity) << '\n'
                  << "Runtime temporal-policy decision: None\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Spiral 5 CU3 test failure: " << error.what() << '\n';
        return 1;
    }
}
