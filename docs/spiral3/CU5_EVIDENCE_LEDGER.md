# Spiral 3 CU5 evidence ledger

Status before owner execution: implemented, not yet qualified on Windows.

| Evidence | Required result |
|---|---|
| WARP corpus | 6 reuse cases x 12 frame palettes = 72 executions. |
| Meaning | A/B/C satisfy reference and pairwise Observation Contract V2. |
| Source authority | Dynamic reference bytes and Package-owned point bytes reproduce their frozen sources. |
| Observer authority | 304-byte GPU records agree with CPU reconstruction for absolute, relative and ULP metrics. |
| Dynamic/frozen boundary | Palette changes alter execution evidence but never Frozen Composition bytes. |
| Order independence | R64 execution evidence is invariant under frame-order reversal. |
| Determinism | Per-R WARP evidence and Freeze bundle are byte-identical across Debug/Release/fresh processes. |
| Controlled recovery | 11 Leaves and 12 resources are released/rematerialized; epoch advances and stale handles fail. |
| Actual removal | Runtime enters `AwaitingAdapter`, releases all objects, rejects stale submission, and excludes the removed adapter. |
| Fresh rematerialization | R256/F07 evidence equals pre-removal evidence in a fresh process. |

The runner prints the authoritative SHA-256 identities after successful owner qualification.
