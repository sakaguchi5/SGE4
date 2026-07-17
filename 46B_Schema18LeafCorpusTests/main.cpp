#include "../00_Foundation/FileIO.h"
#include "../00_Foundation/Sha256.h"
#include "../09_FrozenPackageCore/PackageReader.h"
#include "../10_D3D12PackageSchema/D3D12Encoding.h"
#include "../10A_D3D12CompositionSchema18/D3D12CompositionEncoding18.h"
#include "../12_SGE4Compiler/SGE4Compiler.h"
#include "../12A_CompositionLeafCompiler/CompositionLeafCompiler.h"
#include "../27A_Schema18LeafCorpus/Schema18LeafCorpus.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <set>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
namespace base = sge4::base;
namespace sem = sge4::semantic;
namespace pkg = sge4::package;
namespace pkg17 = sge4::package::d3d12_v13;
namespace pkg18 = sge4::package::d3d12_v18;
namespace compiler = sge4::compiler;
namespace leaf_compiler = sge4::compiler::composition;
namespace corpus = sge4::qualification::schema18_corpus;

constexpr std::string_view ManifestIdentity =
    "SGE4-L4V1-F2-Schema18-Leaf-Corpus-Manifest-v1";
constexpr std::string_view BaseCommit =
    "94c8cb8915437e25e3e0d6a3ae3c5277940114a0";

struct ManifestBuild final
{
    std::string text;
    std::string aggregateDigest;
};

enum class ExpectedAccess : std::uint16_t { ReadOnly = 1, WriteOnly = 2 };

base::Result<ExpectedAccess, std::string> DeriveExpectedAccess(
    const sem::SemanticGraph& graph, sem::ResourceId resource)
{
    bool read = false;
    bool write = false;
    std::uint32_t count = 0;
    for (const auto& use : graph.resourceUses)
    {
        if (use.resource != resource) continue;
        ++count;
        if (use.effect == sem::Effect::Read && use.role == sem::ViewRole::ShaderBuffer)
            read = true;
        else if (use.effect == sem::Effect::Write && use.role == sem::ViewRole::StorageBuffer)
            write = true;
        else
            return base::Result<ExpectedAccess, std::string>::Failure(
                "endpoint has a non-composition ResourceUse");
    }
    if (count == 0 || read == write)
        return base::Result<ExpectedAccess, std::string>::Failure(
            "endpoint access is absent or mixed");
    return base::Result<ExpectedAccess, std::string>::Success(
        read ? ExpectedAccess::ReadOnly : ExpectedAccess::WriteOnly);
}

std::string SafeFilename(std::size_t index, std::string_view name)
{
    std::ostringstream output;
    output.width(3);
    output.fill('0');
    output << index << '_';
    bool previousUnderscore = false;
    for (const unsigned char value : name)
    {
        const bool alphaNumeric =
            (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') ||
            (value >= '0' && value <= '9');
        if (alphaNumeric || value == '.' || value == '-')
        {
            output << static_cast<char>(value);
            previousUnderscore = false;
        }
        else if (!previousUnderscore)
        {
            output << '_';
            previousUnderscore = true;
        }
    }
    return output.str() + ".sgep";
}

base::Result<void, std::string> WriteBytes(
    const std::filesystem::path& path, std::span<const std::byte> bytes)
{
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) return base::Result<void, std::string>::Failure(
        "failed to create output directory: " + error.message());
    return base::WriteAllBytes(path, bytes);
}

