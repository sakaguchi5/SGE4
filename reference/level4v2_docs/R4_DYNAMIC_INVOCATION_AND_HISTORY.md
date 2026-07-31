# Level 4 v2 R4 — Dynamic invocation and history

R4 reconstructs one exact authority boundary for indirect quantity, temporal history, sparse membership and delta transition. It adds no native execution.

```text
R3 Opaque Frozen Composition
        + explicit current membership / modified survivors / timeline / epoch
        + optional verified previous history
        ↓
Raw Dynamic Invocation Request
        ↓
Planner proposal
        ↓
Planner-independent reconstruction
        ↓
Verified Dynamic Invocation
        ↓
Frozen Invocation + verified next History
```

## Exact algebra

For a continuing invocation:

```text
N_t = A_t \ A_(t-1)                         activation
R_t = A_(t-1) \ A_t                         deactivation
W_t = N_t ∪ M_t                             update
H_t = (A_t ∩ A_(t-1)) \ M_t                retain
T_t = W_t ∪ R_t                             transition / exact write set
```

`M_t` must be a subset of the survivors. `T_t` is the only incremental write authority. Every retained member is excluded from `T_t`; steady retention therefore has verified indirect quantity zero.

Initial and Recovery seed modes accept no previous history and produce:

```text
Previous = empty
Update = Active
Retain = empty
Deactivation = empty
Transition = Active
```

## History identity

The next history is created only from a Verified Dynamic Invocation and binds:

- Frozen Composition identity;
- Semantic identity;
- source Invocation identity;
- monotonically advanced history generation;
- Device epoch;
- exact Active-set identity;
- full per-item generation-vector identity.

A continuing invocation rejects history from another Composition, Semantic or Device epoch. R4 defines the Recovery seed contract, but actual Device loss and rematerialization remain R5 responsibilities.

## Representation boundary

The canonical sets and their identities are meaning. Sorted Update/Clear transition records are one verified representation. Planner and Verifier construct these records independently. No Runtime or Backend may infer membership, modify the set algebra, or select a representation Candidate in R4.
