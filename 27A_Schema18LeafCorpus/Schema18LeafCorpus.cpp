#include "Schema18LeafCorpus.h"

#include "../00_Foundation/BinaryIO.h"
#include "../00_Foundation/Sha256.h"
#include "../04_SemanticAnalysis/SemanticAnalysis.h"
#include "../24_SliceScenarios/SliceScenarios.h"
#include "../25_GeneralGraphScenarios/GeneralGraphScenarios.h"
#include "../26_GeneratedGraphCorpus/GeneratedGraphs.h"
#include "../27_RuntimeScenarios/RuntimeScenarios.h"

#include <algorithm>
#include <array>
#include <iomanip>
#include <iterator>
#include <map>
#include <set>
#include <span>
#include <sstream>
#include <string_view>
#include <tuple>
#include <utility>

namespace sge4::qualification::schema18_corpus
{
namespace
{
namespace base = sge4::base;
namespace sem = sge4::semantic;
namespace analysis = sge4::analysis;
namespace lvl1 = sge4::level1;
namespace qual = sge4::qualification;
namespace gen = sge4::qualification::generated;
namespace runtime_cases = sge4::qualification::runtime_scenarios;

constexpr std::string_view SourceSemanticIdentity =
    "SGE3-Canonical-D3D12-v1-FinalFreeze-Corpus1";
constexpr std::string_view SourceBaselineCommit =
    "fc6b883b20d5428a4bf4f82b072fab15e8cb844a";
constexpr std::string_view LeafSemanticIdentity =
    "SGE4-L4V1-F2-Schema18-Leaf-Semantic-Corpus-v1";
constexpr std::string_view LeafBaselineCommit =
    "94c8cb8915437e25e3e0d6a3ae3c5277940114a0";
constexpr std::string_view EndpointAuthoringIdentity =
    "SGE4-L4V1-F2-Schema18-Endpoint-Authoring-v1";
constexpr std::size_t SourceCorpusCount = FrozenCaseCount;

struct SourceCase final
{
    std::string name;
    sem::SemanticGraph graph;
    sge4::target::D3D12TargetProfile profile;
};

void WriteString(base::BinaryWriter& writer, std::string_view value)
{
    writer.WriteU64(static_cast<std::uint64_t>(value.size()));
    writer.WriteBytes(std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(value.data()), value.size()));
}

void WriteId(base::BinaryWriter& writer, std::uint32_t value)
{
    writer.WriteU32(value);
}

template<class T, class F>
std::vector<const T*> SortedPointers(const std::vector<T>& values, F&& less)
{
    std::vector<const T*> result;
    result.reserve(values.size());
    for (const auto& value : values) result.push_back(&value);
    std::sort(result.begin(), result.end(), std::forward<F>(less));
    return result;
}

base::Result<base::Digest256, std::string> SemanticDigest(const SourceCase& value, std::string_view identity)
{
    auto analyzed = analysis::Analyze(value.graph);
    if (!analyzed)
    {
        std::string message = "semantic corpus analysis failed";
        if (!analyzed.Error().empty()) message += ": " + analyzed.Error().front().message;
        return base::Result<base::Digest256, std::string>::Failure(std::move(message));
    }

    base::BinaryWriter writer;
    WriteString(writer, identity);
    WriteString(writer, value.name);

    const auto& profile = value.profile;
    writer.WriteU32(profile.minimumFeatureLevel);
    writer.WriteU16(profile.shaderModelMajor);
    writer.WriteU16(profile.shaderModelMinor);
    writer.WriteU16(profile.rootSignatureMajor);
    writer.WriteU16(profile.rootSignatureMinor);
    writer.WriteU32(static_cast<std::uint32_t>(profile.barrierModel));
    writer.WriteU16(static_cast<std::uint16_t>(profile.shaderBinaryFormat));
    writer.WriteU32(profile.framesInFlight);
    writer.WriteU32(profile.directQueueCount);
    writer.WriteU32(profile.computeQueueCount);
    writer.WriteU32(profile.copyQueueCount);
    writer.WriteU32(profile.surfaceImageCount);
    writer.WriteU32(profile.rtvDescriptorCount);
    writer.WriteU32(profile.dsvDescriptorCount);
    writer.WriteU32(profile.shaderDescriptorCount);
    writer.WriteU32(profile.samplerDescriptorCount);

    const auto resources = SortedPointers(value.graph.resources, [](const auto* left, const auto* right) {
        return left->id.value < right->id.value;
    });
    writer.WriteU32(static_cast<std::uint32_t>(resources.size()));
    for (const auto* resource : resources)
    {
        WriteId(writer, resource->id.value);
        writer.WriteU16(static_cast<std::uint16_t>(resource->kind));
        writer.WriteU16(static_cast<std::uint16_t>(resource->lifetime));
        writer.WriteU16(static_cast<std::uint16_t>(resource->update));
        writer.WriteU16(static_cast<std::uint16_t>(resource->visibility));
        writer.WriteU64(resource->buffer.sizeBytes);
        writer.WriteU32(resource->buffer.strideBytes);
        writer.WriteU16(static_cast<std::uint16_t>(resource->texture2D.extentMeaning));
        writer.WriteU32(resource->texture2D.width);
        writer.WriteU32(resource->texture2D.height);
        writer.WriteU16(static_cast<std::uint16_t>(resource->texture2D.formatMeaning));
        writer.WriteU32(resource->texture2D.rowBytes);
        writer.WriteU16(resource->texture2D.mipLevels);
        writer.WriteU64(resource->dynamicData.requiredBytes);
        writer.WriteU32(resource->dynamicData.requiredAlignment);
        writer.WriteU16(static_cast<std::uint16_t>(resource->surface.formatMeaning));
        writer.WriteU64(static_cast<std::uint64_t>(resource->initialContent.size()));
        writer.WriteBytes(resource->initialContent);
        WriteId(writer, resource->aliasPreparation.value);
    }

    const auto uses = SortedPointers(value.graph.resourceUses, [](const auto* left, const auto* right) {
        return left->id.value < right->id.value;
    });
    writer.WriteU32(static_cast<std::uint32_t>(uses.size()));
    for (const auto* use : uses)
    {
        WriteId(writer, use->id.value);
        WriteId(writer, use->resource.value);
        writer.WriteU16(static_cast<std::uint16_t>(use->effect));
        writer.WriteU16(static_cast<std::uint16_t>(use->role));
        writer.WriteU16(static_cast<std::uint16_t>(use->temporalRelation));
    }

    const auto programs = SortedPointers(value.graph.programs, [](const auto* left, const auto* right) {
        return left->id.value < right->id.value;
    });
    writer.WriteU32(static_cast<std::uint32_t>(programs.size()));
    for (const auto* program : programs)
    {
        WriteId(writer, program->id.value);
        writer.WriteU16(static_cast<std::uint16_t>(program->kind));
        writer.WriteU32(program->interface.vertexStrideBytes);

        const auto vertexInputs = SortedPointers(program->interface.vertexInputs,
            [](const auto* left, const auto* right) {
                return std::tuple{left->byteOffset, left->meaning, left->componentCount} <
                       std::tuple{right->byteOffset, right->meaning, right->componentCount};
            });
        writer.WriteU32(static_cast<std::uint32_t>(vertexInputs.size()));
        for (const auto* input : vertexInputs)
        {
            writer.WriteU16(static_cast<std::uint16_t>(input->meaning));
            writer.WriteU16(input->componentCount);
            writer.WriteU32(input->byteOffset);
        }

        const auto parameters = SortedPointers(program->interface.parameters,
            [](const auto* left, const auto* right) { return left->id.value < right->id.value; });
        writer.WriteU32(static_cast<std::uint32_t>(parameters.size()));
        for (const auto* parameter : parameters)
        {
            WriteId(writer, parameter->id.value);
            writer.WriteU16(static_cast<std::uint16_t>(parameter->kind));
            writer.WriteU16(static_cast<std::uint16_t>(parameter->stage));
            writer.WriteU32(parameter->shaderRegister);
            writer.WriteU64(parameter->requiredBytes);
            writer.WriteU32(parameter->requiredAlignment);
        }
        WriteString(writer, program->source.hlslSource);
        WriteString(writer, program->source.vertexEntry);
        WriteString(writer, program->source.pixelEntry);
        WriteString(writer, program->source.computeEntry);
    }

    const auto works = SortedPointers(value.graph.works, [](const auto* left, const auto* right) {
        return left->id.value < right->id.value;
    });
    writer.WriteU32(static_cast<std::uint32_t>(works.size()));
    for (const auto* work : works)
    {
        WriteId(writer, work->id.value);
        writer.WriteU16(static_cast<std::uint16_t>(work->kind));
        WriteId(writer, work->raster.program.value);
        writer.WriteU32(work->raster.vertexCount);
        writer.WriteU64(work->copy.bytes);
        WriteId(writer, work->compute.program.value);
        writer.WriteU32(work->compute.threadGroupCountX);
        writer.WriteU32(work->compute.threadGroupCountY);
        writer.WriteU32(work->compute.threadGroupCountZ);

        const auto operands = SortedPointers(work->operands, [](const auto* left, const auto* right) {
            return std::tuple{left->kind, left->parameter.value, left->use.value} <
                   std::tuple{right->kind, right->parameter.value, right->use.value};
        });
        writer.WriteU32(static_cast<std::uint32_t>(operands.size()));
        for (const auto* operand : operands)
        {
            writer.WriteU16(static_cast<std::uint16_t>(operand->kind));
            WriteId(writer, operand->use.value);
            WriteId(writer, operand->parameter.value);
        }
    }

    writer.WriteU32(static_cast<std::uint32_t>(analyzed.Value().canonicalWorkOrder.size()));
    for (const auto work : analyzed.Value().canonicalWorkOrder) WriteId(writer, work.value);

    auto dependencies = analyzed.Value().dependencies;
    std::sort(dependencies.begin(), dependencies.end(), [](const auto& left, const auto& right) {
        return std::tuple{left.producer.value, left.consumer.value,
                          static_cast<std::uint16_t>(left.kind), left.resource.value} <
               std::tuple{right.producer.value, right.consumer.value,
                          static_cast<std::uint16_t>(right.kind), right.resource.value};
    });
    writer.WriteU32(static_cast<std::uint32_t>(dependencies.size()));
    for (const auto& dependency : dependencies)
    {
        WriteId(writer, dependency.producer.value);
        WriteId(writer, dependency.consumer.value);
        WriteId(writer, dependency.resource.value);
        writer.WriteU16(static_cast<std::uint16_t>(dependency.kind));
    }

    return base::Result<base::Digest256, std::string>::Success(base::Sha256(writer.Bytes()));
}

sem::SemanticGraph BuildStageGHeadlessGraph()
{
    sem::SemanticGraph graph;
    sem::Resource output;
    output.id = {0};
    output.kind = sem::ResourceKind::Buffer;
    output.lifetime = sem::LifetimeIntent::Persistent;
    output.update = sem::UpdateIntent::GpuWritten;
    output.visibility = sem::Visibility::Internal;
    output.buffer = {16, 16};
    graph.resources.push_back(output);

    graph.resourceUses.push_back({{0}, {0}, sem::Effect::Write,
        sem::ViewRole::StorageBuffer, sem::TemporalRelation::Current});

    sem::Program program;
    program.id = {0};
    program.kind = sem::ProgramKind::Compute;
    program.interface.parameters.push_back({{0}, "StageMHeadlessOutput",
        sem::ProgramParameterKind::UnorderedBuffer, sem::ShaderStage::Compute, 0, 0, 1});
    program.source.hlslSource = R"hlsl(
RWStructuredBuffer<float4> StageMHeadlessOutput : register(u0);
[numthreads(1, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    StageMHeadlessOutput[0] = float4(0.25f, 0.50f, 0.75f, 1.0f);
}
)hlsl";
    program.source.computeEntry = "CSMain";
    graph.programs.push_back(std::move(program));

    sem::Work work;
    work.id = {0};
    work.kind = sem::WorkKind::Compute;
    work.operands.push_back({sem::WorkOperandKind::ProgramParameter, {0}, {0}});
    work.compute.program = {0};
    graph.works.push_back(std::move(work));
    return graph;
}

gen::GeneratedSpec ExplicitSpec(std::string name,
                                std::vector<gen::NodeKind> kinds,
                                std::initializer_list<gen::Edge> edges,
                                std::uint64_t seed)
{
    gen::GeneratedSpec spec;
    spec.name = std::move(name);
    spec.kinds = std::move(kinds);
    spec.edges.assign(edges.begin(), edges.end());
    spec.seed = seed;
    return spec;
}

std::vector<gen::GeneratedSpec> RepresentativeSpecs()
{
    std::vector<gen::GeneratedSpec> specs;
    specs.push_back(ExplicitSpec("StageJ.SingleCompute", {gen::NodeKind::Compute}, {}, 0x101));
    specs.push_back(ExplicitSpec("StageJ.IndependentMixed",
        {gen::NodeKind::Compute, gen::NodeKind::Copy, gen::NodeKind::Compute, gen::NodeKind::Copy}, {}, 0x102));
    specs.push_back(ExplicitSpec("StageJ.MixedChain",
        {gen::NodeKind::Compute, gen::NodeKind::Copy, gen::NodeKind::Compute, gen::NodeKind::Copy, gen::NodeKind::Compute},
        {{0,1},{1,2},{2,3},{3,4}}, 0x103));
    specs.push_back(ExplicitSpec("StageJ.FanOut",
        {gen::NodeKind::Compute, gen::NodeKind::Copy, gen::NodeKind::Compute, gen::NodeKind::Copy, gen::NodeKind::Compute},
        {{0,1},{0,2},{0,3},{0,4}}, 0x104));
    specs.push_back(ExplicitSpec("StageJ.FanInLatestProducer",
        {gen::NodeKind::Compute, gen::NodeKind::Compute, gen::NodeKind::Compute, gen::NodeKind::Copy, gen::NodeKind::Compute},
        {{0,3},{1,3},{2,3},{3,4}}, 0x105));
    specs.push_back(ExplicitSpec("StageJ.Diamond",
        {gen::NodeKind::Copy, gen::NodeKind::Compute, gen::NodeKind::Copy, gen::NodeKind::Compute},
        {{0,1},{0,2},{1,3},{2,3}}, 0x106));
    specs.push_back(ExplicitSpec("StageJ.DisconnectedComponents",
        {gen::NodeKind::Compute, gen::NodeKind::Copy, gen::NodeKind::Compute,
         gen::NodeKind::Copy, gen::NodeKind::Compute, gen::NodeKind::Copy},
        {{0,1},{1,2},{3,4},{4,5}}, 0x107));
    specs.push_back(ExplicitSpec("StageJ.Layered",
        {gen::NodeKind::Compute, gen::NodeKind::Copy, gen::NodeKind::Compute,
         gen::NodeKind::Copy, gen::NodeKind::Compute, gen::NodeKind::Copy},
        {{0,2},{0,3},{1,2},{1,3},{2,4},{2,5},{3,4},{3,5}}, 0x108));
    specs.push_back(ExplicitSpec("StageJ.DenseTransitive",
        {gen::NodeKind::Compute, gen::NodeKind::Copy, gen::NodeKind::Compute,
         gen::NodeKind::Copy, gen::NodeKind::Compute, gen::NodeKind::Copy},
        {{0,1},{0,2},{0,3},{0,4},{0,5},{1,2},{1,3},{1,4},{1,5},
         {2,3},{2,4},{2,5},{3,4},{3,5},{4,5}}, 0x109));

    constexpr std::array<std::uint64_t, 16> masks = {
        0x0000,0x0001,0x0003,0x0015,0x002A,0x0055,0x00AA,0x0123,
        0x0249,0x0492,0x0924,0x1249,0x1555,0x2AAA,0x5A5A,0x7FFF};
    for (std::size_t index = 0; index < masks.size(); ++index)
        specs.push_back(gen::MakeForwardMaskSpec(
            "StageJ.SampleMask." + std::to_string(index), 6, masks[index],
            0xC6BC279692B5CC83ull ^ masks[index], true));
    return specs;
}

std::vector<gen::GeneratedSpec> LargeSeedSpecs()
{
    return {
        gen::MakeSeededSpec("StageJ.Seed.10.2611923443488327891", 10, 0x243F6A8885A308D3ull, 28, true),
        gen::MakeSeededSpec("StageJ.Seed.25.1376283091369227076", 25, 0x13198A2E03707344ull, 18, true),
        gen::MakeSeededSpec("StageJ.Seed.50.11820040416388919760", 50, 0xA4093822299F31D0ull, 12, true),
        gen::MakeSeededSpec("StageJ.Seed.100.589684135938649225", 100, 0x082EFA98EC4E6C89ull, 8, true)
    };
}

base::Result<std::vector<SourceCase>, std::string> BuildSourceCorpus()
{
    std::vector<SourceCase> result;

    for (const auto key : lvl1::StageAInputs())
    {
        auto built = lvl1::BuildScenario(key);
        if (!built) return base::Result<std::vector<SourceCase>, std::string>::Failure(built.Error());
        auto input = std::move(built).Value();
        result.push_back({"Level1/" + input.name, std::move(input.graph), input.targetProfile});
    }

    sge4::target::D3D12TargetProfile headlessProfile;
    headlessProfile.computeQueueCount = 1;
    headlessProfile.copyQueueCount = 0;
    headlessProfile.surfaceImageCount = 0;
    headlessProfile.rtvDescriptorCount = 0;
    headlessProfile.dsvDescriptorCount = 0;
    headlessProfile.shaderDescriptorCount = 1;
    result.push_back({"StageG/HeadlessCompute", BuildStageGHeadlessGraph(), headlessProfile});

    for (const auto key : qual::StageHInputs())
    {
        auto built = qual::BuildStageHScenario(key);
        if (!built) return base::Result<std::vector<SourceCase>, std::string>::Failure(built.Error());
        auto input = std::move(built).Value();
        result.push_back({"StageH/" + input.name, std::move(input.graph), input.targetProfile});
    }

    auto generatedSpecs = RepresentativeSpecs();
    auto largeSpecs = LargeSeedSpecs();
    generatedSpecs.insert(generatedSpecs.end(),
        std::make_move_iterator(largeSpecs.begin()), std::make_move_iterator(largeSpecs.end()));
    for (auto& spec : generatedSpecs)
    {
        auto built = gen::BuildGeneratedCase(std::move(spec));
        if (!built) return base::Result<std::vector<SourceCase>, std::string>::Failure(built.Error());
        auto input = std::move(built).Value();
        result.push_back({"StageJ/" + input.spec.name, std::move(input.graph), input.targetProfile});
    }

    auto pipeline = runtime_cases::Build(runtime_cases::Scenario::DynamicExternalPipeline);
    if (!pipeline) return base::Result<std::vector<SourceCase>, std::string>::Failure(pipeline.Error());
    auto pipelineInput = std::move(pipeline).Value();
    auto fallbackGraph = pipelineInput.graph;
    auto fallbackProfile = pipelineInput.targetProfile;
    fallbackProfile.computeQueueCount = 0;
    fallbackProfile.copyQueueCount = 0;
    result.push_back({"StageK/DynamicExternalDedicated", std::move(pipelineInput.graph), pipelineInput.targetProfile});
    result.push_back({"StageK/DynamicExternalDirectFallback", std::move(fallbackGraph), fallbackProfile});

    auto temporal = runtime_cases::Build(runtime_cases::Scenario::CrossQueueTemporal);
    if (!temporal) return base::Result<std::vector<SourceCase>, std::string>::Failure(temporal.Error());
    auto temporalInput = std::move(temporal).Value();
    result.push_back({"StageK/CrossQueueTemporal", std::move(temporalInput.graph), temporalInput.targetProfile});

    std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
        return left.name < right.name;
    });
    if (result.size() != SourceCorpusCount)
        return base::Result<std::vector<SourceCase>, std::string>::Failure(
            "final corpus cardinality changed: " + std::to_string(result.size()));
    const auto duplicate = std::adjacent_find(result.begin(), result.end(), [](const auto& left, const auto& right) {
        return left.name == right.name;
    });
    if (duplicate != result.end())
        return base::Result<std::vector<SourceCase>, std::string>::Failure(
            "final corpus contains duplicate case name: " + duplicate->name);
    return base::Result<std::vector<SourceCase>, std::string>::Success(std::move(result));
}


