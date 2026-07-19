# Spiral 2 foundation baseline v1

- Required ancestor: `afaed65d9400f3f80a7f5a9094edd9029082f95d`
- Starting HEAD: `7b04cc7`
- Solution: `SemanticGpuEngine4-5.sln`
- Starting projects: 76
- Target ABI: D3D12 Schema 17 / Runtime 17
- Level 4: canonical v1, static Buffer DAG, one DeviceDomain, whole recovery
- Spiral 1: Architecture complete; next selection historically deferred

Baseline commands executed on 2026-07-19:

```text
cmd /c .\run_sge4_5_foundation.bat = 0
cmd /c .\run_sge4_5_gate.bat = 0
```

The Foundation command included the Level 4 v1 Final Integration Freeze and
Stage 0D Debug/Release/WARP/recovery qualification. The canonical 54-Package
semantic digest remained `43322a7c33d2ae82bfa577c0fee79ad7b915bdd8d38655eac1631a4e81c89d9b`.

An environment failure showed nested Windows PowerShell did not auto-load
`Get-FileHash`. The test infrastructure was repaired with a .NET SHA-256 helper;
no expected digest, test assertion, or source-inventory comparison was weakened.

