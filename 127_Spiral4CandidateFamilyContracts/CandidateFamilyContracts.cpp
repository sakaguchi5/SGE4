#include "CandidateFamilyContracts.h"

#include "../00_Foundation/BinaryIO.h"
#include "../121_Spiral4Contracts/Spiral4Contracts.h"

#include <algorithm>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace sge4_5::spiral4::family_contract
{
namespace
{
base::Digest256 LiteralIdentity(std::string_view value)
{
    return base::Sha256(std::as_bytes(std::span(value.data(), value.size())));
}
CandidateFamilyContractErrorV1 Error(std::string stage, std::string message)
{
    return CandidateFamilyContractErrorV1{std::move(stage), std::move(message)};
}
std::uint32_t Groups(std::uint32_t count, std::uint32_t groupSize) noexcept
{
    return count == 0 ? 0u : 1u + ((count - 1u) / groupSize);
}
}

BatchSizeV1::BatchSizeV1(std::uint32_t value) : value_(value)
{
    if (!IsQualifiedBatchSizeV1(value))
    {
        throw std::invalid_argument("BatchSizeV1 is not in the Spiral 4 V1 corpus.");
    }
}

FrozenCandidateFamilyArtifactV1::FrozenCandidateFamilyArtifactV1(
    CandidateFamilyKindV1 kind,
    CommandSignaturePolicyV1 commandSignaturePolicy,
    IssuancePolicyV1 issuancePolicy,
    std::uint32_t batchSize,
    std::uint32_t maxBatchSlots,
    std::uint32_t maxWorkCount,
    std::uint32_t threadsPerGroup,
    std::uint32_t directDispatchX,
    std::uint32_t argumentStrideBytes,
    std::uint32_t argumentBufferBytes,
    std::uint32_t maxCommandCount,
    std::uint32_t producerDispatchX,
    base::Digest256 semanticIdentity,
    base::Digest256 cu2SingleArtifactIdentity,
    base::Digest256 artifactIdentity,
    std::vector<std::byte> bytes)
    : kind_(kind),
      commandSignaturePolicy_(commandSignaturePolicy),
      issuancePolicy_(issuancePolicy),
      batchSize_(batchSize),
      maxBatchSlots_(maxBatchSlots),
      maxWorkCount_(maxWorkCount),
      threadsPerGroup_(threadsPerGroup),
      directDispatchX_(directDispatchX),
      argumentStrideBytes_(argumentStrideBytes),
      argumentBufferBytes_(argumentBufferBytes),
      maxCommandCount_(maxCommandCount),
      producerDispatchX_(producerDispatchX),
      semanticIdentity_(semanticIdentity),
      cu2SingleArtifactIdentity_(cu2SingleArtifactIdentity),
      artifactIdentity_(artifactIdentity),
      bytes_(std::move(bytes))
{
}

bool IsQualifiedBatchSizeV1(std::uint32_t value) noexcept
{
    switch (value)
    {
    case 64:
    case 128:
    case 256:
    case 512:
    case 1024:
        return true;
    default:
        return false;
    }
}

const std::vector<std::uint32_t>& QualifiedBatchSizesV1() noexcept
{
    static const std::vector<std::uint32_t> values{
        64u, 128u, 256u, 512u, 1024u
    };
    return values;
}

base::Result<FrozenCandidateFamilyArtifactV1, CandidateFamilyContractErrorV1>
BuildCandidateFamilyArtifactV1(
    const semantic::ActiveWorkSemanticV1& semantic,
    CandidateFamilyKindV1 kind,
    std::uint32_t batchSize)
{
    try
    {
        const auto semanticIdentity =
            semantic::ActiveWorkSemanticIdentityV1(semantic);
        const auto cu2Single =
            contract::BuildCu2SingleIndirectArtifactV1(semantic);

        CommandSignaturePolicyV1 signature = CommandSignaturePolicyV1::None;
        IssuancePolicyV1 issuance = IssuancePolicyV1::FixedDirectDispatch;
        std::uint32_t canonicalBatchSize = 0;
        std::uint32_t maxBatchSlots = 0;
        std::uint32_t directDispatchX = 0;
        std::uint32_t argumentStrideBytes = 0;
        std::uint32_t argumentBufferBytes = 0;
        std::uint32_t maxCommandCount = 0;
        std::uint32_t producerDispatchX = 0;
        base::Digest256 cu2SingleIdentity{};

        switch (kind)
        {
        case CandidateFamilyKindV1::FixedMaximumGuarded:
            if (batchSize != 0)
            {
                return base::Result<FrozenCandidateFamilyArtifactV1, CandidateFamilyContractErrorV1>::Failure(
                    Error("artifact/fixed", "Fixed Maximum must not carry Batch size."));
            }
            directDispatchX = Groups(
                semantic.maxWorkCount.Value(),
                semantic.threadsPerGroup.Value());
            break;

        case CandidateFamilyKindV1::SingleExecuteIndirectDispatch:
            if (batchSize != 0)
            {
                return base::Result<FrozenCandidateFamilyArtifactV1, CandidateFamilyContractErrorV1>::Failure(
                    Error("artifact/single", "Single Indirect must not carry Batch size."));
            }
            signature = CommandSignaturePolicyV1::DispatchOnly;
            issuance = IssuancePolicyV1::SingleGpuGeneratedIndirect;
            maxBatchSlots = 1;
            argumentStrideBytes = 12;
            argumentBufferBytes = 12;
            maxCommandCount = 1;
            producerDispatchX = 1;
            cu2SingleIdentity = cu2Single.ArtifactIdentity();
            break;

        case CandidateFamilyKindV1::BatchedExecuteIndirectDispatch:
            if (!IsQualifiedBatchSizeV1(batchSize))
            {
                return base::Result<FrozenCandidateFamilyArtifactV1, CandidateFamilyContractErrorV1>::Failure(
                    Error("artifact/batch", "Batched Indirect Batch size is not qualified."));
            }
            signature = CommandSignaturePolicyV1::RootConstantThenDispatch;
            issuance = IssuancePolicyV1::FixedSlotGpuGeneratedBatchIndirect;
            canonicalBatchSize = batchSize;
            maxBatchSlots = semantic.maxWorkCount.Value() / canonicalBatchSize;
            argumentStrideBytes = 16;
            argumentBufferBytes = maxBatchSlots * argumentStrideBytes;
            maxCommandCount = maxBatchSlots;
            producerDispatchX = Groups(maxBatchSlots, 64);
            break;

        default:
            return base::Result<FrozenCandidateFamilyArtifactV1, CandidateFamilyContractErrorV1>::Failure(
                Error("artifact/kind", "Unknown Candidate family kind."));
        }

        base::BinaryWriter writer;
        writer.WriteBytes(LiteralIdentity(
            "SGE4-5.Spiral4.FrozenCandidateFamilyArtifact.V1"));
        writer.WriteBytes(semanticIdentity);
        writer.WriteU32(static_cast<std::uint32_t>(kind));
        writer.WriteU32(static_cast<std::uint32_t>(signature));
        writer.WriteU32(static_cast<std::uint32_t>(issuance));
        writer.WriteU32(canonicalBatchSize);
        writer.WriteU32(maxBatchSlots);
        writer.WriteU32(semantic.maxWorkCount.Value());
        writer.WriteU32(semantic.threadsPerGroup.Value());
        writer.WriteU32(directDispatchX);
        writer.WriteU32(argumentStrideBytes);
        writer.WriteU32(argumentBufferBytes);
        writer.WriteU32(maxCommandCount);
        writer.WriteU32(producerDispatchX);
        writer.WriteBytes(cu2SingleIdentity);
        auto bytes = std::move(writer).Take();
        const auto identity = base::Sha256(bytes);

        return base::Result<FrozenCandidateFamilyArtifactV1, CandidateFamilyContractErrorV1>::Success(
            FrozenCandidateFamilyArtifactV1(
                kind, signature, issuance, canonicalBatchSize, maxBatchSlots,
                semantic.maxWorkCount.Value(), semantic.threadsPerGroup.Value(),
                directDispatchX, argumentStrideBytes, argumentBufferBytes,
                maxCommandCount, producerDispatchX, semanticIdentity,
                cu2SingleIdentity, identity, std::move(bytes)));
    }
    catch (const std::exception& exception)
    {
        return base::Result<FrozenCandidateFamilyArtifactV1, CandidateFamilyContractErrorV1>::Failure(
            Error("artifact/exception", exception.what()));
    }
}

BatchDispatchRecordV1 ExpectedBatchDispatchRecordV1(
    semantic::ActiveWorkCountV1 activeCount,
    semantic::ThreadGroupSizeV1 threadsPerGroup,
    BatchSizeV1 batchSize,
    std::uint32_t slot) noexcept
{
    const std::uint32_t firstWork = slot * batchSize.Value();
    const std::uint32_t remaining =
        firstWork < activeCount.Value() ? activeCount.Value() - firstWork : 0u;
    const std::uint32_t workCount = std::min(batchSize.Value(), remaining);
    return BatchDispatchRecordV1{
        firstWork,
        Groups(workCount, threadsPerGroup.Value()),
        1u,
        1u
    };
}

std::vector<BatchDispatchRecordV1> BuildExpectedBatchDispatchRecordsV1(
    semantic::ActiveWorkCountV1 activeCount,
    semantic::ThreadGroupSizeV1 threadsPerGroup,
    BatchSizeV1 batchSize,
    std::uint32_t maxBatchSlots)
{
    std::vector<BatchDispatchRecordV1> records;
    records.reserve(maxBatchSlots);
    for (std::uint32_t slot = 0; slot < maxBatchSlots; ++slot)
    {
        records.push_back(ExpectedBatchDispatchRecordV1(
            activeCount, threadsPerGroup, batchSize, slot));
    }
    return records;
}

base::Result<void, CandidateFamilyContractErrorV1> ValidateBatchPartitionV1(
    semantic::ActiveWorkCountV1 activeCount,
    semantic::ThreadGroupSizeV1 threadsPerGroup,
    BatchSizeV1 batchSize,
    std::span<const BatchDispatchRecordV1> records)
{
    if (records.empty())
    {
        return base::Result<void, CandidateFamilyContractErrorV1>::Failure(
            Error("partition/empty", "Batch record list is empty."));
    }

    std::uint32_t covered = 0;
    for (std::uint32_t slot = 0; slot < records.size(); ++slot)
    {
        const auto expected = ExpectedBatchDispatchRecordV1(
            activeCount, threadsPerGroup, batchSize, slot);
        if (!(records[slot] == expected))
        {
            return base::Result<void, CandidateFamilyContractErrorV1>::Failure(
                Error("partition/record", "Batch record differs from the canonical partition."));
        }
        const std::uint32_t first = records[slot].firstWork;
        const std::uint32_t remaining =
            first < activeCount.Value() ? activeCount.Value() - first : 0u;
        covered += std::min(batchSize.Value(), remaining);
    }

    if (covered != activeCount.Value())
    {
        return base::Result<void, CandidateFamilyContractErrorV1>::Failure(
            Error("partition/coverage", "Batch partition does not cover the active prefix exactly once."));
    }
    return base::Result<void, CandidateFamilyContractErrorV1>::Success();
}
}
