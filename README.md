# Semantic GPU Engine 4

SGE4 is a new Visual Studio solution that reconstructs the complete proven SGE2 execution path under a stricter project topology.

It is not linked to SGE2 projects, binaries, repositories, or namespaces.

```text
SemanticGpuEngine4.sln
namespace: sge4
Toolset: Visual Studio 2026 / v145
Language: /std:c++latest
Platform: x64
Target ABI: D3D12 Schema 17 / Runtime 17
```

## Production flow

```text
SemanticGraph
  -> SemanticAnalysis
  -> SemanticObligation
  -> CandidatePlanner
  -> ExecutionPlanIR[]
  -> independent VerifyAndSeal
  -> VerifiedExecutionPlan
  -> D3D12PackageLowering
  -> FrozenExecutablePackage
  -> PackageRuntime
  -> D3D12Backend
```

There is no normal compile entry that lowers a raw `ExecutionPlanIR`. `12_SGE4Compiler` is the production facade. Its CanonicalSafe mode is still a policy, not a separate legacy compiler.

The old direct canonical route exists only behind `28_SGE3CompatibilityOracle` so that Planning qualification can prove byte identity against the SGE2 behavior. No production or runtime project may reference that Oracle.

## Build

```powershell
cmd /c .\build.bat Debug
cmd /c .\build.bat Release
```

## Full qualification

```powershell
cmd /c .\run_sge4_freeze.bat
```

This performs:

- architecture dependency verification;
- Debug and Release builds;
- Semantic, Package, Compiler, Planning and Authority tests;
- fixed 54-Package canonical corpus qualification;
- fresh-process and Debug/Release manifest identity;
- WARP execution and readback;
- Buffer and Texture upload verification;
- FrameLocal, Temporal, Aliasing and External boundary tests;
- controlled and actual device-removal recovery paths;
- Classical, SDF and direct PGA frontend equivalence;
- verified alternative schedule/queue observation equivalence.

## Demo

```powershell
cmd /c .\run_demo.bat Debug
```

## Important ABI decision

The first SGE4 freeze intentionally targets the already-proven SGE2 container and D3D12 wire contract (`SGE2PKG`, Schema 17, Runtime 17). That does not create a source dependency. It preserves the executable ABI as a compatibility theorem. A future SGE4 wire revision must be introduced as a versioned Schema/Runtime change rather than silently changing existing Package meaning.


## Stage 0D Freeze correction

The generation-specific manifest identity is separated from the frozen SGE3 semantic-contract identity. Semantic corpus digests now remain generation-neutral for SGE3-to-SGE4 non-regression, while emitted SGE4 manifests retain an SGE4-specific identity. The frozen expected digest remains `43322a7c33d2ae82bfa577c0fee79ad7b915bdd8d38655eac1631a4e81c89d9b`; it was not replaced by the accidental generation-dependent digest.

## Tiered test operation

The `tests` directory separates daily development checks from full freeze qualification.

```bat
run_sge4_dev.bat
run_sge4_gate.bat
run_sge4_regression.bat
run_sge4_freeze.bat
```

See `tests/README_JA.md` and `tests/OPERATIONS_GUIDE_JA.md` for the full workflow.
