#include "Spiral4Contracts.h"

#include "../00_Foundation/BinaryIO.h"

#include <algorithm>
#include <cstring>
#include <utility>

namespace sge4_5::spiral4::contract
{
namespace
{
constexpr std::uint32_t ArtifactMagic = 0x31493453u; // "S4I1"
constexpr std::uint16_t ArtifactVersion = 1;
constexpr std::uint16_t ArtifactKind = 1;
constexpr std::uint32_t DispatchArgumentBytes = 12;
constexpr std::size_t DigestBytes = 32;

ContractErrorV1 Error(std::string stage, std::string message)
{
    return ContractErrorV1{std::move(stage), std::move(message)};
}

void WriteDigest(base::BinaryWriter& writer, const base::Digest256& digest)
{
    writer.WriteBytes(digest);
}

base::Result<base::Digest256, ContractErrorV1> ReadDigest(base::BinaryReader& reader)
{
    auto bytes = reader.ReadBytes(DigestBytes);
    if (!bytes)
    {
        return base::Result<base::Digest256, ContractErrorV1>::Failure(
            Error("parse/digest", bytes.Error()));
    }
    base::Digest256 result{};
    std::memcpy(result.data(), bytes.Value().data(), result.size());
    return base::Result<base::Digest256, ContractErrorV1>::Success(result);
}

}

FrozenSingleIndirectArtifactV1::FrozenSingleIndirectArtifactV1(
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
    std::vector<std::byte> bytes)
    : maxWorkCount_(maxWorkCount),
      threadsPerGroup_(threadsPerGroup),
      argumentStrideBytes_(argumentStrideBytes),
      argumentOffsetBytes_(argumentOffsetBytes),
      maxCommandCount_(maxCommandCount),
      producerDispatchX_(producerDispatchX),
      producerDispatchY_(producerDispatchY),
      producerDispatchZ_(producerDispatchZ),
      commandKind_(commandKind),
      operationCode_(operationCode),
      semanticIdentity_(semanticIdentity),
      artifactIdentity_(artifactIdentity),
      bytes_(std::move(bytes))
{
}

FrozenSingleIndirectArtifactV1 BuildCu2SingleIndirectArtifactV1(
    const semantic::ActiveWorkSemanticV1& semantic)
{
    const auto semanticIdentity = semantic::ActiveWorkSemanticIdentityV1(semantic);

    base::BinaryWriter payload;
    payload.WriteU32(ArtifactMagic);
    payload.WriteU16(ArtifactVersion);
    payload.WriteU16(ArtifactKind);
    payload.WriteU32(static_cast<std::uint32_t>(IndirectCommandKindV1::Dispatch));
    payload.WriteU32(static_cast<std::uint32_t>(IndirectOperationCodeV1::ExecuteDispatch));
    payload.WriteU32(DispatchArgumentBytes);
    payload.WriteU32(0); // argument offset
    payload.WriteU32(1); // max command count
    payload.WriteU32(semantic.maxWorkCount.Value());
    payload.WriteU32(semantic.threadsPerGroup.Value());
    payload.WriteU32(1); // argument producer dispatch X
    payload.WriteU32(1);
    payload.WriteU32(1);
    payload.WriteU32(1); // fixed-capacity Buffer policy
    payload.WriteU32(1); // UAV -> INDIRECT_ARGUMENT transition policy
    payload.WriteU32(1); // zero dispatch groups encode Nf=0
    WriteDigest(payload, semanticIdentity);

    const auto payloadBytes = payload.Bytes();
    const auto artifactIdentity = base::Sha256(payloadBytes);

    base::BinaryWriter complete;
    complete.WriteBytes(payloadBytes);
    WriteDigest(complete, artifactIdentity);
    auto bytes = std::move(complete).Take();

    return FrozenSingleIndirectArtifactV1(
        semantic.maxWorkCount.Value(),
        semantic.threadsPerGroup.Value(),
        DispatchArgumentBytes,
        0,
        1,
        1,
        1,
        1,
        IndirectCommandKindV1::Dispatch,
        IndirectOperationCodeV1::ExecuteDispatch,
        semanticIdentity,
        artifactIdentity,
        std::move(bytes));
}

base::Result<FrozenSingleIndirectArtifactV1, ContractErrorV1>
ParseSingleIndirectArtifactV1(std::span<const std::byte> bytes)
{
    constexpr std::size_t PayloadBytes =
        4 + 2 + 2 + (13 * 4) + DigestBytes;
    constexpr std::size_t CompleteBytes = PayloadBytes + DigestBytes;

    if (bytes.size() != CompleteBytes)
    {
        return base::Result<FrozenSingleIndirectArtifactV1, ContractErrorV1>::Failure(
            Error("parse/size", "Single indirect artifact has a non-canonical byte size."));
    }

    base::BinaryReader reader(bytes);

    auto magic = reader.ReadU32();
    auto version = reader.ReadU16();
    auto artifactKind = reader.ReadU16();
    auto commandKind = reader.ReadU32();
    auto operationCode = reader.ReadU32();
    auto argumentStride = reader.ReadU32();
    auto argumentOffset = reader.ReadU32();
    auto maxCommandCount = reader.ReadU32();
    auto maxWorkCount = reader.ReadU32();
    auto threadsPerGroup = reader.ReadU32();
    auto producerX = reader.ReadU32();
    auto producerY = reader.ReadU32();
    auto producerZ = reader.ReadU32();
    auto capacityPolicy = reader.ReadU32();
    auto transitionPolicy = reader.ReadU32();
    auto zeroPolicy = reader.ReadU32();

    if (!magic || !version || !artifactKind || !commandKind || !operationCode ||
        !argumentStride || !argumentOffset || !maxCommandCount || !maxWorkCount ||
        !threadsPerGroup || !producerX || !producerY || !producerZ ||
        !capacityPolicy || !transitionPolicy || !zeroPolicy)
    {
        return base::Result<FrozenSingleIndirectArtifactV1, ContractErrorV1>::Failure(
            Error("parse/header", "Single indirect artifact header is truncated."));
    }

    auto semanticIdentity = ReadDigest(reader);
    auto storedArtifactIdentity = ReadDigest(reader);
    if (!semanticIdentity || !storedArtifactIdentity || reader.Remaining() != 0)
    {
        return base::Result<FrozenSingleIndirectArtifactV1, ContractErrorV1>::Failure(
            Error("parse/tail", "Single indirect artifact digest tail is invalid."));
    }

    const auto calculatedIdentity = base::Sha256(bytes.first(PayloadBytes));
    if (calculatedIdentity != storedArtifactIdentity.Value())
    {
        return base::Result<FrozenSingleIndirectArtifactV1, ContractErrorV1>::Failure(
            Error("parse/identity", "Single indirect artifact digest mismatch."));
    }

    const bool canonical =
        magic.Value() == ArtifactMagic &&
        version.Value() == ArtifactVersion &&
        artifactKind.Value() == ArtifactKind &&
        commandKind.Value() == static_cast<std::uint32_t>(IndirectCommandKindV1::Dispatch) &&
        operationCode.Value() == static_cast<std::uint32_t>(IndirectOperationCodeV1::ExecuteDispatch) &&
        argumentStride.Value() == DispatchArgumentBytes &&
        argumentOffset.Value() == 0 &&
        maxCommandCount.Value() == 1 &&
        maxWorkCount.Value() == 4096 &&
        threadsPerGroup.Value() == 64 &&
        producerX.Value() == 1 &&
        producerY.Value() == 1 &&
        producerZ.Value() == 1 &&
        capacityPolicy.Value() == 1 &&
        transitionPolicy.Value() == 1 &&
        zeroPolicy.Value() == 1;

    if (!canonical)
    {
        return base::Result<FrozenSingleIndirectArtifactV1, ContractErrorV1>::Failure(
            Error("parse/canonical", "Single indirect artifact contains a non-canonical field."));
    }

    std::vector<std::byte> owned(bytes.begin(), bytes.end());
    return base::Result<FrozenSingleIndirectArtifactV1, ContractErrorV1>::Success(
        FrozenSingleIndirectArtifactV1(
            maxWorkCount.Value(),
            threadsPerGroup.Value(),
            argumentStride.Value(),
            argumentOffset.Value(),
            maxCommandCount.Value(),
            producerX.Value(),
            producerY.Value(),
            producerZ.Value(),
            IndirectCommandKindV1::Dispatch,
            IndirectOperationCodeV1::ExecuteDispatch,
            semanticIdentity.Value(),
            storedArtifactIdentity.Value(),
            std::move(owned)));
}

base::Result<void, ContractErrorV1>
ValidateSingleIndirectArtifactForExecutionV1(
    const FrozenSingleIndirectArtifactV1& artifact,
    const semantic::ActiveWorkSemanticV1& semantic)
{
    if (artifact.Bytes().empty())
    {
        return base::Result<void, ContractErrorV1>::Failure(
            Error("execution/bytes", "Frozen single indirect artifact has no bytes."));
    }

    auto parsed = ParseSingleIndirectArtifactV1(artifact.Bytes());
    if (!parsed)
    {
        return base::Result<void, ContractErrorV1>::Failure(parsed.Error());
    }

    const auto expectedSemanticIdentity = semantic::ActiveWorkSemanticIdentityV1(semantic);
    if (artifact.SemanticIdentity() != expectedSemanticIdentity ||
        parsed.Value().SemanticIdentity() != expectedSemanticIdentity)
    {
        return base::Result<void, ContractErrorV1>::Failure(
            Error("execution/semantic", "Frozen artifact is bound to another Active Work Semantic."));
    }

    if (artifact.ArtifactIdentity() != parsed.Value().ArtifactIdentity())
    {
        return base::Result<void, ContractErrorV1>::Failure(
            Error("execution/identity", "Frozen artifact object does not match its canonical bytes."));
    }

    if (artifact.MaxWorkCount() != semantic.maxWorkCount.Value() ||
        artifact.ThreadsPerGroup() != semantic.threadsPerGroup.Value() ||
        artifact.ArgumentStrideBytes() != DispatchArgumentBytes ||
        artifact.ArgumentOffsetBytes() != 0 ||
        artifact.MaxCommandCount() != 1 ||
        artifact.CommandKind() != IndirectCommandKindV1::Dispatch ||
        artifact.OperationCode() != IndirectOperationCodeV1::ExecuteDispatch)
    {
        return base::Result<void, ContractErrorV1>::Failure(
            Error("execution/contract", "Frozen artifact execution fields are not canonical."));
    }

    return base::Result<void, ContractErrorV1>::Success();
}
}
