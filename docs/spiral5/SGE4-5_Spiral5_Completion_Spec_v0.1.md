# SGE4-5 Spiral 5 completion specification v0.1

- Status: Proposed Frozen Research Contract
- Completion Unit: CU1
- Baseline commit: `f29d6597ec370d963c7b7dfbbc9af9590e8bd58f`
- Title: Temporal State Flow and Verified History Reuse Lowering
- Japanese title: Temporal State Flow＋更新頻度・履歴再利用Lowering
- Baseline ABI: D3D12 Schema 17 / Runtime 17
- Available extension: Spiral 4 Versioned Sidecar Indirect Extension V1
- CU1 ABI mutation: none

## 0. Decision

Spiral 5 removes one fixed assumption that remained through Spirals 1–4:

> One Frozen experiment no longer requires every invocation to recompute derived GPU state when the mathematical source state has not changed.

The Semantic graph, hierarchy, representation path, Work count, Resource capacity, mathematical operation, output order, and Observation Contract remain fixed. A verified external update schedule states when one new source generation begins. Between updates, the exact semantic state is the most recent update state.

Spiral 5 asks:

> For one unchanged piecewise-constant rigid-transform timeline, where should a Compiler retain history: only at source input, after hierarchy as Global Motors, or after the final Consumer output, without returning update, hold, or history-reuse decisions to Runtime or Backend?

The purpose is not to make caching win. The purpose is to make temporal lifetime and history materialization a verified lowering dimension and to extend Level 4 only by the smallest Frozen temporal contract actually required.

## 1. Why Spiral 5 follows Spiral 4

Spiral 1 proved that one mathematical meaning can produce multiple verified GPU representations.

Spiral 2 extended the choice across a dynamic hierarchy.

Spiral 3 moved the representation-materialization boundary through the Consumer and observed a reuse-dependent winner transition.

Spiral 4 made active Work quantity dynamic, constructed a seven-candidate issuance family, completed Recovery, and measured the family on a real GPU. It observed no Fixed/Single or Single/BestBatch winner transition across the frozen Active Count corpus.

Spiral 5 therefore changes a different structural variable:

```text
Spiral 4
  one invocation
  + variable active Work quantity Nf

Spiral 5
  one fixed Work quantity
  + a sequence of invocations
  + externally declared source generations
  + verified history reuse positions
```

Spiral 5 does not infer a game update rate from distance, visibility, importance, or animation policy. The update schedule is an external verified input. Hardware identity remains a Target/Measurement fact, not part of the mathematical Semantic.

## 2. Precise temporal meaning

Spiral 5 V1 uses an exact piecewise-constant timeline.

The formal V1 temporal-rule identifier is `LastUpdateWinsPiecewiseConstant`.

```text
Timeline length T = 128
Invocation index 0 <= t < T
Source generation g(t)
Update event u(t) in {0,1}
```

The first invocation must be an update.

```text
u(0) = 1
g(0) = 0
```

For t > 0:

```text
if u(t) = 1:
  g(t) = g(t-1) + 1

if u(t) = 0:
  g(t) = g(t-1)
```

Every invocation supplies the eight Local Motor bytes. On a hold invocation, the bytes and generation must be byte-identical to the most recent update invocation.

```text
u(t) = 0
  -> LocalMotors(t) = LocalMotors(lastUpdate(t))
  -> Output(t)      = Output(lastUpdate(t))
```

This is exact reuse, not approximation.

Interpolation is not part of V1. Interpolation changes the mathematical temporal meaning and must be introduced by a later explicit Semantic contract rather than treated as a free optimization.

## 3. Fixed mathematical and representation baseline

Spiral 5 isolates temporal history placement. It does not cross-product temporal candidates with Spiral 3 representation candidates or Spiral 4 active-count candidates.

The fixed workload is:

```text
eight Local PGA Motors
  -> fixed eight-bone hierarchy
  -> Direct PGA Global Motor representation
  -> 4096 canonical point-transform records
  -> fixed output order
```

