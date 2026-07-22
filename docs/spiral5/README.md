# Spiral 5 — Temporal State Flow

Spiral 5 studies exact temporal history reuse for one fixed rigid-transform workload.

The V1 meaning is piecewise constant. An external verified schedule states update and hold invocations. Candidate Lowerings differ only in where derived history is retained.

## CU1

CU1 contains documents and verification only. It does not add C++ projects or mutate ABI.

After copying the CU1 files, regenerate the Source Manifest:

```powershell
powershell.exe -NoLogo -NoProfile -NonInteractive `
  -ExecutionPolicy Bypass `
  -File .\tests\tools\Update-SourceManifest.ps1
```

Then run:

```powershell
.\run_sge4_5_spiral5_cu1_research_contract.bat
```

## Candidate summary

```text
A.EveryInvocationRecompute
B.GlobalMotorHistoryReuse
C.FinalOutputHistoryReuse
```

## Completion sequence

```text
CU1 contract
CU2 one temporal history architecture
CU3 independent authority
CU4 Candidate family
CU5 architecture qualification and Recovery
CU6 real-GPU measurement and Decision Report
```
