# Observation tolerance implementation v1

The CU0 Observation Contract required its tolerance values and scale algorithm
to be versioned in project 81. Before the first complete WARP corpus run, those
constants had not yet been represented there; project 87 contained provisional
literal thresholds suitable only for the two-bone vertical slice.

The first complete-corpus failure was H05/F10. It reported maximum reference
error `9.2e-5`, pairwise error `6.1e-5`, axis-length error `1.4e-5`, and
orthogonality error `1.3e-5`. The rigid result was valid, but the provisional
absolute threshold rejected accumulated binary32 hierarchy rounding.

The frozen v1 implementation is:

```text
reference absolute = 2.0e-4
reference relative = 2.0e-5 * max(1, |reference components|)
pairwise absolute  = 1.5e-4
pairwise relative  = 2.0e-5 * max(1, |reference components|)
rigidity absolute  = 5.0e-4
```

The exact values are part of `ObservationContractIdentityV1`; therefore the
semantic, verified representation, Leaf Package, and Composition digests change
for a documented semantic reason. No existing expected digest was rewritten.
The full H00-H08 × F00-F11 corpus, Debug/Release freeze comparison, and Spiral 1
regressions qualify this identity.