base::Result<ManifestBuild, std::string> BuildManifest(
    const std::filesystem::path* corpusDirectory)
{
    auto builtCorpus = corpus::BuildLeafCorpus();
    if (!builtCorpus)
        return base::Result<ManifestBuild, std::string>::Failure(builtCorpus.Error());
    auto cases = std::move(builtCorpus).Value();
    auto identity = corpus::ComputeCorpusIdentity(cases);
    if (!identity)
        return base::Result<ManifestBuild, std::string>::Failure(identity.Error());

    const auto& ids = identity.Value();
    if (ids.sourceSemanticCorpusDigest != corpus::ExpectedSourceSemanticCorpusDigest ||
        ids.leafSemanticCorpusDigest != corpus::ExpectedLeafSemanticCorpusDigest ||
        ids.endpointAuthoringDigest != corpus::ExpectedEndpointAuthoringDigest)
        return base::Result<ManifestBuild, std::string>::Failure(
            "F2 frozen semantic or endpoint identity changed");
    if (cases.size() != corpus::FrozenCaseCount ||
        ids.normalizedCaseCount != corpus::FrozenNormalizedCaseCount ||
        ids.endpointCount != corpus::FrozenEndpointCount ||
        ids.readOnlyEndpointCount != corpus::FrozenReadOnlyEndpointCount ||
        ids.writeOnlyEndpointCount != corpus::FrozenWriteOnlyEndpointCount)
        return base::Result<ManifestBuild, std::string>::Failure(
            "F2 frozen corpus cardinalities changed");

    if (corpusDirectory != nullptr)
    {
        std::error_code ignored;
        std::filesystem::remove_all(*corpusDirectory, ignored);
        std::filesystem::create_directories(*corpusDirectory, ignored);
        if (ignored) return base::Result<ManifestBuild, std::string>::Failure(
            "failed to prepare corpus output directory: " + ignored.message());
    }

    std::ostringstream manifest;
    manifest << "identity=" << ManifestIdentity << '\n';
    manifest << "base-commit=" << BaseCommit << '\n';
    manifest << "source-corpus-count=" << cases.size() << '\n';
    manifest << "source-semantic-corpus-digest=" << ids.sourceSemanticCorpusDigest << '\n';
    manifest << "leaf-semantic-corpus-digest=" << ids.leafSemanticCorpusDigest << '\n';
    manifest << "endpoint-authoring-digest=" << ids.endpointAuthoringDigest << '\n';
    manifest << "target-schema=18\nminimum-runtime=18\n";
    manifest << "normalized-cases=" << ids.normalizedCaseCount << '\n';
    manifest << "endpoint-count=" << ids.endpointCount << '\n';
    manifest << "readonly-endpoints=" << ids.readOnlyEndpointCount << '\n';
    manifest << "writeonly-endpoints=" << ids.writeOnlyEndpointCount << '\n';

    std::set<std::string> artifactNames;
    for (std::size_t index = 0; index < cases.size(); ++index)
    {
        const auto& item = cases[index];
        auto source17 = compiler::CompileCanonical(item.sourceGraph, item.sourceProfile);
        if (!source17)
            return base::Result<ManifestBuild, std::string>::Failure(
                item.name + " source Schema-17 compile failed at " +
                source17.Error().stage + ": " + source17.Error().message);

        std::vector<leaf_compiler::EndpointDeclaration> declarations;
        declarations.reserve(item.endpoints.size());
        for (const auto& endpoint : item.endpoints)
            declarations.push_back({endpoint.resource, endpoint.stableKey});

        auto leaf18 = leaf_compiler::CompileCompositionLeafCanonical(
            item.leafGraph, item.leafProfile, std::move(declarations));
        if (!leaf18)
            return base::Result<ManifestBuild, std::string>::Failure(
                item.name + " Schema-18 compile failed at " +
                leaf18.Error().stage + ": " + leaf18.Error().message);

        auto frozen18 = pkg::PackageReader::Read(leaf18.Value().packageBytes);
        if (!frozen18)
            return base::Result<ManifestBuild, std::string>::Failure(
                item.name + " PackageReader failed: " + frozen18.Error().message);
        if (frozen18.Value().Header().targetSchemaVersion != 18 ||
            frozen18.Value().Header().minimumRuntimeVersion != 18)
            return base::Result<ManifestBuild, std::string>::Failure(
                item.name + " escaped Schema/Runtime 18");
        if (pkg17::D3D12PackageView::Decode(frozen18.Value()))
            return base::Result<ManifestBuild, std::string>::Failure(
                item.name + " was incorrectly accepted by the Schema-17 decoder");

        auto view18 = pkg18::CompositionLeafView::Decode(frozen18.Value());
        if (!view18)
            return base::Result<ManifestBuild, std::string>::Failure(
                item.name + " Schema-18 decode failed: " + view18.Error().message);
        auto baseFrozen = pkg::PackageReader::Read(view18.Value().Schema17BasePackageBytes());
        if (!baseFrozen)
            return base::Result<ManifestBuild, std::string>::Failure(
                item.name + " embedded Schema-17 Package failed: " + baseFrozen.Error().message);
        auto baseView = pkg17::D3D12PackageView::Decode(baseFrozen.Value());
        if (!baseView)
            return base::Result<ManifestBuild, std::string>::Failure(
                item.name + " embedded Schema-17 decode failed: " + baseView.Error().message);
        if (baseView.Value().ExternalSlots().size() != item.endpoints.size() ||
            view18.Value().Endpoints().size() != item.endpoints.size())
            return base::Result<ManifestBuild, std::string>::Failure(
                item.name + " endpoint/external-slot cardinality differs");

        std::size_t readCount = 0;
        std::size_t writeCount = 0;
        std::ostringstream keyList;
        for (std::size_t endpointIndex = 0; endpointIndex < item.endpoints.size(); ++endpointIndex)
        {
            const auto& authored = item.endpoints[endpointIndex];
            const auto& frozen = view18.Value().Endpoints()[endpointIndex];
            auto expected = DeriveExpectedAccess(item.leafGraph, authored.resource);
            if (!expected)
                return base::Result<ManifestBuild, std::string>::Failure(
                    item.name + ": " + expected.Error());
            const auto expectedAccess = expected.Value() == ExpectedAccess::ReadOnly ?
                pkg18::EndpointAccess::ReadOnly : pkg18::EndpointAccess::WriteOnly;
            if (frozen.id != endpointIndex || frozen.externalSlot.value != endpointIndex ||
                frozen.stableKey != pkg18::ComputeStableEndpointKey(authored.stableKey) ||
                frozen.access != expectedAccess || frozen.kind != pkg17::ResourceKind::Buffer ||
                frozen.minimumBytes == 0)
                return base::Result<ManifestBuild, std::string>::Failure(
                    item.name + " frozen endpoint disagrees with derived authoring");
            if (expectedAccess == pkg18::EndpointAccess::ReadOnly) ++readCount; else ++writeCount;
            if (endpointIndex != 0) keyList << ',';
            keyList << authored.stableKey << ':' <<
                (expectedAccess == pkg18::EndpointAccess::ReadOnly ? "R" : "W");
        }

        const auto baseBytes = view18.Value().Schema17BasePackageBytes();
        if (item.normalization == corpus::BoundaryNormalization::None &&
            (baseBytes.size() != source17.Value().packageBytes.size() ||
             !std::equal(baseBytes.begin(), baseBytes.end(),
                         source17.Value().packageBytes.begin())))
            return base::Result<ManifestBuild, std::string>::Failure(
                item.name + " changed its Schema-17 base without declared normalization");

        const auto artifactName = SafeFilename(index, item.name);
        if (!artifactNames.insert(artifactName).second)
            return base::Result<ManifestBuild, std::string>::Failure(
                "F2 corpus artifact filenames collided");
        if (corpusDirectory != nullptr)
        {
            auto written = WriteBytes(*corpusDirectory / artifactName, leaf18.Value().packageBytes);
            if (!written) return base::Result<ManifestBuild, std::string>::Failure(written.Error());
        }

        manifest << "case=" << index
                 << "|name=" << item.name
                 << "|artifact=" << artifactName
                 << "|normalization=" << corpus::BoundaryNormalizationName(item.normalization)
                 << "|source-execution=" << source17.Value().executionDigestHex
                 << "|base-execution=" << base::ToHex(baseFrozen.Value().ExecutionDigest())
                 << "|schema18-execution=" << base::ToHex(frozen18.Value().ExecutionDigest())
                 << "|file=" << base::ToHex(frozen18.Value().Header().fileDigest)
                 << "|bytes=" << frozen18.Value().FileBytes().size()
                 << "|endpoints=" << item.endpoints.size()
                 << "|read=" << readCount
                 << "|write=" << writeCount
                 << "|keys=" << keyList.str()
                 << '\n';
    }

    const auto body = manifest.str();
    const auto bodyBytes = std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(body.data()), body.size());
    const auto aggregate = base::ToHex(base::Sha256(bodyBytes));
    manifest << "qualification-aggregate=" << aggregate << '\n';
    return base::Result<ManifestBuild, std::string>::Success(
        {manifest.str(), aggregate});
}

