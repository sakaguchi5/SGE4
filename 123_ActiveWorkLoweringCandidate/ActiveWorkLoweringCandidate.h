#pragma once

#include "../00_Foundation/Result.h"
#include "../00_Foundation/Sha256.h"
#include "../120_ActiveWorkSemantic/ActiveWorkSemantic.h"
#include "../121_Spiral4Contracts/Spiral4Contracts.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace sge4_5::spiral4::candidate
{
enum class ActiveWorkLoweringKindV1 : std::uint32_t
{
    SingleExecuteIndirectDispatch = 1
};

enum class ActiveRangePolicyV1 : std::uint32_t
{
    PrefixZeroToActiveCount = 1
};

enum class IndirectStatePolicyV1 : std::uint32_t
{
    UavBarrierThenIndirectArgument = 1
};

enum class ZeroWorkPolicyV1 : std::uint32_t
{
    ZeroDispatchGroups = 1
};

struct RawActiveWorkLoweringCandidateV1 final
{
    base::Digest256 semanticIdentity{};
    base::Digest256 targetProfileIdentity{};
    base::Digest256 activeCountContractIdentity{};
    base::Digest256 observationContractIdentity{};
    base::Digest256 commandSignatureContractIdentity{};
    base::Digest256 producerProgramTemplateIdentity{};
    base::Digest256 consumerProgramTemplateIdentity{};
    base::Digest256 proposedArtifactIdentity{};

    ActiveWorkLoweringKindV1 kind =
        ActiveWorkLoweringKindV1::SingleExecuteIndirectDispatch;
    contract::IndirectCommandKindV1 commandKind =
        contract::IndirectCommandKindV1::Dispatch;
    contract::IndirectOperationCodeV1 operationCode =
        contract::IndirectOperationCodeV1::ExecuteDispatch;
    ActiveRangePolicyV1 activeRangePolicy =
        ActiveRangePolicyV1::PrefixZeroToActiveCount;
    IndirectStatePolicyV1 statePolicy =
        IndirectStatePolicyV1::UavBarrierThenIndirectArgument;
    ZeroWorkPolicyV1 zeroWorkPolicy =
        ZeroWorkPolicyV1::ZeroDispatchGroups;

    std::uint32_t maxWorkCount = 0;
    std::uint32_t threadsPerGroup = 0;
    std::uint32_t workRecordStride = 0;
    std::uint32_t outputRecordStride = 0;
    std::uint32_t argumentStrideBytes = 0;
    std::uint32_t argumentOffsetBytes = 0;
    std::uint32_t maxCommandCount = 0;
    std::uint32_t producerDispatchX = 0;
    std::uint32_t producerDispatchY = 0;
    std::uint32_t producerDispatchZ = 0;

    base::Digest256 rawCandidateIdentity{};
};

[[nodiscard]] base::Digest256 ProposedTargetProfileIdentityV1();
[[nodiscard]] base::Digest256 ProposedActiveCountContractIdentityV1();
[[nodiscard]] base::Digest256 ProposedObservationContractIdentityV1();
[[nodiscard]] base::Digest256 ProposedCommandSignatureContractIdentityV1();
[[nodiscard]] base::Digest256 ProposedProducerProgramTemplateIdentityV1();
[[nodiscard]] base::Digest256 ProposedConsumerProgramTemplateIdentityV1();

[[nodiscard]] std::vector<std::byte>
SerializeRawActiveWorkLoweringCandidateV1(
    const RawActiveWorkLoweringCandidateV1& value);

[[nodiscard]] base::Digest256 RawActiveWorkLoweringCandidateIdentityV1(
    const RawActiveWorkLoweringCandidateV1& value);
}