The following remain Frozen:

- hierarchy topology and canonical Motor conventions,
- representation path `DirectPgaThroughConsumerV1`,
- Work count = Active Work count = 4096,
- thread-group size = 64,
- Work and output Buffer capacities,
- timeline length = 128,
- logical history depth = 1,
- Candidate kinds,
- Program templates and Observation Contract,
- Composition graph shape and Recovery unit.

The following vary per invocation:

- invocation index `t`,
- source generation `g`,
- external update event `u`,
- eight Local Motor bytes consistent with `g`,
- temporal indirect argument bytes generated from the verified rule.

No temporal input may resize a Resource, add a Leaf, mutate Frozen bytes, or change the mathematical operation.

## 4. Baseline Level 4 inventory

At baseline commit `f29d6597ec370d963c7b7dfbbc9af9590e8bd58f`:

- `StaticCompositionFrameInvocation` already contains `frameNumber`,
- per-invocation dynamic Leaf data already exists,
- whole-composition Recovery and device epochs already exist,
- Spiral 4 provides verified zero/nonzero indirect Dispatch machinery,
- the canonical Composition Contract describes static producer/consumer Resource Flows,
- no explicit Temporal History Flow role exists,
- no Frozen history-generation artifact exists,
- no update-schedule artifact exists,
- no retained-history completion contract exists.

CU1 records this inventory but does not modify C++, the Solution, Package ABI, Runtime, Backend, or the Spiral 4 sidecar.

A future CU may add a versioned temporal sidecar or another minimal extension only after proving that existing contracts cannot express history generation, retention, and hold completion without hidden Runtime/Backend judgment.

## 5. Candidate Lowerings

All candidates receive the same timeline, update events, Local Motor bytes, output schema, and Observation Contract.

### A. EveryInvocationRecompute

```text
every invocation
  -> hierarchy executes
  -> Consumer executes
  -> output is rewritten
```

The update event does not suppress work. This is the control Candidate.

### B. GlobalMotorHistoryReuse

```text
update invocation
  -> hierarchy executes
  -> Global Motor history generation is replaced

hold invocation
  -> hierarchy dispatch is zero
  -> previous Global Motor history is read

all invocations
  -> Consumer executes
  -> output is rewritten
```

The history materialization boundary is after hierarchy and before the Consumer.

### C. FinalOutputHistoryReuse

```text
update invocation
  -> hierarchy executes
  -> Consumer executes
  -> final output history generation is replaced

hold invocation
  -> hierarchy dispatch is zero
  -> Consumer dispatch is zero
  -> previous final output remains authoritative
```

The history materialization boundary is after the Consumer.

A hold invocation must have an explicit verified completion meaning. Runtime or Backend may not silently skip work merely because an update bit is false.

## 6. External schedule and update interval

The primary experimental variable is update interval `U`.

```text
U = number of invocations from one update to the next periodic update
```

The measurement corpus is:

```text
U = 1, 2, 4, 8, 16, 32, 64
```

`U = 1` updates every invocation. `U = 64` creates one update followed by 63 exact hold invocations before the next update.

Qualification also includes non-periodic schedules. Their update frames are Frozen corpus data, not Runtime choices.

Runtime is forbidden to derive or alter `U`, to classify one invocation as unimportant, or to choose a Candidate from observed performance.

## 7. History and generation rules

V1 has one logical history slot per history role.

```text
HistoryDepth = 1
```

History begins invalid and becomes valid only after the first update succeeds.

Each history object is bound to:

- Frozen Candidate identity,
- Temporal Semantic identity,
- history role,
- source generation,
- device epoch,
- Target profile,
- actual materialized Resource identity.

On update, source generation increments exactly once and the authorized history generation is replaced.

On hold, source generation and history generation remain unchanged.

A history object from another Candidate, schedule contract, role, source generation, device epoch, or Target profile is rejected.

## 8. Authority chain

The only formal route is:

