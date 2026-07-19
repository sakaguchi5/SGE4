# Master Directive

## Objective

Starting from `afaed65d9400f3f80a7f5a9094edd9029082f95d`, implement the complete Spiral 2 roadmap as an integrated extension of `SemanticGpuEngine4-5.sln`.

The final implemented experiment must demonstrate:

```text
fixed hierarchy structure
+ per-frame canonical Local PGA Motor Palette
        ↓
three verified candidate Leaf groups
        ↓
one verified Level 4 Frozen comparison Composition
        ↓
WARP and real-GPU observation
        ↓
determinism and recovery evidence
```

## Normative precedence

When instructions differ, use this order:

1. `00_OWNER_OVERLAY.md`
2. this Master Directive
3. `11_FINAL_OWNER_DECISION_GATE.md`
4. the extracted original roadmap
5. existing baseline constitutional and ABI documents
6. implementation convenience

Implementation convenience never overrides an authority boundary.

## Baseline facts

- Repository: `sakaguchi5/SGE4`
- Baseline: `afaed65d9400f3f80a7f5a9094edd9029082f95d`
- Existing solution: `SemanticGpuEngine4-5.sln`
- Existing Level 4 v1 projects: 16-23 and 46-49A
- Existing Spiral 1 projects: 60-79
- Existing target ABI: D3D12 Schema 17 / Runtime 17
- Existing execution model: Buffer-only finite static same-frame DAG, single writer, one DeviceDomain, whole-composition recovery

## Required development shape

- Add Spiral 2 implementation in the 80-99 project range.
- Reuse proven lower-level contracts where semantically correct.
- Do not copy-paste an entire alternate Runtime or Backend.
- Do not preemptively modify Schema or Runtime.
- Preserve existing project dependency restrictions.
- Add Stage/CU runners, static verification, positive tests, mutation tests, WARP qualification, freeze evidence, and measurement evidence.

## Definition of progress

Documentation without executable tests is not Stage completion.

A Stage is complete only when:

1. the implementation exists,
2. the positive path passes,
3. required negative mutations are rejected at the intended boundary,
4. relevant existing regressions pass,
5. evidence is written and digestable,
6. the progress ledger is updated.

## Completion target

Codex owns completion through S2-22A.

S2-22B is Owner-only.
