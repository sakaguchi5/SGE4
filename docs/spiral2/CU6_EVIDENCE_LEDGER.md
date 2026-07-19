# Spiral 2 CU6 prototype evidence ledger

Stages: S2-19, S2-20, S2-21, S2-22A

- CU1-CU5 executable evidence is mapped in
  `STAGE19_ARCHITECTURE_FINAL_FREEZE.md` as preserved prototype evidence.
- The real-GPU procedure and balanced sample contract are recorded in
  `STAGE20_REAL_GPU_MEASUREMENT.md`.
- Optional Backend timestamp profiling is disabled by default and observes one
  already-authorized Compute dispatch per Frozen Leaf. It does not alter Frozen
  bytes or Runtime decisions.
- A preliminary declaration-order approach failed because the general Level 4
  DAG scheduler legally interleaved independent A/B/C branches. Runtime
  scheduling inference and test weakening were rejected.
- The corrected prototype measurement executes A/B/C as independent
  verifier-sealed Frozen Compositions, explicitly completes each candidate step,
  and repeats all six orders evenly.
- Release real-GPU measurement ran on NVIDIA GeForce RTX 4070 Laptop GPU for
  M0-M5, with 96 balanced-order samples per case and 576 order samples total.
- Binary evidence round-trip passed and preserved the raw per-Leaf timestamps,
  command-recording observations, candidate ranking, and Owner-deferred fields.
- Aggregate prototype ranking: B 49,152 ns; A 140,288 ns; C 143,360 ns.
- Candidate B and Vertex Consumer recommendations remain non-authoritative.
- Architecture review found that final interpretation still requires correction:
  candidate-specific end-to-end and materialization fields, actual dispatch
  accounting, verified-graph-to-Package authority, and complete Observation
  metrics are not closed.
- The first repository-wide Debug Freeze attempt on 2026-07-20 failed in
  `41_SliceExecutionTests.exe` at Slice 15 Classical controlled reconstruction
  with `DXGI_ERROR_DEVICE_REMOVED` while mapping an upload buffer. The preserved
  runner log is `docs/test-logs/20260720-023344-Freeze-Debug.log`. The narrowest
  rerun immediately passed all 18 inputs. No Runtime inference, semantic change,
  retry loop, or test weakening was introduced.
- The final preserved successful repository-wide evidence is
  `docs/test-logs/20260720-024828-Freeze-Debug.log`, followed by
  `docs/test-logs/20260720-024829-Architecture-Debug.log`.

```text
ImplementationStatus = PrototypePaused
PrototypeEvidenceStatus = Preserved
ArchitectureReviewStatus = FindingsOpen
IndependentSoftwareVerifier = Required
RecommendationAuthority = NonAuthoritative
NextCapabilitySelection = DeferredByOwner
SelectionStatus = OWNER_DECISION_REQUIRED
```
