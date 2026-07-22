# Spiral 5 CU5 evidence ledger

## Canonical generated evidence

The CU5 runner generates:

```text
build/tests/spiral5-cu5/architecture-debug-a.bin
build/tests/spiral5-cu5/architecture-debug-b.bin
build/tests/spiral5-cu5/architecture-release.bin

build/tests/spiral5-cu5/controlled-debug-a.bin
build/tests/spiral5-cu5/controlled-debug-b.bin
build/tests/spiral5-cu5/controlled-release.bin

build/tests/spiral5-cu5/fresh-rematerialization-debug-a.bin
build/tests/spiral5-cu5/fresh-rematerialization-debug-b.bin
build/tests/spiral5-cu5/fresh-rematerialization-release.bin
```

Each three-file group must be byte-identical.

## Architecture evidence

Architecture evidence contains:

- canonical Temporal Semantic bytes,
- all nine schedule bytes,
- all Raw and independently Verified Candidate authorities,
- Runtime authority bytes,
- Domain and Candidate binding-set identities,
- all 27 schedule/candidate observations,
- all 3456 invocation observations,
- completion and retained-history handles,
- a trailing evidence digest.

## Controlled Recovery evidence

Controlled evidence records:

- frozen authority before Recovery,
- epoch transition,
- P64 observation before Recovery,
- stale handle/token/history rejection,
- current-epoch rebind requirement,
- explicit generation-zero reseed requirement,
- P64 observation after Recovery,
- equality of pre/post-Recovery semantic observations.

## Noncanonical actual-removal evidence

Actual `RemoveDevice` output is intentionally console evidence rather than canonical binary evidence because removal reason and diagnostic availability are platform observations. The pass gate requires quarantine, same-LUID exclusion and rejection of all stale temporal authority.
