#include "../00_Foundation/FileIO.h"
#include "../00_Foundation/Sha256.h"
#include "../03_SemanticBuilder/SemanticBuilder.h"
#include "../12_SGE4_5Compiler/SGE4_5Compiler.h"
#include "../16_FrozenCompositionArtifact/FrozenCompositionWriter.h"
#include "../17_CompositionContract/CompositionContract.h"
#include "../18_CompositionPlan/CompositionPlan.h"
#include "../19_CompositionVerifier/CompositionVerifier.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
namespace sem = sge4_5::semantic;
namespace canonical = sge4_5::composition::canonical;
namespace planning = sge4_5::composition::canonical::planning;
namespace verification = sge4_5::composition::canonical::verification;

using sge4_5::base::Result;

static_assert(!std::is_default_constructible_v<canonical::ValidatedCompositionContract>);
static_assert(!std::is_default_constructible_v<verification::VerifiedCompositionPlan>);
static_assert(!std::is_default_constructible_v<verification::VerifiedFrozenComposition>);

void Require(bool condition, std::string_view message)
{
    if (!condition) throw std::runtime_error(std::string(message));
}

Result<sem::SemanticGraph, std::string> BuildTransformLeaf()
{
    sem::SemanticBuilder builder;
    auto input = builder.AddExternalBuffer("R2.Input", 16, 16);
    auto output = builder.AddExternalBuffer("R2.Output", 16, 16);
    if (!input || !output)
        return Result<sem::SemanticGraph, std::string>::Failure(
            "external Buffer creation failed");

    auto inputUse = builder.AddUse(
        input.Value(), sem::Effect::Read, sem::ViewRole::ShaderBuffer);
    auto outputUse = builder.AddUse(
        output.Value(), sem::Effect::Write, sem::ViewRole::StorageBuffer);
    if (!inputUse || !outputUse)
        return Result<sem::SemanticGraph, std::string>::Failure(
            "external Buffer use creation failed");

    sem::ProgramInterface interfaceDescription;
    interfaceDescription.parameters = {
        {{0}, "Input", sem::ProgramParameterKind::ReadOnlyBuffer,
         sem::ShaderStage::Compute, 0, 0, 1},
        {{1}, "Output", sem::ProgramParameterKind::UnorderedBuffer,
         sem::ShaderStage::Compute, 0, 0, 1}};
    const std::string shader = R"hlsl(
StructuredBuffer<float4> Input : register(t0);
RWStructuredBuffer<float4> Output : register(u0);
[numthreads(1, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    Output[0] = Input[0] + 1.0f;
}
)hlsl";

    auto program = builder.AddComputeProgram(
        "R2.Transform", std::move(interfaceDescription),
        {shader, {}, {}, "CSMain"});
    if (!program)
        return Result<sem::SemanticGraph, std::string>::Failure(program.Error());

    const std::array operands = {
        sem::WorkOperand{sem::WorkOperandKind::ProgramParameter,
                         inputUse.Value(), {0}},
        sem::WorkOperand{sem::WorkOperandKind::ProgramParameter,
                         outputUse.Value(), {1}}};
    auto work = builder.AddComputeWorkGeneric(
        "R2.Transform.Work", program.Value(), operands, 1, 1, 1);
    if (!work)
        return Result<sem::SemanticGraph, std::string>::Failure(work.Error());
    return Result<sem::SemanticGraph, std::string>::Success(
        std::move(builder).Build());
}

sge4_5::target::D3D12TargetProfile Profile()
{
    sge4_5::target::D3D12TargetProfile profile;
    profile.computeQueueCount = 1;
    profile.copyQueueCount = 0;
    profile.surfaceImageCount = 0;
    profile.rtvDescriptorCount = 0;
    profile.dsvDescriptorCount = 0;
    profile.shaderDescriptorCount = 8;
    return profile;
}

std::vector<std::byte> CompileLeafPackage()
{
    auto graph = BuildTransformLeaf();
    Require(static_cast<bool>(graph), "R2 Leaf graph construction failed");
    auto package = sge4_5::compiler::CompileCanonical(graph.Value(), Profile());
    if (!package)
        throw std::runtime_error(
            package.Error().stage + ": " + package.Error().message);
    return std::move(package).Value().packageBytes;
}

