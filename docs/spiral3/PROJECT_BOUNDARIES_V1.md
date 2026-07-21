# Spiral 3 project boundaries V1

## Present in CU0/CU1

```text
101 Spiral3Contracts
  explicit Bone / Reuse / Point workload boundary types

100 ReuseSweepSemantic
  hierarchy + fixed-R Consumer meaning; no Matrix or Backend concepts

102 Spiral3Corpus
  fixed hierarchy, point corpus, frame pairing and independent CPU reference

110 Spiral3SemanticTests
  compile-time dimension separation, semantic validation and deterministic corpus evidence
```

Enforced boundaries:

- `BoneCountV1`, `ReuseCountV1` and `PointCountV1` are explicit and non-interchangeable.
- Semantic projects do not reference Runtime or Backend.
- Runtime and Backend do not reference projects 100-102.
- Spiral 3 reuses D3D12 Schema 17, Runtime 17 and the Spiral 2 dynamic Motor contract unchanged.

## Planned after CU1

Projects 103-109 and tests 111-119 are reserved by the completion specification but are not part of this delivery.