struct DigestText final { std::string value; };

base::Result<DigestText, std::string> SemanticCorpusDigest(
    std::span<const SourceCase> corpus,
    std::string_view identity,
    std::string_view baseline)
{
    base::BinaryWriter writer;
    WriteString(writer, identity);
    WriteString(writer, baseline);
    writer.WriteU32(static_cast<std::uint32_t>(corpus.size()));
    for (const auto& item : corpus)
    {
        auto digest = SemanticDigest(item, identity);
        if (!digest) return base::Result<DigestText, std::string>::Failure(digest.Error());
        WriteString(writer, item.name);
        writer.WriteBytes(digest.Value());
    }
    return base::Result<DigestText, std::string>::Success(
        {base::ToHex(base::Sha256(writer.Bytes()))});
}

sem::Resource* FindResourceByName(sem::SemanticGraph& graph, std::string_view name)
{
    const auto found = std::find_if(graph.resources.begin(), graph.resources.end(),
        [name](const sem::Resource& resource) { return resource.debugName == name; });
    return found == graph.resources.end() ? nullptr : &*found;
}

const sem::Resource* FindResource(const sem::SemanticGraph& graph, sem::ResourceId id)
{
    const auto found = std::find_if(graph.resources.begin(), graph.resources.end(),
        [id](const sem::Resource& resource) { return resource.id == id; });
    return found == graph.resources.end() ? nullptr : &*found;
}

