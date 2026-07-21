# SGE4-5 Spiral 3 completion specification v0.1

- Status: Proposed Frozen Research Contract
- Baseline commit: `398891936bf498fed532b26c06884bbfbf66f3a8`
- Title: Fixed-Count Reuse Sweep and Representation Materialization Boundary
- Japanese title: 固定長高再利用Consumerと表現物質化位置の交差実験
- Target ABI: D3D12 Schema 17 / Runtime 17

## 0. Decision

Spiral 3 holds the rigid hierarchy, dynamic Local PGA Motor Palette, same-frame execution, and fixed workload shape constant while varying only the transform reuse count `R`. It determines whether and where a representation-materialization crossover exists among three verified execution graphs.

The question is not “is PGA always faster than Matrix?” It is:

> For one unchanged hierarchical rigid-transform meaning, at what fixed reuse count does it become advantageous, if ever, to materialize Cartesian matrices before hierarchy, after hierarchy, or not at all?

## 1. Level 4 / Level 5 spiral

Level 5 contributes one richer workload parameter: the same Global transform is consumed repeatedly. The Compiler must retain the mathematical meaning and generate graph-level alternatives whose conversion positions differ.

Level 4 is tested without extension. The experiment must fit the existing Buffer-only, finite static same-frame DAG, single-writer Flow, one DeviceDomain, Frozen schedule/state/synchronization, and whole-composition recovery. No Spiral 3 mathematical concept may enter Composition Runtime or the D3D12 Backend.

If Level 4 v1 cannot express the experiment, the run must fail and document the precise missing contract. An ad-hoc ABI or Runtime escape hatch is forbidden.

## 2. Fixed meaning and swept workload

The canonical hierarchy is one deterministic eight-bone chain. Bone identities are stable and evaluation order is Compiler-derived. Each frame supplies exactly eight canonical binary32 Local PGA Motors through the existing Dynamic Slot contract.

The fixed reuse corpus is:

```text
R01   reuseCount = 1     pointCount = 8
R04   reuseCount = 4     pointCount = 32
R16   reuseCount = 16    pointCount = 128
R64   reuseCount = 64    pointCount = 512
R256  reuseCount = 256   pointCount = 2048
R512  reuseCount = 512   pointCount = 4096
```

`pointCount = boneCount * reuseCount`. The public construction boundary uses distinct explicit `BoneCountV1`, `ReuseCountV1`, and `PointCountV1` types so these dimensions cannot be substituted implicitly. `PointCountV1` is checked from the first two values and must not exceed 4096. Every point has a stable bone association by canonical record order. The point corpus is deterministic, finite, binary32, homogeneous (`w = 1`), and included in the Reuse Semantic identity.

The following stay fixed across the sweep:

- hierarchy topology and bone count,
- Dynamic Motor schema and frame corpus F00-F11,
- Candidate program-template versions,
- target profile,
- Observation Contract,
- dispatch policy,
- same-frame Composition and Recovery model.

Changing only `R` changes the Reuse Semantic identity and requires a different Frozen experiment. Dynamic Motor values do not change Frozen identity.

## 3. Candidate graphs

All candidates preserve the same Reuse Semantic and output the same point order.

### A. MatrixHierarchyConsumer

```text
Dynamic Local Motor
  -> Local Motor to Matrix
  -> Matrix hierarchy
  -> Matrix point consumer (R points per bone)
```

Matrix materialization occurs before hierarchy. Global transforms use 48-byte affine matrices.

### B. DirectPgaHierarchyConsumer

```text
Dynamic Local Motor
  -> Motor hierarchy
  -> Direct PGA point consumer (R points per bone)
```

No Cartesian Matrix is materialized. Global transforms remain 32-byte Motors through the Consumer.

### C. HybridHierarchyConsumer

```text
Dynamic Local Motor
  -> Motor hierarchy
  -> Global Motor to Matrix
  -> Matrix point consumer (R points per bone)
```

Matrix materialization occurs after hierarchy and is reused by the Consumer.

Raw graphs are proposals only. They cannot Freeze, execute, submit, or mint a verified seal.

## 4. Independent authority

The Reuse Representation Verifier must not reference or call the Planner. Starting from the canonical hierarchy, Reuse Semantic, Candidate kind, and Target contract, it independently rederives and checks:

- Semantic and point-corpus identities,
- candidate roles and exact edge graph,
- conversion position,
- element counts and strides,
- dispatch dimensions,
- Program-template and binding identities,
- Observation Contract and target profile,
- plan and operation identities.

Only the opaque context-bound Verified Reuse Representation may enter the Leaf Compiler. The Verified type is non-aggregate, non-default-constructible, and cannot be constructed or implicitly converted from a Raw Candidate. Before every lowering entry, the Leaf Compiler rechecks the seal context against the hierarchy identity, Reuse Semantic identity, bone/reuse/point dimensions and certificate. Cross-R, cross-corpus and cross-hierarchy replay must fail before Package generation. Every lowered Leaf is rebound to the verified operation, Program, binding, Shader, and dispatch digests of the actual Package.

## 5. Frozen Composition

The canonical comparison contains eleven Leaves:

```text
L0  DynamicInvocationSource
L1  ConsumerPointSource
L2-L4  Candidate A
L5-L6  Candidate B
L7-L9  Candidate C
L10 Observer
```

