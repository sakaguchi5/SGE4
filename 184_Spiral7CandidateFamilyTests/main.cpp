#include "../00_Foundation/BinaryIO.h"
#include "../172_SparseTemporalDeltaSemantic/SparseTemporalDeltaSemantic.h"
#include "../178_SparseTemporalDeltaCandidateFamily/DeltaCandidateFamily.h"
#include "../179_SparseTemporalDeltaCandidateFamilyPlanner/DeltaCandidateFamilyPlanner.h"
#include "../180_SparseTemporalDeltaCandidateFamilyVerifier/DeltaCandidateFamilyVerifier.h"
#include "../181_Spiral7DeltaFamilyExecution/Spiral7DeltaFamilyExecution.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
namespace s7 = sge4_5::spiral7;
namespace base = sge4_5::base;
namespace s6 = sge4_5::spiral6;

static_assert(!std::is_default_constructible_v<
    s7::family_verification::VerifiedDeltaFamilyCandidateV1>);
static_assert(!std::is_convertible_v<
    s7::family_candidate::RawDeltaFamilyCandidateV1,
    s7::family_verification::VerifiedDeltaFamilyCandidateV1>);

void Require(bool condition, const std::string& message)
{
    if (!condition) throw std::runtime_error(message);
}

void Flip(base::Digest256& digest)
{
    digest[0] ^= std::byte{0x80};
}

void WriteFile(const std::string& path, std::span<const std::byte> bytes)
{
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) throw std::runtime_error("Unable to open CU4 evidence output.");
    stream.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    if (!stream) throw std::runtime_error("Unable to write CU4 evidence output.");
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

struct BuiltCorpus final
{
    std::vector<s7::family_execution::VerifiedDeltaFamilyCaseV1> cases;
    std::uint32_t mutationCount = 0;
};

