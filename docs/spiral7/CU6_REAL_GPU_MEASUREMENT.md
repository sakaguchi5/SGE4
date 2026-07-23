# CU6 Real-GPU Measurement — CU6-2

CU6-2 preserves the completed CU5 Architecture and measures the verified A/B/C Candidate family on a real D3D12 hardware Adapter.

The measurement now has two passes: the 160-case canonical surface and a 100-case high-Transition refinement. Both retain the six balanced A/B/C orders, D3D12 Timestamp Query, fixed Candidate-A block controls, correctness-before-timing, adaptive nonzero-dispatch iteration expansion, and zero-dispatch timestamp censoring introduced by CU6-1/FIX02.

The raw Evidence schema is `S7M3`. It records the measurement pass in the config and binds the pass-specific Transition schedule into both profile and schedule identities.

Canonical output:

```text
canonical_measurement_evidence_s7m3.bin
canonical_decision_cases_v3.csv
canonical_decision_report_v3.txt
```

Refinement output:

```text
high_transition_refinement_evidence_s7m3.bin
high_transition_refinement_cases_v3.csv
high_transition_refinement_report_v3.txt
```

Combined output:

```text
combined_paired_decision_cases_v3.csv
combined_paired_decision_report_v3.txt
```