sem::WorkId FindWorkByName(const sem::SemanticGraph& graph, std::string_view name)
{
    const auto found = std::find_if(graph.works.begin(), graph.works.end(),
        [name](const sem::Work& work) { return work.debugName == name; });
    return found == graph.works.end() ? sem::WorkId{} : found->id;
}

void InternalizeExternalBuffer(sem::Resource& resource)
{
    resource.lifetime = sem::LifetimeIntent::Persistent;
    resource.update = sem::UpdateIntent::GpuWritten;
    resource.visibility = sem::Visibility::Internal;
    resource.initialContent.clear();
    resource.dynamicData = {};
}

base::Result<sem::ResourceId, std::string> AppendComputeExportProxy(
    sem::SemanticGraph& graph,
    sem::ResourceId source,
    std::string debugStem,
    sem::WorkId predecessor)
{
    const auto* sourceResource = FindResource(graph, source);
    if (sourceResource == nullptr || sourceResource->kind != sem::ResourceKind::Buffer ||
        sourceResource->buffer.sizeBytes == 0 || sourceResource->buffer.strideBytes == 0)
        return base::Result<sem::ResourceId, std::string>::Failure(
            "compute export proxy requires one valid internal Buffer source");

    sem::Resource external;
    external.id = {static_cast<std::uint32_t>(graph.resources.size())};
    external.debugName = debugStem + ".ExternalOutput";
    external.kind = sem::ResourceKind::Buffer;
    external.lifetime = sem::LifetimeIntent::External;
    external.update = sem::UpdateIntent::External;
    external.visibility = sem::Visibility::Published;
    external.buffer = sourceResource->buffer;
    const auto externalId = external.id;
    graph.resources.push_back(std::move(external));

    sem::ResourceUse read;
    read.id = {static_cast<std::uint32_t>(graph.resourceUses.size())};
    read.resource = source;
    read.effect = sem::Effect::Read;
    read.role = sem::ViewRole::ShaderBuffer;
    const auto readId = read.id;
    graph.resourceUses.push_back(read);

    sem::ResourceUse write;
    write.id = {static_cast<std::uint32_t>(graph.resourceUses.size())};
    write.resource = externalId;
    write.effect = sem::Effect::Write;
    write.role = sem::ViewRole::StorageBuffer;
    const auto writeId = write.id;
    graph.resourceUses.push_back(write);

    sem::Program program;
    program.id = {static_cast<std::uint32_t>(graph.programs.size())};
    program.debugName = debugStem + ".ExportProgram";
    program.kind = sem::ProgramKind::Compute;
    program.interface.parameters = {
        {{0}, "Source", sem::ProgramParameterKind::ReadOnlyBuffer,
            sem::ShaderStage::Compute, 0, 0, 1},
        {{1}, "Destination", sem::ProgramParameterKind::UnorderedBuffer,
            sem::ShaderStage::Compute, 0, 0, 1}};
    program.source.hlslSource = R"hlsl(
StructuredBuffer<float4> Source : register(t0);
RWStructuredBuffer<float4> Destination : register(u0);
[numthreads(1, 1, 1)]
void CSExport(uint3 id : SV_DispatchThreadID)
{
    Destination[0] = Source[0];
}
)hlsl";
    program.source.computeEntry = "CSExport";
    const auto programId = program.id;
    graph.programs.push_back(std::move(program));

    sem::Work work;
    work.id = {static_cast<std::uint32_t>(graph.works.size())};
    work.debugName = debugStem + ".ExportWork";
    work.kind = sem::WorkKind::Compute;
    work.operands = {
        {sem::WorkOperandKind::ProgramParameter, readId, {0}},
        {sem::WorkOperandKind::ProgramParameter, writeId, {1}}};
    if (predecessor.IsValid()) work.dependencies.push_back(predecessor);
    work.compute.program = programId;
    work.compute.threadGroupCountX = 1;
    work.compute.threadGroupCountY = 1;
    work.compute.threadGroupCountZ = 1;
    graph.works.push_back(std::move(work));
    return base::Result<sem::ResourceId, std::string>::Success(externalId);
}

