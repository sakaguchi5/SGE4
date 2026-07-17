#include "../00_Foundation/FileIO.h"
#include "../00_Foundation/Sha256.h"
#include "../09_FrozenPackageCore/PackageReader.h"
#include "../10A_D3D12CompositionSchema18/D3D12CompositionSchema18.h"
#include "../12_SGE4Compiler/SGE4Compiler.h"
#include "../12A_CompositionLeafCompiler/CompositionLeafCompiler.h"
#include "../16A_Schema18CompositionContract/Schema18CompositionContract.h"
#include "../27A_Schema18LeafCorpus/Schema18LeafCorpus.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
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

const corpus::LeafCase* FindCase(
    const std::vector<corpus::LeafCase>& values,
    std::string_view name)
{
    const auto found = std::find_if(values.begin(), values.end(), [&](const auto& value) {
        return value.name == name;
    });
    return found == values.end() ? nullptr : &*found;
}

sge4::base::Result<CompiledLeaf, std::string> CompileLeaf(const corpus::LeafCase& item)
{
    std::vector<compile18::EndpointDeclaration> declarations;
    for (const auto& endpoint : item.endpoints)
        declarations.push_back({endpoint.resource, endpoint.stableKey});
    auto compiled = compile18::CompileCompositionLeafCanonical(
        item.leafGraph, item.leafProfile, std::move(declarations));
    if (!compiled)
        return sge4::base::Result<CompiledLeaf, std::string>::Failure(
            compiled.Error().stage + ": " + compiled.Error().message);

    CompiledLeaf result;
    result.packageBytes = std::move(compiled).Value().packageBytes;
    auto frozen = sge4::package::PackageReader::Read(result.packageBytes);
    if (!frozen)
        return sge4::base::Result<CompiledLeaf, std::string>::Failure(frozen.Error().message);
    auto view = pkg18::CompositionLeafView::Decode(frozen.Value());
    if (!view)
        return sge4::base::Result<CompiledLeaf, std::string>::Failure(view.Error().message);

    for (const auto& authored : item.endpoints)
    {
        const auto stableKey = pkg18::ComputeStableEndpointKey(authored.stableKey);
        const auto found = std::find_if(view.Value().Endpoints().begin(), view.Value().Endpoints().end(),
            [&](const auto& endpoint) { return endpoint.stableKey == stableKey; });
        if (found == view.Value().Endpoints().end())
            return sge4::base::Result<CompiledLeaf, std::string>::Failure(
                "compiled Leaf lost an authored endpoint");
        if (found->access == pkg18::EndpointAccess::ReadOnly)
            result.readEndpointKey = authored.stableKey;
        else if (found->access == pkg18::EndpointAccess::WriteOnly)
            result.writeEndpointKey = authored.stableKey;
    }
    if (item.endpoints.size() == 2 &&
        (result.readEndpointKey.empty() || result.writeEndpointKey.empty()))
        return sge4::base::Result<CompiledLeaf, std::string>::Failure(
            "two-endpoint F3 Leaf did not expose one reader and one writer");
    return sge4::base::Result<CompiledLeaf, std::string>::Success(std::move(result));
}

contract::EndpointReferenceDeclaration Ref(
    std::string leaf,
    std::string endpoint)
{
    return {std::move(leaf), std::move(endpoint)};
}

contract::ContractBuildInput MakeValidInput(
    const CompiledLeaf& producer,
    const CompiledLeaf& consumer)
{
    contract::ContractBuildInput input;
    input.leaves = {
        {"f3/producer", producer.packageBytes},
        {"f3/consumer/a", consumer.packageBytes},
        {"f3/consumer/b", consumer.packageBytes}};

    contract::ResourceFlowDeclaration compositionInput;
    compositionInput.stableKey = "f3/resource/input";
    compositionInput.boundary = contract::ResourceBoundary::CompositionInput;
    compositionInput.consumers = {Ref("f3/producer", producer.readEndpointKey)};

    contract::ResourceFlowDeclaration handoff;
    handoff.stableKey = "f3/resource/handoff";
    handoff.boundary = contract::ResourceBoundary::Internal;
    handoff.producer = Ref("f3/producer", producer.writeEndpointKey);
    handoff.consumers = {
        Ref("f3/consumer/a", consumer.readEndpointKey),
        Ref("f3/consumer/b", consumer.readEndpointKey)};

    contract::ResourceFlowDeclaration outputA;
    outputA.stableKey = "f3/resource/output/a";
    outputA.boundary = contract::ResourceBoundary::CompositionOutput;
    outputA.producer = Ref("f3/consumer/a", consumer.writeEndpointKey);

    contract::ResourceFlowDeclaration outputB;
    outputB.stableKey = "f3/resource/output/b";
    outputB.boundary = contract::ResourceBoundary::CompositionOutput;
    outputB.producer = Ref("f3/consumer/b", consumer.writeEndpointKey);

    input.resources = {
        std::move(compositionInput), std::move(handoff),
        std::move(outputA), std::move(outputB)};
    return input;
}