BuiltCorpus BuildCorpus(const s7::semantic::SparseTemporalDeltaSemanticV1& semanticValue)
{
    namespace fc = s7::family_candidate;
    namespace fp = s7::family_planner;
    namespace fv = s7::family_verification;
    namespace fe = s7::family_execution;

    BuiltCorpus corpus;
    auto previousActive = BuildExact({});
    auto generations = s7::semantic::BuildInitialInvalidGenerationsV1();
    const auto context = fv::BuildCanonicalDeltaFamilyVerificationContextV1();
    std::vector<fv::VerifiedDeltaFamilyCandidateV1> firstVerified;

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

        auto plannedResult = fp::PlanCanonicalDeltaCandidateFamilyV1(
            semanticValue, invocation);
        Require(static_cast<bool>(plannedResult),
                "Delta Candidate Family Planner failed.");
        auto planned = std::move(plannedResult).Value();
        Require(planned.size() == 3, "Planner did not return exactly A/B/C.");

        std::vector<fc::FrozenDeltaFamilyArtifactV1> artifacts;
        std::vector<fv::VerifiedDeltaFamilyCandidateV1> verified;
        for (const auto& proposal : planned)
        {
            auto verifiedResult = fv::VerifyDeltaFamilyCandidateV1(
                semanticValue, invocation, context, proposal);
            Require(static_cast<bool>(verifiedResult),
                    "Independent Delta Family verification failed.");
            verified.push_back(std::move(verifiedResult).Value());
            artifacts.push_back(fc::BuildDeltaFamilyArtifactV1(
                semanticValue, invocation, proposal.kind));
        }

        if (corpus.cases.empty())
        {
            auto reject = [&](fc::RawDeltaFamilyCandidateV1 proposal,
                              const char* name,
                              bool rehash = true)
            {
                if (rehash)
                    proposal.rawCandidateIdentity =
                        fc::RawDeltaFamilyCandidateIdentityV1(proposal);
                auto result = fv::VerifyDeltaFamilyCandidateV1(
                    semanticValue, invocation, context, proposal);
                Require(!result, std::string("Delta Family mutation accepted: ") + name);
                ++corpus.mutationCount;
            };
            for (const auto& proposal : planned)
            {
                { auto p=proposal; Flip(p.proposedArtifactIdentity); reject(p,"artifact identity"); }
                { auto p=proposal; Flip(p.writeSetAuthorityIdentity); reject(p,"write-set authority"); }
                { auto p=proposal; ++p.dispatchGroupsX; reject(p,"dispatch groups"); }
                { auto p=proposal; p.legalWriteSet=fc::DeltaFamilyLegalWriteSetV1::EntireUniverse;
                  if (proposal.kind == fc::DeltaFamilyCandidateKindV1::FullActiveDenseRecompute)
                      p.legalWriteSet=fc::DeltaFamilyLegalWriteSetV1::ExactlyTransitionSet;
                  reject(p,"legal write set"); }
                { auto p=proposal; Flip(p.representationResourceContractIdentity);
                  reject(p,"representation resource contract"); }
            }
            { auto p=planned[1]; Flip(p.rawCandidateIdentity); reject(p,"raw identity",false); }
            Require(corpus.mutationCount == 16, "CU4 Raw mutation count is not 16.");
            firstVerified = verified;
        }

        generations = invocation.ItemGenerations();
        previousActive = active;
        corpus.cases.emplace_back(
            std::move(id), invocation, std::move(artifacts), std::move(verified));
    };

    add("InitialPrefix64", BuildSet(s7::semantic::SparsePatternKindV1::PrefixControl, 64), 0);
    add("HoldPrefix64", BuildSet(s7::semantic::SparsePatternKindV1::PrefixControl, 64), 0);
    add("DirtyOnePrefix64", BuildSet(s7::semantic::SparsePatternKindV1::PrefixControl, 64), 1);
    add("Dirty32Prefix64", BuildSet(s7::semantic::SparsePatternKindV1::PrefixControl, 64), 32);
    add("ActivatePrefix65", BuildSet(s7::semantic::SparsePatternKindV1::PrefixControl, 65), 0);
    add("DeactivatePrefix32", BuildSet(s7::semantic::SparsePatternKindV1::PrefixControl, 32), 0);
    add("BalancedChurnUniform32", BuildSet(s7::semantic::SparsePatternKindV1::UniformStride, 32), 0);
    add("PatternMigrationHash32", BuildSet(s7::semantic::SparsePatternKindV1::HashScatterPermutation, 32), 0);
    add("ActivateBlock256", BuildSet(s7::semantic::SparsePatternKindV1::BlockClusteredPermutation, 256), 0);
    add("HoldBlock256", BuildSet(s7::semantic::SparsePatternKindV1::BlockClusteredPermutation, 256), 0);
    add("Dirty64Block256", BuildSet(s7::semantic::SparsePatternKindV1::BlockClusteredPermutation, 256), 64);
    add("ExpandPrefix1024", BuildSet(s7::semantic::SparsePatternKindV1::PrefixControl, 1024), 0);
    add("MigrateHash1024", BuildSet(s7::semantic::SparsePatternKindV1::HashScatterPermutation, 1024), 64);
    add("HoldHash1024", BuildSet(s7::semantic::SparsePatternKindV1::HashScatterPermutation, 1024), 0);
    add("EmptyReset", BuildExact({}), 0);
    add("FullActivate4096", BuildSet(s7::semantic::SparsePatternKindV1::PrefixControl, 4096), 0);
    add("Dirty1024Full", BuildSet(s7::semantic::SparsePatternKindV1::PrefixControl, 4096), 1024);
    add("DeactivateHash4095", BuildSet(s7::semantic::SparsePatternKindV1::HashScatterPermutation, 4095), 0);

    Require(corpus.cases.size() == 18, "CU4 architecture case count is not 18.");
    Require(!fv::ValidateVerifiedDeltaFamilyCandidateV1(
        firstVerified[0], semanticValue, corpus.cases[1].invocation, context),
        "Cross-invocation Delta Family Verified replay accepted.");
    return corpus;
}
} // namespace

