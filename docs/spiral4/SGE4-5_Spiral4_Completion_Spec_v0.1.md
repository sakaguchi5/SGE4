# SGE4-5 Spiral 4 completion specification v0.1

- Status: Proposed Frozen Research Contract
- Completion Unit: CU1
- Baseline commit: `8c1125394ba4b45d571d7cba4e7ad685bb90918b`
- Title: Dynamic Active Work Count and Verified Indirect Batch Lowering
- Japanese title: 可変Work Count＋DispatchIndirect＋Batch比較
- Baseline ABI: D3D12 Schema 17 / Runtime 17
- CU1 ABI mutation: none

## 0. Decision

Spiral 4 removes one fixed assumption that remained through Spirals 1, 2, and 3:

> One Frozen experiment no longer requires the number of active element-work records to be constant on every frame.

The Semantic graph, Leaf graph, Resource capacity, maximum workload, mathematical operation, output order, and Observation Contract remain fixed. Only the active prefix length `Nf` and the bytes of the active records are runtime inputs.

Spiral 4 asks:

> For one unchanged rigid-transform meaning with fixed capacity `Nmax`, how should a Compiler lower the runtime active count into a fixed maximum dispatch, one verified indirect dispatch, or a verified batch of indirect dispatches without returning dispatch or batching decisions to Runtime or Backend?

The purpose is not to make ExecuteIndirect win. The purpose is to make dynamic execution quantity a verified lowering dimension and to extend Level 4 only by the smallest Frozen contract that is actually required.

## 1. Why Spiral 4 follows Spiral 3

Spiral 1 proved that one retained mathematical meaning can produce multiple verified GPU Lowerings.

Spiral 2 extended the choice across a dynamic hierarchy and graph-level representation paths.

Spiral 3 extended the choice through the Consumer and swept a fixed reuse count, but every Frozen experiment still fixed point count, dispatch dimensions, Buffer capacity, and graph shape.

Spiral 4 changes the active work count inside one Frozen experiment while preserving all structural bounds.

```text
Spiral 3
  one Frozen experiment per fixed R and fixed point count

Spiral 4
  one Frozen capacity Nmax
  + runtime ActiveWorkCount Nf
  + verified physical dispatch/batch Lowering
```

## 2. Precise meaning of “variable Work Count”

“Work” in Spiral 4 does not mean a variable number of Semantic Work nodes, Leaves, Packages, Resources, or graph edges.

The variable is the number of active element-work records consumed by one fixed Compute role.

```text
Nmax = 4096
0 <= Nf <= Nmax

Active range:
  WorkRecord[0, Nf)

Inactive range:
  WorkRecord[Nf, Nmax)
```

The following remain Frozen:

- Semantic operation and output order,
- eight-bone hierarchy and canonical Motor conventions,
- maximum reuse per bone = 512,
- maximum work count = 4096,
- Work record schema and Buffer capacity,
- thread-group size = 64,
- Candidate kinds,
- maximum Batch slots,
- Program templates and Observation Contract,
- Composition graph shape and Recovery unit.

The following may change per FrameInvocation:

- `ActiveWorkCountV1 Nf`,
- active Work record bytes,
- eight Local Motor values,
- indirect argument bytes generated from the verified rule.

`Nf` is not allowed to resize a Resource, create a Leaf, change a Package, or mutate Frozen bytes.

## 3. Fixed mathematical and representation baseline

Spiral 4 isolates dispatch and batching. It does not cross-product the three Spiral 3 representation paths with the new dispatch candidates.

The fixed mathematical workload is:

```text
eight dynamic Local PGA Motors
  -> fixed hierarchy
  -> Direct PGA Global Motor representation
  -> active prefix of canonical point-transform records
```

The representation path is `DirectPgaThroughConsumerV1`, used as an experimental control. This does not authorize it as a universal Runtime policy or as a global winner.

All Spiral 4 candidates receive the same Global Motor bytes, Work records, `Nf`, output schema, and Observation Contract. Candidate differences begin only at physical work issuance and Batch structure.

## 4. Baseline Level 4 inventory

At baseline commit `8c1125394ba4b45d571d7cba4e7ad685bb90918b`:

- Package Resource state already represents `IndirectArgument`,
- Schema 17 has no Frozen ExecuteIndirect/DispatchIndirect operation,
- Schema 17 has no Command Signature artifact,
- Runtime and Backend therefore have no authorized Frozen route for indirect Compute dispatch.

CU1 records this inventory but does not modify ABI, Runtime, Backend, Solution projects, or C++ source.

A future CU may extend the ABI only after proving that Schema 17 cannot express the required execution without hidden Backend judgment. Reusing `ExecuteCompute` with an implicit Backend branch is forbidden.

Project terminology uses “DispatchIndirect” for the research capability. The intended D3D12 physical mechanism is an explicit versioned ExecuteIndirect operation whose verified Command Signature contains a Dispatch argument.

## 5. Candidate Lowerings

All candidates preserve the same active prefix exactly once and must not modify the inactive tail.

### A. FixedMaximumGuarded

