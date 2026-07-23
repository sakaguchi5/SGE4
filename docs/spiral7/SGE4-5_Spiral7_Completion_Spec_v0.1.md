# SGE4-5 Spiral 7 completion specification v0.1

- Status: Proposed Frozen Research Contract
- Completion Unit: CU1
- Baseline commit: `d0bb0d406fca8beabed2331daff870ea414dd87d`
- Title: Versioned Sparse Delta Flow and Verified Incremental History Lowering
- Japanese title: 世代付き疎差分Flowと増分履歴再利用の検証済みLowering
- Baseline ABI: D3D12 Schema 17 / Runtime 17
- Available extensions: Spiral 4 indirect sidecar V1, Spiral 5 temporal sidecar V1 and Spiral 6 sparse sidecar V1
- CU1 ABI mutation: none

## 0. Decision

Spiral 7 removes the isolation boundary that remained through Spiral 6:

> Exact Sparse membership and Temporal History are no longer required to exist in separate experiments.

Spiral 7 V1 receives an exact Active Set and an exact set of modified surviving members for every invocation of a fixed timeline. It derives activation, deactivation, update, retain and physical-transition sets. No game policy and no Runtime policy is inferred.

The formal Semantic identifier is:

```text
VersionedSparseDeltaPointTransformTimelineV1
```

The research question is:

> For a fixed 4096-record point-transform universe with exact per-invocation Active membership and exact modified survivors, should each invocation fully rebuild current active output, update a compact exact transition list while retaining valid history, or update affected blocks with local update/clear masks, without returning membership, change detection, Candidate choice or history policy to Runtime or Backend?

The purpose is not to make incremental execution win. The purpose is to prove the final Sparse x Temporal x Indirect Level 4 authority relation needed before Level 4 v2 Canonical reconstruction.

## 1. Why Spiral 7 follows Spiral 6

Spiral 4 proved dynamic active quantity and ExecuteIndirect for one prefix Active Set. Spiral 5 proved history reuse with a complete 4096-record Work set. Spiral 6 proved arbitrary exact Sparse membership for one invocation and deliberately froze Temporal History.

The three results do not yet prove partial history validity. A whole-resource history generation does not say which universe indices are still active, which survived unchanged, which changed, or which must be cleared.

Spiral 7 therefore changes the structural variable from one exact set to an exact timeline of set transitions.

## 2. Exact transition meaning

Let:

```text
U = {0,1,...,4095}
A_t subseteq U
M_t subseteq A_t intersect A_(t-1)
```

`A_t` is the exact current Active Set. `M_t` contains surviving active members whose point generation changed at invocation `t`.

Derived sets are:

```text
N_t = A_t \ A_(t-1)                    activation
R_t = A_(t-1) \ A_t                    deactivation
W_t = N_t union M_t                    update
H_t = (A_t intersect A_(t-1)) \ M_t   retain
T_t = W_t union R_t                    physical transition
```

Required partitions are:

```text
W_t intersect H_t = empty
W_t intersect R_t = empty
H_t intersect R_t = empty
W_t union H_t = A_t
```

At the first invocation, previous membership is empty and history is invalid:

```text
W_0 = A_0
H_0 = empty
R_0 = empty
```

After a Device epoch change, old GPU history is stale. The first post-Recovery invocation forces:

```text
W_t = A_t
H_t = empty
```

## 3. Semantic authority

`A_t` and `M_t` are external verified inputs. They are canonical strictly increasing unique `uint32` index sequences.

The contract rejects:

- duplicate indices,
- indices outside `[0,4095]`,
- an `M_t` member that is not active both before and after the transition,
- a declared identity different from canonical bytes,
- derived transition sets that disagree with independent set arithmetic,
- update and clear overlap,
- retained history with the wrong per-item generation or Device epoch.

Runtime and Backend are forbidden to decide membership, change, action, Candidate or history policy.

## 4. Fixed workload

```text
one canonical eight-Global-Motor palette at generation 0
  + 4096 canonical point records
  + per-item exact generation
  + 128-invocation exact Active/Modified timeline
  -> full 4096-record output in universe-index order
```

A canonical point is an exact-binary function of universe index and per-item generation. Only members of `W_t` advance generation. This isolates partial temporal change while keeping the Direct PGA representation path fixed.

Frozen controls:

- eight-bone Direct PGA Motor convention,
- representation path `DirectPgaThroughConsumerV1`,
- Work universe and output capacity = 4096,
- thread-group size = 64,
- timeline length = 128,
- history depth = 1,
- fixed Buffer capacities,
- full output order,
- inactive sentinel `float4(0,0,0,0)`,
- whole-Composition Recovery,
- one DeviceDomain.

Every active output has homogeneous `w = 1`. Every inactive output is byte-identical to the canonical sentinel.

