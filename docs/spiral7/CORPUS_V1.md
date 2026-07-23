# Spiral 7 canonical sparse-delta timeline corpus V1

## Fixed universe

```text
WorkUniverseCount = 4096
ThreadsPerGroup = 64
BlockCount = 64
BlockSize = 64
TimelineLength = 128
HistoryDepth = 1
```

Every semantic set is encoded as strictly increasing unique `uint32` universe indices.

## Per-invocation sets

For invocation `t`:

```text
A_t = exact current Active Set
M_t = exact modified-survivor set
N_t = A_t \ A_(t-1)                  activation
R_t = A_(t-1) \ A_t                  deactivation
W_t = N_t union M_t                  update
H_t = (A_t intersect A_(t-1)) \ M_t retain
T_t = W_t union R_t                  physical transition
```

Required relations:

```text
M_t subseteq A_t intersect A_(t-1)
W_t, H_t and R_t are pairwise disjoint
W_t union H_t = A_t
H_t union R_t = A_(t-1) \ M_t
T_t = W_t union R_t
```

At `t=0`, the previous Active Set is empty and history is invalid:

```text
W_0 = A_0
H_0 = empty
R_0 = empty
```

After a Device epoch change:

```text
W_t = A_t
H_t = empty
```

## Active-set pattern constructors

Spiral 7 reuses the four exact Spiral 6 constructors:

- `PrefixControl`
- `UniformStride`
- `BlockClusteredPermutation`
- `HashScatterPermutation`

## Transition constructors

- `Hold`: `A_t=A_(t-1)`, `M_t=empty`.
- `DirtyOnly`: Active membership is unchanged and exactly `D` surviving members are modified.
- `ActivateOnly`: exactly `D` new members enter the Active Set.
- `DeactivateOnly`: exactly `D` previous members leave the Active Set.
- `BalancedChurn`: remove `D` members and add `D` different members while cardinality remains fixed.
- `PatternMigration`: preserve cardinality while moving between exact pattern constructors.
- `FullInvalidation`: `M_t=A_t intersect A_(t-1)`.
- `EmptyReset`: `A_t=empty` and every previous active member is cleared.

All choices use canonical permutation order and are deterministic from scenario id and invocation index.

## Qualification cardinalities

Active counts:

```text
0, 1, 32, 63, 64, 65, 256, 1024, 2048, 4095, 4096
```

Transition counts before legality clipping:

```text
0, 1, 8, 31, 32, 63, 64, 65, 256, 1024, 4096
```

## Measurement grid

Active counts:

```text
64, 256, 1024, 2048, 4096
```

Transition counts:

```text
0, 1, 8, 32, 64, 256, 1024, 4096
```

A measurement case is emitted only when the transition is legal for its Active/previous-Active relation. Rejected combinations are not silently clipped into another case identity.

## Required corpus checks

- every input set is canonical,
- `M_t` is a subset of surviving active membership,
- all derived sets equal independent set arithmetic,
- update and clear sets are disjoint,
- compact transition records reconstruct exactly `T_t`,
- block update/clear masks reconstruct exactly `W_t` and `R_t`,
- hold members preserve previous bytes,
- active members equal the current-generation oracle,
- inactive members equal the canonical sentinel,
- all Candidates produce byte-identical full outputs after every invocation.
