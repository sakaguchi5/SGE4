# SGE4-5 Spiral 6 completion specification v0.1

- Status: Proposed Frozen Research Contract
- Completion Unit: CU1
- Baseline commit: `46554ab55e532c438c9c4214ff1df3e7cd68638e`
- Title: Exact Sparse Work-Set Flow and Verified Sparse Lowering
- Japanese title: 正確な疎Work集合Flowと疎実行表現の検証済みLowering
- Baseline ABI: D3D12 Schema 17 / Runtime 17
- Available extensions: Spiral 4 indirect sidecar V1 and Spiral 5 temporal sidecar V1
- CU1 ABI mutation: none

## 0. Decision

Spiral 6 removes one fixed assumption that remained through Spiral 5:

> Active Work is no longer required to be either the complete 4096-record universe or one contiguous prefix `[0,Nf)`.

Spiral 6 V1 receives one exact subset `S` of a fixed Work universe. The set may be non-contiguous. The Semantic meaning is the set itself, not the input ordering and not a density estimate.

The formal V1 sparse-rule identifier is `ExactSparseWorkSetV1`.

Spiral 6 asks:

> For one exact sparse subset of a fixed 4096-record point-transform universe, should membership be represented as a dense mask, an exact compact index list, or an active-block list with local masks, without returning set selection, compaction, or Candidate choice to Runtime or Backend?

The purpose is not to make sparse execution win. The purpose is to make exact non-prefix Work membership and sparse representation granularity verified Level 4/5 dimensions.

## 1. Why Spiral 6 follows Spiral 5

Spiral 4 proved dynamic active quantity with one prefix set:

```text
S_prefix(Nf) = { i | 0 <= i < Nf }
```

That is not an arbitrary sparse set. Two sets with the same cardinality can require different memory access and dispatch behavior.

Spiral 5 proved temporal reuse while keeping Work count and active Work count equal to 4096. Its explicit non-goals included variable Work count and sparse Work selection.

Spiral 6 changes the structural variable from time to exact membership:

```text
Spiral 5
  fixed complete Work set
  + variable update interval

Spiral 6
  one fixed invocation
  + fixed 4096-record universe
  + externally supplied exact subset S
  + verified sparse representation choices
```

No game culling or visibility policy is inferred. The exact set is an external verified input.

## 2. Exact sparse meaning

Let:

```text
N = 4096
U = {0, 1, ..., 4095}
S subseteq U
K = |S|
```

`S` is mathematically unordered. Its canonical byte encoding is the strictly increasing sequence of all member indices.

The contract rejects:

- duplicate indices,
- indices outside `[0,4095]`,
- a declared `K` different from the sequence length,
- a set identity different from the canonical bytes,
- a derived representation that denotes another set.

The Runtime may not add, remove, reorder semantically, or infer members.

## 3. Fixed mathematical and representation baseline

Spiral 6 isolates sparse membership representation. It does not cross-product Sparse Candidates with Temporal Candidates or representation-path Candidates.

The fixed workload is:

```text
one canonical eight-Global-Motor palette
  + 4096 canonical point-transform records
  + exact sparse set S
  -> transform records whose universe indices are in S
  -> preserve canonical sentinel bytes outside S
  -> full 4096-record output in universe-index order
```

The following remain Frozen:

- eight-bone Direct PGA Motor convention,
- Global Motor palette generation = 0,
- representation path `DirectPgaThroughConsumerV1`,
- Work universe and output capacity = 4096,
- thread-group size = 64,
- point corpus and output schema,
- full output order,
- inactive sentinel `float4(0,0,0,0)`,
- one invocation and no Temporal History dimension,
- Composition graph shape and Recovery unit.

Every active output must have homogeneous `w = 1`. Every inactive output must remain byte-identical to the sentinel.

## 4. Baseline Level 4 inventory

At baseline commit `46554ab55e532c438c9c4214ff1df3e7cd68638e`:

- Spiral 4 supplies verified active-prefix counts and indirect Dispatch,
- Spiral 5 supplies verified Temporal History Flow,
- fixed-capacity Buffer flows and whole-domain Recovery exist,
- no arbitrary exact Sparse Work Set contract exists,
- no canonical sparse membership artifact exists,
- no compact exact index-list role exists,
- no active-block-list plus local-mask role exists,
- no general proof exists that the GPU write set equals an arbitrary `S`,
- no general inactive-sentinel authority exists outside prefix semantics.

CU1 records this inventory but does not modify C++, the Solution, Package ABI, Runtime, Backend, Spiral 4 indirect artifacts, or Spiral 5 temporal artifacts.

A later CU may add one versioned sparse sidecar only after proving that existing contracts cannot represent exact non-prefix membership and candidate-specific derived representations without hidden Runtime or Backend judgment.

## 5. Candidate Lowerings

All Candidates receive the same canonical set `S`, Global Motors, points, output capacity, sentinel initialization, and Observation Contract.

### A. DenseMembershipMaskPredicate

Derived representation:

```text
4096 membership bits = 512 bytes
```

Execution:

```text
64 groups x 64 threads = 4096 threads
thread i tests membership bit i
if i in S: write transformed output[i]
if i not in S: perform no output write
```

This is the dense control Candidate.

### B. CompactIndexListDispatch

Derived representation:

```text
strictly increasing uint32 index list
length K
fixed capacity 4096 indices = 16384 bytes
```

Execution:

```text
dispatch groups = ceil(K / 64), or zero when K = 0
thread j < K reads index = list[j]
writes transformed output[index]
```

### C. ActiveBlockListLocalMask

Partition the universe into 64 blocks of 64 indices.

For each active block, derive one canonical record:

```text
uint32 blockId
uint32 reserved = 0
uint64 localMembershipMask
```

Records are sorted by `blockId` and have fixed capacity 64 records = 1024 bytes.

Execution:

```text
one 64-thread group per active block
lane l tests bit l in the block-local mask
active lanes write output[blockId * 64 + l]
inactive lanes perform no output write
```

## 6. Sparse corpus

Four exact pattern constructors are Frozen.

### PrefixControl

```text
S = {0, ..., K-1}
```

This connects Spiral 6 to Spiral 4 and is not the only Sparse meaning.

### UniformStride

For `K > 0`:

```text
S = { floor(j * 4096 / K) | 0 <= j < K }
```

The formula yields `K` unique increasing indices.

### BlockClusteredPermutation

Blocks are visited in the permutation:

```text
block(j) = (17*j + 5) mod 64
```

Complete 64-element blocks are filled in this order. A remainder fills the lowest lanes of the next block. Because `gcd(17,64)=1`, every block appears exactly once.

### HashScatterPermutation

Take the first `K` values of:

```text
p(j) = (4051*j + 17) mod 4096
```

and sort them into canonical set order. Because `4051` is odd, this is a permutation of the 4096-element universe.

Qualification cardinalities are:

```text
K = 0,1,2,31,32,63,64,65,127,128,129,
    255,256,257,511,512,1024,2048,3072,4095,4096
```

Measurement cardinalities are:

```text
K = 1,8,32,64,128,256,512,1024,2048,3072,4096
```

## 7. Output and write-set authority

Before Candidate execution, every output record is initialized to `float4(0,0,0,0)`.

The independently verified legal write set is exactly `S`.

```text
i in S     -> output[i] equals the canonical point-transform result and w = 1
i not in S -> output[i] remains byte-identical to the sentinel
```

A Candidate fails if it:

- omits any active member,
- writes any inactive member,
- writes one active result to another universe index,
- produces a derived mask/list/block record for a different set,
- changes output capacity or order.

## 8. Authority chain

The only formal route is:

```text
Spiral6ExperimentSpec
  -> Canonical Sparse Semantic
  -> Verified Exact Sparse Work Set
  -> Raw Sparse Lowering Candidate
  -> independent Sparse Verifier
  -> opaque Verified Sparse Plan
  -> sparse sidecar/Leaf Compiler
  -> Frozen Sparse Artifact
  -> Frozen Composition
  -> epoch-bound representation and output bindings
  -> Runtime / D3D12 Executor
```

The Verifier must not depend on the Planner. It independently rederives:

- set cardinality, range, uniqueness, canonical order, and identity,
- the 4096-bit dense mask,
- the compact exact index list,
- active block IDs and all local masks,
- Candidate-specific Dispatch dimensions,
- the exact legal output write set,
- inactive sentinel preservation,
- fixed Resource capacities and roles,
- required states, synchronization, and completion,
- Target and actual materialized Resource identities,
- Recovery and device-epoch boundaries.

Raw Candidates cannot execute, Freeze, submit, mint Verified types, or replay another set's seal.

## 9. Level 4 extension rule

The minimal target capability is:

```text
verified Exact Sparse Work-Set Flow
+ canonical set identity
+ candidate-specific derived representation role
+ exact write-set authority
+ zero-work and fixed-capacity rules
+ mechanical Backend mapping
+ whole-composition Recovery
```

Runtime and Backend are forbidden to decide:

- set membership or cardinality,
- pattern/locality classification,
- dense versus compact versus block representation,
- compaction ordering,
- Dispatch dimensions,
- legal output write indices,
- whether inactive output may change,
- Resource role, state, synchronization, or stale-resource reuse.

## 10. Observation Contract

Qualification records:

- Sparse Semantic and exact set identity,
- pattern constructor and `K`,
- Candidate and derived representation identity,
- derived representation bytes and cardinality,
- expected and observed Dispatch dimensions,
- exact active output results,
- exact inactive sentinel bytes,
- first missing or unauthorized write index,
- pairwise Candidate output digest,
- non-finite and homogeneous-coordinate validity,
- deterministic evidence digest,
- stale set, cross-Candidate, cross-role, and cross-epoch rejection.

All three Candidates must produce byte-identical full output Buffers for the same `S`.

## 11. Recovery and determinism