canonical::LeafPackageDeclaration LeafDeclaration(
    std::string key,
    const std::vector<std::byte>& packageBytes,
    bool reverseEndpoints = false)
{
    canonical::LeafPackageDeclaration declaration;
    declaration.stableKey = std::move(key);
    declaration.packageBytes = packageBytes;
    declaration.endpoints = {
        {0, "input"},
        {1, "output"}};
    if (reverseEndpoints) std::reverse(declaration.endpoints.begin(), declaration.endpoints.end());
    return declaration;
}

canonical::EndpointReferenceDeclaration Ref(
    std::string leaf,
    std::string endpoint)
{
    return {std::move(leaf), std::move(endpoint)};
}

canonical::ResourceFlowDeclaration InputFlow()
{
    canonical::ResourceFlowDeclaration result;
    result.stableKey = "flow/input";
    result.boundary = canonical::ResourceBoundary::CompositionInput;
    result.consumers = {Ref("producer", "input")};
    return result;
}

canonical::ResourceFlowDeclaration InternalFanOut()
{
    canonical::ResourceFlowDeclaration result;
    result.stableKey = "flow/internal";
    result.boundary = canonical::ResourceBoundary::Internal;
    result.producer = Ref("producer", "output");
    result.consumers = {
        Ref("consumer-a", "input"),
        Ref("consumer-b", "input")};
    return result;
}

canonical::ResourceFlowDeclaration OutputFlow(
    std::string stableKey,
    std::string leaf)
{
    canonical::ResourceFlowDeclaration result;
    result.stableKey = std::move(stableKey);
    result.boundary = canonical::ResourceBoundary::CompositionOutput;
    result.producer = Ref(std::move(leaf), "output");
    return result;
}

canonical::ContractBuildInput MakeFanOutInput(
    const std::vector<std::byte>& packageBytes,
    bool reverseDeclarations = false)
{
    canonical::ContractBuildInput input;
    input.leaves = {
        LeafDeclaration("producer", packageBytes),
        LeafDeclaration("consumer-a", packageBytes),
        LeafDeclaration("consumer-b", packageBytes)};
    input.resources = {
        InputFlow(),
        InternalFanOut(),
        OutputFlow("flow/output-a", "consumer-a"),
        OutputFlow("flow/output-b", "consumer-b")};
    if (reverseDeclarations)
    {
        std::reverse(input.leaves.begin(), input.leaves.end());
        std::reverse(input.resources.begin(), input.resources.end());
        for (auto& leaf : input.leaves) std::reverse(leaf.endpoints.begin(), leaf.endpoints.end());
        for (auto& resource : input.resources)
            std::reverse(resource.consumers.begin(), resource.consumers.end());
    }
    return input;
}

canonical::ContractBuildInput MakeCycleInput(
    const std::vector<std::byte>& packageBytes)
{
    canonical::ContractBuildInput input;
    input.leaves = {
        LeafDeclaration("cycle-a", packageBytes),
        LeafDeclaration("cycle-b", packageBytes)};

    canonical::ResourceFlowDeclaration first;
    first.stableKey = "flow/a-to-b";
    first.boundary = canonical::ResourceBoundary::Internal;
    first.producer = Ref("cycle-a", "output");
    first.consumers = {Ref("cycle-b", "input")};

    canonical::ResourceFlowDeclaration second;
    second.stableKey = "flow/b-to-a";
    second.boundary = canonical::ResourceBoundary::Internal;
    second.producer = Ref("cycle-b", "output");
    second.consumers = {Ref("cycle-a", "input")};
    input.resources = {std::move(first), std::move(second)};
    return input;
}

void ExpectContractRejected(
    canonical::ContractBuildInput input,
    std::string_view message)
{
    if (canonical::BuildCompositionContract(std::move(input)))
        throw std::runtime_error(std::string(message));
}