base::Result<void, std::string> NormalizeDynamicExternal(LeafCase& item)
{
    auto* outputA = FindResourceByName(item.leafGraph, "StageK.Pipeline.ExternalOutputA");
    auto* outputB = FindResourceByName(item.leafGraph, "StageK.Pipeline.ExternalOutputB");
    if (outputA == nullptr || outputB == nullptr)
        return base::Result<void, std::string>::Failure(
            item.name + ": Stage-K pipeline output Resources are missing");
    const auto outputBId = outputB->id;
    InternalizeExternalBuffer(*outputA);
    InternalizeExternalBuffer(*outputB);
    const auto predecessor = FindWorkByName(item.leafGraph, "StageK.Pipeline.CopyOutput");
    if (!predecessor.IsValid())
        return base::Result<void, std::string>::Failure(
            item.name + ": Stage-K final Copy Work is missing");
    auto appended = AppendComputeExportProxy(
        item.leafGraph, outputBId, "StageK.Pipeline.Composition", predecessor);
    if (!appended) return base::Result<void, std::string>::Failure(item.name + ": " + appended.Error());
    item.leafProfile.shaderDescriptorCount += 2;
    item.normalization = BoundaryNormalization::ComputeExportProxy;
    return base::Result<void, std::string>::Success();
}