Recovery invalidates:

- candidate-derived membership/index/block Buffers,
- Sparse Set and representation bindings,
- indirect argument Buffers,
- output initialization authority,
- completion and readback state.

After a new device epoch:

- old representation Resources, handles, tokens, and submissions are rejected,
- the exact set requires explicit rebind,
- derived representations are rebuilt only from Verified set authority,
- the output sentinel is explicitly re-established,
- the same Frozen Sparse Composition reproduces the same WARP evidence.

Contract, set, Candidate, Verified Plan, Frozen artifact, and architecture evidence bytes must be deterministic across fresh Debug A, Debug B, and Release.

## 12. Real-GPU experiment

Architecture completion precedes hardware measurement.

The independent variables are:

```text
K = sparse cardinality
Pattern = Prefix / UniformStride / BlockClustered / HashScatter
```

Evidence separates:

- sparse input validation,
- derived representation construction/upload,
- indirect argument generation,
- sparse Consumer execution,
- sentinel initialization and completion,
- command recording,
- submission/fence wait,
- numeric observation.

The Decision Report classifies:

- winner set per `(Pattern,K)`,
- A/B transition or no transition by density,
- B/C transition or no transition by active-block locality,
- stable, unstable, multiple-transition, or noise-equivalent regions,
- best observed sparse representation granularity.

No measurement authorizes Runtime adaptive selection.

## 13. Explicit non-goals

Spiral 6 V1 does not add:

- game visibility, culling, distance, priority, or LOD policy,
- approximate omission or probabilistic membership,
- dynamic Resource capacity,
- unordered duplicate-preserving work queues,
- variable output schema or compacted output order,
- temporal update schedules or history reuse comparison,
- hierarchy/representation-path comparison,
- Texture, Raster, meshlet, BVH, streaming, or residency policy,
- multiple DeviceDomains or partial recovery,
- Runtime Candidate selection,
- a universal rule that sparse dispatch is faster.

## 14. Completion units

- CU1: Owner selection, Spiral 5 closure, baseline inventory, exact set and corpus contract, boundaries, proof ledger; no C++ or ABI change.
- CU2: one `CompactIndexListDispatch` architecture and the minimal versioned Sparse Work-Set extension.
- CU3: independent set/representation authority, mutation gates, context-bound seals and epoch-bound Resources.
- CU4: A/B/C Sparse Candidate family and exact write-set equivalence.
- CU5: complete WARP corpus, inactive-sentinel proof, fresh-process determinism, controlled/actual Recovery.
- CU6: real-GPU `(Pattern,K)` measurement, binary evidence, sparse crossover and granularity Decision Report.

## 15. Completion invariants

| ID | Invariant |
|---|---|
| S6-I01 | One Frozen experiment has a fixed 4096-record universe, fixed output capacity, and one exact set `S`. |
| S6-I02 | Canonical set bytes are strictly increasing, unique, in range, and bind exact cardinality `K`. |
| S6-I03 | Set semantics are independent of external input order. |
| S6-I04 | A/B/C derived representations denote exactly the same `S`. |
| S6-I05 | A dispatches the full universe and predicates by the verified dense mask. |
| S6-I06 | B dispatches exactly the compact list length and writes original universe indices. |
| S6-I07 | C dispatches exactly active blocks and predicates by verified local masks. |
| S6-I08 | The legal output write set is exactly `S`. |
| S6-I09 | Every inactive output remains byte-identical to the canonical sentinel. |
| S6-I10 | Every active output matches the CPU reference and has homogeneous `w = 1`. |
| S6-I11 | A/B/C full output Buffers are byte-identical for every qualified set. |
| S6-I12 | Runtime and Backend make no membership, pattern, Candidate, or compaction decision. |
| S6-I13 | Raw Candidates cannot execute, Freeze, or mint Verified authority. |
| S6-I14 | Set and representation identities bind role, Target, actual Resources, and device epoch. |
| S6-I15 | Stale, cross-set, cross-role, cross-Candidate, and cross-epoch Resources are rejected. |
| S6-I16 | Recovery requires explicit set rebind, representation rebuild, and sentinel reinitialization. |
| S6-I17 | Contract and architecture evidence are deterministic across fresh Debug/Release processes. |
| S6-I18 | Real-GPU evidence separates representation construction, sparse execution, and completion. |
| S6-I19 | Measurement never authorizes adaptive Runtime selection. |

## 16. Completion declarations and Owner boundary

CU5 may declare:

```text
SGE4-5 SPIRAL 6 ARCHITECTURE COMPLETE
```

CU6 may declare:

```text
SGE4-5 SPIRAL 6 EXPERIMENT COMPLETE
```

CU6 must retain:

```text
RuntimePolicyAuthorization = None
NextCapabilitySelection = DeferredByOwner
SelectionStatus = OWNER_DECISION_REQUIRED
```

The `SPIRAL 6 CLOSED` banner remains withheld until the Owner selects the next capability or explicitly closes the research line.
