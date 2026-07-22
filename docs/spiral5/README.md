# Spiral 5 — Temporal State Flow

Spiral 5 studies exact temporal history reuse for one fixed rigid-transform workload. The V1 meaning is piecewise constant: an external verified schedule states Update and Hold invocations, and Candidate Lowerings differ only in where exact derived history is retained.

## Current completion unit

```text
CU3 — Independent Temporal Authority
```

CU3 adds:

```text
Raw Temporal Candidate
-> Planner proposal
-> Planner-independent Verifier
-> opaque Verified Temporal Plan
-> actual CU2 artifact and History Resource binding
-> device-epoch-bound verified WARP execution
```

## Apply CU3

```powershell
.\run_sge4_5_spiral5_cu3_prepare.bat
.\run_sge4_5_spiral5_cu3_independent_authority.bat
```

The prepare step registers four projects and regenerates the Source Manifest. The authority runner builds fresh Debug and Release processes, rejects the mutation and replay matrix, executes verified P4 on WARP, and requires byte-identical authority evidence.

## Candidate sequence

```text
CU2: B.GlobalMotorHistoryReuse architecture
CU3: independent B authority
CU4: A/B/C Candidate family
CU5: all schedules, determinism, Recovery
CU6: real-GPU measurement and Decision Report
```