base::Result<void, std::string> NormalizeTemporalExternal(LeafCase& item)
{
    auto* output = FindResourceByName(item.leafGraph, "StageK.Temporal.ExternalOutput");
    if (output == nullptr)
        return base::Result<void, std::string>::Failure(
            item.name + ": Stage-K temporal output Resource is missing");
    const auto outputId = output->id;
    InternalizeExternalBuffer(*output);
    const auto predecessor = FindWorkByName(item.leafGraph, "StageK.Temporal.CopyPrevious");
    if (!predecessor.IsValid())
        return base::Result<void, std::string>::Failure(
            item.name + ": Stage-K temporal Copy Work is missing");
    auto appended = AppendComputeExportProxy(
        item.leafGraph, outputId, "StageK.Temporal.Composition", predecessor);
    if (!appended) return base::Result<void, std::string>::Failure(item.name + ": " + appended.Error());
    item.leafProfile.shaderDescriptorCount += 2;
    item.normalization = BoundaryNormalization::ComputeExportProxy;
    return base::Result<void, std::string>::Success();
}

enum class DerivedAccess : std::uint16_t { ReadOnly = 1, WriteOnly = 2 };

base::Result<DerivedAccess, std::string> DeriveEndpointAccess(
    const sem::SemanticGraph& graph, sem::ResourceId resource)
{
    bool read = false;
    bool write = false;
    std::uint32_t useCount = 0;
    for (const auto& use : graph.resourceUses)
    {
        if (use.resource != resource) continue;
        ++useCount;
        if (use.effect == sem::Effect::Read && use.role == sem::ViewRole::ShaderBuffer)
            read = true;
        else if (use.effect == sem::Effect::Write && use.role == sem::ViewRole::StorageBuffer)
            write = true;
        else
            return base::Result<DerivedAccess, std::string>::Failure(
                "External Buffer is not a strict SRV input or UAV output");
    }
    if (useCount == 0 || read == write)
        return base::Result<DerivedAccess, std::string>::Failure(
            "External Buffer endpoint access is absent or mixed");
    return base::Result<DerivedAccess, std::string>::Success(
        read ? DerivedAccess::ReadOnly : DerivedAccess::WriteOnly);
}

