# Spiral 4 CU5 — Candidate-family Recovery model

## Device epoch objects

The runtime exposes three epoch-bound values:

```text
CandidateFamilyEpochHandleV1
CandidateFamilyDynamicBindingV1
CandidateFamilySubmissionTokenV1
```

Each carries or is cryptographically bound to the active device epoch and
Frozen Candidate-family domain.

## Controlled rebuild

```text
Active epoch e
  -> release native Device and all transient execution objects
  -> reacquire WARP
  -> epoch e+1
  -> DynamicInputsBound = false
  -> explicit rebind
  -> first current-epoch submission rematerializes Command Signatures and Buffers
  -> regenerate Single and Batched argument records
  -> reproduce identical observations
```

Old handles and submission tokens fail with `candidate-runtime/stale-epoch`.
A current handle paired with an old dynamic binding fails with
`candidate-runtime/rebind-required`.

## Actual removal

```text
Active epoch e
  -> ID3D12Device5::RemoveDevice
  -> capture removal reason and DRED counts
  -> exclude removed adapter LUID
  -> release device objects
  -> AwaitingAdapter at epoch e
```

The epoch does not advance because no replacement device was acquired. Old
handles, tokens, and submit attempts fail at the stronger
`candidate-runtime/device-state` boundary.

Retry in the same single-WARP process preserves the LUID exclusion and remains
`AwaitingAdapter`. A separate fresh process has no inherited exclusion and may
rematerialize the same Frozen Candidate family.

## Recovery unit

The Recovery unit is the complete seven-Candidate family. Candidate identities,
Verified Plans, certificates, sidecar artifacts, and Frozen bindings survive.
Native device objects, Command Signatures, argument Buffers, endpoint states,
and per-submission tokens do not survive.