```text
Spiral5ExperimentSpec
  -> Canonical Temporal Semantic
  -> Verified Update Schedule Contract
  -> Raw History-Reuse Candidate
  -> independent Temporal Verifier
  -> opaque Verified Temporal Plan
  -> temporal Leaf/sidecar Compiler
  -> Frozen Temporal Artifact
  -> Frozen Composition
  -> epoch-bound History Binding
  -> Runtime / D3D12 Executor
```

Raw candidates cannot Freeze, execute, submit, mint a Verified type, or replay another context’s seal.

The independent Verifier starts from the Canonical Temporal Semantic, update schedule contract, Candidate kind, history role, fixed Work meaning, Target contract, and Program-template identities. It independently rederives:

- update and hold invocations,
- source-generation sequence,
- legal history reads and writes,
- hierarchy and Consumer dispatch enablement,
- zero-dispatch argument mapping,
- history Resource role and capacity,
- retained-output authority,
- required states and synchronization,
- completion-token behavior,
- Recovery and stale-generation boundaries,
- output range and Observation Contract,
- actual Frozen artifact identities.

The Planner must not be a dependency of the Verifier.

## 9. Level 4 extension rule

Spiral 5 is expected to require a real temporal capability extension even though Spiral 4 already supplies indirect Dispatch.

The minimal target capability is:

```text
verified Temporal History Flow
+ explicit history role and generation
+ verified update/hold argument generation
+ explicit retained-history completion semantics
+ mechanical Backend mapping
+ whole-composition Recovery
```

The exact binary layout and version are not declared by CU1.

Runtime and Backend are forbidden to decide:

- whether an invocation is update or hold,
- update interval or schedule,
- Candidate kind,
- history materialization position,
- whether hierarchy or Consumer should execute,
- whether previous output may remain authoritative,
- history generation or validity,
- Resource state or synchronization,
- whether stale history may be reused.

## 10. Observation Contract

Qualification compares every Candidate with an independent CPU reference and pairwise with every other Candidate for every invocation.

It records:

- schedule and Candidate identity,
- invocation index, update event, and source generation,
- authorized history generation and history role,
- hierarchy and Consumer dispatch counts,
- exact output bytes or numeric error,
- output digest per invocation,
- hold-stability relation to the most recent update,
- non-finite and homogeneous-coordinate validity,
- first mismatch index,
- max absolute, relative, and ULP error,
- stale-history and cross-generation rejection,
- deterministic evidence digest.

For Candidate C, a hold invocation must prove that no unauthorized output write occurred and that the retained output remains byte-identical to the most recent update output.

The initial invalid-history state is a first-class negative case.

## 11. Canonical corpus

### Periodic measurement schedules

```text
P1:  U=1
P2:  U=2
P4:  U=4
P8:  U=8
P16: U=16
P32: U=32
P64: U=64
```

All use timeline length 128 and update at frame 0.

### Non-periodic qualification schedules

```text
BurstThenHold:
  0, 1, 2, 3, 64, 65, 127

Irregular:
  0, 2, 5, 11, 17, 29, 47, 79, 127
```

Every update begins one new deterministic Local Motor generation. Every hold repeats the latest generation bytes exactly.

## 12. Recovery and determinism

The whole Spiral 5 Composition is one Recovery unit.

Recovery must destroy or invalidate:

- materialized history Resources,
- history-generation bindings,
- temporal indirect argument Buffers,
- temporal completion tokens,
- endpoint state and readback state.

After a new device epoch:

- old history Resources, handles, tokens, and submissions are rejected,
- dynamic temporal inputs require explicit rebind,
- history starts invalid,
- one explicit update reseeds history,
- the same Frozen Temporal Composition reproduces the same WARP timeline.

Canonical contract, schedule, Candidate, Verified Plan, Frozen artifact, and architecture evidence bytes must be deterministic across fresh Debug A, Debug B, and Release.

## 13. Real GPU experiment

Architecture completion precedes hardware measurement.

The primary independent variable is `U`. The Candidate variable is history materialization position.

Evidence separates:

