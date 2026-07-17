#pragma once

#include "../00_Foundation/Result.h"
#include "../02_SemanticModel/SemanticModel.h"
#include "../05_TargetContract/TargetModel.h"
#include "../10A_D3D12CompositionSchema18/D3D12CompositionSchema18.h"
#include "../11_D3D12PackageLowering/D3D12PackageLowering.h"

#include <string>
#include <vector>

namespace sge4::compiler::composition
{
using Error = d3d12::CompileError;

struct EndpointDeclaration final
{
    semantic::ResourceId resource;
    std::string stableKey;
};

struct CompositionLeafCompileOutput final
{
    std::vector<std::byte> packageBytes;
    std::string executionDigestHex;
    std::vector<std::string> completedStages;
    std::vector<package::d3d12_v18::LeafCompositionEndpointArtifact> endpoints;
};

// Explicit Schema-18 entry. The ordinary SGE4 compiler remains Schema 17.
// Every non-surface External Buffer must be declared exactly once. Access is
// derived from the validated SemanticGraph; callers cannot declare direction.
[[nodiscard]] base::Result<CompositionLeafCompileOutput, Error>
CompileCompositionLeafCanonical(
    const semantic::SemanticGraph& graph,
    const target::D3D12TargetProfile& targetProfile,
    std::vector<EndpointDeclaration> declarations);
}
