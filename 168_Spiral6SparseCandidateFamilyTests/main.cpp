#include "../00_Foundation/BinaryIO.h"
#include "../00_Foundation/FileIO.h"
#include "../00_Foundation/Sha256.h"
#include "../154_SparseWorkSetSemantic/SparseWorkSetSemantic.h"
#include "../160_SparseCandidateFamily/SparseCandidateFamily.h"
#include "../161_SparseCandidateFamilyPlanner/SparseCandidateFamilyPlanner.h"
#include "../162_SparseCandidateFamilyVerifier/SparseCandidateFamilyVerifier.h"
#include "../163_Spiral6SparseFamilyExecution/Spiral6SparseFamilyExecution.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace sem = sge4_5::spiral6::semantic;
namespace cand = sge4_5::spiral6::family_candidate;
namespace plan = sge4_5::spiral6::family_planner;
namespace verify = sge4_5::spiral6::family_verification;
namespace exec = sge4_5::spiral6::family_execution;
namespace base = sge4_5::base;

namespace
{
void Require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
void Toggle(base::Digest256& value) { value[0] ^= std::byte{1}; }
std::vector<std::byte> ReadFile(const std::filesystem::path& path)
{
    auto bytes = base::ReadAllBytes(path);
    if (!bytes) throw std::runtime_error(bytes.Error());
    return std::move(bytes).Value();
}

struct CaseData final
{
    sem::SparsePatternKindV1 pattern;
    sem::ExactSparseWorkSetV1 set;
    std::vector<cand::RawSparseFamilyCandidateV1> raw;
    std::vector<verify::VerifiedSparseFamilyCandidateV1> verified;
    std::vector<exec::FrozenVerifiedSparseFamilyCandidateV1> frozen;
    exec::SparseCandidateFamilyCaseResultV1 architecture;

    CaseData(
        sem::SparsePatternKindV1 patternValue,
        sem::ExactSparseWorkSetV1 setValue,
        std::vector<cand::RawSparseFamilyCandidateV1> rawValue,
        std::vector<verify::VerifiedSparseFamilyCandidateV1> verifiedValue,
        std::vector<exec::FrozenVerifiedSparseFamilyCandidateV1> frozenValue,
        exec::SparseCandidateFamilyCaseResultV1 architectureValue)
        : pattern(patternValue), set(std::move(setValue)), raw(std::move(rawValue)),
          verified(std::move(verifiedValue)), frozen(std::move(frozenValue)),
          architecture(std::move(architectureValue)) {}
};

void WriteBundle(
    const std::filesystem::path& path,
    const sem::SparsePointTransformSemanticV1& semanticValue,
    const std::vector<CaseData>& cases)
{
    base::BinaryWriter writer;
    constexpr std::string_view domain = "SGE4-5.Spiral6.CU4.SparseCandidateFamilyEvidence.V1";
    writer.WriteBytes(base::Sha256(std::as_bytes(std::span(domain.data(), domain.size()))));
    const auto semanticBytes = sem::SerializeSparsePointTransformSemanticV1(semanticValue);
    writer.WriteU32(static_cast<std::uint32_t>(semanticBytes.size()));
    writer.WriteBytes(semanticBytes);
    writer.WriteU32(static_cast<std::uint32_t>(cases.size()));
    for (const auto& testCase : cases)
    {
        writer.WriteU32(static_cast<std::uint32_t>(testCase.pattern));
        const auto setBytes = sem::SerializeExactSparseWorkSetV1(testCase.set);
        writer.WriteU32(static_cast<std::uint32_t>(setBytes.size()));
        writer.WriteBytes(setBytes);
        writer.WriteU32(static_cast<std::uint32_t>(testCase.raw.size()));
        for (std::size_t i = 0; i < testCase.raw.size(); ++i)
        {
            const auto rawBytes = cand::SerializeRawSparseFamilyCandidateV1(testCase.raw[i]);
            const auto verifiedBytes = verify::SerializeVerifiedSparseFamilyCandidateV1(testCase.verified[i]);
            const auto frozenBytes = exec::SerializeFrozenVerifiedSparseFamilyCandidateV1(testCase.frozen[i]);
            writer.WriteU32(static_cast<std::uint32_t>(rawBytes.size())); writer.WriteBytes(rawBytes);
            writer.WriteBytes(testCase.raw[i].rawCandidateIdentity);
            writer.WriteU32(static_cast<std::uint32_t>(verifiedBytes.size())); writer.WriteBytes(verifiedBytes);
            writer.WriteU32(static_cast<std::uint32_t>(frozenBytes.size())); writer.WriteBytes(frozenBytes);
        }
        const auto architectureBytes = exec::SerializeSparseCandidateFamilyCaseResultV1(testCase.architecture);
        writer.WriteU32(static_cast<std::uint32_t>(architectureBytes.size()));
        writer.WriteBytes(architectureBytes);
    }
    writer.WriteBytes(base::Sha256(writer.Bytes()));
    std::filesystem::create_directories(path.parent_path());
    auto written = base::WriteAllBytes(path, std::move(writer).Take());
    if (!written) throw std::runtime_error(written.Error());
}
}

