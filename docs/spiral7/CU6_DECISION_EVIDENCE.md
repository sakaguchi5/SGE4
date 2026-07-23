# CU6-2 Decision Evidence

The Decision Map is derived from balanced paired ratios, not from independent Candidate medians.

```text
StableWinner(X) = X wins against each of the other two Candidates
StableEquivalentSet(X,Y) = X/Y are noise-equivalent and X,Y each beat Z
ZeroDispatchEquivalent = T=0 and authority set B/C
Unresolved = no qualified paired authority conclusion
```

`median_A_ns`, `median_B_ns`, and `median_C_ns` remain in the CSV for description. `median_descriptive_set` is explicitly non-authoritative.

Stable crossovers are printed as brackets:

```text
[STABLE-CROSSOVER-T] ... bracket=T0..T1 B->A
[STABLE-CROSSOVER-A] ... bracket=A0..A1 B->B/C
```

Unresolved coordinates may lie inside a bracket. The report does not interpolate or invent an exact threshold.

Runtime Candidate-policy authorization remains `None`. Owner closure remains required.
