# Spiral 3 progress

Baseline: `398891936bf498fed532b26c06884bbfbf66f3a8`.

## Included

- CU0 research contract, manifest, non-goals, project boundaries and proof ledger.
- CU1 fixed eight-bone hierarchy, six-R Semantic/corpus/reference.
- Explicit `BoneCountV1`, `ReuseCountV1`, and `PointCountV1` boundary types.
- CU0/CU1 Windows runners and static verification scripts.

## Portable qualification

The CU1 C++23 test was compiled and executed in the generation environment. R1/R4/R16/R64/R256/R512 passed, repeated evidence bytes matched, and the evidence SHA-256 was:

```text
3E5D7B36908CDD64B6DC39ECB57DE53453C393C05687AB45E2A9F3931AE38FFD
```

## Awaiting Windows qualification

```bat
run_sge4_5_spiral3_cu0_contract_baseline.bat
run_sge4_5_spiral3_cu1_reuse_semantic.bat
```

CU2-CU6 are not included in this delivery.
