# Spiral 7 CU2 baseline sufficiency and cross-capability gap

## Baseline sufficiency

The accepted baseline already supplies three independently verified capabilities:

- Spiral 4: an identity-bound Dispatch command signature and verified ExecuteIndirect quantity,
- Spiral 5: whole-role Temporal History, generation and update/hold meaning,
- Spiral 6: arbitrary exact Sparse membership, canonical set identities and exact Sparse write-set authority.

These capabilities remain valid and are reused. CU2 does not replace them.

## Why their independent composition is insufficient

The three sidecars do not jointly denote one per-invocation partial-history transition.

Spiral 5 history answers whether a whole history role is updated or held. It does not identify which universe records remain valid.

Spiral 6 Sparse membership answers which records are active in one invocation. It deliberately excludes Temporal History and therefore does not prove whether a surviving active record may be retained from a previous invocation.

Spiral 4 ExecuteIndirect supplies a physical Dispatch quantity, but it does not define the semantic relation between that quantity and update, clear and retain actions.

The missing authority is therefore not another independent count or set. It is the relation:

```text
A_(t-1), A_t, M_t
    -> N_t activation
    -> R_t deactivation
    -> W_t update
    -> H_t retain
    -> T_t physical transition
```

Without a new relation-bound artifact, Runtime or Backend would have to decide at least one of the following:

- whether a surviving record is valid history,
- whether a record must be updated or cleared,
- whether an inactive record may retain stale bytes,
- which exact records contribute to indirect Dispatch quantity,
- whether a transition artifact belongs to the current invocation.

Those decisions are forbidden.

## Minimal CU2 extension

CU2 introduces one sidecar only:

```text
VersionedSparseTemporalDeltaExtensionV1
```

It contains a canonical fixed-capacity list of sorted action records:

```text
uint32 universeIndex
uint32 action       // 1 Update, 2 Clear
```

The active prefix of this list denotes exactly:

```text
T_t = W_t union R_t
```

Previous output bytes initialize the output resource. The GPU writes Update results for `W_t`, the canonical zero sentinel for `R_t`, and performs no write to `H_t`.

## Preserved boundary

CU2 changes none of the following:

- D3D12 Schema 17,
- Runtime 17,
- canonical D3D12 Backend,
- canonical Composition Contract,
- Spiral 4, 5 or 6 artifact bytes,
- Runtime policy authorization.

The new sidecar is an experimental Level 4 v2 input, not Level 4 v2 Canonical ABI.
