#include "../189_Level4V2CanonicalVocabulary/CanonicalVocabulary.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace canonical = sge4_5::v2::canonical;

namespace
{
template<class To, class From>
concept BraceConstructibleFrom = requires(From value) { To{value}; };

static_assert(!BraceConstructibleFrom<canonical::ActiveCount, canonical::TransitionCount>);
static_assert(!BraceConstructibleFrom<canonical::TransitionCount, canonical::ReuseCount>);
static_assert(!BraceConstructibleFrom<canonical::ReuseCount, canonical::UpdateInterval>);
static_assert(!BraceConstructibleFrom<canonical::UpdateInterval, canonical::BatchSize>);
static_assert(!BraceConstructibleFrom<canonical::BatchSize, canonical::AffectedBlockCount>);
static_assert(!BraceConstructibleFrom<canonical::AffectedBlockCount, canonical::TimelineOrdinal>);
static_assert(!BraceConstructibleFrom<canonical::TimelineOrdinal, canonical::DeviceEpoch>);
static_assert(!BraceConstructibleFrom<canonical::DeviceEpoch, canonical::ActiveCount>);

static_assert(!BraceConstructibleFrom<canonical::SemanticIdentity, canonical::InvocationIdentity>);
static_assert(!BraceConstructibleFrom<canonical::InvocationIdentity, canonical::CandidateIdentity>);
static_assert(!BraceConstructibleFrom<canonical::CandidateIdentity, canonical::VerifiedPlanIdentity>);
static_assert(!BraceConstructibleFrom<canonical::VerifiedPlanIdentity, canonical::FrozenArtifactIdentity>);
static_assert(!BraceConstructibleFrom<canonical::ResourceContractIdentity, canonical::ResourceInstanceIdentity>);
static_assert(!BraceConstructibleFrom<canonical::WriteSetIdentity, canonical::HistoryValidityIdentity>);
static_assert(!BraceConstructibleFrom<canonical::TargetProfileIdentity, canonical::ObservationContractIdentity>);

static_assert(!std::is_default_constructible_v<canonical::OpaqueVerifiedPlanV1>);
static_assert(!std::is_constructible_v<canonical::OpaqueVerifiedPlanV1, canonical::RawCandidateProposalV1>);
static_assert(!std::is_convertible_v<canonical::RawCandidateProposalV1, canonical::OpaqueVerifiedPlanV1>);
static_assert(!std::is_same_v<canonical::RepresentationHandleV1, canonical::HistoryHandleV1>);
static_assert(!std::is_same_v<canonical::CompletionHandleV1, canonical::SubmissionHandleV1>);

void Require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

void AppendU32(std::vector<std::byte>& bytes, std::uint32_t value)
{
    for (std::uint32_t index = 0; index < sizeof(value); ++index)
        bytes.push_back(static_cast<std::byte>((value >> (index * 8u)) & 0xffu));
}

void AppendU64(std::vector<std::byte>& bytes, std::uint64_t value)
{
    for (std::uint32_t index = 0; index < sizeof(value); ++index)
        bytes.push_back(static_cast<std::byte>((value >> (index * 8u)) & 0xffu));
}

void AppendDigest(std::vector<std::byte>& bytes, const canonical::Digest256V1& digest)
{
    bytes.insert(bytes.end(), digest.begin(), digest.end());
}

void WriteEvidence(const std::string& path, std::span<const std::byte> bytes)
{
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) throw std::runtime_error("Unable to create R1 evidence file.");
    stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!stream) throw std::runtime_error("Unable to write R1 evidence file.");
}

