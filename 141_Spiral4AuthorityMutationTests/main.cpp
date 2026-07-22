#include "../00_Foundation/BinaryIO.h"
#include "../00_Foundation/FileIO.h"
#include "../00_Foundation/Sha256.h"
#include "../120_ActiveWorkSemantic/ActiveWorkSemantic.h"
#include "../123_ActiveWorkLoweringCandidate/ActiveWorkLoweringCandidate.h"
#include "../124_ActiveWorkLoweringPlanner/ActiveWorkLoweringPlanner.h"
#include "../125_ActiveWorkLoweringVerifier/ActiveWorkLoweringVerifier.h"
#include "../126_Spiral4VerifiedExecution/Spiral4VerifiedExecution.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace base = sge4_5::base;
namespace s4sem = sge4_5::spiral4::semantic;
namespace s4candidate = sge4_5::spiral4::candidate;
namespace s4planner = sge4_5::spiral4::planner;
namespace s4verify = sge4_5::spiral4::verification;
namespace s4frozen = sge4_5::spiral4::verified_execution;

static_assert(
    !std::is_aggregate_v<
        s4verify::VerifiedActiveWorkLoweringV1>);
static_assert(
    !std::is_default_constructible_v<
        s4verify::VerifiedActiveWorkLoweringV1>);
static_assert(
    !std::is_constructible_v<
        s4verify::VerifiedActiveWorkLoweringV1,
        s4verify::VerifiedActiveWorkExecutionPlanV1,
        base::Digest256>);
static_assert(
    !std::is_convertible_v<
        s4candidate::RawActiveWorkLoweringCandidateV1,
        s4verify::VerifiedActiveWorkLoweringV1>);
static_assert(
    !std::is_default_constructible_v<
        s4frozen::FrozenVerifiedSingleIndirectExecutionV1>);
static_assert(
    !std::is_convertible_v<
        s4candidate::RawActiveWorkLoweringCandidateV1,
        s4frozen::FrozenVerifiedSingleIndirectExecutionV1>);
static_assert(
    !std::is_convertible_v<
        s4candidate::ActiveWorkLoweringKindV1,
        std::uint32_t>);
static_assert(
    !std::is_convertible_v<
        s4candidate::ActiveRangePolicyV1,
        std::uint32_t>);

