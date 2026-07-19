# Evidence and progress protocol

## Live repository outputs

Create and maintain:

```text
docs/spiral2/SGE4-5_Spiral2_Completion_Spec_v0.1.md
docs/spiral2/NEXT_CAPABILITY_SELECTION_FROM_SPIRAL1.md
docs/spiral2/SPIRAL2_PROGRESS.md
docs/spiral2/STAGE_MAP_V1.md
docs/spiral2/PROOF_LEDGER_V1.md
docs/spiral2/NON_GOALS_V1.md
docs/spiral2/OBSERVATION_CONTRACT_V1.md
docs/spiral2/CORPUS_V1.md
docs/spiral2/PROJECT_BOUNDARIES_V1.md

docs/spiral2/CU0_EVIDENCE_LEDGER.md
...
docs/spiral2/CU6_EVIDENCE_LEDGER.md
```

Add focused Stage documents where the evidence is architecturally important, especially:

```text
STAGE09_DYNAMIC_SOURCE_VERTICAL_SLICE.md
STAGE14_LEVEL4_SUFFICIENCY.md
STAGE19_ARCHITECTURE_FINAL_FREEZE.md
STAGE20_REAL_GPU_MEASUREMENT.md
STAGE21_DECISION_EVIDENCE.md
STAGE22A_EVIDENCE_CLOSURE.md
```

## Build artifacts

Place generated transient evidence under:

```text
build/tests/spiral2/
```

Examples:

- canonical manifests
- verified candidate manifests
- Frozen Package bytes
- Frozen Composition bytes
- readback records
- measurement binary evidence
- generated reports

Do not commit transient logs unless an existing repository policy explicitly commits that evidence format.

## Per-Stage record

Each Stage entry records:

- Stage and CU
- baseline/head commit
- implementation summary
- changed projects
- positive scenarios
- negative mutations
- exact commands
- exit status
- evidence paths
- SHA-256 digests
- unresolved environment blocker
- invariant status changes

## Decision Evidence questions

Stage S2-21 must answer:

1. Was Level 4 v1 sufficient for Dynamic Palette Composition?
2. If not, exactly which Frozen contract was missing?
3. Were per-frame values completely separated from Frozen identity?
4. Did one meaning actually generate different Leaf groups?
5. What changed between early, absent, and late Matrix materialization?
6. Did the winning candidate vary with bone count K or hierarchy depth C?
7. Did 32-byte Motor versus 48-byte Matrix intermediate palettes affect measurements?
8. What was the arithmetic-versus-buffer tradeoff for Direct PGA?
9. Did Hybrid capture advantages of both?
10. Did the Verifier detect graph and lowering-boundary mutations?
11. Which next candidates are supported: Temporal Flow, variable Work, Vertex Consumer, Variant Set, or no extension?
12. Is any Level 4 addition genuinely demanded by evidence?

## Honesty rule

Never replace missing hardware evidence with estimated numbers.

Never claim a Stage passed when its required executable was not run.

A partial completion report must clearly separate:

```text
Qualified
ImplementedButNotRun
BlockedByEnvironment
OwnerDecisionRequired
```