planning::RawCompositionPlan MakePlausibleCyclicRaw(
    const canonical::ValidatedCompositionContract& validated)
{
    const auto& contract = validated.Contract();
    planning::RawCompositionPlan plan;
    plan.contractIdentity = contract.identity;
    for (const auto& resource : contract.resources)
        plan.allocations.push_back({resource.id,
            planning::AllocationOwnership::CompositionOwned,
            resource.kind, resource.format, resource.sizeBytes});
    for (std::uint32_t index = 0; index < contract.leaves.size(); ++index)
        plan.schedule.push_back({index, {index}});
    for (const auto& binding : contract.bindings)
    {
        const auto& endpoint = contract.endpoints[binding.endpoint.value];
        plan.bindings.push_back({binding.endpoint, binding.resource, endpoint.leaf,
            endpoint.localExternalSlot, endpoint.access});
    }
    for (const auto& resource : contract.resources)
    {
        const auto& producer = contract.endpoints[resource.producer.value];
        const auto signalId = static_cast<std::uint32_t>(plan.signals.size());
        plan.signals.push_back({signalId, resource.id, producer.id, producer.leaf,
            producer.leaf.value});
        for (const auto consumerId : resource.consumers)
        {
            const auto& consumer = contract.endpoints[consumerId.value];
            plan.handoffs.push_back({resource.id, producer.id, consumer.id,
                producer.leaf, consumer.leaf, producer.guaranteedOutgoingState,
                consumer.requiredIncomingState});
        }
    }
    std::sort(plan.handoffs.begin(), plan.handoffs.end(), [](const auto& left, const auto& right) {
        return std::tie(left.resource.value, left.consumer.value) <
               std::tie(right.resource.value, right.consumer.value);
    });
    for (const auto& handoff : plan.handoffs)
    {
        const auto signal = static_cast<std::uint32_t>(std::find_if(
            plan.signals.begin(), plan.signals.end(), [&](const auto& value) {
                return value.resource == handoff.resource;
            }) - plan.signals.begin());
        plan.waits.push_back({static_cast<std::uint32_t>(plan.waits.size()),
            signal, handoff.resource, handoff.consumer, handoff.consumerLeaf,
            handoff.consumerLeaf.value});
    }
    for (const auto& leaf : contract.leaves) plan.recovery.recreateLeaves.push_back(leaf.id);
    for (const auto& resource : contract.resources)
        plan.recovery.recreateResources.push_back(resource.id);
    plan.identity = planning::ComputeRawCompositionPlanIdentity(plan);
    Require(static_cast<bool>(planning::ValidateRawCompositionPlanShape(plan)),
        "plausible cyclic Raw Plan fixture is not shape-valid");
    return plan;
}

void Resign(planning::RawCompositionPlan& plan)
{
    plan.identity = planning::ComputeRawCompositionPlanIdentity(plan);
}

template<class Mutate>
void ExpectResignedPlanRejected(
    const canonical::ValidatedCompositionContract& contract,
    const planning::RawCompositionPlan& source,
    Mutate mutate,
    std::string_view message)
{
    auto changed = source;
    const auto bytesBeforeMutation = planning::SerializeRawCompositionPlan(changed);
    mutate(changed);
    Require(
        planning::SerializeRawCompositionPlan(changed) != bytesBeforeMutation,
        "negative Raw Plan mutation fixture did not change serialized bytes");
    Resign(changed);
    if (verification::VerifyAndSeal(contract, changed))
        throw std::runtime_error(std::string(message));
}

sge4_5::composition::FrozenCompositionBuildInput MakeLowLevelContainerInput(
    const canonical::ValidatedCompositionContract& contract,
    const planning::RawCompositionPlan& plan,
    const verification::VerificationCertificate& certificate)
{
    sge4_5::composition::FrozenCompositionBuildInput input;
    for (const auto& leaf : contract.Leaves())
        input.leaves.push_back({leaf.stableKey, leaf.packageBytes});
    if (contract.Contract().presenterLeaf.IsValid())
    {
        input.hasPresenter = true;
        input.presenterStableKey = contract.Leaves()[
            contract.Contract().presenterLeaf.value].stableKey;
    }
    input.contractBytes = canonical::SerializeCompositionContract(contract.Contract());
    input.verifiedDecisionBytes = planning::SerializeRawCompositionPlan(plan);
    input.verificationCertificateBytes =
        verification::SerializeVerificationCertificate(certificate);
    return input;
}

