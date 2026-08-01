#include "DynamicInvocationPackage.h"

#include "../../canonical/base/BinaryIO.h"

#include <algorithm>
#include <string>
#include <utility>

namespace sge4::dynamic
{
namespace
{
using base::BinaryWriter;

template<class T>
[[nodiscard]] base::Expected<T, Error> Fail(std::string stage, std::string message)
{
    return base::Failure<T, Error>({std::move(stage), std::move(message)});
}

void WriteSet(BinaryWriter& writer, const ExactIndexSetV1& set)
{
    writer.WriteU32(set.Universe().value());
    writer.WriteU32(set.Count());
    writer.WriteBytes(set.Identity().Digest());
    for (const auto index : set.Indices()) writer.WriteU32(index);
}

[[nodiscard]] std::vector<std::byte> BuildManifest(
    const VerifiedDynamicInvocationV1& verified,
    const frozen_dynamic_detail::OpaqueFrozenDynamicInvocationV1& frozen)
{
    BinaryWriter writer;
    writer.WriteU32(FrozenInvocationManifestSchemaVersion);
    writer.WriteU8(static_cast<std::uint8_t>(frozen.Mode()));
    writer.WriteZeroes(3);
    writer.WriteU64(verified.Request().timelineOrdinal.value());
    writer.WriteU64(verified.Request().deviceEpoch.value());
    writer.WriteU32(verified.Request().universe.value());
    writer.WriteU32(frozen.IndirectWorkCount().value());
    writer.WriteBytes(frozen.Identity().Digest());
    writer.WriteBytes(frozen.CompositionIdentity().Digest());
    writer.WriteBytes(frozen.InvocationIdentity().Digest());
    writer.WriteBytes(frozen.DecisionIdentity().Digest());
    writer.WriteBytes(frozen.SealIdentity().Digest());
    writer.WriteBytes(frozen.WriteSetIdentity().Digest());
    writer.WriteBytes(frozen.ExecutionPayloadIdentity().Digest());
    writer.WriteBytes(verified.Decision().conditionalExecutionIdentity.Digest());
    writer.WriteBytes(verified.Decision().indirectDispatch.identity.Digest());
    writer.WriteU32(static_cast<std::uint32_t>(verified.Decision().conditionalSelections.size()));
    writer.WriteU32(static_cast<std::uint32_t>(verified.Decision().enabledLeaves.size()));
    writer.WriteU32(std::to_underlying(verified.Request().executionMode));
    writer.WriteU32(verified.Request().canonicalMemberBytes);
    writer.WriteCountU32(verified.Request().executionRoutes.size());
    writer.WriteU32(std::to_underlying(verified.Request().indirectDispatchContract.mode));
    writer.WriteU32(verified.Request().indirectDispatchContract.targetLeaf.value);
    writer.WriteU32(verified.Request().indirectDispatchContract.targetComputeCommand);
    writer.WriteU32(verified.Request().indirectDispatchContract.maxWorkCount);
    writer.WriteBytes(frozen.NextHistory().Descriptor().identity.Digest());
    writer.WriteU8(frozen.PreviousHistoryIdentity().has_value() ? 1u : 0u);
    writer.WriteZeroes(7);
    if (frozen.PreviousHistoryIdentity().has_value())
        writer.WriteBytes(frozen.PreviousHistoryIdentity()->Digest());
    return std::move(writer).Take();
}

[[nodiscard]] std::vector<std::byte> BuildSetBytes(const DynamicDecisionV1& decision)
{
    BinaryWriter writer;
    writer.WriteU32(6);
    WriteSet(writer, decision.previousActiveSet);
    WriteSet(writer, decision.activationSet);
    WriteSet(writer, decision.deactivationSet);
    WriteSet(writer, decision.updateSet);
    WriteSet(writer, decision.retainSet);
    WriteSet(writer, decision.transitionSet);
    return std::move(writer).Take();
}

[[nodiscard]] std::vector<std::byte> BuildTransitionBytes(const DynamicDecisionV1& decision)
{
    BinaryWriter writer;
    writer.WriteCountU32(decision.transitionRecords.size());
    writer.WriteBytes(decision.transitionRecordSetIdentity.Digest());
    writer.WriteBytes(decision.dynamicWriteSetIdentity.Digest());
    for (const auto& record : decision.transitionRecords)
    {
        writer.WriteU32(record.member.value());
        writer.WriteU8(static_cast<std::uint8_t>(record.action));
        writer.WriteZeroes(3);
    }
    return std::move(writer).Take();
}


[[nodiscard]] std::vector<std::byte> BuildExecutionPayloadBytes(
    const DynamicInvocationRequestV1& request)
{
    BinaryWriter writer;
    writer.WriteU32(2);
    writer.WriteU32(std::to_underlying(request.executionMode));
    writer.WriteU32(request.canonicalMemberBytes);
    writer.WriteCountU32(request.executionRoutes.size());
    for (const auto& route : request.executionRoutes)
    {
        writer.WriteU32(route.targetLeaf.value);
        writer.WriteU32(route.targetDynamicSlot);
        writer.WriteU32(route.sourceByteOffset);
        writer.WriteU32(route.routeMemberBytes);
    }
    writer.WriteCountU32(request.updatePayloads.size());
    writer.WriteBytes(request.executionPayloadIdentity.Digest());
    for (const auto& payload : request.updatePayloads)
    {
        writer.WriteU32(payload.member.value());
        writer.WriteCountU32(payload.bytes.size());
        writer.WriteBytes(payload.bytes);
    }
    return std::move(writer).Take();
}

[[nodiscard]] std::vector<std::byte> BuildConditionalExecutionBytes(
    const DynamicDecisionV1& decision)
{
    BinaryWriter writer;
    writer.WriteU32(1);
    writer.WriteBytes(decision.conditionalExecutionIdentity.Digest());
    writer.WriteCountU32(decision.conditionalSelections.size());
    for (const auto& selection : decision.conditionalSelections)
    {
        writer.WriteU32(selection.region.value);
        writer.WriteU8(selection.predicateValue ? 1u : 0u);
        writer.WriteZeroes(3);
    }
    writer.WriteCountU32(decision.enabledLeaves.size());
    for (const auto leaf : decision.enabledLeaves) writer.WriteU32(leaf.value);
    return std::move(writer).Take();
}

[[nodiscard]] std::vector<std::byte> BuildIndirectDispatchBytes(
    const VerifiedIndirectDispatchV1& dispatch)
{
    BinaryWriter writer;
    writer.WriteU32(1);
    writer.WriteU32(std::to_underlying(dispatch.mode));
    writer.WriteU32(dispatch.targetLeaf.value);
    writer.WriteU32(dispatch.targetComputeCommand);
    writer.WriteU32(dispatch.maxWorkCount);
    writer.WriteU32(dispatch.workCount);
    writer.WriteU32(dispatch.threadGroupCountX);
    writer.WriteU32(dispatch.threadGroupCountY);
    writer.WriteU32(dispatch.threadGroupCountZ);
    writer.WriteBytes(dispatch.identity.Digest());
    return std::move(writer).Take();
}

[[nodiscard]] std::vector<std::byte> BuildHistoryBytes(const VerifiedHistoryStateV1& history)
{
    BinaryWriter writer;
    writer.WriteU32(1);
    writer.WriteU64(history.Descriptor().generation.value());
    writer.WriteU64(history.Descriptor().deviceEpoch.value());
    writer.WriteU8(static_cast<std::uint8_t>(history.Descriptor().state));
    writer.WriteZeroes(3);
    writer.WriteU32(history.Universe().value());
    writer.WriteBytes(history.Descriptor().identity.Digest());
    writer.WriteBytes(history.CompositionIdentity().Digest());
    writer.WriteBytes(history.ActiveSet().Identity().Digest());
    writer.WriteBytes(history.GenerationIdentity().Digest());
    writer.WriteU32(history.ActiveSet().Count());
    for (const auto index : history.ActiveSet().Indices()) writer.WriteU32(index);
    writer.WriteCountU32(history.ItemGenerations().size());
    for (const auto generation : history.ItemGenerations()) writer.WriteU64(generation);
    return std::move(writer).Take();
}
}

base::Expected<DynamicInvocationRequestV1, Error> BuildDynamicInvocationRequest(
    const composition::FrozenCompositionPackage& composition,
    canonical::DeviceEpoch deviceEpoch,
    InvocationInputV1 input,
    std::optional<VerifiedHistoryStateV1> previousHistory)
{
    const auto universe = UniverseCount(composition.DynamicContract().universeCount);
    auto active = BuildExactIndexSetV1(universe, input.activeMembers);
    if (!active.Accepted())
        return Fail<DynamicInvocationRequestV1>("DynamicInvocation", "入力または内部状態が無効であるか、契約条件を満たしていません。");
    auto modified = BuildExactIndexSetV1(universe, input.modifiedSurvivors);
    if (!modified.Accepted())
        return Fail<DynamicInvocationRequestV1>("DynamicInvocation", "入力または内部状態が無効であるか、契約条件を満たしていません。");

    const auto& execution = composition.DynamicContract();
    std::sort(input.updatePayloads.begin(), input.updatePayloads.end(),
        [](const auto& left, const auto& right) { return left.member < right.member; });
    std::vector<MemberUpdatePayloadV1> payloads;
    payloads.reserve(input.updatePayloads.size());
    for (std::size_t index = 0; index < input.updatePayloads.size(); ++index)
    {
        auto& payload = input.updatePayloads[index];
        if (payload.member >= universe.value() ||
            (index > 0 && input.updatePayloads[index - 1].member == payload.member))
            return Fail<DynamicInvocationRequestV1>(
                "DynamicInvocation/Payload", "Dynamic payload memberがCanonicalではありません。");
        if (execution.executionMode == composition::DynamicExecutionModeV1::AuthorityOnly)
            return Fail<DynamicInvocationRequestV1>(
                "DynamicInvocation/Payload", "Authority-only Invocationへpayloadを渡せません。");
        if (payload.bytes.size() != execution.canonicalMemberBytes)
            return Fail<DynamicInvocationRequestV1>(
                "DynamicInvocation/Payload", "Canonical Dynamic payloadのbyte数が契約と一致しません。");
        payloads.push_back({MemberIndex(payload.member), std::move(payload.bytes)});
    }

    return base::Success<DynamicInvocationRequestV1, Error>(
        MakeDynamicInvocationRequestV1(
            composition.Certificate().artifactIdentity,
            composition.DynamicSemanticIdentity(),
            canonical::TimelineOrdinal(input.timelineOrdinal),
            deviceEpoch,
            universe,
            input.mode,
            std::move(*active.set),
            std::move(*modified.set),
            execution.executionMode, execution.canonicalMemberBytes,
            execution.executionRoutes,
            composition.Certificate().leafCount, execution.conditionalRegions,
            execution.indirectDispatch, std::move(payloads),
            std::move(previousHistory)));
}

base::Expected<FrozenDynamicInvocationPackage, Error> FreezeVerifiedInvocation(
    const VerifiedDynamicInvocationV1& verified)
{
    auto frozen = frozen_dynamic_detail::FrozenDynamicInvocationBuilderV1::Freeze(verified);

    std::vector<SectionInput> sections;
    sections.push_back({static_cast<std::uint32_t>(FrozenInvocationSectionKind::Manifest), 1,
        static_cast<std::uint16_t>(SectionFlags::Required) |
            static_cast<std::uint16_t>(SectionFlags::ExecutionAffecting),
        8, BuildManifest(verified, frozen)});
    sections.push_back({static_cast<std::uint32_t>(FrozenInvocationSectionKind::ExactSets), 1,
        static_cast<std::uint16_t>(SectionFlags::Required) |
            static_cast<std::uint16_t>(SectionFlags::ExecutionAffecting),
        8, BuildSetBytes(verified.Decision())});
    sections.push_back({static_cast<std::uint32_t>(FrozenInvocationSectionKind::TransitionRecords), 1,
        static_cast<std::uint16_t>(SectionFlags::Required) |
            static_cast<std::uint16_t>(SectionFlags::ExecutionAffecting),
        8, BuildTransitionBytes(verified.Decision())});
    sections.push_back({static_cast<std::uint32_t>(FrozenInvocationSectionKind::NextHistory), 1,
        static_cast<std::uint16_t>(SectionFlags::Required) |
            static_cast<std::uint16_t>(SectionFlags::ExecutionAffecting),
        8, BuildHistoryBytes(frozen.NextHistory())});
    sections.push_back({static_cast<std::uint32_t>(FrozenInvocationSectionKind::ExecutionPayload), 2,
        static_cast<std::uint16_t>(SectionFlags::Required) |
            static_cast<std::uint16_t>(SectionFlags::ExecutionAffecting),
        8, BuildExecutionPayloadBytes(verified.Request())});
    sections.push_back({static_cast<std::uint32_t>(FrozenInvocationSectionKind::ConditionalExecution), 1,
        static_cast<std::uint16_t>(SectionFlags::Required) |
            static_cast<std::uint16_t>(SectionFlags::ExecutionAffecting),
        8, BuildConditionalExecutionBytes(verified.Decision())});
    sections.push_back({static_cast<std::uint32_t>(FrozenInvocationSectionKind::IndirectDispatch), 1,
        static_cast<std::uint16_t>(SectionFlags::Required) |
            static_cast<std::uint16_t>(SectionFlags::ExecutionAffecting),
        8, BuildIndirectDispatchBytes(verified.Decision().indirectDispatch)});

    auto bytes = WriteSectionedArtifact(
        FrozenInvocationMagic, FrozenInvocationFormatMajor, FrozenInvocationFormatMinor,
        std::move(sections));
    if (!bytes)
        return Fail<FrozenDynamicInvocationPackage>(bytes.error().stage, bytes.error().message);

    FrozenDynamicExecutionPayloadV1 executionPayload{
        verified.Request().executionMode, verified.Request().canonicalMemberBytes,
        verified.Request().executionRoutes, verified.Request().executionPayloadIdentity,
        verified.Request().updatePayloads};
    return base::Success<FrozenDynamicInvocationPackage, Error>(
        FrozenDynamicInvocationPackage(
            std::move(bytes).value(), std::move(frozen), verified.Decision(),
            std::move(executionPayload), verified.Decision().indirectDispatch));
}
}
