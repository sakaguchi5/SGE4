# R5 changeset

- adds six R5 Projects: Backend-neutral model/materialization/submission/recovery, deterministic tests and Windows WARP qualification;
- adds a read-only verified `InvocationModeV1` projection to the R4 Frozen Invocation so R5 can enforce `RecoverySeed` exactly;
- adds epoch-bound handle validation, explicit external rebind, whole-composition invalidation/rematerialization, removed-Adapter exclusion and AwaitingAdapter preservation;
- updates the accepted R4 handoff and advances the reconstruction input to R6;
- does not delete reference Projects, alter Schema 17/Runtime 17, or authorize Runtime performance policy.