void ExpectForgedArtifactRejected(
    sge4_5::composition::FrozenCompositionBuildInput input,
    std::string_view message)
{
    auto bytes = sge4_5::composition::FrozenCompositionWriter::Write(std::move(input));
    Require(static_cast<bool>(bytes), "low-level R1 writer rejected the intended R2 forgery fixture");
    if (verification::ReadVerifiedFrozenComposition(std::move(bytes).Value()))
        throw std::runtime_error(std::string(message));
}
}

int main(int argc, char** argv)
{
    try
    {
        const auto leafPackage = CompileLeafPackage();

        auto built = canonical::BuildCompositionContract(
            MakeFanOutInput(leafPackage, false));
        auto reordered = canonical::BuildCompositionContract(
            MakeFanOutInput(leafPackage, true));
        Require(static_cast<bool>(built), "canonical Composition Contract build failed");
        Require(static_cast<bool>(reordered), "reordered Composition Contract build failed");
        auto validated = std::move(built).Value();
        auto reorderedValidated = std::move(reordered).Value();

        const auto contractBytes = canonical::SerializeCompositionContract(
            validated.Contract());
        Require(contractBytes == canonical::SerializeCompositionContract(
            reorderedValidated.Contract()),
            "author declaration order changed canonical Contract bytes");
        auto decodedContract = canonical::DeserializeCompositionContract(contractBytes);
        Require(static_cast<bool>(decodedContract), "canonical Contract decode failed");
        Require(canonical::SerializeCompositionContract(decodedContract.Value()) == contractBytes,
                "canonical Contract round trip changed bytes");

        auto missingEndpoint = MakeFanOutInput(leafPackage);
        missingEndpoint.leaves[0].endpoints.pop_back();
        ExpectContractRejected(std::move(missingEndpoint),
            "missing Leaf endpoint declaration was accepted");

        auto duplicateEndpointKey = MakeFanOutInput(leafPackage);
        duplicateEndpointKey.leaves[0].endpoints[1].stableKey = "input";
        ExpectContractRejected(std::move(duplicateEndpointKey),
            "duplicate Leaf endpoint key was accepted");

        auto wrongProducer = MakeFanOutInput(leafPackage);
        wrongProducer.resources[1].producer = Ref("producer", "input");
        ExpectContractRejected(std::move(wrongProducer),
            "ReadOnly endpoint was accepted as Resource Flow producer");

        auto wrongConsumer = MakeFanOutInput(leafPackage);
        wrongConsumer.resources[1].consumers[0] = Ref("consumer-a", "output");
        ExpectContractRejected(std::move(wrongConsumer),
            "WriteOnly endpoint was accepted as Resource Flow consumer");

        auto duplicateBinding = MakeFanOutInput(leafPackage);
        duplicateBinding.resources[2].producer = Ref("producer", "output");
        ExpectContractRejected(std::move(duplicateBinding),
            "one Endpoint was accepted in two Resource Flows");

        auto unboundEndpoint = MakeFanOutInput(leafPackage);
        unboundEndpoint.resources.pop_back();
        ExpectContractRejected(std::move(unboundEndpoint),
            "unbound required Endpoint was accepted");

        auto wrongBoundary = MakeFanOutInput(leafPackage);
        wrongBoundary.resources[0].producer = Ref("producer", "output");
        ExpectContractRejected(std::move(wrongBoundary),
            "CompositionInput with a producer was accepted");

        auto forgedAccess = validated.Contract();
        forgedAccess.endpoints[0].access =
            forgedAccess.endpoints[0].access == canonical::EndpointAccess::ReadOnly
                ? canonical::EndpointAccess::WriteOnly
                : canonical::EndpointAccess::ReadOnly;
        forgedAccess.identity = canonical::ComputeCompositionContractIdentity(forgedAccess);
        std::vector<canonical::CanonicalLeafPackage> leafCopies(
            validated.Leaves().begin(), validated.Leaves().end());
        Require(!canonical::ValidateCompositionContractAgainstLeaves(
            forgedAccess, leafCopies),
            "Leaf-derived endpoint access forgery was accepted after Contract resigning");

        auto forgedSize = validated.Contract();
        ++forgedSize.endpoints[0].minimumBytes;
        forgedSize.identity = canonical::ComputeCompositionContractIdentity(forgedSize);
        Require(!canonical::ValidateCompositionContractAgainstLeaves(
            forgedSize, leafCopies),
            "Leaf-derived endpoint size forgery was accepted after Contract resigning");

        auto forgedState = validated.Contract();
        forgedState.endpoints[0].requiredIncomingState.explicitBits ^= 1u;
        forgedState.endpoints[0].guaranteedOutgoingState =
            forgedState.endpoints[0].requiredIncomingState;
        forgedState.identity = canonical::ComputeCompositionContractIdentity(forgedState);
        Require(!canonical::ValidateCompositionContractAgainstLeaves(
            forgedState, leafCopies),
            "Leaf-derived endpoint state forgery was accepted after Contract resigning");

        auto twoPresenters = validated.Contract();
        twoPresenters.leaves[0].surfaceSlotCount = 1;
        twoPresenters.leaves[1].surfaceSlotCount = 1;
        twoPresenters.presenterLeaf = twoPresenters.leaves[0].id;
        twoPresenters.identity = canonical::ComputeCompositionContractIdentity(twoPresenters);
        Require(!canonical::ValidateCompositionContractShape(twoPresenters),
            "two presenting Leaves were accepted");

        auto rawResult = planning::ProposeCompositionPlan(validated);
        Require(static_cast<bool>(rawResult), "raw Composition Plan proposal failed");
        auto raw = std::move(rawResult).Value();
        Require(raw.allocations.size() == 4, "fan-out plan allocation count differs");
        Require(raw.schedule.size() == 3, "fan-out plan schedule count differs");
        Require(raw.bindings.size() == 6, "fan-out plan binding count differs");
        Require(raw.handoffs.size() == 2, "fan-out plan handoff count differs");
        Require(raw.signals.size() == 1, "fan-out plan signal count differs");
        Require(raw.waits.size() == 2, "fan-out plan wait count differs");
        Require(raw.recovery.recreateLeaves.size() == 3,
                "whole-composition Leaf recovery set differs");
        Require(raw.recovery.recreateResources.size() == 1,
                "composition-owned Resource recovery set differs");

        auto rawRoundTrip = planning::DeserializeRawCompositionPlan(
            planning::SerializeRawCompositionPlan(raw));
        Require(static_cast<bool>(rawRoundTrip), "raw Plan decode failed");
        Require(planning::SerializeRawCompositionPlan(rawRoundTrip.Value()) ==
                    planning::SerializeRawCompositionPlan(raw),
                "raw Plan round trip changed bytes");

        auto cyclic = canonical::BuildCompositionContract(MakeCycleInput(leafPackage));
        Require(static_cast<bool>(cyclic), "cyclic Contract construction unexpectedly failed");
        Require(!planning::ProposeCompositionPlan(cyclic.Value()),
                "cyclic same-frame Composition produced a raw Plan");
        auto plausibleCyclePlan = MakePlausibleCyclicRaw(cyclic.Value());
        Require(!verification::VerifyAndSeal(cyclic.Value(), plausibleCyclePlan),
                "independent verifier accepted a cyclic same-frame Composition");

        auto corruptIdentity = raw;
        corruptIdentity.identity[0] ^= std::byte{1};
        Require(!verification::VerifyAndSeal(validated, corruptIdentity),
                "raw Plan with corrupt identity was verified");

        ExpectResignedPlanRejected(validated, raw, [](auto& plan) {
            plan.contractIdentity[0] ^= std::byte{1};
        }, "resigned Contract identity mutation was verified");
        ExpectResignedPlanRejected(validated, raw, [](auto& plan) {
            ++plan.allocations[0].sizeBytes;
        }, "resigned allocation mutation was verified");
        ExpectResignedPlanRejected(validated, raw, [](auto& plan) {
            std::swap(plan.schedule[1].leaf, plan.schedule[2].leaf);
        }, "resigned schedule mutation was verified");
        ExpectResignedPlanRejected(validated, raw, [](auto& plan) {
            plan.bindings[0].resource = plan.bindings[1].resource;
        }, "resigned binding mutation was verified");
        ExpectResignedPlanRejected(validated, raw, [](auto& plan) {
            plan.bindings[0].access =
                plan.bindings[0].access == canonical::EndpointAccess::ReadOnly
                    ? canonical::EndpointAccess::WriteOnly
                    : canonical::EndpointAccess::ReadOnly;
        }, "resigned binding access mutation was verified");
        ExpectResignedPlanRejected(validated, raw, [](auto& plan) {
            plan.handoffs[0].consumerIncomingState.explicitBits ^= 1u;
        }, "resigned state handoff mutation was verified");
        ExpectResignedPlanRejected(validated, raw, [](auto& plan) {
            plan.signals[0].producerScheduleOrdinal = plan.schedule.back().ordinal;
        }, "resigned signal mutation was verified");
        ExpectResignedPlanRejected(validated, raw, [](auto& plan) {
            plan.waits[0].signal = sge4_5::base::InvalidIndex;
        }, "resigned wait mutation was verified");
        ExpectResignedPlanRejected(validated, raw, [](auto& plan) {
            plan.recovery.recreateResources.clear();
        }, "resigned recovery mutation was verified");

        auto verifiedResult = verification::VerifyAndSeal(validated, raw);
        Require(static_cast<bool>(verifiedResult), "independent verification failed");
        auto verified = std::move(verifiedResult).Value();
        Require(static_cast<bool>(verification::ValidateVerifiedPlan(validated, verified)),
                "verified Plan validation failed");

        auto frozenBytesResult = verification::FreezeVerifiedComposition(validated, verified);
        Require(static_cast<bool>(frozenBytesResult), "verified-only freeze failed");
        auto frozenBytes = std::move(frozenBytesResult).Value();
        auto frozen = verification::ReadVerifiedFrozenComposition(frozenBytes);
        Require(static_cast<bool>(frozen), "verified Frozen Composition read failed");
        Require(frozen.Value().ValidatedContract().Contract().identity ==
                    validated.Contract().identity,
                "decoded Contract authority differs");
        Require(frozen.Value().VerifiedPlan().Plan().identity == raw.identity,
                "decoded verified Plan authority differs");

        auto forgedPlan = raw;
        ++forgedPlan.allocations[0].sizeBytes;
        Resign(forgedPlan);
        ExpectForgedArtifactRejected(
            MakeLowLevelContainerInput(validated, forgedPlan, verified.Certificate()),
            "resigned forged Plan inside a digest-valid Artifact was accepted");

        auto forgedCertificate = verified.Certificate();
        forgedCertificate.seal[0] ^= std::byte{1};
        ExpectForgedArtifactRejected(
            MakeLowLevelContainerInput(validated, raw, forgedCertificate),
            "forged verification certificate inside a digest-valid Artifact was accepted");

        auto forgedContract = validated.Contract();
        ++forgedContract.endpoints[0].minimumBytes;
        forgedContract.identity = canonical::ComputeCompositionContractIdentity(forgedContract);
        auto forgedContractContainer = MakeLowLevelContainerInput(
            validated, raw, verified.Certificate());
        forgedContractContainer.contractBytes =
            canonical::SerializeCompositionContract(forgedContract);
        ExpectForgedArtifactRejected(
            std::move(forgedContractContainer),
            "resigned forged Contract inside a digest-valid Artifact was accepted");

        if (argc == 3 && std::string_view(argv[1]) == "--emit")
        {
            auto written = sge4_5::base::WriteAllBytes(argv[2], frozenBytes);
            Require(static_cast<bool>(written), "failed to emit R2 Frozen Composition Artifact");
        }

        std::cout << "R2 Composition Contract and Independent Verification tests passed\n";
        std::cout << "Contract: Leaf-derived Buffer endpoint authority and canonical Resource Flows\n";
        std::cout << "Raw Plan: proposal only; independent verifier re-derived every represented decision\n";
        std::cout << "Artifact bytes: " << frozenBytes.size() << '\n';
        std::cout << "Artifact SHA-256: "
                  << sge4_5::base::ToHex(sge4_5::base::Sha256(frozenBytes)) << '\n';
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "R2 qualification failed: " << exception.what() << '\n';
        return 1;
    }
}
