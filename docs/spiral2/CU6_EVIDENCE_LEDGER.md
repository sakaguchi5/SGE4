# Spiral 2 CU6 evidence ledger

Stages: S2-19, S2-20, S2-21, S2-22A

- S2-I01-I22 are mapped to executable CU1-CU5 evidence in
  `STAGE19_ARCHITECTURE_FINAL_FREEZE.md`.
- The canonical real-GPU procedure and balanced sample contract are recorded in
  `STAGE20_REAL_GPU_MEASUREMENT.md`.
- Optional Backend timestamp profiling is disabled by default and observes one
  already-authorized Compute dispatch per Frozen Leaf. It does not alter Frozen
  bytes or Runtime decisions.
- A preliminary declaration-order approach failed because the general Level 4
  DAG scheduler legally interleaved independent A/B/C branches. The failure was
  retained as design evidence. Runtime scheduling inference and test weakening
  were rejected.
- The correction measures A/B/C as independent verifier-sealed Frozen
  Compositions, explicitly completes each candidate step, and repeats all six
  orders evenly. The Debug M0-M5 smoke passed before the Release run.
- Release real-GPU measurement passed on NVIDIA GeForce RTX 4070 Laptop GPU:
  M0-M5, 96 balanced-order samples per case, 576 order samples total.
- Binary evidence round-trip passed. The report contains all required metrics,
  all twelve required answers, raw per-Leaf timestamp rows, candidate ranking,
  and the Owner-deferred selection fields.
- Aggregate median ranking: B 49,152 ns; A 140,288 ns; C 143,360 ns.
- Candidate B and Vertex Consumer recommendations are explicitly
  non-authoritative.
- The first repository-wide Debug Freeze attempt on 2026-07-20 failed in
  `41_SliceExecutionTests.exe` at Slice 15 Classical controlled reconstruction
  with `DXGI_ERROR_DEVICE_REMOVED` while mapping an upload buffer. The failing
  command was `cmd /c .\tests\run_freeze.bat`; its preserved runner log is
  `docs/test-logs/20260720-023344-Freeze-Debug.log`. No MSBuild worker or other
  D3D12 test process remained after the failure. The narrowest rerun,
  `build\bin\x64\Debug\41_SliceExecutionTests.exe`, immediately passed all 18
  inputs including Slice 15 Classical/SDF/PGA and controlled reconstruction.
  This was retained as transient WARP device-removal evidence; no Runtime
  inference, semantic change, retry loop, or test weakening was introduced.

```text
IndependentReview = NotRequiredByOwner
SelfAudit = Passed
IndependentSoftwareVerifier = Required
RecommendationAuthority = NonAuthoritative
NextCapabilitySelection = DeferredByOwner
SelectionStatus = OWNER_DECISION_REQUIRED
```
