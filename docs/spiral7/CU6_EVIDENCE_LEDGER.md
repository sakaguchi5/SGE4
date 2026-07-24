# CU6 Evidence Ledger — CU6-2

## Accepted Owner run

```text
Accepted final commit       f802c12b162569a869c214da22b80142b3a4a0dd
Adapter                     NVIDIA GeForce RTX 4070 Laptop GPU
Adapter fingerprint         8a7a825e84c0a157b16c6e54f51b833d6d191173e7e1d28d84062a60840a6b3f
Canonical evidence          0d7a50c448269cbb346ea80b0ceda09e7f28b10c068ed20a96d6c47621b4802a
Refinement evidence         44fa4dd72e64aab300bec53fc45c6ae2bf7a7012630b0e15a83fe6717c7ff80b
Combined Decision CSV       e07af74f1f5f53143adbaf5e6fbd65d54d83bc4a0d314c6e7d648bf4edcd7bd4
Combined Decision report    b840cb1425f5e45ccac2f0c84750584313350458062dab98f8552290e8bfda81
External evidence ZIP       9ee852c2a2574a8cb9c163b6e94213b855317bba74319e6443e9aa98c10c24d9
```

| Evidence | Accepted property |
|---|---|
| S7M3 canonical evidence | 4 x 5 x 8 cases/run; 320 accepted blocks; 0 rejected attempts |
| S7M3 refinement evidence | 4 x 5 x 5 cases/run; 300 accepted blocks; 1 rejected attempt excluded and replaced |
| Debug/Release self-test bundle | byte-identical SHA-256 `84b761da6529aa5e553a6d557a98606a8eb43c140e83b2a3d2eab958fee4b047` |
| Direct/reloaded canonical report | byte-identical SHA-256 `82a498d44e8564c38d6346e412b370874a4d3ad41b9590945b360924f901129c` |
| Direct/reloaded refinement report | byte-identical SHA-256 `6e68bd9fd48e5f3684e126cdb5d057bb0bf65b8527dc8552b38a6453568ad745` |
| Combined Decision Map | 220 unique coordinates; refinement overrides duplicate T=1024/4096 coordinates |
| Timestamp censoring | 190 samples, all zero-dispatch; nonzero-dispatch censored count 0 |
| Decision classes | ZeroDispatch 20 / StableWinner 41 / StableEquivalent 61 / Unresolved 98 |
| Stable single winner | B only; 19 UniformStride and 22 HashScatter coordinates |
| Stable equivalent sets | B/C in 59 coordinates; A/B/C in 2 coordinates |
| Stable crossover brackets | 8 Transition-direction and 10 Active-direction brackets |

No Evidence authorizes Runtime Candidate selection or a universal winner. The Owner subsequently closed Spiral 7 and selected Level 4 v2 Canonical reconstruction.
