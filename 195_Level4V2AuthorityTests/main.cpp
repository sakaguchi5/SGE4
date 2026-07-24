#include "../192_Level4V2CandidatePlanner/CandidatePlanner.h"
#include "../193_Level4V2IndependentVerifier/IndependentVerifier.h"
#include "../194_Level4V2FrozenAuthority/FrozenAuthority.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include <string_view>

namespace canonical = sge4_5::v2::canonical;
namespace authority = sge4_5::v2::authority;
namespace frozen = sge4_5::v2::frozen;

namespace
{
template<class To, class From>
concept BraceConstructibleFrom = requires(From value) { To{value}; };

static_assert(!std::is_default_constructible_v<canonical::OpaqueVerifiedPlanV1>);
static_assert(!BraceConstructibleFrom<canonical::OpaqueVerifiedPlanV1, authority::PlannerProposalV1>);
static_assert(!BraceConstructibleFrom<canonical::OpaqueVerifiedPlanV1, canonical::RawCandidateProposalV1>);
static_assert(!std::is_default_constructible_v<authority::VerifiedAuthorityV1>);
static_assert(!BraceConstructibleFrom<frozen::OpaqueFrozenArtifactV1, canonical::RawCandidateProposalV1>);
static_assert(!BraceConstructibleFrom<frozen::OpaqueFrozenArtifactV1, authority::PlannerProposalV1>);

void Require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

void AppendU8(std::vector<std::byte>& bytes, std::uint8_t value)
{
    bytes.push_back(static_cast<std::byte>(value));
}

void AppendU32(std::vector<std::byte>& bytes, std::uint32_t value)
{
    for (std::uint32_t index = 0; index < sizeof(value); ++index)
        bytes.push_back(static_cast<std::byte>((value >> (index * 8u)) & 0xffu));
}

void AppendDigest(std::vector<std::byte>& bytes, const canonical::Digest256V1& digest)
{
    bytes.insert(bytes.end(), digest.begin(), digest.end());
}

template<class Identity>
Identity MutateIdentity(const Identity& identity, std::uint8_t mask)
{
    auto digest = identity.Digest();
    digest[0] ^= static_cast<std::byte>(mask);
    return Identity::FromDigest(std::move(digest));
}

void WriteEvidence(const std::string& path, std::span<const std::byte> bytes)
{
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) throw std::runtime_error("Unable to create R2 evidence file.");
    stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!stream) throw std::runtime_error("Unable to write R2 evidence file.");
}

struct Fixture final
{
    authority::AuthorityRequestV1 request;
    authority::ExplicitCandidateInputV1 candidateInput;
    authority::PlannerProposalV1 proposal;
};

Fixture BuildFixture()
{
    const std::array<std::byte, 5> semanticPayload{
        std::byte{0x11}, std::byte{0x22}, std::byte{0x33}, std::byte{0x44}, std::byte{0x55}};
    const std::array<std::byte, 4> targetPayload{
        std::byte{0x02}, std::byte{0x04}, std::byte{0x06}, std::byte{0x08}};
    const std::array<std::byte, 6> resourcePayload{
        std::byte{0x10}, std::byte{0x12}, std::byte{0x14},
        std::byte{0x16}, std::byte{0x18}, std::byte{0x1a}};
    const std::array<std::byte, 3> writePayload{
        std::byte{0x21}, std::byte{0x23}, std::byte{0x25}};
    const std::array<std::byte, 7> candidatePayload{
        std::byte{0x31}, std::byte{0x32}, std::byte{0x33}, std::byte{0x34},
        std::byte{0x35}, std::byte{0x36}, std::byte{0x37}};

    const auto epoch = canonical::DeviceEpoch::TryCreate(7u);
    Require(epoch.has_value(), "Nonzero Device epoch was rejected.");
    auto semantic = authority::MakeSemanticDefinitionV1(canonical::SemanticVersion{2u}, semanticPayload);
    const canonical::DynamicExecutionDimensionsV1 dimensions{
        canonical::ActiveCount{19u}, canonical::TransitionCount{6u}, canonical::ReuseCount{3u},
        canonical::UpdateInterval{5u}, canonical::BatchSize{8u}, canonical::AffectedBlockCount{4u}};
    auto invocation = authority::MakeInvocationDescriptorV1(
        semantic.descriptor.identity, canonical::TimelineOrdinal{13u}, *epoch, dimensions);
    auto target = authority::MakeTargetProfileDefinitionV1(3u, targetPayload);
    auto resource = authority::MakeResourceContractDefinitionV1(4u, resourcePayload);
    auto writeSet = authority::MakeWriteSetDefinitionV1(5u, writePayload);
    authority::AuthorityRequestV1 request{
        std::move(semantic), invocation, std::move(target), std::move(resource), std::move(writeSet)};
    authority::ExplicitCandidateInputV1 candidateInput{6u, std::vector<std::byte>(candidatePayload.begin(), candidatePayload.end())};
    auto proposal = authority::CandidatePlannerV1::Plan(request, candidateInput);
    return {std::move(request), std::move(candidateInput), std::move(proposal)};
}

