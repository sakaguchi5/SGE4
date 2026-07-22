# Spiral 5 CU6 — decision rules

## Primary value

For every `U` and Candidate, CU6 takes the median of all `GpuCandidateTimelineNanoseconds` samples.

The default sample count is 24 per Candidate and U.

## Noise-equivalent rule

Two medians `a` and `b` are noise-equivalent when:

```text
abs(a - b) <= max(100 ns, 0.02 * min(a,b))
```

The thresholds are stored in the Measurement Profile and binary evidence.

## Per-U winner set

For the fastest median `m`, the winner set contains every Candidate within the frozen noise threshold of `m`. CU6 does not force one winner when the evidence supports a tie or unresolved region.

## Pairwise crossover classifications

CU6 independently classifies:

```text
A.EveryInvocationRecompute versus B.GlobalMotorHistoryReuse
B.GlobalMotorHistoryReuse versus C.FinalOutputHistoryReuse
```

Possible results are:

- `NoObservedWinnerTransition`
- `SingleStableWinnerTransition`
- `MultipleWinnerTransitions`
- `NoiseEquivalentOrUnresolved`

A transition records the adjacent measured U values between which the non-noise winning side changes. Noise-equivalent points keep the classification unresolved rather than assigning an unsupported side.

## Update and Hold interpretation

The report includes median cost per update invocation and per hold invocation for every Candidate. These values explain the measured timeline result but do not replace the timeline primary metric.

A Candidate may have a cheaper Hold and still lose at low U because update cost, argument generation, state transitions, completion, or fixed history capacity dominates.

## History materialization result

The report gives a winner set for every U and an overall observed winner set based on the median of the seven per-U medians. The overall set is descriptive only; it is not a universal materialization rule.

## Non-authorization boundary

Every report retains:

```text
RuntimePolicyAuthorization = None
NextCapabilitySelection = DeferredByOwner
SelectionStatus = OWNER_DECISION_REQUIRED
```

No measurement authorizes Runtime to inspect U or select A/B/C. Performance superiority and crossover are not completion gates. No observed crossover is a valid result.

CU6 pass means `SGE4-5 SPIRAL 5 EXPERIMENT COMPLETE`. `SPIRAL 5 CLOSED` remains withheld until the Owner selects the next capability or explicitly closes the research line.