std::string StableKey(std::size_t ordinal, DerivedAccess access, sem::ResourceId resource)
{
    std::ostringstream key;
    key << "leaf/" << std::setw(2) << std::setfill('0') << ordinal << '/'
        << (access == DerivedAccess::ReadOnly ? "input" : "output") << '/'
        << resource.value;
    return key.str();
}

base::Result<void, std::string> AuthorEndpoints(LeafCase& item, std::size_t ordinal)
{
    auto analyzed = analysis::Analyze(item.leafGraph);
    if (!analyzed)
    {
        std::string message = item.name + ": normalized SemanticAnalysis failed";
        if (!analyzed.Error().empty()) message += ": " + analyzed.Error().front().message;
        return base::Result<void, std::string>::Failure(std::move(message));
    }

    for (const auto resourceId : analyzed.Value().canonicalResourceOrder)
    {
        const auto* resource = FindResource(item.leafGraph, resourceId);
        if (resource == nullptr) return base::Result<void, std::string>::Failure(
            item.name + ": canonical Resource disappeared");
        if (resource->lifetime != sem::LifetimeIntent::External ||
            resource->kind == sem::ResourceKind::SurfaceImage)
            continue;
        if (resource->kind != sem::ResourceKind::Buffer)
            return base::Result<void, std::string>::Failure(
                item.name + ": F2 supports only External Buffer endpoints");
        auto access = DeriveEndpointAccess(item.leafGraph, resourceId);
        if (!access) return base::Result<void, std::string>::Failure(
            item.name + ": " + access.Error());
        item.endpoints.push_back({resourceId, StableKey(ordinal, access.Value(), resourceId)});
    }
    return base::Result<void, std::string>::Success();
}