## 5. Candidate Lowerings

### A. FullActiveDenseRecompute

Derived representation:

```text
4096 Active membership bits = 512 bytes
```

Execution:

```text
64 groups x 64 threads
active lane   -> write current-generation transformed output
inactive lane -> write canonical sentinel
```

This is the full-state control Candidate. Its legal write set is the entire universe.

### B. CompactDeltaIndexHistoryReuse

Derived representation:

```text
sorted records over T_t
record = uint32 universeIndex + uint32 action
maximum 4096 records
fixed capacity = 32768 bytes
```

Actions are exactly `Update` for `W_t` and `Clear` for `R_t`.

Execution:

```text
dispatch groups = ceil(|T_t| / 64), or zero when T_t is empty
Update -> write current-generation transformed output[index]
Clear  -> write canonical sentinel to output[index]
H_t    -> no write; retain previous valid bytes
```

### C. AffectedBlockDeltaHistoryReuse

Partition the universe into 64 blocks of 64 indices. Each affected block record is:

```text
uint32 blockId
uint32 reserved = 0
uint64 updateMask
uint64 clearMask
```

`updateMask & clearMask` must be zero. Records are sorted by block id.

Execution:

```text
one 64-thread group per affected block
update lane -> write current-generation result
clear lane  -> write canonical sentinel
other lane  -> no write
```

## 6. Candidate equivalence

After every invocation, all Candidates must produce a byte-identical full 4096-record output.

For each universe index `i`:

```text
i in A_t -> output[i] equals current-generation canonical transform and w = 1
i not in A_t -> output[i] equals float4(0,0,0,0)
i in H_t -> bytes are retained from valid previous history and receive no write
```

A Candidate fails if it omits an update, retains a modified member, fails to clear a deactivation, writes a held member, reuses stale history, or denotes another transition set.

## 7. Corpus

Active membership reuses the four Spiral 6 exact constructors:

- PrefixControl
- UniformStride
- BlockClusteredPermutation
- HashScatterPermutation

Transition kinds are:

- Hold
- DirtyOnly
- ActivateOnly
- DeactivateOnly
- BalancedChurn
- PatternMigration
- FullInvalidation
- EmptyReset

The authoritative cardinalities and legality rules are in `CORPUS_V1.md` and `CONTRACT_MANIFEST_V1.json`.

## 8. Level 4 requirement

The existing sidecars are individually sufficient for their isolated experiments but are not assumed sufficient for partial history validity.

CU2 must first prove baseline insufficiency. Only then may it add:

```text
VersionedSparseTemporalDeltaExtensionV1
```

The minimal extension may represent only what is required for:

- per-invocation exact Active identity,
- exact modified-survivor identity,
- derived transition identity,
- per-item history validity/generation,
- exact update/clear action,
- Candidate-specific transition representation,
- Device epoch binding and forced post-Recovery rebuild.

It must not mutate D3D12 Schema 17, Runtime 17, Level 4 v1 Canonical bytes or earlier sidecar bytes.

## 9. Level 5 requirement

The Semantic result is fixed. Level 5 may compare only the verified materialization and incremental execution granularity:

```text
FullActiveDenseRecompute
CompactDeltaIndexHistoryReuse
AffectedBlockDeltaHistoryReuse
```

RuntimePolicyAuthorization = None

No measurement result by itself authorizes Runtime to inspect Active count, transition count, pattern or transition kind and select A/B/C.

## 10. Recovery

Recovery remains whole-Composition.

- old Device-epoch history is rejected,
- external `A_t` and `M_t` must be explicitly rebound,
- derived transition representations are rebuilt,
- the first post-Recovery invocation performs a full rebuild of current active members,
- inactive records return to the canonical sentinel,
- stale history may never be treated as retained `H_t`.

## 11. Completion Units

- CU1 — Research Contract Freeze
- CU2 — Sparse Temporal Delta Architecture
- CU3 — Independent Delta Authority
- CU4 — Incremental History Candidate Family
- CU5 — Architecture Qualification
- CU6 — Real-GPU Measurement and Decision Evidence

Successful CU5 may declare:

```text
SGE4-5 SPIRAL 7 ARCHITECTURE COMPLETE
```

Successful CU6 may declare:

```text
SGE4-5 SPIRAL 7 EXPERIMENT COMPLETE
```

Closure remains Owner-gated:

```text
SPIRAL 7 CLOSED
RuntimePolicyAuthorization = None
NextCapabilitySelection = DeferredByOwner
```

After Owner closure, the next program is Level 4 v2 Canonical reconstruction rather than another exploratory capability Spiral.

## 12. CU1 boundary

CU1 consists only of documents and verification. It does not add C++, projects, ABI fields, Runtime behavior or Backend behavior.

Runtime and Backend are forbidden to decide any Semantic or Candidate dimension.
