# Spiral 7 CU6-2 — Paired Decision Authority and High-Transition Refinement

## Why CU6-2 exists

CU6-1 successfully captured the complete 4 x 5 x 8 real-GPU surface, but its `winner_set` was derived from per-Candidate medians. The first Owner result showed that a median-only winner could disagree with the six-order paired observations. Therefore median ranking is retained only as a descriptive statistic.

Reinterpreting the supplied `decision_cases_v2.csv` under the strict paired rule gives 20 zero-dispatch-equivalent cases, 25 stable B winners, 29 stable B/C equivalent sets, and 86 unresolved cases. It gives no stable A or C winner. CU6-2 therefore makes paired observations authoritative and adds a focused high-Transition pass where Candidate A began to recover competitiveness.

## Decision authority

A Candidate is a `StableWinner` only when it wins the paired decision against both alternatives under the configured noise floor and minimum pair agreement.

A pair is a `StableEquivalentSet` only when the pair is noise-equivalent and both members beat the outside Candidate. All other cases are `Unresolved`.

`T=0` is classified independently as `B/C ZeroDispatchEquivalent`. B and C both have zero dispatch, so timestamp-resolution upper bounds cannot authorize either one as faster. These cases are excluded from crossover analysis.

Median ordering is printed and exported as `median_descriptive_set`, but it cannot authorize a winner, equivalent set, or crossover.

## Measurement passes

### CanonicalSurface

- Patterns: 4
- Active counts: 64, 256, 1024, 2048, 4096
- Transition counts: 0, 1, 8, 32, 64, 256, 1024, 4096
- Cases/run: 160
- Default: 2 runs, 1 cycle/order, 128 iterations

### HighTransitionRefinement

- Patterns: 4
- Active counts: 64, 256, 1024, 2048, 4096
- Transition counts: 1024, 1536, 2048, 3072, 4096
- Cases/run: 100
- Default: 3 runs, 3 cycles/order, 256 iterations

The refinement pass supplies 54 paired observations per Candidate pair at each coordinate. In the combined Decision Map, refinement observations replace canonical observations at duplicate T=1024 and T=4096 coordinates.

## Crossover policy

Only resolved `StableWinner` and `StableEquivalentSet` coordinates participate. `ZeroDispatchEquivalent` and `Unresolved` coordinates are skipped. A reported crossover is therefore a bracket between consecutive resolved coordinates, not a claim that the exact boundary lies at either endpoint.

## Authority boundary

The output remains adapter-, driver-, Shader-, and measurement-profile-specific. Runtime Candidate-policy authorization remains `None`; no universal winner is permitted.
