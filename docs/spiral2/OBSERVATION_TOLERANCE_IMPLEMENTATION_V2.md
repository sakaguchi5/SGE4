# Observation tolerance implementation v2

Review Closure introduced the Observation Contract that the Spiral 2 specification
already required: absolute error, relative error, and ULP distance are evaluated
**component by component** for A-reference, B-reference, C-reference, A-B, A-C,
and B-C. Signed zero is canonicalized only for ULP distance.

The historical V1 implementation used one scale value derived from the largest
component of a complete point. In H07/F10 (64-bone serial chain with mixed-magnitude
translation), a large X component therefore concealed accumulated binary32 error in
the much smaller Y component. The componentwise implementation correctly exposed it.
The first failing residuals after the relative term were:

```text
reference residual: 2.2162e-4
pairwise residual:  2.0423e-4
```

The rigid-transform evidence remained valid:

```text
max axis-length error: 1.06e-4
max orthogonality error: 1.52e-4
orientation: positive
```

V2 therefore retains the relative and rigidity terms and changes only the absolute
floors needed by the componentwise contract:

```text
reference absolute = 2.5e-4
reference relative = 2.0e-5 * max(1, |reference component|)
pairwise absolute  = 2.5e-4
pairwise relative  = 2.0e-5 * max(1, |left component|, |right component|)
rigidity absolute  = 5.0e-4
ULP(+0.0, -0.0)    = 0
```

V1 constants and `ObservationContractIdentityV1` remain unchanged as historical
evidence. Current Spiral 2 semantics, verified representations, Frozen Leaf Packages,
Frozen Composition, Freeze evidence, and Measurement Evidence bind
`ObservationContractIdentityV2`.

This is not a relaxation based on the aggregate `max-reference=0.001221` value. That
aggregate maximum occurred on large coordinates and already passed the relative term.
V2 addresses only the smaller components whose componentwise absolute floor was
insufficient.
