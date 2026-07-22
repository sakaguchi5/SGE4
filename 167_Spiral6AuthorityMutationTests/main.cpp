#include "../00_Foundation/BinaryIO.h"
#include "../154_SparseWorkSetSemantic/SparseWorkSetSemantic.h"
#include "../155_Spiral6SparseContracts/Spiral6SparseContracts.h"
#include "../156_Spiral6SparseExecution/Spiral6SparseExecution.h"
#include "../157_SparseLoweringCandidate/SparseLoweringCandidate.h"
#include "../158_SparseLoweringPlanner/SparseLoweringPlanner.h"
#include "../159_SparseLoweringVerifier/SparseLoweringVerifier.h"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
namespace s6 = sge4_5::spiral6;
namespace base = sge4_5::base;

static_assert(!std::is_default_constructible_v<s6::verification::VerifiedSparseLoweringV1>);
static_assert(!std::is_default_constructible_v<s6::execution::FrozenVerifiedCompactIndexExecutionV1>);
static_assert(!std::is_convertible_v<
    s6::candidate::RawSparseLoweringCandidateV1,
    s6::verification::VerifiedSparseLoweringV1>);
static_assert(!std::is_convertible_v<
    s6::candidate::RawSparseLoweringCandidateV1,
    s6::execution::FrozenVerifiedCompactIndexExecutionV1>);

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
    if (!stream) throw std::runtime_error("Unable to open CU3 evidence output.");
    stream.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    if (!stream) throw std::runtime_error("Unable to write CU3 evidence output.");
}
}

