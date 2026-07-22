# Spiral 4 CU5 — Architecture Complete qualification

Baseline commit: `8577ad1e4bb99f2a1c4d7125c6e586b33fb54154`

CU5 is the final architecture Completion Unit. The runner may declare
`SGE4-5 SPIRAL 4 ARCHITECTURE COMPLETE` only after every gate below passes.

## Full WARP corpus

```text
7 verified Candidates
× 19 Active Count values
= 133 Candidate/case executions
```

Every case requires:

- active output equal to the independent reference,
- no non-finite result or first mismatch,
- inactive-tail sentinel preserved,
- non-finite and homogeneous-coordinate counts remain zero,
- maximum absolute, relative, and ULP errors are recorded,
- duplicate, missing, reordered, and out-of-range Work counts remain zero,
- identical full output digest across A/B/C for the same `Nf`,
- Batched GPU argument records equal to the independently derived partition.

## One native DeviceDomain

CU5 adds native-device overloads to the CU2 Single Indirect and CU4 Candidate
family executors. The persistent CU5 runtime creates one WARP `ID3D12Device` and
runs A, B, and all C candidates on that same device and epoch.

The old `OnWarp` entry points remain as compatibility wrappers. They create a
WARP device and delegate to the native-device entry points.

## Determinism

The runner creates Architecture evidence in three fresh processes:

```text
Debug A
Debug B
Release
```

The files must be byte-identical. Shader bytecode digests are deliberately not
part of canonical Architecture evidence because Debug and Release DXBC may
differ. Candidate, Plan, Frozen artifact, partition, output, and runtime
authority bytes must not differ.

## Recovery

Controlled Recovery rematerializes the native Device and increments the
device epoch. Command Signatures, argument Buffers, endpoint state, and readback
state are rematerialized by the first explicit post-rebind submission. Frozen Candidate identities remain unchanged. Dynamic inputs are
unbound and require an explicit current-epoch rebind.

Actual Recovery calls `ID3D12Device5::RemoveDevice` on the same device used by
the Candidate family. The old process enters `AwaitingAdapter`, excludes the
removed WARP LUID, releases all device objects, and rejects handles, submission
tokens, and new submissions. Rematerialization is qualified in a fresh process.

## Completion boundary

CU5 does not measure performance and does not select a winner. Successful CU5
execution closes Architecture Complete only. CU6 remains responsible for
real-GPU measurement, crossover classification, Batch-size evidence, and the
Owner-only next-capability decision.
