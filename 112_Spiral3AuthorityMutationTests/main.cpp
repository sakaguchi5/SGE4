#include "../102_Spiral3Corpus/Spiral3Corpus.h"
#include "../104_ReuseRepresentationPlanner/ReuseRepresentationPlanner.h"
#include "../105_ReuseRepresentationVerifier/ReuseRepresentationVerifier.h"

#include <iostream>
#include <type_traits>

namespace corpus = sge4_5::spiral3::corpus;
namespace candidate = sge4_5::spiral3::candidate;
namespace planner = sge4_5::spiral3::planner;
namespace verification = sge4_5::spiral3::verification;
namespace base = sge4_5::base;

static_assert(!std::is_aggregate_v<verification::VerifiedReuseRepresentationV1>);
static_assert(!std::is_default_constructible_v<verification::VerifiedReuseRepresentationV1>);
static_assert(!std::is_constructible_v<
    verification::VerifiedReuseRepresentationV1,
    verification::VerifiedReuseExecutionPlanV1,
    base::Digest256>);
static_assert(!std::is_convertible_v<
    candidate::RawCandidateGraphV1,
    verification::VerifiedReuseRepresentationV1>);
static_assert(!std::is_convertible_v<candidate::CandidateKindV1, std::uint32_t>);
static_assert(!std::is_convertible_v<candidate::OperationKindV1, std::uint32_t>);
static_assert(!std::is_convertible_v<candidate::MatrixLoweringPositionV1, std::uint32_t>);

int main()
{
    auto hierarchy = corpus::BuildCanonicalMeasurementHierarchyV1();
    if (!hierarchy)
        return 1;

    auto cases = corpus::BuildReuseCorpusV1(hierarchy.Value());
    if (!cases)
        return 2;

    const auto& semantic = cases.Value()[3].semantic;
    auto raw = planner::PlanCandidateGraphV1(
        hierarchy.Value(), semantic, candidate::CandidateKindV1::HybridHierarchy);
    if (!raw)
        return 3;

    std::uint32_t rejected = 0;
    auto reject = [&](candidate::RawCandidateGraphV1 proposal)
    {
        if (!verification::VerifyCandidateGraphV1(hierarchy.Value(), semantic, proposal))
            ++rejected;
    };

    auto proposal = raw.Value();
    ++proposal.boneCount;
    reject(proposal);

    proposal = raw.Value();
    ++proposal.reuseCount;
    reject(proposal);

    proposal = raw.Value();
    ++proposal.pointCount;
    reject(proposal);

    proposal = raw.Value();
    proposal.localPointCorpusIdentity = {};
    reject(proposal);

    proposal = raw.Value();
    proposal.reuseSemanticIdentity = {};
    reject(proposal);

    proposal = raw.Value();
    proposal.hierarchySemanticIdentity = {};
    reject(proposal);

    proposal = raw.Value();
    proposal.dynamicInvocationSchemaIdentity = {};
    reject(proposal);

    proposal = raw.Value();
    proposal.observationContractIdentity = {};
    reject(proposal);

    proposal = raw.Value();
    proposal.kind = candidate::CandidateKindV1::MatrixHierarchy;
    reject(proposal);

    proposal = raw.Value();
    proposal.proposedLoweringPosition = candidate::MatrixLoweringPositionV1::BeforeHierarchy;
    reject(proposal);

    proposal = raw.Value();
    proposal.proposedHierarchyExecutionModel =
        static_cast<candidate::HierarchyExecutionModelV1>(0);
    reject(proposal);

    proposal = raw.Value();
    ++proposal.hierarchyDepthCount;
    reject(proposal);

    proposal = raw.Value();
    proposal.commonSourceDispatchCount = 1;
    reject(proposal);

    proposal = raw.Value();
    ++proposal.proposedCandidateLeafDispatchCount;
    reject(proposal);

    proposal = raw.Value();
    ++proposal.nodes.back().ordinal;
    reject(proposal);

    proposal = raw.Value();
    proposal.nodes.back().operation = candidate::OperationKindV1::DirectPgaConsumerApply;
    reject(proposal);

    proposal = raw.Value();
    --proposal.nodes.back().primaryInputStride;
    reject(proposal);

    proposal = raw.Value();
    --proposal.nodes.back().secondaryInputElementCount;
    reject(proposal);

    proposal = raw.Value();
    ++proposal.nodes.back().dispatchX;
    reject(proposal);

    proposal = raw.Value();
    proposal.nodes.back().dispatchY = 2;
    reject(proposal);

    proposal = raw.Value();
    proposal.nodes.back().programIdentity = {};
    reject(proposal);

    proposal = raw.Value();
    proposal.edges.pop_back();
    reject(proposal);

    proposal = raw.Value();
    proposal.edges.back().source = 0;
    reject(proposal);

    proposal = raw.Value();
    proposal.edges.back().destination = 0;
    reject(proposal);

    std::cout << "Spiral 3 authority mutation tests rejected " << rejected
              << " invalid proposals; compile-time Raw/Verified type boundaries passed.\n";
    return rejected == 24 ? 0 : 4;
}
