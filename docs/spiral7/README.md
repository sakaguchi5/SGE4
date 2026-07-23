# SGE4-5 Spiral 7

## Capability

```text
Versioned Sparse Delta Flow and Verified Incremental History Lowering
```

Spiral 7 is the final exploratory Spiral before Level 4 v2 Canonical reconstruction.

It combines three already verified dimensions:

```text
Spiral 4: verified indirect execution quantity
Spiral 5: verified temporal history
Spiral 6: verified exact sparse membership
```

into one authority problem:

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

- CU1: Research Contract Freeze
- CU2: Sparse Temporal Delta Architecture
- CU3: Independent Delta Authority
- CU4: Incremental History Candidate Family
- CU5: Architecture Qualification
- CU6: Real-GPU Measurement and Decision Evidence

## CU1 command

After extracting the overlay into a clean checkout at the baseline commit, run:

```powershell
.\run_sge4_5_spiral7_cu1_prepare.bat
```

The prepare runner regenerates `SOURCE_MANIFEST.sha256` and then executes the CU1 gate.

Direct rerun after the manifest is current:

```powershell
.\run_sge4_5_spiral7_cu1_research_contract.bat
```

## Authority boundary

CU1 adds no C++ and changes no ABI. Runtime and Backend remain forbidden to choose Active membership, modified membership, transition action, Candidate or history policy.
