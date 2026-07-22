# Spiral 5 canonical corpus v1

## Fixed dimensions

```text
Bone count:          8
Point Work count:    4096
Active Work count:   4096
Thread-group size:   64
Timeline length:     128
History depth:       1
Representation:      DirectPgaThroughConsumerV1
Temporal meaning:    LastUpdateWinsPiecewiseConstant
```

## Periodic measurement schedules

| ID | Update interval U | Update frames |
|---|---:|---|
| P1 | 1 | every frame |
| P2 | 2 | 0,2,4,...,126 |
| P4 | 4 | 0,4,8,...,124 |
| P8 | 8 | 0,8,16,...,120 |
| P16 | 16 | 0,16,32,...,112 |
| P32 | 32 | 0,32,64,96 |
| P64 | 64 | 0,64 |

## Non-periodic qualification schedules

```text
BurstThenHold:
0, 1, 2, 3, 64, 65, 127

Irregular:
0, 2, 5, 11, 17, 29, 47, 79, 127
```

## Generation rule

- Frame 0 is generation 0 and is always an update.
- Each later update increments generation by exactly one.
- A hold preserves generation.
- Local Motor bytes on a hold are byte-identical to the latest update bytes.
- Each generation uses a deterministic canonical eight-Motor corpus.

## Candidate corpus

```text
A.EveryInvocationRecompute
B.GlobalMotorHistoryReuse
C.FinalOutputHistoryReuse
```

## Required negative cases

- invocation 0 marked as hold,
- generation increment on hold,
- generation not incremented on update,
- Local Motor bytes changed on hold,
- history read before first valid update,
- history read from another Candidate or role,
- stale device-epoch history,
- retained output with mismatched generation.
