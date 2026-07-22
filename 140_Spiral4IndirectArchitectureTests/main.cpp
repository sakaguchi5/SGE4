#include "../00_Foundation/BinaryIO.h"
#include "../00_Foundation/FileIO.h"
#include "../00_Foundation/Sha256.h"
#include "../120_ActiveWorkSemantic/ActiveWorkSemantic.h"
#include "../121_Spiral4Contracts/Spiral4Contracts.h"
#include "../122_Spiral4IndirectExecution/Spiral4IndirectExecution.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
namespace s4sem = sge4_5::spiral4::semantic;
namespace s4contract = sge4_5::spiral4::contract;
namespace s4exec = sge4_5::spiral4::execution;
namespace base = sge4_5::base;

void Require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

void WriteBundle(
    const std::filesystem::path& path,
    const s4sem::ActiveWorkSemanticV1& semantic,
    const s4contract::FrozenSingleIndirectArtifactV1& artifact)
{
    const auto semanticBytes = s4sem::SerializeActiveWorkSemanticV1(semantic);

    base::BinaryWriter writer;
    writer.WriteU32(0x32433453u); // "S4C2"
    writer.WriteU32(1);
    writer.WriteU32(static_cast<std::uint32_t>(semanticBytes.size()));
    writer.WriteBytes(semanticBytes);
    writer.WriteU32(static_cast<std::uint32_t>(artifact.Bytes().size()));
    writer.WriteBytes(artifact.Bytes());
    const auto bundleDigest = base::Sha256(writer.Bytes());
    writer.WriteBytes(bundleDigest);

    std::filesystem::create_directories(path.parent_path());
    const auto bytes = std::move(writer).Take();
    auto result = base::WriteAllBytes(path, bytes);
    if (!result)
    {
        throw std::runtime_error(result.Error());
    }
}

void CorruptionTests(
    const s4contract::FrozenSingleIndirectArtifactV1& artifact)
{
    {
        auto bytes = artifact.Bytes();
        bytes[0] ^= std::byte{0x01};
        Require(
            !s4contract::ParseSingleIndirectArtifactV1(bytes),
            "magic corruption was accepted");
    }
    {
        auto bytes = artifact.Bytes();
        bytes[16] ^= std::byte{0x01};
        Require(
            !s4contract::ParseSingleIndirectArtifactV1(bytes),
            "operation corruption was accepted");
    }
    {
        auto bytes = artifact.Bytes();
        bytes.pop_back();
        Require(
            !s4contract::ParseSingleIndirectArtifactV1(bytes),
            "truncated artifact was accepted");
    }
    {
        auto bytes = artifact.Bytes();
        bytes.push_back(std::byte{0});
        Require(
            !s4contract::ParseSingleIndirectArtifactV1(bytes),
            "artifact with trailing bytes was accepted");
    }
}

void TypeBoundaryTests()
{
    bool rejected = false;
    try
    {
        static_cast<void>(
            s4sem::ActiveWorkCountV1(
                4097,
                s4sem::MaxWorkCountV1(4096)));
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    Require(rejected, "Nf > Nmax was not rejected");

    rejected = false;
    try
    {
        static_cast<void>(s4sem::ThreadGroupSizeV1(0));
    }
    catch (const std::invalid_argument&)
    {
        rejected = true;
    }
    Require(rejected, "zero thread-group size was not rejected");
}
}

int main(int argc, char** argv)
{
    try
    {
        const auto semantic = s4sem::BuildCanonicalActiveWorkSemanticV1();
        const auto semanticBytesA = s4sem::SerializeActiveWorkSemanticV1(semantic);
        const auto semanticBytesB = s4sem::SerializeActiveWorkSemanticV1(semantic);
        Require(semanticBytesA == semanticBytesB, "semantic bytes are not deterministic");

        const auto artifact =
            s4contract::BuildCu2SingleIndirectArtifactV1(semantic);
        const auto parsed =
            s4contract::ParseSingleIndirectArtifactV1(artifact.Bytes());
        Require(static_cast<bool>(parsed), "canonical indirect artifact did not parse");
        Require(
            parsed.Value().ArtifactIdentity() == artifact.ArtifactIdentity(),
            "parsed artifact identity mismatch");

        const auto executionValidation =
            s4contract::ValidateSingleIndirectArtifactForExecutionV1(
                artifact,
                semantic);
        Require(
            static_cast<bool>(executionValidation),
            "canonical indirect artifact failed execution validation");

        CorruptionTests(artifact);
        TypeBoundaryTests();

        constexpr std::array<std::uint32_t, 7> architectureCases{
            0u, 1u, 63u, 64u, 65u, 1024u, 4096u
        };
        const auto run = s4exec::RunSingleIndirectArchitectureOnWarpV1(
            semantic,
            artifact,
            architectureCases);
        if (!run)
        {
            std::cerr
                << "CU2 WARP failure [" << run.Error().stage << "]: "
                << run.Error().message;
            if (run.Error().hresult != 0)
            {
                std::cerr << " HRESULT=" << run.Error().hresult;
            }
            std::cerr << '\n';
            return 2;
        }

        Require(
            run.Value().cases.size() == architectureCases.size(),
            "CU2 WARP case count mismatch");
        for (const auto& result : run.Value().cases)
        {
            Require(
                result.observedDispatchGroupCountX ==
                    result.expectedDispatchGroupCountX,
                "indirect group count mismatch");
            Require(
                result.activeMismatchCount == 0,
                "active output mismatch");
            Require(
                result.inactiveTailPreserved,
                "inactive tail was modified");
            std::cout
                << "[PASS] Nf=" << result.activeWorkCount
                << " groups=" << result.observedDispatchGroupCountX
                << " digest=" << base::ToHex(result.readbackDigest)
                << '\n';
        }

        if (argc == 3 && std::string(argv[1]) == "--emit")
        {
            WriteBundle(argv[2], semantic, artifact);
        }
        else if (argc != 1)
        {
            std::cerr << "Usage: 140_Spiral4IndirectArchitectureTests [--emit path]\n";
            return 3;
        }

        std::cout
            << "Spiral 4 CU2 Single ExecuteIndirect architecture passed.\n"
            << "Adapter: " << run.Value().adapterDescription << '\n'
            << "Artifact SHA-256: "
            << base::ToHex(artifact.ArtifactIdentity()) << '\n'
            << "Runtime dispatch-dimension decision: None\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Spiral 4 CU2 test failure: " << error.what() << '\n';
        return 1;
    }
}
