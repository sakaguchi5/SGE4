# SGE4-5 repository instructions for Codex

## Mission

Implement **Spiral 2: Fixed Dynamic Motor Palette and Hierarchical Representation Composition** completely, using repository `sakaguchi5/SGE4` at baseline `afaed65d9400f3f80a7f5a9094edd9029082f95d`.

This is an implementation task, not merely a planning or documentation task.

## Read before editing

Read these files in order:

1. `docs/spiral2-codex/00_OWNER_OVERLAY.md`
2. `docs/spiral2-codex/01_MASTER_DIRECTIVE.md`
3. `docs/spiral2-codex/02_BASELINE_AND_REPOSITORY_MAP.md`
4. `docs/spiral2-codex/03_SCOPE_AND_AUTHORITY.md`
5. `docs/spiral2-codex/04_STAGE_EXECUTION_PLAN.md`
6. `docs/spiral2-codex/05_INVARIANT_LEDGER.md`
7. `docs/spiral2-codex/06_PROJECT_AND_DEPENDENCY_PLAN.md`
8. `docs/spiral2-codex/07_TEST_AND_RUNNER_PLAN.md`
9. `docs/spiral2-codex/08_LEVEL4_SUFFICIENCY_AND_EXTENSION_POLICY.md`
10. `docs/spiral2-codex/09_EVIDENCE_AND_PROGRESS_PROTOCOL.md`
11. `docs/spiral2-codex/10_SELF_REVIEW_POLICY.md`
12. `docs/spiral2-codex/11_FINAL_OWNER_DECISION_GATE.md`
13. `docs/spiral2-codex/reference/SPIRAL2_ROADMAP_EXTRACTED.md`

The extracted roadmap is the full technical source specification. The Owner Overlay is authoritative only for the explicit process changes it names.

## Environment

- Windows
- PowerShell and `.bat` runners are the canonical execution path
- Visual Studio 2026 / toolset v145
- x64 Debug and Release
- `/std:c++latest`
- Solution: `SemanticGpuEngine4-5.sln`
- Do not use Visual Studio F5 as the canonical test or reproduction path

## Autonomous execution

Proceed through CU0-CU6 without asking for confirmation after each Stage.

When a test fails:

1. Preserve the failing evidence.
2. Diagnose the first root cause.
3. Repair the implementation or specification evidence.
4. Add or retain a regression test.
5. Rerun the narrowest relevant suite, then the required CU gate.
6. Continue to the next Stage only after the current Stage passes.

Do not stop for ordinary design choices. Choose the smallest solution justified by the roadmap and existing constitutional boundaries, document the choice, and continue.

## Non-negotiable architecture rules

- Dynamic Local Motor Palette values remain outside Frozen identity.
- Bone count and topology remain in Frozen identity.
- All candidates consume exactly the same canonical Motor input bytes.
- Candidate A must not receive a CPU-generated Matrix Palette.
- Raw Candidate Graphs cannot Freeze, execute, or enter Composition.
- The Verifier must not call Planner implementation.
- The Leaf Compiler accepts only a Verifier-sealed representation.
- Runtime and Backend must not redecide candidate kind, lowering position, schedule, allocation, state, or synchronization.
- No speculative future types, capability bits, reserved operations, or empty extension hooks.
- Do not weaken, delete, skip, or rewrite a test merely to make a gate pass.
- Do not change expected digests until the semantic reason is documented and verified.
- Preserve Level 4 v1 and Spiral 1 regression evidence.

## Independent review policy

Do not launch a separate review agent, worktree reviewer, or PR review pass unless the Owner later asks.

This does not relax the mandatory independent software Verifier.

Perform the self-audit in `docs/spiral2-codex/10_SELF_REVIEW_POLICY.md` at every CU boundary.

## Level 4 sufficiency

At S2-09 and S2-14, test the existing Level 4 contract before extending it.

If it is insufficient, do not add a Runtime hack. Create the required insufficiency evidence, define the smallest Frozen contract extension, perform Canonical Reconstruction, rerun Level 4 and Spiral 1 regressions, and continue autonomously.

## Test cadence

Use narrow suites during editing, Gate before each CU commit, Regression for cross-boundary changes, and Freeze for formal CU or Architecture completion.

After intentionally adding or changing tracked source files:

```powershell
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File .\tests\tools\Update-SourceManifest.ps1
cmd /c .\tests\run_architecture.bat
```

Review the `SOURCE_MANIFEST.sha256` diff before committing.

## Git behavior

- Verify the baseline before implementation.
- A continuation branch is valid when `afaed65d9400f3f80a7f5a9094edd9029082f95d` is an ancestor and existing Spiral 2 progress is internally consistent.
- Do not discard unrelated Owner changes.
- Prefer one intentional commit per Completion Unit.
- Do not push unless explicitly instructed.
- Do not rewrite published history.

## Mandatory final stop

Complete Architecture, WARP, determinism, recovery, real-GPU measurement when available, and Decision Evidence.

Then stop at the selection of the capability after Spiral 2.

Allowed:

- rank candidates
- recommend one candidate
- explain evidence and tradeoffs

Forbidden:

- set an authoritative next capability
- modify `NextCapabilitySelection` away from `DeferredByOwner`
- create the next capability's empty types or projects
- begin the next Spiral

Required final state:

```text
NextCapabilitySelection = DeferredByOwner
SelectionStatus = OWNER_DECISION_REQUIRED
```
