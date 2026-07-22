# Spiral 6 CU6 — real-GPU Sparse measurement

CU6 measures the three CU4/CU5-qualified Sparse Candidates on one explicitly selected hardware D3D12 adapter.

## Independent variables

```text
Pattern = PrefixControl / UniformStride / BlockClusteredPermutation / HashScatterPermutation
K = 1,8,32,64,128,256,512,1024,2048,3072,4096
```

The exact Sparse set is constructed and verified before Candidate execution. The 4096-record universe, Direct PGA operation, full-size output order and inactive `float4(0,0,0,0)` sentinel remain Frozen.

## Candidate variable

```text
A.DenseMembershipMaskPredicate
B.CompactIndexListDispatch
C.ActiveBlockListLocalMask
```

The Candidates differ only in the already-Verified derived Sparse representation and Dispatch granularity.

## Balanced order

CU6 uses all six permutations:

```text
ABC ACB BAC BCA CAB CBA
```

Each Candidate appears twice in each order position. The default produces 24 measured samples per Candidate and `(Pattern,K)`:

```text
6 orders × 2 measured cycles × 2 runs = 24
```

The complete default measurement contains:

```text
4 patterns × 11 K values × 3 Candidates × 24 samples = 3168 measured Candidate samples
```

## GPU timestamp decomposition

Six timestamps define five intervals:

1. output sentinel initialization,
2. verified indirect-argument generation,
3. Sparse Consumer execution,
4. output/argument completion and transition,
5. output and argument observation copy.

The primary metric excludes observation copy:

```text
GpuCandidatePathNanoseconds
  = GpuSentinelInitializationNanoseconds
  + GpuIndirectArgumentGenerationNanoseconds
  + GpuSparseConsumerExecutionNanoseconds
  + GpuCompletionNanoseconds
```

Sparse input validation, Candidate representation construction/upload, command recording, submit/fence wait, numeric observation and CPU end-to-end time remain separate evidence.

## Hardware eligibility

Adapters are enumerated through `DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE`. Software adapters are rejected. `adapterIndex` selects among eligible D3D12 hardware adapters.

Evidence records description, PCI IDs, LUID, driver version, UMA properties, timestamp frequency and an adapter fingerprint.

## Correctness during measurement

Every measured sample verifies:

- generated indirect Dispatch X equals the Verified Candidate authority,
- active write count equals `K`,
- no active write is missing,
- no inactive record is modified,
- active output matches the independent CPU PGA reference,
- homogeneous `w` is exactly `1.0f`,
- no non-finite value occurs,
- all repeated A/B/C outputs for one exact set have one byte-identical digest.

CU5 remains the authority for the complete `K=0` corpus, full WARP qualification, Recovery, stale-handle rejection and fresh-process rematerialization. CU6 binds those evidence identities and does not weaken them.
