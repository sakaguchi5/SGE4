#pragma once

#include "../../tests/fixtures/leaf/SemanticBuilder.h"
#include "../../src/leaf/toolchain/compiler/LeafCompiler.h"
#include "../../src/composition/toolchain/CompositionToolchain.h"
#include "../../src/composition/model/DynamicExecutionContract.h"
#include "../../src/dynamic/artifact/DynamicInvocationPackage.h"
#include "../../src/dynamic/planner/DynamicInvocationPlanner.h"
#include "../../src/dynamic/verifier/DynamicInvocationVerifier.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sge4::level5::vertical1
{
namespace sem = sge4::semantic;
namespace comp = sge4::composition;
namespace dyn = sge4::dynamic;

inline constexpr std::string_view StateLeafKey = "level5/v1/state-writer";
inline constexpr std::string_view TextureLeafKey = "level5/v1/texture-writer";
inline constexpr std::string_view TemporalLeafKey = "level5/v1/temporal-producer";
inline constexpr std::string_view ObservationLeafKey = "level5/v1/observation";

inline constexpr std::string_view StateEndpoint = "level5/v1/state/output";
inline constexpr std::string_view TextureEndpoint = "level5/v1/texture/output";
inline constexpr std::string_view TemporalStateInputEndpoint = "level5/v1/temporal/state-input";
inline constexpr std::string_view TemporalCurrentEndpoint = "level5/v1/temporal/current";
inline constexpr std::string_view ObservationPreviousEndpoint = "level5/v1/observation/previous";
inline constexpr std::string_view ObservationStateEndpoint = "level5/v1/observation/state";
inline constexpr std::string_view ObservationOutputEndpoint = "level5/v1/observation/output";

inline constexpr std::string_view StateFlowKey = "level5/v1/state";
inline constexpr std::string_view TextureFlowKey = "level5/v1/texture";
inline constexpr std::string_view TemporalFlowKey = "level5/v1/temporal-history";
inline constexpr std::string_view ObservationFlowKey = "level5/v1/observation-output";

struct CompiledLeaf final
{
    std::vector<std::byte> packageBytes;
    std::string executionDigestHex;
};

enum class CandidateKind : std::uint32_t
{
    DenseDirect = 1,
    VerifiedSparseIndirect = 2
};

struct CandidateBuild final
{
    CandidateKind kind = CandidateKind::DenseDirect;
    comp::FrozenCompositionPackage package;
    std::string stateWriterExecutionDigestHex;
    comp::ResourceFlowId observationResource;
    comp::ResourceFlowId temporalResource;
    comp::ResourceFlowId textureResource;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

inline target::D3D12TargetProfile ComputeProfile(std::uint32_t descriptors)
{
    target::D3D12TargetProfile profile;
    profile.directQueueCount = 1;
    profile.computeQueueCount = 1;
    profile.copyQueueCount = 0;
    profile.surfaceImageCount = 0;
    profile.rtvDescriptorCount = 0;
    profile.dsvDescriptorCount = 0;
    profile.shaderDescriptorCount = descriptors;
    return profile;
}

inline base::Expected<CompiledLeaf, std::string> CompileLeaf(
    sem::SemanticGraph graph,
    target::D3D12TargetProfile profile)
{
    auto compiled = compiler::CompileCanonical(graph, profile);
    if (!compiled)
        return base::Failure<CompiledLeaf, std::string>(
            compiled.error().stage + "：" + compiled.error().message);
    CompiledLeaf result;
    result.packageBytes = std::move(compiled.value().packageBytes);
    result.executionDigestHex = std::move(compiled.value().executionDigestHex);
    return base::Success<CompiledLeaf, std::string>(std::move(result));
}

inline base::Expected<CompiledLeaf, std::string> BuildStateWriterLeaf(
    std::uint32_t universe)
{
    constexpr std::uint32_t MemberBytes = 16;
    const auto totalBytes = static_cast<std::uint64_t>(universe) * MemberBytes;
    sem::SemanticBuilder builder;
    auto dynamicState = builder.AddDynamicBuffer(
        "L5V1.State.Dynamic", totalBytes, MemberBytes, MemberBytes);
    auto output = builder.AddExternalBuffer(
        "L5V1.State.Output", totalBytes, MemberBytes);
    if (!dynamicState || !output)
        return base::Failure<CompiledLeaf, std::string>(
            "Level 5 State Bufferが検証または実行の契約に違反しています。");

    auto dynamicUse = builder.AddUse(
        dynamicState.value(), sem::Effect::Read, sem::ViewRole::ShaderBuffer);
    auto outputUse = builder.AddUse(
        output.value(), sem::Effect::Write, sem::ViewRole::StorageBuffer);
    if (!dynamicUse || !outputUse)
        return base::Failure<CompiledLeaf, std::string>(
            "Level 5 State ResourceUseが検証または実行の契約に違反しています。");

    sem::ProgramInterface interfaceDescription;
    interfaceDescription.parameters = {
        {{0}, "DynamicState", sem::ProgramParameterKind::ReadOnlyBuffer,
            sem::ShaderStage::Compute, 0, 0, 1},
        {{1}, "StateOutput", sem::ProgramParameterKind::UnorderedBuffer,
            sem::ShaderStage::Compute, 0, 0, 1}};
    auto program = builder.AddComputeProgram(
        "L5V1.State.Program", std::move(interfaceDescription), {R"hlsl(
StructuredBuffer<float4> DynamicState : register(t0);
RWStructuredBuffer<float4> StateOutput : register(u0);
[numthreads(1, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    float4 value = DynamicState[id.x];
    [loop]
    for (uint iteration = 0; iteration < 64u; ++iteration)
    {
        value = sin(value) * 0.99991f;
    }
    StateOutput[id.x] = value;
}
)hlsl", {}, {}, "CSMain"});
    if (!program)
        return base::Failure<CompiledLeaf, std::string>(program.error());
    const std::array operands = {
        sem::WorkOperand{sem::WorkOperandKind::ProgramParameter, dynamicUse.value(), {0}},
        sem::WorkOperand{sem::WorkOperandKind::ProgramParameter, outputUse.value(), {1}}};
    auto work = builder.AddComputeWorkGeneric(
        "L5V1.State.Work", program.value(), operands, universe, 1, 1);
    if (!work)
        return base::Failure<CompiledLeaf, std::string>(work.error());
    return CompileLeaf(std::move(builder).Build(), ComputeProfile(4));
}

inline base::Expected<CompiledLeaf, std::string> BuildTextureWriterLeaf(
    std::uint32_t width,
    std::uint32_t height)
{
    constexpr std::uint32_t MemberBytes = 16;
    const auto universe = width * height;
    const auto totalBytes = static_cast<std::uint64_t>(universe) * MemberBytes;
    sem::SemanticBuilder builder;
    auto dynamicColor = builder.AddDynamicBuffer(
        "L5V1.Texture.Dynamic", totalBytes, MemberBytes, MemberBytes);
    auto output = builder.AddExternalTexture2D(
        "L5V1.Texture.Output", width, height,
        sem::FormatMeaning::Rgba32Float, width * 16u);
    if (!dynamicColor || !output)
        return base::Failure<CompiledLeaf, std::string>(
            "Level 5 Texture Resourceが検証または実行の契約に違反しています。");

    auto dynamicUse = builder.AddUse(
        dynamicColor.value(), sem::Effect::Read, sem::ViewRole::ShaderBuffer);
    auto outputUse = builder.AddUse(
        output.value(), sem::Effect::Write, sem::ViewRole::StorageTexture2D);
    if (!dynamicUse || !outputUse)
        return base::Failure<CompiledLeaf, std::string>(
            "Level 5 Texture ResourceUseが検証または実行の契約に違反しています。");

    sem::ProgramInterface interfaceDescription;
    interfaceDescription.parameters = {
        {{0}, "DynamicColor", sem::ProgramParameterKind::ReadOnlyBuffer,
            sem::ShaderStage::Compute, 0, 0, 1},
        {{1}, "Image", sem::ProgramParameterKind::UnorderedTexture2D,
            sem::ShaderStage::Compute, 0, 0, 1}};
    const auto source = std::string(R"hlsl(
StructuredBuffer<float4> DynamicColor : register(t0);
RWTexture2D<float4> Image : register(u0);
static const uint ImageWidth = )hlsl") + std::to_string(width) + R"hlsl(u;
[numthreads(1, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    const uint2 pixel = uint2(id.x % ImageWidth, id.x / ImageWidth);
    Image[pixel] = saturate(DynamicColor[id.x]);
}
)hlsl";
    auto program = builder.AddComputeProgram(
        "L5V1.Texture.Program", std::move(interfaceDescription),
        {source, {}, {}, "CSMain"});
    if (!program)
        return base::Failure<CompiledLeaf, std::string>(program.error());
    const std::array operands = {
        sem::WorkOperand{sem::WorkOperandKind::ProgramParameter, dynamicUse.value(), {0}},
        sem::WorkOperand{sem::WorkOperandKind::ProgramParameter, outputUse.value(), {1}}};
    auto work = builder.AddComputeWorkGeneric(
        "L5V1.Texture.Work", program.value(), operands, universe, 1, 1);
    if (!work)
        return base::Failure<CompiledLeaf, std::string>(work.error());
    return CompileLeaf(std::move(builder).Build(), ComputeProfile(4));
}

inline base::Expected<CompiledLeaf, std::string> BuildTemporalProducerLeaf(
    std::uint32_t universe)
{
    const auto stateBytes = static_cast<std::uint64_t>(universe) * 16u;
    sem::SemanticBuilder builder;
    auto state = builder.AddExternalBuffer("L5V1.Temporal.State", stateBytes, 16);
    auto current = builder.AddExternalBuffer("L5V1.Temporal.Current", 16, 16);
    if (!state || !current)
        return base::Failure<CompiledLeaf, std::string>(
            "Level 5 Temporal Resourceが検証または実行の契約に違反しています。");

    auto stateUse = builder.AddUse(
        state.value(), sem::Effect::Read, sem::ViewRole::ShaderBuffer);
    auto currentUse = builder.AddUse(
        current.value(), sem::Effect::Write, sem::ViewRole::StorageBuffer);
    if (!stateUse || !currentUse)
        return base::Failure<CompiledLeaf, std::string>(
            "Level 5 Temporal ResourceUseが検証または実行の契約に違反しています。");

    sem::ProgramInterface interfaceDescription;
    interfaceDescription.parameters = {
        {{0}, "State", sem::ProgramParameterKind::ReadOnlyBuffer,
            sem::ShaderStage::Compute, 0, 0, 1},
        {{1}, "Current", sem::ProgramParameterKind::UnorderedBuffer,
            sem::ShaderStage::Compute, 0, 0, 1}};
    const auto source = std::string(R"hlsl(
StructuredBuffer<float4> State : register(t0);
RWStructuredBuffer<float4> Current : register(u0);
static const uint Universe = )hlsl") + std::to_string(universe) + R"hlsl(u;
[numthreads(1, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    float stateSum = 0.0f;
    [loop]
    for (uint member = 0; member < Universe; ++member)
    {
        stateSum += State[member].x;
    }
    Current[0] = float4(stateSum, 0.0f, stateSum, float(Universe));
}
)hlsl";
    auto program = builder.AddComputeProgram(
        "L5V1.Temporal.Program", std::move(interfaceDescription),
        {source, {}, {}, "CSMain"});
    if (!program)
        return base::Failure<CompiledLeaf, std::string>(program.error());
    const std::array operands = {
        sem::WorkOperand{sem::WorkOperandKind::ProgramParameter, stateUse.value(), {0}},
        sem::WorkOperand{sem::WorkOperandKind::ProgramParameter, currentUse.value(), {1}}};
    auto work = builder.AddComputeWorkGeneric(
        "L5V1.Temporal.Work", program.value(), operands, 1, 1, 1);
    if (!work)
        return base::Failure<CompiledLeaf, std::string>(work.error());
    return CompileLeaf(std::move(builder).Build(), ComputeProfile(4));
}

inline base::Expected<CompiledLeaf, std::string> BuildObservationLeaf(
    std::uint32_t universe)
{
    const auto stateBytes = static_cast<std::uint64_t>(universe) * 16u;
    sem::SemanticBuilder builder;
    auto previous = builder.AddExternalBuffer("L5V1.Observation.Previous", 16, 16);
    auto state = builder.AddExternalBuffer("L5V1.Observation.State", stateBytes, 16);
    auto output = builder.AddExternalBuffer("L5V1.Observation.Output", 16, 16);
    if (!previous || !state || !output)
        return base::Failure<CompiledLeaf, std::string>(
            "Level 5 Observation Resourceが検証または実行の契約に違反しています。");

    auto previousUse = builder.AddUse(
        previous.value(), sem::Effect::Read, sem::ViewRole::ShaderBuffer);
    auto stateUse = builder.AddUse(
        state.value(), sem::Effect::Read, sem::ViewRole::ShaderBuffer);
    auto outputUse = builder.AddUse(
        output.value(), sem::Effect::Write, sem::ViewRole::StorageBuffer);
    if (!previousUse || !stateUse || !outputUse)
        return base::Failure<CompiledLeaf, std::string>(
            "Level 5 Observation ResourceUseが検証または実行の契約に違反しています。");

    sem::ProgramInterface interfaceDescription;
    interfaceDescription.parameters = {
        {{0}, "Previous", sem::ProgramParameterKind::ReadOnlyBuffer,
            sem::ShaderStage::Compute, 0, 0, 1},
        {{1}, "State", sem::ProgramParameterKind::ReadOnlyBuffer,
            sem::ShaderStage::Compute, 1, 0, 1},
        {{2}, "Observation", sem::ProgramParameterKind::UnorderedBuffer,
            sem::ShaderStage::Compute, 0, 0, 1}};
    const auto source = std::string(R"hlsl(
StructuredBuffer<float4> Previous : register(t0);
StructuredBuffer<float4> State : register(t1);
RWStructuredBuffer<float4> Observation : register(u0);
static const uint Universe = )hlsl") + std::to_string(universe) + R"hlsl(u;
[numthreads(1, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    float stateSum = 0.0f;
    [loop]
    for (uint member = 0; member < Universe; ++member)
    {
        stateSum += State[member].x;
    }
    Observation[0] = float4(
        stateSum, Previous[0].x, stateSum - Previous[0].x, float(Universe));
}
)hlsl";
    auto program = builder.AddComputeProgram(
        "L5V1.Observation.Program", std::move(interfaceDescription),
        {source, {}, {}, "CSMain"});
    if (!program)
        return base::Failure<CompiledLeaf, std::string>(program.error());
    const std::array operands = {
        sem::WorkOperand{sem::WorkOperandKind::ProgramParameter, previousUse.value(), {0}},
        sem::WorkOperand{sem::WorkOperandKind::ProgramParameter, stateUse.value(), {1}},
        sem::WorkOperand{sem::WorkOperandKind::ProgramParameter, outputUse.value(), {2}}};
    auto work = builder.AddComputeWorkGeneric(
        "L5V1.Observation.Work", program.value(), operands, 1, 1, 1);
    if (!work)
        return base::Failure<CompiledLeaf, std::string>(work.error());
    return CompileLeaf(std::move(builder).Build(), ComputeProfile(6));
}

inline comp::LeafPackageDeclaration StateDeclaration(const CompiledLeaf& leaf)
{
    return {std::string(StateLeafKey), leaf.packageBytes,
        {{0, std::string(StateEndpoint)}}};
}

inline comp::LeafPackageDeclaration TextureDeclaration(const CompiledLeaf& leaf)
{
    return {std::string(TextureLeafKey), leaf.packageBytes,
        {{0, std::string(TextureEndpoint)}}};
}

inline comp::LeafPackageDeclaration TemporalDeclaration(const CompiledLeaf& leaf)
{
    return {std::string(TemporalLeafKey), leaf.packageBytes,
        {{0, std::string(TemporalStateInputEndpoint)},
         {1, std::string(TemporalCurrentEndpoint)}}};
}

inline comp::LeafPackageDeclaration ObservationDeclaration(const CompiledLeaf& leaf)
{
    return {std::string(ObservationLeafKey), leaf.packageBytes,
        {{0, std::string(ObservationPreviousEndpoint)},
         {1, std::string(ObservationStateEndpoint)},
         {2, std::string(ObservationOutputEndpoint)}}};
}

inline comp::EndpointReferenceDeclaration Ref(
    std::string_view leaf,
    std::string_view endpoint)
{
    return {std::string(leaf), std::string(endpoint)};
}

inline std::map<std::string, comp::LeafPackageId> ResolveLeafIds()
{
    std::vector<std::pair<comp::StableKey, std::string>> keys = {
        {comp::ComputeStableLeafKey(StateLeafKey), std::string(StateLeafKey)},
        {comp::ComputeStableLeafKey(TextureLeafKey), std::string(TextureLeafKey)},
        {comp::ComputeStableLeafKey(TemporalLeafKey), std::string(TemporalLeafKey)},
        {comp::ComputeStableLeafKey(ObservationLeafKey), std::string(ObservationLeafKey)}};
    std::ranges::sort(keys, {}, &std::pair<comp::StableKey, std::string>::first);
    std::map<std::string, comp::LeafPackageId> result;
    for (std::uint32_t index = 0; index < keys.size(); ++index)
        result.emplace(keys[index].second, comp::LeafPackageId{index});
    return result;
}

inline comp::ResourceFlowId FindResource(
    const comp::FrozenCompositionPackage& package,
    std::string_view authorKey)
{
    const auto stable = comp::ComputeStableResourceKey(authorKey);
    const auto& resources = package.VerifiedComposition().ValidatedContract().Contract().resources;
    const auto found = std::ranges::find(resources, stable, &comp::ResourceFlowContract::stableKey);
    return found == resources.end() ? comp::ResourceFlowId{} : found->id;
}

inline base::Expected<CandidateBuild, std::string> BuildCandidate(
    CandidateKind kind,
    std::uint32_t width,
    std::uint32_t height)
{
    if (width == 0 || height == 0 || width > 256 || height > 256)
        return base::Failure<CandidateBuild, std::string>(
            "Level 5実験extentが検証または実行の契約に違反しています。");
    const auto universe = width * height;
    auto state = BuildStateWriterLeaf(universe);
    auto texture = BuildTextureWriterLeaf(width, height);
    auto temporal = BuildTemporalProducerLeaf(universe);
    auto observation = BuildObservationLeaf(universe);
    if (!state || !texture || !temporal || !observation)
        return base::Failure<CandidateBuild, std::string>(
            !state ? state.error() : !texture ? texture.error() :
            !temporal ? temporal.error() : observation.error());

    comp::ContractBuildInput input;
    input.leaves = {
        StateDeclaration(state.value()),
        TextureDeclaration(texture.value()),
        TemporalDeclaration(temporal.value()),
        ObservationDeclaration(observation.value())};

    comp::ResourceFlowDeclaration stateFlow;
    stateFlow.stableKey = std::string(StateFlowKey);
    stateFlow.boundary = comp::ResourceBoundary::Internal;
    stateFlow.producer = Ref(StateLeafKey, StateEndpoint);
    stateFlow.consumers = {
        Ref(TemporalLeafKey, TemporalStateInputEndpoint),
        Ref(ObservationLeafKey, ObservationStateEndpoint)};

    comp::ResourceFlowDeclaration textureFlow;
    textureFlow.stableKey = std::string(TextureFlowKey);
    textureFlow.boundary = comp::ResourceBoundary::CompositionOutput;
    textureFlow.producer = Ref(TextureLeafKey, TextureEndpoint);

    comp::ResourceFlowDeclaration temporalFlow;
    temporalFlow.stableKey = std::string(TemporalFlowKey);
    temporalFlow.boundary = comp::ResourceBoundary::Internal;
    temporalFlow.lifetime = comp::ResourceFlowLifetime::TemporalHistory;
    temporalFlow.historyDepth = 1;
    temporalFlow.producer = Ref(TemporalLeafKey, TemporalCurrentEndpoint);
    temporalFlow.consumers = {
        Ref(ObservationLeafKey, ObservationPreviousEndpoint)};

    comp::ResourceFlowDeclaration observationFlow;
    observationFlow.stableKey = std::string(ObservationFlowKey);
    observationFlow.boundary = comp::ResourceBoundary::CompositionOutput;
    observationFlow.producer = Ref(ObservationLeafKey, ObservationOutputEndpoint);

    input.resources = {
        std::move(stateFlow), std::move(textureFlow),
        std::move(temporalFlow), std::move(observationFlow)};

    const auto leafIds = ResolveLeafIds();
    const auto stateLeaf = leafIds.at(std::string(StateLeafKey));
    const auto textureLeaf = leafIds.at(std::string(TextureLeafKey));
    std::vector<comp::DynamicExecutionRouteV1> routes = {
        comp::MakeDynamicExecutionRouteV1(stateLeaf, 0, 0, 16),
        comp::MakeDynamicExecutionRouteV1(textureLeaf, 0, 16, 16)};
    std::ranges::sort(routes, [](const auto& left, const auto& right) {
        if (left.targetLeaf != right.targetLeaf)
            return left.targetLeaf.value < right.targetLeaf.value;
        return left.targetDynamicSlot < right.targetDynamicSlot;
    });
    comp::VerifiedIndirectDispatchContractV1 indirect;
    if (kind == CandidateKind::VerifiedSparseIndirect)
        indirect = comp::MakeVerifiedIndirectDispatchContractV1(stateLeaf, 0, universe);
    auto dynamicContract = comp::MakeVerifiedRoutedSlotsDynamicContractV1(
        universe, 32, std::move(routes), {}, indirect);
    auto package = comp::BuildFrozenCompositionPackage(
        std::move(input), std::move(dynamicContract));
    if (!package)
        return base::Failure<CandidateBuild, std::string>(
            package.error().stage + "：" + package.error().message);

    CandidateBuild result{
        kind,
        std::move(package).value(),
        state.value().executionDigestHex,
        {}, {}, {}, width, height};
    result.observationResource = FindResource(result.package, ObservationFlowKey);
    result.temporalResource = FindResource(result.package, TemporalFlowKey);
    result.textureResource = FindResource(result.package, TextureFlowKey);
    if (!result.observationResource.IsValid() || !result.temporalResource.IsValid() ||
        !result.textureResource.IsValid())
        return base::Failure<CandidateBuild, std::string>(
            "Level 5実験Resource Flowの解決に失敗しました。");
    return base::Success<CandidateBuild, std::string>(std::move(result));
}

inline base::Expected<dyn::FrozenDynamicInvocationPackage, std::string> BuildInvocation(
    const comp::FrozenCompositionPackage& composition,
    canonical::DeviceEpoch deviceEpoch,
    dyn::InvocationInputV1 input,
    std::optional<dyn::VerifiedHistoryStateV1> previousHistory = std::nullopt)
{
    auto request = dyn::BuildDynamicInvocationRequest(
        composition, deviceEpoch, std::move(input), std::move(previousHistory));
    if (!request)
        return base::Failure<dyn::FrozenDynamicInvocationPackage, std::string>(
            request.error().stage + "：" + request.error().message);
    auto proposal = dyn::DynamicInvocationPlannerV1::Plan(request.value());
    if (!proposal.Planned())
        return base::Failure<dyn::FrozenDynamicInvocationPackage, std::string>(
            "Level 5 Dynamic Planが検証または実行の契約に違反しています。");
    auto verified = dyn::DynamicInvocationVerifierV1::Verify(
        request.value(), *proposal.proposal);
    if (!verified.Accepted())
        return base::Failure<dyn::FrozenDynamicInvocationPackage, std::string>(
            "Level 5 Dynamic Verificationが検証または実行の契約に違反しています。");
    auto frozen = dyn::FreezeVerifiedInvocation(*verified.verified);
    if (!frozen)
        return base::Failure<dyn::FrozenDynamicInvocationPackage, std::string>(
            frozen.error().stage + "：" + frozen.error().message);
    return base::Success<dyn::FrozenDynamicInvocationPackage, std::string>(
        std::move(frozen).value());
}

inline std::vector<std::byte> CanonicalMemberPayload(
    std::uint32_t member,
    std::uint64_t frameOrdinal)
{
    const float phase = static_cast<float>((frameOrdinal % 97u) + 1u) * 0.0005f;
    const float baseValue = static_cast<float>(member + 1u) * 0.001f + phase;
    const std::array<float, 4> state{
        baseValue, baseValue * 0.5f, baseValue * 0.25f, 1.0f};
    const std::array<float, 4> color{
        std::min(1.0f, baseValue * 3.0f),
        std::min(1.0f, baseValue * 2.0f),
        std::min(1.0f, baseValue), 1.0f};
    std::vector<std::byte> bytes(sizeof(state) + sizeof(color));
    std::memcpy(bytes.data(), state.data(), sizeof(state));
    std::memcpy(bytes.data() + sizeof(state), color.data(), sizeof(color));
    return bytes;
}

inline dyn::InvocationInputV1 MakePrefixInvocationInput(
    std::uint64_t timelineOrdinal,
    dyn::InvocationModeV1 mode,
    std::uint32_t activeCount,
    bool modifiedAll)
{
    dyn::InvocationInputV1 input;
    input.timelineOrdinal = timelineOrdinal;
    input.mode = mode;
    input.activeMembers.reserve(activeCount);
    input.updatePayloads.reserve(activeCount);
    if (modifiedAll) input.modifiedSurvivors.reserve(activeCount);
    for (std::uint32_t member = 0; member < activeCount; ++member)
    {
        input.activeMembers.push_back(member);
        if (modifiedAll) input.modifiedSurvivors.push_back(member);
        input.updatePayloads.push_back({member, CanonicalMemberPayload(member, timelineOrdinal)});
    }
    return input;
}

inline std::vector<std::byte> ZeroTemporalSeed()
{
    return std::vector<std::byte>(16, std::byte{0});
}
}
