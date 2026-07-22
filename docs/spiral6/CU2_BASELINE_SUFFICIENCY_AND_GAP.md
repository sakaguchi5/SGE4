# Spiral 6 CU2 baseline sufficiency and gap

## Reused capability

The baseline already provides:

- fixed-capacity Buffer Resources,
- deterministic Dynamic Data and Frozen artifacts,
- D3D12 Compute dispatch,
- Spiral 4 verified zero/nonzero Dispatch machinery,
- Spiral 5 epoch and Recovery infrastructure,
- a 4096-record point-transform universe and Direct PGA Consumer.

These are sufficient for the physical Buffer, Dispatch, and Observation mechanisms.

## Missing authority

The baseline does not represent one arbitrary non-prefix set `S` as a first-class exact Semantic input. Spiral 4 represents only:

```text
S = [0,Nf)
```

An active count alone cannot distinguish two sets with the same `K = |S|` but different members. Existing Package and Composition contracts also do not bind:

- a canonical strictly increasing Sparse set identity,
- a compact universe-index list role,
- exact original-index write authority,
- inactive sentinel preservation,
- rejection of cross-set artifact replay.

The Temporal extension is orthogonal: it identifies generations and history lifetime, not arbitrary Work membership.

## Minimal extension

CU2 therefore adds:

```text
VersionedSidecarSparseExtensionV1
```

It contains one fixed-capacity compact index representation and binds it to:

- the exact Sparse Semantic identity,
- exact set identity,
- `CompactIndexListDispatch`,
- `CompactSortedUniverseIndexList`,
- `ceil(K/64)` Dispatch authority,
- Direct PGA Consumer Program identity,
- exact-write-set Observation Contract,
- the proven Spiral 4 Dispatch contract identity.

Schema 17, Runtime 17, the canonical D3D12 Backend, and the Composition Contract remain byte-unchanged.
