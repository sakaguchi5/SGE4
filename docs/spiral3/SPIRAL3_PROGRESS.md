# Spiral 3 progress

Baseline: `398891936bf498fed532b26c06884bbfbf66f3a8`.

## Qualified by the owner

- CU0 research contract and baseline gate.
- CU1 fixed eight-bone hierarchy, R1/R4/R16/R64/R256/R512 Semantic/corpus/reference and explicit count boundary types.
- CU1 corpus SHA-256: `3E5D7B36908CDD64B6DC39ECB57DE53453C393C05687AB45E2A9F3931AE38FFD`.

## Included in this delivery

- CU2 Candidate, Planner and Planner-independent Verifier projects.
- A/B/C graph generation for all six reuse workloads.
- Private verifier-issued `VerifiedReuseRepresentationV1` authority boundary.
- Compile-time Raw/Verified construction and conversion checks.
- 24 identity, workload, execution-model, node, dispatch, Program and edge mutation gates.
- Debug A/B/Release deterministic Evidence runner.

## Portable qualification

The CU2 C++23 tests were compiled and executed in the generation environment:

```text
18 verified reuse/candidate plans
24 invalid proposals rejected
Verified plan SHA-256:
EC1080429217FC82871AC47B7F4D4FE1DF84DC2401143BF9E1FE3018C7BF2156
```

## Next Windows qualification

```bat
run_sge4_5_spiral3_cu2_candidate_authority.bat
```

CU3-CU6 are not included in this delivery.
