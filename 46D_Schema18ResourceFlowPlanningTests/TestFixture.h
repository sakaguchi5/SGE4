#pragma once

#include "../09_FrozenPackageCore/PackageReader.h"
#include "../10A_D3D12CompositionSchema18/D3D12CompositionSchema18.h"
#include "../12A_CompositionLeafCompiler/CompositionLeafCompiler.h"
#include "../16A_Schema18CompositionContract/Schema18CompositionContract.h"
#include "../27A_Schema18LeafCorpus/Schema18LeafCorpus.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sge4::qualification::schema18_planning_fixture
{
namespace corpus = sge4::qualification::schema18_corpus;
namespace compile18 = sge4::compiler::composition;
namespace contract = sge4::composition::schema18;
namespace pkg18 = sge4::package::d3d12_v18;

struct CompiledLeaf final
{
    std::vector<std::byte> packageBytes;
    std::string readEndpointKey;
    std::string writeEndpointKey;
};

struct Fixture final
{
    contract::PackageCompositionContract contract;
    CompiledLeaf producer;
    CompiledLeaf consumer;
};

inline const corpus::LeafCase* FindCase(
    const std::vector<corpus::LeafCase>& values,
    std::string_view name)
{
    const auto found = std::find_if(values.begin(), values.end(),
        [&](const auto& value) { return value.name == name; });
    return found == values.end() ? nullptr : &*found;
}

inline base::Result<CompiledLeaf, std::string>
CompileLeaf(const corpus::LeafCase& item)
{
    std::vector<compile18::EndpointDeclaration> declarations;
    for (const auto& endpoint : item.endpoints)
        declarations.push_back({endpoint.resource, endpoint.stableKey});
    auto compiled = compile18::CompileCompositionLeafCanonical(
        item.leafGraph, item.leafProfile, std::move(declarations));
    if (!compiled)
        return base::Result<CompiledLeaf, std::string>::Failure(
            compiled.Error().stage + ": " + compiled.Error().message);

    CompiledLeaf result;
    result.packageBytes = std::move(compiled).Value().packageBytes;
    auto frozen = package::PackageReader::Read(result.packageBytes);
    if (!frozen)
        return base::Result<CompiledLeaf, std::string>::Failure(
            frozen.Error().message);
    auto view = pkg18::CompositionLeafView::Decode(frozen.Value());
    if (!view)
        return base::Result<CompiledLeaf, std::string>::Failure(
            view.Error().message);
    for (const auto& authored : item.endpoints)
    {
        const auto key = pkg18::ComputeStableEndpointKey(authored.stableKey);
        const auto found = std::find_if(
            view.Value().Endpoints().begin(), view.Value().Endpoints().end(),
            [&](const auto& endpoint) { return endpoint.stableKey == key; });
        if (found == view.Value().Endpoints().end())
            return base::Result<CompiledLeaf, std::string>::Failure(
                "compiled Leaf lost an authored endpoint");
        if (found->access == pkg18::EndpointAccess::ReadOnly)
            result.readEndpointKey = authored.stableKey;
        else if (found->access == pkg18::EndpointAccess::WriteOnly)
            result.writeEndpointKey = authored.stableKey;
    }
    if (result.readEndpointKey.empty() || result.writeEndpointKey.empty())
        return base::Result<CompiledLeaf, std::string>::Failure(
            "planning fixture requires one ReadOnly and one WriteOnly endpoint");
    return base::Result<CompiledLeaf, std::string>::Success(std::move(result));
}

inline contract::EndpointReferenceDeclaration Ref(
    std::string leaf, std::string endpoint)
{
    return {std::move(leaf), std::move(endpoint)};
}

inline contract::ContractBuildInput MakeMainInput(
    const CompiledLeaf& producer,
    const CompiledLeaf& consumer)
{
    contract::ContractBuildInput input;
    input.leaves = {
        {"f4/producer", producer.packageBytes},
        {"f4/consumer/a", consumer.packageBytes},
        {"f4/consumer/b", consumer.packageBytes}};

    contract::ResourceFlowDeclaration compositionInput;
    compositionInput.stableKey = "f4/resource/input";
    compositionInput.boundary = contract::ResourceBoundary::CompositionInput;
    compositionInput.consumers = {
        Ref("f4/producer", producer.readEndpointKey)};

    contract::ResourceFlowDeclaration handoff;
    handoff.stableKey = "f4/resource/handoff";
    handoff.boundary = contract::ResourceBoundary::Internal;
    handoff.producer = Ref("f4/producer", producer.writeEndpointKey);
    handoff.consumers = {
        Ref("f4/consumer/a", consumer.readEndpointKey),
        Ref("f4/consumer/b", consumer.readEndpointKey)};

    contract::ResourceFlowDeclaration outputA;
    outputA.stableKey = "f4/resource/output/a";
    outputA.boundary = contract::ResourceBoundary::CompositionOutput;
    outputA.producer = Ref("f4/consumer/a", consumer.writeEndpointKey);

    contract::ResourceFlowDeclaration outputB;
    outputB.stableKey = "f4/resource/output/b";
    outputB.boundary = contract::ResourceBoundary::CompositionOutput;
    outputB.producer = Ref("f4/consumer/b", consumer.writeEndpointKey);

    input.resources = {
        std::move(compositionInput), std::move(handoff),
        std::move(outputA), std::move(outputB)};
    return input;
}

inline base::Result<Fixture, std::string> BuildFixture()
{
    auto corpusResult = corpus::BuildLeafCorpus();
    if (!corpusResult)
        return base::Result<Fixture, std::string>::Failure(corpusResult.Error());
    const auto* producerCase = FindCase(
        corpusResult.Value(), "StageK/DynamicExternalDedicated");
    const auto* consumerCase = FindCase(
        corpusResult.Value(), "StageK/DynamicExternalDirectFallback");
    if (producerCase == nullptr || consumerCase == nullptr)
        return base::Result<Fixture, std::string>::Failure(
            "F2 planning fixture cases are missing");
    auto producer = CompileLeaf(*producerCase);
    auto consumer = CompileLeaf(*consumerCase);
    if (!producer || !consumer)
        return base::Result<Fixture, std::string>::Failure(
            producer ? consumer.Error() : producer.Error());
    auto built = contract::BuildSchema18CompositionContract(
        MakeMainInput(producer.Value(), consumer.Value()));
    if (!built)
        return base::Result<Fixture, std::string>::Failure(
            built.Error().stage + ": " + built.Error().message);
    Fixture fixture;
    fixture.contract = std::move(built).Value();
    fixture.producer = std::move(producer).Value();
    fixture.consumer = std::move(consumer).Value();
    return base::Result<Fixture, std::string>::Success(std::move(fixture));
}

inline base::Result<contract::PackageCompositionContract, std::string>
BuildCycleContract(const CompiledLeaf& left, const CompiledLeaf& right)
{
    contract::ContractBuildInput input;
    input.leaves = {
        {"f4/cycle/a", left.packageBytes},
        {"f4/cycle/b", right.packageBytes}};

    contract::ResourceFlowDeclaration aToB;
    aToB.stableKey = "f4/cycle/a-to-b";
    aToB.boundary = contract::ResourceBoundary::Internal;
    aToB.producer = Ref("f4/cycle/a", left.writeEndpointKey);
    aToB.consumers = {Ref("f4/cycle/b", right.readEndpointKey)};

    contract::ResourceFlowDeclaration bToA;
    bToA.stableKey = "f4/cycle/b-to-a";
    bToA.boundary = contract::ResourceBoundary::Internal;
    bToA.producer = Ref("f4/cycle/b", right.writeEndpointKey);
    bToA.consumers = {Ref("f4/cycle/a", left.readEndpointKey)};
    input.resources = {std::move(aToB), std::move(bToA)};

    auto built = contract::BuildSchema18CompositionContract(std::move(input));
    if (!built)
        return base::Result<contract::PackageCompositionContract, std::string>::Failure(
            built.Error().stage + ": " + built.Error().message);
    return base::Result<contract::PackageCompositionContract, std::string>::Success(
        std::move(built).Value());
}
}
