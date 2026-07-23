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
- CU6: Real-GPU Measurement and Decision Evidence — pending

CU5 accepted exhaustive-audit base commit:

```text
67cb40b5204e1e06ecac576206ba969ec2db02b6
```

## Candidate family

```text
A = FullActiveDenseRecompute
B = CompactDeltaIndexHistoryReuse
C = AffectedBlockDeltaHistoryReuse
```

CU4 proves per-invocation semantic equivalence. CU5 qualifies the complete 128-invocation timeline, Runtime materialization and Recovery boundary. Neither unit chooses a winner.

## CU5 routine command

```powershell
.\run_sge4_5_spiral7_cu5_prepare.bat
.\run_sge4_5_spiral7_cu5_architecture_qualification.bat
```

The routine gate uses Debug for a representative four-invocation A/B/C WARP smoke and Release for the complete 128-invocation authority, Architecture Qualification, Controlled Recovery, actual RemoveDevice and Fresh rematerialization. Release evidence must equal the accepted exhaustive-audit SHA-256 values.

## CU5 exhaustive audit

```powershell
.\run_sge4_5_spiral7_cu5_exhaustive_audit.bat
```

The exhaustive audit repeats the complete Debug and Release paths and verifies byte-identical evidence. It is reserved for Architecture, serializer, toolchain, Recovery, Device-epoch or D3D12 execution changes, or explicit Owner re-audit.

## Authority boundary

Runtime and Backend remain forbidden to choose Active membership, modified membership, transition action, Candidate or history policy. Spiral 7 adds no Canonical Level 4 v2 ABI mutation. Runtime Candidate-policy authorization remains `None`.
