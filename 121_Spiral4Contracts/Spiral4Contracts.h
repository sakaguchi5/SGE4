#pragma once

#include "../00_Foundation/Result.h"
#include "../00_Foundation/Sha256.h"
#include "../120_ActiveWorkSemantic/ActiveWorkSemantic.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace sge4_5::spiral4::contract
{
enum class IndirectCommandKindV1 : std::uint16_t
{
    Dispatch = 1
};

enum class IndirectOperationCodeV1 : std::uint32_t
{
    ExecuteDispatch = 0x0004'0201u
};

struct ContractErrorV1 final
{
    std::string stage;
    std::string message;
};

class FrozenSingleIndirectArtifactV1 final
{
public:
    FrozenSingleIndirectArtifactV1() = delete;
    FrozenSingleIndirectArtifactV1(const FrozenSingleIndirectArtifactV1&) = default;
    FrozenSingleIndirectArtifactV1& operator=(const FrozenSingleIndirectArtifactV1&) = default;

    [[nodiscard]] std::uint32_t MaxWorkCount() const noexcept { return maxWorkCount_; }
    [[nodiscard]] std::uint32_t ThreadsPerGroup() const noexcept { return threadsPerGroup_; }
    [[nodiscard]] std::uint32_t ArgumentStrideBytes() const noexcept { return argumentStrideBytes_; }
    [[nodiscard]] std::uint32_t ArgumentOffsetBytes() const noexcept { return argumentOffsetBytes_; }
    [[nodiscard]] std::uint32_t MaxCommandCount() const noexcept { return maxCommandCount_; }
    [[nodiscard]] std::uint32_t ProducerDispatchX() const noexcept { return producerDispatchX_; }
    [[nodiscard]] std::uint32_t ProducerDispatchY() const noexcept { return producerDispatchY_; }
    [[nodiscard]] std::uint32_t ProducerDispatchZ() const noexcept { return producerDispatchZ_; }
    [[nodiscard]] IndirectCommandKindV1 CommandKind() const noexcept { return commandKind_; }
    [[nodiscard]] IndirectOperationCodeV1 OperationCode() const noexcept { return operationCode_; }
    [[nodiscard]] const base::Digest256& SemanticIdentity() const noexcept { return semanticIdentity_; }
    [[nodiscard]] const base::Digest256& ArtifactIdentity() const noexcept { return artifactIdentity_; }
    [[nodiscard]] const std::vector<std::byte>& Bytes() const noexcept { return bytes_; }

private:
    friend FrozenSingleIndirectArtifactV1 BuildCu2SingleIndirectArtifactV1(
        const semantic::ActiveWorkSemanticV1&);
    friend base::Result<FrozenSingleIndirectArtifactV1, ContractErrorV1>
        ParseSingleIndirectArtifactV1(std::span<const std::byte>);

    FrozenSingleIndirectArtifactV1(
        std::uint32_t maxWorkCount,
        std::uint32_t threadsPerGroup,
        std::uint32_t argumentStrideBytes,
        std::uint32_t argumentOffsetBytes,
        std::uint32_t maxCommandCount,
        std::uint32_t producerDispatchX,
        std::uint32_t producerDispatchY,
        std::uint32_t producerDispatchZ,
        IndirectCommandKindV1 commandKind,
        IndirectOperationCodeV1 operationCode,
        base::Digest256 semanticIdentity,
        base::Digest256 artifactIdentity,
        std::vector<std::byte> bytes);

    std::uint32_t maxWorkCount_ = 0;
    std::uint32_t threadsPerGroup_ = 0;
    std::uint32_t argumentStrideBytes_ = 0;
    std::uint32_t argumentOffsetBytes_ = 0;
    std::uint32_t maxCommandCount_ = 0;
    std::uint32_t producerDispatchX_ = 0;
    std::uint32_t producerDispatchY_ = 0;
    std::uint32_t producerDispatchZ_ = 0;
    IndirectCommandKindV1 commandKind_ = IndirectCommandKindV1::Dispatch;
    IndirectOperationCodeV1 operationCode_ = IndirectOperationCodeV1::ExecuteDispatch;
    base::Digest256 semanticIdentity_{};
    base::Digest256 artifactIdentity_{};
    std::vector<std::byte> bytes_;
};

[[nodiscard]] FrozenSingleIndirectArtifactV1 BuildCu2SingleIndirectArtifactV1(
    const semantic::ActiveWorkSemanticV1& semantic);

[[nodiscard]] base::Result<FrozenSingleIndirectArtifactV1, ContractErrorV1>
ParseSingleIndirectArtifactV1(std::span<const std::byte> bytes);

[[nodiscard]] base::Result<void, ContractErrorV1>
ValidateSingleIndirectArtifactForExecutionV1(
    const FrozenSingleIndirectArtifactV1& artifact,
    const semantic::ActiveWorkSemanticV1& semantic);
}
