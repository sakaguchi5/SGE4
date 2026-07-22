#include "../00_Foundation/BinaryIO.h"
#include "../00_Foundation/FileIO.h"
#include "../00_Foundation/Sha256.h"
#include "../120_ActiveWorkSemantic/ActiveWorkSemantic.h"
#include "../127_Spiral4CandidateFamilyContracts/CandidateFamilyContracts.h"
#include "../128_ActiveWorkCandidateFamily/ActiveWorkCandidateFamily.h"
#include "../129_ActiveWorkCandidateFamilyPlanner/ActiveWorkCandidateFamilyPlanner.h"
#include "../130_ActiveWorkCandidateFamilyVerifier/ActiveWorkCandidateFamilyVerifier.h"
#include "../131_Spiral4CandidateFamilyExecution/CandidateFamilyExecution.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace base = sge4_5::base;
namespace sem = sge4_5::spiral4::semantic;
namespace contract = sge4_5::spiral4::family_contract;
namespace candidate = sge4_5::spiral4::family_candidate;
namespace planner = sge4_5::spiral4::family_planner;
namespace verify = sge4_5::spiral4::family_verification;
namespace execute = sge4_5::spiral4::family_execution;

static_assert(!std::is_aggregate_v<verify::VerifiedCandidateFamilyV1>);
static_assert(!std::is_default_constructible_v<verify::VerifiedCandidateFamilyV1>);
static_assert(!std::is_convertible_v<candidate::RawCandidateFamilyV1, verify::VerifiedCandidateFamilyV1>);
static_assert(!std::is_default_constructible_v<execute::FrozenVerifiedCandidateFamilyV1>);
static_assert(!std::is_convertible_v<candidate::RawCandidateFamilyV1, execute::FrozenVerifiedCandidateFamilyV1>);
static_assert(!std::is_convertible_v<contract::CandidateFamilyKindV1, std::uint32_t>);

namespace
{
void Require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

void Toggle(base::Digest256& digest)
{
    digest[0] ^= std::byte{0x01};
}

std::string CandidateName(
    contract::CandidateFamilyKindV1 kind,
    std::uint32_t batchSize)
{
    switch (kind)
    {
    case contract::CandidateFamilyKindV1::FixedMaximumGuarded:
        return "A.FixedMaximumGuarded";
    case contract::CandidateFamilyKindV1::SingleExecuteIndirectDispatch:
        return "B.SingleExecuteIndirectDispatch";
    case contract::CandidateFamilyKindV1::BatchedExecuteIndirectDispatch:
        return "C.BatchedExecuteIndirect.B" + std::to_string(batchSize);
    default:
        return "Unknown";
    }
}

void WriteBundle(
    const std::filesystem::path& path,
    const sem::ActiveWorkSemanticV1& semantic,
    const std::vector<candidate::RawCandidateFamilyV1>& raw,
    const std::vector<verify::VerifiedCandidateFamilyV1>& verified,
    const std::vector<execute::FrozenVerifiedCandidateFamilyV1>& frozen)
{
    base::BinaryWriter writer;
    writer.WriteU32(0x34433453u);
    writer.WriteU32(1);
    const auto semanticBytes = sem::SerializeActiveWorkSemanticV1(semantic);
    writer.WriteU32(static_cast<std::uint32_t>(semanticBytes.size()));
    writer.WriteBytes(semanticBytes);
    writer.WriteU32(static_cast<std::uint32_t>(raw.size()));

    for (std::size_t i = 0; i < raw.size(); ++i)
    {
        const auto rawBytes = candidate::SerializeRawCandidateFamilyV1(raw[i]);
        const auto verifiedBytes = verify::SerializeVerifiedCandidateFamilyV1(verified[i]);
        const auto frozenBytes = execute::SerializeFrozenVerifiedCandidateFamilyV1(frozen[i]);
        writer.WriteU32(static_cast<std::uint32_t>(rawBytes.size()));
        writer.WriteBytes(rawBytes);
        writer.WriteBytes(raw[i].rawCandidateIdentity);
        writer.WriteU32(static_cast<std::uint32_t>(verifiedBytes.size()));
        writer.WriteBytes(verifiedBytes);
        writer.WriteU32(static_cast<std::uint32_t>(frozenBytes.size()));
        writer.WriteBytes(frozenBytes);
    }

    const auto digest = base::Sha256(writer.Bytes());
    writer.WriteBytes(digest);
    const auto bytes = std::move(writer).Take();
    std::filesystem::create_directories(path.parent_path());
    auto result = base::WriteAllBytes(path, bytes);
    if (!result) throw std::runtime_error(result.Error());
}
}

