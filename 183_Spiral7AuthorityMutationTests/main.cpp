#include "../00_Foundation/BinaryIO.h"
#include "../172_SparseTemporalDeltaSemantic/SparseTemporalDeltaSemantic.h"
#include "../173_Spiral7DeltaContracts/Spiral7DeltaContracts.h"
#include "../174_Spiral7DeltaExecution/Spiral7DeltaExecution.h"
#include "../175_SparseTemporalDeltaLoweringCandidate/DeltaLoweringCandidate.h"
#include "../176_SparseTemporalDeltaLoweringPlanner/DeltaLoweringPlanner.h"
#include "../177_SparseTemporalDeltaLoweringVerifier/DeltaLoweringVerifier.h"

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
namespace s6 = sge4_5::spiral6;
namespace base = sge4_5::base;

static_assert(!std::is_default_constructible_v<s7::verification::VerifiedDeltaLoweringV1>);
static_assert(!std::is_default_constructible_v<s7::execution::FrozenVerifiedCompactDeltaExecutionV1>);
static_assert(!std::is_convertible_v<
    s7::candidate::RawDeltaLoweringCandidateV1,
    s7::verification::VerifiedDeltaLoweringV1>);
static_assert(!std::is_convertible_v<
    s7::candidate::RawDeltaLoweringCandidateV1,
    s7::execution::FrozenVerifiedCompactDeltaExecutionV1>);

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
    if (!stream) throw std::runtime_error("Unable to open Spiral 7 CU3 evidence output.");
    stream.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    if (!stream) throw std::runtime_error("Unable to write Spiral 7 CU3 evidence output.");
}

s7::semantic::ExactSparseWorkSetV1 BuildPatternSet(
    s7::semantic::SparsePatternKindV1 pattern,
    std::uint32_t count)
{
    auto result = s6::semantic::BuildCanonicalSparseWorkSetV1(
        pattern, s7::semantic::SparseCardinalityV1(count));
    if (!result) throw std::runtime_error(result.Error().stage + ": " + result.Error().message);
    return std::move(result).Value();
}

s7::semantic::ExactSparseWorkSetV1 BuildExactSet(const std::vector<std::uint32_t>& indices)
{
    auto result = s6::semantic::BuildExactSparseWorkSetV1(indices);
    if (!result) throw std::runtime_error(result.Error().stage + ": " + result.Error().message);
    return std::move(result).Value();
}

struct AuthorityFixture final
{
    s7::semantic::SparseTemporalDeltaSemanticV1 semantic;
    s7::semantic::SparseDeltaInvocationV1 seedInvocation;
    s7::semantic::SparseDeltaInvocationV1 invocation;
    s7::semantic::SparseDeltaInvocationV1 alternateInvocation;
    std::vector<std::byte> previousHistoryBytes;

    AuthorityFixture(
        s7::semantic::SparseTemporalDeltaSemanticV1 semanticValue,
        s7::semantic::SparseDeltaInvocationV1 seedValue,
        s7::semantic::SparseDeltaInvocationV1 invocationValue,
        s7::semantic::SparseDeltaInvocationV1 alternateValue,
        std::vector<std::byte> historyValue)
        : semantic(std::move(semanticValue)),
          seedInvocation(std::move(seedValue)),
          invocation(std::move(invocationValue)),
          alternateInvocation(std::move(alternateValue)),
          previousHistoryBytes(std::move(historyValue)) {}
};

