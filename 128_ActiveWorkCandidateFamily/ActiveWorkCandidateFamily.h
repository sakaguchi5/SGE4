#pragma once

#include "../00_Foundation/Sha256.h"
#include "../127_Spiral4CandidateFamilyContracts/CandidateFamilyContracts.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace sge4_5::spiral4::family_candidate
{
struct RawCandidateFamilyV1 final
{
    base::Digest256 semanticIdentity{};
    base::Digest256 targetProfileIdentity{};
    base::Digest256 activeCountContractIdentity{};
    base::Digest256 observationContractIdentity{};
    base::Digest256 commandSignatureContractIdentity{};
    base::Digest256 producerProgramTemplateIdentity{};
    base::Digest256 consumerProgramTemplateIdentity{};
    base::Digest256 proposedArtifactIdentity{};

    family_contract::CandidateFamilyKindV1 kind =
        family_contract::CandidateFamilyKindV1::FixedMaximumGuarded;
    family_contract::CommandSignaturePolicyV1 commandSignaturePolicy =
        family_contract::CommandSignaturePolicyV1::None;
    family_contract::IssuancePolicyV1 issuancePolicy =
        family_contract::IssuancePolicyV1::FixedDirectDispatch;

    std::uint32_t batchSize = 0;
    std::uint32_t maxBatchSlots = 0;
    std::uint32_t maxWorkCount = 0;
    std::uint32_t threadsPerGroup = 0;
    std::uint32_t directDispatchX = 0;
    std::uint32_t argumentStrideBytes = 0;
    std::uint32_t argumentBufferBytes = 0;
    std::uint32_t maxCommandCount = 0;
    std::uint32_t producerDispatchX = 0;

    base::Digest256 rawCandidateIdentity{};
};

[[nodiscard]] base::Digest256 ProposedFamilyTargetProfileIdentityV1();
[[nodiscard]] base::Digest256 ProposedFamilyActiveCountContractIdentityV1();
[[nodiscard]] base::Digest256 ProposedFamilyObservationContractIdentityV1();
[[nodiscard]] base::Digest256 ProposedCommandSignatureContractIdentityV1(
    family_contract::CandidateFamilyKindV1 kind,
    std::uint32_t batchSize);
[[nodiscard]] base::Digest256 ProposedProducerProgramTemplateIdentityV1(
    family_contract::CandidateFamilyKindV1 kind,
    std::uint32_t batchSize);
[[nodiscard]] base::Digest256 ProposedConsumerProgramTemplateIdentityV1();

[[nodiscard]] std::vector<std::byte>
SerializeRawCandidateFamilyV1(const RawCandidateFamilyV1& value);

[[nodiscard]] base::Digest256 RawCandidateFamilyIdentityV1(
    const RawCandidateFamilyV1& value);
}