```text
fixed direct Compute dispatch for Nmax
  -> each thread reads Nf
  -> index >= Nf exits without output write
```

The physical dispatch size is Frozen. The runtime active count affects only the Program guard.

### B. SingleExecuteIndirectDispatch

```text
ActiveCount / argument input
  -> Argument Producer Leaf
  -> one Dispatch argument record
  -> one verified ExecuteIndirect dispatch
```

The group count rule is:

```text
groupCountX = ceil(Nf / threadsPerGroup)
groupCountY = 1
groupCountZ = 1
```

`Nf = 0` produces zero dispatch groups and no active output writes.

Runtime must not read back `Nf` or argument bytes to choose a direct Dispatch call.

### C. BatchedExecuteIndirectDispatch

The active prefix is partitioned into deterministic contiguous, non-overlapping Batch ranges.

For Batch size `B`:

```text
batch i:
  firstWork = i * B
  workCount = min(B, max(Nf - firstWork, 0))
```

Candidate identity includes `B`. The V1 Batch-size corpus is:

```text
64, 128, 256, 512, 1024
```

V1 reserves 64 fixed Batch slots. Empty slots encode zero Dispatch groups. V1 does not use an indirect count Buffer, GPU compaction, prefix sum, or variable Operation count.

The union of nonempty Batch ranges must equal `[0, Nf)`. Ranges must not overlap, leave gaps, reorder outputs, or execute one record twice.

## 6. Authority chain

The only formal route is:

```text
Spiral4ExperimentSpec
  -> Canonical Active Work Semantic
  -> Raw Dispatch/Batch Candidate
  -> Independent Active Work Verifier
  -> Verified Dispatch/Batch Plan
  -> Leaf Target Compiler
  -> Frozen Leaf Package
  -> Composition Contract
  -> Independent Composition Verifier
  -> Frozen Composition
  -> Runtime / D3D12 Executor
```

Raw candidates cannot Freeze, execute, submit, mint a Verified type, or replay another context’s seal.

The Independent Verifier starts from the Canonical Semantic, `Nmax`, `Nf` contract, thread-group size, Candidate kind, Batch size, Target contract, and Program-template identities. It independently rederives:

- active and inactive ranges,
- direct or indirect dispatch dimensions,
- Batch partition and argument-record mapping,
- maximum command and Buffer bounds,
- Producer/Consumer Flow identities,
- required Resource states and transitions,
- Command Signature and Operation identity,
- output range and Observation Contract,
- Target profile and actual Package digests.

The Planner must not be a dependency of the Verifier.

## 7. Level 4 extension rule

Spirals 1–3 expanded the proven application range of unchanged Level 4 v1. Spiral 4 is expected to require a real Level 4 capability extension.

The minimal target capability is:

```text
verified Indirect Argument Flow
+ explicit Frozen Command Signature
+ explicit Frozen ExecuteIndirect operation
+ mechanical Backend mapping
+ whole-composition Recovery
```

Any extension must remain versioned and must preserve old Schema 17 / Runtime 17 artifacts. The exact next version is not declared by CU1.

Runtime and Backend are forbidden to decide:

- direct versus indirect execution,
- dispatch dimensions,
- Batch size or Batch count,
- active range,
- argument record layout or offset,
- Command Signature kind,
- Producer/Consumer dependency,
- Resource state or synchronization,
- whether an empty Batch should be skipped.

## 8. Observation Contract

Qualification compares every candidate with an independent CPU reference and pairwise with every other candidate.

For each case, it records:

- active count and Candidate identity,
- exact output bytes or numeric error for `[0, Nf)`,
- inactive-tail sentinel preservation for `[Nf, Nmax)`,
- non-finite and homogeneous-coordinate validity,
- first mismatch index,
- max absolute, relative, and ULP error,
- duplicate, missing, reordered, or out-of-range Work flags,
- deterministic readback digest.

Qualification may reset candidate output Buffers to a deterministic sentinel before execution. Performance timestamps must distinguish reset, argument production, indirect execution, and observation costs.

`Nf = 0` is a first-class valid case.

## 9. Canonical corpus

The Active Count corpus is:

```text
0,
1,
63, 64, 65,
127, 128, 129,
255, 256, 257,
511, 512, 513,
1023, 1024,
2048,
4095, 4096
```

It covers zero, minimum, thread-group boundaries, Batch boundaries, partial capacity, and full capacity.

The V1 Work-cost profile is uniform. Heterogeneous cost distribution, culling, compaction, sparse index lists, and data-dependent Work generation are deferred.

## 10. Recovery and determinism

The whole Spiral 4 Composition is one Recovery unit.

Recovery must destroy and rematerialize:

- Candidate Leaves,
- fixed-capacity Work and output Buffers,
- indirect argument Buffers,
- Command Signature objects,
- endpoint states and completion tokens,
- readback state.

After a new device epoch:

- old argument Buffers, tokens, submissions, and Package instances are rejected,
- dynamic Motor, Work, and Active Count inputs require explicit rebind,
- argument records are regenerated,
- the same Frozen Composition reproduces the same WARP observation.

