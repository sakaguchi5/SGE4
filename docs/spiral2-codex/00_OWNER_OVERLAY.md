# Owner Overlay

This file contains the only intentional process overrides to the original Spiral 2 roadmap.

## O1. No separate independent review

No separate Codex review agent, worktree reviewer, or pull-request review is required.

Codex performs implementation, self-audit, and the complete test/evidence cycle in the same working context.

This does **not** remove or weaken the Independent Hierarchy Verifier. Verifier independence is a software authority boundary, not a human review process.

## O2. Work autonomously

Codex should implement as much as the environment permits and should not pause after each Stage or Completion Unit for approval.

Ordinary implementation choices, test repairs, and evidence-backed minimal Level 4 extensions are delegated to Codex.

## O3. Only the post-Spiral-2 capability selection is reserved

The only planned Owner hold is the authoritative selection of the capability or Spiral after Spiral 2.

Codex must complete:

- Architecture
- WARP corpus
- multi-frame invocation
- determinism
- recovery
- real GPU measurement when hardware is available
- Decision Evidence Report
- candidate ranking and optional recommendation

Codex must not make the final selection.

Required record:

```text
NextCapabilitySelection = DeferredByOwner
SelectionStatus = OWNER_DECISION_REQUIRED
```

## O4. Split S2-22 into two parts

- **S2-22A Evidence Closure**: Codex-owned and required.
- **S2-22B Next Capability Selection**: Owner-only and intentionally deferred.

The broad `SPIRAL 2 EXPERIMENT COMPLETE` banner remains withheld until S2-22B is performed by the Owner.

Codex may instead emit:

```text
============================================================
SGE4-5 SPIRAL 2 MEASUREMENT AND DECISION EVIDENCE COMPLETE
Architecture, WARP, determinism, recovery and measurement evidence are closed.
Next capability selection is intentionally deferred by the Owner.
NextCapabilitySelection = DeferredByOwner
SelectionStatus = OWNER_DECISION_REQUIRED
============================================================
```

## O5. Hardware limitation behavior

If a real hardware Adapter is unavailable, Codex must not fabricate performance evidence.

It should finish every non-hardware task, produce exact commands and an environment blocker record, and leave only hardware measurement plus the Owner selection unresolved.
