# SGE4-5 Spiral 7

## Capability

```text
Versioned Sparse Delta Flow and Verified Incremental History Lowering
```

Spiral 7 combines verified indirect quantity, temporal history and exact sparse membership into one authority relation over `A_t`, `M_t`, `N_t`, `R_t`, `W_t`, `H_t` and `T_t`. It is the final exploratory Spiral before Level 4 v2 Canonical reconstruction.

## Completion Units

- CU1: Research Contract Freeze — PASSED
- CU2: Sparse Temporal Delta Architecture — PASSED
- CU3: Independent Delta Authority — PASSED
- CU4: Incremental History Candidate Family — PASSED
- CU5: Architecture Qualification — PASSED
- CU6: Real-GPU Measurement and Decision Evidence — implementation supplied; Owner run pending

CU5 accepted commit:

```text
c9f0b5a62e2a7f3c9e0355cdaa1c683819c6dcfa
```

## Candidate family

```text
A = FullActiveDenseRecompute
B = CompactDeltaIndexHistoryReuse
C = AffectedBlockDeltaHistoryReuse
```

CU4 proves per-invocation semantic equivalence. CU5 qualifies the complete Runtime materialization and Recovery boundary. CU6 observes relative performance without changing either result.

## CU5 routine command

```powershell
.\run_sge4_5_spiral7_cu5_architecture_qualification.bat
```

The routine gate preserves the accepted CU5 Architecture evidence in approximately normal regression-test time. The complete Debug/Release audit remains available through:

```powershell
.\run_sge4_5_spiral7_cu5_exhaustive_audit.bat
```

## CU6 commands

```powershell
.\run_sge4_5_spiral7_cu6_prepare.bat
.\run_sge4_5_spiral7_cu6_measurement_decision_evidence.bat
```

CU6 uses a real non-software D3D12 Adapter and measures:

```text
4 Sparse patterns
x 5 Active counts
x 8 Transition counts
= 160 exact cases per run
```

Every case first revalidates A/B/C output and write authority. Timing then uses D3D12 Timestamp Query, instrumentation-free output-equivalent Timing shaders, six balanced Candidate orders and a fixed within-block drift control.

Outputs:

```text
build/tests/spiral7-cu6/measurement_evidence_v1.bin
build/tests/spiral7-cu6/decision_cases_v1.csv
build/tests/spiral7-cu6/decision_report_v1.txt
```

## Authority boundary

Runtime and Backend remain forbidden to choose Active membership, modified membership, transition action, Candidate or history policy. Spiral 7 adds no Canonical Level 4 v2 ABI mutation.

```text
RuntimeCandidatePolicyAuthorization = None
UniversalWinnerClaim = Forbidden
Spiral7Closure = OwnerRequired
```

After Owner closure, exploratory Spiral growth stops and Level 4 v2 Canonical reconstruction begins.
