# Spiral 7 CU5 evidence ledger

| Evidence | Required result |
|---|---|
| Portable authority self-test | 128 invocations and 384 verified Candidate artifacts validate without Runtime policy selection. |
| Architecture qualification | 128 chained WARP invocations and 384 Candidate executions pass all output and exact-write observations. |
| Corpus coverage | All frozen Active counts, Transition counts and eight transition kinds are present. |
| Runtime handle integrity | Forged representation, History and Completion identities are rejected. |
| Controlled Recovery | Device epoch increments; all old handles are stale; explicit `A_t/M_t` rebind and representation rebuild are mandatory. |
| Forced recovery seed | `W_t=A_t`, `H_t=empty`; exact modified-survivor generations are rebuilt without stale-history retention. |
| Actual removal | `ID3D12Device5::RemoveDevice` enters quarantine and excludes the removed WARP LUID from retry. |
| Fresh process | The same 128-invocation Frozen authority rematerializes and a representative 16-invocation prefix produces qualified observations. |
| Determinism | Debug and Release Architecture, Controlled-Recovery and Fresh-process evidence bytes are identical. |
| Legacy boundary | Schema 17, Runtime 17, canonical Backend and Composition Contract remain unchanged. |

The actual evidence SHA-256 values are printed by the Owner-run gate and are not predeclared in this overlay.

## Execution optimization V2 evidence condition

The Architecture, Controlled Recovery and Fresh evidence bytes remain defined by the same serializers. V2 changes only the physical test harness schedule: three independently verified Candidate dispatches are submitted together per invocation, all three Output and Write-Audit resources are read back, and the existing observation predicates are evaluated unchanged. Debug/Release byte equality remains mandatory.
