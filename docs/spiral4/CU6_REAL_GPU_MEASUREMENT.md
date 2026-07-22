# Spiral 4 CU6 — real GPU measurement

Baseline commit: `302e31569c2cec5d69fbc53f3ddb363cd0af777c`

## Purpose

CU1–CU5 established the meaning, Candidate authority, WARP equivalence,
determinism, and Recovery of variable Active Work Count. CU6 measures the
already-qualified Candidate family on one selected non-software D3D12 adapter.

CU6 does not modify the Candidate space and does not authorize Runtime adaptive
selection.

## Measured Candidate corpus

```text
A.FixedMaximumGuarded
B.SingleExecuteIndirectDispatch
C.BatchedExecuteIndirect.B64
C.BatchedExecuteIndirect.B128
C.BatchedExecuteIndirect.B256
C.BatchedExecuteIndirect.B512
C.BatchedExecuteIndirect.B1024
```

All Candidates use the same:

- Canonical Active Work Semantic,
- Nmax = 4096,
- nineteen-value Active Count corpus,
- Direct PGA Consumer meaning,
- input records and Motor values,
- output order and Observation Contract,
- Verified Plan and Frozen artifact identities produced by CU4/CU5.

## Balanced order design

Seven Candidates require more than a simple ABBA order. CU6 uses fourteen
orders:

```text
7 cyclic rotations of A,B,C64,C128,C256,C512,C1024
+
7 cyclic rotations of the reverse sequence
```

Every Candidate appears exactly twice at each of the seven order positions.
The order-balance table is self-tested and its canonical identity is embedded
in the binary evidence.

The default measurement profile is:

```text
warmup cycles per order       = 1
measurement cycles per order  = 2
runs                           = 2
orders                         = 14
samples per Candidate and Nf   = 56
total measured samples         = 7 × 19 × 56 = 7448
```

## Hardware selection

The requested adapter index is resolved through
`DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE`. Software adapters are rejected.

The evidence records:

- adapter description,
- vendor/device/subsystem/revision,
- adapter LUID,
- driver version,
- UMA and cache-coherent UMA properties,
- compute-queue timestamp frequency,
- adapter fingerprint.

CU6 fails instead of silently falling back to WARP when no compatible hardware
adapter exists.

## Timestamp boundaries

D3D12 timestamp queries divide one Candidate invocation into:

```text
q0 -> q1  GPU argument generation
q1 -> q2  UAV ordering and INDIRECT_ARGUMENT transition
q2 -> q3  direct Dispatch or ExecuteIndirect Consumer execution
q3 -> q4  output UAV ordering and output transition
q4 -> q5  output/argument readback copies
```

The primary metric is:

```text
GpuCandidatePathNanoseconds = q0 -> q4
```

It therefore includes the physical Candidate path while excluding deterministic
sentinel reset and readback-copy cost.

CU6 separately records:

- active-input CPU preparation,
- command recording,
- submit and fence wait,
- numeric observation,
- CPU end-to-end,
- every GPU timestamp component,
- GPU total including observation copy.

## Observation remains mandatory

Every timed sample is rejected unless:

- active output matches the independent reference,
- inactive-tail sentinel is unchanged,
- no non-finite or homogeneous-coordinate violation occurs,
- no duplicate, missing, reordered, or out-of-range Work is observed,
- Single/Batched argument records match the verified formula,
- output digest is produced.

Warmup invocations use the same validation. Warmup results are not serialized.

## Output files

```text
spiral4_measurement_evidence_v1.bin
spiral4_measurement_samples_v1.csv
spiral4_measurement_summary_v1.json
SPIRAL4_DECISION_EVIDENCE_REPORT.md
```

The binary file is authoritative. It carries a trailing SHA-256 checksum and is
subject to strict parser limits and corruption rejection.
