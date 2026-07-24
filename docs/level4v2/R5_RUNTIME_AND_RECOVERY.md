# Level 4 v2 R5 Runtime and Recovery

R5 is the first reconstruction stage that adds execution lifecycle authority. It consumes, but never creates, R2/R3/R4 decisions.

```text
R3 Opaque Frozen Composition
  -> explicit Adapter identity
  -> native materialization
  -> explicit external-state rebind

R4 Opaque Frozen Dynamic Invocation
  -> exact verified write-set identity
  -> exact verified indirect work count
  -> submission and completion handles
```

All live representation, History, completion and submission handles carry one Device epoch. A controlled whole-composition rebuild releases all runtime objects, advances the epoch, rematerializes the same Frozen Composition, clears accepted History and requires explicit external rebind. The first accepted invocation after that transition must expose the verified R4 `RecoverySeed` mode.

Forced removal excludes the removed Adapter identity. The runtime enters `AwaitingAdapter`, rejects submission and preserves that state when the explicitly ordered reacquisition candidates contain no eligible non-excluded Adapter.

The Canonical runtime is Backend-neutral. The Windows qualification uses the accepted Level 4 v1 D3D12/WARP runtime as a retained Backend primitive proof. Equivalence between every R3 Frozen Composition and the retained serialized Composition corpus is an R6 migration obligation, not an implicit R5 claim.