base::Result<void, std::string> WriteText(
    const std::filesystem::path& path, std::string_view text)
{
    return WriteBytes(path, std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(text.data()), text.size()));
}
}

int main(int argc, char** argv)
{
    const std::string_view command = argc >= 2 ? std::string_view(argv[1]) : std::string_view{};
    const std::filesystem::path* corpusDirectory = nullptr;
    std::filesystem::path corpusPath;
    std::filesystem::path manifestPath;

    if (command == "--print-identities")
    {
        auto built = corpus::BuildLeafCorpus();
        if (!built) { std::cerr << built.Error() << '\n'; return 1; }
        auto identity = corpus::ComputeCorpusIdentity(built.Value());
        if (!identity) { std::cerr << identity.Error() << '\n'; return 2; }
        std::cout << "source=" << identity.Value().sourceSemanticCorpusDigest << '\n';
        std::cout << "leaf=" << identity.Value().leafSemanticCorpusDigest << '\n';
        std::cout << "endpoints=" << identity.Value().endpointAuthoringDigest << '\n';
        return 0;
    }
    if (command == "--emit-manifest")
    {
        if (argc != 3) { std::cerr << "--emit-manifest requires one path\n"; return 3; }
        manifestPath = argv[2];
    }
    else if (command == "--emit-corpus")
    {
        if (argc != 4) { std::cerr << "--emit-corpus requires directory and manifest paths\n"; return 4; }
        corpusPath = argv[2];
        manifestPath = argv[3];
        corpusDirectory = &corpusPath;
    }
    else if (!command.empty())
    {
        std::cerr << "unknown command\n";
        return 5;
    }

    auto manifest = BuildManifest(corpusDirectory);
    if (!manifest)
    {
        std::cerr << "L4V1-F2 failed: " << manifest.Error() << '\n';
        return 6;
    }
    if (!manifestPath.empty())
    {
        auto written = WriteText(manifestPath, manifest.Value().text);
        if (!written) { std::cerr << written.Error() << '\n'; return 7; }
    }

    std::cout << "L4V1-F2 Schema 18 Leaf Corpus tests passed\n";
    std::cout << "Source scenarios: 54; normalized boundaries: 3; endpoints: 12 (R9/W3)\n";
    std::cout << "Source semantic digest: " << corpus::ExpectedSourceSemanticCorpusDigest << '\n';
    std::cout << "Leaf semantic digest: " << corpus::ExpectedLeafSemanticCorpusDigest << '\n';
    std::cout << "Endpoint authoring digest: " << corpus::ExpectedEndpointAuthoringDigest << '\n';
    std::cout << "Qualification aggregate: " << manifest.Value().aggregateDigest << '\n';
    return 0;
}
