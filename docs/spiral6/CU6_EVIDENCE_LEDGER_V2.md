# Spiral 6 CU6 V2 evidence ledger

| Evidence | Boundary |
|---|---|
| Relative Performance Evidence V2 binary | Canonical config, adapter, CU5 evidence identities, Candidate profiles, accepted/rejected block attempts and all paired samples |
| Candidate sample CSV | Absolute batch time, ns/iteration, local control, control-normalized score, order position and output digest |
| Block stability CSV | Fixed-control before/after/reference/ratio and optional diagnostic telemetry |
| Relative manifest JSON | Measurement profile, balanced schedules, primary decision metric and Evidence digest |
| Relative Decision Map V2 | Winner sets, A/B and B/C paired ratios, agreement fractions, crossover and Pattern sensitivity |

## Authority carried from CU5

CU6 binds:

- Architecture evidence identity,
- Controlled Recovery evidence identity,
- Fresh Rematerialization evidence identity,
- canonical Sparse Semantic identity,
- independent verification context identity,
- Candidate artifact and Frozen binding identities per exact set.

## New CU6 proof boundary

CU6 V2 proves only an observed relative map on one explicitly selected hardware adapter.

It does not prove:

- a universal Candidate winner,
- an adapter-independent crossover,
- that one P-state is preferable,
- that absolute nanoseconds are comparable across Pattern/K blocks,
- that Runtime may inspect K or Pattern and select A/B/C.
