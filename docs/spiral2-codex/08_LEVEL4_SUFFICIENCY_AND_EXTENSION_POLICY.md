# Level 4 sufficiency and extension policy

This policy applies especially to S2-09 and S2-14.

## First principle

Do not extend Level 4 because a feature seems likely to be needed.

Run the minimum vertical slice using existing contracts first.

## S2-09 required experiment

```text
2 bones
2 frames
1 candidate branch
FrameInvocation -> Leaf dynamic data -> Composition Flow
```

Attempt this with existing Schema 17, Runtime 17, and Level 4 v1.

## If existing Level 4 is sufficient

Create an explicit record:

```text
Level4Sufficiency = Sufficient
SchemaExtension = None
RuntimeExtension = None
```

Continue without adding placeholder capability.

## If existing Level 4 is insufficient

Do not add a temporary Runtime or Backend shortcut.

Create `docs/spiral2/LEVEL4_SUFFICIENCY_FINDING.md` containing:

- the exact unrepresentable fact,
- which layer owns that fact,
- the current contract that fails,
- a reproducible negative scenario,
- the missing Frozen information,
- Runtime and recovery consequences,
- rejected alternatives,
- the smallest proposed extension,
- versioning consequences.

Likely candidates may include:

- Composition-level Dynamic Invocation mapping
- explicit connection from Leaf Dynamic Slot to shared Flow
- invocation generation identity
- continuous-frame input ownership

These are hypotheses, not preapproved features.

## Autonomous minimal extension

The Owner has delegated evidence-backed minimal Level 4 extension decisions to Codex for this task.

Codex must:

1. prove the insufficiency,
2. choose the smallest contract extension preserving constitutional ownership,
3. define invariants and rejection rules,
4. define Frozen representation and versioning,
5. define Runtime and recovery meaning,
6. implement discovery proof,
7. perform Canonical Reconstruction,
8. reconnect Spiral 2,
9. rerun Level 4 v1 and Spiral 1 regressions,
10. document the outcome.

Do not ask for routine approval. Continue unless the environment prevents implementation.

## Prohibited shortcuts

- Runtime infers topology or candidate kind
- Backend constructs candidate-specific Matrix input
- stale Invocation is silently reused after recovery
- Dynamic values enter Frozen digest
- a new field is added but not verified or consumed
- discovery code is renamed “canonical” without reconstruction
- a broad universal resource/variant abstraction is introduced
