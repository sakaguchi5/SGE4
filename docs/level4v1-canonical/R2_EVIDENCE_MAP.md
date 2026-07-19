# Level 4 v1 Canonical R2 Evidence Map

| Proof ID | R2 evidence | Status |
|---|---|---|
| ABI-001 | Endpoint access is derived from frozen SRV/UAV view authority; author declarations contain no access field | qualified by boundary + negative tests |
| ABI-002 | every required Schema 17 External slot receives one key and one Resource Flow binding | qualified |
| ABI-003 | kind, format, bytes, state, synchronization and required flag are re-derived from embedded Leaf | qualified |
| ABI-004 | malformed or inconsistent Leaf interface is rejected before any D3D12 object exists | qualified |
| CONTRACT-001..004 | Internal/Input/Output producer-consumer shapes | qualified |
| CONTRACT-005 | duplicate, missing, wrong-access and incompatible bindings rejected | qualified |
| CONTRACT-006 | zero or one Presenter; forged two-Presenter Contract rejected | qualified |
| CONTRACT-007 | reversed Leaf, Endpoint, Resource and consumer declaration order emits identical Contract bytes | qualified |
| PLAN-001 | Raw Plan represents allocation, schedule, binding, handoff, signal, wait and recovery proposals | qualified |
| PLAN-002 | cyclic same-frame Contract rejected by Planner and independently by Verifier | qualified |
| PLAN-003 | stable-key IDs and ordered topological ready-set define deterministic schedule | qualified |
| VERIFY-001 | Raw Plan public surface has no Execute, Submit or Freeze entry | qualified by boundary |
| VERIFY-002 | VerifiedCompositionPlan has a private token constructor; only VerifyAndSeal is friend | qualified |
| VERIFY-003 | Verifier source does not call ProposeCompositionPlan and re-derives every represented decision | qualified |
| VERIFY-004 | resigned mutations of Contract identity, allocation, schedule, binding, access, handoff, signal, wait and recovery are rejected | qualified |
| ARTIFACT-001 | FreezeVerifiedComposition accepts only Validated Contract + Verified Plan | qualified |
| ARTIFACT-002 | high-level Reader repeats Leaf/Contract/Plan/Certificate verification | qualified |
| ARTIFACT-003 | digest-valid forged Contract, Plan and Certificate containers are rejected | qualified |
| ARTIFACT-004 | fresh Debug A/B and Release emit byte-identical R2 Artifacts | Windows qualification required |
| REGRESSION-001..002 | R1 and Stage 0D Freeze remain gates | Windows qualification required |
| ARCH-001..002 | Contract/Planner/Verifier dependency boundaries, script contracts and SOURCE_MANIFEST | qualified by scripts |

R2 completion requires the normal Windows runner to pass Debug, Release, R1 regression and Stage 0D Foundation Freeze.
