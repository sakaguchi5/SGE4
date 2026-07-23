# SGE4-5 Spiral 7

## Capability

```text
Versioned Sparse Delta Flow and Verified Incremental History Lowering
```

Spiral 7 is the final exploratory Spiral before Level 4 v2 Canonical reconstruction.

It combines:

```text
Spiral 4: verified indirect execution quantity
Spiral 5: verified temporal history
Spiral 6: verified exact sparse membership
```

into one authority relation:

```text
A_t  current exact Active Set
M_t  exact modified surviving members
N_t  activation
R_t  deactivation
W_t  update
H_t  retained valid history
T_t  physical transition
```

## Completion Units

- CU1: Research Contract Freeze — PASSED
- CU2: Sparse Temporal Delta Architecture — PASSED
- CU3: Independent Delta Authority — PASSED
- CU4: Incremental History Candidate Family — PASSED
- CU5: Architecture Qualification — implementation supplied
- CU6: Real-GPU Measurement and Decision Evidence

## Candidate family

```text
A = FullActiveDenseRecompute
B = CompactDeltaIndexHistoryReuse
C = AffectedBlockDeltaHistoryReuse
```

CU4 proves per-invocation semantic equivalence. CU5 qualifies the complete fixed timeline and Recovery boundary. Neither unit chooses a winner.

## CU2 commands

```powershell
.\run_sge4_5_spiral7_cu2_prepare.bat
.\run_sge4_5_spiral7_cu2_compact_delta_history.bat
```

## CU3 commands

```powershell
.\run_sge4_5_spiral7_cu3_prepare.bat
.\run_sge4_5_spiral7_cu3_independent_authority.bat
```

## CU4 commands

```powershell
.\run_sge4_5_spiral7_cu4_prepare.bat
.\run_sge4_5_spiral7_cu4_candidate_family.bat
```

## CU5 commands

```powershell
.\run_sge4_5_spiral7_cu5_prepare.bat
.\run_sge4_5_spiral7_cu5_architecture_qualification.bat
```

CU5 builds Debug and Release and requires:

- 128 chained Sparse–Temporal invocations,
- 384 WARP Candidate executions per configuration,
- complete frozen Active/Transition count and transition-kind coverage,
- byte-identical A/B/C output after every invocation,
- forged and stale Runtime handle rejection,
- Controlled Recovery with explicit `A_t/M_t` rebind,
- exact-generation full-active post-Recovery rebuild,
- actual `ID3D12Device5::RemoveDevice` quarantine,
- fresh-process authority rematerialization,
- byte-identical Debug/Release evidence.

## Authority boundary

Runtime and Backend remain forbidden to choose Active membership, modified membership, transition action, Candidate or history policy. Spiral 7 adds no Canonical Level 4 v2 ABI mutation.
