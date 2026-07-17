#pragma once

#include "../00_Foundation/Result.h"
#include "../02_SemanticModel/SemanticModel.h"
#include "../05_TargetContract/TargetModel.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace sge4::qualification::schema18_corpus
{
inline constexpr std::size_t FrozenCaseCount = 54;
inline constexpr std::size_t FrozenNormalizedCaseCount = 3;
inline constexpr std::size_t FrozenEndpointCount = 12;
inline constexpr std::size_t FrozenReadOnlyEndpointCount = 9;
inline constexpr std::size_t FrozenWriteOnlyEndpointCount = 3;

// This is the existing Level-3/Schema-17 semantic corpus identity. F2 must
// begin from precisely the same 54 source scenarios before boundary adaptation.
inline constexpr const char* ExpectedSourceSemanticCorpusDigest =
    "43322a7c33d2ae82bfa577c0fee79ad7b915bdd8d38655eac1631a4e81c89d9b";

// Frozen by the portable F2 semantic qualification and rechecked by 46B.
inline constexpr const char* ExpectedLeafSemanticCorpusDigest =
    "6ecafb273230ccf3e0ea0afe020fea440e5e26f9d4c7d330e1ef7922c5268d34";
inline constexpr const char* ExpectedEndpointAuthoringDigest =
    "1455e539474d5433d4e827013279b3ca7e8f7a4561a8c56fb71b1d451fff057a";

enum class BoundaryNormalization : std::uint16_t
{
    None = 0,
    ComputeExportProxy = 1
};

struct EndpointAuthoring final
{
    semantic::ResourceId resource;
    std::string stableKey;
};

struct LeafCase final
{
    std::string name;
    semantic::SemanticGraph sourceGraph;
    target::D3D12TargetProfile sourceProfile;
    semantic::SemanticGraph leafGraph;
    target::D3D12TargetProfile leafProfile;
    std::vector<EndpointAuthoring> endpoints;
    BoundaryNormalization normalization = BoundaryNormalization::None;
};

struct CorpusIdentity final
{
    std::string sourceSemanticCorpusDigest;
    std::string leafSemanticCorpusDigest;
    std::string endpointAuthoringDigest;
    std::size_t normalizedCaseCount = 0;
    std::size_t endpointCount = 0;
    std::size_t readOnlyEndpointCount = 0;
    std::size_t writeOnlyEndpointCount = 0;
};

[[nodiscard]] const char* BoundaryNormalizationName(BoundaryNormalization value) noexcept;
[[nodiscard]] base::Result<std::vector<LeafCase>, std::string> BuildLeafCorpus();
[[nodiscard]] base::Result<CorpusIdentity, std::string> ComputeCorpusIdentity(
    std::span<const LeafCase> corpus);
}
