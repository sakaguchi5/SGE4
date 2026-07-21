# Spiral 2 Stage 20 Real GPU Measurement

The final candidate-specific Measurement Evidence V2 run passed on:

```text
Adapter: NVIDIA GeForce RTX 4070 Laptop GPU
Cases: M0-M5
Samples per case: 96
Total balanced-order samples: 576
Order balance: ABC/ACB/BAC/BCA/CAB/CBA
```

M0-M5 cover 8, 15, 32, 64, 64, and 128 bones with star, balanced,
mixed, and chain hierarchy shapes. WARP is excluded from performance claims.

Measurement Evidence V2 records candidate-specific values rather than a
three-candidate aggregate:

- materialization time
- end-to-end time
- actual Package dispatch count
- dynamic invocation bytes
- Package and resource figures
- raw per-Leaf timestamp samples
- numerical Observation Contract V2 metrics

The generated binary evidence was validated for all six cases. Generated
hardware artifacts are transient build outputs and are intentionally not stored
in the source ZIP. They are reproduced by:

```powershell
cmd /c .\run_sge4_5_spiral2_final_qualification.bat -IncludeRealGpu
```

The canonical runner writes them under:

```text
build/tests/spiral2-final-qualification/hardware/
```

```text
HardwareEvidence = Qualified
MeasurementEvidenceSchema = V2
MeasurementSamples = 576
CandidateOrderBalance = ABC/ACB/BAC/BCA/CAB/CBA
RecommendationAuthority = NonAuthoritative
ImplementationStatus = ExperimentComplete
NextCapabilitySelection = DeferredByOwner
SelectionStatus = OWNER_DECISION_REQUIRED
```
