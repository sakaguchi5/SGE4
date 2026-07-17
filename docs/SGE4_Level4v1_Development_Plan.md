# SGE4 Level 4 v1 Development Plan

## Sequencing rule

Stages are completed in order. A stage may add tests for its own boundary, but may not pre-empt authority assigned to a later layer. Existing Stage 0D artifacts are checked throughout and `SOURCE_MANIFEST.sha256` is updated only after final acceptance.

Implementation status: L4V1-1 through L4V1-10 are implemented. Final acceptance is performed by `run_sge4_level4v1.bat`; this document does not substitute for a passing run.

## L4V1-1 — Capability Constitution

Freeze this constitution, composition contract, and development plan. No product behavior changes in this stage.

## L4V1-2 — Composition Contract Model

Add `16_CompositionContract` and `46_CompositionContractTests`. Decode Schema-17 Leaves, extract external-slot facts, validate explicit endpoint declarations, canonicalize IDs, and compute contract identity. Do not reference Runtime or Backend.

## L4V1-3 — Link Plan Model

Add `17_LinkPlanModel`. Define raw plan records for Package schedule, shared resource allocation, endpoint bindings, signal/wait dependencies, state handoffs, and recovery. The model contains data only and exposes no execution path.

## L4V1-4 — Independent Link Verifier

Add `18_LinkPlanVerifier`, `47_LinkPlanningTests`, and `48_LinkAuthorityTests`. Add a deterministic linker proposal in `19_PackageLinker`, independently verify it, and make `VerifiedLinkPlan` constructible only by `VerifyAndSeal`. Cover cycles, identity corruption, binding, ordering, state, target compatibility, writer authority, and signal/wait authority.

## L4V1-5 — Frozen Composition Artifact

Complete `19_PackageLinker` with canonical writer/reader and the standalone embedded-Leaf v1 container. Prove insertion-order independence, round-trip bytes, same-process repeatability, corruption rejection, and Debug/Release/fresh-process identity.

## L4V1-6 — Shared DeviceDomain

Extend `13_PackageRuntime` and `14_D3D12Backend` with a DeviceDomain API while keeping single-Package APIs as compatibility facades. Multiple independent Leaves in a domain must share the D3D12 device and epoch. No cross-Package resource is connected yet.

## L4V1-7 — Composition-owned Shared Buffer

Move shared Buffer and completion ownership from Leaf instance/slot to DeviceDomain/resource identity. Bind the same native Buffer to compatible endpoints in multiple Leaves. Prove producer-token wait, state handoff, stale epoch rejection, and WARP readback.

## L4V1-8 — Static Package DAG Runtime

Add `29_CompositionRuntime` and `49_CompositionRuntimeTests`. Load only verified frozen compositions and execute their fixed schedule for independent, linear, fan-out, multi-input, diamond, headless, and single-presenter cases. Runtime must not call the linker or verifier.

## L4V1-9 — Composition Recovery

Recover the complete DeviceDomain. Prove simultaneous epoch change, stale resource/token rejection, complete Leaf/shared-resource rematerialization, temporal reset, unchanged LinkPlan identity, controlled rebuild, and actual-device-removal behavior. When the removed adapter cannot be reacquired in-process, the domain remains `AwaitingAdapter`, rejects execution, and the same frozen composition must rematerialize in a fresh process.

## L4V1-10 — Final Qualification

Extend architecture, development, gate, regression, and freeze runners. Qualification must include:

- unchanged 54 Leaf Package bytes and planning manifest;
- all existing Level 1–3 suites;
- contract, link plan, verifier, and artifact determinism;
- raw-plan execution impossibility and corrupt-identity rejection;
- same-process, fresh-process, and Debug/Release composition-byte identity;
- WARP producer/consumer readback, fan-out, and diamond DAG;
- controlled and actual-removal whole-domain recovery;
- frozen-composition rematerialization without compiler/linker availability.

The final banner is:

```text
SGE4 LEVEL 4 V1
STATIC VERIFIED PACKAGE COMPOSITION
FINAL FREEZE PASSED
```
