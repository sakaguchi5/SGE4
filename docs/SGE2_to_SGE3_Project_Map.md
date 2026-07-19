# SGE2 to SGE4 Project Map

This map records provenance without creating a build dependency.

| SGE2 responsibility | SGE4 responsibility |
|---|---|
| `00_Base` | `00_Foundation` |
| `02_SemanticModel` | `02_SemanticModel` |
| `03_SemanticBuilder` | `03_SemanticBuilder` |
| `04_SemanticAnalysis` | `04_SemanticAnalysis` |
| `05_TargetModel` | `05_TargetContract` |
| `12_Level3PlanModel` | `06_ExecutionPlanModel` |
| `13_Level3PlanVerifier` | `07_ExecutionPlanVerifier` |
| `Level3Compiler` candidate logic | `08_CandidatePlanner` |
| `07_FrozenPackageCore` | `09_FrozenPackageCore` |
| `08_D3D12PackageSchema` | `10_D3D12PackageSchema` |
| verified part of `06_D3D12TargetCompiler` | `11_D3D12PackageLowering` |
| no single equivalent | `12_SGE4_5Compiler` production facade |
| `09_PackageRuntime` | `13_PackageRuntime` |
| `10_D3D12Executor` | `14_D3D12Backend` |
| `11_PlatformWin32` | `15_PlatformWin32` |
| Level-1/2/3 test support | `24-28` qualification support |
| Level-1/2/3 tests | `30-45` property-oriented qualification projects |

The structural change is not a renumbering alone. Candidate planning, verification, verified lowering and the normal compiler facade are physically distinct projects. Runtime and Backend have no route back to Source or Compiler projects.