int main(int argc, char** argv)
{
    try
    {
        const auto semantic = sem::BuildCanonicalActiveWorkSemanticV1();
        const auto context =
            verify::BuildCanonicalCandidateFamilyVerificationContextV1();

        auto rawResult = planner::PlanCanonicalCandidateFamilyCorpusV1(semantic);
        Require(static_cast<bool>(rawResult), "Candidate family Planner failed.");
        const auto raw = rawResult.Value();
        Require(raw.size() == 7, "Candidate family corpus must contain seven candidates.");

        std::vector<verify::VerifiedCandidateFamilyV1> verified;
        std::vector<execute::FrozenVerifiedCandidateFamilyV1> frozen;
        verified.reserve(raw.size());
        frozen.reserve(raw.size());

        for (const auto& proposal : raw)
        {
            auto verifiedResult =
                verify::VerifyCandidateFamilyV1(semantic, context, proposal);
            Require(static_cast<bool>(verifiedResult),
                    "Canonical family proposal failed verification.");
            verified.push_back(verifiedResult.Value());

            auto frozenResult =
                execute::FreezeVerifiedCandidateFamilyV1(
                    semantic, context, verified.back());
            Require(static_cast<bool>(frozenResult),
                    "Verified family failed Freeze.");
            frozen.push_back(frozenResult.Value());
        }

        std::uint32_t attempted = 0;
        std::uint32_t rejected = 0;
        auto reject = [&](candidate::RawCandidateFamilyV1 proposal, bool rehash)
        {
            ++attempted;
            if (rehash)
            {
                proposal.rawCandidateIdentity =
                    candidate::RawCandidateFamilyIdentityV1(proposal);
            }
            if (!verify::VerifyCandidateFamilyV1(
                    semantic, context, proposal))
            {
                ++rejected;
            }
        };

        const auto canonicalBatch = raw[4];
        auto proposal = canonicalBatch; Toggle(proposal.semanticIdentity); reject(proposal, false);
        proposal = canonicalBatch; Toggle(proposal.targetProfileIdentity); reject(proposal, false);
        proposal = canonicalBatch; Toggle(proposal.activeCountContractIdentity); reject(proposal, false);
        proposal = canonicalBatch; Toggle(proposal.observationContractIdentity); reject(proposal, false);
        proposal = canonicalBatch; Toggle(proposal.commandSignatureContractIdentity); reject(proposal, false);
        proposal = canonicalBatch; Toggle(proposal.producerProgramTemplateIdentity); reject(proposal, false);
        proposal = canonicalBatch; Toggle(proposal.consumerProgramTemplateIdentity); reject(proposal, false);
        proposal = canonicalBatch; Toggle(proposal.proposedArtifactIdentity); reject(proposal, false);
        proposal = canonicalBatch; proposal.kind = contract::CandidateFamilyKindV1::FixedMaximumGuarded; reject(proposal, false);
        proposal = canonicalBatch; proposal.commandSignaturePolicy = contract::CommandSignaturePolicyV1::DispatchOnly; reject(proposal, false);
        proposal = canonicalBatch; proposal.issuancePolicy = contract::IssuancePolicyV1::SingleGpuGeneratedIndirect; reject(proposal, false);
        proposal = canonicalBatch; ++proposal.batchSize; reject(proposal, false);
        proposal = canonicalBatch; ++proposal.maxBatchSlots; reject(proposal, false);
        proposal = canonicalBatch; ++proposal.maxWorkCount; reject(proposal, false);
        proposal = canonicalBatch; ++proposal.threadsPerGroup; reject(proposal, false);
        proposal = canonicalBatch; ++proposal.directDispatchX; reject(proposal, false);
        proposal = canonicalBatch; ++proposal.argumentStrideBytes; reject(proposal, false);
        proposal = canonicalBatch; ++proposal.argumentBufferBytes; reject(proposal, false);
        proposal = canonicalBatch; ++proposal.maxCommandCount; reject(proposal, false);
        proposal = canonicalBatch; ++proposal.producerDispatchX; reject(proposal, false);
        proposal = canonicalBatch; Toggle(proposal.rawCandidateIdentity); reject(proposal, false);
        proposal = canonicalBatch; ++proposal.batchSize; reject(proposal, true);
        proposal = canonicalBatch; ++proposal.argumentStrideBytes; reject(proposal, true);
        proposal = canonicalBatch; ++proposal.maxBatchSlots; reject(proposal, true);

        Require(attempted == 24 && rejected == attempted,
                "Candidate family mutation gate failed.");

        for (std::size_t i = 2; i < verified.size(); ++i)
        {
            for (const std::uint32_t active : sem::ActiveCountCorpusV1())
            {
                auto records =
                    verify::DeriveVerifiedBatchDispatchRecordsV1(
                        verified[i],
                        sem::ActiveWorkCountV1(
                            active, semantic.maxWorkCount));
                Require(static_cast<bool>(records),
                        "Verified Batch partition derivation failed.");
                auto valid = contract::ValidateBatchPartitionV1(
                    sem::ActiveWorkCountV1(
                        active, semantic.maxWorkCount),
                    semantic.threadsPerGroup,
                    contract::BatchSizeV1(
                        verified[i].Plan().batchSize),
                    records.Value());
                Require(static_cast<bool>(valid),
                        "Verified Batch partition coverage failed.");
            }
        }

        auto invalidBatch = planner::PlanCandidateFamilyV1(
            semantic,
            contract::CandidateFamilyKindV1::BatchedExecuteIndirectDispatch,
            192);
        Require(!invalidBatch, "Unqualified Batch size was accepted.");

        auto replayContext = context;
        Toggle(replayContext.targetProfileIdentity);
        Require(
            !verify::ValidateVerifiedCandidateFamilyContextV1(
                verified.front(), semantic, replayContext),
            "Cross-target Candidate family replay was accepted.");

        constexpr std::array<std::uint32_t, 3> architectureCounts{
            0u, 65u, 4096u
        };
        auto runs = execute::RunVerifiedCandidateFamilyCorpusOnWarpV1(
            semantic, context, frozen, architectureCounts);
        if (!runs)
        {
            std::cerr << "CU4 WARP failure ["
                      << runs.Error().stage << "]: "
                      << runs.Error().message << '\n';
            return 2;
        }
        Require(runs.Value().size() == 7,
                "CU4 run corpus size mismatch.");

        for (std::size_t caseIndex = 0;
             caseIndex < architectureCounts.size();
             ++caseIndex)
        {
            const auto expectedDigest =
                runs.Value().front().cases[caseIndex].outputDigest;
            for (const auto& run : runs.Value())
            {
                Require(
                    run.cases[caseIndex].outputDigest == expectedDigest,
                    "Candidate outputs are not byte-identical.");
            }
        }

        for (const auto& run : runs.Value())
        {
            std::cout
                << "[PASS] " << CandidateName(run.kind, run.batchSize)
                << " cases=0/65/4096"
                << " binding=" << base::ToHex(run.verifiedBindingIdentity)
                << " actual=" << base::ToHex(run.actualExecutionIdentity)
                << '\n';
        }

        if (argc == 3 && std::string(argv[1]) == "--emit")
        {
            WriteBundle(argv[2], semantic, raw, verified, frozen);
        }
        else if (argc != 1)
        {
            std::cerr
                << "Usage: 142_Spiral4CandidateFamilyTests "
                   "[--emit path]\n";
            return 3;
        }

        std::cout
            << "Spiral 4 CU4 Candidate family passed.\n"
            << "Candidates: A + B + C(B64/B128/B256/B512/B1024) = 7\n"
            << "Mutation rejections: " << rejected << '\n'
            << "Verified Batch partitions: 5 sizes x 19 Active Counts = 95\n"
            << "WARP equivalence cases: 7 candidates x 3 Active Counts = 21\n"
            << "Runtime dispatch-dimension decision: None\n";
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr
            << "Spiral 4 CU4 test failure: "
            << exception.what() << '\n';
        return 1;
    }
}
