# Spiral 2 Stage 21 Prototype Decision Evidence

The Release prototype measurement completed on
`NVIDIA GeForce RTX 4070 Laptop GPU`. M0-M5 each contain 96 balanced-order
samples after warm-up. WARP is excluded from performance claims.

Aggregate sample-median GPU ranking emitted by the prototype executable:

1. `B.DirectPgaHierarchy`: 49,152 ns
2. `A.MatrixHierarchy`: 140,288 ns
3. `C.HybridHierarchy`: 143,360 ns

`RecommendationAuthority = NonAuthoritative`

Candidate B is the measured-prototype representation recommendation on this
adapter. It is not a global policy selection. `Vertex Consumer` remains only a
non-authoritative direction because the current prototype consumes qualified
global transforms through fixed probes.

The generated transient report and binary/CSV evidence remain under
`build/tests/spiral2/measurement/` when reproduced. They preserve per-Leaf GPU
timestamps, command-recording observations, ordering, Package and resource
figures, and numeric observations.

Architecture review limits the interpretation of the existing report:

- candidate-specific end-to-end values were not isolated from the three-candidate
  sample total;
- materialization values require candidate-specific accounting;
- the displayed preparation dispatch count is a proposed graph value rather than
  the final Package-operation count;
- Dynamic upload and intermediate-byte labels require refinement;
- final authority and Observation-contract findings remain open.

Therefore the ranking is retained as useful prototype evidence, but Stage 21 is
not final Decision Evidence and does not authorize a representation policy or
next capability.

```text
ImplementationStatus = PrototypePaused
PrototypeDecisionEvidence = Preserved
InterpretationReview = Required
ArchitectureReviewStatus = FindingsOpen
NextCapabilitySelection = DeferredByOwner
SelectionStatus = OWNER_DECISION_REQUIRED
```
