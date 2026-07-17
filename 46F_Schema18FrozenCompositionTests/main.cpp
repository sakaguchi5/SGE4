#include "../46D_Schema18ResourceFlowPlanningTests/TestFixture.h"

#include "../00_Foundation/FileIO.h"
#include "../00_Foundation/Sha256.h"
#include "../17A_Schema18ResourceFlowPlan/Schema18ResourceFlowPlan.h"
#include "../18A_Schema18ResourceFlowVerifier/Schema18ResourceFlowVerifier.h"
#include "../19A_Schema18CompositionLinker/Schema18CompositionLinker.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

namespace
{
namespace fixture = sge4::qualification::schema18_planning_fixture;
namespace planning = sge4::composition::schema18::planning;
namespace verification = sge4::composition::schema18::verification;
namespace linking = sge4::composition::schema18::linking;

bool WriteBytes(const std::filesystem::path& path,
                std::span<const std::byte> bytes)
{
    std::filesystem::create_directories(path.parent_path());
    return static_cast<bool>(sge4::base::WriteAllBytes(path, bytes));
}

void ResignArtifact(std::vector<std::byte>& bytes)
{
    const auto digest = sge4::base::Sha256(
        std::span<const std::byte>(bytes.data(), bytes.size()).first(bytes.size() - 32));
    std::copy(digest.begin(), digest.end(), bytes.end() - 32);
}
}

int main(int argc, char** argv)
{
    static_assert(!std::is_default_constructible_v<
        verification::VerifiedResourceFlowPlan>);

    auto fixtureResult = fixture::BuildFixture();
    if (!fixtureResult)
    {
        std::cerr << fixtureResult.Error() << '\n';
        return 1;
    }
    const auto& contract = fixtureResult.Value().contract;
    auto proposed = planning::ProposeResourceFlowPlan(contract);
    if (!proposed) return 2;
    auto verified = verification::VerifyAndSeal(contract, proposed.Value());
    if (!verified) return 3;

    auto frozen = linking::FreezeVerifiedComposition(contract, verified.Value());
    if (!frozen)
    {
        std::cerr << frozen.Error().stage << ": " << frozen.Error().message << '\n';
        return 4;
    }
    if (frozen.Value().contract.identity != contract.identity ||
        frozen.Value().plan.identity != proposed.Value().identity ||
        frozen.Value().verifierSeal != verified.Value().Seal() ||
        frozen.Value().fileBytes.empty()) return 5;

    auto read = linking::ReadFrozenComposition(frozen.Value().fileBytes);
    if (!read || read.Value().artifactDigest != frozen.Value().artifactDigest ||
        read.Value().fileBytes != frozen.Value().fileBytes ||
        sge4::composition::schema18::SerializeCompositionContract(
            read.Value().contract) !=
        sge4::composition::schema18::SerializeCompositionContract(contract) ||
        planning::SerializeRawResourceFlowPlan(read.Value().plan) !=
        planning::SerializeRawResourceFlowPlan(proposed.Value())) return 6;

    auto repeated = linking::FreezeVerifiedComposition(contract, verified.Value());
    if (!repeated || repeated.Value().fileBytes != frozen.Value().fileBytes)
        return 7;

    auto corrupt = frozen.Value().fileBytes;
    corrupt[corrupt.size() / 2] ^= std::byte{0x01};
    if (linking::ReadFrozenComposition(corrupt)) return 8;

    // Re-signing the outer artifact cannot forge the independent Verifier seal.
    auto forgedSeal = frozen.Value().fileBytes;
    constexpr std::size_t VerifierSealOffset = 96;
    forgedSeal[VerifierSealOffset] ^= std::byte{0x01};
    ResignArtifact(forgedSeal);
    auto forgedSealRead = linking::ReadFrozenComposition(forgedSeal);
    if (forgedSealRead || forgedSealRead.Error().stage != "frozen/seal") return 9;

    auto truncated = frozen.Value().fileBytes;
    truncated.resize(truncated.size() - 1);
    if (linking::ReadFrozenComposition(truncated)) return 10;

    if (argc == 3 && std::string_view(argv[1]) == "--emit")
    {
        if (!WriteBytes(argv[2], frozen.Value().fileBytes)) return 11;
    }

    std::cout << "L4V1-F6 Schema 18 Frozen Composition tests passed\n";
    std::cout << "Embedded authority: Contract + Raw Plan + independent Verifier seal\n";
    std::cout << "Rematerialization boundary: Frozen reader validates without Compiler or Planner\n";
    std::cout << "Artifact bytes: " << frozen.Value().fileBytes.size() << '\n';
    std::cout << "Artifact SHA-256: "
              << sge4::base::ToHex(frozen.Value().artifactDigest) << '\n';
    return 0;
}
