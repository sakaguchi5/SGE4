# Level 4 v2 Canonical reconstruction

This directory is the sole active planning and reconstruction entry point after Spiral 7 closure.

Level 4 v2 is a reconstruction program, not a new capability Spiral. It extracts the minimal common architecture proven by Level 4 v1 and Spirals 1–7, migrates retained Evidence, and removes experimental duplication only after replacement proof exists.

## Current status

```text
Preparation handoff = PASSED
R0 Input Freeze = PASSED
R1 Canonical Vocabulary = PASSED
R2 Unified Authority Chain = COMPLETE PACKAGE SUPPLIED; OWNER GATE PENDING
R3 Canonical Composition = NEXT AFTER R2 ACCEPTANCE
Runtime Candidate-policy authorization = None
```

## Active command

```powershell
.\run_sge4_5_level4v2_r2_prepare.bat
.\run_sge4_5_level4v2_r2_unified_authority.bat
```

## Current reading order

1. `R0_INPUT_FREEZE_MANIFEST_V1.json`
2. `R1_CANONICAL_VOCABULARY_MANIFEST_V1.json`
3. `R2_UNIFIED_AUTHORITY_MANIFEST_V1.json`
4. `R2_UNIFIED_AUTHORITY_CHAIN.md`
5. `R2_AUTHORITY_OWNERSHIP_V1.md`
6. `R2_MUTATION_REPLAY_MATRIX_V1.md`
7. `R2_EVIDENCE_LEDGER_V1.md`
8. `R3_CANONICAL_COMPOSITION_ENTRY_CONTRACT_V1.md`
9. `RECONSTRUCTION_ROADMAP_V1.md`

## Hard boundaries

```text
No new capability during reconstruction
No Runtime policy inference
No hardware-specific winner embedded in the Canonical ABI
No application-policy inference
No mutation of accepted Schema 17 / Runtime 17 Evidence before migration proof
No deletion of reference Projects until v2 reproduces their Invariants
No public staged names in the v2 Canonical model
```
