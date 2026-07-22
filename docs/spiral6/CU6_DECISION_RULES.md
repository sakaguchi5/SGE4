# Spiral 6 CU6 — decision rules

## Primary value

For every `(Pattern,K)` and Candidate, CU6 takes the median of all `GpuCandidatePathNanoseconds` samples. The default sample count is 24.

## Noise-equivalent rule

Two medians `a` and `b` are noise-equivalent when:

```text
abs(a - b) <= max(100 ns, 0.02 * min(a,b))
```

The winner set contains every Candidate within this threshold of the fastest median. CU6 never forces one winner when the evidence supports a tie.

## Pairwise transitions

CU6 independently classifies, for each Pattern across increasing `K`:

```text
A.DenseMembershipMaskPredicate versus B.CompactIndexListDispatch
B.CompactIndexListDispatch versus C.ActiveBlockListLocalMask
```

Possible classifications are:

- `NoObservedWinnerTransition`
- `SingleStableWinnerTransition`
- `MultipleWinnerTransitions`
- `NoiseEquivalentOrUnresolved`

A transition records the adjacent measured `K` values between which the non-noise winning side changes.

## Pattern sensitivity

For every measured `K`, CU6 compares winner sets across all four Pattern constructors. `SameCardinalityWinnerDependsOnPattern = true` means cardinality alone does not determine the observed best Sparse granularity on the measured adapter.

This is the central distinction from Spiral 4's prefix-only Active Count experiment.

## Construction and execution costs

The Decision Report uses GPU Candidate Path as the primary ranking metric. It also preserves Candidate representation construction/upload cost separately. A Candidate may win GPU execution while costing more to construct, or lose despite a compact representation because locality, fixed Dispatch or block occupancy dominates.

## Non-authorization boundary

Every report retains:

```text
RuntimePolicyAuthorization = None
NextCapabilitySelection = DeferredByOwner
SelectionStatus = OWNER_DECISION_REQUIRED
```

No measurement authorizes Runtime to infer Pattern, inspect `K`, or select A/B/C. Performance superiority and crossover are not completion gates. No observed crossover is a valid result.

CU6 pass means `SGE4-5 SPIRAL 6 EXPERIMENT COMPLETE`. `SPIRAL 6 CLOSED` remains withheld until the Owner selects the next capability or explicitly closes the line.
