# Spiral 6 CU6 NVIDIA Performance-State Telemetry

## Purpose

This diagnostic extension names the GPU or driver performance-state transition observed by the Intra-Sample Transition Probe. It does not alter CU6 Candidate semantics, authority, output qualification, or Runtime policy authorization.

## Fixed control

- Pattern: `PrefixControl`
- K: `1`
- Exact Set: `{0}`
- Candidate sequence: `A.Dense / C.ActiveBlock / B.Compact` repeated
- Existing D3D12 checkpoint and ten-timestamp main command retained

## In-process NVML

`nvml.dll` is loaded at runtime from Windows System32 or the NVIDIA NVSMI directory. No CUDA Toolkit or NVML import library is required.

The probe records:

- Graphics, SM, memory and video clocks
- P-state
- GPU temperature
- Instantaneous power usage and current power limit
- GPU and memory utilization
- Active clock-event reason bitmask
- Per-query NVML result code

Snapshots are taken at the important sample checkpoints and by a background polling thread. The timeline uses QPC-compatible steady elapsed nanoseconds and Unix nanoseconds.

GeForce support is query-specific. `NVML_ERROR_NOT_SUPPORTED` is retained; missing values are never invented.

## Independent nvidia-smi sidecar

The PowerShell runner first executes an in-process-NVML-only pair, then a second pair with one long-lived `nvidia-smi --loop-ms` sidecar. This makes any observer effect from the sidecar visible. The sidecar records average/instantaneous power and clock-event cumulative counters when the installed driver exposes them.

## Stable Power State diagnostic

`-RunStablePowerState` adds fresh diagnostic runs using `ID3D12Device::SetStablePowerState(TRUE)`. This may require Windows Developer Mode. Stable-state timings are diagnostic causal controls and are excluded from Spiral 6 Candidate ranking evidence.

## Outputs per run

- `spiral6_transition_probe_samples_v1.csv`
- `spiral6_transition_probe_nvml_timeline_v1.csv`
- `nvidia_smi_sidecar.csv` when available
- `nvidia_smi_sidecar_fields.txt`
- `SPIRAL6_TRANSITION_PROBE_REPORT.md`

The combined report summarizes P-state ranges, clock ranges, active event reasons and the diagnosis at the first sustained D3D12 slowdown.
