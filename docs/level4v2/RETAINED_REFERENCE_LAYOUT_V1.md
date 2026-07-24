# Retained reference layout V1

## Retained until migration closure

- All Spiral 1–7 implementation Projects.
- All contract and architecture manifests.
- All proof and evidence ledgers.
- All independent Verifier and mutation-test Projects.
- CU5 routine and exhaustive Architecture gates.
- CU6-2 measurement and Decision reconstruction gate.
- The accepted Source Manifest and frozen legacy-boundary hashes.

These files are not part of the future public v2 architecture merely because they remain in the repository. They are the migration oracle.

## Historical-only entry points

The per-CU preparation and application runners are no longer normal development commands. They remain tracked because existing static Verifiers intentionally prove the original staged package layout and lineage.

Do not use them to start Level 4 v2. Use:

```powershell
.\run_sge4_5_spiral7_reference_gate.bat
```

## Physical retirement rule

A Spiral Project or document may be removed only after a v2 migration test identifies the replacement invariant and proves one of:

1. byte-identical preserved evidence,
2. observation-equivalent versioned replacement,
3. explicit rejection because the behavior was experiment-specific and never Canonical.

No bulk deletion is authorized before that mapping exists.
