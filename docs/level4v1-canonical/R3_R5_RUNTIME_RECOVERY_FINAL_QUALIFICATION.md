# Level 4 v1 Canonical R3-R5 Runtime, Recovery and Final Qualification

## 1. Base and reconstruction rule

This change set applies to the R2-complete source state identified by commit
`18152c574a85383cf55630d5b3fe2dd851fbff92`.

The package is self-contained. Preparation and qualification do not read source
files from repository history, switch revisions, or invoke repository commands.
Every changed existing file and every new file is present in the ZIP.

The earlier discovery implementation contributed only requirements and observed
failure modes. It is not a source, ABI, project-layout, or byte-compatibility
dependency.

## 2. One ownership model for R3 and R4

`LoadedStaticComposition` is the whole runtime ownership unit. It retains:

- the independently verified Frozen Composition;
- one `SharedDeviceDomain` and its current epoch;
- all Schema 17 Leaf instances;
- one Composition-owned native Buffer per verified Resource Flow;
- the current completion-token chain for each Resource Flow;
- the original input bytes required for whole-composition rematerialization.

No Leaf creates an independent D3D12 device. `LoadIntoDomain` materializes every
Leaf against the same native DeviceDomain. A shared resource and every token
which orders access to it carry the same owner and device epoch.

## 3. R3 proof boundary: normal execution

R3 accepts Frozen Composition bytes and immediately calls
`ReadVerifiedFrozenComposition`. It does not accept `RawCompositionPlan`, call a
planner, call the independent verifier as an authoring operation, or construct a
new Frozen artifact.

The runtime follows the verified decisions for:

- canonical Leaf schedule;
- endpoint-to-Resource-Flow binding;
- one allocation per Resource Flow;
- incoming and outgoing state handoff;
- signal and wait authority;
- presenter identity.

The runtime may materialize native objects and perform the transitions already
required by those verified decisions. It may not choose a different graph,
allocation, binding, or schedule.

R3 WARP scenarios cover independent, linear, fan-out, multi-input, diamond,
headless, and single-presenter graphs. Repeated fan-out execution proves that all
readers retire before the next producer reuse.

## 4. R4 proof boundary: whole-composition recovery

Recovery is not performed one Leaf at a time. Before the native DeviceDomain
transition, R4 destroys:

- every shared Buffer;
- every completion token;
- every Leaf instance;
- every base-package runtime owner associated with those instances.

A controlled rebuild advances the DeviceDomain epoch. After reacquisition, R4
re-reads the same Frozen Composition bytes through the R2 high-level Reader,
rematerializes every Leaf, and recreates every Resource Flow from its retained
initial bytes. Old resource and token handles are rejected by epoch.

Actual `ID3D12Device5::RemoveDevice` qualification records the removed adapter
LUID, excludes it from reacquisition, enters `AwaitingAdapter` when no eligible
adapter remains, retains no Leaf or shared resource, rejects submission, and
preserves that state across retry when the same excluded WARP adapter is still
the only candidate.

R4 proves only whole-composition recovery. Partial Leaf recovery, partial graph
recovery, multiple devices, and cross-device migration are outside Level 4 v1.

## 5. R5 proof boundary: final qualification

R5 adds no new execution authority. It combines evidence from:

- R1 Frozen Composition container and corruption gates;
- R2 Contract and independent verification authority;
- R3 shared DeviceDomain static-DAG execution;
- R4 controlled and actual-removal recovery;
- Architecture and source-inventory checks;
- Stage 0D Foundation Freeze;
- fresh Debug-process and Debug/Release evidence identity.

The final banner is emitted only after all applicable gates pass:

```text
============================================================
SGE4 LEVEL 4 V1 CANONICAL FREEZE PASSED
Foundation: preserved Stage 0D / Schema 17 / Runtime 17
Composition: verified static acyclic multi-Leaf Buffer graph
Authority: independent verification and frozen-only runtime
Device: one DeviceDomain and whole-composition recovery
Determinism: fresh Debug and Release evidence is byte-identical
============================================================
```

## 6. Explicit non-claims

Level 4 v1 does not claim Texture Resource Flows, multiple writers, conditional
regions, graph cycles, variants, streaming/residency, partial recovery, or
multiple DeviceDomains. Those capabilities require a later reconstruction after
their contracts and proof boundaries are known.
