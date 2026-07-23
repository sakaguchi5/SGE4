# Spiral 6 CU6 V2 — relative decision rules

## Primary evidence

The primary evidence is not an absolute time table. It is the paired ratio inside one Pattern/K observation:

```text
r_AB = time(A) / time(B)
r_BC = time(B) / time(C)
r_AC = time(A) / time(C)
```

Because numerator and denominator come from the same paired block, a block-wide clock scale largely cancels.

## Control-normalized descriptive score

For an accepted block:

```text
controlReference = sqrt(controlBefore × controlAfter)
normalized(X)    = time(X) / controlReference
```

The median normalized score is used to form the three-way winner set. Absolute nanoseconds remain in Evidence for inspection, but are descriptive only.

## Pair classification

For one pair, CU6 records:

- median paired ratio,
- number of paired observations,
- fraction where the left Candidate is faster,
- fraction where the right Candidate is faster.

With the default 2% noise floor and 70% agreement boundary:

```text
Left wins:
  median(left/right) < 0.98
  and left is faster in at least 70% of paired observations

Right wins:
  median(left/right) > 1.02
  and right is faster in at least 70% of paired observations

otherwise:
  NoiseEquivalent
```

CU6 never forces a winner when the paired observations disagree.

## Winner set

The winner set is derived from paired decisions, not from cross-block absolute timing.

A Candidate is removed only when another Candidate definitively beats it under the paired ratio and agreement rule. Noise-equivalent pairs do not eliminate either side. If the observed pair relations form a non-transitive cycle, all three Candidates remain in the winner set rather than forcing a false total order.

Control-normalized medians are retained as descriptive scores and as an audit of block scale, but do not override paired disagreement.

## Crossover map

For every Pattern and increasing K, CU6 separately follows:

```text
A versus B
B versus C
```

The classification is:

- `NoObservedWinnerTransition`,
- `SingleStableWinnerTransition`,
- `MultipleWinnerTransitions`,
- `NoiseEquivalentOrUnresolved`.

A crossover concerns paired relative winners. It does not compare the absolute nanoseconds of one K block with another K block.

## Pattern sensitivity

For each K, CU6 compares the winner sets of all four exact-set patterns.

```text
SameKWinnerDependsOnPattern = true
```

means that cardinality alone is insufficient to determine the observed representation granularity on the measured adapter.

## Completion and authorization

A Candidate does not need to win and a crossover does not need to exist.

CU6 passes when:

- all canonical Pattern/K cases have an accepted complete paired block for every requested run,
- every accepted block contains all six orders and complete A/B/C pairs,
- rejected blocks are excluded from decisions,
- all outputs retain CU5 correctness,
- Evidence round-trip and corruption rejection pass,
- a Relative Decision Map is generated.

Every report retains:

```text
RuntimePolicyAuthorization = None
NextCapabilitySelection = DeferredByOwner
SelectionStatus = OWNER_DECISION_REQUIRED
```
