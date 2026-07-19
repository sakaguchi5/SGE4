# Spiral 2 observation contract v1

For every bone, candidates A, B, and C output five binary32 homogeneous points
in the fixed P0-P4 order. The CPU authority promotes the exact dynamic binary32
Motor bytes to binary64, composes the hierarchy without calling any candidate
implementation, applies P0-P4, then rounds once to binary32.

The observer records componentwise absolute error, relative error, ULP distance,
non-finite flags, A-B/A-C/B-C differences, axis-length error, pairwise axis-dot
error, determinant orientation, and translation error. Output order is bone,
probe, component. Aggregation is deterministic and retains the first mismatch.

Qualification requires zero non-finite values and zero contract mismatches.
Tolerance values and their scale algorithm are versioned in `81_Spiral2Contracts`;
changing them changes the Observation Contract identity.

