# SGE4-5 Spiral 2 Codex implementation pack

This archive is a repository-overlay instruction pack for implementing Spiral 2 from:

- Repository: `sakaguchi5/SGE4`
- Baseline commit: `afaed65d9400f3f80a7f5a9094edd9029082f95d`
- Solution: `SemanticGpuEngine4-5.sln`
- Target: D3D12 Package Schema 17 / Runtime 17 unless evidence proves a minimal extension is required

It contains no C++ implementation. It gives Codex a durable, repository-local body of instructions, the complete extracted roadmap, progress/evidence templates, test policy, and the final Owner decision gate.

## Use

Recommended clean start:

```powershell
git switch -c spiral2-implementation afaed65d9400f3f80a7f5a9094edd9029082f95d
```

Extract this ZIP into the repository root so that these paths exist:

```text
AGENTS.md
README_SPIRAL2_CODEX_PACK.md
docs/spiral2-codex/...
codex-prompts/...
```

Then open Codex at the repository root and paste the contents of:

```text
codex-prompts/01_START_FULL_IMPLEMENTATION.txt
```

Codex is instructed to:

1. Read this entire pack before changing implementation files.
2. Verify that `afaed65d9400f3f80a7f5a9094edd9029082f95d` is the baseline or an ancestor of the current work.
3. Add the instruction files to `SOURCE_MANIFEST.sha256` as an intentional inventory change.
4. Implement CU0 through CU6 without waiting for an independent review.
5. Run the appropriate Dev, Gate, Regression, Freeze, WARP, recovery, and real-GPU evidence commands.
6. Diagnose and repair failures rather than weakening tests.
7. Stop only at the final selection of the capability after Spiral 2.

## Important distinction

The Owner has removed the process requirement for a separate independent review agent.

The **Independent Hierarchy Verifier remains mandatory software architecture**. It must independently rederive and verify candidate facts and must not call the Planner implementation.

## Final stopping point

Codex may produce a non-authoritative recommendation and ranking for the next capability, but it must record:

```text
NextCapabilitySelection = DeferredByOwner
SelectionStatus = OWNER_DECISION_REQUIRED
```

It must not create a placeholder abstraction or begin implementing the recommended next capability.
