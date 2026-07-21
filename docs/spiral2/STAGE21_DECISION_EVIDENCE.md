# Spiral 2 Stage 21 Decision Evidence

The final candidate-specific hardware report was generated on
`NVIDIA GeForce RTX 4070 Laptop GPU`. M0-M5 each contain 96 balanced-order
samples after warm-up. WARP is excluded from performance claims.

Aggregate candidate GPU ranking:

1. `B.DirectPgaHierarchy`: 49,152 ns
2. `A.MatrixHierarchy`: 140,288 ns
3. `C.HybridHierarchy`: 144,384 ns

This result is authoritative as Spiral 2 evidence for the measured implementation
and adapter. It is not a universal claim that Direct PGA is always faster, and
it does not authorize a global Runtime selection policy.

The valid interpretation is limited to the qualified fixed hierarchy,
canonical-order serial single-dispatch implementations, the six M0-M5 cases,
and the recorded adapter. Measurement Evidence V2 isolates candidate-specific
materialization and end-to-end values and binds actual Package dispatch evidence.

```text
ExperimentEvidenceStatus = Qualified
RecommendationAuthority = NonAuthoritative
MeasuredRecommendation = B.DirectPgaHierarchy
NextCapabilitySelection = DeferredByOwner
SelectionStatus = OWNER_DECISION_REQUIRED
```
