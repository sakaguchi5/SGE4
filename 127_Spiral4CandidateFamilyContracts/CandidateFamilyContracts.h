#pragma once

#include "../00_Foundation/Result.h"
#include "../00_Foundation/Sha256.h"
#include "../120_ActiveWorkSemantic/ActiveWorkSemantic.h"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace sge4_5::spiral4::family_contract
{
enum class CandidateFamilyKindV1 : std::uint32_t
{
    FixedMaximumGuarded = 1,
    SingleExecuteIndirectDispatch = 2,
    BatchedExecuteIndirectDispatch = 3
};

enum class CommandSignaturePolicyV1 : std::uint32_t
{
    None = 0,
    DispatchOnly = 1,
    RootConstantThenDispatch = 2
};

enum class IssuancePolicyV1 : std::uint32_t
{
    FixedDirectDispatch = 1,
    SingleGpuGeneratedIndirect = 2,
    FixedSlotGpuGeneratedBatchIndirect = 3
};

class BatchSizeV1 final
{
public:
    explicit BatchSizeV1(std::uint32_t value);
    [[nodiscard]] std::uint32_t Value() const noexcept { return value_; }
    auto operator<=>(const BatchSizeV1&) const = default;
private:
    std::uint32_t value_ = 0;
};

struct BatchDispatchRecordV1 final
{
    std::uint32_t firstWork = 0;
    std::uint32_t threadGroupCountX = 0;
    std::uint32_t threadGroupCountY = 1;
    std::uint32_t threadGroupCountZ = 1;
    bool operator==(const BatchDispatchRecordV1&) const = default;
};

struct CandidateFamilyContractErrorV1 final
{
    std::string stage;
    std::string message;
};

class FrozenCandidateFamilyArtifactV1 final
{
public:
    FrozenCandidateFamilyArtifactV1() = delete;
    FrozenCandidateFamilyArtifactV1(
        const FrozenCandidateFamilyArtifactV1&) = default;
    FrozenCandidateFamilyArtifactV1& operator=(
        const FrozenCandidateFamilyArtifactV1&) = default;

    [[nodiscard]] CandidateFamilyKindV1 Kind() const noexcept { return kind_; }
    [[nodiscard]] CommandSignaturePolicyV1 CommandSignaturePolicy() const noexcept { return commandSignaturePolicy_; }
    [[nodiscard]] IssuancePolicyV1 IssuancePolicy() const noexcept { return issuancePolicy_; }
    [[nodiscard]] std::uint32_t BatchSize() const noexcept { return batchSize_; }
    [[nodiscard]] std::uint32_t MaxBatchSlots() const noexcept { return maxBatchSlots_; }
    [[nodiscard]] std::uint32_t MaxWorkCount() const noexcept { return maxWorkCount_; }
    [[nodiscard]] std::uint32_t ThreadsPerGroup() const noexcept { return threadsPerGroup_; }
    [[nodiscard]] std::uint32_t DirectDispatchX() const noexcept { return directDispatchX_; }
    [[nodiscard]] std::uint32_t ArgumentStrideBytes() const noexcept { return argumentStrideBytes_; }
    [[nodiscard]] std::uint32_t ArgumentBufferBytes() const noexcept { return argumentBufferBytes_; }
    [[nodiscard]] std::uint32_t MaxCommandCount() const noexcept { return maxCommandCount_; }
    [[nodiscard]] std::uint32_t ProducerDispatchX() const noexcept { return producerDispatchX_; }
    [[nodiscard]] const base::Digest256& SemanticIdentity() const noexcept { return semanticIdentity_; }
    [[nodiscard]] const base::Digest256& Cu2SingleArtifactIdentity() const noexcept { return cu2SingleArtifactIdentity_; }
    [[nodiscard]] const base::Digest256& ArtifactIdentity() const noexcept { return artifactIdentity_; }
    [[nodiscard]] const std::vector<std::byte>& Bytes() const noexcept { return bytes_; }

private:
    friend base::Result<FrozenCandidateFamilyArtifactV1, CandidateFamilyContractErrorV1>
    BuildCandidateFamilyArtifactV1(
        const semantic::ActiveWorkSemanticV1&,
        CandidateFamilyKindV1,
        std::uint32_t);

    FrozenCandidateFamilyArtifactV1(
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
        std::vector<std::byte> bytes);

    CandidateFamilyKindV1 kind_ = CandidateFamilyKindV1::FixedMaximumGuarded;
    CommandSignaturePolicyV1 commandSignaturePolicy_ = CommandSignaturePolicyV1::None;
    IssuancePolicyV1 issuancePolicy_ = IssuancePolicyV1::FixedDirectDispatch;
    std::uint32_t batchSize_ = 0;
    std::uint32_t maxBatchSlots_ = 0;
    std::uint32_t maxWorkCount_ = 0;
    std::uint32_t threadsPerGroup_ = 0;
    std::uint32_t directDispatchX_ = 0;
    std::uint32_t argumentStrideBytes_ = 0;
    std::uint32_t argumentBufferBytes_ = 0;
    std::uint32_t maxCommandCount_ = 0;
    std::uint32_t producerDispatchX_ = 0;
    base::Digest256 semanticIdentity_{};
    base::Digest256 cu2SingleArtifactIdentity_{};
    base::Digest256 artifactIdentity_{};
    std::vector<std::byte> bytes_;
};

[[nodiscard]] bool IsQualifiedBatchSizeV1(std::uint32_t value) noexcept;
[[nodiscard]] const std::vector<std::uint32_t>& QualifiedBatchSizesV1() noexcept;

[[nodiscard]] base::Result<
    FrozenCandidateFamilyArtifactV1,
    CandidateFamilyContractErrorV1>
BuildCandidateFamilyArtifactV1(
    const semantic::ActiveWorkSemanticV1& semantic,
    CandidateFamilyKindV1 kind,
    std::uint32_t batchSize);

[[nodiscard]] BatchDispatchRecordV1 ExpectedBatchDispatchRecordV1(
    semantic::ActiveWorkCountV1 activeCount,
    semantic::ThreadGroupSizeV1 threadsPerGroup,
    BatchSizeV1 batchSize,
    std::uint32_t slot) noexcept;

[[nodiscard]] std::vector<BatchDispatchRecordV1>
BuildExpectedBatchDispatchRecordsV1(
    semantic::ActiveWorkCountV1 activeCount,
    semantic::ThreadGroupSizeV1 threadsPerGroup,
    BatchSizeV1 batchSize,
    std::uint32_t maxBatchSlots);

[[nodiscard]] base::Result<void, CandidateFamilyContractErrorV1>
ValidateBatchPartitionV1(
    semantic::ActiveWorkCountV1 activeCount,
    semantic::ThreadGroupSizeV1 threadsPerGroup,
    BatchSizeV1 batchSize,
    std::span<const BatchDispatchRecordV1> records);
}