std::vector<std::byte> BuildEvidence()
{
    const std::array<std::byte, 6> semanticPayload{
        std::byte{0x10}, std::byte{0x20}, std::byte{0x30},
        std::byte{0x40}, std::byte{0x50}, std::byte{0x60}};
    const std::array<std::byte, 4> invocationPayload{
        std::byte{0x01}, std::byte{0x03}, std::byte{0x05}, std::byte{0x07}};

    const auto semanticIdentity = canonical::MakeCanonicalIdentityV1<canonical::SemanticIdentity>({
        "SGE.Canonical.SemanticMeaning.V1", canonical::CanonicalVocabularySchemaVersionV1, semanticPayload});
    const auto semanticIdentityAgain = canonical::MakeCanonicalIdentityV1<canonical::SemanticIdentity>({
        "SGE.Canonical.SemanticMeaning.V1", canonical::CanonicalVocabularySchemaVersionV1, semanticPayload});
    const auto invocationIdentity = canonical::MakeCanonicalIdentityV1<canonical::InvocationIdentity>({
        "SGE.Canonical.Invocation.V1", canonical::CanonicalVocabularySchemaVersionV1, invocationPayload});
    const auto candidateIdentity = canonical::MakeCanonicalIdentityV1<canonical::CandidateIdentity>({
        "SGE.Canonical.CandidateProposal.V1", canonical::CanonicalVocabularySchemaVersionV1, invocationPayload});
    const auto verifiedIdentity = canonical::MakeCanonicalIdentityV1<canonical::VerifiedPlanIdentity>({
        "SGE.Canonical.VerifiedPlan.V1", canonical::CanonicalVocabularySchemaVersionV1, invocationPayload});
    const auto frozenIdentity = canonical::MakeCanonicalIdentityV1<canonical::FrozenArtifactIdentity>({
        "SGE.Canonical.FrozenArtifact.V1", canonical::CanonicalVocabularySchemaVersionV1, invocationPayload});
    const auto resourceContractIdentity = canonical::MakeCanonicalIdentityV1<canonical::ResourceContractIdentity>({
        "SGE.Canonical.ResourceContract.V1", canonical::CanonicalVocabularySchemaVersionV1, invocationPayload});
    const auto resourceInstanceIdentity = canonical::MakeCanonicalIdentityV1<canonical::ResourceInstanceIdentity>({
        "SGE.Canonical.ResourceInstance.V1", canonical::CanonicalVocabularySchemaVersionV1, invocationPayload});
    const auto writeSetIdentity = canonical::MakeCanonicalIdentityV1<canonical::WriteSetIdentity>({
        "SGE.Canonical.WriteSet.V1", canonical::CanonicalVocabularySchemaVersionV1, invocationPayload});
    const auto historyIdentity = canonical::MakeCanonicalIdentityV1<canonical::HistoryValidityIdentity>({
        "SGE.Canonical.HistoryValidity.V1", canonical::CanonicalVocabularySchemaVersionV1, invocationPayload});
    const auto targetIdentity = canonical::MakeCanonicalIdentityV1<canonical::TargetProfileIdentity>({
        "SGE.Canonical.TargetProfile.V1", canonical::CanonicalVocabularySchemaVersionV1, invocationPayload});
    const auto observationIdentity = canonical::MakeCanonicalIdentityV1<canonical::ObservationContractIdentity>({
        "SGE.Canonical.ObservationContract.V1", canonical::CanonicalVocabularySchemaVersionV1, invocationPayload});

    Require(semanticIdentity == semanticIdentityAgain, "Canonical identity is not deterministic.");
    Require(semanticIdentity.Digest() != invocationIdentity.Digest(), "Domain separation did not change identity.");
    Require(!canonical::DeviceEpoch::TryCreate(0u).has_value(), "Zero Device epoch was accepted.");
    const auto epoch = canonical::DeviceEpoch::TryCreate(9u);
    Require(epoch.has_value(), "Nonzero Device epoch was rejected.");

    const canonical::SemanticDescriptorV1 semantic{semanticIdentity, canonical::SemanticVersion{3u}};
    const canonical::DynamicExecutionDimensionsV1 dimensions{
        canonical::ActiveCount{17u}, canonical::TransitionCount{5u}, canonical::ReuseCount{2u},
        canonical::UpdateInterval{4u}, canonical::BatchSize{7u}, canonical::AffectedBlockCount{3u}};
    const canonical::InvocationDescriptorV1 invocation{
        invocationIdentity, semantic.identity, canonical::TimelineOrdinal{11u}, *epoch, dimensions};
    const canonical::RawCandidateProposalV1 rawCandidate{
        candidateIdentity, semantic.identity, invocation.identity, targetIdentity,
        resourceContractIdentity, writeSetIdentity};
    const canonical::FrozenArtifactDescriptorV1 frozen{
        frozenIdentity, verifiedIdentity, targetIdentity, resourceContractIdentity};
    const canonical::HistoryValidityDescriptorV1 history{
        historyIdentity, semantic.identity, invocation.identity, canonical::HistoryGeneration{12u},
        *epoch, canonical::HistoryStateV1::Valid};
    const canonical::RepresentationHandleV1 representationHandle{resourceInstanceIdentity, *epoch};
    const canonical::HistoryHandleV1 historyHandle{resourceInstanceIdentity, *epoch};

    Require(invocation.dimensions.activeCount->Value() == 17u, "Active count changed.");
    Require(invocation.dimensions.transitionCount->Value() == 5u, "Transition count changed.");
    Require(rawCandidate.identity == candidateIdentity, "Raw Candidate identity changed.");
    Require(frozen.verifiedPlanIdentity == verifiedIdentity, "Verified Plan reference changed.");
    Require(history.generation.Value() == 12u, "History generation changed.");
    Require(representationHandle.Epoch() == historyHandle.Epoch(), "Epoch-bound handle mismatch.");

    std::vector<std::byte> evidence;
    for (const char value : std::string_view{"L4V2R1V1"})
        evidence.push_back(static_cast<std::byte>(static_cast<unsigned char>(value)));
    AppendU32(evidence, canonical::CanonicalVocabularySchemaVersionV1);
    AppendU32(evidence, 11u);
    AppendU32(evidence, 8u);
    AppendU32(evidence, invocation.dimensions.activeCount->Value());
    AppendU32(evidence, invocation.dimensions.transitionCount->Value());
    AppendU32(evidence, invocation.dimensions.reuseCount->Value());
    AppendU32(evidence, invocation.dimensions.updateInterval->Value());
    AppendU32(evidence, invocation.dimensions.batchSize->Value());
    AppendU32(evidence, invocation.dimensions.affectedBlockCount->Value());
    AppendU64(evidence, invocation.timelineOrdinal.Value());
    AppendU64(evidence, invocation.deviceEpoch.Value());
    AppendDigest(evidence, semanticIdentity.Digest());
    AppendDigest(evidence, invocationIdentity.Digest());
    AppendDigest(evidence, candidateIdentity.Digest());
    AppendDigest(evidence, verifiedIdentity.Digest());
    AppendDigest(evidence, frozenIdentity.Digest());
    AppendDigest(evidence, resourceContractIdentity.Digest());
    AppendDigest(evidence, resourceInstanceIdentity.Digest());
    AppendDigest(evidence, writeSetIdentity.Digest());
    AppendDigest(evidence, historyIdentity.Digest());
    AppendDigest(evidence, targetIdentity.Digest());
    AppendDigest(evidence, observationIdentity.Digest());
    return evidence;
}
}

int main(int argc, char** argv)
{
    try
    {
        std::string emitPath;
        for (int index = 1; index < argc; ++index)
        {
            const std::string argument = argv[index];
            if (argument == "--emit")
            {
                if (index + 1 >= argc) throw std::invalid_argument("--emit requires a path.");
                emitPath = argv[++index];
            }
            else if (argument != "--self-test")
            {
                throw std::invalid_argument("Unknown argument: " + argument);
            }
        }

        const auto evidence = BuildEvidence();
        if (!emitPath.empty()) WriteEvidence(emitPath, evidence);

        std::cout << "Level 4 v2 R1 canonical vocabulary self-test passed.\n";
        std::cout << "Strong identity concepts: 11.\n";
        std::cout << "Strong dynamic concepts: 8.\n";
        std::cout << "Opaque Verified Plan construction from Raw Candidate: compile-time unavailable.\n";
        std::cout << "Runtime capability added: None.\n";
        std::cout << "Schema 17 mutation: None. Runtime 17 mutation: None.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Level 4 v2 R1 failure: " << error.what() << '\n';
        return 1;
    }
}
