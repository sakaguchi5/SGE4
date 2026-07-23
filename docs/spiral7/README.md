# SGE4-5 Spiral 7

## Capability

```text
Versioned Sparse Delta Flow and Verified Incremental History Lowering
```

Spiral 7 combines verified indirect quantity, temporal history and exact sparse membership over `A_t`, `M_t`, `N_t`, `R_t`, `W_t`, `H_t` and `T_t`.

## Status

- CU1–CU5: PASSED
- CU6-1: real-GPU canonical surface measured; Decision rule required refinement
- CU6-2: paired Decision authority and high-Transition refinement supplied; Owner run pending

## Candidate family

```text
A = FullActiveDenseRecompute
B = CompactDeltaIndexHistoryReuse
C = AffectedBlockDeltaHistoryReuse
```

## CU6-2 command

```powershell
.\run_sge4_5_spiral7_cu6_prepare.bat
.\run_sge4_5_spiral7_cu6_measurement_decision_evidence.bat
```

The runner performs:

```text
CanonicalSurface:         4 patterns x 5 Active x 8 Transition = 160 cases/run
HighTransitionRefinement: 4 patterns x 5 Active x 5 Transition = 100 cases/run
```

The refinement Transition set is `1024,1536,2048,3072,4096`. It uses more runs and balanced-order cycles than the canonical pass. Duplicate 1024/4096 coordinates are replaced by refinement results in the combined map.

Decision authority is paired agreement. A Candidate is stable only when it beats both alternatives. Median ordering is descriptive. `T=0` is B/C ZeroDispatchEquivalent. Unresolved and zero-dispatch cases do not create crossovers.

Outputs are written under:

```text
build/tests/spiral7-cu6-2/
```

The principal outputs are:

```text
canonical_measurement_evidence_s7m3.bin
high_transition_refinement_evidence_s7m3.bin
combined_paired_decision_cases_v3.csv
combined_paired_decision_report_v3.txt
```

## Authority boundary

```text
RuntimeCandidatePolicyAuthorization = None
UniversalWinnerClaim = Forbidden
Spiral7Closure = OwnerRequired
```

After Owner closure, the next program is Level 4 v2 Canonical reconstruction.
