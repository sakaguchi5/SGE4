# Spiral 5 CU2 Global Motor History architecture

## Data path

```text
Verified Update Schedule record
  -> GPU Update/Hold Argument Producer
  -> Dispatch argument X = Update ? 1 : 0
  -> ExecuteIndirect hierarchy writer
  -> Global Motor History Buffer, depth 1
  -> fixed Direct-PGA Consumer Dispatch
  -> output readback and retained-history completion
```

Runtime supplies the frozen invocation record mechanically. It does not calculate Dispatch dimensions or decide Update versus Hold.

## Update invocation

```text
source generation g
GPU argument X = 1
hierarchy composes 8 Local Motors
Global Motor History becomes generation g
Consumer transforms 4096 points
Consumer completion authorizes retained history g
```

## Hold invocation

```text
source generation remains g
Local Motor bytes select the same canonical generation g
GPU argument X = 0
hierarchy executes zero groups
Global Motor History bytes remain unchanged
Consumer transforms 4096 points from retained history g
Consumer completion retains authority for history g
```

The zero Dispatch is explicit evidence. The Backend does not skip the hierarchy because it interpreted an update flag.

## CU2 WARP corpus

```text
P1:        128 updates
P4:         32 updates + 96 holds
P64:         2 updates + 126 holds
Irregular:   9 updates + 119 holds
Total:     512 ordered invocations per build
```

Every invocation validates:

- GPU-generated hierarchy Dispatch X,
- source/history generation,
- Global Motor history against an independent CPU hierarchy,
- byte stability on Hold,
- 4096 Consumer outputs against CPU reference,
- last-update-wins output identity,
- non-finite and homogeneous-coordinate validity,
- retained completion ordinal and native fence order.

Debug and Release emit one canonical architecture evidence bundle and must be byte-identical.

## Deliberately deferred

CU2 does not implement A or C, independent Planner/Verifier authority, Recovery execution, actual Device removal, or real-GPU measurement.