base::Result<DigestText, std::string> EndpointDigest(std::span<const LeafCase> corpus)
{
    base::BinaryWriter writer;
    WriteString(writer, EndpointAuthoringIdentity);
    WriteString(writer, LeafBaselineCommit);
    writer.WriteU32(static_cast<std::uint32_t>(corpus.size()));
    for (const auto& item : corpus)
    {
        WriteString(writer, item.name);
        writer.WriteU16(static_cast<std::uint16_t>(item.normalization));
        writer.WriteU32(static_cast<std::uint32_t>(item.endpoints.size()));
        for (const auto& endpoint : item.endpoints)
        {
            WriteId(writer, endpoint.resource.value);
            WriteString(writer, endpoint.stableKey);
            auto access = DeriveEndpointAccess(item.leafGraph, endpoint.resource);
            if (!access) return base::Result<DigestText, std::string>::Failure(
                item.name + ": " + access.Error());
            writer.WriteU16(static_cast<std::uint16_t>(access.Value()));
        }
    }
    return base::Result<DigestText, std::string>::Success(
        {base::ToHex(base::Sha256(writer.Bytes()))});
}
}

const char* BoundaryNormalizationName(BoundaryNormalization value) noexcept
{
    switch (value)
    {
    case BoundaryNormalization::None: return "none";
    case BoundaryNormalization::ComputeExportProxy: return "compute-export-proxy";
    default: return "unknown";
    }
}

