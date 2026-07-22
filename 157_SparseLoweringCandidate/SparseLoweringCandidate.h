#pragma once

#include "../00_Foundation/Sha256.h"
#include "../154_SparseWorkSetSemantic/SparseWorkSetSemantic.h"
#include "../155_Spiral6SparseContracts/Spiral6SparseContracts.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace sge4_5::spiral6::candidate
{
enum class SparseLoweringKindV1 : std::uint32_t
{
    CompactIndexListDispatch = 2
};

enum class SparseCompletionPolicyV1 : std::uint32_t
{
    ExecuteIndirectCompletionAuthorizesExactWriteSet = 1
};

enum class SparseDeviceEpochPolicyV1 : std::uint32_t
{
    DerivedResourceBoundToDeviceEpoch = 1
};

struct RawSparseLoweringCandidateV1 final
{
    base::Digest256 semanticIdentity{};
    base::Digest256 sparseSetIdentity{};
    base::Digest256 targetProfileIdentity{};
    base::Digest256 observationContractIdentity{};
    base::Digest256 indexResourceContractIdentity{};
    base::Digest256 completionContractIdentity{};
    base::Digest256 deviceEpochPolicyIdentity{};
    base::Digest256 spiral4IndirectArtifactIdentity{};
    base::Digest256 consumerProgramIdentity{};
    base::Digest256 proposedArtifactIdentity{};
    base::Digest256 proposedIndexResourceIdentity{};
    base::Digest256 writeSetAuthorityIdentity{};

    SparseLoweringKindV1 kind = SparseLoweringKindV1::CompactIndexListDispatch;
    contract::SparseRepresentationRoleV1 representationRole =
        contract::SparseRepresentationRoleV1::CompactSortedUniverseIndexList;
    contract::SparseOperationCodeV1 operationCode =
        contract::SparseOperationCodeV1::ExecuteCompactIndexList;
    contract::SparseOutputInitializationV1 outputInitialization =
        contract::SparseOutputInitializationV1::CanonicalFloat4Zero;
    contract::SparseWriteSetPolicyV1 writeSetPolicy =
        contract::SparseWriteSetPolicyV1::ExactlyVerifiedSparseSet;
    SparseCompletionPolicyV1 completionPolicy =
        SparseCompletionPolicyV1::ExecuteIndirectCompletionAuthorizesExactWriteSet;
    SparseDeviceEpochPolicyV1 deviceEpochPolicy =
        SparseDeviceEpochPolicyV1::DerivedResourceBoundToDeviceEpoch;

    std::uint32_t universeCount = 0;
    std::uint32_t activeCount = 0;
    std::uint32_t threadsPerGroup = 0;
    std::uint32_t dispatchGroupsX = 0;
    std::uint32_t indexStrideBytes = 0;
    std::uint32_t fixedCapacityBytes = 0;
    std::uint32_t outputCapacity = 0;
    std::uint32_t outputStrideBytes = 0;
    std::uint32_t unusedIndexSentinel = 0;
    std::uint32_t zeroActiveMapsToZeroDispatch = 0;

    base::Digest256 rawCandidateIdentity{};
};

[[nodiscard]] base::Digest256 ProposedSparseTargetProfileIdentityV1();
[[nodiscard]] base::Digest256 ProposedSparseObservationContractIdentityV1();
[[nodiscard]] base::Digest256 ProposedCompactIndexResourceContractIdentityV1();
[[nodiscard]] base::Digest256 ProposedSparseCompletionContractIdentityV1();
[[nodiscard]] base::Digest256 ProposedSparseDeviceEpochPolicyIdentityV1();
[[nodiscard]] base::Digest256 ProposedCompactIndexResourceIdentityV1(
    const contract::FrozenCompactIndexListArtifactV1& artifact);
[[nodiscard]] base::Digest256 SparseWriteSetAuthorityIdentityV1(
    const semantic::SparsePointTransformSemanticV1& semantic,
    const semantic::ExactSparseWorkSetV1& set,
    const contract::FrozenCompactIndexListArtifactV1& artifact);

[[nodiscard]] std::vector<std::byte> SerializeRawSparseLoweringCandidateV1(
    const RawSparseLoweringCandidateV1& value);
[[nodiscard]] base::Digest256 RawSparseLoweringCandidateIdentityV1(
    const RawSparseLoweringCandidateV1& value);
}
