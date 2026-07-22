# Spiral 5 CU2 baseline sufficiency and Composition gap

## Result

Spiral 5 does **not** require mutation of D3D12 Schema 17 or Runtime 17 at CU2.

The existing Package ABI already represents Package-internal temporal mechanics:

```text
ResourceFlags::Temporal
ResourceViewFlags::TemporalCurrent
ResourceViewFlags::TemporalPrevious
D3D12OperationCode::WaitTemporal
FrameSubmission temporal instance/fence fields
```

That machinery is sufficient to materialize multiple physical instances and wait on an earlier temporal dependency inside one Package execution contract.

## Precise remaining gap

The canonical Composition Contract represents static producer/consumer Buffer Flows. It does not state:

```text
which Flow is authoritative history,
which mathematical source generation the bytes represent,
which Frozen external schedule authorizes Update or Hold,
which generation may be read or replaced,
what completion authorizes retained history after a Hold,
when Recovery invalidates and requires reseeding.
```

Using `ResourceFlags::Temporal` alone would therefore leave Semantic generation and Hold authority to hidden Runtime or Backend convention. That is forbidden.

## Minimal extension

CU2 adds `VersionedSidecarTemporalExtensionV1` outside the legacy Package and Composition binary formats.

The sidecar binds:

- Canonical Temporal Semantic identity,
- Update Schedule identity,
- `GlobalMotorHistoryReuse` Candidate kind,
- `GlobalMotorHistory` role,
- one logical history generation,
- GPU-generated Update/Hold hierarchy Dispatch rule,
- fixed Consumer Dispatch,
- retained-history completion policy,
- Recovery invalidation/reseed policy,
- Spiral 4 Dispatch-only indirect artifact identity,
- actual Program-template identities.

Legacy Schema 17, Runtime 17, the canonical D3D12 Backend, and old Packages remain unchanged.

## Why this is not CU3 authority

CU2 builds one canonical artifact directly from the frozen Semantic and schedule. It proves that the physical path and sidecar layout are sufficient.

CU3 must still add:

```text
Raw Temporal Candidate
Planner proposal
Planner-independent rederivation
opaque Verified Temporal Plan
context/role/generation/Target seals
actual Resource identity binding
mutation and replay rejection
```