base::Result<std::vector<LeafCase>, std::string> BuildLeafCorpus()
{
    auto source = BuildSourceCorpus();
    if (!source) return base::Result<std::vector<LeafCase>, std::string>::Failure(source.Error());

    std::vector<LeafCase> result;
    result.reserve(source.Value().size());
    for (std::size_t ordinal = 0; ordinal < source.Value().size(); ++ordinal)
    {
        auto& original = source.Value()[ordinal];
        LeafCase item;
        item.name = original.name;
        item.sourceGraph = original.graph;
        item.sourceProfile = original.profile;
        item.leafGraph = std::move(original.graph);
        item.leafProfile = original.profile;

        base::Result<void, std::string> normalized = base::Result<void, std::string>::Success();
        if (item.name == "StageK/DynamicExternalDedicated" ||
            item.name == "StageK/DynamicExternalDirectFallback")
            normalized = NormalizeDynamicExternal(item);
        else if (item.name == "StageK/CrossQueueTemporal")
            normalized = NormalizeTemporalExternal(item);
        if (!normalized) return base::Result<std::vector<LeafCase>, std::string>::Failure(normalized.Error());

        auto authored = AuthorEndpoints(item, ordinal);
        if (!authored) return base::Result<std::vector<LeafCase>, std::string>::Failure(authored.Error());
        result.push_back(std::move(item));
    }

    std::size_t normalizedCount = 0;
    std::size_t endpointCount = 0;
    std::size_t readCount = 0;
    std::size_t writeCount = 0;
    std::set<std::string> keys;
    for (const auto& item : result)
    {
        if (item.normalization != BoundaryNormalization::None) ++normalizedCount;
        endpointCount += item.endpoints.size();
        for (const auto& endpoint : item.endpoints)
        {
            if (!keys.insert(endpoint.stableKey).second)
                return base::Result<std::vector<LeafCase>, std::string>::Failure(
                    "F2 endpoint author keys are not globally unique");
            auto access = DeriveEndpointAccess(item.leafGraph, endpoint.resource);
            if (!access) return base::Result<std::vector<LeafCase>, std::string>::Failure(access.Error());
            if (access.Value() == DerivedAccess::ReadOnly) ++readCount; else ++writeCount;
        }
    }
    if (result.size() != FrozenCaseCount || normalizedCount != FrozenNormalizedCaseCount ||
        endpointCount != FrozenEndpointCount || readCount != FrozenReadOnlyEndpointCount ||
        writeCount != FrozenWriteOnlyEndpointCount)
        return base::Result<std::vector<LeafCase>, std::string>::Failure(
            "F2 frozen corpus cardinalities changed: cases=" + std::to_string(result.size()) +
            ", normalized=" + std::to_string(normalizedCount) +
            ", endpoints=" + std::to_string(endpointCount) +
            ", read=" + std::to_string(readCount) +
            ", write=" + std::to_string(writeCount));

    return base::Result<std::vector<LeafCase>, std::string>::Success(std::move(result));
}

base::Result<CorpusIdentity, std::string> ComputeCorpusIdentity(
    std::span<const LeafCase> corpus)
{
    std::vector<SourceCase> source;
    std::vector<SourceCase> leaf;
    source.reserve(corpus.size());
    leaf.reserve(corpus.size());
    CorpusIdentity identity;
    for (const auto& item : corpus)
    {
        source.push_back({item.name, item.sourceGraph, item.sourceProfile});
        leaf.push_back({item.name, item.leafGraph, item.leafProfile});
        if (item.normalization != BoundaryNormalization::None) ++identity.normalizedCaseCount;
        identity.endpointCount += item.endpoints.size();
        for (const auto& endpoint : item.endpoints)
        {
            auto access = DeriveEndpointAccess(item.leafGraph, endpoint.resource);
            if (!access) return base::Result<CorpusIdentity, std::string>::Failure(access.Error());
            if (access.Value() == DerivedAccess::ReadOnly) ++identity.readOnlyEndpointCount;
            else ++identity.writeOnlyEndpointCount;
        }
    }
    auto sourceDigest = SemanticCorpusDigest(source, SourceSemanticIdentity, SourceBaselineCommit);
    if (!sourceDigest) return base::Result<CorpusIdentity, std::string>::Failure(sourceDigest.Error());
    auto leafDigest = SemanticCorpusDigest(leaf, LeafSemanticIdentity, LeafBaselineCommit);
    if (!leafDigest) return base::Result<CorpusIdentity, std::string>::Failure(leafDigest.Error());
    auto endpointDigest = EndpointDigest(corpus);
    if (!endpointDigest) return base::Result<CorpusIdentity, std::string>::Failure(endpointDigest.Error());
    identity.sourceSemanticCorpusDigest = sourceDigest.Value().value;
    identity.leafSemanticCorpusDigest = leafDigest.Value().value;
    identity.endpointAuthoringDigest = endpointDigest.Value().value;
    return base::Result<CorpusIdentity, std::string>::Success(std::move(identity));
}
}
