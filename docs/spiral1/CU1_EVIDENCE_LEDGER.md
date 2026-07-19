# Spiral 1 Completion Unit 1 Evidence Ledger

- Base contract commit: `025218ca31e3b9445bfca0fb36a9d6361f10e698`
- Frozen Proof Ledger: `PROOF_LEDGER_V1.md` remains byte-unchanged
- Status: evidence becomes Qualified only when `run_sge4_5_cu1_semantic_observation.bat` passes

## Evidence mapping

| Invariant | CU1 evidence | Remaining evidence |
|---|---|---|
| S1-I01 | `60_PgaRigidTransformSemantic` has no Matrix/execution vocabulary; canonical Motor and Apply Semantic bytes pass Positive/Negative and Debug A/Debug B/Release comparison | None after CU1 gate passes |
| S1-I13 | `67_Spiral1Observer` aggregates point-index ascending with binary64 accumulators; CPU bundle is deterministic | WARP readback ordering and fresh-process readback remain for Stage 11 |

## CU1 artifact evidence

The CU1 gate emits three independent bundles:

- Stage 04 Semantic bundle
- Stage 05 Corpus/Reference bundle
- CU1 Semantic/Observation bundle

Each selected gate compares fresh Debug process A, fresh Debug process B and Release byte-for-byte. Hash equality alone is not used as the comparison criterion.
