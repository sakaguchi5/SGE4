# R5 recovery state machine V1

```text
Uninitialized
  -- explicit Adapter + R3 Frozen Composition --> ActiveNeedsExternalRebind

ActiveNeedsExternalRebind
  -- explicit current-epoch binding set --> Active

Active
  -- controlled rebuild --> Lost
  -- actual removal --> AwaitingAdapter

Lost
  -- same explicit Adapter reacquired, epoch+1, full rematerialization
      --> ActiveNeedsExternalRebind

AwaitingAdapter
  -- no eligible explicitly ordered candidate --> AwaitingAdapter
  -- eligible non-excluded candidate, epoch+1, full rematerialization
      --> ActiveNeedsExternalRebind
```

After every transition to `ActiveNeedsExternalRebind`, submission is forbidden until rebind. The first later submission must be an R4 `RecoverySeed`; `InitialSeed` and `ContinueHistory` are rejected at that boundary.
