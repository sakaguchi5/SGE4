# Self-review policy

The Owner does not require a separate independent review pass.

Codex must still perform a disciplined self-audit at every CU boundary.

## Mandatory self-audit

```powershell
git status --short
git diff --check
git diff --stat
git diff -- SOURCE_MANIFEST.sha256
cmd /c .\tests\run_architecture.bat
```

Then inspect the implementation diff for:

- accidental speculative types or reserved capabilities,
- Runtime/Backend dependencies on Semantic or Planner,
- Verifier calls into Planner implementation,
- raw Candidate execution or Freeze paths,
- dynamic Palette bytes entering Frozen identity,
- Candidate A receiving CPU Matrix input,
- hidden Matrix materialization in Direct PGA,
- missing mutation tests,
- expected digest changes without semantic explanation,
- stale evidence files reused as current results,
- relaxed assertions or skipped tests,
- inconsistent Debug/Release project configuration,
- non-ASCII repository filenames.

## Verifier independence is still mandatory

A separate human/agent review and an independent software Verifier are different concepts.

The process review is omitted.

The software Verifier must still:

- use a separate project,
- avoid Planner implementation references,
- independently rederive authority facts,
- issue the only valid seal,
- reject copied, replayed, or cross-Semantic seals.

## Completion statement

Every CU evidence ledger should include:

```text
IndependentReview = NotRequiredByOwner
SelfAudit = Passed
IndependentSoftwareVerifier = Required
```
