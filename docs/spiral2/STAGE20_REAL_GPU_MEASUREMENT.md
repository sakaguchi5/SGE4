# Spiral 2 Stage 20 Real GPU Prototype Measurement

The Release prototype measurement completed on the hardware adapter
`NVIDIA GeForce RTX 4070 Laptop GPU`. WARP was excluded from performance
claims.

Measurement procedure preserved:

- M0-M5 cover 8, 15, 32, 64, 64, and 128 bones with star, balanced, mixed,
  and chain hierarchy shapes.
- Each case uses two warm-up cycles and eight measured cycles for each of the
  six orders `ABC ACB BAC BCA CAB CBA`, repeated twice.
- Each case contains 96 balanced-order samples; the evidence contains 576 order
  samples in total.
- A, B, and C run as separate verifier-sealed Frozen Compositions, and each order
  step completes before the next starts.
- All candidates consume the same canonical Dynamic Motor input bytes for a
  case and use the same D3D12 Direct queue contract and timestamp frequency.
- The binary evidence records adapter and clock-profile information and passed
  its format round-trip self-test.

Canonical command:

```powershell
powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass `
  -File .\tests\Run-Spiral2CU6.ps1 -Mode Measurement
```

Generated transient evidence remains under
`build/tests/spiral2/measurement/`. The tracked Stage 21 document preserves the
prototype ranking. Final Decision Evidence requires correction of the open
measurement-interpretation and Architecture findings documented in the CU6
ledger.

```text
HardwarePrototypeEvidence = Preserved
MeasurementSamples = 576
CandidateOrderBalance = ABC/ACB/BAC/BCA/CAB/CBA
InterpretationReview = Required
RecommendationAuthority = NonAuthoritative
ImplementationStatus = PrototypePaused
```

`NextCapabilitySelection = DeferredByOwner`

`SelectionStatus = OWNER_DECISION_REQUIRED`
