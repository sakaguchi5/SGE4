# Spiral 7 project boundaries V1

## CU1 boundary

CU1 is documents and verification only.

It must not modify:

- D3D12 Schema 17,
- Runtime 17,
- Level 4 v1 Canonical bytes,
- Spiral 4 indirect extension bytes,
- Spiral 5 temporal extension bytes,
- Spiral 6 sparse extension bytes,
- C++ projects,
- Solution project registration,
- Runtime or Backend policy.

## Future project reservation

Later Completion Units may add projects beginning at 172 only after CU2 proves that the existing three extensions cannot express partial history validity and exact update/clear authority without hidden Runtime judgment.

Reserved project roles are:

```text
172_SparseTemporalDeltaSemantic
173_Spiral7DeltaContracts
174_Spiral7DeltaExecution
175_SparseTemporalDeltaLoweringCandidate
176_SparseTemporalDeltaLoweringPlanner
177_SparseTemporalDeltaLoweringVerifier
178_SparseTemporalDeltaCandidateFamily
179_SparseTemporalDeltaCandidateFamilyPlanner
180_SparseTemporalDeltaCandidateFamilyVerifier
181_Spiral7DeltaFamilyExecution
182_Spiral7DeltaArchitectureTests
183_Spiral7AuthorityMutationTests
184_Spiral7CandidateFamilyTests
185_Spiral7ArchitectureQualificationTests
186_Spiral7PerformanceExperiment
187_Spiral7PerformanceTests
```

The names reserve responsibilities, not implementation approval.

## Authority split

- External verified input owns `A_t` and `M_t`.
- Semantic construction derives `N_t`, `R_t`, `W_t`, `H_t` and `T_t`.
- Planner proposes a Candidate-specific physical representation.
- Planner-independent Verifier proves set relations, generation legality and binding identity.
- Frozen artifact owns the verified Candidate and all derived identities.
- Runtime and Backend execute only the verified result.
- Owner alone may close the Spiral or select the next capability.
