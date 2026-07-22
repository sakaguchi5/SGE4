#include "TemporalCandidateFamily.h"

#include "../00_Foundation/BinaryIO.h"

#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace sge4_5::spiral5::family_candidate
{
namespace
{
base::Digest256 Literal(std::string_view value)
{
    return base::Sha256(std::as_bytes(std::span(value.data(), value.size())));
}
void Digest(base::BinaryWriter& writer, const base::Digest256& value) { writer.WriteBytes(value); }

struct PhysicalShape final
{
    TemporalFamilyHistoryRoleV1 role{};
    TemporalIssuancePolicyV1 issuance{};
    TemporalCompletionPolicyV1 completion{};
    TemporalHoldPolicyV1 hold{};
    std::uint32_t historyBytes = 0;
    std::uint32_t argumentBufferBytes = 0;
    std::uint32_t maxCommandCount = 0;
    std::uint32_t argumentProducerDispatchX = 0;
    std::uint32_t hierarchyUpdateDispatchX = 0;
    std::uint32_t hierarchyHoldDispatchX = 0;
    std::uint32_t consumerUpdateDispatchX = 0;
    std::uint32_t consumerHoldDispatchX = 0;
};

PhysicalShape Shape(TemporalCandidateKindV1 kind)
{
    switch (kind)
    {
    case TemporalCandidateKindV1::EveryInvocationRecompute:
        return {TemporalFamilyHistoryRoleV1::None,
                TemporalIssuancePolicyV1::DirectEveryInvocation,
                TemporalCompletionPolicyV1::EveryInvocationOutputCompletion,
                TemporalHoldPolicyV1::RecomputeUsingLatestSource,
                0u, 0u, 0u, 0u, 1u, 1u, 64u, 64u};
    case TemporalCandidateKindV1::GlobalMotorHistoryReuse:
        return {TemporalFamilyHistoryRoleV1::GlobalMotorHistory,
                TemporalIssuancePolicyV1::UpdateIndirectHierarchy,
                TemporalCompletionPolicyV1::ConsumerCompletionRetainsGlobalMotor,
                TemporalHoldPolicyV1::ZeroHierarchyReuseGlobalMotor,
                semantic::BoneCountV1 * sizeof(semantic::PgaMotorRecordV1),
                12u, 1u, 1u, 1u, 0u, 64u, 64u};
    case TemporalCandidateKindV1::FinalOutputHistoryReuse:
        return {TemporalFamilyHistoryRoleV1::FinalOutputHistory,
                TemporalIssuancePolicyV1::UpdateIndirectHierarchyAndConsumer,
                TemporalCompletionPolicyV1::OutputCompletionRetainsFinalOutput,
                TemporalHoldPolicyV1::ZeroHierarchyAndConsumerRetainOutput,
                semantic::WorkCountV1 * sizeof(semantic::PointRecordV1),
                24u, 2u, 1u, 1u, 0u, 64u, 0u};
    default:
        throw std::invalid_argument("Unknown Temporal Candidate kind.");
    }
}

}

base::Digest256 TemporalFamilyTargetProfileIdentityV1()
{
    return Literal("SGE4-5.Spiral5.TargetProfile.D3D12.TemporalCandidateFamily.WarpQualified.V1");
}
base::Digest256 TemporalFamilyObservationContractIdentityV1()
{
    return Literal("SGE4-5.Spiral5.Observation.TemporalFamily.ExactPiecewiseConstant.Pairwise.V1");
}
base::Digest256 TemporalFamilyDeviceEpochPolicyIdentityV1()
{
    return Literal("SGE4-5.Spiral5.DeviceEpoch.TemporalFamily.StaleRejected.ReseedRequired.V1");
}
base::Digest256 TemporalFamilyCompletionContractIdentityV1(TemporalCandidateKindV1 kind)
{
    switch (kind)
    {
    case TemporalCandidateKindV1::EveryInvocationRecompute:
        return Literal("SGE4-5.Spiral5.Completion.EveryInvocationOutput.V1");
    case TemporalCandidateKindV1::GlobalMotorHistoryReuse:
        return Literal("SGE4-5.Spiral5.Completion.ConsumerRetainsGlobalMotorHistory.V1");
    case TemporalCandidateKindV1::FinalOutputHistoryReuse:
        return Literal("SGE4-5.Spiral5.Completion.OutputRetainsFinalOutputHistory.V1");
    default: throw std::invalid_argument("Unknown Temporal Candidate kind.");
    }
}
base::Digest256 TemporalFamilyArgumentProducerProgramIdentityV1()
{
    return Literal("SGE4-5.Spiral5.Program.TemporalFamily.UpdateDispatchArguments.V1");
}
base::Digest256 TemporalFamilyHierarchyProgramIdentityV1()
{
    return Literal("SGE4-5.Spiral5.Program.TemporalFamily.DirectPgaHierarchy.V1");
}
base::Digest256 TemporalFamilyConsumerProgramIdentityV1()
{
    return Literal("SGE4-5.Spiral5.Program.TemporalFamily.DirectPgaConsumer4096.V1");
}

base::Digest256 TemporalFamilyArtifactIdentityV1(
    const semantic::TemporalStateSemanticV1& semanticValue,
    const semantic::UpdateScheduleV1& schedule,
    TemporalCandidateKindV1 kind)
{
    const auto shape = Shape(kind);
    base::BinaryWriter writer;
    Digest(writer, Literal("SGE4-5.Spiral5.FrozenTemporalCandidateArtifact.V1"));
    Digest(writer, semantic::TemporalStateSemanticIdentityV1(semanticValue));
    Digest(writer, semantic::UpdateScheduleIdentityV1(schedule));
    writer.WriteU32(static_cast<std::uint32_t>(kind));
    writer.WriteU32(static_cast<std::uint32_t>(shape.role));
    writer.WriteU32(static_cast<std::uint32_t>(shape.issuance));
    writer.WriteU32(static_cast<std::uint32_t>(shape.completion));
    writer.WriteU32(static_cast<std::uint32_t>(shape.hold));
    writer.WriteU32(shape.historyBytes);
    writer.WriteU32(shape.argumentBufferBytes);
    writer.WriteU32(shape.maxCommandCount);
    writer.WriteU32(shape.argumentProducerDispatchX);
    writer.WriteU32(shape.hierarchyUpdateDispatchX);
    writer.WriteU32(shape.hierarchyHoldDispatchX);
    writer.WriteU32(shape.consumerUpdateDispatchX);
    writer.WriteU32(shape.consumerHoldDispatchX);
    Digest(writer, TemporalFamilyCompletionContractIdentityV1(kind));
    Digest(writer, kind == TemporalCandidateKindV1::EveryInvocationRecompute
        ? base::Digest256{}
        : contract::BuildCu2GlobalMotorHistoryArtifactV1(semanticValue, schedule).Spiral4IndirectArtifactIdentity());
    Digest(writer, kind == TemporalCandidateKindV1::EveryInvocationRecompute
        ? base::Digest256{}
        : TemporalFamilyArgumentProducerProgramIdentityV1());
    Digest(writer, TemporalFamilyHierarchyProgramIdentityV1());
    Digest(writer, TemporalFamilyConsumerProgramIdentityV1());
    return base::Sha256(writer.Bytes());
}

base::Digest256 TemporalFamilyHistoryResourceIdentityV1(
    const semantic::TemporalStateSemanticV1& semanticValue,
    const semantic::UpdateScheduleV1& schedule,
    TemporalCandidateKindV1 kind)
{
    const auto shape = Shape(kind);
    base::BinaryWriter writer;
    Digest(writer, Literal("SGE4-5.Spiral5.ActualTemporalFamilyHistoryResource.V1"));
    Digest(writer, TemporalFamilyArtifactIdentityV1(semanticValue, schedule, kind));
    writer.WriteU32(static_cast<std::uint32_t>(kind));
    writer.WriteU32(static_cast<std::uint32_t>(shape.role));
    writer.WriteU32(shape.historyBytes);
    writer.WriteU32(shape.role == TemporalFamilyHistoryRoleV1::None ? 0u : semantic::HistoryDepthV1);
    return base::Sha256(writer.Bytes());
}

base::Digest256 TemporalFamilyInvocationAuthorityIdentityV1(
    const semantic::TemporalStateSemanticV1& semanticValue,
    const semantic::UpdateScheduleV1& schedule,
    TemporalCandidateKindV1 kind)
{
    (void)semanticValue;
    const auto shape = Shape(kind);
    base::BinaryWriter writer;
    Digest(writer, Literal("SGE4-5.Spiral5.TemporalFamilyInvocationAuthority.V1"));
    Digest(writer, semantic::UpdateScheduleIdentityV1(schedule));
    writer.WriteU32(static_cast<std::uint32_t>(kind));
    writer.WriteU32(static_cast<std::uint32_t>(schedule.invocations.size()));
    for (const auto& invocation : schedule.invocations)
    {
        const bool update = invocation.event == semantic::UpdateEventV1::Update;
        writer.WriteU32(invocation.invocationIndex);
        writer.WriteU32(static_cast<std::uint32_t>(invocation.event));
        writer.WriteU32(invocation.sourceGeneration);
        writer.WriteU32(update ? shape.hierarchyUpdateDispatchX : shape.hierarchyHoldDispatchX);
        writer.WriteU32(update ? shape.consumerUpdateDispatchX : shape.consumerHoldDispatchX);
        writer.WriteU32(invocation.sourceGeneration);
        writer.WriteU32(update && shape.role != TemporalFamilyHistoryRoleV1::None
            ? invocation.sourceGeneration : semantic::InvalidGenerationV1);
        writer.WriteU64(static_cast<std::uint64_t>(invocation.invocationIndex) + 1u);
    }
    return base::Sha256(writer.Bytes());
}

std::vector<std::byte> SerializeRawTemporalCandidateV1(const RawTemporalCandidateV1& value)
{
    base::BinaryWriter writer;
    Digest(writer, Literal("SGE4-5.Spiral5.RawTemporalCandidateFamily.V1"));
    Digest(writer, value.semanticIdentity);
    Digest(writer, value.scheduleIdentity);
    Digest(writer, value.targetProfileIdentity);
    Digest(writer, value.observationContractIdentity);
    Digest(writer, value.completionContractIdentity);
    Digest(writer, value.deviceEpochPolicyIdentity);
    Digest(writer, value.spiral4IndirectArtifactIdentity);
    Digest(writer, value.argumentProducerProgramIdentity);
    Digest(writer, value.hierarchyProgramIdentity);
    Digest(writer, value.consumerProgramIdentity);
    Digest(writer, value.proposedArtifactIdentity);
    Digest(writer, value.proposedHistoryResourceIdentity);
    Digest(writer, value.invocationAuthorityIdentity);
    writer.WriteU32(static_cast<std::uint32_t>(value.kind));
    writer.WriteU32(static_cast<std::uint32_t>(value.historyRole));
    writer.WriteU32(static_cast<std::uint32_t>(value.issuancePolicy));
    writer.WriteU32(static_cast<std::uint32_t>(value.completionPolicy));
    writer.WriteU32(static_cast<std::uint32_t>(value.holdPolicy));
    writer.WriteU32(value.timelineLength);
    writer.WriteU32(value.workCount);
    writer.WriteU32(value.threadsPerGroup);
    writer.WriteU32(value.historyDepth);
    writer.WriteU32(value.historyBytes);
    writer.WriteU32(value.argumentBufferBytes);
    writer.WriteU32(value.argumentStrideBytes);
    writer.WriteU32(value.maxCommandCount);
    writer.WriteU32(value.argumentProducerDispatchX);
    writer.WriteU32(value.hierarchyUpdateDispatchX);
    writer.WriteU32(value.hierarchyHoldDispatchX);
    writer.WriteU32(value.consumerUpdateDispatchX);
    writer.WriteU32(value.consumerHoldDispatchX);
    writer.WriteU32(value.updateCount);
    writer.WriteU32(value.holdCount);
    writer.WriteU32(value.maximumSourceGeneration);
    return std::move(writer).Take();
}
base::Digest256 RawTemporalCandidateIdentityV1(const RawTemporalCandidateV1& value)
{
    const auto bytes = SerializeRawTemporalCandidateV1(value);
    return base::Sha256(bytes);
}
}
