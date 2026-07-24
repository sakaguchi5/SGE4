# Level 4 v2 Canonical reconstruction

This directory is the sole active planning and reconstruction entry point after Spiral 7 closure.

Level 4 v2 is a reconstruction program, not a new capability Spiral. It extracts the minimal common architecture proven by Level 4 v1 and Spirals 1–7, migrates retained evidence, and removes experimental duplication only after replacement proof exists.

## Current status

```text
Preparation handoff = PASSED
R0 Input Freeze = PASSED
R1 Canonical Vocabulary = COMPLETE PACKAGE SUPPLIED; OWNER GATE PENDING
R2 Unified Authority Chain = NEXT AFTER SUCCESSFUL R1 GATE
Runtime Candidate-policy authorization = None
```

Read in this order:

1. `CANONICAL_RECONSTRUCTION_INPUT_V1.md`
2. `R0_INPUT_FREEZE_MANIFEST_V1.json`
3. `R0_CARRIED_INVARIANT_MAP_V1.md`
4. `R0_CANONICAL_LAYER_OWNERSHIP_V1.md`
5. `R1_CANONICAL_VOCABULARY_ENTRY_CONTRACT_V1.md`
6. `R1_CANONICAL_VOCABULARY_MANIFEST_V1.json`
7. `R1_CANONICAL_VOCABULARY.md`
8. `R1_TYPE_OWNERSHIP_V1.md`
9. `R1_NEGATIVE_TEST_MATRIX_V1.md`
10. `R2_UNIFIED_AUTHORITY_ENTRY_CONTRACT_V1.md`
11. `PROVEN_CAPABILITY_MATRIX_V1.md`
12. `RETAINED_REFERENCE_LAYOUT_V1.md`
13. `RECONSTRUCTION_ROADMAP_V1.md`
14. `EXTERNAL_EVIDENCE_INDEX_V1.md`
15. `PRE_V2_REPOSITORY_DECISION_V1.md`

## Active command

```powershell
.\run_sge4_5_level4v2_r1_prepare.bat
.\run_sge4_5_level4v2_r1_canonical_vocabulary.bat
```

## Hard boundaries

```text
No new capability during reconstruction
No Runtime policy inference
No hardware-specific winner embedded in the Canonical ABI
No application-policy inference
No mutation of accepted Schema 17 / Runtime 17 evidence before migration proof
No deletion of reference projects until v2 reproduces their invariants
No staged experiment names in the v2 Canonical vocabulary
R1 defines vocabulary only; authority behavior begins in R2
```
