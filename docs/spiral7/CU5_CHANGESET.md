# Spiral 7 CU5 finalization changeset

Base commit:

```text
67cb40b5204e1e06ecac576206ba969ec2db02b6
```

## Changed

- The standard CU5 runner is now the routine regression gate.
- Added a four-invocation Debug A/B/C WARP smoke mode.
- Routine full qualification, Controlled Recovery, RemoveDevice and Fresh rematerialization run in Release.
- Routine evidence must equal the accepted exhaustive-audit SHA-256 values.
- Added an explicit full Debug/Release exhaustive determinism audit runner.
- Updated manifest, evidence ledger, application instructions, progress and proof ledger.

## Added

```text
tests/Run-Spiral7CU5ExhaustiveAudit.ps1
tests/tools/Finalize-Spiral7CU5Layout.ps1
run_sge4_5_spiral7_cu5_exhaustive_audit.bat
docs/spiral7/CU5_TEST_OPERATION_POLICY.md
docs/spiral7/CU5_DELETIONS.md
```

## Deleted

```text
docs/spiral7/CU5_EXECUTION_OPTIMIZATION_V2.md
```

The deleted document was an intermediate optimization note. Its durable requirements are consolidated into the final Architecture, operation-policy and evidence documents.

## Unchanged

- 128-invocation Frozen authority.
- A/B/C Candidate semantics and order.
- Evidence serialization and accepted evidence bytes.
- Recovery and Device-epoch authority.
- Schema 17, Runtime 17, canonical Backend and Composition Contract.
- Runtime Candidate-policy authorization: `None`.

Git operations are not included.
