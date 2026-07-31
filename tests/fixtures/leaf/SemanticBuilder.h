#pragma once

#include "../../../src/canonical/base/Expected.h"
#include "../../../src/leaf/model/semantic/SemanticModel.h"

#include <span>
#include <string>

namespace sge4::semantic
{
class SemanticBuilder final
{
public:
    [[nodiscard]] base::Expected<ResourceId, std::string> AddImmutableBuffer(
        std::string debugName,
        std::uint32_t strideBytes,
        std::span<const std::byte> initialContent);

    [[nodiscard]] base::Expected<ResourceId, std::string> AddExternalBuffer(
        std::string debugName,
        std::uint64_t sizeBytes,
        std::uint32_t strideBytes);

    [[nodiscard]] base::Expected<ResourceId, std::string> AddPreparationBuffer(
        std::string debugName,
        std::uint32_t strideBytes,
        std::span<const std::byte> initialContent);

    [[nodiscard]] base::Expected<ResourceId, std::string> AddImmutableTexture2D(
        std::string debugName,
        std::uint32_t width,
        std::uint32_t height,
        FormatMeaning formatMeaning,
        std::uint32_t rowBytes,
        std::span<const std::byte> initialContent);

    [[nodiscard]] base::Expected<ResourceId, std::string> AddDepthAttachmentTexture2D(
        std::string debugName,
        FormatMeaning formatMeaning);

    [[nodiscard]] base::Expected<ResourceId, std::string> AddTemporalGpuWrittenBuffer(
        std::string debugName,
        std::uint32_t strideBytes,
        std::span<const std::byte> initialContent);

    [[nodiscard]] base::Expected<ResourceId, std::string> AddPersistentGpuWrittenBuffer(
        std::string debugName,
        std::uint64_t sizeBytes,
        std::uint32_t strideBytes);

    [[nodiscard]] base::Expected<ResourceId, std::string> AddGpuWrittenBuffer(
        std::string debugName,
        std::uint64_t sizeBytes,
        std::uint32_t strideBytes);

    [[nodiscard]] base::Expected<ResourceId, std::string> AddDynamicBuffer(
        std::string debugName,
        std::uint64_t requiredBytes,
        std::uint32_t strideBytes,
        std::uint32_t requiredAlignment);

    [[nodiscard]] base::Expected<ResourceId, std::string> AddPresentationSurface(
        std::string debugName,
        FormatMeaning formatMeaning);

    [[nodiscard]] base::Expected<void, std::string> SetAliasPreparation(
        ResourceId target,
        ResourceId preparation);

    [[nodiscard]] base::Expected<ResourceUseId, std::string> AddUse(
        ResourceId resource,
        Effect effect,
        ViewRole role,
        TemporalRelation temporalRelation = TemporalRelation::Current);

    [[nodiscard]] base::Expected<ProgramId, std::string> AddRasterProgram(
        std::string debugName,
        ProgramInterface interfaceDescription,
        ProgramSource source);

    [[nodiscard]] base::Expected<ProgramId, std::string> AddComputeProgram(
        std::string debugName,
        ProgramInterface interfaceDescription,
        ProgramSource source);

    [[nodiscard]] base::Expected<WorkId, std::string> AddRasterWorkGeneric(
        std::string debugName,
        ProgramId program,
        std::span<const WorkOperand> operands,
        std::uint32_t vertexCount);

    [[nodiscard]] base::Expected<WorkId, std::string> AddCopyWork(
        std::string debugName,
        ResourceUseId source,
        ResourceUseId destination,
        std::uint64_t bytes);

    [[nodiscard]] base::Expected<WorkId, std::string> AddComputeWorkGeneric(
        std::string debugName,
        ProgramId program,
        std::span<const WorkOperand> operands,
        std::uint32_t threadGroupCountX,
        std::uint32_t threadGroupCountY,
        std::uint32_t threadGroupCountZ);

    [[nodiscard]] base::Expected<WorkId, std::string> AddPresentWork(
        std::string debugName,
        ResourceUseId presentSource);

    [[nodiscard]] base::Expected<void, std::string> AddDependency(
        WorkId predecessor,
        WorkId successor);

    [[nodiscard]] SemanticGraph Build() &&;

private:
    [[nodiscard]] base::Expected<void, std::string> ValidateOperands(
        ProgramId program,
        std::span<const WorkOperand> operands,
        ProgramKind expectedKind) const;

    SemanticGraph graph_;
};
}
