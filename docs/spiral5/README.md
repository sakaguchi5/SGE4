# Spiral 5 — Temporal State Flow

Spiral 5 studies exact temporal history reuse for one fixed rigid-transform workload.

The V1 meaning is piecewise constant. An external verified schedule states Update and Hold invocations. Candidate Lowerings differ only in where derived history is retained.

## Current completion unit

```text
CU2 — Global Motor History Architecture
```

CU2 adds one Candidate:

```text
B.GlobalMotorHistoryReuse
```

It uses a versioned Temporal sidecar bound to the existing Spiral 4 Dispatch-only indirect artifact. Schema 17, Runtime 17, and the canonical Backend remain unchanged.

## Apply CU2

```powershell
.\run_sge4_5_spiral5_cu2_prepare.bat
.\run_sge4_5_spiral5_cu2_global_motor_history.bat
```

The prepare step registers four projects and regenerates the Source Manifest. The architecture runner builds Debug and Release, executes P1/P4/P64/Irregular on WARP, and requires byte-identical evidence.

## Candidate sequence

```text
CU2: B.GlobalMotorHistoryReuse
CU3: independent Temporal authority
CU4: A/B/C Candidate family
CU5: all schedules, determinism, Recovery
CU6: real-GPU measurement and Decision Report
```
