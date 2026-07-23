# Spiral 6 CU6 V2 — block-local paired real-GPU measurement

## Purpose

CU1–CU5 established that A/B/C are authority-controlled, byte-identical physical lowerings of one exact Sparse set `S`.

CU6 does not try to force one absolute GPU clock or compare absolute nanoseconds across the complete 44-case sweep. Its target is the relative map:

```text
for one exact (Pattern,K) set
  measure A, B and C inside the same GPU block
  -> derive A/B, B/C and A/C paired ratios
  -> repeat across K and Pattern
```

The research question is:

> How does the relative advantage of Dense Mask, Compact Index List and Active Block Local Mask change with sparse cardinality and exact set placement?

## Frozen workload

```text
Universe = 4096 records
Pattern  = PrefixControl / UniformStride / BlockClusteredPermutation / HashScatterPermutation
K        = 1,8,32,64,128,256,512,1024,2048,3072,4096
Operation = canonical Direct PGA point transform
Output    = full 4096-record universe-index order
Inactive  = byte-identical float4(0,0,0,0)
```

A/B/C differ only in the CU4/CU5-qualified Sparse representation and dispatch granularity.

## Resource and timing boundary

Before a Pattern/K block is timed, CU6:

- constructs and verifies the exact set,
- constructs all A/B/C authority bundles,
- rematerializes and validates all representation artifacts,
- creates representation, indirect argument, output and readback Resources,
- uploads the canonical representation payloads and dispatch arguments.

The Candidate timestamp contains only repeated `ExecuteIndirect` Sparse Consumer execution plus the required UAV completion barrier.

The following are outside Candidate timestamps:

- Sparse validation,
- artifact construction and upload,
- output initialization,
- Resource creation,
- command recording,
- submission and Fence wait,
- readback and CPU numerical validation.

These excluded phases may still be recorded as descriptive evidence, but do not decide A/B/C.

## Paired block

Every accepted Pattern/K block contains all six Candidate permutations:

```text
ABC  ACB  BAC  BCA  CAB  CBA
```

Every Candidate appears twice in each order position. For every order and measurement cycle, A/B/C form one paired observation because they execute in the same command list, on the same queue, with the same exact set and the same surrounding block state.

Default paired observations per Pattern/K are:

```text
6 orders × 2 cycles × 2 runs = 24 paired A/B/C observations
```

Each Candidate observation is a batch of 512 repeated executions. The timestamp is divided by 512 to obtain descriptive nanoseconds per execution.

## Fixed control

One fixed compute control is measured before and after the complete paired block.

```text
controlRatio = max(controlBefore, controlAfter)
             / min(controlBefore, controlAfter)
```

A block is rejected when `controlRatio > 1.20` by default. Rejection means that the hardware changed materially while A/B/C were being compared.

The complete block is discarded and retried. A rejected block exposes no ranking samples, but its rejection reason remains in Evidence.

## What is not required

CU6 V2 does not require:

- one P-state for the full experiment,
- one Graphics or Memory clock for all K values,
- a P0/P3 performance state,
- rejection of P8 solely because it is P8,
- an NVIDIA GPU,
- NVML availability,
- direct comparison of absolute nanoseconds between two Pattern/K blocks.

NVML telemetry, when available, is diagnostic only. It may explain a capture, but it cannot authorize or reject a result.

## Correctness during measurement

After one block completes, every A/B/C output must still pass:

- observed active write count equals `K`,
- no active write is missing,
- no inactive record is modified,
- every active result matches the independent CPU PGA reference,
- homogeneous `w` is bit-identical `1.0f`,
- no non-finite result exists,
- A/B/C full output digests are byte-identical.

CU5 remains the authority for the complete 84-set WARP corpus, determinism, Recovery and stale-object rejection.
