# CU6-2 Decision Evidence

The Decision Map is derived from balanced paired ratios, not from independent Candidate medians.

```text
StableWinner(X) = X wins against each of the other two Candidates
StableEquivalentSet(X,Y) = X/Y are noise-equivalent and X,Y each beat Z
ZeroDispatchEquivalent = T=0 and authority set B/C
Unresolved = no qualified paired authority conclusion
```

`median_A_ns`, `median_B_ns`, and `median_C_ns` remain in the CSV for description. `median_descriptive_set` is explicitly non-authoritative.

## Accepted combined result

```text
Coordinates              220
ZeroDispatchEquivalent    20
StableWinner              41
StableEquivalentSet       61
Unresolved                98
StableWinner authority    B only
B/C equivalent            59
A/B/C equivalent           2
```

B's stable wins occur only in UniformStride and HashScatter patterns. C never stably beats B, but is frequently stably equivalent to B when transition work is block-local. A becomes competitive at high Transition counts, including two A/B/C-equivalent coordinates, but is never a stable single winner.

Stable crossovers are printed as resolved-coordinate brackets:

```text
[STABLE-CROSSOVER-T] ... bracket=T0..T1 B->A/B/C
[STABLE-CROSSOVER-A] ... bracket=A0..A1 B->B/C
```

Unresolved coordinates may lie inside a bracket. The report does not interpolate or invent an exact threshold.

```text
Runtime Candidate-policy authorization = None
Universal winner claim                 = Forbidden
Measurement scope                      = Adapter/Driver/Profile specific
Spiral 7 status                        = Closed by Owner after accepted CU6-2 run
```