bool WriteBytes(const std::filesystem::path& path, std::span<const std::byte> bytes)
{
    std::filesystem::create_directories(path.parent_path());
    return static_cast<bool>(sge4::base::WriteAllBytes(path, bytes));
}

bool Failed(const sge4::base::Result<contract::PackageCompositionContract, contract::ContractError>& value)
{
    return !value;
}
}

int main(int argc, char** argv)
{
    auto corpusResult = corpus::BuildLeafCorpus();
    if (!corpusResult) { std::cerr << corpusResult.Error() << '\n'; return 1; }
    const auto& cases = corpusResult.Value();
    const auto* producerCase = FindCase(cases, "StageK/DynamicExternalDedicated");
    const auto* consumerCase = FindCase(cases, "StageK/DynamicExternalDirectFallback");
    if (producerCase == nullptr || consumerCase == nullptr) return 2;

    auto producerResult = CompileLeaf(*producerCase);
    auto consumerResult = CompileLeaf(*consumerCase);
    if (!producerResult || !consumerResult)
    {
        std::cerr << (producerResult ? consumerResult.Error() : producerResult.Error()) << '\n';
        return 3;
    }
    const auto& producer = producerResult.Value();
    const auto& consumer = consumerResult.Value();

    auto input = MakeValidInput(producer, consumer);
    auto built = contract::BuildSchema18CompositionContract(input);
    if (!built)
    {
        std::cerr << built.Error().stage << ": " << built.Error().message << '\n';
        return 4;
    }
    const auto& value = built.Value();
    if (value.leaves.size() != 3 || value.endpoints.size() != 6 ||
        value.resources.size() != 4 || value.bindings.size() != 6 ||
        value.presenterLeaf.IsValid()) return 5;
    if (!contract::ValidateCompositionContract(value)) return 6;

    auto reorderedInput = MakeValidInput(producer, consumer);
    std::reverse(reorderedInput.leaves.begin(), reorderedInput.leaves.end());
    std::reverse(reorderedInput.resources.begin(), reorderedInput.resources.end());
    for (auto& resource : reorderedInput.resources)
        std::reverse(resource.consumers.begin(), resource.consumers.end());
    auto reordered = contract::BuildSchema18CompositionContract(std::move(reorderedInput));
    if (!reordered || reordered.Value().identity != value.identity ||
        contract::SerializeCompositionContract(reordered.Value()) !=
        contract::SerializeCompositionContract(value)) return 7;

    auto repeated = contract::BuildSchema18CompositionContract(MakeValidInput(producer, consumer));
    if (!repeated || contract::SerializeCompositionContract(repeated.Value()) !=
        contract::SerializeCompositionContract(value)) return 8;

    // Schema 17 bytes are not Composition Contracts, even when their semantics match.
    auto schema17 = sge4::compiler::CompileCanonical(
        producerCase->leafGraph, producerCase->leafProfile);
    if (!schema17) return 9;
    auto schema17Input = MakeValidInput(producer, consumer);
    schema17Input.leaves[0].packageBytes = schema17.Value().packageBytes;
    if (!Failed(contract::BuildSchema18CompositionContract(std::move(schema17Input)))) return 10;

    auto missing = MakeValidInput(producer, consumer);
    missing.resources.pop_back();
    if (!Failed(contract::BuildSchema18CompositionContract(std::move(missing)))) return 11;

    auto writerAsConsumer = MakeValidInput(producer, consumer);
    writerAsConsumer.resources[0].consumers[0].endpointKey = producer.writeEndpointKey;
    if (!Failed(contract::BuildSchema18CompositionContract(std::move(writerAsConsumer)))) return 12;

    auto readerAsProducer = MakeValidInput(producer, consumer);
    readerAsProducer.resources[1].producer->endpointKey = producer.readEndpointKey;
    if (!Failed(contract::BuildSchema18CompositionContract(std::move(readerAsProducer)))) return 13;

    auto duplicateBinding = MakeValidInput(producer, consumer);
    duplicateBinding.resources[1].consumers.push_back(
        Ref("f3/producer", producer.readEndpointKey));
    if (!Failed(contract::BuildSchema18CompositionContract(std::move(duplicateBinding)))) return 14;

    auto duplicateLeafKey = MakeValidInput(producer, consumer);
    duplicateLeafKey.leaves[1].stableKey = duplicateLeafKey.leaves[0].stableKey;
    if (!Failed(contract::BuildSchema18CompositionContract(std::move(duplicateLeafKey)))) return 15;

    auto duplicateResourceKey = MakeValidInput(producer, consumer);
    duplicateResourceKey.resources[1].stableKey = duplicateResourceKey.resources[0].stableKey;
    if (!Failed(contract::BuildSchema18CompositionContract(std::move(duplicateResourceKey)))) return 16;

    auto unknownEndpoint = MakeValidInput(producer, consumer);
    unknownEndpoint.resources[0].consumers[0].endpointKey = "unknown/endpoint";
    if (!Failed(contract::BuildSchema18CompositionContract(std::move(unknownEndpoint)))) return 17;

    auto invalidBoundary = MakeValidInput(producer, consumer);
    invalidBoundary.resources[0].producer = Ref("f3/producer", producer.writeEndpointKey);
    if (!Failed(contract::BuildSchema18CompositionContract(std::move(invalidBoundary)))) return 18;

    auto tampered = MakeValidInput(producer, consumer);
    tampered.leaves[0].packageBytes.back() ^= std::byte{0x01};
    if (!Failed(contract::BuildSchema18CompositionContract(std::move(tampered)))) return 19;

    auto forgedAccess = value;
    forgedAccess.endpoints[0].access = pkg18::EndpointAccess::WriteOnly;
    forgedAccess.identity = contract::ComputeContractIdentity(forgedAccess);
    if (contract::ValidateCompositionContract(forgedAccess)) return 20;

    auto forgedSize = value;
    forgedSize.resources[0].sizeBytes += 16;
    forgedSize.identity = contract::ComputeContractIdentity(forgedSize);
    if (contract::ValidateCompositionContract(forgedSize)) return 21;

    // Two presentation Leaves are structurally valid Schema-18 Packages but are
    // outside the one-presenter Level 4 v1 contract.
    const auto* rasterA = FindCase(cases, "Level1/Slice 1 Fixed Triangle [Neutral]");
    const auto* rasterB = FindCase(cases, "Level1/Slice 2 Dynamic constant slot / FrameInvocation [Neutral]");
    if (rasterA == nullptr || rasterB == nullptr) return 22;
    auto presenterA = CompileLeaf(*rasterA);
    auto presenterB = CompileLeaf(*rasterB);
    if (!presenterA || !presenterB) return 23;
    contract::ContractBuildInput twoPresenters;
    twoPresenters.leaves = {
        {"f3/presenter/a", presenterA.Value().packageBytes},
        {"f3/presenter/b", presenterB.Value().packageBytes}};
    if (!Failed(contract::BuildSchema18CompositionContract(std::move(twoPresenters)))) return 24;

    const auto artifact = contract::SerializeCompositionContract(value);
    if (sge4::base::Sha256(artifact) != value.identity) return 25;
    if (argc == 3 && std::string_view(argv[1]) == "--emit")
    {
        if (!WriteBytes(argv[2], artifact)) return 26;
    }

    std::cout << "L4V1-F3 Schema 18 Composition Contract tests passed\n";
    std::cout << "Leaves: 3; endpoints: 6; Resource Flows: 4; bindings: 6\n";
    std::cout << "Authority: Leaf endpoints derive producer/consumer access, shape, size, state, and synchronization\n";
    std::cout << "Contract identity: " << sge4::base::ToHex(value.identity) << '\n';
    return 0;
}
