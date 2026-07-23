# Spiral 6 CU6 V2 changeset

Baseline:

```text
18cc16db82c5cf7b5d7be00d51345196df046447
```

## Added projects

```text
170_Spiral6PerformanceExperiment
171_Spiral6PerformanceTests
```

## Added capability

- hardware D3D12 execution of the CU5-qualified A/B/C family,
- prebuilt Pattern/K Resources,
- repeated batched Candidate execution,
- all-six-order position balance,
- block-local A/B/C paired observations,
- fixed-control within-block stationarity gate,
- complete-block rejection and retry,
- exact write-set and full-output validation after measurement,
- binary Evidence, CSVs, manifest and Relative Decision Map,
- optional diagnostic-only NVML capture,
- no Runtime policy authorization.

## Deliberately excluded

- full-run absolute clock homogeneity,
- mandatory P-state or clock range,
- mandatory NVIDIA telemetry,
- cross-block absolute-nanosecond ranking,
- workload conditioning intended to force a driver power state.