void ExpectError(
    authority::VerificationErrorV1 expected,
    const authority::AuthorityRequestV1& request,
    const authority::ExplicitCandidateInputV1& candidateInput,
    const authority::PlannerProposalV1& proposal,
    std::vector<authority::VerificationErrorV1>& observed)
{
    const auto result = authority::IndependentVerifierV1::Verify(request, candidateInput, proposal);
    Require(!result.Accepted(), "Mutated authority input was accepted.");
    Require(result.error == expected, "Mutation was rejected at an unexpected boundary.");
    observed.push_back(result.error);
}

std::vector<std::byte> BuildEvidence()
{
    const auto fixture = BuildFixture();
    const auto accepted = authority::IndependentVerifierV1::Verify(
        fixture.request, fixture.candidateInput, fixture.proposal);
    Require(accepted.Accepted(), "Valid Planner proposal was rejected.");
    Require(accepted.verified.has_value(), "Accepted result did not contain Verified authority.");
    const auto frozenArtifact = frozen::FrozenAuthorityBuilderV1::Freeze(*accepted.verified);
    Require(frozenArtifact.Descriptor().verifiedPlanIdentity == accepted.verified->Plan().Identity(),
        "Frozen artifact lost the Verified Plan identity.");

    std::vector<authority::VerificationErrorV1> observed;
    observed.reserve(18u);

    {
        auto request = fixture.request;
        request.semantic.canonicalPayload[0] ^= std::byte{0x01};
        ExpectError(authority::VerificationErrorV1::SemanticDefinitionIdentityMismatch,
            request, fixture.candidateInput, fixture.proposal, observed);
    }
    {
        auto request = fixture.request;
        request.invocation.semanticIdentity = MutateIdentity(request.invocation.semanticIdentity, 0x02u);
        ExpectError(authority::VerificationErrorV1::InvocationSemanticBindingMismatch,
            request, fixture.candidateInput, fixture.proposal, observed);
    }
    {
        auto request = fixture.request;
        request.invocation.dimensions.activeCount = canonical::ActiveCount{20u};
        ExpectError(authority::VerificationErrorV1::InvocationDefinitionIdentityMismatch,
            request, fixture.candidateInput, fixture.proposal, observed);
    }
    {
        auto request = fixture.request;
        request.targetProfile.canonicalPayload[0] ^= std::byte{0x04};
        ExpectError(authority::VerificationErrorV1::TargetProfileDefinitionIdentityMismatch,
            request, fixture.candidateInput, fixture.proposal, observed);
    }
    {
        auto request = fixture.request;
        request.resourceContract.canonicalPayload[0] ^= std::byte{0x08};
        ExpectError(authority::VerificationErrorV1::ResourceContractDefinitionIdentityMismatch,
            request, fixture.candidateInput, fixture.proposal, observed);
    }
    {
        auto request = fixture.request;
        request.legalWriteSet.canonicalPayload[0] ^= std::byte{0x10};
        ExpectError(authority::VerificationErrorV1::WriteSetDefinitionIdentityMismatch,
            request, fixture.candidateInput, fixture.proposal, observed);
    }
    {
        auto proposal = fixture.proposal;
        proposal.rawCandidate.semanticIdentity = MutateIdentity(proposal.rawCandidate.semanticIdentity, 0x11u);
        ExpectError(authority::VerificationErrorV1::CandidateSemanticBindingMismatch,
            fixture.request, fixture.candidateInput, proposal, observed);
    }
    {
        auto proposal = fixture.proposal;
        proposal.rawCandidate.invocationIdentity = MutateIdentity(proposal.rawCandidate.invocationIdentity, 0x12u);
        ExpectError(authority::VerificationErrorV1::CandidateInvocationBindingMismatch,
            fixture.request, fixture.candidateInput, proposal, observed);
    }
    {
        const std::array<std::byte, 2> alternatePayload{std::byte{0x71}, std::byte{0x72}};
        auto request = fixture.request;
        request.targetProfile = authority::MakeTargetProfileDefinitionV1(3u, alternatePayload);
        ExpectError(authority::VerificationErrorV1::CandidateTargetBindingMismatch,
            request, fixture.candidateInput, fixture.proposal, observed);
    }
    {
        const std::array<std::byte, 2> alternatePayload{std::byte{0x73}, std::byte{0x74}};
        auto request = fixture.request;
        request.resourceContract = authority::MakeResourceContractDefinitionV1(4u, alternatePayload);
        ExpectError(authority::VerificationErrorV1::CandidateResourceBindingMismatch,
            request, fixture.candidateInput, fixture.proposal, observed);
    }
    {
        const std::array<std::byte, 2> alternatePayload{std::byte{0x75}, std::byte{0x76}};
        auto request = fixture.request;
        request.legalWriteSet = authority::MakeWriteSetDefinitionV1(5u, alternatePayload);
        ExpectError(authority::VerificationErrorV1::CandidateWriteSetBindingMismatch,
            request, fixture.candidateInput, fixture.proposal, observed);
    }
    {
        auto candidateInput = fixture.candidateInput;
        candidateInput.canonicalPayload[0] ^= std::byte{0x20};
        ExpectError(authority::VerificationErrorV1::CandidateIdentityMismatch,
            fixture.request, candidateInput, fixture.proposal, observed);
    }
    {
        auto proposal = fixture.proposal;
        proposal.operations.pop_back();
        ExpectError(authority::VerificationErrorV1::OperationCountMismatch,
            fixture.request, fixture.candidateInput, proposal, observed);
    }
    {
        auto proposal = fixture.proposal;
        proposal.operations[0].ordinal = authority::OperationOrdinal{9u};
        ExpectError(authority::VerificationErrorV1::OperationOrdinalMismatch,
            fixture.request, fixture.candidateInput, proposal, observed);
    }
    {
        auto proposal = fixture.proposal;
        proposal.operations[0].kind = authority::AuthorityOperationKindV1::BindInvocation;
        ExpectError(authority::VerificationErrorV1::OperationKindMismatch,
            fixture.request, fixture.candidateInput, proposal, observed);
    }
    {
        auto proposal = fixture.proposal;
        proposal.operations[0].subjectIdentity[0] ^= std::byte{0x40};
        ExpectError(authority::VerificationErrorV1::OperationSubjectMismatch,
            fixture.request, fixture.candidateInput, proposal, observed);
    }
    {
        auto proposal = fixture.proposal;
        proposal.identity = MutateIdentity(proposal.identity, 0x41u);
        ExpectError(authority::VerificationErrorV1::PlannerProposalIdentityMismatch,
            fixture.request, fixture.candidateInput, proposal, observed);
    }
    {
        auto proposal = fixture.proposal;
        proposal.rawCandidate.identity = MutateIdentity(proposal.rawCandidate.identity, 0x42u);
        ExpectError(authority::VerificationErrorV1::CandidateIdentityMismatch,
            fixture.request, fixture.candidateInput, proposal, observed);
    }

    Require(observed.size() == 18u, "Mutation matrix observation count mismatch.");

    const auto contextIdentity = authority::ComputeAuthorityContextIdentityV1(fixture.request);
    const auto operationSequenceIdentity = authority::ComputeOperationSequenceIdentityV1(fixture.proposal.operations);

    std::vector<std::byte> evidence;
    for (const char character : std::string_view{"L4V2R2V1"})
        evidence.push_back(static_cast<std::byte>(static_cast<unsigned char>(character)));
    AppendU32(evidence, authority::AuthorityModelSchemaVersionV1);
    AppendU32(evidence, authority::AuthorityOperationCountV1);
    AppendU32(evidence, static_cast<std::uint32_t>(observed.size()));
    AppendDigest(evidence, fixture.request.semantic.descriptor.identity.Digest());
    AppendDigest(evidence, fixture.request.invocation.identity.Digest());
    AppendDigest(evidence, fixture.request.targetProfile.identity.Digest());
    AppendDigest(evidence, fixture.request.resourceContract.identity.Digest());
    AppendDigest(evidence, fixture.request.legalWriteSet.identity.Digest());
    AppendDigest(evidence, fixture.proposal.rawCandidate.identity.Digest());
    AppendDigest(evidence, fixture.proposal.identity.Digest());
    AppendDigest(evidence, contextIdentity.Digest());
    AppendDigest(evidence, operationSequenceIdentity.Digest());
    AppendDigest(evidence, accepted.verified->Plan().Identity().Digest());
    AppendDigest(evidence, accepted.verified->SealIdentity().Digest());
    AppendDigest(evidence, frozenArtifact.Descriptor().identity.Digest());
    for (const auto error : observed) AppendU8(evidence, static_cast<std::uint8_t>(error));
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

        std::cout << "Level 4 v2 R2 unified authority self-test passed.\n";
        std::cout << "Authority operations: 6. Mutation/replay rejections: 18.\n";
        std::cout << "Planner Verified-plan construction: unavailable.\n";
        std::cout << "Verifier dependency on Planner project: None.\n";
        std::cout << "Runtime capability added: None.\n";
        std::cout << "Schema 17 mutation: None. Runtime 17 mutation: None.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Level 4 v2 R2 failure: " << error.what() << '\n';
        return 1;
    }
}
