# Level 4 v2 Canonical reconstruction

This directory is the sole active reconstruction entry point after the closed research program.

## Current status

```text
Preparation handoff = PASSED
R0 Input Freeze = PASSED
R1 Canonical Vocabulary = PASSED
R2 Unified Authority Chain = PASSED
R3 Canonical Composition = COMPLETE PACKAGE SUPPLIED; OWNER GATE PENDING
R4 Dynamic Invocation and History = NEXT AFTER R3 ACCEPTANCE
Runtime Candidate-policy authorization = None
```

## Active command

```powershell
.\run_sge4_5_level4v2_r3_prepare.bat
.\run_sge4_5_level4v2_r3_canonical_composition.bat
```

## Current reading order

1. `R0_INPUT_FREEZE_MANIFEST_V1.json`
2. `R1_CANONICAL_VOCABULARY_MANIFEST_V1.json`
3. `R2_UNIFIED_AUTHORITY_MANIFEST_V1.json`
4. `R3_CANONICAL_COMPOSITION_MANIFEST_V1.json`
5. `R3_CANONICAL_COMPOSITION.md`
6. `R3_AUTHORITY_OWNERSHIP_V1.md`
7. `R3_MUTATION_REPLAY_MATRIX_V1.md`
8. `R3_EVIDENCE_LEDGER_V1.md`
9. `R4_DYNAMIC_INVOCATION_HISTORY_ENTRY_CONTRACT_V1.md`
10. `RECONSTRUCTION_ROADMAP_V1.md`

## Hard boundaries

```text
No new capability during reconstruction
No Runtime policy inference
No hardware-specific winner in the Canonical ABI
No application-policy inference
No mutation of accepted Schema 17 / Runtime 17 Evidence before migration proof
No deletion of reference Projects until v2 reproduces their Invariants
No public staged names in the v2 Canonical model
```
