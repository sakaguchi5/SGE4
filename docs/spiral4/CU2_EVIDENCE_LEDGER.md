# Spiral 4 CU2 evidence ledger

| Evidence | Producer | Required observation |
|---|---|---|
| Active Work Semantic | `120_ActiveWorkSemantic` | canonical bytes and explicit Nmax/Nf/group types |
| Indirect extension bytes | `121_Spiral4Contracts` | canonical parse and SHA-256 corruption rejection |
| Schema insufficiency | `CU2_SCHEMA17_INSUFFICIENCY.md` | state exists; executable command contract does not |
| Command Signature | `122_Spiral4IndirectExecution` | Dispatch, stride 12, max command count 1 |
| Argument production | GPU Compute shader | `ceil(Nf/64),1,1` |
| State authority | command list | UAV barrier and UAV→INDIRECT_ARGUMENT |
| Indirect issue | command list | `ExecuteIndirect`, no CPU dispatch choice |
| Active observation | WARP readback | active prefix equals CPU reference |
| Inactive observation | WARP readback | inactive tail preserves sentinel |
| Zero Work | Nf=0 | groupCountX=0 and no output write |
| Legacy compatibility | CU2 verifier | six legacy Schema/Runtime/Backend files byte-unchanged |
| Solution identity | Identity gate + CU2 verifier | all four CU2 projects and Debug/Release x64 configurations registered |
| Determinism subset | CU2 runner | Debug/Release architecture bundles byte-identical |

## CU2 completion boundary

CU2 proves one explicit indirect execution architecture.

It does not yet prove:

- Planner independence,
- opaque Verifier authority,
- cross-context seal rejection,
- Candidate-family equivalence,
- whole-composition Recovery,
- real hardware performance.

Those remain CU3–CU6.

## Fix01 correction

The initial CU2 package created valid Project files but omitted their Solution
registration. This was not an implementation failure in ExecuteIndirect; it
was an integration failure at the repository identity boundary.

Fix01 keeps `Verify-SGE4_5Identity.ps1` authoritative and changes the Solution
instead of bypassing or relaxing the gate.


## Fix02 phase-aware regression evidence

| Mode | Purpose | Later CU projects allowed |
|---|---|---:|
| CU1 Snapshot | prove the exact contract-only CU1 checkout | No |
| CU1 Regression | prove the immutable CU1 contract survives later work | Yes |

CU2 invokes `Verify-Spiral4CU1.ps1 -Mode Regression`. This preserves the CU1
contract checks while avoiding the false requirement that CU2 projects remain
absent after CU2 has intentionally created them.