namespace
{
void Require(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

void Toggle(base::Digest256& value)
{
    value[0] ^= std::byte{0x01};
}

void WriteAuthorityBundle(
    const std::filesystem::path& path,
    const s4sem::ActiveWorkSemanticV1& semantic,
    const s4candidate::RawActiveWorkLoweringCandidateV1& raw,
    const s4verify::VerifiedActiveWorkLoweringV1& verified,
    const s4frozen::FrozenVerifiedSingleIndirectExecutionV1& frozen)
{
    const auto semanticBytes =
        s4sem::SerializeActiveWorkSemanticV1(semantic);
    const auto rawBytes =
        s4candidate::SerializeRawActiveWorkLoweringCandidateV1(raw);
    const auto verifiedBytes =
        s4verify::SerializeVerifiedActiveWorkLoweringV1(verified);
    const auto frozenBytes =
        s4frozen::SerializeFrozenVerifiedSingleIndirectExecutionV1(
            frozen);

    base::BinaryWriter writer;
    writer.WriteU32(0x33433453u); // "S4C3"
    writer.WriteU32(1);

    writer.WriteU32(
        static_cast<std::uint32_t>(semanticBytes.size()));
    writer.WriteBytes(semanticBytes);

    writer.WriteU32(static_cast<std::uint32_t>(rawBytes.size()));
    writer.WriteBytes(rawBytes);
    writer.WriteBytes(raw.rawCandidateIdentity);

    writer.WriteU32(
        static_cast<std::uint32_t>(verifiedBytes.size()));
    writer.WriteBytes(verifiedBytes);

    writer.WriteU32(
        static_cast<std::uint32_t>(frozenBytes.size()));
    writer.WriteBytes(frozenBytes);

    const auto digest = base::Sha256(writer.Bytes());
    writer.WriteBytes(digest);

    std::filesystem::create_directories(path.parent_path());
    const auto bytes = std::move(writer).Take();
    auto result = base::WriteAllBytes(path, bytes);
    if (!result)
    {
        throw std::runtime_error(result.Error());
    }
}
}

int main(int argc, char** argv)
{
    try
    {
        const auto semantic =
            s4sem::BuildCanonicalActiveWorkSemanticV1();
        const auto context =
            s4verify::BuildCanonicalActiveWorkVerificationContextV1();

        auto rawResult =
            s4planner::PlanSingleExecuteIndirectCandidateV1(
                semantic);
        Require(
            static_cast<bool>(rawResult),
            "canonical Planner proposal failed");
        const auto raw = rawResult.Value();

        auto verifiedResult =
            s4verify::VerifyActiveWorkLoweringCandidateV1(
                semantic,
                context,
                raw);
        Require(
            static_cast<bool>(verifiedResult),
            "canonical proposal failed independent verification");
        const auto verified = verifiedResult.Value();

        std::uint32_t rejected = 0;
        std::uint32_t attempted = 0;
        auto reject = [&](
            s4candidate::RawActiveWorkLoweringCandidateV1 proposal,
            bool rehash)
        {
            ++attempted;
            if (rehash)
            {
                proposal.rawCandidateIdentity =
                    s4candidate::
                        RawActiveWorkLoweringCandidateIdentityV1(
                            proposal);
            }

            if (!s4verify::VerifyActiveWorkLoweringCandidateV1(
                    semantic,
                    context,
                    proposal))
            {
                ++rejected;
            }
        };

        auto proposal = raw;
        Toggle(proposal.semanticIdentity);
        reject(proposal, false);

        proposal = raw;
        Toggle(proposal.targetProfileIdentity);
        reject(proposal, false);

        proposal = raw;
        Toggle(proposal.activeCountContractIdentity);
        reject(proposal, false);

        proposal = raw;
        Toggle(proposal.observationContractIdentity);
        reject(proposal, false);

        proposal = raw;
        Toggle(proposal.commandSignatureContractIdentity);
        reject(proposal, false);

        proposal = raw;
        Toggle(proposal.producerProgramTemplateIdentity);
        reject(proposal, false);

        proposal = raw;
        Toggle(proposal.consumerProgramTemplateIdentity);
        reject(proposal, false);

        proposal = raw;
        Toggle(proposal.proposedArtifactIdentity);
        reject(proposal, false);

        proposal = raw;
        proposal.kind =
            static_cast<
                s4candidate::ActiveWorkLoweringKindV1>(0);
        reject(proposal, false);

        proposal = raw;
        ++proposal.maxWorkCount;
        reject(proposal, false);

        proposal = raw;
        ++proposal.threadsPerGroup;
        reject(proposal, false);

        proposal = raw;
        ++proposal.workRecordStride;
        reject(proposal, false);

        proposal = raw;
        ++proposal.outputRecordStride;
        reject(proposal, false);

        proposal = raw;
        ++proposal.argumentStrideBytes;
        reject(proposal, false);

        proposal = raw;
        ++proposal.argumentOffsetBytes;
        reject(proposal, false);

        proposal = raw;
        ++proposal.maxCommandCount;
        reject(proposal, false);

        proposal = raw;
        ++proposal.producerDispatchX;
        reject(proposal, false);

        proposal = raw;
        ++proposal.producerDispatchY;
        reject(proposal, false);

        proposal = raw;
        ++proposal.producerDispatchZ;
        reject(proposal, false);

        proposal = raw;
        proposal.commandKind =
            static_cast<
                sge4_5::spiral4::contract::
                    IndirectCommandKindV1>(0);
        reject(proposal, false);

        proposal = raw;
        proposal.operationCode =
            static_cast<
                sge4_5::spiral4::contract::
                    IndirectOperationCodeV1>(0);
        reject(proposal, false);

        proposal = raw;
        proposal.activeRangePolicy =
            static_cast<
                s4candidate::ActiveRangePolicyV1>(0);
        reject(proposal, false);

        proposal = raw;
        proposal.statePolicy =
            static_cast<
                s4candidate::IndirectStatePolicyV1>(0);
        reject(proposal, false);

        proposal = raw;
        proposal.zeroWorkPolicy =
            static_cast<
                s4candidate::ZeroWorkPolicyV1>(0);
        reject(proposal, false);

        proposal = raw;
        Toggle(proposal.rawCandidateIdentity);
        reject(proposal, false);

        proposal = raw;
        ++proposal.maxWorkCount;
        reject(proposal, true);

        proposal = raw;
        ++proposal.argumentStrideBytes;
        reject(proposal, true);

        Require(
            rejected == attempted && attempted == 27,
            "one or more authority mutations were accepted");

        auto replayContext = context;
        Toggle(replayContext.targetProfileIdentity);
        Require(
            !s4verify::
                ValidateVerifiedActiveWorkLoweringContextV1(
                    verified,
                    semantic,
                    replayContext),
            "cross-target Verified seal replay was accepted");

        replayContext = context;
        Toggle(replayContext.commandSignatureContractIdentity);
        Require(
            !s4verify::
                ValidateVerifiedActiveWorkLoweringContextV1(
                    verified,
                    semantic,
                    replayContext),
            "cross-command-signature seal replay was accepted");

        auto frozenResult =
            s4frozen::FreezeVerifiedSingleIndirectExecutionV1(
                semantic,
                context,
                verified);
        Require(
            static_cast<bool>(frozenResult),
            "verified plan failed actual artifact binding");
        const auto frozen = frozenResult.Value();

        replayContext = context;
        Toggle(replayContext.observationContractIdentity);
        Require(
            !s4frozen::FreezeVerifiedSingleIndirectExecutionV1(
                semantic,
                replayContext,
                verified),
            "cross-observation Freeze replay was accepted");

        constexpr std::array<std::uint32_t, 3> cases{
            0u,
            65u,
            4096u
        };

        auto run =
            s4frozen::RunVerifiedSingleIndirectOnWarpV1(
                frozen,
                semantic,
                context,
                cases);
        if (!run)
        {
            std::cerr
                << "Verified CU3 WARP failure ["
                << run.Error().stage << "]: "
                << run.Error().message << '\n';
            return 2;
        }

        Require(
            run.Value().architecture.cases.size() ==
                cases.size(),
            "verified WARP case count mismatch");
        Require(
            run.Value().architecture.
                producerShaderBytecodeDigest !=
                    base::Digest256{},
            "producer shader bytecode digest missing");
        Require(
            run.Value().architecture.
                consumerShaderBytecodeDigest !=
                    base::Digest256{},
            "consumer shader bytecode digest missing");
        Require(
            run.Value().actualExecutionIdentity !=
                base::Digest256{},
            "actual execution identity missing");

        if (argc == 3 && std::string(argv[1]) == "--emit")
        {
            WriteAuthorityBundle(
                argv[2],
                semantic,
                raw,
                verified,
                frozen);
        }
        else if (argc != 1)
        {
            std::cerr
                << "Usage: 141_Spiral4AuthorityMutationTests "
                   "[--emit path]\n";
            return 3;
        }

        std::cout
            << "Spiral 4 CU3 authority mutation tests rejected "
            << rejected
            << " invalid proposals; compile-time Raw/Verified "
               "boundaries passed.\n"
            << "Verified ExecuteIndirect WARP cases: 0, 65, 4096\n"
            << "Verified plan SHA-256: "
            << base::ToHex(verified.Plan().planIdentity)
            << '\n'
            << "Verifier certificate SHA-256: "
            << base::ToHex(verified.CertificateIdentity())
            << '\n'
            << "Frozen binding SHA-256: "
            << base::ToHex(frozen.BindingIdentity())
            << '\n'
            << "Actual execution SHA-256: "
            << base::ToHex(run.Value().actualExecutionIdentity)
            << '\n'
            << "Runtime dispatch-dimension decision: None\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "Spiral 4 CU3 test failure: "
            << error.what() << '\n';
        return 1;
    }
}
