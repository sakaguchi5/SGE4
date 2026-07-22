# Spiral 6 CU3 — Independent Sparse Authority

CU3 turns the CU2 Compact Index architecture into an authority-controlled execution path.

```text
Exact Sparse Semantic + exact set S
  -> Raw Sparse Candidate
  -> Planner
  -> Planner-independent Verifier
  -> opaque Verified Sparse Plan
  -> CU2 fixed-capacity Sparse sidecar
  -> actual Compact Index Resource identity
  -> device-epoch-bound Frozen execution
  -> WARP
```

The Verifier does not depend on the Planner. It independently rebuilds the CU2 artifact from the canonical Semantic and exact set, then rederives Dispatch dimensions, fixed-capacity layout, Program identities, exact write-set authority and logical Resource identity.

The qualified CU3 case is `HashScatterPermutation`, `K=1024`, device epoch 1. This is intentionally non-prefix and distinguishes exact set authority from cardinality-only authority.

CU3 does not add the Dense Mask or Active Block Candidates, does not perform Recovery and does not authorize Runtime Sparse selection.
