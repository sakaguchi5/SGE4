# Spiral 6 CU4 — Sparse Candidate Family

## Fixed semantic meaning

CU4 keeps one exact sparse set `S` over the fixed universe `{0,...,4095}`. The point corpus, eight Global PGA Motors, output capacity, output order, inactive `float4(0,0,0,0)` sentinel and Direct PGA transform are identical for all Candidates.

Only the physical representation used to recover a Universe index changes.

## Candidate A — DenseMembershipMaskPredicate

- representation: 4096-bit membership mask;
- fixed payload: 512 bytes;
- dispatch: 64 groups, including `K=0`;
- each of the 4096 threads tests one membership bit;
- only members write their original Universe output index.

## Candidate B — CompactIndexListDispatch

- representation: strictly increasing Universe indices followed by `0xFFFFFFFF` tail entries;
- fixed payload: 16384 bytes;
- dispatch: `ceil(K/64)` groups and zero groups for `K=0`;
- the CU2 Compact Index Artifact identity is committed into the CU4 family artifact;
- each compact ordinal loads one original Universe index.

## Candidate C — ActiveBlockListLocalMask

The Universe is partitioned into 64 blocks of 64 records.

- representation: at most 64 records;
- record: `blockIndex`, low 32 mask bits, high 32 mask bits, active-count proof field;
- fixed payload: `64 * 16 = 1024` bytes;
- dispatch: one 64-thread group per active block;
- each thread tests its local bit and writes the corresponding original Universe index.

Unused block records are the canonical all-`0xFFFFFFFF` sentinel record.

## Authority chain

```text
Exact Sparse Set
  -> Raw Sparse Family Candidate
  -> Family Planner
  -> Planner-independent Family Verifier
  -> opaque Verified Sparse Family Plan
  -> candidate-specific Frozen representation artifact
  -> actual representation Resource identity
  -> device-epoch-bound Frozen Candidate
  -> WARP
```

The Family Verifier does not depend on the Family Planner. The execution project accepts opaque Verified and Frozen types and has no Raw-Candidate or Planner entry point.

## Qualification corpus

CU4 preserves the CU2 twelve-case corpus:

- Prefix `K=0`;
- Hash Scatter `K=1`;
- all four patterns at `K=65`;
- all four patterns at `K=1024`;
- Hash Scatter `K=4095`;
- Prefix `K=4096`.

Each case runs all three Candidates on one WARP device. A build therefore performs 36 Candidate executions.

## Required observations

For every Candidate and every case:

- Dispatch count equals the independently Verified Plan;
- active write count equals `K`;
- every active record matches the CPU reference within the frozen numeric tolerance;
- every inactive record remains byte-identical `float4(0,0,0,0)`;
- all active `w` values are bit-identical `1.0f`;
- no non-finite value appears.

The complete 65536-byte Output Buffer must also be byte-identical across A, B and C.

## Boundary retained

CU4 proves that all three physical representations preserve one exact sparse meaning. It does not authorize Runtime selection, declare an architecture winner or generalize any observed result. Full corpus determinism and Recovery remain CU5. Real-GPU performance remains CU6.
