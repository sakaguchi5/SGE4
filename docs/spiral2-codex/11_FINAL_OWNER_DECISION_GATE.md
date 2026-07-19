# Final Owner decision gate

## Codex-owned closure: S2-22A

Codex completes all evidence it can execute and creates:

```text
docs/spiral2/STAGE21_DECISION_EVIDENCE.md
docs/spiral2/STAGE22A_EVIDENCE_CLOSURE.md
build/tests/spiral2/measurement/SPIRAL2_DECISION_EVIDENCE_REPORT.md
```

The report may rank and recommend candidates, but its authoritative fields must be:

```text
NextCapabilitySelection = DeferredByOwner
SelectionStatus = OWNER_DECISION_REQUIRED
```

A recommended candidate must be explicitly labeled:

```text
RecommendationAuthority = NonAuthoritative
```

## Forbidden final actions

Codex must not:

- set an authoritative next Spiral,
- authorize an unrelated Level 4 extension for future use,
- create the next capability's project skeleton,
- add empty Capability bits or reserved operations,
- begin implementing the recommendation,
- emit the broad `SPIRAL 2 EXPERIMENT COMPLETE` banner.

## Required Codex banner

```text
============================================================
SGE4-5 SPIRAL 2 MEASUREMENT AND DECISION EVIDENCE COMPLETE
Architecture, WARP, determinism, recovery and measurement evidence are closed.
Next capability selection is intentionally deferred by the Owner.
NextCapabilitySelection = DeferredByOwner
SelectionStatus = OWNER_DECISION_REQUIRED
============================================================
```

If real hardware is unavailable, replace the first line with:

```text
SGE4-5 SPIRAL 2 NON-HARDWARE EVIDENCE COMPLETE
```

and explicitly list hardware measurement as pending.

## Owner-only closure: S2-22B

After the Owner selects a capability, a later small task may:

1. write the authoritative selection record,
2. update `NextCapabilitySelection`,
3. verify that no preselection placeholder was added,
4. emit the final `SPIRAL 2 EXPERIMENT COMPLETE` banner.

This pack intentionally stops before that action.