int main(int argc, char** argv)
{
    try
    {
        std::filesystem::path output = "build/tests/spiral6-cu4/evidence.bin";
        for (int i = 1; i < argc; ++i)
        {
            const std::string argument = argv[i];
            if (argument == "--emit" && i + 1 < argc) output = argv[++i];
            else throw std::runtime_error("unknown argument");
        }

        const auto semanticValue = sem::BuildCanonicalSparsePointTransformSemanticV1();
        const auto context = verify::BuildCanonicalSparseFamilyVerificationContextV1();
        const std::vector<std::pair<sem::SparsePatternKindV1, std::uint32_t>> corpus{
            {sem::SparsePatternKindV1::PrefixControl, 0u},
            {sem::SparsePatternKindV1::HashScatterPermutation, 1u},
            {sem::SparsePatternKindV1::PrefixControl, 65u},
            {sem::SparsePatternKindV1::UniformStride, 65u},
            {sem::SparsePatternKindV1::BlockClusteredPermutation, 65u},
            {sem::SparsePatternKindV1::HashScatterPermutation, 65u},
            {sem::SparsePatternKindV1::PrefixControl, 1024u},
            {sem::SparsePatternKindV1::UniformStride, 1024u},
            {sem::SparsePatternKindV1::BlockClusteredPermutation, 1024u},
            {sem::SparsePatternKindV1::HashScatterPermutation, 1024u},
            {sem::SparsePatternKindV1::HashScatterPermutation, 4095u},
            {sem::SparsePatternKindV1::PrefixControl, 4096u}};

        std::vector<CaseData> cases;
        for (const auto& [pattern, cardinality] : corpus)
        {
            auto setResult = sem::BuildCanonicalSparseWorkSetV1(pattern, sem::SparseCardinalityV1(cardinality));
            Require(static_cast<bool>(setResult), "Sparse Family set construction failed");
            auto set = std::move(setResult).Value();
            auto planned = plan::PlanCanonicalSparseCandidateFamilyV1(semanticValue, set);
            Require(static_cast<bool>(planned), "Sparse Family Planner failed");
            auto raw = std::move(planned).Value();
            Require(raw.size() == 3, "Sparse Family size differs from three");
            std::vector<verify::VerifiedSparseFamilyCandidateV1> verified;
            std::vector<exec::FrozenVerifiedSparseFamilyCandidateV1> frozen;
            for (const auto& proposal : raw)
            {
                auto verifiedResult = verify::VerifySparseFamilyCandidateV1(semanticValue, set, context, proposal);
                Require(static_cast<bool>(verifiedResult), "Sparse Family independent verification failed");
                verified.push_back(std::move(verifiedResult).Value());
                const auto artifact = cand::BuildSparseFamilyArtifactV1(semanticValue, set, proposal.kind);
                const auto binding = exec::BuildCanonicalSparseFamilyResourceBindingInputV1(verified.back(), 1);
                auto frozenResult = exec::FreezeVerifiedSparseFamilyCandidateV1(
                    semanticValue, set, context, verified.back(), artifact, binding);
                Require(static_cast<bool>(frozenResult), "Sparse Family Freeze failed");
                frozen.push_back(std::move(frozenResult).Value());
            }
            auto architecture = exec::RunVerifiedSparseCandidateFamilyOnWarpV1(
                semanticValue, pattern, set, context, frozen, 1);
            if (!architecture)
                throw std::runtime_error(architecture.Error().stage + ": " + architecture.Error().message);
            Require(architecture.Value().pairwiseOutputByteIdentical, "Sparse Family output equivalence failed");
            cases.emplace_back(pattern, std::move(set), std::move(raw), std::move(verified),
                std::move(frozen), std::move(architecture).Value());
        }
        Require(cases.size() == 12, "Sparse Family corpus size differs from 12");

        auto representativeSetResult = sem::BuildCanonicalSparseWorkSetV1(
            sem::SparsePatternKindV1::HashScatterPermutation, sem::SparseCardinalityV1(1024));
        Require(static_cast<bool>(representativeSetResult), "Representative set failed");
        const auto representativeSet = std::move(representativeSetResult).Value();
        auto representativeFamily = plan::PlanCanonicalSparseCandidateFamilyV1(semanticValue, representativeSet);
        Require(static_cast<bool>(representativeFamily), "Representative family failed");
        const auto canonical = representativeFamily.Value()[2];
        std::uint32_t attempted = 0;
        std::uint32_t rejected = 0;
        auto reject = [&](cand::RawSparseFamilyCandidateV1 proposal, bool rehash)
        {
            ++attempted;
            if (rehash) proposal.rawCandidateIdentity = cand::RawSparseFamilyCandidateIdentityV1(proposal);
            if (!verify::VerifySparseFamilyCandidateV1(semanticValue, representativeSet, context, proposal)) ++rejected;
        };
        auto p = canonical; Toggle(p.semanticIdentity); reject(p, false);
        p = canonical; Toggle(p.sparseSetIdentity); reject(p, false);
        p = canonical; Toggle(p.targetProfileIdentity); reject(p, false);
        p = canonical; Toggle(p.observationContractIdentity); reject(p, false);
        p = canonical; Toggle(p.representationResourceContractIdentity); reject(p, false);
        p = canonical; Toggle(p.completionContractIdentity); reject(p, false);
        p = canonical; Toggle(p.deviceEpochPolicyIdentity); reject(p, false);
        p = canonical; Toggle(p.spiral4IndirectArtifactIdentity); reject(p, false);
        p = canonical; Toggle(p.consumerProgramIdentity); reject(p, false);
        p = canonical; Toggle(p.proposedArtifactIdentity); reject(p, false);
        p = canonical; Toggle(p.proposedRepresentationResourceIdentity); reject(p, false);
        p = canonical; Toggle(p.writeSetAuthorityIdentity); reject(p, false);
        p = canonical; Toggle(p.cu2CompactArtifactIdentity); reject(p, false);
        p = canonical; p.kind = cand::SparseFamilyCandidateKindV1::CompactIndexListDispatch; reject(p, false);
        p = canonical; p.representationRole = cand::SparseFamilyRepresentationRoleV1::CompactSortedUniverseIndexList; reject(p, false);
        p = canonical; p.operationCode = cand::SparseFamilyOperationCodeV1::ExecuteCompactIndexList; reject(p, false);
        p = canonical; p.dispatchPolicy = cand::SparseFamilyDispatchPolicyV1::CeilCardinalityGroups; reject(p, false);
        p = canonical; p.completionPolicy = static_cast<cand::SparseFamilyCompletionPolicyV1>(2); reject(p, false);
        p = canonical; p.deviceEpochPolicy = static_cast<cand::SparseFamilyDeviceEpochPolicyV1>(2); reject(p, false);
        p = canonical; ++p.universeCount; reject(p, false);
        p = canonical; ++p.activeCount; reject(p, false);
        p = canonical; ++p.activeBlockCount; reject(p, false);
        p = canonical; ++p.threadsPerGroup; reject(p, false);
        p = canonical; ++p.dispatchGroupsX; reject(p, false);
        p = canonical; ++p.payloadStrideBytes; reject(p, false);
        p = canonical; ++p.fixedCapacityBytes; reject(p, false);
        p = canonical; --p.outputCapacity; reject(p, false);
        p = canonical; ++p.outputStrideBytes; reject(p, false);
        p = canonical; p.zeroActiveMapsToZeroDispatch = 0; reject(p, false);
        p = canonical; Toggle(p.rawCandidateIdentity); reject(p, false);
        p = canonical; p.kind = cand::SparseFamilyCandidateKindV1::CompactIndexListDispatch; reject(p, true);
        p = canonical; p.representationRole = cand::SparseFamilyRepresentationRoleV1::CompactSortedUniverseIndexList; reject(p, true);
        p = canonical; ++p.dispatchGroupsX; reject(p, true);
        p = canonical; ++p.activeBlockCount; reject(p, true);
        p = canonical; Toggle(p.proposedArtifactIdentity); reject(p, true);
        p = canonical; Toggle(p.proposedRepresentationResourceIdentity); reject(p, true);
        p = canonical; Toggle(p.writeSetAuthorityIdentity); reject(p, true);
        p = canonical; Toggle(p.cu2CompactArtifactIdentity); reject(p, true);
        p = canonical; ++p.fixedCapacityBytes; reject(p, true);
        p = canonical; p.zeroActiveMapsToZeroDispatch = 0; reject(p, true);
        Require(attempted == 40 && rejected == attempted, "Sparse Family 40-mutation gate failed");

        auto otherSetResult = sem::BuildCanonicalSparseWorkSetV1(
            sem::SparsePatternKindV1::UniformStride, sem::SparseCardinalityV1(1024));
        Require(static_cast<bool>(otherSetResult), "Cross-set fixture failed");
        const auto otherSet = std::move(otherSetResult).Value();
        auto verifiedC = verify::VerifySparseFamilyCandidateV1(semanticValue, representativeSet, context, canonical);
        Require(static_cast<bool>(verifiedC), "Representative C verification failed");
        Require(!verify::ValidateVerifiedSparseFamilyCandidateV1(verifiedC.Value(), semanticValue, otherSet, context),
            "Cross-set Sparse Family seal replay accepted");
        const auto artifactC = cand::BuildSparseFamilyArtifactV1(
            semanticValue, representativeSet, cand::SparseFamilyCandidateKindV1::ActiveBlockListLocalMask);
        auto badResource = exec::BuildCanonicalSparseFamilyResourceBindingInputV1(verifiedC.Value(), 1);
        Toggle(badResource.actualRepresentationResourceIdentity);
        Require(!exec::FreezeVerifiedSparseFamilyCandidateV1(
            semanticValue, representativeSet, context, verifiedC.Value(), artifactC, badResource),
            "Wrong Sparse Family Resource accepted");
        auto badRole = exec::BuildCanonicalSparseFamilyResourceBindingInputV1(verifiedC.Value(), 1);
        badRole.representationRole = cand::SparseFamilyRepresentationRoleV1::CompactSortedUniverseIndexList;
        Require(!exec::FreezeVerifiedSparseFamilyCandidateV1(
            semanticValue, representativeSet, context, verifiedC.Value(), artifactC, badRole),
            "Cross-role Sparse Family binding accepted");
        auto badLayout = exec::BuildCanonicalSparseFamilyResourceBindingInputV1(verifiedC.Value(), 1);
        ++badLayout.fixedCapacityBytes;
        Require(!exec::FreezeVerifiedSparseFamilyCandidateV1(
            semanticValue, representativeSet, context, verifiedC.Value(), artifactC, badLayout),
            "Wrong Sparse Family layout accepted");
        const auto goodBinding = exec::BuildCanonicalSparseFamilyResourceBindingInputV1(verifiedC.Value(), 1);
        auto frozenC = exec::FreezeVerifiedSparseFamilyCandidateV1(
            semanticValue, representativeSet, context, verifiedC.Value(), artifactC, goodBinding);
        Require(static_cast<bool>(frozenC), "Representative C Freeze failed");
        Require(!exec::ValidateFrozenVerifiedSparseFamilyCandidateV1(
            frozenC.Value(), semanticValue, representativeSet, context, 2),
            "Cross-epoch Sparse Family Frozen Candidate accepted");

        WriteBundle(output, semanticValue, cases);
        std::cout << "Spiral 6 CU4 Sparse Candidate family passed.\n";
        std::cout << "Adapter: " << cases.front().architecture.adapterDescription << "\n";
        std::cout << "Candidates: A.DenseMembershipMaskPredicate, B.CompactIndexListDispatch, C.ActiveBlockListLocalMask\n";
        std::cout << "Cases: 12; Candidate executions: 12 x 3 = 36\n";
        std::cout << "Pairwise full-output equivalence: byte-identical\n";
        std::cout << "Exact write set and inactive Float4Zero sentinel: qualified\n";
        std::cout << "Runtime sparse-policy decision: None\n";
        std::cout << "CU4 Candidate family evidence SHA-256: "
                  << base::ToHex(base::Sha256(ReadFile(output))) << "\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Spiral 6 CU4 failure: " << error.what() << '\n';
        return 1;
    }
}
