# Spiral 2 CU5 evidence ledger

Stages: S2-15, S2-16, S2-17, S2-18

- H00-H08 × F00-F11 passed on WARP in both Debug and Release: 216 executions.
- Each topology loaded one Frozen Composition and submitted all twelve palettes
  consecutively without recompilation or device-epoch change.
- Frozen bytes remained unchanged, F00 and F04 produced distinct outputs, and
  reversed frame order reproduced the matching per-invocation output exactly.
- Every result passed independent CPU-reference, A/B/C pairwise, non-finite,
  homogeneous-coordinate, orientation, axis-length, and orthogonality checks.
- The complete semantic, corpus, raw graph, verified representation, Leaf,
  Composition, and observation evidence bundle was byte-identical across two
  Debug fresh processes and one Release fresh process.
- Controlled whole-composition recovery rematerialized ten Leaves and eleven
  flows, incremented the epoch, rejected stale Buffer/token handles, rejected a
  submission without explicit dynamic rebind, and reproduced F04 exactly.
- Actual RemoveDevice quarantined the removed WARP LUID in AwaitingAdapter,
  rejected stale handles/submission, and a fresh process reproduced F07 exactly.
- Controlled recovery output hash:
  `B2D1D943CCFA1FF174C582265F268ADC163F9130EF1A03A113BCE60B3C3F0E9C`.
- Actual-removal/fresh-process F07 hash:
  `927C73623F1ADA234754BE6C1463B977CAFCED9B01EA45902FD114FB32065ABE`.
- Freeze bundle hash, identical for Debug A, Debug B, and Release:
  `0FAF67FB31B5A6114F50BC260C705117ACA4DCCE69A50BC311DB7B113F9E055A`.
- Dynamic invocation objects are value payloads, not epoch handles; submitting
  their bytes after recovery is an explicit new bind. No binding is retained or
  replayed implicitly.
- The historical V1 tolerance identity remains preserved. Final qualification
  uses componentwise `OBSERVATION_CONTRACT_V2.md`, with its fixed constants and
  signed-zero rule justified in `OBSERVATION_TOLERANCE_IMPLEMENTATION_V2.md`.
- Regression and Gate passed. Two later formal foundation Freeze attempts
  preserved transient `DXGI_ERROR_DEVICE_REMOVED` evidence after long WARP
  runs. The cause was 31 orphaned MSBuild worker nodes left by node reuse; the
  workers were stopped and `/nr:false` was added to the active build paths
  before repeating the same formal gate.

```text
IndependentReview = NotRequiredByOwner
SelfAudit = Passed
IndependentSoftwareVerifier = Required
NextCapabilitySelection = DeferredByOwner
SelectionStatus = OWNER_DECISION_REQUIRED
FinalQualificationStatus = Passed
```
