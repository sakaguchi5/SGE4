#include "./D3D12PackageLowering.h"

#include "../../../../canonical/base/BinaryIO.h"
#include "../../../../canonical/base/Sha256.h"
#include "../../../../leaf/model/analysis/SemanticAnalysis.h"
#include "../../../../leaf/artifact/package/PackageReader.h"
#include "../../../../leaf/verifier/ExecutionPlanVerifier.h"
#include "../../artifact/D3D12Encoding.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <d3d12.h>
#include <d3dcompiler.h>
#include <d3d11shader.h>
#include <wrl/client.h>

#ifdef interface
#undef interface
#endif
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include <algorithm>
#include <bit>
#include <cctype>
#include <array>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxguid.lib")

namespace sge4::compiler::d3d12
{
using Microsoft::WRL::ComPtr;
namespace pkg = package::d3d12_v13;

namespace
{
#include "./detail/ShaderCompilation.inl"
#include "./detail/LoweringUtilities.inl"
#include "./detail/LoweringStages.inl"
#include "./detail/PackageLowering.inl"
#include "./detail/PackageFreezing.inl"
}

#include "./detail/CompilerApi.inl"
}
