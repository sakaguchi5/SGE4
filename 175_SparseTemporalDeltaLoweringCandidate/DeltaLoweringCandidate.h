#pragma once

#include "../00_Foundation/Sha256.h"
#include "../172_SparseTemporalDeltaSemantic/SparseTemporalDeltaSemantic.h"
#include "../173_Spiral7DeltaContracts/Spiral7DeltaContracts.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace sge4_5::spiral7::candidate
{
enum class DeltaLoweringKindV1 : std::uint32_t
{
    CompactDeltaIndexHistoryReuse = 2
};

enum class DeltaCompletionPolicyV1 : std::uint32_t
{
    ExecuteIndirectCompletionAuthorizesExactTransitionSet = 1
};

enum class DeltaDeviceEpochPolicyV1 : std::uint32_t
{
    HistoryAndDerivedResourcesBoundToDeviceEpoch = 1
};

struct RawDeltaLoweringCandidateV1 final
{
    base::Digest256 semanticIdentity{};
    base::Digest256 invocationIdentity{};
    base::Digest256 previousActiveSetIdentity{};
    base::Digest256 activeSetIdentity{};
    base::Digest256 modifiedSetIdentity{};
    base::Digest256 transitionSetIdentity{};
    base::Digest256 transitionRecordsIdentity{};
    base::Digest256 targetProfileIdentity{};
    base::Digest256 observationContractIdentity{};
    base::Digest256 deltaResourceContractIdentity{};
    base::Digest256 previousHistoryResourceContractIdentity{};
    base::Digest256 outputHistoryResourceContractIdentity{};
    base::Digest256 completionContractIdentity{};
    base::Digest256 deviceEpochPolicyIdentity{};
    base::Digest256 spiral4IndirectIdentity{};
    base::Digest256 spiral5TemporalIdentity{};
    base::Digest256 spiral6SparseIdentity{};
    base::Digest256 consumerProgramIdentity{};
    base::Digest256 proposedArtifactIdentity{};
    base::Digest256 proposedDeltaResourceIdentity{};
    base::Digest256 proposedPreviousHistoryResourceIdentity{};
    base::Digest256 proposedOutputHistoryResourceIdentity{};
    base::Digest256 transitionAuthorityIdentity{};

    DeltaLoweringKindV1 kind = DeltaLoweringKindV1::CompactDeltaIndexHistoryReuse;
    contract::DeltaRepresentationRoleV1 representationRole =
        contract::DeltaRepresentationRoleV1::CanonicalSortedIndexActionList;
    contract::DeltaOperationCodeV1 operationCode =
        contract::DeltaOperationCodeV1::ExecuteCompactDeltaHistory;
    contract::DeltaOutputHistoryPolicyV1 outputHistoryPolicy =
        contract::DeltaOutputHistoryPolicyV1::CopyPreviousHistoryThenWriteExactlyTransitionSet;
    DeltaCompletionPolicyV1 completionPolicy =
        DeltaCompletionPolicyV1::ExecuteIndirectCompletionAuthorizesExactTransitionSet;
    DeltaDeviceEpochPolicyV1 deviceEpochPolicy =
        DeltaDeviceEpochPolicyV1::HistoryAndDerivedResourcesBoundToDeviceEpoch;

    std::uint32_t invocationIndex = 0;
    std::uint32_t universeCount = 0;
    std::uint32_t transitionCount = 0;
    std::uint32_t updateCount = 0;
    std::uint32_t clearCount = 0;
    std::uint32_t retainCount = 0;
    std::uint32_t threadsPerGroup = 0;
    std::uint32_t dispatchGroupsX = 0;
    std::uint32_t recordStrideBytes = 0;
    std::uint32_t fixedCapacityBytes = 0;
    std::uint32_t historyBytes = 0;
    std::uint32_t outputCapacity = 0;
    std::uint32_t outputStrideBytes = 0;
    std::uint32_t zeroTransitionMapsToZeroDispatch = 0;

    base::Digest256 rawCandidateIdentity{};
};

[[nodiscard]] base::Digest256 ProposedDeltaTargetProfileIdentityV1();
[[nodiscard]] base::Digest256 ProposedDeltaObservationContractIdentityV1();
[[nodiscard]] base::Digest256 ProposedDeltaRecordResourceContractIdentityV1();
[[nodiscard]] base::Digest256 ProposedPreviousHistoryResourceContractIdentityV1();
[[nodiscard]] base::Digest256 ProposedOutputHistoryResourceContractIdentityV1();
[[nodiscard]] base::Digest256 ProposedDeltaCompletionContractIdentityV1();
[[nodiscard]] base::Digest256 ProposedDeltaDeviceEpochPolicyIdentityV1();

[[nodiscard]] base::Digest256 ProposedDeltaRecordResourceIdentityV1(
    const contract::FrozenCompactDeltaArtifactV1& artifact);
[[nodiscard]] base::Digest256 ProposedPreviousHistoryResourceIdentityV1(
    const semantic::SparseDeltaInvocationV1& invocation);
[[nodiscard]] base::Digest256 ProposedOutputHistoryResourceIdentityV1(
    const semantic::SparseDeltaInvocationV1& invocation);
[[nodiscard]] base::Digest256 DeltaTransitionAuthorityIdentityV1(
    const semantic::SparseTemporalDeltaSemanticV1& semantic,
    const semantic::SparseDeltaInvocationV1& invocation,
    const contract::FrozenCompactDeltaArtifactV1& artifact);

[[nodiscard]] std::vector<std::byte> SerializeRawDeltaLoweringCandidateV1(
    const RawDeltaLoweringCandidateV1& value);
[[nodiscard]] base::Digest256 RawDeltaLoweringCandidateIdentityV1(
    const RawDeltaLoweringCandidateV1& value);
} // namespace sge4_5::spiral7::candidate
