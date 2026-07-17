# SGE4 Level 4 v1 Qualification

## Formal entry point

Run from a Visual Studio developer-capable Windows environment:

```bat
tests\run_freeze.bat
```

This invokes `run_sge4_level4v1.bat`. The run first checks architectural dependencies, script contracts, and `SOURCE_MANIFEST.sha256`; then builds and executes the complete Debug and Release regression set; finally it emits Frozen Composition artifacts in separate processes and compares Debug-A, Debug-B, and Release byte-for-byte.

## Level 4 v1 evidence

- `46_CompositionContractTests`: Schema-17 endpoint extraction, invalid endpoint rejection, and insertion-order-independent contract identity.
- `47_LinkPlanningTests`: deterministic proposal, independent seal, Frozen Composition read/write identity, corruption rejection, and optional artifact emission.
- `48_LinkAuthorityTests`: raw-plan non-constructibility, corrupt identity, schedule, binding, state, and extracted-Leaf-interface mutation rejection.
- `49_CompositionRuntimeTests`: independent Leaves, a WARP diamond DAG with producer fan-out and a two-input merge, a headless-producer-to-single-presenter DAG, shared DeviceDomain Buffers, cross-Package tokens, readback, controlled whole-domain recovery, stale epoch observation, actual `RemoveDevice`, inactive-domain submission rejection, and Frozen Composition fresh-process rematerialization.

The existing Stage 0D suites remain in the same run and retain the 54 canonical Leaf Package and planning-manifest comparisons. Schema 17 and Runtime 17 are not versioned or rewritten by Level 4 v1.

## Pass condition

Only a successful command exit and the following terminal banner constitute the Level 4 v1 freeze:

```text
SGE4 LEVEL 4 V1 STATIC VERIFIED PACKAGE COMPOSITION FREEZE PASSED
```
