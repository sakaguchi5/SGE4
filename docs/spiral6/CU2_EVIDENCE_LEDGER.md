# Spiral 6 CU2 evidence ledger

The CU2 runner emits:

```text
build/tests/spiral6-cu2/debug.bin
build/tests/spiral6-cu2/release.bin
```

The two files must be byte-identical.

The evidence binds:

- Sparse Semantic identity,
- exact set identity per case,
- Compact Index artifact identity,
- expected and observed Dispatch groups,
- active write count,
- active CPU-reference qualification,
- inactive sentinel byte stability,
- homogeneous-coordinate and finite-value qualification,
- full output digest.

Negative evidence rejects duplicate, unsorted, out-of-range, corrupted, and cross-set inputs before GPU execution.
