# Spiral 6 CU6 Equivalent Set Audit

## Purpose

This diagnostic does not re-measure the complete Spiral 6 corpus. It isolates only the naturally equivalent Exact Sparse Set controls that exposed the CU6 discrepancy.

```text
K=1:
  PrefixControl = {0}
  UniformStride = {0}

K=4096:
  PrefixControl
  UniformStride
  BlockClusteredPermutation
  HashScatterPermutation
  = {0,1,...,4095}
```

For one Candidate, equivalent labels must therefore have identical:

- Exact Set identity;
- Frozen Candidate Artifact identity and bytes;
- Representation Resource authority identity;
- active count and active-block count;
- Dispatch argument;
- output digest.

The audit fails immediately if any of these structural identities differ.

## Schedules

### Balanced Interleaved

`K=1` uses both label orders:

```text
Prefix, Uniform
Uniform, Prefix
```

`K=4096` uses a four-row Latin square:

```text
Prefix, Uniform, Block, Hash
Uniform, Hash, Prefix, Block
Block, Prefix, Hash, Uniform
Hash, Block, Uniform, Prefix
```

The two cardinality controls alternate their execution position. Inside every label occurrence, all six Candidate permutations are used. One balanced run contains 432 measured Candidate samples. Every equivalent label has 24 samples per Candidate, and every Candidate appears eight times in each Candidate order position.

### Fixed Pattern-major

The control labels are grouped in this order:

```text
K=1 Prefix
K=1 Uniform
K=4096 Prefix
K=4096 Uniform
K=4096 Block
K=4096 Hash
```

Four rounds produce the same 24 samples per label and Candidate, for 432 samples total. Candidate order remains six-order balanced, while Pattern label order intentionally reproduces the confounding shape of CU6.

## Fresh-process protocol

The runner launches three separate Release processes:

```text
Balanced A
Fixed Pattern-major
Balanced B
```

The original CU6 CSV is retained as the baseline. The combined analyzer compares all four data sources.

## Retained telemetry

Each audit sample records:

- all CU6 GPU phase timestamps;
- CPU submit/wait and end-to-end time;
- D3D12 timestamp frequency before and after the sample;
- `ID3D12CommandQueue::GetClockCalibration` before and after the sample;
- DXGI local and non-local memory budget and current usage;
- elapsed audit-process time;
- Set, Artifact, Resource authority, Frozen binding and output identities.

The analyzer reports equivalent-label median spread, time-quartile medians and the largest sustained rolling-window step in the fixed sentinel-copy phase.

## Interpretation boundary

This is a measurement audit. It does not authorize Runtime Candidate selection, does not alter CU1-CU5 Architecture evidence and does not close Spiral 6. Timing differences are reported rather than used as a pass/fail completion gate. Structural identity violations remain hard failures.
