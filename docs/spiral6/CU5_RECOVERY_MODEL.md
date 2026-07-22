# Spiral 6 CU5 Recovery model

## Owned current-epoch state

The Sparse Candidate Family Runtime owns or authorizes:

- the WARP Device and command execution context,
- current Exact Sparse Set binding,
- Dense Membership Mask representation,
- Compact Sorted Index List representation,
- Active Block List and local masks,
- ExecuteIndirect arguments,
- completion-token state,
- readback and Observation state.

## Controlled Recovery

```text
Active epoch E
  -> release Device and every current-epoch execution object
  -> create a new WARP execution context
  -> epoch E+1
  -> no Sparse Set bound
  -> no Derived Representation built
```

The Verified Candidate corpus and its identity remain unchanged. Materialized Resource authority does not.

Old handles are rejected by epoch before use. The new epoch requires both explicit steps:

1. bind one externally verified Exact Sparse Set;
2. rebuild all three Candidate-specific Derived Representations.

A set rebind alone never authorizes stale representation reuse.

## Actual Device Removal

`ForceRemovalForTest` queries `ID3D12Device5`, calls `RemoveDevice`, captures the removal reason and removed Adapter LUID, releases native objects, and enters:

```text
AwaitingAdapter
```

The device epoch is not advanced while the Runtime is quarantined. Retry may not reacquire the excluded removed WARP LUID.

## Policy boundary

Recovery does not choose Sparse membership, cardinality, pattern, Candidate kind or winner. `RuntimePolicyAuthorization` remains `None`.