AuthorityFixture BuildFixture()
{
    const auto semantic = s7::semantic::BuildCanonicalSparseTemporalDeltaSemanticV1();
    const auto empty = BuildExactSet({});
    const auto previous = BuildPatternSet(
        s7::semantic::SparsePatternKindV1::PrefixControl, 1024);
    const auto initialGenerations = s7::semantic::BuildInitialInvalidGenerationsV1();
    auto seedResult = s7::semantic::BuildSparseDeltaInvocationV1(
        0, empty, previous, empty, initialGenerations, true);
    Require(static_cast<bool>(seedResult), "Seed invocation construction failed.");
    auto seed = std::move(seedResult).Value();

    const auto current = BuildPatternSet(
        s7::semantic::SparsePatternKindV1::HashScatterPermutation, 1024);
    std::vector<std::uint32_t> survivors;
    std::set_intersection(
        previous.Indices().begin(), previous.Indices().end(),
        current.Indices().begin(), current.Indices().end(),
        std::back_inserter(survivors));
    Require(survivors.size() >= 64, "Authority fixture has fewer than 64 survivors.");
    std::vector<std::uint32_t> modified(survivors.begin(), survivors.begin() + 64);
    const auto modifiedSet = BuildExactSet(modified);
    const auto noModified = BuildExactSet({});

    auto invocationResult = s7::semantic::BuildSparseDeltaInvocationV1(
        1, previous, current, modifiedSet, seed.ItemGenerations(), false);
    Require(static_cast<bool>(invocationResult), "Authority invocation construction failed.");
    auto invocation = std::move(invocationResult).Value();

    auto alternateResult = s7::semantic::BuildSparseDeltaInvocationV1(
        2, previous, current, noModified, seed.ItemGenerations(), false);
    Require(static_cast<bool>(alternateResult), "Alternate invocation construction failed.");
    auto alternate = std::move(alternateResult).Value();

    const auto previousPoints = s7::semantic::BuildCpuReferenceOutputV1(seed);
    auto previousBytes = sge4_5::spiral5::semantic::SerializePointRecordsV1(previousPoints);
    return AuthorityFixture(
        semantic, std::move(seed), std::move(invocation),
        std::move(alternate), std::move(previousBytes));
}
} // namespace

