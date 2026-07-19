# Spiral 2 Stage 20 Real GPU Measurement

The canonical Release measurement completed on the hardware adapter
`NVIDIA GeForce RTX 4070 Laptop GPU`. WARP was excluded from all performance
claims.

Measurement contract:

- M0-M5 cover 8, 15, 32, 64, 64, and 128 bones with star, balanced, mixed,
  and chain hierarchy shapes.
- Each case uses two warm-up cycles and eight measured cycles for each of the
  six orders `ABC ACB BAC BCA CAB CBA`, repeated twice.
- Each case therefore contains 96 balanced-order samples; the evidence contains
  576 order samples in total.
- A, B, and C are independently verifier-sealed Frozen Compositions. Each order
  step completes before the next step starts.
- All candidates consume the same canonical Dynamic Motor input bytes for a
  case and use the same D3D12 Direct queue contract and timestamp frequency.
- The evidence records the adapter, driver, timestamp clock, and clock-policy
  fingerprint. The binary reader rejected malformed evidence in its format
  self-test and validated all six canonical cases before report generation.

Canonical command:

```powershell
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass `
  -File .\tests\Run-Spiral2CU6.ps1 -Mode Measurement
```

Generated transient evidence remains under
`build/tests/spiral2/measurement/`. The tracked Stage 21 report records the
validated aggregate ranking and all twelve required experimental answers.

```text
HardwareEvidence = Qualified
MeasurementSamples = 576
CandidateOrderBalance = ABC/ACB/BAC/BCA/CAB/CBA
RecommendationAuthority = NonAuthoritative
```

`NextCapabilitySelection = DeferredByOwner`

`SelectionStatus = OWNER_DECISION_REQUIRED`
