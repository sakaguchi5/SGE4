# Spiral 2 observation contract v2

V2 is the current Spiral 2 observation authority. V1 remains available as
historical evidence and is not redefined.

For every bone, candidates A, B, and C output five binary32 homogeneous points
in fixed P0-P4 order. The independent CPU authority promotes the exact dynamic
binary32 Motor bytes to binary64, composes the hierarchy without calling a
candidate implementation, applies P0-P4, then rounds once to binary32.

For each X, Y, and Z component independently, V2 records and validates:

- A-reference, B-reference, and C-reference absolute error
- A-B, A-C, and B-C absolute error
- componentwise relative error using that component's scale
- ULP distance with +0.0 and -0.0 canonicalized to the same ordered value

It also records deterministic RMS Euclidean error, first mismatch index,
non-finite and homogeneous flags, translation error, axis-length error,
pairwise axis-dot error, minimum determinant, and orientation preservation.

The fixed tolerance algorithm is documented in
`OBSERVATION_TOLERANCE_IMPLEMENTATION_V2.md`. Changing its constants, scale
algorithm, signed-zero rule, output ordering, or record schema requires a new
Observation Contract identity.