int main(int argc, char** argv)
{
    try
    {
        std::string emitPath;
        if (argc == 3 && std::string(argv[1]) == "--emit") emitPath = argv[2];
        else if (argc != 1) throw std::runtime_error("Usage: test [--emit <path>]");

        const auto fixture = BuildFixture();
        const auto artifact = s7::contract::BuildCompactDeltaArtifactV1(
            fixture.semantic, fixture.invocation);
        auto planned = s7::planner::PlanCompactDeltaHistoryCandidateV1(
            fixture.semantic, fixture.invocation);
        Require(static_cast<bool>(planned), "Delta Planner failed: " +
            (planned ? std::string{} : planned.Error()));
        const auto raw = planned.Value();
        const auto context = s7::verification::BuildCanonicalDeltaVerificationContextV1();
        auto verifiedResult = s7::verification::VerifyDeltaLoweringCandidateV1(
            fixture.semantic, fixture.invocation, context, raw);
        Require(static_cast<bool>(verifiedResult), "Independent Delta verification failed.");
        const auto verified = verifiedResult.Value();
        Require(static_cast<bool>(s7::verification::ValidateVerifiedDeltaLoweringContextV1(
            verified, fixture.semantic, fixture.invocation, context)),
            "Verified Delta context validation failed.");

        std::uint32_t rawAttackCount = 0;
        auto reject = [&](s7::candidate::RawDeltaLoweringCandidateV1 proposal,
                          const char* name,
                          bool rehash = true)
        {
            if (rehash)
                proposal.rawCandidateIdentity =
                    s7::candidate::RawDeltaLoweringCandidateIdentityV1(proposal);
            const auto result = s7::verification::VerifyDeltaLoweringCandidateV1(
                fixture.semantic, fixture.invocation, context, proposal);
            Require(!result, std::string("Mutation accepted: ") + name);
            ++rawAttackCount;
        };

        { auto p=raw; Flip(p.semanticIdentity); reject(p,"semantic identity"); }
        { auto p=raw; Flip(p.invocationIdentity); reject(p,"invocation identity"); }
        { auto p=raw; Flip(p.previousActiveSetIdentity); reject(p,"previous Active identity"); }
        { auto p=raw; Flip(p.activeSetIdentity); reject(p,"Active identity"); }
        { auto p=raw; Flip(p.modifiedSetIdentity); reject(p,"Modified identity"); }
        { auto p=raw; Flip(p.transitionSetIdentity); reject(p,"Transition set identity"); }
        { auto p=raw; Flip(p.transitionRecordsIdentity); reject(p,"Transition records identity"); }
        { auto p=raw; Flip(p.targetProfileIdentity); reject(p,"Target identity"); }
        { auto p=raw; Flip(p.observationContractIdentity); reject(p,"Observation identity"); }
        { auto p=raw; Flip(p.deltaResourceContractIdentity); reject(p,"Delta Resource contract"); }
        { auto p=raw; Flip(p.previousHistoryResourceContractIdentity); reject(p,"previous History contract"); }
        { auto p=raw; Flip(p.outputHistoryResourceContractIdentity); reject(p,"output History contract"); }
        { auto p=raw; Flip(p.completionContractIdentity); reject(p,"Completion identity"); }
        { auto p=raw; Flip(p.deviceEpochPolicyIdentity); reject(p,"epoch policy identity"); }
        { auto p=raw; Flip(p.spiral4IndirectIdentity); reject(p,"Spiral 4 identity"); }
        { auto p=raw; Flip(p.spiral5TemporalIdentity); reject(p,"Spiral 5 identity"); }
        { auto p=raw; Flip(p.spiral6SparseIdentity); reject(p,"Spiral 6 identity"); }
        { auto p=raw; Flip(p.consumerProgramIdentity); reject(p,"Consumer identity"); }
        { auto p=raw; Flip(p.proposedArtifactIdentity); reject(p,"artifact identity"); }
        { auto p=raw; Flip(p.proposedDeltaResourceIdentity); reject(p,"Delta Resource identity"); }
        { auto p=raw; Flip(p.proposedPreviousHistoryResourceIdentity); reject(p,"previous History identity"); }
        { auto p=raw; Flip(p.proposedOutputHistoryResourceIdentity); reject(p,"output History identity"); }
        { auto p=raw; Flip(p.transitionAuthorityIdentity); reject(p,"Transition authority"); }
        { auto p=raw; p.kind=static_cast<s7::candidate::DeltaLoweringKindV1>(99); reject(p,"Candidate kind"); }
        { auto p=raw; p.representationRole=static_cast<s7::contract::DeltaRepresentationRoleV1>(99); reject(p,"representation role"); }
        { auto p=raw; p.operationCode=static_cast<s7::contract::DeltaOperationCodeV1>(99); reject(p,"operation code"); }
        { auto p=raw; p.outputHistoryPolicy=static_cast<s7::contract::DeltaOutputHistoryPolicyV1>(99); reject(p,"History policy"); }
        { auto p=raw; p.completionPolicy=static_cast<s7::candidate::DeltaCompletionPolicyV1>(99); reject(p,"completion policy"); }
        { auto p=raw; p.deviceEpochPolicy=static_cast<s7::candidate::DeltaDeviceEpochPolicyV1>(99); reject(p,"device epoch policy"); }
        { auto p=raw; ++p.invocationIndex; reject(p,"invocation index"); }
        { auto p=raw; ++p.universeCount; reject(p,"universe count"); }
        { auto p=raw; ++p.transitionCount; reject(p,"Transition count"); }
        { auto p=raw; ++p.updateCount; reject(p,"Update count"); }
        { auto p=raw; ++p.clearCount; reject(p,"Clear count"); }
        { auto p=raw; ++p.retainCount; reject(p,"Retain count"); }
        { auto p=raw; ++p.threadsPerGroup; reject(p,"threads per group"); }
        { auto p=raw; ++p.dispatchGroupsX; reject(p,"Dispatch groups"); }
        { auto p=raw; ++p.recordStrideBytes; reject(p,"record stride"); }
        { auto p=raw; p.fixedCapacityBytes-=8; reject(p,"fixed capacity"); }
        { auto p=raw; p.historyBytes-=16; reject(p,"History bytes"); }
        { auto p=raw; --p.outputCapacity; reject(p,"output capacity"); }
        { auto p=raw; ++p.outputStrideBytes; reject(p,"output stride"); }
        { auto p=raw; p.zeroTransitionMapsToZeroDispatch=0; reject(p,"zero Transition rule"); }
        { auto p=raw; Flip(p.rawCandidateIdentity); reject(p,"raw identity",false); }
        { auto p=raw; p.transitionCount=0; reject(p,"zero Transition rehash"); }
        { auto p=raw; p.dispatchGroupsX=0; reject(p,"zero Dispatch rehash"); }
        { auto p=raw; p.historyBytes-=16; reject(p,"short History rehash"); }
        { auto p=raw; p.outputCapacity=4095; reject(p,"short output rehash"); }
        { auto p=raw; p.fixedCapacityBytes+=8; reject(p,"oversized Delta Resource rehash"); }
        { auto p=raw; ++p.invocationIndex; reject(p,"cross invocation rehash"); }
        Require(rawAttackCount == 50, "Raw Delta mutation count is not 50.");

        Require(!s7::verification::ValidateVerifiedDeltaLoweringContextV1(
            verified, fixture.semantic, fixture.alternateInvocation, context),
            "Cross-invocation Verified seal replay accepted.");
        auto badTarget = context;
        Flip(badTarget.targetProfileIdentity);
        Require(!s7::verification::ValidateVerifiedDeltaLoweringContextV1(
            verified, fixture.semantic, fixture.invocation, badTarget),
            "Cross-target Verified seal replay accepted.");
        auto badObservation = context;
        Flip(badObservation.observationContractIdentity);
        Require(!s7::verification::ValidateVerifiedDeltaLoweringContextV1(
            verified, fixture.semantic, fixture.invocation, badObservation),
            "Cross-observation Verified seal replay accepted.");
        auto badHistoryContract = context;
        Flip(badHistoryContract.previousHistoryResourceContractIdentity);
        Require(!s7::verification::ValidateVerifiedDeltaLoweringContextV1(
            verified, fixture.semantic, fixture.invocation, badHistoryContract),
            "Cross-History-contract Verified seal replay accepted.");

        s7::execution::DeltaResourceBindingInputV1 binding;
        binding.deviceEpoch = 7;
        binding.actualDeltaResourceIdentity = verified.Plan().operation.deltaResourceIdentity;
        binding.actualPreviousHistoryResourceIdentity =
            verified.Plan().operation.previousHistoryResourceIdentity;
        binding.actualOutputHistoryResourceIdentity =
            verified.Plan().operation.outputHistoryResourceIdentity;
        binding.deltaFixedCapacityBytes = verified.Plan().operation.fixedCapacityBytes;
        binding.deltaRecordStrideBytes = verified.Plan().operation.recordStrideBytes;
        binding.previousHistoryBytes = verified.Plan().operation.historyBytes;
        binding.outputHistoryBytes = verified.Plan().operation.historyBytes;

        auto wrong = binding;
        Flip(wrong.actualDeltaResourceIdentity);
        Require(!s7::execution::FreezeVerifiedCompactDeltaExecutionV1(
            verified, fixture.semantic, fixture.invocation, artifact, wrong),
            "Wrong Delta Resource identity accepted.");
        wrong = binding;
        Flip(wrong.actualPreviousHistoryResourceIdentity);
        Require(!s7::execution::FreezeVerifiedCompactDeltaExecutionV1(
            verified, fixture.semantic, fixture.invocation, artifact, wrong),
            "Wrong previous History identity accepted.");
        wrong = binding;
        Flip(wrong.actualOutputHistoryResourceIdentity);
        Require(!s7::execution::FreezeVerifiedCompactDeltaExecutionV1(
            verified, fixture.semantic, fixture.invocation, artifact, wrong),
            "Wrong output History identity accepted.");
        wrong = binding;
        wrong.deltaRole = static_cast<s7::execution::DeltaBindingRoleV1>(99);
        Require(!s7::execution::FreezeVerifiedCompactDeltaExecutionV1(
            verified, fixture.semantic, fixture.invocation, artifact, wrong),
            "Wrong Delta role accepted.");
        wrong = binding;
        wrong.previousHistoryRole = s7::execution::DeltaBindingRoleV1::OutputHistoryWriteOnly;
        Require(!s7::execution::FreezeVerifiedCompactDeltaExecutionV1(
            verified, fixture.semantic, fixture.invocation, artifact, wrong),
            "Writable previous History role accepted.");
        wrong = binding;
        wrong.outputHistoryRole = s7::execution::DeltaBindingRoleV1::PreviousHistoryReadOnly;
        Require(!s7::execution::FreezeVerifiedCompactDeltaExecutionV1(
            verified, fixture.semantic, fixture.invocation, artifact, wrong),
            "Read-only output History role accepted.");
        wrong = binding;
        ++wrong.deltaFixedCapacityBytes;
        Require(!s7::execution::FreezeVerifiedCompactDeltaExecutionV1(
            verified, fixture.semantic, fixture.invocation, artifact, wrong),
            "Wrong Delta bytes accepted.");
        wrong = binding;
        ++wrong.deltaRecordStrideBytes;
        Require(!s7::execution::FreezeVerifiedCompactDeltaExecutionV1(
            verified, fixture.semantic, fixture.invocation, artifact, wrong),
            "Wrong Delta stride accepted.");
        wrong = binding;
        wrong.previousHistoryBytes -= 16;
        Require(!s7::execution::FreezeVerifiedCompactDeltaExecutionV1(
            verified, fixture.semantic, fixture.invocation, artifact, wrong),
            "Short previous History accepted.");
        wrong = binding;
        wrong.outputHistoryBytes -= 16;
        Require(!s7::execution::FreezeVerifiedCompactDeltaExecutionV1(
            verified, fixture.semantic, fixture.invocation, artifact, wrong),
            "Short output History accepted.");
        wrong = binding;
        wrong.deviceEpoch = 0;
        Require(!s7::execution::FreezeVerifiedCompactDeltaExecutionV1(
            verified, fixture.semantic, fixture.invocation, artifact, wrong),
            "Zero device epoch accepted.");
        wrong = binding;
        wrong.actualOutputHistoryResourceIdentity = wrong.actualPreviousHistoryResourceIdentity;
        Require(!s7::execution::FreezeVerifiedCompactDeltaExecutionV1(
            verified, fixture.semantic, fixture.invocation, artifact, wrong),
            "Aliased previous/output History identity accepted.");

        const auto alternateArtifact = s7::contract::BuildCompactDeltaArtifactV1(
            fixture.semantic, fixture.alternateInvocation);
        Require(!s7::execution::FreezeVerifiedCompactDeltaExecutionV1(
            verified, fixture.semantic, fixture.invocation, alternateArtifact, binding),
            "Cross-invocation artifact accepted.");

        auto frozenResult = s7::execution::FreezeVerifiedCompactDeltaExecutionV1(
            verified, fixture.semantic, fixture.invocation, artifact, binding);
        Require(static_cast<bool>(frozenResult), "Canonical Compact Delta freeze failed.");
        const auto frozen = frozenResult.Value();
        Require(static_cast<bool>(s7::execution::ValidateFrozenCompactDeltaExecutionEpochV1(
            frozen, 7)), "Canonical Delta epoch validation failed.");
        Require(!s7::execution::ValidateFrozenCompactDeltaExecutionEpochV1(
            frozen, 8), "Cross-device-epoch Frozen replay accepted.");

        std::cout << "Spiral 7 CU3 authority and replay gates passed before WARP.\n";
        auto run = s7::execution::ExecuteVerifiedCompactDeltaHistoryV1(
            fixture.semantic, fixture.invocation, frozen,
            fixture.previousHistoryBytes, 7);
        if (!run)
        {
#if !defined(_WIN32)
            std::cerr << "Verified CU3 WARP failure [" << run.Error().stage << "]: "
                      << run.Error().message << '\n';
            return 2;
#else
            throw std::runtime_error("Verified Delta WARP execution failed: " +
                run.Error().stage + ": " + run.Error().message);
#endif
        }
        Require(run.Value().cases.size() == 1, "Verified Delta report case count mismatch.");
        Require(run.Value().cases.front().updateAndClearQualified,
            "Verified Delta Transition audit failed.");
        Require(run.Value().cases.front().retainedHistoryByteStable,
            "Verified Delta retained History changed.");

        base::BinaryWriter evidence;
        constexpr std::string_view domain =
            "SGE4-5.Spiral7.CU3.IndependentDeltaAuthorityEvidence.V1";
        evidence.WriteBytes(base::Sha256(std::as_bytes(
            std::span(domain.data(), domain.size()))));
        const auto rawBytes = s7::candidate::SerializeRawDeltaLoweringCandidateV1(raw);
        const auto verifiedBytes = s7::verification::SerializeVerifiedDeltaLoweringV1(verified);
        const auto frozenBytes =
            s7::execution::SerializeFrozenVerifiedCompactDeltaExecutionV1(frozen);
        const auto reportBytes =
            s7::execution::SerializeCompactDeltaArchitectureReportV1(run.Value());
        for (const auto& bytes : {rawBytes, verifiedBytes, frozenBytes, reportBytes})
        {
            evidence.WriteU32(static_cast<std::uint32_t>(bytes.size()));
            evidence.WriteBytes(bytes);
        }
        evidence.WriteU32(rawAttackCount);
        evidence.WriteU32(4);  // verified context/invocation replay gates
        evidence.WriteU32(14); // resource/artifact/epoch replay gates
        const auto digest = base::Sha256(evidence.Bytes());
        evidence.WriteBytes(digest);
        auto finalEvidence = std::move(evidence).Take();
        if (!emitPath.empty()) WriteFile(emitPath, finalEvidence);

        std::cout << "Spiral 7 CU3 independent Delta authority passed.\n";
        std::cout << "Invocation: Prefix1024 -> HashScatter1024 with 64 modified survivors\n";
        std::cout << "Mutations: 50 Raw attacks plus invocation/context/resource/artifact/epoch replay gates rejected\n";
        std::cout << "Verified WARP execution: Compact Delta History, device epoch 7\n";
        std::cout << "Runtime transition-policy decision: None\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Spiral 7 CU3 authority test failed: " << error.what() << '\n';
        return 1;
    }
}
