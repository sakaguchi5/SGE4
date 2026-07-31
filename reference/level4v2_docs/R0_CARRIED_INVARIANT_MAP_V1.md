# Level 4 v2 R0 carried invariant map V1

## Status

```text
R0 Input Freeze = COMPLETE
Base commit = a44b2ee3e5e151eab2f25f94e92d838b7931a4d9
Carried invariants = 40
New capability = None
Runtime Candidate-policy authorization = None
```

Each row freezes one fact that Level 4 v2 must reproduce, explicitly version, or reject with an Owner-approved supersession record. An invariant may not silently disappear during cleanup.

| ID | Canonical fact | Owning v2 layer | Intended destination | Frozen sources |
|---|---|---|---|---|
| `V2-R0-I001` | `SemanticExecutionSeparation` | `CanonicalSemantic` | `R1CanonicalVocabulary` | `Level4V1ReconstructionPolicy`, `Spiral1ProofLedger` |
| `V2-R0-I002` | `ExplicitDynamicInvocationInputs` | `CanonicalInvocation` | `R4DynamicInvocationAndHistory` | `Spiral4ProofLedger`, `Spiral5ProofLedger`, `Spiral6ProofLedger`, `Spiral7ProofLedger` |
| `V2-R0-I003` | `ExternalMembershipAuthority` | `CanonicalInvocation` | `R4DynamicInvocationAndHistory` | `Spiral7ProofLedger` |
| `V2-R0-I004` | `ExactTransitionAlgebra` | `CanonicalSemantic` | `R4DynamicInvocationAndHistory` | `Spiral7ProofLedger` |
| `V2-R0-I005` | `FirstInvocationFullActiveRule` | `CanonicalInvocation` | `R4DynamicInvocationAndHistory` | `Spiral7ProofLedger` |
| `V2-R0-I006` | `NoGamePolicyInference` | `CanonicalSemantic` | `R1CanonicalVocabulary` | `CanonicalInputNarrative`, `Spiral7Closure` |
| `V2-R0-I007` | `RawCandidateCannotExecute` | `CandidateProposal` | `R2UnifiedAuthorityChain` | `Spiral3ProofLedger`, `Spiral4ProofLedger`, `Spiral5ProofLedger`, `Spiral6ProofLedger`, `Spiral7ProofLedger` |
| `V2-R0-I008` | `PlannerHasNoSealAuthority` | `CandidatePlanner` | `R2UnifiedAuthorityChain` | `Spiral4ProofLedger`, `Spiral5ProofLedger`, `Spiral6ProofLedger`, `Spiral7ProofLedger` |
| `V2-R0-I009` | `PlannerIndependentVerification` | `IndependentVerifier` | `R2UnifiedAuthorityChain` | `Spiral3ProofLedger`, `Spiral4ProofLedger`, `Spiral5ProofLedger`, `Spiral6ProofLedger`, `Spiral7ProofLedger` |
| `V2-R0-I010` | `OpaqueVerifiedPlan` | `IndependentVerifier` | `R2UnifiedAuthorityChain` | `Spiral4ProofLedger`, `Spiral5ProofLedger`, `Spiral6ProofLedger`, `Spiral7ProofLedger` |
| `V2-R0-I011` | `DeterministicFrozenIdentity` | `FrozenArtifact` | `R2UnifiedAuthorityChain` | `AcceptedArchitectureQualification`, `Level4V1RuntimeRecoveryQualification` |
| `V2-R0-I012` | `ResourceIdentityBinding` | `FrozenArtifact` | `R2UnifiedAuthorityChain` | `Spiral6ProofLedger`, `Spiral7ProofLedger` |
| `V2-R0-I013` | `TargetBindingAuthority` | `IndependentVerifier` | `R2UnifiedAuthorityChain` | `Spiral4ProofLedger`, `Spiral5ProofLedger`, `Spiral6ProofLedger`, `Spiral7ProofLedger` |
| `V2-R0-I014` | `FiniteBufferDagComposition` | `CanonicalComposition` | `R3CanonicalComposition` | `Level4V1ReconstructionPolicy`, `CapabilityMatrix` |
| `V2-R0-I015` | `SingleWriterFlow` | `CanonicalComposition` | `R3CanonicalComposition` | `Level4V1ReconstructionPolicy` |
| `V2-R0-I016` | `OptionalSinglePresenter` | `CanonicalComposition` | `R3CanonicalComposition` | `Level4V1ReconstructionPolicy` |
| `V2-R0-I017` | `SingleAdapterScope` | `CanonicalComposition` | `R3CanonicalComposition` | `CanonicalInputNarrative`, `Level4V1ReconstructionPolicy` |
| `V2-R0-I018` | `WholeCompositionRecoveryScope` | `CanonicalRecovery` | `R5RuntimeAndRecovery` | `Level4V1RuntimeRecoveryQualification` |
| `V2-R0-I019` | `NoUnprovenCompositionGeneralization` | `CanonicalComposition` | `R3CanonicalComposition` | `CanonicalInputNarrative` |
| `V2-R0-I020` | `StrongDynamicDimensions` | `CanonicalVocabulary` | `R1CanonicalVocabulary` | `CanonicalInputNarrative`, `CapabilityMatrix` |
| `V2-R0-I021` | `VerifiedIndirectQuantity` | `CanonicalInvocation` | `R4DynamicInvocationAndHistory` | `Spiral4ProofLedger` |
| `V2-R0-I022` | `ExplicitHistoryValidity` | `CanonicalHistory` | `R4DynamicInvocationAndHistory` | `Spiral5ProofLedger`, `Spiral7ProofLedger` |
| `V2-R0-I023` | `ExactSparseMembership` | `CanonicalInvocation` | `R4DynamicInvocationAndHistory` | `Spiral6ProofLedger` |
| `V2-R0-I024` | `SparseTemporalDeltaAuthority` | `CanonicalInvocation` | `R4DynamicInvocationAndHistory` | `Spiral7ProofLedger` |
| `V2-R0-I025` | `ExactIncrementalWriteSet` | `IndependentVerifier` | `R4DynamicInvocationAndHistory` | `Spiral7ProofLedger` |
| `V2-R0-I026` | `RetainedHistoryByteStability` | `CanonicalHistory` | `R4DynamicInvocationAndHistory` | `Spiral7ProofLedger` |
| `V2-R0-I027` | `EpochBoundRuntimeHandles` | `CanonicalRuntime` | `R5RuntimeAndRecovery` | `Spiral7ProofLedger`, `AcceptedArchitectureQualification` |
| `V2-R0-I028` | `RecoveryInvalidatesTemporalState` | `CanonicalRecovery` | `R5RuntimeAndRecovery` | `AcceptedArchitectureQualification`, `Level4V1RuntimeRecoveryQualification` |
| `V2-R0-I029` | `ExplicitExternalRecoveryRebind` | `CanonicalRecovery` | `R5RuntimeAndRecovery` | `Spiral7ProofLedger`, `AcceptedArchitectureQualification` |
| `V2-R0-I030` | `FullActiveRecoverySeed` | `CanonicalRecovery` | `R5RuntimeAndRecovery` | `Spiral7ProofLedger`, `AcceptedArchitectureQualification` |
| `V2-R0-I031` | `RemovedAdapterExclusion` | `CanonicalRuntime` | `R5RuntimeAndRecovery` | `AcceptedArchitectureQualification` |
| `V2-R0-I032` | `CorrectnessMeasurementSeparation` | `QualificationEvidence` | `R6MigrationCorpus` | `AcceptedArchitectureQualification`, `AcceptedObservationalMeasurement` |
| `V2-R0-I033` | `MeasurementIdentityBinding` | `QualificationEvidence` | `R6MigrationCorpus` | `AcceptedObservationalMeasurement` |
| `V2-R0-I034` | `PairedDecisionAuthority` | `QualificationEvidence` | `R6MigrationCorpus` | `AcceptedObservationalMeasurement`, `FinalDecisionSummary` |
| `V2-R0-I035` | `NoRuntimePerformancePolicy` | `CanonicalRuntime` | `R5RuntimeAndRecovery` | `Spiral7Closure`, `FinalDecisionSummary` |
| `V2-R0-I036` | `ExternalRawEvidenceRetention` | `QualificationEvidence` | `R6MigrationCorpus` | `Spiral7Closure` |
| `V2-R0-I037` | `ReferenceRetentionUntilMigrationProof` | `MigrationProgram` | `R7ReferenceRetirement` | `CanonicalInputNarrative`, `RoadmapBeforeR0` |
| `V2-R0-I038` | `SingleFactOwnership` | `CanonicalVocabulary` | `R2UnifiedAuthorityChain` | `CanonicalInputNarrative` |
| `V2-R0-I039` | `RepresentationChoiceRemainsVerified` | `CandidateProposal` | `R2UnifiedAuthorityChain` | `Spiral1ProofLedger`, `Spiral2ProofLedger`, `Spiral3ProofLedger` |
| `V2-R0-I040` | `NoHardwarePolicyInCanonicalAbi` | `CanonicalVocabulary` | `R1CanonicalVocabulary` | `Spiral7Closure`, `CanonicalInputNarrative` |

## Migration rule

For every row, R1–R6 must eventually record exactly one outcome:

```text
Reproduced
VersionedReplacement
ExplicitlyNotCarriedWithOwnerReason
```

`Missing`, implied inheritance, and undocumented semantic drift are not valid outcomes. Reference retirement at R7 is forbidden while any row lacks a terminal migration outcome.
