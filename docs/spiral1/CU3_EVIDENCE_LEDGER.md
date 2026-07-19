# Spiral 1 Completion Unit 3 Evidence Ledger

- Completion Unit: `CU3 Dual Frozen Leaf Qualification`
- Stages: `08 Matrix Leaf` and `09 Direct PGA Leaf`
- Baseline commit: `53da5d1dbb15ccce984180a67db17b7140a35f2a`
- Status before execution: Proposed implementation evidence

## Authority closure

```text
Verified Representation Plan
  -> Spiral1 Leaf Compiler
  -> canonical Semantic Graph bridge
  -> existing CandidatePlanner / ExecutionPlan Verifier
  -> existing SGE4_5Compiler Package lowering
  -> verified Program binary substitution
  -> Schema 17 Frozen Leaf Package
```

The final Package retains every existing verified scheduling, allocation, state,
binding and synchronization decision. Only the Compute shader blob is replaced
with the exact binary whose digest was sealed by the Independent Representation
Verifier. The rebuilt Package is decoded and validated again before it is
returned.

## Stage 08 evidence

- MatrixExpandedComputeV1 verified constants become Package-owned immutable data.
- Input and output are exactly two External Buffer slots.
- The final Package contains the verified Matrix Program binary digest.
- Representative S00, S07 and S14 cases execute on WARP and satisfy the CPU reference contract.

## Stage 09 evidence

- DirectPgaMotorComputeV1 verified Motor coefficients become Package-owned immutable data.
- Input and output use the same schema and external slot contract as Matrix.
- The final Package contains the verified Direct PGA Program binary digest.
- Representative S00, S07 and S14 cases execute on WARP and satisfy the CPU reference contract.

## CU3 joint evidence

- One Semantic identity produces two different Candidate identities.
- Matrix and Direct PGA produce different Package execution digests.
- Both Packages have the same I/O and Observation Contract.
- Pairwise observation has zero mismatches for the representative qualification set.
- Fresh Debug A, Debug B and Release evidence bundles are byte-identical.

Full S00-S15 Composition execution remains owned by CU4 / Stage 10-11.


## Schema 17 constant binding correction

Stage 08 initial implementation attempted to bind the verified constant payload from an Immutable Buffer as `ConstantData`. Existing Schema 17 rejects this because `RootParameterKind::ConstantBuffer` is represented only by a `DynamicDataSlot`; the Backend likewise requires that slot before dispatch.

CU3 therefore keeps Schema 17 / Runtime 17 unchanged and defines the authoritative Spiral 1 Leaf as the aggregate `FrozenRepresentationLeafV1`, not the raw `.sgep` bytes alone. The aggregate freezes:

- the verifier-sealed Representation Plan,
- the Schema 17 Package,
- `lockedDynamicSlot = 0`,
- the exact verifier-approved constant payload.

Execution must bind those locked bytes to slot 0. Mutation of the locked payload is rejected by `ValidateFrozenRepresentationLeafV1`. This is a fixed-value use of the existing invocation mechanism, not runtime lowering reselection. A raw `.sgep` extracted from the aggregate is not by itself a qualified Spiral 1 Representation Leaf.