It contains twelve single-writer Buffer Flows: shared dynamic Motor input, shared immutable point input, reference output, candidate intermediates and outputs, and observation records. The Runtime follows only the verified Frozen schedule, allocation, state, synchronization, and bindings.

The Dynamic source supplies Motor and independent CPU-reference bytes. The point source owns immutable point bytes. Motor and point Buffers fan out to all applicable candidates. Candidate outputs and reference fan in to the Observer.

## 6. Observation

For every R/F execution, candidate A, B, and C are compared against an independent binary64 CPU reference and pairwise against each other. The record contract measures:

- absolute and relative error,
- ULP distance,
- non-finite and homogeneous-coordinate validity,
- rigidity checks using the first canonical probe subset where available,
- deterministic point index and record order.

GPU reduction order is not an authority. CPU aggregation is deterministic in record order. Performance superiority is not a completion gate.

## 7. Recovery and determinism

The whole eleven-Leaf Composition is one Recovery unit. Controlled recovery must increment the device epoch, release and rematerialize all eleven Leaves and twelve shared Buffers, reject stale handles/tokens, and require explicit Dynamic rebind. Actual RemoveDevice must quarantine the removed adapter and enter `AwaitingAdapter` when no eligible replacement exists.

The following must be byte-identical across fresh Debug A, fresh Debug B, and Release:

- Reuse Semantic and point corpus,
- raw and verified candidate graphs,
- Frozen Leaf evidence,
- Frozen Composition evidence,
- CPU-reference identities,
- final Architecture freeze bundle.

WARP output bytes must be deterministic for all 6 reuse cases x 12 frames = 72 executions.

## 8. Real GPU measurement

After Architecture Complete, one eligible hardware adapter measures A/B/C using balanced six-order permutations:

```text
ABC ACB BAC BCA CAB CBA
```

For every reuse case, record preparation, Package/materialization, command recording, GPU timestamp, end-to-end, dispatch/barrier counts, bytes, and numeric error. Report the winner per R and whether a stable winner transition is observed. A transition is evidence for a materialization boundary; absence of a transition is also a valid result.

Hardware itself is not Semantic input. Evidence is bound to Candidate identity, Target profile, Adapter fingerprint, Driver, timestamp frequency, measurement configuration, and Observation Contract.

## 9. Explicit non-goals

Spiral 3 does not add:

- variable point or Work count inside one Frozen Package,
- DispatchIndirect or ExecuteIndirect,
- Temporal Flow or frame history,
- Vertex/Raster Consumer, Texture Flow, or image observation,
- Runtime Variant selection or Conditional Region,
- Skinning weights, blend shapes, IK, animation blending,
- multiple DeviceDomain, partial recovery, streaming, or residency,
- Universal mathematical Semantic IR,
- a global PGA-versus-Matrix performance rule.

## 10. Completion units

- CU0: contract, baseline, project boundaries, manifests.
- CU1: fixed hierarchy, R sweep Semantic, point corpus, independent CPU reference.
- CU2: three candidate graphs, independent verifier, authority mutations.
- CU3: Package-bound source and candidate Leaf groups.
- CU4: eleven-Leaf/twelve-Flow Frozen Composition using unchanged Level 4 v1.
- CU5: 72 WARP runs, fresh-process determinism, controlled/actual recovery.
- CU6: hardware measurement, binary evidence, crossover Decision Report.

## 11. Completion invariants

| ID | Invariant |
|---|---|
| S3-I01 | Canonical Semantic retains hierarchy and Motor meaning; it contains no Matrix or Backend concept. |
| S3-I02 | Exactly six fixed reuse counts produce distinct deterministic Reuse identities; Bone/Reuse/Point dimensions are distinct explicit boundary types. |
| S3-I03 | Same Semantic produces A/B/C with conversion before, never, or after hierarchy. |
| S3-I04 | Raw candidates are non-executable and non-freezable, and cannot construct or convert to the Verified type. |
| S3-I05 | Verifier is Planner-independent and rederives graph, schemas, dispatches, and Program identities. |
| S3-I06 | Leaf Compiler accepts only verifier-sealed operations, rejects cross-context seal replay, and binds operations to actual Package digests. |
| S3-I07 | One immutable point source and one dynamic source fan out without duplicate writers. |
| S3-I08 | Level 4 v1 expresses eleven Leaves and twelve Buffer Flows without ABI/Runtime extension. |
| S3-I09 | Runtime does not choose representation, conversion position, schedule, state, or synchronization. |
| S3-I10 | A/B/C satisfy reference and pairwise Observation Contracts for all 72 WARP executions. |
| S3-I11 | Palette changes do not modify Frozen bytes; R changes do. |
| S3-I12 | Debug/fresh-process/Release artifacts are byte-identical. |
| S3-I13 | Controlled and actual recovery preserve authority and reject stale epochs. |
| S3-I14 | Hardware evidence is bound to adapter, target, configuration, candidate, and observation identities. |
| S3-I15 | Decision Report identifies observed winner transitions without authorizing Runtime policy. |
| S3-I16 | Next capability remains Owner-deferred. |

## 12. Completion declaration

Architecture completion requires CU0-CU5. Experiment completion additionally requires CU6 on an eligible real adapter.

The final Decision Report must end with:

```text
NextCapabilitySelection = DeferredByOwner
SelectionStatus = OWNER_DECISION_REQUIRED
```
