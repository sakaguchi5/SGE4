# Spiral 6 CU2 Compact Index List architecture

## Semantic path

```text
ExactSparseWorkSetV1 S
  -> canonical strictly increasing universe indices
  -> fixed-capacity Compact Index List sidecar
  -> Frozen Dispatch arguments X = ceil(|S| / 64), or zero when S is empty
  -> Spiral 4 Dispatch Command Signature / ExecuteIndirect
  -> one GPU thread per compact ordinal
  -> index = CompactIndices[ordinal]
  -> Direct PGA point transform
  -> Output[index]
```

The compact ordinal is never the output address. The original universe index is the only authorized output location.

## Fixed-capacity representation

```text
capacity       = 4096 uint32 indices
capacity bytes = 16384
active prefix  = exactly K canonical indices
unused tail    = 0xFFFFFFFF
```

Unused capacity is part of canonical artifact bytes. Any different tail value is rejected.

## Output contract

Before Dispatch, all 4096 output records are initialized to:

```text
float4(0,0,0,0)
```

For each universe index `i`:

```text
i in S     -> Output[i] is the canonical Direct PGA result, w = 1
i not in S -> Output[i] remains byte-identical float4(0,0,0,0)
```

This proves both the active result and absence of unauthorized inactive writes.

## CU2 corpus

CU2 uses 12 WARP cases:

- `K=0`, `K=1`, `K=4095`, `K=4096` boundary cases,
- all four patterns at `K=65`,
- all four patterns at `K=1024`.

`K=65` crosses one thread-group boundary. `K=1024` demonstrates that equal cardinality does not collapse distinct set identities or placements.

## Authority boundary

CU2 validates canonical construction and artifact parsing. It does not yet create Raw Candidate, Planner, independent Verifier, opaque Verified Plan, Resource binding, or device-epoch seal. Those are CU3 responsibilities.

Runtime and Backend do not choose membership, cardinality, pattern, Dispatch dimensions, or output addresses.
