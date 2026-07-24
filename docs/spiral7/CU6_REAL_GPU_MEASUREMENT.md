# CU6 Real-GPU Measurement — CU6-2

CU6-2 preserves the completed CU5 Architecture and measures the verified A/B/C Candidate family on a real D3D12 hardware Adapter.

The accepted Owner run used:

```text
Adapter              NVIDIA GeForce RTX 4070 Laptop GPU
Adapter fingerprint  8a7a825e84c0a157b16c6e54f51b833d6d191173e7e1d28d84062a60840a6b3f
```

The measurement has two passes: the 160-case canonical surface and a 100-case high-Transition refinement. Both retain the six balanced A/B/C orders, D3D12 Timestamp Query, fixed Candidate-A block controls, correctness-before-timing, adaptive nonzero-dispatch iteration expansion and zero-dispatch timestamp censoring.

The raw Evidence schema is `S7M3`. It records the measurement pass in the config and binds the pass-specific Transition schedule into both profile and schedule identities.

## Accepted outputs

Canonical:

```text
canonical_measurement_evidence_s7m3.bin  0d7a50c448269cbb346ea80b0ceda09e7f28b10c068ed20a96d6c47621b4802a
canonical_decision_cases_v3.csv          49bc2fff4e0805f243170e992695d327cc09649d53b50b0287fd1a5feae87b37
canonical_decision_report_v3.txt         82a498d44e8564c38d6346e412b370874a4d3ad41b9590945b360924f901129c
```

Refinement:

```text
high_transition_refinement_evidence_s7m3.bin  44fa4dd72e64aab300bec53fc45c6ae2bf7a7012630b0e15a83fe6717c7ff80b
high_transition_refinement_cases_v3.csv       239cd013558553206729045fe6020c521137f2e51e595e51a2a172d2b065a388
high_transition_refinement_report_v3.txt      6e68bd9fd48e5f3684e126cdb5d057bb0bf65b8527dc8552b38a6453568ad745
```

Combined:

```text
combined_paired_decision_cases_v3.csv   e07af74f1f5f53143adbaf5e6fbd65d54d83bc4a0d314c6e7d648bf4edcd7bd4
combined_paired_decision_report_v3.txt  b840cb1425f5e45ccac2f0c84750584313350458062dab98f8552290e8bfda81
```

The complete generated bundle is stored outside Git with SHA-256 `9ee852c2a2574a8cb9c163b6e94213b855317bba74319e6443e9aa98c10c24d9`.

The canonical pass accepted 320 blocks with no rejection. The refinement pass accepted 300 blocks; one drifted attempt was rejected and excluded. All 190 timestamp-resolution-censored samples belonged to legal zero-dispatch cases, and no nonzero-dispatch sample was censored.
