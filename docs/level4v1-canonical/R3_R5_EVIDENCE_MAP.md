# Level 4 v1 Canonical R3-R5 Evidence Map

| Proof ID | Evidence |
|---|---|
| RUNTIME-001 | `48_CanonicalCompositionRuntimeTests`: all Leaves report one non-zero DeviceDomain epoch. |
| RUNTIME-002 | `MaterializeSharedResources`: one native Buffer is created for each verified allocation. |
| RUNTIME-003 | `SubmitStaticComposition`: canonical schedule and verified endpoint bindings are consumed without replanning. |
| RUNTIME-004 | `PrepareForEndpoint` and `UpdateAfterRelease`: verified state handoff and token chain are authoritative. |
| RUNTIME-005 | fan-out and repeated diamond frames retire all readers before producer reuse. |
| RUNTIME-006 | independent, linear, fan-out, multi-input, diamond and headless WARP scenarios pass. |
| RUNTIME-007 | single-presenter WARP scenario binds the surface only to the verified presenter Leaf. |
| RECOVERY-001 | R4 releases shared resources, endpoint tokens and all Leaf instances before native recovery. |
| RECOVERY-002 | controlled rebuild increments DeviceDomain epoch. |
| RECOVERY-003 | stale resource and completion-token handles are rejected after epoch advance. |
| RECOVERY-004 | the Frozen Composition is re-read through `ReadVerifiedFrozenComposition` after reacquisition. |
| RECOVERY-005 | all verified Leaves and Resource Flows are rematerialized exactly. |
| RECOVERY-006 | pre- and post-controlled-rebuild GPU output bytes are identical. |
| RECOVERY-007 | presenter Leaf and surface ownership survive whole-composition controlled reconstruction. |
| RECOVERY-008 | actual `RemoveDevice` records loss, excludes removed LUID, and reaches `AwaitingAdapter` on WARP. |
| RECOVERY-009 | retry preserves `AwaitingAdapter` and retains zero Leaf/resource objects while the excluded adapter remains unavailable. |
| REGRESSION-001 | R1 and R2 canonical qualification runners pass. |
| REGRESSION-002 | Stage 0D Foundation Freeze passes after R3-R5. |
| ARCH-001 | `Verify-Level4v1R3R5Boundaries.ps1` rejects planning/freeze authority in Runtime and Composition authority in Backend. |
| ARCH-002 | scripts and ZIP contain no repository-history extraction path; source inventory and dependency checks pass. |
