# Update-set expectation correction

## Observed failure

The first Windows full-gate run completed both Debug and Release builds, then stopped in
`60_UnifiedArchitectureTests` with:

```text
New SGE4 unified architecture failure: update set mismatch
```

## Root cause

The production planner and the independent verifier both implement the accepted R4 algebra:

```text
N_t = A_t \ A_(t-1)              activation
R_t = A_(t-1) \ A_t              deactivation
W_t = N_t union M_t               update
H_t = survivors \ M_t            retain
T_t = W_t union R_t               transition
```

For the regression scenario:

```text
A_(t-1) = {0, 2, 7}
A_t     = {0, 3, 7}
M_t     = {7}
```

therefore:

```text
N_t = {3}
R_t = {2}
W_t = {3, 7}
H_t = {0}
T_t = {2, 3, 7}
```

The unified Smoke test incorrectly expected `W_t = {7}`, treating the update set as only the
modified-survivor input. The implementation was correct; the test oracle was wrong.

## Correction

`tests/60_UnifiedArchitectureTests/main.cpp` now expects `{3, 7}` and contains an explicit
comment tying the assertion to `W_t = N_t union M_t`.

No production planner, verifier, Frozen Artifact, Runtime, ABI, or D3D12 code was changed.
