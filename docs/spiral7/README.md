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
- CU4: Incremental History Candidate Family — implementation supplied
- CU5: Architecture Qualification
- CU6: Real-GPU Measurement and Decision Evidence

## CU2 commands

After extracting the CU2 overlay into the checkout where CU1 passed, register the four projects and regenerate the Source Manifest:

```powershell
.\run_sge4_5_spiral7_cu2_prepare.bat
```

Then execute the CU2 gate:

```powershell
.\run_sge4_5_spiral7_cu2_compact_delta_history.bat
```

The gate builds Debug and Release, runs fourteen WARP transition cases and requires byte-identical evidence.

## CU2 architecture

```text
previous history bytes
    + exact A_t
    + exact M_t
    -> independently derived W_t / H_t / R_t / T_t
    -> fixed-capacity sorted uint2 transition records
    -> ExecuteIndirect ceil(|T_t|/64)
    -> Update W_t / Clear R_t / do not write H_t
```

## Authority boundary

Runtime and Backend remain forbidden to choose Active membership, modified membership, transition action, Candidate or history policy. CU2 adds no Canonical Level 4 v2 ABI.

## CU3 commands

```powershell
.\run_sge4_5_spiral7_cu3_prepare.bat
.\run_sge4_5_spiral7_cu3_independent_authority.bat
```

CU3 builds Debug and Release, rejects 50 Raw proposal mutations plus Verified/resource/artifact/epoch replay attempts, executes one verified WARP transition, and requires byte-identical evidence.

## CU4 commands

```powershell
.\run_sge4_5_spiral7_cu4_prepare.bat
.\run_sge4_5_spiral7_cu4_candidate_family.bat
```

CU4 executes A/B/C over one chained eighteen-invocation WARP timeline, requires 54 Candidate executions and pairwise byte-identical full outputs, and does not select a winner.
