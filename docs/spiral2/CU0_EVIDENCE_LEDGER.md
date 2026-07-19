# Spiral 2 CU0 evidence ledger

Stages: S2-00, S2-01, S2-02

- Baseline ancestor check: passed
- Starting tree: clean before manifest repair
- Owner selection record: `FixedDynamicMotorPaletteAndHierarchy`
- Completion specification, non-goals, observation, corpus, project boundaries,
  stage map, proof ledger, and baseline record: present
- Foundation: passed
- Gate: passed
- Strongest baseline Freeze: passed through Foundation final integration chain
- Schema/Runtime: 17/17 unchanged
- Existing Level 4 and Spiral 1 expected digests: unchanged
- PowerShell failure root cause: unavailable `Get-FileHash` auto-load in nested runner
- Correction: mandatory .NET SHA-256 calculation, with manifest equality preserved
- First CU0 Freeze attempt: preserved failure at `Slice 15 PGA` controlled rebuild with
  `DXGI_ERROR_DEVICE_REMOVED` after repeated WARP Freeze runs
- Narrow reproduction: the same Debug `41_SliceExecutionTests` execution passed all
  18 cases, including `Slice 15 PGA` controlled reconstruction
- Required CU0 rerun: Foundation Freeze and Gate passed without source repair; the
  event is classified as the already-recorded intermittent WARP removal condition

```text
IndependentReview = NotRequiredByOwner
SelfAudit = Passed
IndependentSoftwareVerifier = Required
NextCapabilitySelection = DeferredByOwner
SelectionStatus = OWNER_DECISION_REQUIRED
```
