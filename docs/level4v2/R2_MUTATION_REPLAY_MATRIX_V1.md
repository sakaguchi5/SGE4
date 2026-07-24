# Level 4 v2 R2 mutation and replay matrix V1

| # | Mutation or replay | Required rejection |
|---:|---|---|
| 1 | Semantic canonical payload changed | `SemanticDefinitionIdentityMismatch` |
| 2 | Invocation bound to another Semantic | `InvocationSemanticBindingMismatch` |
| 3 | Invocation dimension changed without identity reconstruction | `InvocationDefinitionIdentityMismatch` |
| 4 | Target definition payload changed | `TargetProfileDefinitionIdentityMismatch` |
| 5 | Resource-contract definition payload changed | `ResourceContractDefinitionIdentityMismatch` |
| 6 | Legal write-set definition payload changed | `WriteSetDefinitionIdentityMismatch` |
| 7 | Candidate claims another Semantic | `CandidateSemanticBindingMismatch` |
| 8 | Candidate claims another Invocation | `CandidateInvocationBindingMismatch` |
| 9 | Proposal replayed under another valid Target profile | `CandidateTargetBindingMismatch` |
| 10 | Proposal replayed under another valid resource contract | `CandidateResourceBindingMismatch` |
| 11 | Proposal replayed under another valid legal write set | `CandidateWriteSetBindingMismatch` |
| 12 | Explicit Candidate payload changed | `CandidateIdentityMismatch` |
| 13 | Authority operation removed | `OperationCountMismatch` |
| 14 | Operation ordinal changed | `OperationOrdinalMismatch` |
| 15 | Operation kind changed | `OperationKindMismatch` |
| 16 | Operation subject identity changed | `OperationSubjectMismatch` |
| 17 | Planner-proposal identity changed | `PlannerProposalIdentityMismatch` |
| 18 | Raw Candidate identity changed | `CandidateIdentityMismatch` |

Every rejected case must return no `VerifiedAuthorityV1`.