The following must be byte-identical across fresh Debug A, Debug B, and Release:

- Canonical Active Work Semantic,
- contract and corpus manifests,
- Raw and Verified plans,
- Frozen Leaf Packages,
- Command Signature artifact,
- Frozen Composition,
- Observation Contract and architecture evidence.

## 11. Real GPU experiment

Architecture completion precedes hardware measurement.

The primary independent variable is `Nf`. The secondary variable for candidate C is Batch size `B`.

Evidence must separate:

- active-input preparation,
- argument generation,
- state transition and synchronization,
- command recording,
- direct or indirect GPU execution,
- end-to-end submission,
- numeric observation.

The Decision Report classifies:

- per-`Nf` winner,
- Fixed versus Single Indirect transition or no transition,
- Single versus Batched transition or no transition,
- best observed Batch size,
- stable, unstable, multiple-transition, or noise-equivalent regions.

No observed crossover is a valid result. No performance result authorizes Runtime adaptive selection.

## 12. Explicit non-goals

Spiral 4 V1 does not add:

- variable Semantic Work-node, Leaf, Resource, Flow, or Package count,
- Resource reallocation or truly variable-capacity Buffer,
- sparse active-index list, GPU culling, compaction, prefix sum, or scan,
- indirect count Buffer,
- data-dependent graph mutation,
- Temporal Flow or frame history,
- Vertex/Raster Consumer, Texture Flow, or image observation,
- heterogeneous Work-cost distribution,
- representation-path comparison,
- Runtime Variant selection or Conditional Region,
- multiple DeviceDomain, partial recovery, streaming, or residency,
- a universal rule that indirect or batching is faster.

## 13. Completion units

- CU1: Owner selection, baseline inventory, research contract, corpus, boundaries, proof ledger, no C++ or ABI change.
- CU2: one Single ExecuteIndirect architecture and minimal versioned Level 4 extension.
- CU3: independent authority, mutation gates, context-bound seals, Package-bound operation identities.
- CU4: Fixed Maximum, Single Indirect, and Batch-size Candidate family.
- CU5: WARP corpus, inactive-tail proof, fresh-process determinism, controlled/actual recovery.
- CU6: real-GPU measurement, binary evidence, crossover/Batch Decision Report.

## 14. Completion invariants

| ID | Invariant |
|---|---|
| S4-I01 | `Nf` varies at Runtime while `Nmax`, graph shape, Resources, and Semantic operation remain Frozen. |
| S4-I02 | `0 <= Nf <= 4096`; `Nf = 0` and `Nf = Nmax` are valid. |
| S4-I03 | Active Work is exactly prefix `[0, Nf)`; inactive tail is not executed or written. |
| S4-I04 | Direct PGA representation is fixed as the experiment control and is not a Runtime policy authorization. |
| S4-I05 | A/B/C preserve identical active Work, output order, and Observation Contract. |
| S4-I06 | Raw Dispatch/Batch Candidates are non-executable and non-freezable. |
| S4-I07 | Verifier is Planner-independent and rederives dispatch, Batch, argument, state, and Command Signature identities. |
| S4-I08 | Runtime never reads back `Nf` to choose Dispatch dimensions. |
| S4-I09 | Backend mechanically maps an explicit Frozen indirect Operation and never hides an indirect branch inside ExecuteCompute. |
| S4-I10 | Batch ranges are contiguous, disjoint, complete over `[0, Nf)`, and deterministic. |
| S4-I11 | V1 uses fixed Batch slots and zero-group empty records, not an indirect count Buffer. |
| S4-I12 | Schema/Runtime extension is minimal, versioned, and justified by a Schema 17 insufficiency proof. |
| S4-I13 | Active output matches reference and inactive-tail sentinel remains unchanged. |
| S4-I14 | Debug/Release/fresh-process artifacts are byte-identical. |
| S4-I15 | Controlled and actual recovery reject stale epochs and regenerate argument records. |
| S4-I16 | Hardware evidence binds `Nf`, Batch size, Candidate, target, adapter, measurement, and observation identities. |
| S4-I17 | Performance superiority and crossover are not completion gates. |
| S4-I18 | Runtime policy authorization remains `None`. |
| S4-I19 | Spiral 3 evidence remains historical and is not rewritten by the Spiral 4 Owner selection. |
| S4-I20 | Next capability remains Owner-only after the Spiral 4 Decision Report. |

## 15. Completion declaration

### Architecture Complete

CU1–CU5 are complete when dynamic active count, verified indirect dispatch, all Candidate Lowerings, semantic equivalence, authority gates, deterministic artifacts, and Recovery are qualified.

### Experiment Complete

Architecture Complete plus CU6 real-hardware evidence and crossover/Batch classification.

### Spiral 4 Closed

Experiment Complete plus a Decision Report that records:

```text
RuntimePolicyAuthorization = None
NextCapabilitySelection = DeferredByOwner
SelectionStatus = OWNER_DECISION_REQUIRED
```

Indirect or Batched execution does not need to win. A crossover does not need to exist.