int main(int argc, char** argv)
{
    try
    {
        std::string emitPath;
        if (argc == 3 && std::string(argv[1]) == "--emit") emitPath = argv[2];
        else if (argc != 1) throw std::runtime_error("Usage: test [--emit <path>]");

        const auto semantic = s6::semantic::BuildCanonicalSparsePointTransformSemanticV1();
        auto setResult = s6::semantic::BuildCanonicalSparseWorkSetV1(
            s6::semantic::SparsePatternKindV1::HashScatterPermutation,
            s6::semantic::SparseCardinalityV1(1024));
        Require(static_cast<bool>(setResult), "Canonical K=1024 HashScatter set failed.");
        const auto set = std::move(setResult).Value();
        const auto artifact = s6::contract::BuildCompactIndexListArtifactV1(semantic, set);

        auto planned = s6::planner::PlanCompactIndexListCandidateV1(semantic, set);
        Require(static_cast<bool>(planned), "Sparse Planner failed: " +
            (planned ? std::string{} : planned.Error()));
        const auto raw = planned.Value();
        const auto context = s6::verification::BuildCanonicalSparseVerificationContextV1();
        auto verifiedResult = s6::verification::VerifySparseLoweringCandidateV1(
            semantic, set, context, raw);
        Require(static_cast<bool>(verifiedResult), "Independent Sparse verification failed.");
        const auto verified = verifiedResult.Value();
        Require(static_cast<bool>(s6::verification::ValidateVerifiedSparseLoweringContextV1(
            verified, semantic, set, context)), "Verified Sparse context validation failed.");

        std::uint32_t rawAttackCount = 0;
        auto reject = [&](s6::candidate::RawSparseLoweringCandidateV1 proposal,
                          const char* name,
                          bool rehash = true)
        {
            if (rehash)
                proposal.rawCandidateIdentity =
                    s6::candidate::RawSparseLoweringCandidateIdentityV1(proposal);
            const auto result = s6::verification::VerifySparseLoweringCandidateV1(
                semantic, set, context, proposal);
            Require(!result, std::string("Mutation accepted: ") + name);
            ++rawAttackCount;
        };

        { auto p=raw; Flip(p.semanticIdentity); reject(p,"semantic identity"); }
        { auto p=raw; Flip(p.sparseSetIdentity); reject(p,"Sparse set identity"); }
        { auto p=raw; Flip(p.targetProfileIdentity); reject(p,"Target identity"); }
        { auto p=raw; Flip(p.observationContractIdentity); reject(p,"Observation identity"); }
        { auto p=raw; Flip(p.indexResourceContractIdentity); reject(p,"Resource contract identity"); }
        { auto p=raw; Flip(p.completionContractIdentity); reject(p,"Completion identity"); }
        { auto p=raw; Flip(p.deviceEpochPolicyIdentity); reject(p,"epoch policy identity"); }
        { auto p=raw; Flip(p.spiral4IndirectArtifactIdentity); reject(p,"Spiral 4 identity"); }
        { auto p=raw; Flip(p.consumerProgramIdentity); reject(p,"Consumer identity"); }
        { auto p=raw; Flip(p.proposedArtifactIdentity); reject(p,"artifact identity"); }
        { auto p=raw; Flip(p.proposedIndexResourceIdentity); reject(p,"Resource identity"); }
        { auto p=raw; Flip(p.writeSetAuthorityIdentity); reject(p,"write-set authority"); }
        { auto p=raw; p.kind=static_cast<s6::candidate::SparseLoweringKindV1>(99); reject(p,"Candidate kind"); }
        { auto p=raw; p.representationRole=static_cast<s6::contract::SparseRepresentationRoleV1>(99); reject(p,"representation role"); }
        { auto p=raw; p.operationCode=static_cast<s6::contract::SparseOperationCodeV1>(99); reject(p,"operation code"); }
        { auto p=raw; p.outputInitialization=static_cast<s6::contract::SparseOutputInitializationV1>(99); reject(p,"output initialization"); }
        { auto p=raw; p.writeSetPolicy=static_cast<s6::contract::SparseWriteSetPolicyV1>(99); reject(p,"write-set policy"); }
        { auto p=raw; p.completionPolicy=static_cast<s6::candidate::SparseCompletionPolicyV1>(99); reject(p,"completion policy"); }
        { auto p=raw; p.deviceEpochPolicy=static_cast<s6::candidate::SparseDeviceEpochPolicyV1>(99); reject(p,"epoch policy"); }
        { auto p=raw; ++p.universeCount; reject(p,"universe count"); }
        { auto p=raw; ++p.activeCount; reject(p,"active count"); }
        { auto p=raw; ++p.threadsPerGroup; reject(p,"threads per group"); }
        { auto p=raw; ++p.dispatchGroupsX; reject(p,"Dispatch groups"); }
        { auto p=raw; ++p.indexStrideBytes; reject(p,"index stride"); }
        { auto p=raw; p.fixedCapacityBytes-=4; reject(p,"fixed capacity"); }
        { auto p=raw; --p.outputCapacity; reject(p,"output capacity"); }
        { auto p=raw; ++p.outputStrideBytes; reject(p,"output stride"); }
        { auto p=raw; p.unusedIndexSentinel=0; reject(p,"unused tail sentinel"); }
        { auto p=raw; p.zeroActiveMapsToZeroDispatch=0; reject(p,"zero Dispatch rule"); }
        { auto p=raw; Flip(p.rawCandidateIdentity); reject(p,"raw identity",false); }
        { auto p=raw; p.activeCount=0; reject(p,"active count zero rehash"); }
        { auto p=raw; p.activeCount=4096; reject(p,"active count full rehash"); }
        { auto p=raw; p.dispatchGroupsX=0; reject(p,"zero Dispatch rehash"); }
        { auto p=raw; p.fixedCapacityBytes+=4; reject(p,"oversized Resource rehash"); }
        { auto p=raw; p.outputCapacity=4095; reject(p,"short output rehash"); }
        { auto p=raw; p.unusedIndexSentinel=0xFFFF'FFFEu; reject(p,"alternate tail sentinel rehash"); }
        Require(rawAttackCount == 36, "Raw Sparse mutation count is not 36.");

        auto otherSetResult = s6::semantic::BuildCanonicalSparseWorkSetV1(
            s6::semantic::SparsePatternKindV1::UniformStride,
            s6::semantic::SparseCardinalityV1(1024));
        Require(static_cast<bool>(otherSetResult), "Alternate Sparse set failed.");
        const auto otherSet = std::move(otherSetResult).Value();
        Require(!s6::verification::ValidateVerifiedSparseLoweringContextV1(
            verified, semantic, otherSet, context), "Cross-set Verified seal replay accepted.");

        auto badTarget = context;
        Flip(badTarget.targetProfileIdentity);
        Require(!s6::verification::ValidateVerifiedSparseLoweringContextV1(
            verified, semantic, set, badTarget), "Cross-target Verified seal replay accepted.");
        auto badObservation = context;
        Flip(badObservation.observationContractIdentity);
        Require(!s6::verification::ValidateVerifiedSparseLoweringContextV1(
            verified, semantic, set, badObservation), "Cross-observation replay accepted.");

        s6::execution::SparseIndexResourceBindingInputV1 binding;
        binding.deviceEpoch = 1;
        binding.actualIndexResourceIdentity = verified.Plan().operation.indexResourceIdentity;
        binding.representationRole = verified.Plan().operation.representationRole;
        binding.fixedCapacityBytes = verified.Plan().operation.fixedCapacityBytes;
        binding.indexStrideBytes = verified.Plan().operation.indexStrideBytes;

        auto wrong = binding;
        Flip(wrong.actualIndexResourceIdentity);
        Require(!s6::execution::FreezeVerifiedCompactIndexExecutionV1(
            verified, semantic, set, artifact, wrong), "Wrong actual Resource accepted.");
        wrong = binding;
        wrong.representationRole = static_cast<s6::contract::SparseRepresentationRoleV1>(99);
        Require(!s6::execution::FreezeVerifiedCompactIndexExecutionV1(
            verified, semantic, set, artifact, wrong), "Wrong Resource role accepted.");
        wrong = binding;
        ++wrong.fixedCapacityBytes;
        Require(!s6::execution::FreezeVerifiedCompactIndexExecutionV1(
            verified, semantic, set, artifact, wrong), "Wrong Resource bytes accepted.");
        wrong = binding;
        ++wrong.indexStrideBytes;
        Require(!s6::execution::FreezeVerifiedCompactIndexExecutionV1(
            verified, semantic, set, artifact, wrong), "Wrong Resource stride accepted.");
        wrong = binding;
        wrong.deviceEpoch = 0;
        Require(!s6::execution::FreezeVerifiedCompactIndexExecutionV1(
            verified, semantic, set, artifact, wrong), "Zero device epoch accepted.");
        const auto otherArtifact = s6::contract::BuildCompactIndexListArtifactV1(
            semantic, otherSet);
        Require(!s6::execution::FreezeVerifiedCompactIndexExecutionV1(
            verified, semantic, set, otherArtifact, binding), "Cross-set artifact accepted.");

        auto frozenResult = s6::execution::FreezeVerifiedCompactIndexExecutionV1(
            verified, semantic, set, artifact, binding);
        Require(static_cast<bool>(frozenResult), "Canonical Sparse freeze failed.");
        const auto frozen = frozenResult.Value();
        Require(static_cast<bool>(s6::execution::ValidateFrozenCompactIndexExecutionEpochV1(
            frozen, 1)), "Canonical epoch validation failed.");
        Require(!s6::execution::ValidateFrozenCompactIndexExecutionEpochV1(
            frozen, 2), "Cross-device-epoch Frozen replay accepted.");

        std::cout << "Spiral 6 CU3 authority and replay gates passed before WARP.\n";

        auto run = s6::execution::ExecuteVerifiedCompactIndexListV1(
            semantic, set, frozen, 1);
        if (!run)
        {
#if !defined(_WIN32)
            std::cerr << "Verified CU3 WARP failure [" << run.Error().stage << "]: "
                      << run.Error().message << '\n';
            return 2;
#else
            throw std::runtime_error("Verified Sparse WARP execution failed: " +
                run.Error().stage + ": " + run.Error().message);
#endif
        }

        base::BinaryWriter evidence;
        constexpr std::string_view domain =
            "SGE4-5.Spiral6.CU3.IndependentSparseAuthorityEvidence.V1";
        evidence.WriteBytes(base::Sha256(std::as_bytes(
            std::span(domain.data(), domain.size()))));
        const auto rawBytes = s6::candidate::SerializeRawSparseLoweringCandidateV1(raw);
        const auto verifiedBytes = s6::verification::SerializeVerifiedSparseLoweringV1(verified);
        const auto frozenBytes = s6::execution::SerializeFrozenVerifiedCompactIndexExecutionV1(frozen);
        const auto reportBytes = s6::execution::SerializeVerifiedCompactIndexExecutionReportV1(
            run.Value());
        for (const auto& bytes : {rawBytes, verifiedBytes, frozenBytes, reportBytes})
        {
            evidence.WriteU32(static_cast<std::uint32_t>(bytes.size()));
            evidence.WriteBytes(bytes);
        }
        evidence.WriteU32(rawAttackCount);
        const auto digest = base::Sha256(evidence.Bytes());
        evidence.WriteBytes(digest);
        auto finalEvidence = std::move(evidence).Take();
        if (!emitPath.empty()) WriteFile(emitPath, finalEvidence);

        std::cout << "Spiral 6 CU3 independent Sparse authority passed.\n";
        std::cout << "Sparse set: HashScatterPermutation K=1024\n";
        std::cout << "Mutations: 36 Raw proposal attacks plus set/context/resource/epoch replay gates rejected\n";
        std::cout << "Verified WARP execution: Compact Index List, device epoch 1\n";
        std::cout << "Runtime sparse-policy decision: None\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Spiral 6 CU3 authority test failed: " << error.what() << '\n';
        return 1;
    }
}