int main(int argc, char** argv)
{
    try
    {
        std::string emitPath;
        if (argc == 3 && std::string(argv[1]) == "--emit") emitPath = argv[2];
        else if (argc != 1)
            throw std::runtime_error("Usage: 184_Spiral7CandidateFamilyTests [--emit <path>]");

        const auto semanticValue =
            s7::semantic::BuildCanonicalSparseTemporalDeltaSemanticV1();
        auto corpus = BuildCorpus(semanticValue);
        auto report = s7::family_execution::ExecuteVerifiedDeltaCandidateFamilyV1(
            semanticValue, corpus.cases);
        if (!report)
        {
#if !defined(_WIN32)
            std::cerr << "CU4 WARP failure [" << report.Error().stage << "]: "
                      << report.Error().message << '\n';
            return 2;
#else
            throw std::runtime_error(report.Error().stage + ": " + report.Error().message);
#endif
        }
        Require(report.Value().invocations.size() == corpus.cases.size(),
                "CU4 report invocation count mismatch.");
        for (const auto& invocation : report.Value().invocations)
        {
            Require(invocation.exactCandidateOrder, "A/B/C Candidate order mismatch.");
            Require(invocation.pairwiseOutputByteIdentical,
                    "A/B/C full outputs are not byte-identical.");
            Require(invocation.candidates.size() == 3,
                    "A/B/C observation count mismatch.");
            for (const auto& candidate : invocation.candidates)
            {
                Require(candidate.activeReferenceQualified, "Active reference failed.");
                Require(candidate.inactiveSentinelByteStable, "Inactive sentinel failed.");
                Require(candidate.retainedHistoryByteStable, "Retained history bytes changed.");
                Require(candidate.exactWriteSetQualified, "Candidate legal write set failed.");
                Require(candidate.observedWriteCount == candidate.legalWriteCount,
                        "Candidate observed write count mismatch.");
            }
        }

        base::BinaryWriter evidence;
        constexpr std::string_view domain =
            "SGE4-5.Spiral7.CU4.DeltaCandidateFamilyEvidence.V1";
        evidence.WriteBytes(base::Sha256(std::as_bytes(
            std::span(domain.data(), domain.size()))));
        evidence.WriteU32(static_cast<std::uint32_t>(corpus.cases.size()));
        for (const auto& testCase : corpus.cases)
        {
            evidence.WriteBytes(testCase.invocation.Identity());
            for (std::size_t index = 0; index < testCase.artifacts.size(); ++index)
            {
                const auto& artifactBytes = testCase.artifacts[index].Bytes();
                evidence.WriteU32(static_cast<std::uint32_t>(artifactBytes.size()));
                evidence.WriteBytes(artifactBytes);
                const auto verifiedBytes =
                    s7::family_verification::SerializeVerifiedDeltaFamilyCandidateV1(
                        testCase.verifiedCandidates[index]);
                evidence.WriteU32(static_cast<std::uint32_t>(verifiedBytes.size()));
                evidence.WriteBytes(verifiedBytes);
            }
        }
        const auto reportBytes =
            s7::family_execution::SerializeDeltaFamilyArchitectureReportV1(report.Value());
        evidence.WriteU32(static_cast<std::uint32_t>(reportBytes.size()));
        evidence.WriteBytes(reportBytes);
        evidence.WriteU32(corpus.mutationCount);
        const auto digest = base::Sha256(evidence.Bytes());
        evidence.WriteBytes(digest);
        auto finalEvidence = std::move(evidence).Take();
        if (!emitPath.empty()) WriteFile(emitPath, finalEvidence);

        std::cout << "Spiral 7 CU4 Incremental History Candidate Family passed.\n";
        std::cout << "Adapter: " << report.Value().adapterDescription << "\n";
        std::cout << "Timeline invocations: " << report.Value().invocations.size() << "\n";
        std::cout << "Candidates: A=FullActiveDenseRecompute, B=CompactDeltaIndexHistoryReuse, C=AffectedBlockDeltaHistoryReuse\n";
        std::cout << "Pairwise full-output bytes: identical after every invocation\n";
        std::cout << "Write authority: A=entire universe; B/C=exactly T_t; block masks reconstruct exact W_t/R_t\n";
        std::cout << "Runtime Candidate-policy decision: None\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Spiral 7 CU4 failure: " << error.what() << '\n';
        return 1;
    }
}
