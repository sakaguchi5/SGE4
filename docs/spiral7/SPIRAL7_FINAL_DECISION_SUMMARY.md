# Spiral 7 final Decision summary

## What was proven

Spiral 7 proved an exact, versioned relation between sparse membership, temporal history and indirect GPU work. The external inputs `A_t` and `M_t` determine `N_t`, `R_t`, `W_t`, `H_t` and `T_t`; Planner output cannot execute until an independent Verifier reconstructs and seals the same authority.

The three qualified lowerings are:

```text
A = FullActiveDenseRecompute
B = CompactDeltaIndexHistoryReuse
C = AffectedBlockDeltaHistoryReuse
```

CU5 proved semantic equivalence, exact legal write sets, deterministic Frozen authority, whole-composition Recovery, stale-epoch rejection and explicit post-Recovery rebind/rebuild.

## What the real-GPU experiment showed

The accepted combined map contains 220 unique `(Pattern, ActiveCount, TransitionCount)` coordinates.

```text
ZeroDispatchEquivalent  20
StableWinner            41
StableEquivalentSet     61
Unresolved              98
```

All 41 stable single-winner coordinates selected B. Nineteen occur in `UniformStride` and twenty-two in `HashScatterPermutation`. No stable single-winner coordinate selected A or C.

B and C are stably equivalent in 59 coordinates. This occurs chiefly where transition work is sufficiently block-local that C's affected-block representation does not pay for a wide block surface.

At high transition counts A becomes competitive. Two coordinates classify A/B/C as stably equivalent, but A never becomes a stable single winner. Therefore the experiment does not support a universal threshold such as “use A above T=N”.

## Correct interpretation

The relevant local variables include:

```text
|A_t|
|T_t|
|W_t|
|R_t|
AffectedBlockCount
Sparse Pattern
Adapter / Driver / Measurement Profile
```

The result supports a local Decision Map, not a universal Candidate policy. Runtime authorization remains `None`. A future compiler may consume a separately verified profile, but no such policy is introduced by Spiral 7.

## Why unresolved is a valid result

An unresolved coordinate means the balanced paired observations did not justify one stable authority set under the frozen noise and agreement contract. It is deliberately excluded from crossover. Assigning a winner would manufacture information that the measurement did not establish.

## Closure decision

Spiral 7 has answered its research question. The next step is not another exploratory Spiral. Level 4 v2 must reconstruct a canonical architecture from the proven common structure while preserving this Decision evidence as Adapter-specific observational input.
