# Spiral 4 progress

## Current state

CU1 passed at commit `4033e8bf84650b6d1edbb6a8a83d97d5e1c3e4d1`.

CU2 delivery prepared.

```text
[x] Owner selection and Research Contract Freeze
[x] ActiveWorkSemanticV1 with Nmax/Nf/thread-group type boundaries
[x] versioned Single Indirect extension bytes
[x] canonical parser and digest corruption rejection
[x] explicit Dispatch Command Signature contract
[x] GPU Argument Producer
[x] UAV -> INDIRECT_ARGUMENT state transition
[x] mechanical ExecuteIndirect call
[x] WARP cases Nf=0/1/63/64/65/1024/4096
[x] active reference and inactive-tail sentinel observation
[x] legacy Schema 17 / Runtime 17 / Backend files preserved
[ ] CU2 projects registered in SemanticGpuEngine4-5.sln
[x] Fix01 Solution registration tool supplied
[ ] source manifest regenerated after Solution registration
[ ] CU2 runner executed
[ ] CU3 Planner-independent authority started
```

## CU2 architectural status

CU2 is a real D3D12 indirect execution path, but it is not yet the final Verified Candidate authority path.

CU3 must introduce the independent Planner/Verifier separation before the extension may be treated as a general Level 5 lowering authority.

## CU2 Fix01

The first CU2 delivery incorrectly deferred Solution membership until CU3.
That contradicted the repository-wide identity invariant that every `.vcxproj`
must be registered in `SemanticGpuEngine4-5.sln`.

Fix01 restores the invariant by registering all four CU2 projects and both
Debug/Release x64 configurations. The identity verifier is not weakened.


## CU2 Fix02

CU2 then exposed a second integration issue: the CU1 verifier treated
“future projects do not exist yet” as a permanent invariant.

Fix02 separates:

```text
CU1 Snapshot mode
  exact CU1 checkout; later projects and indirect implementation must be absent

CU1 Regression mode
  immutable CU1 contract remains valid; later Completion Units may exist
```

CU2 and all later Spiral 4 gates invoke CU1 in Regression mode. The standalone
CU1 runner uses Auto mode and selects Snapshot before CU2, Regression after CU2.


## CU3 delivery

Baseline: `ca29f228687691769355afe33f390adf04c1ed24`

```text
[x] Raw Active Work Lowering Candidate
[x] independent Planner project
[x] Planner-independent Verifier project
[x] opaque Verified Active Work Lowering type
[x] 27 authority mutation rejections
[x] attacker-recomputed Raw identity negative controls
[x] target / command-signature / observation replay rejection
[x] Verified Plan bound to actual CU2 sidecar artifact
[x] producer and Consumer compiled shader bytecode digests observed
[x] Frozen Verified execution path
[x] Verified WARP cases Nf=0/65/4096
[x] Debug/Release authority bundle comparison
[ ] CU3 project registration and Source Manifest regeneration in destination checkout
[ ] CU3 runner executed
[ ] CU4 Candidate family started
```

CU3 does not change canonical Schema 17, Runtime 17, Composition Runtime, or
the canonical D3D12 Backend.


## CU4 delivery

Baseline: `04488a5203570d559ecd74372513d86abe34e4d0`

```text
[x] A.FixedMaximumGuarded
[x] B.SingleExecuteIndirectDispatch
[x] C.BatchedExecuteIndirectDispatch
[x] Batch sizes 64/128/256/512/1024
[x] independent Candidate-family Verifier
[x] fixed-slot Batch artifacts and 16-byte records
[x] 95 Active Count/Batch partition proofs
[x] 24 mutation and attacker-rehash rejections
[x] GPU Batch-record observation
[x] 21 minimal WARP equivalence cases
[x] byte-identical output across all seven Candidates
[x] CU3 V1 authority left unchanged
[ ] destination Solution registration and Manifest regeneration
[ ] CU4 runner executed
[ ] CU5 full architecture qualification started
```


## CU5 delivery

Baseline: `8577ad1e4bb99f2a1c4d7125c6e586b33fb54154`

```text
[x] one persistent WARP DeviceDomain for A/B/C
[x] full 7 × 19 WARP qualification design
[x] 133 inactive-tail proofs
[x] 95 GPU Batch-record proofs
[x] fresh Debug A / Debug B / Release evidence comparison
[x] controlled Recovery and device-epoch advance
[x] stale handle and submission-token rejection
[x] explicit dynamic-input rebind after Recovery
[x] actual ID3D12Device5::RemoveDevice quarantine
[x] removed WARP LUID exclusion in old process
[x] fresh-process rematerialization
[ ] destination Solution registration and Manifest regeneration
[ ] CU5 runner executed
[ ] Architecture Complete declaration observed
[ ] CU6 real-GPU experiment started
```
