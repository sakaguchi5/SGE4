# Spiral 3 CU2 evidence ledger

Scope: graph-level A/B/C candidate authority.

Implemented evidence:

- 18 candidate plans: six reuse cases × Matrix-before / Direct-PGA / Matrix-after;
- verifier has no ProjectReference or function call to the Planner;
- `VerifiedReuseRepresentationV1` is non-aggregate, non-default-constructible, and cannot be constructed or converted from Raw Candidate state;
- `enum class` boundaries prevent implicit Candidate/Operation/Lowering integer conversion;
- 24 independent identity, workload, execution-model, node, dispatch, Program and edge mutations are rejected;
- portable C++23 compile/run qualification in the generation environment.

Windows completion command: `run_sge4_5_spiral3_cu2_candidate_authority.bat`.
