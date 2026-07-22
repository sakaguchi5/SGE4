# Spiral 4 CU6 — decision rules

## Primary comparison value

For each `Nf` and Candidate, CU6 takes the median of all
`GpuCandidatePathNanoseconds` samples.

The default sample count is 56 per Candidate and Nf.

## Noise-equivalent rule

Two medians `a` and `b` are noise-equivalent when:

```text
abs(a - b) <= max(100 ns, 0.02 * min(a,b))
```

The absolute and relative thresholds are frozen in the Measurement Profile and
stored in the evidence. They can be overridden only through a future versioned
measurement contract, not by the Decision Report.

## Per-Nf global winner set

Let `m` be the fastest median among all seven Candidates. The winner set
contains every Candidate whose median is within the frozen noise threshold of
`m`.

Therefore the result may be:

```text
one observed winner
or
multiple noise-equivalent observed winners
```

The report does not force a single winner when the evidence does not support
one.

## Best observed Batch size

Among C64/C128/C256/C512/C1024, the lowest median is reported for every `Nf`.
Changes of best Batch size are reported independently from the Single-versus-
Batched winner transition.

A change from C64 to C256 while Batched remains faster than Single is not
classified as a Single/Batched crossover.

## Pairwise transition classifications

CU6 classifies:

```text
A.FixedMaximum versus B.SingleIndirect
B.SingleIndirect versus best observed C.Batched
```

Possible classifications are:

- `NoObservedWinnerTransition`
- `SingleStableWinnerTransition`
- `MultipleWinnerTransitions`
- `NoiseEquivalentOrUnresolved`

A transition is recorded only when the non-noise winning side changes between
two measured Active Count values. Noise-equivalent cases make the classification
unresolved rather than being silently assigned to either side.

## Non-authorization boundary

The Decision Evidence Report always contains:

```text
RuntimePolicyAuthorization = None
NextCapabilitySelection = DeferredByOwner
SelectionStatus = OWNER_DECISION_REQUIRED
```

Observed performance does not authorize Runtime to inspect `Nf` and choose a
Candidate. It also does not establish a universal winner outside the measured
adapter, driver, clock policy, Candidate corpus, and measurement profile.

Performance superiority and crossover are not completion gates. No observed
crossover is a valid result.

CU6 pass means `Experiment Complete`. `Spiral 4 Closed` remains withheld until
the Owner records the next capability selection.
