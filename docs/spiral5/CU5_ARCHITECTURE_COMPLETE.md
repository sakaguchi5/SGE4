# Spiral 5 CU5 architecture completion

Baseline commit: `a3d9a19a1a3033b723df918e70f5af6a24d884a0`.

CU5 qualifies the complete frozen Temporal schedule corpus:

```text
P1, P2, P4, P8, P16, P32, P64, BurstThenHold, Irregular
```

Each schedule executes the three verified candidates over 128 ordered invocations:

```text
9 schedules x 3 candidates x 128 invocations
= 3456 Candidate invocations per architecture qualification process
```

For every invocation CU5 requires:

- the independently verified Update/Hold dispatch dimensions,
- CPU-reference equivalence,
- pairwise byte-identical A/B/C output,
- finite output and homogeneous coordinate validity,
- explicit retained completion ordinal,
- byte-stable retained state on Hold,
- no unauthorized Hierarchy or Consumer write on B/C Hold.

CU5 also qualifies fresh-process determinism, controlled Recovery, actual D3D12 device removal, stale history rejection, current-epoch dynamic rebind and explicit generation-zero reseed.

Successful execution may declare:

```text
SGE4-5 SPIRAL 5 ARCHITECTURE COMPLETE
```

This declaration does not authorize a Runtime temporal policy. Real-GPU performance measurement and the non-authoritative Decision Report remain CU6 work.
