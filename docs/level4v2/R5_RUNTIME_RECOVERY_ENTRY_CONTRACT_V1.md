# Level 4 v2 R5 Runtime and Recovery entry contract V1

R5 may begin only after R4 exact-set, History replay, proposal mutation and deterministic Evidence gates pass.

R5 must materialize and submit only R2/R3/R4 Frozen authority. It may not create Candidate, Composition or Dynamic decisions.

Required responsibilities:

- one Device epoch for all live representation, History, completion and submission handles;
- stale-handle and stale-History rejection;
- explicit external-state rebind after epoch change;
- whole-composition invalidation and rematerialization;
- first post-Recovery invocation as the R4 `RecoverySeed` full-active rule;
- removed-Adapter exclusion and `AwaitingAdapter` preservation;
- no Runtime Candidate-performance policy.

R5 remains one Adapter and whole-composition Recovery only. Partial Recovery, multi-device migration, streaming and residency remain outside the proven subset.
