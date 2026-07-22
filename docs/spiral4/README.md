# SGE4-5 Spiral 4

Spiral 4 studies dynamic Active Work Count and verified indirect Batch Lowering.

## Completed scope

- CU1: Research Contract Freeze
- CU2: Single ExecuteIndirect architecture

CU2 introduces:

```text
ActiveWorkSemanticV1
FrozenSingleIndirectArtifactV1
GPU Argument Producer
Dispatch Command Signature
ExecuteIndirect Consumer
WARP observation
```

Legacy Schema 17 / Runtime 17 / Backend files remain unchanged. CU2 uses a versioned sidecar extension so the missing command contract is explicit without silently changing old Package bytes.

## Run CU2

After copying the CU2 files and Fix01 into commit
`4033e8bf84650b6d1edbb6a8a83d97d5e1c3e4d1`:

```powershell
.\run_sge4_5_spiral4_cu2_fix01_register_solution.bat

powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass `
  -File .\tests\tools\Update-SourceManifest.ps1

.\run_sge4_5_spiral4_cu2_single_execute_indirect.bat
```

The Solution registration step is mandatory. The repository identity gate requires every `.vcxproj` to be a formal member of `SemanticGpuEngine4-5.sln`.

## Read in this order

1. `SGE4-5_Spiral4_Completion_Spec_v0.1.md`
2. `CU2_SCHEMA17_INSUFFICIENCY.md`
3. `CU2_SINGLE_INDIRECT_ARCHITECTURE.md`
4. `CU2_ARCHITECTURE_MANIFEST_V1.json`
5. `PROOF_LEDGER_V1.md`
6. `STAGE_MAP_V1.md`

CU3 is the independent authority stage. CU2’s canonical parser is not a substitute for a Planner-independent Verifier.


## Historical CU verification modes

`Verify-Spiral4CU1.ps1` has two meanings:

```powershell
# Exact CU1 checkout proof
.\tests\tools\Verify-Spiral4CU1.ps1 -Mode Snapshot

# CU1 contract regression inside CU2 or later
.\tests\tools\Verify-Spiral4CU1.ps1 -Mode Regression
```

`-Mode Auto` selects Snapshot when no CU2 manifest exists and Regression after
CU2 has been installed.


## CU3 — independent authority

CU3 adds:

```text
123_ActiveWorkLoweringCandidate
124_ActiveWorkLoweringPlanner
125_ActiveWorkLoweringVerifier
126_Spiral4VerifiedExecution
141_Spiral4AuthorityMutationTests
```

Apply and prepare:

```powershell
.\run_sge4_5_spiral4_cu3_prepare.bat
```

This registers the five projects and regenerates `SOURCE_MANIFEST.sha256`.

Then run:

```powershell
.\run_sge4_5_spiral4_cu3_independent_authority.bat
```

CU3 does not authorize Fixed Maximum or Batched execution. Those become
additional verified Candidate families in CU4.


## CU4 — Candidate family

Prepare the six CU4 projects and regenerate the Source Manifest:

```powershell
.\run_sge4_5_spiral4_cu4_prepare.bat
```

Then execute:

```powershell
.\run_sge4_5_spiral4_cu4_candidate_family.bat
```

CU4 compares correctness and authority, not performance. Real GPU ranking and
crossover evidence remain CU6.
