# SGE4 Stage 0D Change Log

## From SGE3

- Created independent `SemanticGpuEngine4-5.sln` and `sge4` namespace.
- Reissued every Visual Studio Project GUID for the SGE4 solution.
- Added `05A_CompilationInput` as the shared source-analysis and D3D12 Schema-17 feasibility boundary.
- Removed the `08_CandidatePlanner -> 11_D3D12PackageLowering` ProjectReference.
- Replaced `CandidatePlanner::CompileWithPolicy` with two Package-free phases:
  - `PlanCandidates`
  - `SelectCandidate`
- Moved verified candidate lowering and compile orchestration into `12_SGE4_5Compiler`.
- Added mandatory re-sealing immediately before each `LowerVerifiedPlan` call.
- Renamed and expanded `28_SGE3CompatibilityOracle` to retain:
  - the historical direct canonical reference path;
  - a separate SGE3-style planning orchestration for Stage-0D comparison.
- Extended Planning tests to compare SGE4 and SGE3-reference candidate manifests, selected Plan identities, and Package bytes.
- Extended dependency verification to reject Package responsibilities in CandidatePlanner and production lowering calls outside SGE4_5Compiler.
- Added `run_sge4_5_stage0d.bat` as the authoritative Debug/Release freeze gate.

## Intentionally unchanged

- Semantic language and 54-case corpus.
- ExecutionPlanIR fields and Plan identity encoding.
- Independent verifier rules.
- D3D12 Package Schema 17 and Runtime 17.
- Frozen Package wire ABI (`SGE2PKG`).
- Runtime, Backend, WARP/readback, external, temporal, aliasing, and recovery contracts.


## Stage 0D Freeze修正

SGE4固有のmanifest identityと、SGE3から凍結継承するsemantic contract identityを分離しました。これにより、Solution／namespaceの世代名だけではsemantic corpus digestが変化しません。期待digest `43322a7c33d2ae82bfa577c0fee79ad7b915bdd8d38655eac1631a4e81c89d9b`は維持し、誤って発生した世代依存digestへ更新していません。
