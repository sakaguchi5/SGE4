# Spiral 5 CU5 Recovery model

## Recovery unit

The whole Temporal Candidate family is one Recovery unit. The frozen Semantic, schedule corpus, Verified Plans and Candidate binding-set identity survive Recovery. Materialized device state does not.

## Invalidated state

Recovery invalidates or destroys:

- Global Motor and Final Output history Resources,
- Temporal indirect argument Buffers,
- submission and retained-history handles,
- completion state,
- readback state,
- current-epoch dynamic input binding,
- generation-zero history seed authority.

## Controlled Recovery

Controlled Recovery performs:

```text
Active epoch E
  -> release WARP Device and all execution objects
  -> recreate WARP execution context
  -> epoch E + 1
  -> DynamicInputsBound = false
  -> HistorySeeded = false
```

The following are mandatory after the epoch change:

1. old epoch handles, completion tokens and retained-history handles are rejected;
2. dynamic temporal inputs are rebound for the new epoch;
3. submission remains forbidden until an explicit generation-zero reseed is issued;
4. the first invocation of every frozen schedule is still an Update of generation zero;
5. the post-Recovery timeline reproduces the pre-Recovery Observation.

## Actual removal

The actual removal path invokes `ID3D12Device5::RemoveDevice` on the same WARP Device owned by the Temporal qualification Runtime.

After removal:

```text
State = AwaitingAdapter
DeviceEpoch = unchanged
RemovedAdapterLuid = excluded
```

All old handles, tokens, histories and submissions are rejected while quarantined. Retry must not reacquire the same removed WARP LUID in the same process.

## Authority boundary

Recovery does not choose Update frequency, Candidate kind or history placement. These remain Frozen Verified authority. Recovery only invalidates physical state and requires explicit rematerialization, rebind and reseed.
