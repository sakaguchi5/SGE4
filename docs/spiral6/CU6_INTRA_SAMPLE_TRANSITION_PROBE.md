# Spiral 6 CU6 Intra-Sample Transition Probe

## Purpose

The Equivalent Set Audit observed a sustained GPU-wide slowdown between two consecutive samples:

```text
ordinal 247: PrefixControl K=1 Candidate C — fast
ordinal 248: PrefixControl K=1 Candidate B — slow from the first GPU phase
```

The Pattern, cardinality, Exact Set, block and Candidate order remained unchanged. This probe locates the earliest measured interval in which the slowdown becomes observable.

This is a diagnostic extension. It does not alter CU6 measurement evidence, the Decision Report, Runtime policy authority, or the Owner decision boundary.

## Fixed control

Every measured sample uses:

```text
Pattern label: PrefixControl
K: 1
Exact Sparse Set: {0}
Candidate sequence: A, C, B, A, C, B, ...
```

The repeated `A/C/B` sequence reproduces the local ordering around the observed `247 -> 248` boundary.

Every sample revalidates:

- Exact Set identity;
- Candidate Artifact identity;
- Representation Resource authority identity;
- Frozen binding identity;
- expected and observed Dispatch;
- exact active write set;
- inactive Float4Zero sentinel;
- CPU reference output.

## CheckpointOnly mode

This mode preserves the original one-main-submit shape. It records absolute CPU time at:

1. sample begin;
2. validation complete;
3. Artifact rebuild complete;
4. Resource creation/upload complete;
5. command recording complete;
6. immediately before main submit;
7. immediately after `ExecuteCommandLists` returns;
8. immediately after `Signal` returns;
9. Fence completion;
10. Readback completion;
11. sample end.

Full D3D12 clock-calibration and DXGI memory telemetry is captured at the key boundaries: sample begin, Resource completion, immediately before main submit, Fence completion, and sample end. Intermediate CPU checkpoints retain elapsed time without adding repeated queue telemetry calls.

The main GPU command is divided into:

1. 64 KiB sentinel copy;
2. Output transition to UAV;
3. argument-producer Dispatch;
4. argument UAV/indirect finalization;
5. Sparse Consumer;
6. Output UAV barrier;
7. Copy Source transitions;
8. 64 KiB Output readback copy;
9. argument readback copy.

## BracketedCalibration mode

This mode adds a fixed 64 KiB GPU calibration command at four boundaries:

```text
pre-resource
post-resource
pre-submit
post-main
```

Each calibration performs the same Upload -> Default and Default -> Readback copy sequence using persistent calibration Resources. If a sustained transition is reproduced, these brackets classify the earliest interval as one of:

```text
BetweenSamples
ValidationOrResourceCreation
CommandRecording
SubmitOrBeforeMainGpu
DuringMainGpu
AfterMainGpu
UnresolvedInsideSample
```

The calibration mode is intentionally diagnostic and more intrusive than the original flow. For this reason the runner also records two minimally intrusive CheckpointOnly fresh processes.

## Fresh-process runner

The canonical runner executes:

```text
Checkpoint A: 768 samples
Bracketed A:  512 samples
Checkpoint B: 768 samples
Bracketed B:  512 samples
```

All runs use Release executables and create a new D3D12 DeviceContext in a fresh process.

A sustained transition is defined by:

```text
baseline = median of first 32 sentinel-path samples
slow threshold = baseline x 2.5
required persistence = 8 consecutive samples
```

## Outputs

```text
build/tests/spiral6-cu6/transition-probe/
  checkpoint-a/
    spiral6_transition_probe_samples_v1.csv
    SPIRAL6_TRANSITION_PROBE_REPORT.md
  bracketed-a/
    ...
  checkpoint-b/
    ...
  bracketed-b/
    ...
  SPIRAL6_TRANSITION_PROBE_COMBINED_REPORT.md
```

The test locates the measured interval where the slowdown begins. It does not assign a hardware or driver event name without clock, P-state, power, temperature or vendor telemetry.