- temporal input preparation,
- update/hold argument generation,
- hierarchy execution,
- Consumer execution,
- retained-history synchronization/completion,
- command recording,
- submission and fence wait,
- numeric observation.

The Decision Report classifies:

- per-`U` winner set,
- A versus B transition or no transition,
- B versus C transition or no transition,
- best observed history materialization position,
- stable, unstable, multiple-transition, or noise-equivalent regions.

No observed crossover is a valid result. No performance result authorizes Runtime adaptive selection.

## 14. Explicit non-goals

Spiral 5 V1 does not add:

- interpolation, extrapolation, motion prediction, or approximation,
- multiple history generations, ring Buffers, or arbitrary temporal queries,
- omitted Motor payloads on hold invocations,
- variable Work count or sparse Work selection,
- representation-path comparison,
- variable hierarchy topology,
- Texture, Vertex/Raster, or image history,
- game-derived update policy, distance LOD, visibility policy, or priority,
- Runtime Variant selection or general Conditional Region,
- multiple DeviceDomain, partial recovery, streaming, or residency,
- a universal rule that history reuse is faster.

## 15. Completion units

- CU1: Owner selection, baseline inventory, research contract, schedules, boundaries, proof ledger; no C++ or ABI change.
- CU2: one `GlobalMotorHistoryReuse` architecture and the minimal versioned temporal extension.
- CU3: independent authority, mutation gates, context-bound seals, generation-bound history identities.
- CU4: A/B/C Candidate family and exact update/hold execution rules.
- CU5: complete WARP schedules, hold-stability proof, fresh-process determinism, controlled/actual Recovery.
- CU6: real-GPU measurement, binary evidence, update-interval crossover and history-position Decision Report.

## 16. Completion invariants

| ID | Invariant |
|---|---|
| S5-I01 | One Frozen experiment represents 128 ordered invocations with fixed graph shape and fixed Work count. |
| S5-I02 | Invocation 0 is an update; history is invalid before it succeeds. |
| S5-I03 | Source generation increments exactly once on update and remains unchanged on hold. |
| S5-I04 | Hold Motor bytes are byte-identical to the most recent update generation. |
| S5-I05 | The exact temporal meaning is piecewise constant; interpolation is not implicit. |
| S5-I06 | A/B/C produce identical output bytes or qualified numeric observations for every invocation. |
| S5-I07 | A executes hierarchy and Consumer every invocation. |
| S5-I08 | B executes hierarchy only on update and Consumer every invocation. |
| S5-I09 | C executes hierarchy and Consumer only on update and retains final output on hold. |
| S5-I10 | Update/hold Dispatch arguments follow one independently rederived rule. |
| S5-I11 | Runtime and Backend make no update, hold, Candidate, or history-placement decision. |
| S5-I12 | Raw Candidates cannot execute, Freeze, or mint Verified authority. |
| S5-I13 | History identity binds role, source generation, device epoch, Target, and actual Resource. |
| S5-I14 | Stale, cross-role, cross-Candidate, and cross-generation history is rejected. |
| S5-I15 | Recovery invalidates history and requires explicit reseed by an update invocation. |
| S5-I16 | Contract and architecture evidence are deterministic across fresh Debug/Release processes. |
| S5-I17 | Real-GPU evidence separates update cost, hold cost, hierarchy, Consumer, and completion. |
| S5-I18 | Measurement never authorizes adaptive Runtime selection. |

## 17. Completion declarations and Owner boundary

CU5 may declare:

```text
SGE4-5 SPIRAL 5 ARCHITECTURE COMPLETE
```

CU6 may declare:

```text
SGE4-5 SPIRAL 5 EXPERIMENT COMPLETE
```

CU6 must retain:

```text
RuntimePolicyAuthorization = None
NextCapabilitySelection = DeferredByOwner
SelectionStatus = OWNER_DECISION_REQUIRED
```

The `SPIRAL 5 CLOSED` banner remains withheld until the Owner selects the next capability or explicitly closes the research line.
