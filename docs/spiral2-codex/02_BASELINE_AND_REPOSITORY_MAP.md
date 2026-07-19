# Baseline and repository map

## Exact baseline

```text
Repository: sakaguchi5/SGE4
Commit:     afaed65d9400f3f80a7f5a9094edd9029082f95d
Commit role: Spiral 1 measurement and decision evidence complete
```

Historical fact: the baseline preserved the Spiral 1 next-capability choice as `DeferredByOwner`. The Owner has now selected the Spiral 2 described by this pack. Record that as a new post-baseline Owner decision; do not rewrite the historical Spiral 1 Stage 14 document.

## Baseline verification

Run before implementation:

```powershell
git status --short
git rev-parse HEAD
git merge-base --is-ancestor afaed65d9400f3f80a7f5a9094edd9029082f95d HEAD
```

Valid starts:

- `HEAD == afaed65d9400f3f80a7f5a9094edd9029082f95d`, or
- the baseline is an ancestor and the branch contains coherent Spiral 2 progress.

Invalid starts:

- the baseline is not an ancestor,
- unrelated uncommitted changes would be overwritten,
- the solution identity is not `SemanticGpuEngine4-5.sln`.

Do not silently reset or delete Owner changes.

## Existing project bands

```text
00-15  Foundation, Semantic, Planning, Package, Runtime, Backend
16-23  Level 4 Frozen Composition, DeviceDomain, shared resources, runtime, recovery
20-28  Experiment frontends and scenarios
30-49A test and Level 4 qualification projects
50-51 launcher and package compiler
60-69 Spiral 1 semantic, contracts, candidate, planner, verifier, compiler, observer, scenarios
70-79 Spiral 1 tests, freeze, performance, launcher
80-99 reserved by this roadmap for Spiral 2
```

## Existing critical files to inspect

```text
README_JA.md
SemanticGpuEngine4-5.sln
SOURCE_MANIFEST.sha256
tests/OPERATIONS_GUIDE_JA.md
tests/Invoke-SGE4_5Tests.ps1
tests/tools/Update-SourceManifest.ps1
tests/tools/Verify-SourceManifest.ps1

docs/level4v1-canonical/COMPLETION_SPECIFICATION.md
docs/level4v1-canonical/PROOF_LEDGER.md
docs/level4v1-canonical/RECONSTRUCTION_POLICY.md

docs/spiral1/SGE4-5_Spiral1_Completion_Spec_v0.3.md
docs/spiral1/ARCHITECTURE_FINAL_FREEZE_V1.md
docs/spiral1/STAGE13_REAL_GPU_MEASUREMENT_V1.md
docs/spiral1/STAGE14_DECISION_EVIDENCE_V1.md
docs/spiral1/STAGE_MAP_V1.md

run_sge4_5_foundation.bat
run_sge4_5_dev.bat
run_sge4_5_gate.bat
run_sge4_5_regression.bat
run_sge4_5_freeze.bat
run_sge4_5_cu1_semantic_observation.bat
run_sge4_5_cu2_representation_authority.bat
run_sge4_5_cu3_dual_frozen_leaf.bat
run_sge4_5_cu4_frozen_comparison.bat
run_sge4_5_cu5_architecture_final_freeze.bat
run_sge4_5_cu6_measurement_decision_evidence.bat
```

## Initial qualification

Before C++ changes:

```powershell
cmd /c .\run_sge4_5_foundation.bat
cmd /c .\run_sge4_5_gate.bat
```

For S2-02 Foundation Baseline Freeze, also run the strongest baseline qualification that the local environment supports:

```powershell
cmd /c .\run_sge4_5_freeze.bat
```

If real hardware measurement is not rerun at baseline, preserve and cite the committed Spiral 1 measurement evidence rather than fabricating a new run.

## Source inventory

Adding this pack changes the tracked inventory. After reviewing the files:

```powershell
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass `
  -File .\tests\tools\Update-SourceManifest.ps1

cmd /c .\tests\run_architecture.bat
```

Review `SOURCE_MANIFEST.sha256` before committing.
