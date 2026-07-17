# SGE4 Stage 0D Constitution

## Purpose

Stage 0D is the final foundation gate before Package composition begins. It must preserve every represented Level-3 planning decision and every proven Schema-17 runtime observation while changing the ownership of compile orchestration.

## Required dependency direction

```text
Semantic / Analysis / Target Contract
  -> CompilationInput
  -> ExecutionPlanModel
  -> ExecutionPlanVerifier
  -> CandidatePlanner

CandidatePlanner + D3D12PackageLowering
  -> SGE4Compiler
  -> FrozenPackage / Runtime / Backend
```

CandidatePlanner must not reference Package schema, Package lowering, Runtime, or Backend.

## Authority rule

CandidatePlanner may report that a candidate verified, but it does not export a reusable verifier capability token. SGE4Compiler must call `VerifyAndSeal` immediately before `LowerVerifiedPlan`. A mutated raw Plan therefore cannot cross the Package boundary.

## Compatibility rule

The following remain fixed at Stage 0D:

- Semantic corpus digest and 54 canonical inputs
- D3D12 Package Schema 17
- Runtime 17
- canonical Package bytes
- candidate manifest bytes
- selected Plan identity for each fixed policy/profile case
- Authority rejection behavior
- WARP/readback, external, temporal, aliasing, and recovery observations

## Level 4 entry rule

Package composition must be added above existing Leaf Packages as a distinct Contract/LinkPlan/LinkVerifier layer. Runtime must never infer cross-Package state, synchronization, version compatibility, or ownership.
