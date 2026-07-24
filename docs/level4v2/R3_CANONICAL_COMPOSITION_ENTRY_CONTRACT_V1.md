# Level 4 v2 R3 Canonical composition entry contract V1

## Entry condition

R3 may begin only after the R2 valid-chain, mutation/replay and Debug/Release deterministic Evidence gates pass.

## R3 responsibility

R3 reconstructs the proven finite Buffer-flow DAG subset around the opaque R2 Verified Plan boundary. It must make flow, allocation, schedule, state and synchronization authority explicit without letting Backend or Runtime reinterpret Semantic meaning.

## Initial proven limits

```text
Finite static DAG
Buffer flows only
Exactly one writer per flow
Zero or one presenter
One Adapter
Whole-composition Recovery scope
```

R3 must not silently add Texture sharing, cycles, conditionals, variants, streaming, residency, partial Recovery or multi-device support.

## Required preservation

- R2 Verified bindings remain opaque and cannot be rewritten by Composition.
- Composition may reference Verified Plan identities but cannot mint them.
- Raw or Planner proposals cannot enter execution or Frozen composition artifacts.
- Existing Level 4 v1 corpus must migrate without Backend judgment.
