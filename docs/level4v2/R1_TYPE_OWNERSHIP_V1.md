# Level 4 v2 R1 type ownership V1

R1 follows the R0 single-fact rule. Records may refer to an identity owned elsewhere, but they must not independently duplicate the authoritative fact behind that identity.

| Type or record | Owns | Refers to | Explicitly does not own |
|---|---|---|---|
| `SemanticDescriptorV1` | Semantic identity and version vocabulary | Nothing execution-specific | representation, dispatch, application policy |
| `InvocationDescriptorV1` | explicit invocation identity, ordinal, epoch and dynamic dimensions | Semantic identity | Candidate choice, scheduling, Backend policy |
| `RawCandidateProposalV1` | proposal identity and claimed bindings | Semantic, Invocation, Target, resource contract and proposed write-set identities | verification seal, execution permission |
| `OpaqueVerifiedPlanV1` | opaque Verified identity boundary | future Verifier authority | public construction, Raw conversion |
| `FrozenArtifactDescriptorV1` | Frozen identity vocabulary | Verified Plan, Target and resource-contract identities | live materialization handles |
| `HistoryValidityDescriptorV1` | validity identity, generation, epoch and state | Semantic and source Invocation identities | hidden update policy |
| Epoch-bound handles | handle kind, resource instance and Device epoch | Resource-instance identity | membership, Candidate or Recovery policy |

## Single-fact examples

`InvocationDescriptorV1` stores `SemanticIdentity`, not a second copy of the Semantic description.

`FrozenArtifactDescriptorV1` stores `VerifiedPlanIdentity`, not a mutable copy of the plan operations.

History validity stores the source Invocation identity and generation rather than recopying all invocation fields.

R2 must preserve these reference relationships when it adds the authority chain.
