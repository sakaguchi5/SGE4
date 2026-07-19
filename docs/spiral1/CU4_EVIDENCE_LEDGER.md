# Spiral 1 Completion Unit 4 Evidence Ledger

- Base commit: `cbc062e4801c596703ae86a4747f5d4725af09e5`
- Completion Unit: CU4 Frozen Comparison Architecture
- Internal stages: Stage 10 Four-Leaf Frozen Comparison Composition / Stage 11 WARP Corpus
- Status before Windows qualification: implementation supplied; evidence pending local MSVC/WARP gate

## Frozen authority aggregate

Schema 17 `FrozenComposition` freezes Leaf package bytes, the Composition Contract, verified schedule/resource/state/synchronization decisions, and the Composition verifier certificate. Matrix and Direct PGA constants remain Schema 17 DynamicDataSlots. CU4 therefore defines the authoritative experiment artifact as `FrozenComparisonScenarioV1`, which freezes:

- the verified four-Leaf Frozen Composition,
- the Matrix and Direct PGA `FrozenRepresentationLeafV1` aggregates,
- both verifier-approved locked constant payloads,
- source and comparison Leaf package bytes,
- scenario and Observation Contract identities.

Runtime receives the two locked bindings from this aggregate. It does not select a lowering or accept caller-selected Matrix/Motor bytes.

## Four Leaf roles

```text
L0 CorpusAndReferenceSource
L1 MatrixExpandedTransform
L2 DirectPgaMotorTransform
L3 PointComparison
```

## Frozen Resource Flows

```text
InputPoints:       L0 -> L1, L2
ReferencePoints:   L0 -> L3
MatrixOutput:      L1 -> L3
DirectPgaOutput:   L2 -> L3
ComparisonRecords: L3 -> Composition Output
```

All Resource Flows are Buffer-only, same-frame, static, single-writer, one DeviceDomain, no presenter.

## Stage 10 evidence

- four embedded Leaves and five Resource Flows
- InputPoints fan-out has exactly one writer and two readers
- Composition Planner output is independently verified before freeze
- swapped Matrix/Direct comparison endpoints are rejected by the Spiral 1 role gate even though they remain structurally valid Level 4 v1 graphs
- a producer endpoint cannot be rebound as a second writer
- locked constant mutation and Frozen Composition corruption are rejected

## Stage 11 evidence

- all Corpus V1 cases S00-S15 execute as one Frozen Comparison Scenario each
- L0 GPU output bytes equal frozen input/reference bytes
- Matrix and Direct outputs satisfy the CPU binary64 reference contract
- L3 GPU Comparison Record bytes equal deterministic CPU `PointComparisonRecordV1` bytes
- point-index-ordered CPU aggregation produces zero mismatches and zero non-finite outputs
- fresh Debug A, Debug B and Release WARP evidence must be byte-identical on the same WARP environment

## Point record arithmetic qualification

CPU binary64 reference generation remains unchanged. After the reference has been rounded once to `Float4Point`, `PointComparisonRecordV1` is generated from binary32 inputs by an explicitly mirrored binary32 sequence on CPU and GPU:

- subtract in binary32,
- apply absolute value in binary32,
- evaluate tolerance using separate binary32 multiply and add steps,
- use `precise` HLSL intermediates to forbid contraction,
- serialize the resulting binary32 bit patterns directly.

This removes GPU-double implementation variation from the byte-identity requirement while preserving the frozen tolerance formula. A record mismatch reports its first global byte, point-record index, record offset, and the GPU/CPU byte values.

## Qualified invariants after local pass

- S1-I08 Level 4 v1 Buffer Flow alone constructs the comparison Composition
- S1-I09 every internal Flow has exactly one writer
- S1-I11 S00-S15 satisfy the CPU reference error contract
- S1-I12 Matrix and Direct PGA pairwise error satisfies the contract
- S1-I13 Comparison aggregation is deterministic and point-index ordered

S1-I10 remains partially qualified here: runtime uses only Frozen Composition decisions and locked aggregate constants. Its full recovery/fresh-process authority qualification belongs to CU5 / Stage 12.


## Observation Contract V2 supersession evidence

S00-S11 established record byte parity. S12 then exposed a V1 conditioning counterexample: large rigid-rotation terms cancel in one output component, so component-local relative scaling underestimates the normal binary32 forward error. Observation V2 keeps the numeric tolerances, CPU reference, corpus, record layout and authority chain unchanged, but scales every component by the full point-vector magnitude. V1 remains implemented as a negative control; the deterministic S12 representative must fail V1 and pass V2.
